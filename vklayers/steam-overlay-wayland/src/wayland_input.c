#include <linux/input-event-codes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <wayland-client.h>
#include <wayland-cursor.h>

#include "relative-pointer-unstable-v1-client-protocol.h"
#include "steam_overlay_bridge.h"

#define GE_WHEEL_DELTA 120

struct ge_overlay_wayland_surface
{
    struct wl_display *display;
    struct wl_surface *target_surface;
    struct wl_surface *keyboard_surface;
    struct wl_surface *pointer_surface;
    struct wl_event_queue *queue;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_subcompositor *subcompositor;
    struct wl_shm *shm;
    struct wl_seat *seat;
    struct wl_keyboard *keyboard;
    struct wl_pointer *pointer;
    struct zwp_relative_pointer_manager_v1 *relative_pointer_manager;
    struct zwp_relative_pointer_v1 *relative_pointer;
    struct wl_cursor_theme *cursor_theme;
    struct wl_surface *cursor_surface;
    struct wl_surface *overlay_cursor_surface;
    struct wl_subsurface *overlay_cursor_subsurface;
    uint32_t seat_name;
    uint32_t subcompositor_name;
    uint32_t relative_pointer_manager_name;
    uint32_t pointer_serial;
    int relative_motion_seen;
    int bridge_registered;
    int keyboard_focus;
    int pointer_focus;
    int bridge_focus;
    int pointer_x;
    int pointer_y;
    int cursor_hotspot_x;
    int cursor_hotspot_y;
    int overlay_cursor_active;
    unsigned int refs;
    uint8_t keys[256];
    struct ge_steam_overlay_pointer_frame frame;
};

static pthread_mutex_t input_surface_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct ge_overlay_wayland_surface *process_input_surface;
static pthread_mutex_t focused_pointer_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct ge_overlay_wayland_surface *focused_pointer;
static uint32_t requested_cursor_shape = GE_STEAM_OVERLAY_CURSOR_DEFAULT;
static int requested_overlay_active;

static int overlay_debug_enabled(void)
{
    const char *env = getenv("GE_WAYLAND_STEAM_OVERLAY_DEBUG");
    const char *winedebug;

    if (env) return atoi(env) != 0;
    winedebug = getenv("WINEDEBUG");
    return winedebug && strstr(winedebug, "+waylanddrv");
}

static void overlay_trace(const char *message)
{
    if (overlay_debug_enabled())
        fprintf(stderr, "steam-overlay-wayland: %s\n", message);
}

static void update_bridge_focus(struct ge_overlay_wayland_surface *surface)
{
    int focused = surface->keyboard_focus || surface->pointer_focus;

    if (focused == surface->bridge_focus) return;
    surface->bridge_focus = focused;
    ge_overlay_bridge_focus(focused);
}

static const char *cursor_name(uint32_t shape)
{
    switch (shape)
    {
    case GE_STEAM_OVERLAY_CURSOR_POINTER: return "hand2";
    case GE_STEAM_OVERLAY_CURSOR_TEXT: return "text";
    case GE_STEAM_OVERLAY_CURSOR_WAIT: return "watch";
    case GE_STEAM_OVERLAY_CURSOR_CROSSHAIR: return "crosshair";
    case GE_STEAM_OVERLAY_CURSOR_ALL_RESIZE: return "fleur";
    case GE_STEAM_OVERLAY_CURSOR_EW_RESIZE: return "sb_h_double_arrow";
    case GE_STEAM_OVERLAY_CURSOR_NS_RESIZE: return "sb_v_double_arrow";
    case GE_STEAM_OVERLAY_CURSOR_NWSE_RESIZE: return "size_fdiag";
    case GE_STEAM_OVERLAY_CURSOR_NESW_RESIZE: return "size_bdiag";
    case GE_STEAM_OVERLAY_CURSOR_NOT_ALLOWED: return "not-allowed";
    case GE_STEAM_OVERLAY_CURSOR_HELP: return "question_arrow";
    default: return "left_ptr";
    }
}

static struct wl_cursor_image *get_cursor_image(
    struct ge_overlay_wayland_surface *surface, uint32_t shape)
{
    struct wl_cursor *cursor;
    const char *size_env;
    const char *theme;
    unsigned long size = 24;

    if (!surface || !surface->compositor || !surface->shm) return NULL;

    if (!surface->cursor_theme)
    {
        if ((size_env = getenv("XCURSOR_SIZE")))
        {
            char *end;
            unsigned long parsed = strtoul(size_env, &end, 10);
            if (end != size_env && !*end && parsed >= 8 && parsed <= 256)
                size = parsed;
        }
        theme = getenv("XCURSOR_THEME");
        surface->cursor_theme = wl_cursor_theme_load(
            theme && *theme ? theme : NULL, (int)size, surface->shm);
        if (!surface->cursor_theme) return NULL;
    }

    cursor = wl_cursor_theme_get_cursor(surface->cursor_theme,
                                        cursor_name(shape));
    if (!cursor)
        cursor = wl_cursor_theme_get_cursor(surface->cursor_theme, "left_ptr");
    if (!cursor || !cursor->image_count) return NULL;

    return cursor->images[0];
}

static void apply_hardware_cursor_shape(
    struct ge_overlay_wayland_surface *surface, uint32_t shape)
{
    struct wl_cursor_image *image;

    if (!surface || !surface->pointer || !surface->pointer_serial ||
        !surface->compositor || !(image = get_cursor_image(surface, shape)))
        return;

    if (!surface->cursor_surface)
    {
        surface->cursor_surface =
            wl_compositor_create_surface(surface->compositor);
        if (!surface->cursor_surface) return;
        wl_proxy_set_queue((struct wl_proxy *)surface->cursor_surface,
                           surface->queue);
    }

    wl_pointer_set_cursor(surface->pointer, surface->pointer_serial,
                          surface->cursor_surface,
                          (int32_t)image->hotspot_x,
                          (int32_t)image->hotspot_y);
    wl_surface_attach(surface->cursor_surface,
                      wl_cursor_image_get_buffer(image), 0, 0);
    wl_surface_damage(surface->cursor_surface, 0, 0,
                      (int32_t)image->width, (int32_t)image->height);
    wl_surface_commit(surface->cursor_surface);
    wl_display_flush(surface->display);
}

static void destroy_overlay_cursor(struct ge_overlay_wayland_surface *surface)
{
    if (surface->overlay_cursor_subsurface)
    {
        wl_subsurface_destroy(surface->overlay_cursor_subsurface);
        surface->overlay_cursor_subsurface = NULL;
    }
    if (surface->overlay_cursor_surface)
    {
        wl_surface_destroy(surface->overlay_cursor_surface);
        surface->overlay_cursor_surface = NULL;
    }
}

static int ensure_overlay_cursor(struct ge_overlay_wayland_surface *surface)
{
    struct wl_region *empty_region;

    if (surface->overlay_cursor_surface &&
        surface->overlay_cursor_subsurface)
        return 1;
    if (!surface->overlay_cursor_active || !surface->pointer_focus ||
        !surface->pointer_surface || !surface->pointer ||
        !surface->pointer_serial || !surface->compositor ||
        !surface->subcompositor)
        return 0;

    surface->overlay_cursor_surface =
        wl_compositor_create_surface(surface->compositor);
    if (!surface->overlay_cursor_surface) return 0;
    wl_proxy_set_queue((struct wl_proxy *)surface->overlay_cursor_surface,
                       surface->queue);

    surface->overlay_cursor_subsurface = wl_subcompositor_get_subsurface(
        surface->subcompositor, surface->overlay_cursor_surface,
        surface->target_surface ? surface->target_surface :
                                  surface->pointer_surface);
    if (!surface->overlay_cursor_subsurface)
    {
        destroy_overlay_cursor(surface);
        return 0;
    }
    wl_proxy_set_queue((struct wl_proxy *)surface->overlay_cursor_subsurface,
                       surface->queue);
    wl_subsurface_set_desync(surface->overlay_cursor_subsurface);
    wl_subsurface_place_above(
        surface->overlay_cursor_subsurface,
        surface->target_surface ? surface->target_surface :
                                  surface->pointer_surface);

    /* The visual cursor must never become a pointer-focus target itself. */
    if ((empty_region = wl_compositor_create_region(surface->compositor)))
    {
        wl_surface_set_input_region(surface->overlay_cursor_surface,
                                    empty_region);
        wl_region_destroy(empty_region);
    }

    /* Wine's constrained hardware cursor cannot follow Steam's synthetic
     * X11 coordinates, so hide it while the layer renders the movable cursor. */
    wl_pointer_set_cursor(surface->pointer, surface->pointer_serial,
                          NULL, 0, 0);
    overlay_trace("created movable overlay cursor surface");
    return 1;
}

static void apply_overlay_cursor_shape(
    struct ge_overlay_wayland_surface *surface, uint32_t shape)
{
    struct wl_cursor_image *image;

    if (!ensure_overlay_cursor(surface) ||
        !(image = get_cursor_image(surface, shape)))
        return;

    surface->cursor_hotspot_x = (int)image->hotspot_x;
    surface->cursor_hotspot_y = (int)image->hotspot_y;
    wl_subsurface_set_position(
        surface->overlay_cursor_subsurface,
        surface->pointer_x - surface->cursor_hotspot_x,
        surface->pointer_y - surface->cursor_hotspot_y);
    wl_surface_attach(surface->overlay_cursor_surface,
                      wl_cursor_image_get_buffer(image), 0, 0);
    wl_surface_damage(surface->overlay_cursor_surface, 0, 0,
                      (int32_t)image->width, (int32_t)image->height);
    wl_surface_commit(surface->overlay_cursor_surface);
    wl_display_flush(surface->display);
}

void ge_overlay_wayland_set_cursor_shape(uint32_t shape)
{
    if (shape > GE_STEAM_OVERLAY_CURSOR_HELP)
        shape = GE_STEAM_OVERLAY_CURSOR_DEFAULT;

    pthread_mutex_lock(&focused_pointer_mutex);
    requested_cursor_shape = shape;
    if (focused_pointer && focused_pointer->overlay_cursor_active)
        apply_overlay_cursor_shape(focused_pointer, shape);
    pthread_mutex_unlock(&focused_pointer_mutex);
}

void ge_overlay_wayland_set_cursor_position(int32_t x, int32_t y)
{
    pthread_mutex_lock(&focused_pointer_mutex);
    if (focused_pointer)
    {
        focused_pointer->pointer_x = x;
        focused_pointer->pointer_y = y;
        if (focused_pointer->overlay_cursor_subsurface)
            wl_subsurface_set_position(
                focused_pointer->overlay_cursor_subsurface,
                x - focused_pointer->cursor_hotspot_x,
                y - focused_pointer->cursor_hotspot_y);
    }
    pthread_mutex_unlock(&focused_pointer_mutex);
}

void ge_overlay_wayland_set_overlay_active(int active)
{
    pthread_mutex_lock(&focused_pointer_mutex);
    requested_overlay_active = !!active;
    if (focused_pointer)
    {
        focused_pointer->overlay_cursor_active = requested_overlay_active;
        if (active)
            apply_overlay_cursor_shape(focused_pointer,
                                       requested_cursor_shape);
        else
        {
            destroy_overlay_cursor(focused_pointer);
            apply_hardware_cursor_shape(focused_pointer,
                                        GE_STEAM_OVERLAY_CURSOR_DEFAULT);
        }
    }
    pthread_mutex_unlock(&focused_pointer_mutex);
}

static void release_pressed_keys(struct ge_overlay_wayland_surface *surface);

static void keyboard_keymap(void *data, struct wl_keyboard *keyboard,
                            uint32_t format, int32_t fd, uint32_t size)
{
    (void)data;
    (void)keyboard;
    (void)format;
    (void)size;
    close(fd);
}

static void keyboard_enter(void *data, struct wl_keyboard *keyboard,
                           uint32_t serial, struct wl_surface *wl_surface,
                           struct wl_array *keys)
{
    struct ge_overlay_wayland_surface *surface = data;
    uint32_t *key;

    (void)keyboard;
    (void)serial;
    if (!wl_surface) return;

    /* Wine gives Vulkan a client subsurface, while keyboard focus is sent to
     * its parent toplevel.  Both belong to the same Wayland connection, so
     * use the compositor's focused surface rather than comparing proxy
     * pointers with VkWaylandSurfaceCreateInfoKHR::surface. */
    if (surface->keyboard_focus &&
        surface->keyboard_surface != wl_surface)
        release_pressed_keys(surface);

    surface->keyboard_surface = wl_surface;
    surface->keyboard_focus = 1;
    wl_array_for_each(key, keys)
    {
        if (*key >= sizeof(surface->keys) || surface->keys[*key]) continue;
        surface->keys[*key] = 1;
        ge_overlay_bridge_filter_key(0, *key, 1);
    }
    update_bridge_focus(surface);
    overlay_trace("Wayland toplevel gained keyboard focus");
}

static void release_pressed_keys(struct ge_overlay_wayland_surface *surface)
{
    unsigned int key;

    for (key = 0; key < sizeof(surface->keys); ++key)
    {
        if (!surface->keys[key]) continue;
        surface->keys[key] = 0;
        ge_overlay_bridge_filter_key(0, key, 0);
    }
}

static void keyboard_leave(void *data, struct wl_keyboard *keyboard,
                           uint32_t serial, struct wl_surface *wl_surface)
{
    struct ge_overlay_wayland_surface *surface = data;

    (void)keyboard;
    (void)serial;
    if (!surface->keyboard_focus ||
        wl_surface != surface->keyboard_surface)
        return;

    release_pressed_keys(surface);
    surface->keyboard_surface = NULL;
    surface->keyboard_focus = 0;
    update_bridge_focus(surface);
    overlay_trace("Wayland toplevel lost keyboard focus");
}

static void keyboard_key(void *data, struct wl_keyboard *keyboard,
                         uint32_t serial, uint32_t time, uint32_t key,
                         uint32_t state)
{
    struct ge_overlay_wayland_surface *surface = data;
    int pressed = state == WL_KEYBOARD_KEY_STATE_PRESSED;

    (void)keyboard;
    (void)serial;
    if (!surface->keyboard_focus) return;
    if (key < sizeof(surface->keys)) surface->keys[key] = pressed;
    ge_overlay_bridge_filter_key(time, key, pressed);
}

static void keyboard_modifiers(void *data, struct wl_keyboard *keyboard,
                               uint32_t serial, uint32_t depressed,
                               uint32_t latched, uint32_t locked,
                               uint32_t group)
{
    (void)data;
    (void)keyboard;
    (void)serial;
    (void)depressed;
    (void)latched;
    (void)locked;
    (void)group;
}

static void keyboard_repeat_info(void *data, struct wl_keyboard *keyboard,
                                 int32_t rate, int32_t delay)
{
    (void)data;
    (void)keyboard;
    (void)rate;
    (void)delay;
}

static const struct wl_keyboard_listener keyboard_listener =
{
    .keymap = keyboard_keymap,
    .enter = keyboard_enter,
    .leave = keyboard_leave,
    .key = keyboard_key,
    .modifiers = keyboard_modifiers,
    .repeat_info = keyboard_repeat_info,
};

static void pointer_enter(void *data, struct wl_pointer *pointer,
                          uint32_t serial, struct wl_surface *wl_surface,
                          wl_fixed_t x, wl_fixed_t y)
{
    struct ge_overlay_wayland_surface *surface = data;

    (void)pointer;
    if (!wl_surface) return;

    surface->pointer_surface = wl_surface;
    surface->pointer_focus = 1;
    surface->pointer_serial = serial;
    surface->pointer_x = wl_fixed_to_int(x);
    surface->pointer_y = wl_fixed_to_int(y);
    surface->relative_motion_seen = 0;
    surface->frame.time = 0;
    surface->frame.x = surface->pointer_x;
    surface->frame.y = surface->pointer_y;
    surface->frame.flags |= GE_STEAM_OVERLAY_FRAME_ABSOLUTE;

    pthread_mutex_lock(&focused_pointer_mutex);
    focused_pointer = surface;
    surface->overlay_cursor_active = requested_overlay_active;
    if (surface->overlay_cursor_active)
        apply_overlay_cursor_shape(surface, requested_cursor_shape);
    pthread_mutex_unlock(&focused_pointer_mutex);
    update_bridge_focus(surface);
    overlay_trace("Wayland toplevel gained pointer focus");
}

static void pointer_leave(void *data, struct wl_pointer *pointer,
                          uint32_t serial, struct wl_surface *wl_surface)
{
    struct ge_overlay_wayland_surface *surface = data;

    (void)pointer;
    (void)serial;
    if (!surface->pointer_focus ||
        wl_surface != surface->pointer_surface)
        return;

    pthread_mutex_lock(&focused_pointer_mutex);
    if (focused_pointer == surface) focused_pointer = NULL;
    destroy_overlay_cursor(surface);
    pthread_mutex_unlock(&focused_pointer_mutex);
    surface->pointer_surface = NULL;
    surface->pointer_focus = 0;
    surface->pointer_serial = 0;
    surface->relative_motion_seen = 0;
    memset(&surface->frame, 0, sizeof(surface->frame));
    update_bridge_focus(surface);
    overlay_trace("Wayland toplevel lost pointer focus");
}

static void pointer_motion(void *data, struct wl_pointer *pointer,
                           uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
    struct ge_overlay_wayland_surface *surface = data;

    (void)pointer;
    if (!surface->pointer_focus) return;
    if (surface->relative_motion_seen) return;
    surface->pointer_x = wl_fixed_to_int(x);
    surface->pointer_y = wl_fixed_to_int(y);
    surface->frame.time = time;
    surface->frame.x = surface->pointer_x;
    surface->frame.y = surface->pointer_y;
    surface->frame.flags |= GE_STEAM_OVERLAY_FRAME_ABSOLUTE;
}

static void pointer_button(void *data, struct wl_pointer *pointer,
                           uint32_t serial, uint32_t time, uint32_t button,
                           uint32_t state)
{
    struct ge_overlay_wayland_surface *surface = data;

    (void)pointer;
    (void)serial;
    if (!surface->pointer_focus) return;
    ge_overlay_bridge_filter_pointer_button(
        time, button, state == WL_POINTER_BUTTON_STATE_PRESSED);
}

static void pointer_axis(void *data, struct wl_pointer *pointer,
                         uint32_t time, uint32_t axis, wl_fixed_t value)
{
    struct ge_overlay_wayland_surface *surface = data;
    double scroll = wl_fixed_to_double(value) / 15.0;

    (void)pointer;
    if (!surface->pointer_focus) return;
    surface->frame.time = time;
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
    {
        surface->frame.axis += -scroll * GE_WHEEL_DELTA;
        surface->frame.flags |= GE_STEAM_OVERLAY_FRAME_AXIS;
    }
    else if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
    {
        surface->frame.horz_axis += scroll * GE_WHEEL_DELTA;
        surface->frame.flags |= GE_STEAM_OVERLAY_FRAME_AXIS_HORZ;
    }
}

static void pointer_frame(void *data, struct wl_pointer *pointer)
{
    struct ge_overlay_wayland_surface *surface = data;

    (void)pointer;
    if (!surface->pointer_focus) return;
    ge_overlay_bridge_filter_pointer_frame(&surface->frame);
    memset(&surface->frame, 0, sizeof(surface->frame));
}

static void pointer_axis_source(void *data, struct wl_pointer *pointer,
                                uint32_t source)
{
    (void)data;
    (void)pointer;
    (void)source;
}

static void pointer_axis_stop(void *data, struct wl_pointer *pointer,
                              uint32_t time, uint32_t axis)
{
    (void)data;
    (void)pointer;
    (void)time;
    (void)axis;
}

static void pointer_axis_discrete(void *data, struct wl_pointer *pointer,
                                  uint32_t axis, int32_t discrete)
{
    struct ge_overlay_wayland_surface *surface = data;

    (void)pointer;
    if (!surface->pointer_focus || !discrete) return;
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
    {
        surface->frame.scroll += -GE_WHEEL_DELTA * discrete;
        surface->frame.flags |= GE_STEAM_OVERLAY_FRAME_DISCRETE_WHEEL;
    }
    else if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
    {
        surface->frame.horz_scroll += GE_WHEEL_DELTA * discrete;
        surface->frame.flags |= GE_STEAM_OVERLAY_FRAME_DISCRETE_WHEEL_HORZ;
    }
}

static const struct wl_pointer_listener pointer_listener =
{
    .enter = pointer_enter,
    .leave = pointer_leave,
    .motion = pointer_motion,
    .button = pointer_button,
    .axis = pointer_axis,
    .frame = pointer_frame,
    .axis_source = pointer_axis_source,
    .axis_stop = pointer_axis_stop,
    .axis_discrete = pointer_axis_discrete,
};

static void relative_pointer_motion(
    void *data, struct zwp_relative_pointer_v1 *relative_pointer,
    uint32_t utime_hi, uint32_t utime_lo, wl_fixed_t dx, wl_fixed_t dy,
    wl_fixed_t dx_unaccel, wl_fixed_t dy_unaccel)
{
    struct ge_overlay_wayland_surface *surface = data;
    struct ge_steam_overlay_pointer_frame frame = {0};
    uint64_t usec = ((uint64_t)utime_hi << 32) | utime_lo;

    (void)relative_pointer;
    (void)dx_unaccel;
    (void)dy_unaccel;

    if (!surface->pointer_focus) return;

    frame.time = (uint32_t)(usec / 1000);

    /* Relative-pointer events continue while Wine has constrained the game
     * pointer. Once they begin, use them for the remainder of this pointer
     * focus. Some compositors also emit a stationary wl_pointer.motion event;
     * using a time gate for that event can otherwise suppress every real
     * relative delta and leave Steam's visible cursor frozen in place. */
    if (!surface->relative_motion_seen)
    {
        surface->relative_motion_seen = 1;
        surface->frame.flags &= ~GE_STEAM_OVERLAY_FRAME_ABSOLUTE;
        overlay_trace("relative-pointer motion is active");
    }

    frame.dx = wl_fixed_to_double(dx);
    frame.dy = wl_fixed_to_double(dy);
    frame.flags = GE_STEAM_OVERLAY_FRAME_RELATIVE;
    ge_overlay_bridge_filter_pointer_frame(&frame);
}

static const struct zwp_relative_pointer_v1_listener relative_pointer_listener =
{
    .relative_motion = relative_pointer_motion,
};

static void create_relative_pointer(struct ge_overlay_wayland_surface *surface)
{
    if (surface->relative_pointer || !surface->relative_pointer_manager ||
        !surface->pointer)
        return;

    surface->relative_pointer =
        zwp_relative_pointer_manager_v1_get_relative_pointer(
            surface->relative_pointer_manager, surface->pointer);
    if (!surface->relative_pointer) return;

    wl_proxy_set_queue((struct wl_proxy *)surface->relative_pointer,
                       surface->queue);
    zwp_relative_pointer_v1_add_listener(surface->relative_pointer,
                                         &relative_pointer_listener, surface);
    overlay_trace("attached relative-pointer listener");
}

static void destroy_relative_pointer(struct ge_overlay_wayland_surface *surface)
{
    if (!surface->relative_pointer) return;
    zwp_relative_pointer_v1_destroy(surface->relative_pointer);
    surface->relative_pointer = NULL;
    surface->relative_motion_seen = 0;
}

static void destroy_seat_devices(struct ge_overlay_wayland_surface *surface)
{
    if (surface->bridge_focus)
    {
        surface->keyboard_focus = 0;
        surface->pointer_focus = 0;
        surface->bridge_focus = 0;
        ge_overlay_bridge_focus(0);
    }
    pthread_mutex_lock(&focused_pointer_mutex);
    if (focused_pointer == surface) focused_pointer = NULL;
    destroy_overlay_cursor(surface);
    pthread_mutex_unlock(&focused_pointer_mutex);

    surface->keyboard_surface = NULL;
    surface->pointer_surface = NULL;
    release_pressed_keys(surface);

    destroy_relative_pointer(surface);
    if (surface->pointer)
    {
        wl_pointer_release(surface->pointer);
        surface->pointer = NULL;
    }
    if (surface->keyboard)
    {
        wl_keyboard_release(surface->keyboard);
        surface->keyboard = NULL;
    }
    if (surface->seat)
    {
        wl_seat_release(surface->seat);
        surface->seat = NULL;
    }
    surface->seat_name = 0;
}

static void seat_capabilities(void *data, struct wl_seat *seat, uint32_t caps)
{
    struct ge_overlay_wayland_surface *surface = data;

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !surface->keyboard)
    {
        surface->keyboard = wl_seat_get_keyboard(seat);
        wl_proxy_set_queue((struct wl_proxy *)surface->keyboard, surface->queue);
        wl_keyboard_add_listener(surface->keyboard, &keyboard_listener, surface);
    }
    else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && surface->keyboard)
    {
        release_pressed_keys(surface);
        surface->keyboard_focus = 0;
        wl_keyboard_release(surface->keyboard);
        surface->keyboard = NULL;
        update_bridge_focus(surface);
    }

    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !surface->pointer)
    {
        surface->pointer = wl_seat_get_pointer(seat);
        wl_proxy_set_queue((struct wl_proxy *)surface->pointer, surface->queue);
        wl_pointer_add_listener(surface->pointer, &pointer_listener, surface);
        create_relative_pointer(surface);
    }
    else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && surface->pointer)
    {
        pthread_mutex_lock(&focused_pointer_mutex);
        if (focused_pointer == surface) focused_pointer = NULL;
        destroy_overlay_cursor(surface);
        pthread_mutex_unlock(&focused_pointer_mutex);
        surface->pointer_focus = 0;
        destroy_relative_pointer(surface);
        wl_pointer_release(surface->pointer);
        surface->pointer = NULL;
        update_bridge_focus(surface);
    }
}

static void seat_name(void *data, struct wl_seat *seat, const char *name)
{
    (void)data;
    (void)seat;
    (void)name;
}

static const struct wl_seat_listener seat_listener =
{
    .capabilities = seat_capabilities,
    .name = seat_name,
};

static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface,
                            uint32_t version)
{
    struct ge_overlay_wayland_surface *surface = data;

    if (!strcmp(interface, wl_compositor_interface.name) && !surface->compositor)
    {
        surface->compositor = wl_registry_bind(
            registry, name, &wl_compositor_interface, version < 4 ? version : 4);
        wl_proxy_set_queue((struct wl_proxy *)surface->compositor, surface->queue);
    }
    else if (!strcmp(interface, wl_subcompositor_interface.name) &&
             !surface->subcompositor)
    {
        surface->subcompositor_name = name;
        surface->subcompositor = wl_registry_bind(
            registry, name, &wl_subcompositor_interface, 1);
        wl_proxy_set_queue((struct wl_proxy *)surface->subcompositor,
                           surface->queue);
    }
    else if (!strcmp(interface, wl_shm_interface.name) && !surface->shm)
    {
        surface->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
        wl_proxy_set_queue((struct wl_proxy *)surface->shm, surface->queue);
    }
    else if (!strcmp(interface, wl_seat_interface.name) && !surface->seat &&
             version >= 5)
    {
        surface->seat_name = name;
        surface->seat = wl_registry_bind(registry, name, &wl_seat_interface, 5);
        wl_proxy_set_queue((struct wl_proxy *)surface->seat, surface->queue);
        wl_seat_add_listener(surface->seat, &seat_listener, surface);
    }
    else if (!strcmp(interface, zwp_relative_pointer_manager_v1_interface.name) &&
             !surface->relative_pointer_manager)
    {
        surface->relative_pointer_manager_name = name;
        surface->relative_pointer_manager = wl_registry_bind(
            registry, name, &zwp_relative_pointer_manager_v1_interface, 1);
        wl_proxy_set_queue((struct wl_proxy *)surface->relative_pointer_manager,
                           surface->queue);
        create_relative_pointer(surface);
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry,
                                   uint32_t name)
{
    struct ge_overlay_wayland_surface *surface = data;

    (void)registry;
    if (surface->seat_name == name) destroy_seat_devices(surface);
    if (surface->subcompositor_name == name)
    {
        destroy_overlay_cursor(surface);
        wl_subcompositor_destroy(surface->subcompositor);
        surface->subcompositor = NULL;
        surface->subcompositor_name = 0;
    }
    if (surface->relative_pointer_manager_name == name)
    {
        destroy_relative_pointer(surface);
        zwp_relative_pointer_manager_v1_destroy(
            surface->relative_pointer_manager);
        surface->relative_pointer_manager = NULL;
        surface->relative_pointer_manager_name = 0;
    }
}

static const struct wl_registry_listener registry_listener =
{
    .global = registry_global,
    .global_remove = registry_global_remove,
};

struct ge_overlay_wayland_surface *ge_overlay_wayland_surface_create(
    struct wl_display *display, struct wl_surface *target_surface)
{
    struct ge_overlay_wayland_surface *surface;
    struct wl_display *display_wrapper;

    if (!display) return NULL;

    pthread_mutex_lock(&input_surface_mutex);
    if (process_input_surface)
    {
        if (process_input_surface->display != display)
        {
            pthread_mutex_unlock(&input_surface_mutex);
            return NULL;
        }
        ++process_input_surface->refs;
        surface = process_input_surface;
        pthread_mutex_unlock(&input_surface_mutex);
        return surface;
    }

    if (!(surface = calloc(1, sizeof(*surface))))
    {
        pthread_mutex_unlock(&input_surface_mutex);
        return NULL;
    }

    surface->display = display;
    surface->target_surface = target_surface;
    surface->refs = 1;
    if (!(surface->queue = wl_display_create_queue(display))) goto fail;

    if (!(display_wrapper = (struct wl_display *)wl_proxy_create_wrapper(display)))
        goto fail;
    wl_proxy_set_queue((struct wl_proxy *)display_wrapper, surface->queue);
    surface->registry = wl_display_get_registry(display_wrapper);
    wl_proxy_wrapper_destroy(display_wrapper);
    if (!surface->registry) goto fail;

    wl_registry_add_listener(surface->registry, &registry_listener, surface);
    if (wl_display_roundtrip_queue(display, surface->queue) < 0 ||
        wl_display_roundtrip_queue(display, surface->queue) < 0)
        goto fail;

    ge_overlay_bridge_surface_created();
    surface->bridge_registered = 1;
    process_input_surface = surface;
    pthread_mutex_unlock(&input_surface_mutex);
    overlay_trace("attached input listeners to process Wayland connection");
    return surface;

fail:
    destroy_seat_devices(surface);
    if (surface->relative_pointer_manager)
        zwp_relative_pointer_manager_v1_destroy(
            surface->relative_pointer_manager);
    if (surface->cursor_surface) wl_surface_destroy(surface->cursor_surface);
    destroy_overlay_cursor(surface);
    if (surface->cursor_theme) wl_cursor_theme_destroy(surface->cursor_theme);
    if (surface->shm) wl_shm_destroy(surface->shm);
    if (surface->compositor) wl_compositor_destroy(surface->compositor);
    if (surface->subcompositor)
        wl_subcompositor_destroy(surface->subcompositor);
    if (surface->registry) wl_registry_destroy(surface->registry);
    if (surface->queue) wl_event_queue_destroy(surface->queue);
    free(surface);
    pthread_mutex_unlock(&input_surface_mutex);
    return NULL;
}

void ge_overlay_wayland_surface_dispatch(struct ge_overlay_wayland_surface *surface)
{
    if (!surface || !surface->display || !surface->queue) return;
    if (wl_display_dispatch_queue_pending(surface->display, surface->queue) < 0)
        overlay_trace("failed to dispatch private Wayland input queue");
}

void ge_overlay_wayland_surface_destroy(struct ge_overlay_wayland_surface *surface)
{
    if (!surface) return;

    pthread_mutex_lock(&input_surface_mutex);
    if (surface != process_input_surface || !surface->refs)
    {
        pthread_mutex_unlock(&input_surface_mutex);
        return;
    }
    if (--surface->refs)
    {
        pthread_mutex_unlock(&input_surface_mutex);
        return;
    }
    process_input_surface = NULL;
    pthread_mutex_unlock(&input_surface_mutex);

    destroy_seat_devices(surface);
    if (surface->relative_pointer_manager)
        zwp_relative_pointer_manager_v1_destroy(
            surface->relative_pointer_manager);
    if (surface->cursor_surface) wl_surface_destroy(surface->cursor_surface);
    destroy_overlay_cursor(surface);
    if (surface->cursor_theme) wl_cursor_theme_destroy(surface->cursor_theme);
    if (surface->shm) wl_shm_destroy(surface->shm);
    if (surface->compositor) wl_compositor_destroy(surface->compositor);
    if (surface->subcompositor)
        wl_subcompositor_destroy(surface->subcompositor);
    if (surface->registry) wl_registry_destroy(surface->registry);
    if (surface->queue) wl_event_queue_destroy(surface->queue);
    if (surface->bridge_registered) ge_overlay_bridge_surface_destroyed();
    free(surface);
}
