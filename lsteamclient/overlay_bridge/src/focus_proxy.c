/*
 * Early X11 focus proxy owned by the lsteamclient overlay bridge.
 *
 * win32u creates a host Vulkan instance before a Wine-Wayland process has a
 * presentation surface.  Use that instance creation to advertise the
 * steam_app_* target Steam Input expects.  Once the focused Wayland surface's
 * full bridge takes selection ownership this bootstrap proxy exits.
 */

#include "steam_overlay_bridge.h"

#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
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

static void init_proxy_config(void)
{
    const char *appid = getenv("SteamAppId");
    const char *display = getenv("DISPLAY");

    proxy_enabled = env_enabled("WINE_WAYLAND_STEAM_OVERLAY_LAYER", 0) &&
                    !env_enabled("DISABLE_WINE_WAYLAND_STEAM_OVERLAY_LAYER", 0) &&
                    !env_enabled("PROTON_NO_STEAMINPUT", 0) &&
                    appid && appid[0] && display && display[0];
}

static int proxy_should_stop(void)
{
    int stop;

    pthread_mutex_lock(&proxy_mutex);
    stop = proxy_stop;
    pthread_mutex_unlock(&proxy_mutex);
    return stop;
}

static void *run_focus_proxy(void *arg)
{
    struct direct_xlib xlib;
    XSetWindowAttributes attributes = {0};
    struct timespec sleep_time = {0, 50 * 1000 * 1000};
    XClassHint class_hint;
    const char *display_name = getenv("DISPLAY");
    const char *appid = getenv("SteamAppId");
    char owner_selection[160];
    char window_class[128];
    Display *display;
    Window current_focus;
    Window root;
    Window window;
    Atom owner_atom;
    Atom net_wm_pid;
    unsigned long pid = (unsigned long)getpid();
    int revert_to;
    int owns_selection = 0;

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

    root = DefaultRootWindow(display);
    attributes.override_redirect = True;
    window = xlib.create_window(display, root, -1, -1, 1, 1, 0, 0,
                                InputOnly, CopyFromParent, CWOverrideRedirect,
                                &attributes);
    if (!window)
    {
        xlib.close_display(display);
        return NULL;
    }

    snprintf(window_class, sizeof(window_class), "steam_app_%s", appid);
    snprintf(owner_selection, sizeof(owner_selection),
             "_WINE_WAYLAND_STEAM_FOCUS_%s", appid);
    owner_atom = XInternAtom(display, owner_selection, False);
    class_hint.res_name = window_class;
    class_hint.res_class = window_class;
    XSetClassHint(display, window, &class_hint);
    XStoreName(display, window, window_class);

    net_wm_pid = XInternAtom(display, "_NET_WM_PID", False);
    XChangeProperty(display, window, net_wm_pid, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)&pid, 1);
    xlib.map_window(display, window);

    /* Every Wine process creates an instance. Only bootstrap focus when no
     * process in this app already owns the per-game target. A focused Wayland
     * surface is still allowed to transfer this selection later. */
    XGrabServer(display);
    if (XGetSelectionOwner(display, owner_atom) == None)
    {
        XSetSelectionOwner(display, owner_atom, window, CurrentTime);
        XSetInputFocus(display, window, RevertToParent, CurrentTime);
        XSync(display, False);
        owns_selection = XGetSelectionOwner(display, owner_atom) == window;
    }
    XUngrabServer(display);
    XSync(display, False);

    if (owns_selection && debug_enabled())
        fprintf(stderr, "steam-overlay-wayland: in-process focus proxy owns %#lx\n",
                window);

    while (owns_selection && !proxy_should_stop())
    {
        if (XGetSelectionOwner(display, owner_atom) != window) break;

        XGetInputFocus(display, &current_focus, &revert_to);
        if (current_focus != window)
        {
            XSetInputFocus(display, window, RevertToParent, CurrentTime);
            XFlush(display);
        }
        nanosleep(&sleep_time, NULL);
    }

    if (XGetSelectionOwner(display, owner_atom) == window)
        XSetSelectionOwner(display, owner_atom, None, CurrentTime);
    XGetInputFocus(display, &current_focus, &revert_to);
    if (current_focus == window)
        XSetInputFocus(display, PointerRoot, RevertToPointerRoot, CurrentTime);
    xlib.destroy_window(display, window);
    XSync(display, False);
    xlib.close_display(display);

    if (owns_selection && debug_enabled())
        fputs("steam-overlay-wayland: in-process focus proxy released ownership\n",
              stderr);
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
