/*
 * Early X11 focus proxy owned by the lsteamclient overlay bridge.
 *
 * win32u creates a host Vulkan instance before a Wine-Wayland process has a
 * presentation surface.  Use that instance creation to advertise the
 * steam_app_* target Steam Input expects.  Once the focused Wayland surface's
 * full bridge takes selection ownership this bootstrap proxy exits.
 */

#include "steam_overlay_bridge.h"
#include "x11_focus.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xutil.h>

enum proxy_thread_state
{
    PROXY_THREAD_NONE,
    PROXY_THREAD_RUNNING,
    PROXY_THREAD_STOPPING,
};

static pthread_once_t proxy_config_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t proxy_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t proxy_cond = PTHREAD_COND_INITIALIZER;
static pthread_t proxy_thread;
static unsigned int proxy_instance_count;
static enum proxy_thread_state proxy_state;
static int proxy_enabled;
static int proxy_stop;

struct direct_xlib
{
    Display *(*open_display)(const char *);
    Window (*create_window)(Display *, Window, int, int, unsigned int,
                            unsigned int, unsigned int, int, unsigned int,
                            Visual *, unsigned long, XSetWindowAttributes *);
    int (*map_window)(Display *, Window);
    int (*destroy_window)(Display *, Window);
    int (*close_display)(Display *);
};

static int load_direct_xlib(struct direct_xlib *xlib)
{
    memset(xlib, 0, sizeof(*xlib));
    /* gameoverlayrenderer interposes dlsym as well as Xlib. Looking symbols
     * up through a libX11 dlopen handle therefore still returns Steam's
     * wrappers. Its RTLD_NEXT path deliberately returns the underlying Xlib
     * entry points, which keeps this temporary bootstrap window from being
     * mistaken for the game's render target. */
    xlib->open_display = (Display *(*)(const char *))dlsym(
        RTLD_NEXT, "XOpenDisplay");
    xlib->create_window = (Window (*)(Display *, Window, int, int,
                                      unsigned int, unsigned int, unsigned int,
                                      int, unsigned int, Visual *, unsigned long,
                                      XSetWindowAttributes *))dlsym(
        RTLD_NEXT, "XCreateWindow");
    xlib->map_window = (int (*)(Display *, Window))dlsym(
        RTLD_NEXT, "XMapWindow");
    xlib->destroy_window = (int (*)(Display *, Window))dlsym(
        RTLD_NEXT, "XDestroyWindow");
    xlib->close_display = (int (*)(Display *))dlsym(
        RTLD_NEXT, "XCloseDisplay");
    return xlib->open_display && xlib->create_window && xlib->map_window &&
           xlib->destroy_window && xlib->close_display;
}

Window ge_overlay_create_x11_focus_window(Display *display, const char *window_class)
{
    struct direct_xlib xlib;
    XSetWindowAttributes attributes = {0};
    XClassHint class_hint;
    unsigned long pid = (unsigned long)getpid();
    Atom net_wm_pid;
    Window window;

    if (!load_direct_xlib(&xlib)) return None;

    /* Steam Input needs an X11 focus identity, not a drawable. Mutter tracks
     * InputOutput windows even with override_redirect and focusing one takes
     * keyboard focus away from the native Wayland game. InputOnly windows
     * are excluded from that window-management path. Keep this root child
     * separate from the overlay drawable, and out of Steam's render hooks.
     * https://github.com/GloriousEggroll/proton-ge-custom/issues/754 */
    attributes.override_redirect = True;
    window = xlib.create_window(display, DefaultRootWindow(display), -1, -1,
                                1, 1, 0, 0, InputOnly, CopyFromParent,
                                CWOverrideRedirect, &attributes);
    if (!window) return None;

    class_hint.res_name = (char *)window_class;
    class_hint.res_class = (char *)window_class;
    XSetClassHint(display, window, &class_hint);
    XStoreName(display, window, window_class);
    net_wm_pid = XInternAtom(display, "_NET_WM_PID", False);
    XChangeProperty(display, window, net_wm_pid, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)&pid, 1);
    xlib.map_window(display, window);
    return window;
}

void ge_overlay_destroy_x11_focus_window(Display *display, Window window)
{
    struct direct_xlib xlib;

    if (window && load_direct_xlib(&xlib)) xlib.destroy_window(display, window);
}

static int env_enabled(const char *name, int default_value)
{
    const char *value = getenv(name);
    return value ? atoi(value) != 0 : default_value;
}

static int debug_enabled(void)
{
    const char *value = getenv("GE_WAYLAND_STEAM_OVERLAY_DEBUG");
    const char *winedebug;

    if (value) return atoi(value) != 0;
    winedebug = getenv("WINEDEBUG");
    return winedebug && strstr(winedebug, "+waylanddrv");
}

/* Steam identifies the focused game through the process that owns the focused
 * window, and only knows processes that load its overlay. Wine removes the
 * overlay from explorer.exe, and Proton's stubs aren't the game. */
static int process_known_to_steam(void)
{
    static const char *const stubs[] = {"steam.exe", "umu.exe"};
    char cmdline[4096], *arg, *name;
    unsigned int i, index;
    Dl_info info;
    ssize_t size;
    int fd;

    if (!dladdr((void *)XCheckIfEvent, &info) || !info.dli_fname ||
        !strstr(info.dli_fname, "gameoverlayrenderer"))
        return 0;

    if ((fd = open("/proc/self/cmdline", O_RDONLY | O_CLOEXEC)) < 0) return 1;
    size = read(fd, cmdline, sizeof(cmdline) - 1);
    close(fd);
    if (size <= 0) return 1;
    cmdline[size] = 0;

    /* The executable follows the loader and preloader. */
    for (arg = cmdline, index = 0; arg < cmdline + size && index < 3; arg += strlen(arg) + 1, index++)
    {
        name = arg;
        if (strrchr(name, '\\')) name = strrchr(name, '\\') + 1;
        if (strrchr(name, '/')) name = strrchr(name, '/') + 1;
        for (i = 0; i < sizeof(stubs) / sizeof(*stubs); i++)
            if (!strcasecmp(name, stubs[i])) return 0;
    }
    return 1;
}

static void init_proxy_config(void)
{
    const char *appid = getenv("SteamAppId");
    const char *display = getenv("DISPLAY");

    proxy_enabled = env_enabled("WINE_WAYLAND_STEAM_OVERLAY_LAYER", 0) &&
                    !env_enabled("DISABLE_WINE_WAYLAND_STEAM_OVERLAY_LAYER", 0) &&
                    !env_enabled("PROTON_NO_STEAMINPUT", 0) &&
                    appid && appid[0] && display && display[0] &&
                    process_known_to_steam();
}

static int proxy_should_stop(void)
{
    int stop;

    pthread_mutex_lock(&proxy_mutex);
    stop = proxy_stop;
    pthread_mutex_unlock(&proxy_mutex);
    return stop;
}

/* Any window can close between the focus query and looking at it, and the
 * default Xlib error handler would then exit the process. XSetErrorHandler is
 * process-wide, so handle errors on this private connection only. */
static int ignore_x11_error(Display *display, xError *error, XExtCodes *codes,
                            int *ret_code)
{
    (void)display;
    (void)error;
    (void)codes;
    *ret_code = 0;
    return 1;
}

/* Returns whether X11 focus is on a normal application window, such as the
 * Steam client. Compositors hold X11 focus on an override-redirect helper
 * window, or on nothing, while a native Wayland window has keyboard focus. */
static int focus_is_on_x11_app(Display *display, Window focus)
{
    Window root = DefaultRootWindow(display), query_root, parent, *children;
    XWindowAttributes attributes;
    unsigned int count;

    if (focus == None || focus == PointerRoot || focus == root) return 0;

    for (;;)
    {
        if (!XQueryTree(display, focus, &query_root, &parent, &children, &count))
            return 1;
        if (children) XFree(children);
        if (parent == root || parent == None) break;
        focus = parent;
    }

    if (!XGetWindowAttributes(display, focus, &attributes)) return 1;
    return !attributes.override_redirect;
}

/* Hold the per-game target until another process takes it or the proxy stops. */
static void hold_focus_target(Display *display, Window window, Atom owner_atom)
{
    struct timespec sleep_time = {0, 50 * 1000 * 1000};
    Window current_focus;
    int revert_to;

    while (!proxy_should_stop())
    {
        if (XGetSelectionOwner(display, owner_atom) != window) break;

        XGetInputFocus(display, &current_focus, &revert_to);
        /* Hold the target against the compositor resetting focus, but leave
         * it alone when the user has moved focus to another X11 application.
         * https://github.com/GloriousEggroll/proton-ge-custom/issues/950 */
        if (current_focus != window && !focus_is_on_x11_app(display, current_focus))
        {
            XSetInputFocus(display, window, RevertToParent, CurrentTime);
            XFlush(display);
        }
        nanosleep(&sleep_time, NULL);
    }
}

static void *run_focus_proxy(void *arg)
{
    struct direct_xlib xlib;
    struct timespec sleep_time = {0, 50 * 1000 * 1000};
    const char *display_name = getenv("DISPLAY");
    const char *appid = getenv("SteamAppId");
    char owner_selection[160];
    char window_class[128];
    Display *display;
    XExtCodes *codes;
    Window current_focus;
    Window window;
    Atom owner_atom;
    int revert_to;
    int owns_selection;
    unsigned int waited;

    (void)arg;
    XInitThreads();
    /* This bootstrap window exists only to associate Steam Input with a
     * native Wayland process before its render surface exists. Keep its X11
     * lifecycle out of gameoverlayrenderer: otherwise Steam selects this 1x1
     * InputOnly window as the first game target, then loses its passive HUD
     * state when the proxy is replaced by the real GLX presenter. */
    if (!appid || !display_name || !load_direct_xlib(&xlib))
        return NULL;
    if (!(display = xlib.open_display(display_name)))
        return NULL;
    if (!(codes = XAddExtension(display)))
    {
        xlib.close_display(display);
        return NULL;
    }
    XESetError(display, codes->extension, ignore_x11_error);

    snprintf(window_class, sizeof(window_class), "steam_app_%s", appid);
    snprintf(owner_selection, sizeof(owner_selection),
             "_WINE_WAYLAND_STEAM_FOCUS_%s", appid);
    owner_atom = XInternAtom(display, owner_selection, False);

    /* Every Wine process creates an instance, but one process owns the target
     * at a time. A focused Wayland surface can take it at any time. The others
     * wait, as the owner can be a launcher stub that exits right away. */
    while (!proxy_should_stop())
    {
        if (XGetSelectionOwner(display, owner_atom) != None)
        {
            /* There can be many waiting processes, so check once a second. */
            for (waited = 0; waited < 20 && !proxy_should_stop(); waited++)
                nanosleep(&sleep_time, NULL);
            continue;
        }

        if (!(window = ge_overlay_create_x11_focus_window(display, window_class)))
            break;

        owns_selection = 0;
        XGrabServer(display);
        if (XGetSelectionOwner(display, owner_atom) == None)
        {
            XSetSelectionOwner(display, owner_atom, window, CurrentTime);
            XGetInputFocus(display, &current_focus, &revert_to);
            if (!focus_is_on_x11_app(display, current_focus))
                XSetInputFocus(display, window, RevertToParent, CurrentTime);
            XSync(display, False);
            owns_selection = XGetSelectionOwner(display, owner_atom) == window;
        }
        XUngrabServer(display);
        XSync(display, False);

        if (owns_selection)
        {
            if (debug_enabled())
                fprintf(stderr, "steam-overlay-wayland: in-process focus proxy owns %#lx\n",
                        window);
            hold_focus_target(display, window, owner_atom);
            if (XGetSelectionOwner(display, owner_atom) == window)
                XSetSelectionOwner(display, owner_atom, None, CurrentTime);
            XGetInputFocus(display, &current_focus, &revert_to);
            if (current_focus == window)
                XSetInputFocus(display, PointerRoot, RevertToPointerRoot, CurrentTime);
            if (debug_enabled())
                fputs("steam-overlay-wayland: in-process focus proxy released ownership\n",
                      stderr);
        }
        ge_overlay_destroy_x11_focus_window(display, window);
        XSync(display, False);
    }

    xlib.close_display(display);
    return NULL;
}

void ge_overlay_focus_proxy_instance_created(void)
{
    pthread_once(&proxy_config_once, init_proxy_config);
    if (!proxy_enabled) return;

    pthread_mutex_lock(&proxy_mutex);
    while (proxy_state == PROXY_THREAD_STOPPING)
        pthread_cond_wait(&proxy_cond, &proxy_mutex);

    ++proxy_instance_count;
    if (proxy_state == PROXY_THREAD_NONE)
    {
        proxy_stop = 0;
        if (!pthread_create(&proxy_thread, NULL, run_focus_proxy, NULL))
        {
            proxy_state = PROXY_THREAD_RUNNING;
            if (debug_enabled())
                fputs("steam-overlay-wayland: vkCreateInstance started focus proxy\n",
                      stderr);
        }
        else if (debug_enabled())
        {
            fputs("steam-overlay-wayland: failed to start focus proxy\n", stderr);
        }
    }
    pthread_mutex_unlock(&proxy_mutex);
}

void ge_overlay_focus_proxy_instance_destroyed(void)
{
    pthread_t thread;
    int join_thread = 0;

    pthread_once(&proxy_config_once, init_proxy_config);
    if (!proxy_enabled) return;

    pthread_mutex_lock(&proxy_mutex);
    if (proxy_instance_count && !--proxy_instance_count &&
        proxy_state == PROXY_THREAD_RUNNING)
    {
        proxy_stop = 1;
        proxy_state = PROXY_THREAD_STOPPING;
        thread = proxy_thread;
        join_thread = 1;
    }
    pthread_mutex_unlock(&proxy_mutex);

    if (!join_thread) return;
    pthread_join(thread, NULL);

    pthread_mutex_lock(&proxy_mutex);
    proxy_state = PROXY_THREAD_NONE;
    proxy_stop = 0;
    pthread_cond_broadcast(&proxy_cond);
    pthread_mutex_unlock(&proxy_mutex);
}
