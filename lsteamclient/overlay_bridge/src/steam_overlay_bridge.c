/*
 * Steam overlay X11 compatibility bridge for native Wine Wayland games.
 *
 * Wayland input is collected by the Vulkan layer on a private event queue and
 * translated here for Steam's existing X11 overlay hooks.
 */

#include "steam_overlay_bridge.h"

#include <dlfcn.h>
#include <linux/input.h>
#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define GE_WHEEL_DELTA 120
#define GE_CURSOR_MAP_SIZE 64
#define GE_EVENT_ALL_ACCESS 0x001f0003u
#define GE_OBJ_CASE_INSENSITIVE 0x00000040u
#define GE_OBJ_OPENIF 0x00000080u
#define GE_PROCESS_SESSION_INFORMATION 24

typedef int32_t wine_ntstatus_t;
typedef void *wine_handle_t;

struct wine_unicode_string
{
    uint16_t length;
    uint16_t maximum_length;
    uint16_t *buffer;
};

struct wine_object_attributes
{
    uint32_t length;
    wine_handle_t root_directory;
    struct wine_unicode_string *object_name;
    uint32_t attributes;
    void *security_descriptor;
    void *security_quality_of_service;
};

typedef wine_ntstatus_t (*wine_nt_close_fn)(wine_handle_t);
typedef wine_ntstatus_t (*wine_nt_create_event_fn)(
    wine_handle_t *, uint32_t, const struct wine_object_attributes *, int,
    uint8_t);
typedef wine_ntstatus_t (*wine_nt_query_information_process_fn)(
    wine_handle_t, int, void *, uint32_t, uint32_t *);
typedef wine_ntstatus_t (*wine_nt_reset_event_fn)(wine_handle_t, int32_t *);
typedef wine_ntstatus_t (*wine_nt_set_event_fn)(wine_handle_t, int32_t *);
typedef wine_ntstatus_t (*wine_nt_wait_for_single_object_fn)(
    wine_handle_t, uint8_t, const int64_t *);

struct wine_ntdll_api
{
    void *module;
    wine_nt_close_fn close;
    wine_nt_create_event_fn create_event;
    wine_nt_query_information_process_fn query_information_process;
    wine_nt_reset_event_fn reset_event;
    wine_nt_set_event_fn set_event;
    wine_nt_wait_for_single_object_fn wait_for_single_object;
};

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
typedef void (*vulkan_steam_overlay_set_window_type_fn)(uintptr_t, int);
typedef int (*overlay_needs_present_fn)(void);
typedef int (*overlay_is_enabled_fn)(void);
typedef void (*overlay_input_stream_write_fn)(void *, const void *, size_t);

#define STEAM_OVERLAY_WINDOW_TYPE_XLIB 2
#define STEAM_OVERLAY_INPUT_SOURCE_X11 2
#define STEAM_OVERLAY_INPUT_EVENT_CHARACTER 0x102
#define STEAM_OVERLAY_INPUT_EVENT_SIZE 36

/* Bypass gameoverlayrenderer's XPutBackEvent wrapper. The event must first be
 * observed by its wrapped XCheckIfEvent call below. */
extern int _XPutBackEvent(Display *display, XEvent *event);

static pthread_mutex_t overlay_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t cursor_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t overlay_event_api_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t overlay_event_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct wine_ntdll_api wine_ntdll;
static wine_handle_t overlay_event;
static int overlay_event_owned;
static Display *overlay_display;
static Window overlay_window;
static Window overlay_root;
static Atom overlay_owner_atom;
static int (*previous_after_function)(Display *display);
static xcheck_if_event_fn overlay_check_if_event;
static vulkan_steam_overlay_set_window_type_fn overlay_set_window_type;
static overlay_needs_present_fn overlay_needs_present;
static overlay_is_enabled_fn overlay_is_enabled;
static void **overlay_input_stream_slot;
static void *overlay_renderer_base;
static int overlay_input_stream_scanned;
static int *overlay_screen_width_slot;
static int *overlay_screen_height_slot;
static int overlay_screen_size_scanned;
static unsigned long overlay_serial;
static unsigned int overlay_state;
static int overlay_initialized;
static int overlay_active;
static int overlay_input_active;
static int overlay_focus_owner;
static int overlay_requested_focus;
static int overlay_advertised_focus = -1;
static int overlay_bridge_suspended;
static unsigned int overlay_surface_count;
static unsigned int overlay_focused_surface_count;
static uint64_t overlay_next_init_retry_ms;
static int overlay_wait_logged;
static int pointer_x;
static int pointer_y;
static struct overlay_cursor_entry overlay_cursor_map[GE_CURSOR_MAP_SIZE];
static uint32_t overlay_cursor_shape = GE_STEAM_OVERLAY_CURSOR_DEFAULT;
static int overlay_cursor_dirty;
static int overlay_opengl_requested;
static GLXContext overlay_glx_context;
static Colormap overlay_glx_colormap;
static int overlay_glx_requested_x;
static int overlay_glx_requested_y;
static int overlay_glx_requested_width;
static int overlay_glx_requested_height;
static int overlay_glx_geometry_dirty;
static pthread_mutex_t overlay_glx_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t overlay_glx_cond = PTHREAD_COND_INITIALIZER;
static pthread_t overlay_glx_thread;
static unsigned long overlay_glx_frame;
static int overlay_glx_thread_running;
static int overlay_glx_thread_ready;
static int overlay_glx_thread_failed;
static int overlay_glx_stop;

static int request_overlay_control_state(int closed);

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

static void load_wine_ntdll_api(void)
{
    void *module;

    module = dlopen("ntdll.so", RTLD_NOW | RTLD_NOLOAD);
    if (!module) return;

    wine_ntdll.close = (wine_nt_close_fn)dlsym(module, "NtClose");
    wine_ntdll.create_event =
        (wine_nt_create_event_fn)dlsym(module, "NtCreateEvent");
    wine_ntdll.query_information_process =
        (wine_nt_query_information_process_fn)dlsym(
            module, "NtQueryInformationProcess");
    wine_ntdll.reset_event =
        (wine_nt_reset_event_fn)dlsym(module, "NtResetEvent");
    wine_ntdll.set_event =
        (wine_nt_set_event_fn)dlsym(module, "NtSetEvent");
    wine_ntdll.wait_for_single_object =
        (wine_nt_wait_for_single_object_fn)dlsym(
            module, "NtWaitForSingleObject");

    if (!wine_ntdll.close || !wine_ntdll.create_event ||
        !wine_ntdll.query_information_process || !wine_ntdll.reset_event ||
        !wine_ntdll.set_event || !wine_ntdll.wait_for_single_object)
    {
        memset(&wine_ntdll, 0, sizeof(wine_ntdll));
        dlclose(module);
        return;
    }

    wine_ntdll.module = module;
}

static int create_overlay_event_locked(void)
{
    struct wine_object_attributes attributes;
    struct wine_unicode_string name;
    uint16_t path_w[256];
    uint32_t session_id;
    char path[256];
    wine_ntstatus_t status;
    int length;
    int i;

    if (overlay_event) return 1;

    pthread_once(&overlay_event_api_once, load_wine_ntdll_api);
    if (!wine_ntdll.module) return 0;

    status = wine_ntdll.query_information_process(
        (wine_handle_t)(intptr_t)-1, GE_PROCESS_SESSION_INFORMATION,
        &session_id, sizeof(session_id), NULL);
    if (status < 0) return 0;

    length = snprintf(
        path, sizeof(path),
        "\\Sessions\\%u\\BaseNamedObjects\\__wine_steamclient_GameOverlayActivated",
        session_id);
    if (length < 0 || (size_t)length >= sizeof(path) ||
        (size_t)length >= sizeof(path_w) / sizeof(path_w[0]))
        return 0;

    for (i = 0; i <= length; ++i) path_w[i] = (unsigned char)path[i];

    name.length = (uint16_t)(length * sizeof(path_w[0]));
    name.maximum_length = (uint16_t)((length + 1) * sizeof(path_w[0]));
    name.buffer = path_w;

    memset(&attributes, 0, sizeof(attributes));
    attributes.length = sizeof(attributes);
    attributes.object_name = &name;
    attributes.attributes = GE_OBJ_CASE_INSENSITIVE | GE_OBJ_OPENIF;

    status = wine_ntdll.create_event(
        &overlay_event, GE_EVENT_ALL_ACCESS, &attributes,
        0 /* NotificationEvent */, 0);
    if (status < 0 || !overlay_event)
    {
        overlay_event = NULL;
        return 0;
    }

    overlay_trace("opened shared Steam overlay input event\n");
    return 1;
}

static void set_overlay_event_active(int active)
{
    int64_t timeout = 0;

    pthread_mutex_lock(&overlay_event_mutex);
    if (!create_overlay_event_locked())
    {
        pthread_mutex_unlock(&overlay_event_mutex);
        return;
    }

    if (active)
    {
        if (wine_ntdll.wait_for_single_object(overlay_event, 0, &timeout))
        {
            if (!wine_ntdll.set_event(overlay_event, NULL))
            {
                overlay_event_owned = 1;
                overlay_trace("signaled shared Steam overlay input event\n");
            }
        }
    }
    else if (overlay_event_owned)
    {
        wine_ntdll.reset_event(overlay_event, NULL);
        overlay_event_owned = 0;
        overlay_trace("reset shared Steam overlay input event\n");
    }
    pthread_mutex_unlock(&overlay_event_mutex);
}

static void destroy_overlay_event(void)
{
    pthread_mutex_lock(&overlay_event_mutex);
    if (overlay_event)
    {
        if (overlay_event_owned)
            wine_ntdll.reset_event(overlay_event, NULL);
        wine_ntdll.close(overlay_event);
    }
    overlay_event = NULL;
    overlay_event_owned = 0;
    pthread_mutex_unlock(&overlay_event_mutex);
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

static XVisualInfo *choose_overlay_glx_visual(Display *display, int screen)
{
    static const int attributes[] =
    {
        GLX_X_RENDERABLE, True,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        GLX_DOUBLEBUFFER, True,
        None,
    };
    GLXFBConfig *configs;
    XVisualInfo *best = NULL;
    int count = 0;
    int i;

    if (!(configs = glXChooseFBConfig(display, screen, attributes, &count)))
        return NULL;

    for (i = 0; i < count; ++i)
    {
        XVisualInfo *visual = glXGetVisualFromFBConfig(display, configs[i]);
        int alpha = 0;

        if (!visual) continue;
        glXGetFBConfigAttrib(display, configs[i], GLX_ALPHA_SIZE, &alpha);
        if (alpha < 8)
        {
            XFree(visual);
            continue;
        }

        if (!best || visual->depth == 32)
        {
            if (best) XFree(best);
            best = visual;
            if (visual->depth == 32) break;
        }
        else
        {
            XFree(visual);
        }
    }

    XFree(configs);
    return best;
}

static void *run_opengl_presenter(void *arg)
{
    enum
    {
        OVERLAY_PRIME_FIRST_SWAP,
        OVERLAY_PRIME_WAIT_FOCUS,
        OVERLAY_PRIME_WAIT_ENABLE,
        OVERLAY_PRIME_WAIT_DISABLE,
        OVERLAY_PRIME_COMPLETE,
    } prime_state = OVERLAY_PRIME_FIRST_SWAP;
    uint64_t prime_deadline = 0;
    unsigned long frame = 0;
    int context_current;

    (void)arg;
    /* Use the classic GLX calls intercepted by gameoverlayrenderer. */
    context_current = glXMakeCurrent(overlay_display, overlay_window,
                                     overlay_glx_context);
    pthread_mutex_lock(&overlay_glx_mutex);
    overlay_glx_thread_ready = 1;
    overlay_glx_thread_failed = !context_current;
    pthread_cond_broadcast(&overlay_glx_cond);
    pthread_mutex_unlock(&overlay_glx_mutex);
    if (!context_current)
    {
        overlay_trace("failed to make isolated GLX overlay context current\n");
        return NULL;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    for (;;)
    {
        int geometry_dirty;
        int x, y, width, height;
        int stop;

        pthread_mutex_lock(&overlay_glx_mutex);
        while (!overlay_glx_stop && frame == overlay_glx_frame)
            pthread_cond_wait(&overlay_glx_cond, &overlay_glx_mutex);
        stop = overlay_glx_stop;
        frame = overlay_glx_frame;
        geometry_dirty = overlay_glx_geometry_dirty;
        x = overlay_glx_requested_x;
        y = overlay_glx_requested_y;
        width = overlay_glx_requested_width;
        height = overlay_glx_requested_height;
        if (geometry_dirty)
            overlay_glx_geometry_dirty = 0;
        pthread_mutex_unlock(&overlay_glx_mutex);
        if (stop) break;

        if (geometry_dirty)
        {
            XMoveResizeWindow(overlay_display, overlay_window, x, y,
                              (unsigned int)width, (unsigned int)height);
            XRaiseWindow(overlay_display, overlay_window);
            XSync(overlay_display, False);
            overlay_trace("resized isolated GLX overlay to %dx%d%+d%+d\n",
                          width, height, x, y);
        }

        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);

        if (prime_state == OVERLAY_PRIME_WAIT_FOCUS)
        {
            int focused;

            pthread_mutex_lock(&overlay_mutex);
            focused = overlay_requested_focus && overlay_focus_owner;
            pthread_mutex_unlock(&overlay_mutex);

            if (focused && request_overlay_control_state(0))
            {
                prime_state = OVERLAY_PRIME_WAIT_ENABLE;
                prime_deadline = monotonic_msec() + 1000;
                overlay_trace("requested hidden OpenGL overlay initialization\n");
            }
        }
        else if (prime_state == OVERLAY_PRIME_WAIT_ENABLE)
        {
            if ((overlay_is_enabled && overlay_is_enabled()) ||
                monotonic_msec() >= prime_deadline)
            {
                if (request_overlay_control_state(1))
                {
                    prime_state = OVERLAY_PRIME_WAIT_DISABLE;
                    prime_deadline = monotonic_msec() + 1000;
                    overlay_trace("requested initial OpenGL overlay closure\n");
                }
            }
        }
        else if (prime_state == OVERLAY_PRIME_WAIT_DISABLE)
        {
            if (!overlay_is_enabled || !overlay_is_enabled())
            {
                prime_state = OVERLAY_PRIME_COMPLETE;
                overlay_trace("completed hidden OpenGL overlay initialization\n");
            }
            else if (monotonic_msec() >= prime_deadline)
            {
                request_overlay_control_state(1);
                prime_deadline = monotonic_msec() + 1000;
            }
        }

        /* This resolves to gameoverlayrenderer.so's GLX hook. It operates on
         * this private drawable and never sees Wine's EGL context or surface. */
        if (overlay_needs_present) overlay_needs_present();

        /* Initialize GLX tracking with one transparent swap. During the
         * activation/closure handshake, do not present Steam's temporary
         * active frame to the user. */
        if (prime_state == OVERLAY_PRIME_FIRST_SWAP)
        {
            glXSwapBuffers(overlay_display, overlay_window);
            prime_state = OVERLAY_PRIME_WAIT_FOCUS;
        }
        else if (prime_state == OVERLAY_PRIME_WAIT_FOCUS ||
                 prime_state == OVERLAY_PRIME_COMPLETE)
        {
            glXSwapBuffers(overlay_display, overlay_window);
        }
    }

    glXMakeCurrent(overlay_display, None, NULL);
    return NULL;
}

static int start_opengl_presenter(void)
{
    pthread_t thread;
    int failed;

    pthread_mutex_lock(&overlay_glx_mutex);
    overlay_glx_stop = 0;
    overlay_glx_frame = 0;
    if (overlay_glx_requested_width < 1) overlay_glx_requested_width = 1;
    if (overlay_glx_requested_height < 1) overlay_glx_requested_height = 1;
    overlay_glx_geometry_dirty = 0;
    overlay_glx_thread_ready = 0;
    overlay_glx_thread_failed = 0;
    if (pthread_create(&overlay_glx_thread, NULL, run_opengl_presenter, NULL))
    {
        pthread_mutex_unlock(&overlay_glx_mutex);
        overlay_trace("failed to start isolated GLX overlay thread\n");
        return 0;
    }
    overlay_glx_thread_running = 1;
    while (!overlay_glx_thread_ready)
        pthread_cond_wait(&overlay_glx_cond, &overlay_glx_mutex);
    failed = overlay_glx_thread_failed;
    thread = overlay_glx_thread;
    pthread_mutex_unlock(&overlay_glx_mutex);

    if (failed)
    {
        pthread_join(thread, NULL);
        pthread_mutex_lock(&overlay_glx_mutex);
        overlay_glx_thread_running = 0;
        overlay_glx_thread_ready = 0;
        overlay_glx_thread_failed = 0;
        pthread_mutex_unlock(&overlay_glx_mutex);
        return 0;
    }

    overlay_trace("started isolated GLX overlay presenter\n");
    return 1;
}

static void stop_opengl_presenter(void)
{
    pthread_t thread;
    int join = 0;

    pthread_mutex_lock(&overlay_glx_mutex);
    if (overlay_glx_thread_running)
    {
        overlay_glx_stop = 1;
        pthread_cond_broadcast(&overlay_glx_cond);
        thread = overlay_glx_thread;
        join = 1;
    }
    pthread_mutex_unlock(&overlay_glx_mutex);

    if (join) pthread_join(thread, NULL);

    pthread_mutex_lock(&overlay_glx_mutex);
    overlay_glx_thread_running = 0;
    overlay_glx_thread_ready = 0;
    overlay_glx_thread_failed = 0;
    overlay_glx_stop = 0;
    overlay_glx_frame = 0;
    overlay_glx_geometry_dirty = 0;
    pthread_mutex_unlock(&overlay_glx_mutex);
}

static void destroy_opengl_presenter_resources(void)
{
    if (overlay_display && overlay_glx_context)
        glXDestroyContext(overlay_display, overlay_glx_context);
    if (overlay_display && overlay_glx_colormap)
        XFreeColormap(overlay_display, overlay_glx_colormap);

    overlay_glx_context = NULL;
    overlay_glx_colormap = None;
    overlay_glx_requested_x = 0;
    overlay_glx_requested_y = 0;
    overlay_glx_requested_width = 0;
    overlay_glx_requested_height = 0;
    overlay_glx_geometry_dirty = 0;
}

void ge_overlay_bridge_enable_opengl_presenter(int32_t x, int32_t y,
                                               uint32_t width,
                                               uint32_t height)
{
    if (!width) width = 1;
    if (!height) height = 1;
    if (width > 65535) width = 65535;
    if (height > 65535) height = 65535;

    pthread_mutex_lock(&overlay_glx_mutex);
    overlay_glx_requested_x = x;
    overlay_glx_requested_y = y;
    overlay_glx_requested_width = width;
    overlay_glx_requested_height = height;
    overlay_glx_geometry_dirty = 0;
    pthread_mutex_unlock(&overlay_glx_mutex);

    pthread_mutex_lock(&overlay_mutex);
    overlay_opengl_requested = 1;
    pthread_mutex_unlock(&overlay_mutex);
}

void ge_overlay_bridge_present_opengl(int32_t x, int32_t y,
                                      uint32_t width, uint32_t height)
{
    if (!width) width = 1;
    if (!height) height = 1;
    if (width > 65535) width = 65535;
    if (height > 65535) height = 65535;

    pthread_mutex_lock(&overlay_glx_mutex);
    if (overlay_glx_thread_running)
    {
        if (overlay_glx_requested_x != x || overlay_glx_requested_y != y ||
            overlay_glx_requested_width != (int)width ||
            overlay_glx_requested_height != (int)height)
        {
            overlay_glx_requested_x = x;
            overlay_glx_requested_y = y;
            overlay_glx_requested_width = width;
            overlay_glx_requested_height = height;
            overlay_glx_geometry_dirty = 1;
        }
        ++overlay_glx_frame;
        pthread_cond_signal(&overlay_glx_cond);
    }
    pthread_mutex_unlock(&overlay_glx_mutex);
}

static int resolve_overlay_renderer_hook(void)
{
    Dl_info info;
    Dl_info needs_present_info;

    if (overlay_check_if_event) return 1;

    /* Use the process-interposed symbol exactly as an X11 application does.
     * dlsym() on gameoverlayrenderer's handle is allowed to search its
     * dependencies and can silently return libX11's unwrapped implementation. */
    if (!dladdr((void *)XCheckIfEvent, &info) || !info.dli_fname ||
        !strstr(info.dli_fname, "gameoverlayrenderer"))
        return 0;

    overlay_check_if_event = XCheckIfEvent;
    overlay_renderer_base = info.dli_fbase;
    overlay_set_window_type =
        (vulkan_steam_overlay_set_window_type_fn)dlsym(
            RTLD_DEFAULT, "VulkanSteamOverlaySetWindowType");
    overlay_needs_present =
        (overlay_needs_present_fn)dlsym(RTLD_DEFAULT, "BOverlayNeedsPresent");
    if (!overlay_needs_present ||
        !dladdr((void *)overlay_needs_present, &needs_present_info) ||
        needs_present_info.dli_fbase != overlay_renderer_base)
        overlay_needs_present = NULL;
    overlay_is_enabled =
        (overlay_is_enabled_fn)dlsym(RTLD_DEFAULT, "IsOverlayEnabled");
    if (overlay_is_enabled)
    {
        Dl_info enabled_info;

        if (!dladdr((void *)overlay_is_enabled, &enabled_info) ||
            enabled_info.dli_fbase != overlay_renderer_base)
            overlay_is_enabled = NULL;
    }
    overlay_trace("using XCheckIfEvent interposed by %s\n", info.dli_fname);
    if (overlay_needs_present)
        overlay_trace("using Steam overlay present-state poll hook\n");
    return 1;
}

static int resolve_overlay_input_stream(void)
{
#if defined(__x86_64__)
    /* Steam's X11 handler writes WM_CHAR-equivalent records to this private
     * stream after XwcLookupString(). The Wayland renderer has no XIC, so find
     * the same stream through a tightly checked instruction sequence and fail
     * closed when Steam changes its implementation. */
    static const unsigned char stream_load_suffix[] =
        {0x48, 0x8b, 0x07, 0xff, 0x50, 0x18};
    unsigned char *function;
    Dl_info function_info;
    Dl_info slot_info;
    int32_t displacement;
    unsigned int i;

    if (overlay_input_stream_slot) return 1;
    if (overlay_input_stream_scanned) return 0;

    function = (unsigned char *)dlsym(RTLD_DEFAULT, "BOverlayNeedsPresent");
    if (!function || !dladdr(function, &function_info) ||
        function_info.dli_fbase != overlay_renderer_base)
        return 0;

    overlay_input_stream_scanned = 1;
    for (i = 0; i + 13 <= 128; ++i)
    {
        void **slot;

        if (function[i] != 0x48 || function[i + 1] != 0x8b ||
            function[i + 2] != 0x3d ||
            memcmp(function + i + 7, stream_load_suffix,
                   sizeof(stream_load_suffix)))
            continue;

        memcpy(&displacement, function + i + 3, sizeof(displacement));
        slot = (void **)(function + i + 7 + displacement);
        if (!dladdr(slot, &slot_info) ||
            slot_info.dli_fbase != overlay_renderer_base)
            continue;

        overlay_input_stream_slot = slot;
        overlay_trace("resolved Steam overlay character event stream\n");
        return 1;
    }

    overlay_trace("Steam overlay character event stream signature not found\n");
#endif
    return 0;
}

static int request_overlay_control_state(int closed)
{
    overlay_input_stream_write_fn write_event;
    void **vtable;
    void *stream;
    Dl_info writer_info;
    int32_t state = !!closed;

    pthread_mutex_lock(&overlay_mutex);
    if (!resolve_overlay_input_stream() ||
        !(stream = *overlay_input_stream_slot) ||
        !(vtable = *(void ***)stream) ||
        !(write_event = (overlay_input_stream_write_fn)vtable[4]) ||
        !dladdr((void *)write_event, &writer_info) ||
        writer_info.dli_fbase != overlay_renderer_base)
    {
        pthread_mutex_unlock(&overlay_mutex);
        return 0;
    }

    /* Steam's base-input protocol uses 0 to request activation and 1 to
     * request closure. */
    write_event(stream, &state, sizeof(state));
    pthread_mutex_unlock(&overlay_mutex);
    return 1;
}

static int resolve_overlay_screen_size_slots(void)
{
#if defined(__x86_64__)
    static const unsigned char width_load[] = {0x8b, 0x44, 0x24, 0x08, 0x89, 0x05};
    static const unsigned char height_load[] = {0x8b, 0x44, 0x24, 0x0c, 0x89, 0x05};
    unsigned char *function;
    Dl_info function_info;
    Dl_info width_info;
    Dl_info height_info;
    int32_t displacement;
    unsigned int i;

    if (overlay_screen_width_slot && overlay_screen_height_slot) return 1;
    if (overlay_screen_size_scanned) return 0;
    overlay_screen_size_scanned = 1;

    function = (unsigned char *)dlsym(RTLD_DEFAULT, "glXSwapBuffers");
    if (!function || !dladdr(function, &function_info) ||
        function_info.dli_fbase != overlay_renderer_base)
        return 0;

    /* gameoverlayrenderer stores the dimensions returned by its internal
     * pre-swap path in two module globals. Resolve the stores themselves so
     * the passive overlay receives valid geometry before its first frame. */
    for (i = 0; i + 20 <= 256; ++i)
    {
        int *width;
        int *height;

        if (memcmp(function + i, width_load, sizeof(width_load)) ||
            memcmp(function + i + 10, height_load, sizeof(height_load)))
            continue;

        memcpy(&displacement, function + i + 6, sizeof(displacement));
        width = (int *)(function + i + 10 + displacement);
        memcpy(&displacement, function + i + 16, sizeof(displacement));
        height = (int *)(function + i + 20 + displacement);
        if (!dladdr(width, &width_info) || !dladdr(height, &height_info) ||
            width_info.dli_fbase != overlay_renderer_base ||
            height_info.dli_fbase != overlay_renderer_base)
            continue;

        overlay_screen_width_slot = width;
        overlay_screen_height_slot = height;
        overlay_trace("resolved Steam overlay screen-size slots\n");
        return 1;
    }

    overlay_trace("Steam overlay screen-size signature not found\n");
#endif
    return 0;
}

static void prime_overlay_screen_size(unsigned int width, unsigned int height)
{
    if (!resolve_overlay_screen_size_slots()) return;

    __atomic_store_n(overlay_screen_width_slot, (int)width, __ATOMIC_RELAXED);
    __atomic_store_n(overlay_screen_height_slot, (int)height, __ATOMIC_RELAXED);
    overlay_trace("primed Steam overlay screen size to %ux%u\n", width, height);
}

static int dispatch_overlay_character(uint32_t utf32)
{
    unsigned char event[STEAM_OVERLAY_INPUT_EVENT_SIZE] = {0};
    overlay_input_stream_write_fn write_event;
    uint32_t source = STEAM_OVERLAY_INPUT_SOURCE_X11;
    uint32_t message = STEAM_OVERLAY_INPUT_EVENT_CHARACTER;
    uint64_t window;
    int64_t character = utf32;
    void **vtable;
    void *stream;
    Dl_info writer_info;

    if (utf32 < 0x20 || utf32 == 0x7f || utf32 > 0x10ffff ||
        (utf32 >= 0xd800 && utf32 <= 0xdfff))
        return 0;

    pthread_mutex_lock(&overlay_mutex);
    if (overlay_initialized <= 0 || !overlay_window ||
        !resolve_overlay_input_stream() ||
        !(stream = *overlay_input_stream_slot) ||
        !(vtable = *(void ***)stream) ||
        !(write_event = (overlay_input_stream_write_fn)vtable[4]) ||
        !dladdr((void *)write_event, &writer_info) ||
        writer_info.dli_fbase != overlay_renderer_base)
    {
        pthread_mutex_unlock(&overlay_mutex);
        return 0;
    }

    window = overlay_window;
    memcpy(event, &source, sizeof(source));
    memcpy(event + 4, &window, sizeof(window));
    memcpy(event + 12, &message, sizeof(message));
    memcpy(event + 20, &character, sizeof(character));
    write_event(stream, event, sizeof(event));
    pthread_mutex_unlock(&overlay_mutex);
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

    if (dirty || force)
    {
        overlay_trace("applying overlay cursor shape %u\n", shape);
        ge_overlay_wayland_set_cursor_shape(shape);
    }
}

static int update_overlay_active(void)
{
    int active;
    int changed;

    pthread_mutex_lock(&overlay_mutex);
    active = overlay_input_active;
    changed = active != overlay_active;
    overlay_active = active;
    pthread_mutex_unlock(&overlay_mutex);

    if (active)
        set_overlay_event_active(1);
    else if (changed)
        set_overlay_event_active(0);

    if (changed)
    {
        overlay_trace("overlay is now %s\n", active ? "active" : "inactive");
        if (active) bridge_set_cursor_shape(GE_STEAM_OVERLAY_CURSOR_DEFAULT);
        ge_overlay_wayland_set_overlay_active(active);
    }

    return active;
}

static void update_overlay_input_ownership(int consumed)
{
    int changed;

    pthread_mutex_lock(&overlay_mutex);
    changed = consumed != overlay_input_active;
    overlay_input_active = consumed;
    pthread_mutex_unlock(&overlay_mutex);

    if (changed)
        overlay_trace("overlay %s ownership through X11 input\n",
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
    XVisualInfo *glx_visual = NULL;
    Visual *window_visual = (Visual *)CopyFromParent;
    const char *display_name;
    const char *env;
    char owner_selection[160];
    char window_class[128];
    unsigned long window_mask = CWOverrideRedirect | CWEventMask;
    unsigned long pid;
    unsigned long bypass_compositor = 0;
    unsigned int window_width = 1;
    unsigned int window_height = 1;
    int window_depth = CopyFromParent;
    int window_x = -1;
    int window_y = -1;
    int screen;
    Atom bypass_compositor_atom;
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

    screen = DefaultScreen(overlay_display);
    overlay_root = DefaultRootWindow(overlay_display);
    attributes.override_redirect = True;
    attributes.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask |
                            ButtonReleaseMask | PointerMotionMask;

    if (overlay_opengl_requested)
    {
        if (!(glx_visual = choose_overlay_glx_visual(overlay_display, screen)))
        {
            overlay_trace("failed to find an ARGB GLX visual\n");
            XCloseDisplay(overlay_display);
            overlay_display = NULL;
            goto retry;
        }

        pthread_mutex_lock(&overlay_glx_mutex);
        window_x = overlay_glx_requested_x;
        window_y = overlay_glx_requested_y;
        window_width = overlay_glx_requested_width ? overlay_glx_requested_width : 1;
        window_height = overlay_glx_requested_height ? overlay_glx_requested_height : 1;
        pthread_mutex_unlock(&overlay_glx_mutex);
        window_depth = glx_visual->depth;
        window_visual = glx_visual->visual;
        overlay_glx_colormap = XCreateColormap(
            overlay_display, overlay_root, window_visual, AllocNone);
        if (!overlay_glx_colormap)
        {
            XFree(glx_visual);
            glx_visual = NULL;
            XCloseDisplay(overlay_display);
            overlay_display = NULL;
            goto retry;
        }

        attributes.colormap = overlay_glx_colormap;
        attributes.background_pixel = 0;
        attributes.border_pixel = 0;
        window_mask |= CWColormap | CWBackPixel | CWBorderPixel;
    }

    overlay_window = XCreateWindow(
        overlay_display, overlay_root, window_x, window_y,
        window_width, window_height, 0, window_depth, InputOutput,
        window_visual, window_mask, &attributes);
    if (!overlay_window)
    {
        if (glx_visual) XFree(glx_visual);
        destroy_opengl_presenter_resources();
        XCloseDisplay(overlay_display);
        overlay_display = NULL;
        goto retry;
    }

    if (overlay_opengl_requested)
    {
        XserverRegion input_region;

        /* Steam's GLX renderer owns this drawable. Wayland input continues
        * through the private bridge queue, so the XWayland surface itself
         * must never intercept compositor input. */
        input_region = XFixesCreateRegion(overlay_display, NULL, 0);
        if (!input_region)
        {
            overlay_trace("failed to create empty GLX overlay input region\n");
            XFree(glx_visual);
            glx_visual = NULL;
            XDestroyWindow(overlay_display, overlay_window);
            overlay_window = None;
            destroy_opengl_presenter_resources();
            XCloseDisplay(overlay_display);
            overlay_display = NULL;
            goto retry;
        }
        XFixesSetWindowShapeRegion(overlay_display, overlay_window,
                                   ShapeInput, 0, 0, input_region);
        XFixesDestroyRegion(overlay_display, input_region);

        bypass_compositor_atom = XInternAtom(
            overlay_display, "_NET_WM_BYPASS_COMPOSITOR", False);
        XChangeProperty(overlay_display, overlay_window,
                        bypass_compositor_atom, XA_CARDINAL, 32,
                        PropModeReplace,
                        (unsigned char *)&bypass_compositor, 1);

        overlay_glx_context = glXCreateContext(
            overlay_display, glx_visual, NULL, True);
        XFree(glx_visual);
        glx_visual = NULL;
        if (!overlay_glx_context)
        {
            overlay_trace("failed to create isolated GLX overlay context\n");
            XDestroyWindow(overlay_display, overlay_window);
            overlay_window = None;
            destroy_opengl_presenter_resources();
            XCloseDisplay(overlay_display);
            overlay_display = NULL;
            goto retry;
        }
    }

    if (overlay_set_window_type)
    {
        overlay_set_window_type((uintptr_t)overlay_window,
                                STEAM_OVERLAY_WINDOW_TYPE_XLIB);
        overlay_trace("registered X11 input proxy as an Xlib overlay window\n");
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
    if (overlay_opengl_requested) XRaiseWindow(overlay_display, overlay_window);
    XSync(overlay_display, False);
    if (overlay_opengl_requested)
        prime_overlay_screen_size(window_width, window_height);
    if (overlay_opengl_requested && !start_opengl_presenter())
    {
        XSetAfterFunction(overlay_display, previous_after_function);
        previous_after_function = NULL;
        XDestroyWindow(overlay_display, overlay_window);
        overlay_window = None;
        destroy_opengl_presenter_resources();
        XCloseDisplay(overlay_display);
        overlay_display = NULL;
        goto retry;
    }
    overlay_initialized = 1;
    overlay_next_init_retry_ms = 0;
    overlay_wait_logged = 0;
    pthread_mutex_unlock(&overlay_mutex);

    overlay_trace("created X11 %s window %#lx\n",
                  overlay_opengl_requested ? "GLX overlay" : "input proxy",
                  overlay_window);
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
    if (!overlay_opengl_requested)
    {
        event.type = focused ? FocusIn : FocusOut;
        event.xfocus.mode = NotifyNormal;
        event.xfocus.detail = NotifyNonlinear;
        forward_overlay_x11_event(&event);
    }
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

    if (event->type == MotionNotify || event->type == KeyPress ||
        event->type == KeyRelease || event->type == ButtonPress ||
        event->type == ButtonRelease)
        update_overlay_input_ownership(consumed);

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

void ge_overlay_bridge_focus(int focused)
{
    pthread_mutex_lock(&overlay_mutex);
    if (focused)
        ++overlay_focused_surface_count;
    else if (overlay_focused_surface_count)
        --overlay_focused_surface_count;
    overlay_requested_focus = overlay_focused_surface_count != 0;
    pthread_mutex_unlock(&overlay_mutex);

    if (!init_overlay_bridge(1)) return;
    update_overlay_focus();
}

int ge_overlay_bridge_filter_key(uint32_t time, uint32_t key, int pressed,
                                 uint32_t utf32)
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
    if (pressed && update_overlay_active())
        dispatch_overlay_character(utf32);
    update_key_state(key, pressed);
    return consumed;
}

int ge_overlay_bridge_filter_pointer_button(uint32_t time, uint32_t button,
                                             int pressed)
{
    unsigned int xbutton;

    if (!init_overlay_bridge(1)) return 0;
    update_overlay_focus();
    if (!(xbutton = button_to_xbutton(button))) return update_overlay_active();
    return dispatch_button(time, xbutton, pressed);
}

int ge_overlay_bridge_filter_pointer_frame(
    const struct ge_steam_overlay_pointer_frame *frame)
{
    XEvent event = {0};
    int consumed = 0;
    int scroll = 0;
    int horz_scroll = 0;
    int motion_x = 0;
    int motion_y = 0;

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
        pthread_mutex_lock(&overlay_mutex);
        pointer_x += (int)round(frame->dx);
        pointer_y += (int)round(frame->dy);
        pointer_x = fmax(0, fmin(pointer_x,
            DisplayWidth(overlay_display, DefaultScreen(overlay_display)) - 1));
        pointer_y = fmax(0, fmin(pointer_y,
            DisplayHeight(overlay_display, DefaultScreen(overlay_display)) - 1));
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
        motion_x = pointer_x;
        motion_y = pointer_y;
        pthread_mutex_unlock(&overlay_mutex);
        consumed = dispatch_overlay_event(&event);
        ge_overlay_wayland_set_cursor_position(motion_x, motion_y);
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

void ge_overlay_bridge_surface_created(void)
{
    int first_surface;

    pthread_mutex_lock(&overlay_mutex);
    first_surface = !overlay_surface_count++;
    if (overlay_bridge_suspended)
    {
        overlay_bridge_suspended = 0;
        overlay_trace("re-enabled bridge for a new Wayland toplevel\n");
    }
    pthread_mutex_unlock(&overlay_mutex);

    if (!first_surface) return;

    /* Toplevel creation normally follows gameoverlayrenderer loading. Set up
     * the proxy now instead of dropping the first shortcut while waiting for
     * a passive retry interval to expire. */
    if (init_overlay_bridge(1)) update_overlay_focus();
}

static void destroy_overlay_bridge(void)
{
    Window current_focus;
    int revert_to;

    stop_opengl_presenter();
    destroy_overlay_event();
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
        overlay_set_window_type = NULL;
        overlay_is_enabled = NULL;
        overlay_input_stream_slot = NULL;
        overlay_renderer_base = NULL;
        overlay_input_stream_scanned = 0;
        overlay_screen_width_slot = NULL;
        overlay_screen_height_slot = NULL;
        overlay_screen_size_scanned = 0;
        pthread_mutex_unlock(&overlay_mutex);
        return;
    }

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
    destroy_opengl_presenter_resources();
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
    overlay_set_window_type = NULL;
    overlay_is_enabled = NULL;
    overlay_input_stream_slot = NULL;
    overlay_renderer_base = NULL;
    overlay_input_stream_scanned = 0;
    overlay_screen_width_slot = NULL;
    overlay_screen_height_slot = NULL;
    overlay_screen_size_scanned = 0;
    pointer_x = 0;
    pointer_y = 0;

    pthread_mutex_lock(&cursor_mutex);
    memset(overlay_cursor_map, 0, sizeof(overlay_cursor_map));
    overlay_cursor_shape = GE_STEAM_OVERLAY_CURSOR_DEFAULT;
    overlay_cursor_dirty = 0;
    pthread_mutex_unlock(&cursor_mutex);
    pthread_mutex_unlock(&overlay_mutex);

    ge_overlay_wayland_set_overlay_active(0);
    ge_overlay_wayland_set_cursor_shape(GE_STEAM_OVERLAY_CURSOR_DEFAULT);
    overlay_trace("destroyed X11 overlay bridge window\n");
}

void ge_overlay_bridge_surface_destroyed(void)
{
    int last_surface = 0;

    pthread_mutex_lock(&overlay_mutex);
    if (overlay_surface_count && !--overlay_surface_count)
    {
        overlay_focused_surface_count = 0;
        last_surface = 1;
    }
    pthread_mutex_unlock(&overlay_mutex);

    if (last_surface) destroy_overlay_bridge();
}
