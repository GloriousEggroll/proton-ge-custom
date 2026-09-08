#ifndef GE_OVERLAY_X11_FOCUS_H
#define GE_OVERLAY_X11_FOCUS_H

#include <X11/Xlib.h>

Window ge_overlay_create_x11_focus_window(Display *display, const char *window_class);
void ge_overlay_destroy_x11_focus_window(Display *display, Window window);

#endif
