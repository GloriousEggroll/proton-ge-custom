/*
 * Temporary X11 focus proxy for native Wayland launchers.
 *
 * Steam Input still identifies its target through an X11 steam_app_* window.
 * A launcher that never creates a Vulkan surface cannot load the overlay
 * layer, so this helper owns the proxy only until that layer takes over.
 */

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

static volatile sig_atomic_t running = 1;

static void stop_proxy(int signal_number)
{
    (void)signal_number;
    running = 0;
}

static int debug_enabled(void)
{
    const char *value = getenv("GE_WAYLAND_STEAM_OVERLAY_DEBUG");
    const char *winedebug;

    if (value) return atoi(value) != 0;
    winedebug = getenv("WINEDEBUG");
    return winedebug && strstr(winedebug, "+waylanddrv");
}

int main(int argc, char **argv)
{
    XSetWindowAttributes attributes = {0};
    struct sigaction action = {0};
    struct timespec sleep_time = {0, 50 * 1000 * 1000};
    XClassHint class_hint;
    const char *display_name;
    const char *appid;
    char owner_selection[160];
    char window_class[128];
    Display *display;
    Window current_focus;
    Window root;
    Window window;
    Atom owner_atom;
    Atom net_wm_pid;
    unsigned long pid;
    int focus_owned = 1;
    int revert_to;

    appid = argc > 1 ? argv[1] : getenv("SteamAppId");
    display_name = getenv("DISPLAY");
    if (!appid || !*appid || !display_name || !*display_name) return 0;

    if (!(display = XOpenDisplay(display_name))) return 0;
    root = DefaultRootWindow(display);
    attributes.override_redirect = True;
    window = XCreateWindow(display, root, -1, -1, 1, 1, 0, 0,
                           InputOnly, CopyFromParent, CWOverrideRedirect,
                           &attributes);
    if (!window)
    {
        XCloseDisplay(display);
        return 0;
    }

    snprintf(window_class, sizeof(window_class), "steam_app_%s", appid);
    snprintf(owner_selection, sizeof(owner_selection),
             "_WINE_WAYLAND_STEAM_FOCUS_%s", appid);
    owner_atom = XInternAtom(display, owner_selection, False);
    class_hint.res_name = window_class;
    class_hint.res_class = window_class;
    XSetClassHint(display, window, &class_hint);
    XStoreName(display, window, window_class);

    pid = (unsigned long)getppid();
    if (argc > 2)
    {
        char *end;
        unsigned long requested_pid = strtoul(argv[2], &end, 10);

        if (end != argv[2] && !*end && requested_pid)
            pid = requested_pid;
    }
    net_wm_pid = XInternAtom(display, "_NET_WM_PID", False);
    XChangeProperty(display, window, net_wm_pid, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)&pid, 1);

    XMapWindow(display, window);
    XSetSelectionOwner(display, owner_atom, window, CurrentTime);
    XSetInputFocus(display, window, RevertToParent, CurrentTime);
    XSync(display, False);

    action.sa_handler = stop_proxy;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    prctl(PR_SET_PDEATHSIG, SIGTERM);

    if (debug_enabled())
        fprintf(stderr, "steam-overlay-wayland: launcher focus proxy owns %#lx\n",
                window);

    while (running && getppid() != 1)
    {
        if (XGetSelectionOwner(display, owner_atom) != window) break;

        /* Xwayland may reset its input focus after the native Wayland
         * launcher maps. The launcher has no Vulkan surface from which the
         * layer can observe a later keyboard-enter event, so keep this proxy
         * focused for as long as it owns the per-game selection. */
        XGetInputFocus(display, &current_focus, &revert_to);
        if (current_focus != window)
        {
            XSetInputFocus(display, window, RevertToParent, CurrentTime);
            XFlush(display);
            if (focus_owned && debug_enabled())
                fputs("steam-overlay-wayland: launcher focus proxy restored X11 focus\n",
                      stderr);
            focus_owned = 0;
        }
        else
        {
            focus_owned = 1;
        }
        nanosleep(&sleep_time, NULL);
    }

    if (XGetSelectionOwner(display, owner_atom) == window)
        XSetSelectionOwner(display, owner_atom, None, CurrentTime);
    XGetInputFocus(display, &current_focus, &revert_to);
    if (current_focus == window)
        XSetInputFocus(display, PointerRoot, RevertToPointerRoot, CurrentTime);
    XDestroyWindow(display, window);
    XSync(display, False);
    XCloseDisplay(display);

    if (debug_enabled())
        fputs("steam-overlay-wayland: launcher focus proxy released ownership\n",
              stderr);
    return 0;
}
