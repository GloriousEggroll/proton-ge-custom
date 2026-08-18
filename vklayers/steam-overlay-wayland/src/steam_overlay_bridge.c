/*
 * Steam overlay X11 compatibility bridge for native Wine Wayland games.
 *
 * The bridge intentionally lives outside winewayland.drv. Wine forwards only
 * the Wayland events which an external component cannot observe directly.
 */

#include "steam_overlay_bridge.h"

#include <dlfcn.h>
#include <limits.h>
#include <link.h>
#include <linux/input.h>
#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define GE_WHEEL_DELTA 120
#define GE_CURSOR_MAP_SIZE 64

enum overlay_x11_request_type
{
    OVERLAY_X11_CHANGE_WINDOW_ATTRIBUTES = 2,
    OVERLAY_X11_CREATE_CURSOR = 93,
    OVERLAY_X11_CREATE_GLYPH_CURSOR = 94,
    OVERLAY_X11_FREE_CURSOR = 95,
};

struct overlay_x11_request
{
    uint8_t type;
    uint8_t data;
    uint16_t length;
};

struct overlay_x11_resource_request
{
    uint8_t type;
    uint8_t pad;
    uint16_t length;
    uint32_t id;
};

struct overlay_x11_create_cursor_request
{
    uint8_t type;
    uint8_t pad;
    uint16_t length;
    uint32_t cursor;
    uint32_t source;
    uint32_t mask;
    uint16_t foreground_red;
    uint16_t foreground_green;
    uint16_t foreground_blue;
    uint16_t background_red;
    uint16_t background_green;
    uint16_t background_blue;
    uint16_t x;
    uint16_t y;
};

struct overlay_x11_create_glyph_cursor_request
{
    uint8_t type;
    uint8_t pad;
    uint16_t length;
    uint32_t cursor;
    uint32_t source;
    uint32_t mask;
    uint16_t source_char;
    uint16_t mask_char;
    uint16_t foreground_red;
    uint16_t foreground_green;
    uint16_t foreground_blue;
    uint16_t background_red;
    uint16_t background_green;
    uint16_t background_blue;
};

struct overlay_x11_change_window_attributes_request
{
    uint8_t type;
    uint8_t pad;
    uint16_t length;
    uint32_t window;
    uint32_t value_mask;
};

struct overlay_event_match
{
    Window window;
    unsigned long serial;
    int type;
};

struct overlay_cursor_entry
{
    Cursor cursor;
    uint32_t shape;
};

typedef Bool (*xcheck_if_event_fn)(Display *, XEvent *,
                                  Bool (*)(Display *, XEvent *, XPointer),
                                  XPointer);

/* Bypass gameoverlayrenderer's XPutBackEvent wrapper. The event must first be
 * observed by its wrapped XCheckIfEvent call below. */
extern int _XPutBackEvent(Display *display, XEvent *event);

static pthread_mutex_t overlay_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t cursor_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct ge_steam_overlay_host_v1 overlay_host;
static Display *overlay_display;
static Window overlay_window;
static Window overlay_root;
static Atom overlay_owner_atom;
static int (*previous_after_function)(Display *display);
static xcheck_if_event_fn overlay_check_if_event;
static unsigned long overlay_serial;
static unsigned int overlay_state;
static int overlay_initialized;
static int overlay_active;
static int overlay_input_active;
static int overlay_focus_owner;
static int overlay_requested_focus;
static int overlay_advertised_focus = -1;
static int overlay_bridge_suspended;
static uint64_t overlay_next_init_retry_ms;
static int overlay_wait_logged;
static int pointer_x;
static int pointer_y;
static struct overlay_cursor_entry overlay_cursor_map[GE_CURSOR_MAP_SIZE];
static uint32_t overlay_cursor_shape = GE_STEAM_OVERLAY_CURSOR_DEFAULT;
static int overlay_cursor_dirty;

static int overlay_debug_enabled(void)
{
    const char *env = getenv("GE_WAYLAND_STEAM_OVERLAY_DEBUG");
    const char *winedebug;

    if (env) return atoi(env) != 0;
    winedebug = getenv("WINEDEBUG");
    return winedebug && strstr(winedebug, "+waylanddrv");
}

static void overlay_trace(const char *format, ...)
{
    va_list args;

    if (!overlay_debug_enabled()) return;
    fputs("steam-overlay-wayland: ", stderr);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}

static int env_enabled(const char *name, int default_value)
{
    const char *value = getenv(name);
    return value ? atoi(value) != 0 : default_value;
}

static uint64_t monotonic_msec(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts)) return 0;
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

struct overlay_renderer_lookup
{
    char path[PATH_MAX];
};

static int find_overlay_renderer(struct dl_phdr_info *info, size_t size,
                                 void *data)
{
    struct overlay_renderer_lookup *lookup = data;

    (void)size;
    if (!info->dlpi_name || !strstr(info->dlpi_name, "gameoverlayrenderer"))
        return 0;

    snprintf(lookup->path, sizeof(lookup->path), "%s", info->dlpi_name);
    return 1;
}

static int resolve_overlay_renderer_hook(void)
{
    struct overlay_renderer_lookup lookup = {0};
    void *handle;

    if (overlay_check_if_event) return 1;

    dl_iterate_phdr(find_overlay_renderer, &lookup);
    if (!lookup.path[0]) return 0;

    handle = dlopen(lookup.path, RTLD_LAZY | RTLD_LOCAL | RTLD_NOLOAD);
    if (!handle) return 0;

    overlay_check_if_event =
        (xcheck_if_event_fn)dlsym(handle, "XCheckIfEvent");
    if (!overlay_check_if_event)
    {
        dlclose(handle);
        return 0;
    }

    overlay_trace("resolved XCheckIfEvent directly from %s\n", lookup.path);
    /* gameoverlayrenderer is already loaded by Steam. Do not retain another
     * reference which would keep its process resources alive after teardown. */
    dlclose(handle);
    return 1;
}

static Bool match_overlay_event(Display *display, XEvent *event, XPointer arg)
{
    const struct overlay_event_match *match = (const struct overlay_event_match *)arg;

    (void)display;
    return event->xany.window == match->window &&
           event->xany.serial == match->serial && event->type == match->type;
}

static uint32_t cursor_shape_from_glyph(unsigned int glyph)
{
    switch (glyph)
    {
    case XC_hand1:
    case XC_hand2:
        return GE_STEAM_OVERLAY_CURSOR_POINTER;
    case XC_xterm:
        return GE_STEAM_OVERLAY_CURSOR_TEXT;
    case XC_clock:
    case XC_watch:
        return GE_STEAM_OVERLAY_CURSOR_WAIT;
    case XC_cross:
    case XC_cross_reverse:
    case XC_crosshair:
    case XC_diamond_cross:
    case XC_dot:
    case XC_dotbox:
    case XC_iron_cross:
    case XC_plus:
    case XC_target:
    case XC_tcross:
        return GE_STEAM_OVERLAY_CURSOR_CROSSHAIR;
    case XC_fleur:
    case XC_sizing:
        return GE_STEAM_OVERLAY_CURSOR_ALL_RESIZE;
    case XC_left_side:
    case XC_left_tee:
    case XC_right_side:
    case XC_right_tee:
    case XC_sb_h_double_arrow:
        return GE_STEAM_OVERLAY_CURSOR_EW_RESIZE;
    case XC_bottom_side:
    case XC_bottom_tee:
    case XC_sb_v_double_arrow:
    case XC_top_side:
    case XC_top_tee:
        return GE_STEAM_OVERLAY_CURSOR_NS_RESIZE;
    case XC_bottom_right_corner:
    case XC_top_left_corner:
        return GE_STEAM_OVERLAY_CURSOR_NWSE_RESIZE;
    case XC_bottom_left_corner:
    case XC_top_right_corner:
        return GE_STEAM_OVERLAY_CURSOR_NESW_RESIZE;
    case XC_pirate:
        return GE_STEAM_OVERLAY_CURSOR_NOT_ALLOWED;
    case XC_question_arrow:
        return GE_STEAM_OVERLAY_CURSOR_HELP;
    default:
        return GE_STEAM_OVERLAY_CURSOR_DEFAULT;
    }
}

static void record_overlay_cursor(Cursor cursor, uint32_t shape)
{
    unsigned int i;
    unsigned int free_slot = GE_CURSOR_MAP_SIZE;

    for (i = 0; i < GE_CURSOR_MAP_SIZE; ++i)
    {
        if (overlay_cursor_map[i].cursor == cursor)
        {
            overlay_cursor_map[i].shape = shape;
            return;
        }
        if (!overlay_cursor_map[i].cursor && free_slot == GE_CURSOR_MAP_SIZE)
            free_slot = i;
    }

    if (free_slot == GE_CURSOR_MAP_SIZE) free_slot = cursor % GE_CURSOR_MAP_SIZE;
    overlay_cursor_map[free_slot].cursor = cursor;
    overlay_cursor_map[free_slot].shape = shape;
}

static uint32_t find_overlay_cursor_shape(Cursor cursor)
{
    unsigned int i;

    if (!cursor) return GE_STEAM_OVERLAY_CURSOR_DEFAULT;
    for (i = 0; i < GE_CURSOR_MAP_SIZE; ++i)
        if (overlay_cursor_map[i].cursor == cursor)
            return overlay_cursor_map[i].shape;

    return GE_STEAM_OVERLAY_CURSOR_DEFAULT;
}

static void bridge_set_cursor_shape(uint32_t shape)
{
    if (shape > GE_STEAM_OVERLAY_CURSOR_HELP)
        shape = GE_STEAM_OVERLAY_CURSOR_DEFAULT;

    pthread_mutex_lock(&cursor_mutex);
    overlay_cursor_shape = shape;
    overlay_cursor_dirty = 1;
    pthread_mutex_unlock(&cursor_mutex);
}

static unsigned int window_attribute_value_index(unsigned long mask,
                                                 unsigned long attribute)
{
    unsigned int index = 0;
    unsigned long bit;

    for (bit = 1; bit < attribute; bit <<= 1)
        if (mask & bit) ++index;
    return index;
}

static int overlay_x11_after_request(Display *display)
{
    const struct overlay_x11_request *request =
        (const struct overlay_x11_request *)((_XPrivDisplay)display)->private11;

    pthread_mutex_lock(&cursor_mutex);
    if (request)
    {
        switch (request->type)
        {
        case OVERLAY_X11_CREATE_GLYPH_CURSOR:
        {
            const struct overlay_x11_create_glyph_cursor_request *cursor =
                (const struct overlay_x11_create_glyph_cursor_request *)request;
            record_overlay_cursor(cursor->cursor,
                                  cursor_shape_from_glyph(cursor->source_char));
            break;
        }
        case OVERLAY_X11_CREATE_CURSOR:
        {
            const struct overlay_x11_create_cursor_request *cursor =
                (const struct overlay_x11_create_cursor_request *)request;
            record_overlay_cursor(cursor->cursor, GE_STEAM_OVERLAY_CURSOR_DEFAULT);
            break;
        }
        case OVERLAY_X11_FREE_CURSOR:
        {
            const struct overlay_x11_resource_request *cursor =
                (const struct overlay_x11_resource_request *)request;
            unsigned int i;

            for (i = 0; i < GE_CURSOR_MAP_SIZE; ++i)
                if (overlay_cursor_map[i].cursor == cursor->id)
                {
                    overlay_cursor_map[i].cursor = 0;
                    break;
                }
            break;
        }
        case OVERLAY_X11_CHANGE_WINDOW_ATTRIBUTES:
        {
            const struct overlay_x11_change_window_attributes_request *attributes =
                (const struct overlay_x11_change_window_attributes_request *)request;

            if (attributes->value_mask & CWCursor)
            {
                const uint32_t *values = (const uint32_t *)(attributes + 1);
                Cursor cursor = values[window_attribute_value_index(
                    attributes->value_mask, CWCursor)];

                overlay_cursor_shape = find_overlay_cursor_shape(cursor);
                overlay_cursor_dirty = 1;
            }
            break;
        }
        default:
            break;
        }
    }
    pthread_mutex_unlock(&cursor_mutex);

    if (previous_after_function && previous_after_function != overlay_x11_after_request)
        return previous_after_function(display);
    return 0;
}

static void apply_overlay_cursor(int force)
{
    uint32_t shape;
    int dirty;

    pthread_mutex_lock(&cursor_mutex);
    shape = overlay_cursor_shape;
    dirty = overlay_cursor_dirty;
    overlay_cursor_dirty = 0;
    pthread_mutex_unlock(&cursor_mutex);

    if ((dirty || force) && overlay_host.set_cursor_shape)
    {
        overlay_trace("applying overlay cursor shape %u\n", shape);
        overlay_host.set_cursor_shape(overlay_host.userdata, shape);
    }
}

static int update_overlay_active(void)
{
    int active;
    int changed;

    pthread_mutex_lock(&overlay_mutex);
    active = overlay_input_active;
    if (overlay_host.overlay_event_is_active)
        active |= overlay_host.overlay_event_is_active(overlay_host.userdata);
    changed = active != overlay_active;
    overlay_active = active;
    pthread_mutex_unlock(&overlay_mutex);

    if (changed)
    {
        overlay_trace("overlay is now %s\n", active ? "active" : "inactive");
        if (active) bridge_set_cursor_shape(GE_STEAM_OVERLAY_CURSOR_DEFAULT);
        if (overlay_host.set_overlay_active)
            overlay_host.set_overlay_active(overlay_host.userdata, active);
    }

    return active;
}

static void update_overlay_pointer_ownership(int consumed)
{
    int changed;

    pthread_mutex_lock(&overlay_mutex);
    changed = consumed != overlay_input_active;
    overlay_input_active = consumed;
    if (overlay_host.set_overlay_event_owned)
        overlay_host.set_overlay_event_owned(overlay_host.userdata, consumed);
    pthread_mutex_unlock(&overlay_mutex);

    if (changed)
        overlay_trace("overlay %s pointer ownership through X11 input\n",
                      consumed ? "acquired" : "released");
}

static int forward_overlay_x11_event(XEvent *event)
{
    struct overlay_event_match match;
    XEvent returned;
    int consumed;

    pthread_mutex_lock(&overlay_mutex);
    if (overlay_initialized <= 0 || !overlay_display || !overlay_check_if_event)
    {
        pthread_mutex_unlock(&overlay_mutex);
        return 0;
    }

    event->xany.display = overlay_display;
    event->xany.window = overlay_window;
    event->xany.send_event = False;
    event->xany.serial = ++overlay_serial;

    match.window = overlay_window;
    match.serial = overlay_serial;
    match.type = event->type;

    XLockDisplay(overlay_display);
    _XPutBackEvent(overlay_display, event);
    XUnlockDisplay(overlay_display);

    consumed = !overlay_check_if_event(overlay_display, &returned,
                                       match_overlay_event, (XPointer)&match);
    pthread_mutex_unlock(&overlay_mutex);
    return consumed;
}

static int init_overlay_bridge(int force_retry)
{
    XSetWindowAttributes attributes = {0};
    XClassHint class_hint;
    const char *display_name;
    const char *env;
    char owner_selection[160];
    char window_class[128];
    unsigned long pid;
    Atom net_wm_pid;
    uint64_t now;

    pthread_mutex_lock(&overlay_mutex);
    if (overlay_bridge_suspended)
    {
        pthread_mutex_unlock(&overlay_mutex);
        return 0;
    }

    if (overlay_initialized)
    {
        int ready = overlay_initialized > 0;
        pthread_mutex_unlock(&overlay_mutex);
        return ready;
    }

    if (!env_enabled("WINE_WAYLAND_STEAM_OVERLAY_LAYER", 0) ||
        env_enabled("DISABLE_WINE_WAYLAND_STEAM_OVERLAY_LAYER", 0) ||
        !env_enabled("WAYLANDDRV_STEAM_OVERLAY_X11", 1))
        goto disabled;

    now = monotonic_msec();
    if (!force_retry && overlay_next_init_retry_ms &&
        now < overlay_next_init_retry_ms)
    {
        pthread_mutex_unlock(&overlay_mutex);
        return 0;
    }

    if (!resolve_overlay_renderer_hook())
        goto retry;

    display_name = getenv("DISPLAY");
    if (!display_name) display_name = getenv("GAMESCOPE_XWAYLAND_DISPLAY");
    if (!display_name) goto retry;

    XInitThreads();
    if (!(overlay_display = XOpenDisplay(display_name))) goto retry;

    overlay_root = DefaultRootWindow(overlay_display);
    attributes.override_redirect = True;
    attributes.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask |
                            ButtonReleaseMask | PointerMotionMask;
    overlay_window = XCreateWindow(
        overlay_display, overlay_root, -1, -1, 1, 1, 0, 0, InputOnly,
        CopyFromParent, CWOverrideRedirect | CWEventMask, &attributes);
    if (!overlay_window)
    {
        XCloseDisplay(overlay_display);
        overlay_display = NULL;
        goto retry;
    }

    XSelectInput(overlay_display, overlay_window, attributes.event_mask);
    env = getenv("SteamAppId");
    if (env && env[0])
    {
        snprintf(window_class, sizeof(window_class), "steam_app_%s", env);
        snprintf(owner_selection, sizeof(owner_selection),
                 "_WINE_WAYLAND_STEAM_FOCUS_%s", env);
    }
    else
    {
        snprintf(window_class, sizeof(window_class), "steam_proton");
        snprintf(owner_selection, sizeof(owner_selection),
                 "_WINE_WAYLAND_STEAM_FOCUS_DEFAULT");
    }

    overlay_owner_atom = XInternAtom(overlay_display, owner_selection, False);
    class_hint.res_name = window_class;
    class_hint.res_class = window_class;
    XSetClassHint(overlay_display, overlay_window, &class_hint);
    XStoreName(overlay_display, overlay_window, window_class);

    pid = (unsigned long)getpid();
    net_wm_pid = XInternAtom(overlay_display, "_NET_WM_PID", False);
    XChangeProperty(overlay_display, overlay_window, net_wm_pid, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)&pid, 1);
    previous_after_function =
        XSetAfterFunction(overlay_display, overlay_x11_after_request);
    XMapWindow(overlay_display, overlay_window);
    XFlush(overlay_display);
    overlay_initialized = 1;
    overlay_next_init_retry_ms = 0;
    overlay_wait_logged = 0;
    pthread_mutex_unlock(&overlay_mutex);

    overlay_trace("created X11 input proxy window %#lx\n", overlay_window);
    return 1;

retry:
    overlay_next_init_retry_ms = now + 200;
    if (!overlay_wait_logged)
    {
        overlay_trace("waiting for gameoverlayrenderer X11 hooks\n");
        overlay_wait_logged = 1;
    }
    pthread_mutex_unlock(&overlay_mutex);
    return 0;

disabled:
    overlay_initialized = -1;
    pthread_mutex_unlock(&overlay_mutex);
    return 0;
}

static void sync_overlay_focus(void)
{
    XEvent event = {0};
    Window current_focus;
    Window selection_owner;
    int advertise_focus = 0;
    int focused;
    int revert_to;

    pthread_mutex_lock(&overlay_mutex);
    if (overlay_initialized <= 0 || !overlay_display)
    {
        pthread_mutex_unlock(&overlay_mutex);
        return;
    }

    focused = overlay_requested_focus;
    if (overlay_advertised_focus == focused)
    {
        pthread_mutex_unlock(&overlay_mutex);
        return;
    }

    XLockDisplay(overlay_display);
    if (focused)
    {
        selection_owner =
            XGetSelectionOwner(overlay_display, overlay_owner_atom);
        if (selection_owner != overlay_window)
        {
            /* The native Wayland process that actually has keyboard focus
             * must own the per-game proxy.  Transfer ownership directly from
             * a launcher or an earlier process instead of waiting for that
             * process to exit and retrying from a foreign helper thread. */
            XSetSelectionOwner(overlay_display, overlay_owner_atom,
                               overlay_window, CurrentTime);
            XSync(overlay_display, False);
        }
        overlay_focus_owner = XGetSelectionOwner(
            overlay_display, overlay_owner_atom) == overlay_window;

        if (overlay_focus_owner)
        {
            XSetInputFocus(overlay_display, overlay_window,
                           RevertToParent, CurrentTime);
            advertise_focus = 1;
        }
    }
    else if (overlay_focus_owner)
    {
        XGetInputFocus(overlay_display, &current_focus, &revert_to);
        if (current_focus == overlay_window)
            XSetInputFocus(overlay_display, PointerRoot,
                           RevertToPointerRoot, CurrentTime);

        /* Retain ownership until bridge_destroy(). Releasing it on every
         * FocusOut makes gameoverlayrenderer tear down and reacquire its
         * per-game input path while the game is still alive. */
        advertise_focus = 1;
    }
    else
    {
        overlay_advertised_focus = 0;
    }
    XFlush(overlay_display);
    XUnlockDisplay(overlay_display);

    if (advertise_focus) overlay_advertised_focus = focused;
    pthread_mutex_unlock(&overlay_mutex);

    if (!advertise_focus)
        return;

    overlay_trace("X11 focus proxy is now %s\n",
                  focused ? "focused" : "unfocused");
    event.type = focused ? FocusIn : FocusOut;
    event.xfocus.mode = NotifyNormal;
    event.xfocus.detail = NotifyNonlinear;
    forward_overlay_x11_event(&event);
    update_overlay_active();
}

static void update_overlay_focus(void)
{
    sync_overlay_focus();
}

static int dispatch_overlay_event(XEvent *event)
{
    int active;
    int consumed;

    if (!init_overlay_bridge(0)) return 0;
    update_overlay_focus();
    consumed = forward_overlay_x11_event(event);

    if (event->type == MotionNotify)
        update_overlay_pointer_ownership(consumed);

    active = update_overlay_active();
    if (active) apply_overlay_cursor(0);
    return consumed || active;
}

static void update_key_state(uint32_t key, int pressed)
{
    unsigned int mask = 0;

    switch (key)
    {
    case KEY_LEFTSHIFT:
    case KEY_RIGHTSHIFT:
        mask = ShiftMask;
        break;
    case KEY_LEFTCTRL:
    case KEY_RIGHTCTRL:
        mask = ControlMask;
        break;
    case KEY_LEFTALT:
    case KEY_RIGHTALT:
        mask = Mod1Mask;
        break;
    case KEY_LEFTMETA:
    case KEY_RIGHTMETA:
        mask = Mod4Mask;
        break;
    case KEY_CAPSLOCK:
        if (pressed) overlay_state ^= LockMask;
        return;
    case KEY_NUMLOCK:
        if (pressed) overlay_state ^= Mod2Mask;
        return;
    default:
        return;
    }

    if (pressed) overlay_state |= mask;
    else overlay_state &= ~mask;
}

static unsigned int button_to_xbutton(uint32_t button)
{
    switch (button)
    {
    case BTN_LEFT:
        return Button1;
    case BTN_MIDDLE:
        return Button2;
    case BTN_RIGHT:
        return Button3;
    case BTN_SIDE:
    case BTN_BACK:
        return 8;
    case BTN_EXTRA:
    case BTN_FORWARD:
        return 9;
    default:
        return 0;
    }
}

static unsigned int button_to_mask(unsigned int button)
{
    switch (button)
    {
    case Button1:
        return Button1Mask;
    case Button2:
        return Button2Mask;
    case Button3:
        return Button3Mask;
    case Button4:
        return Button4Mask;
    case Button5:
        return Button5Mask;
    default:
        return 0;
    }
}

static int dispatch_button(uint32_t time, unsigned int button, int pressed)
{
    XEvent event = {0};
    int consumed;

    event.type = pressed ? ButtonPress : ButtonRelease;
    event.xbutton.root = overlay_root;
    event.xbutton.subwindow = None;
    event.xbutton.time = time;
    event.xbutton.x = pointer_x;
    event.xbutton.y = pointer_y;
    event.xbutton.x_root = pointer_x;
    event.xbutton.y_root = pointer_y;
    event.xbutton.state = overlay_state;
    event.xbutton.button = button;
    event.xbutton.same_screen = True;

    consumed = dispatch_overlay_event(&event);
    if (pressed) overlay_state |= button_to_mask(button);
    else overlay_state &= ~button_to_mask(button);
    return consumed;
}

static int dispatch_wheel(uint32_t time, int value, int horizontal)
{
    unsigned int button;
    int count;
    int i;
    int consumed = 0;

    if (!value) return update_overlay_active();

    button = horizontal ? (value > 0 ? 7 : 6) :
                          (value > 0 ? Button4 : Button5);
    count = abs(value) / GE_WHEEL_DELTA;
    if (count > 32) count = 32;
    for (i = 0; i < count && !consumed; ++i)
    {
        consumed = dispatch_button(time, button, 1);
        if (!consumed) consumed = dispatch_button(time, button, 0);
    }

    return consumed || update_overlay_active();
}

static void bridge_focus(int focused)
{
    pthread_mutex_lock(&overlay_mutex);
    overlay_requested_focus = !!focused;
    pthread_mutex_unlock(&overlay_mutex);

    if (!init_overlay_bridge(1)) return;
    update_overlay_focus();
}

static int bridge_filter_key(uint32_t time, uint32_t key, int pressed)
{
    XEvent event = {0};
    int consumed;

    if (!init_overlay_bridge(1)) return 0;
    update_overlay_focus();
    if (key > 247) return update_overlay_active();

    event.type = pressed ? KeyPress : KeyRelease;
    event.xkey.root = overlay_root;
    event.xkey.subwindow = None;
    event.xkey.time = time;
    event.xkey.x = pointer_x;
    event.xkey.y = pointer_y;
    event.xkey.x_root = pointer_x;
    event.xkey.y_root = pointer_y;
    event.xkey.state = overlay_state;
    event.xkey.keycode = key + 8;
    event.xkey.same_screen = True;

    consumed = dispatch_overlay_event(&event);
    update_key_state(key, pressed);
    return consumed;
}

static int bridge_filter_pointer_button(uint32_t time, uint32_t button,
                                        int pressed)
{
    unsigned int xbutton;

    if (!init_overlay_bridge(1)) return 0;
    update_overlay_focus();
    if (!(xbutton = button_to_xbutton(button))) return update_overlay_active();
    return dispatch_button(time, xbutton, pressed);
}

static int bridge_filter_pointer_frame(
    const struct ge_steam_overlay_pointer_frame_v1 *frame)
{
    XEvent event = {0};
    int consumed = 0;
    int scroll = 0;
    int horz_scroll = 0;

    if (!frame || !init_overlay_bridge(0)) return 0;
    update_overlay_focus();

    if (frame->flags & GE_STEAM_OVERLAY_FRAME_ABSOLUTE)
    {
        pthread_mutex_lock(&overlay_mutex);
        pointer_x = frame->x;
        pointer_y = frame->y;
        pthread_mutex_unlock(&overlay_mutex);
    }
    else if (frame->flags & GE_STEAM_OVERLAY_FRAME_RELATIVE)
    {
        int width;
        int height;

        pthread_mutex_lock(&overlay_mutex);
        pointer_x += (int)round(frame->dx);
        pointer_y += (int)round(frame->dy);
        width = DisplayWidth(overlay_display, DefaultScreen(overlay_display));
        height = DisplayHeight(overlay_display, DefaultScreen(overlay_display));
        if (pointer_x < 0) pointer_x = 0;
        else if (pointer_x >= width) pointer_x = width - 1;
        if (pointer_y < 0) pointer_y = 0;
        else if (pointer_y >= height) pointer_y = height - 1;
        pthread_mutex_unlock(&overlay_mutex);
    }

    if (frame->flags & (GE_STEAM_OVERLAY_FRAME_ABSOLUTE |
                        GE_STEAM_OVERLAY_FRAME_RELATIVE))
    {
        pthread_mutex_lock(&overlay_mutex);
        event.type = MotionNotify;
        event.xmotion.root = overlay_root;
        event.xmotion.subwindow = None;
        event.xmotion.time = frame->time;
        event.xmotion.x = pointer_x;
        event.xmotion.y = pointer_y;
        event.xmotion.x_root = pointer_x;
        event.xmotion.y_root = pointer_y;
        event.xmotion.state = overlay_state;
        event.xmotion.is_hint = NotifyNormal;
        event.xmotion.same_screen = True;
        pthread_mutex_unlock(&overlay_mutex);
        consumed = dispatch_overlay_event(&event);
    }

    if (frame->flags & GE_STEAM_OVERLAY_FRAME_DISCRETE_WHEEL)
        scroll = frame->scroll;
    else if (frame->flags & GE_STEAM_OVERLAY_FRAME_AXIS)
        scroll = (int)trunc(frame->axis / GE_WHEEL_DELTA) * GE_WHEEL_DELTA;

    if (frame->flags & GE_STEAM_OVERLAY_FRAME_DISCRETE_WHEEL_HORZ)
        horz_scroll = frame->horz_scroll;
    else if (frame->flags & GE_STEAM_OVERLAY_FRAME_AXIS_HORZ)
        horz_scroll =
            (int)trunc(frame->horz_axis / GE_WHEEL_DELTA) * GE_WHEEL_DELTA;

    if (!consumed) consumed = dispatch_wheel(frame->time, scroll, 0);
    if (!consumed) consumed = dispatch_wheel(frame->time, horz_scroll, 1);
    return consumed || update_overlay_active();
}

static int bridge_is_active(void)
{
    if (!init_overlay_bridge(0)) return 0;
    update_overlay_focus();
    return update_overlay_active();
}

static void bridge_enable(void)
{
    pthread_mutex_lock(&overlay_mutex);
    if (overlay_bridge_suspended)
    {
        overlay_bridge_suspended = 0;
        overlay_trace("re-enabled bridge for a new Wayland toplevel\n");
    }
    pthread_mutex_unlock(&overlay_mutex);

    /* Toplevel creation normally follows gameoverlayrenderer loading. Set up
     * the proxy now instead of dropping the first shortcut while waiting for
     * a passive retry interval to expire. */
    if (init_overlay_bridge(1)) update_overlay_focus();
}

static void bridge_destroy(void)
{
    Window current_focus;
    int restore_pointer = 0;
    int revert_to;

    pthread_mutex_lock(&overlay_mutex);
    overlay_requested_focus = 0;
    overlay_bridge_suspended = 1;
    overlay_advertised_focus = -1;
    overlay_next_init_retry_ms = 0;
    overlay_wait_logged = 0;
    if (overlay_initialized <= 0)
    {
        overlay_initialized = 0;
        overlay_check_if_event = NULL;
        pthread_mutex_unlock(&overlay_mutex);
        return;
    }

    restore_pointer = overlay_active || overlay_input_active;
    if (overlay_host.set_overlay_event_owned)
        overlay_host.set_overlay_event_owned(overlay_host.userdata, 0);

    XLockDisplay(overlay_display);
    if (overlay_focus_owner &&
        XGetSelectionOwner(overlay_display, overlay_owner_atom) == overlay_window)
        XSetSelectionOwner(overlay_display, overlay_owner_atom, None, CurrentTime);

    XGetInputFocus(overlay_display, &current_focus, &revert_to);
    if (current_focus == overlay_window)
        XSetInputFocus(overlay_display, PointerRoot,
                       RevertToPointerRoot, CurrentTime);

    XSetAfterFunction(overlay_display, previous_after_function);
    XDestroyWindow(overlay_display, overlay_window);
    XSync(overlay_display, False);
    XUnlockDisplay(overlay_display);
    XCloseDisplay(overlay_display);

    overlay_display = NULL;
    overlay_window = None;
    overlay_root = None;
    overlay_owner_atom = None;
    previous_after_function = NULL;
    overlay_serial = 0;
    overlay_state = 0;
    overlay_initialized = 0;
    overlay_active = 0;
    overlay_input_active = 0;
    overlay_focus_owner = 0;
    overlay_check_if_event = NULL;
    pointer_x = 0;
    pointer_y = 0;

    pthread_mutex_lock(&cursor_mutex);
    memset(overlay_cursor_map, 0, sizeof(overlay_cursor_map));
    overlay_cursor_shape = GE_STEAM_OVERLAY_CURSOR_DEFAULT;
    overlay_cursor_dirty = 0;
    pthread_mutex_unlock(&cursor_mutex);
    pthread_mutex_unlock(&overlay_mutex);

    if (restore_pointer && overlay_host.set_overlay_active)
        overlay_host.set_overlay_active(overlay_host.userdata, 0);
    overlay_trace("destroyed X11 input proxy\n");
}

static const struct ge_steam_overlay_api_v1 overlay_api = {
    .abi_version = GE_STEAM_OVERLAY_BRIDGE_ABI_VERSION,
    .struct_size = sizeof(overlay_api),
    .enable = bridge_enable,
    .destroy = bridge_destroy,
    .focus = bridge_focus,
    .filter_key = bridge_filter_key,
    .filter_pointer_button = bridge_filter_pointer_button,
    .filter_pointer_frame = bridge_filter_pointer_frame,
    .is_active = bridge_is_active,
    .set_cursor_shape = bridge_set_cursor_shape,
};

__attribute__((visibility("default")))
const struct ge_steam_overlay_api_v1 *
ge_steam_overlay_bridge_get_v1(uint32_t abi_version,
                               const struct ge_steam_overlay_host_v1 *host)
{
    if (abi_version != GE_STEAM_OVERLAY_BRIDGE_ABI_VERSION || !host ||
        host->abi_version != GE_STEAM_OVERLAY_BRIDGE_ABI_VERSION ||
        host->struct_size < sizeof(*host) || !host->set_overlay_active ||
        !host->set_cursor_shape || !host->overlay_event_is_active ||
        !host->set_overlay_event_owned)
        return NULL;

    pthread_mutex_lock(&overlay_mutex);
    overlay_host = *host;
    pthread_mutex_unlock(&overlay_mutex);
    return &overlay_api;
}
