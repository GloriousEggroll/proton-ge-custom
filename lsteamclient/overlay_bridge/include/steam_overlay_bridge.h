#ifndef GE_STEAM_OVERLAY_BRIDGE_H
#define GE_STEAM_OVERLAY_BRIDGE_H

#include <stdint.h>

struct wl_display;
struct wl_surface;

#if defined(__GNUC__)
#define GE_OVERLAY_API __attribute__((visibility("default")))
#else
#define GE_OVERLAY_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum ge_steam_overlay_cursor_shape
{
    GE_STEAM_OVERLAY_CURSOR_DEFAULT,
    GE_STEAM_OVERLAY_CURSOR_POINTER,
    GE_STEAM_OVERLAY_CURSOR_TEXT,
    GE_STEAM_OVERLAY_CURSOR_WAIT,
    GE_STEAM_OVERLAY_CURSOR_CROSSHAIR,
    GE_STEAM_OVERLAY_CURSOR_ALL_RESIZE,
    GE_STEAM_OVERLAY_CURSOR_EW_RESIZE,
    GE_STEAM_OVERLAY_CURSOR_NS_RESIZE,
    GE_STEAM_OVERLAY_CURSOR_NWSE_RESIZE,
    GE_STEAM_OVERLAY_CURSOR_NESW_RESIZE,
    GE_STEAM_OVERLAY_CURSOR_NOT_ALLOWED,
    GE_STEAM_OVERLAY_CURSOR_HELP,
};

enum ge_steam_overlay_pointer_frame_flags
{
    GE_STEAM_OVERLAY_FRAME_ABSOLUTE = 1u << 0,
    GE_STEAM_OVERLAY_FRAME_DISCRETE_WHEEL = 1u << 1,
    GE_STEAM_OVERLAY_FRAME_DISCRETE_WHEEL_HORZ = 1u << 2,
    GE_STEAM_OVERLAY_FRAME_AXIS = 1u << 3,
    GE_STEAM_OVERLAY_FRAME_AXIS_HORZ = 1u << 4,
    GE_STEAM_OVERLAY_FRAME_RELATIVE = 1u << 5,
};

struct ge_steam_overlay_pointer_frame
{
    uint32_t time;
    int32_t x;
    int32_t y;
    double dx;
    double dy;
    double axis;
    double horz_axis;
    int32_t scroll;
    int32_t horz_scroll;
    uint32_t flags;
};

struct ge_overlay_wayland_surface;

GE_OVERLAY_API void ge_overlay_focus_proxy_instance_created(void);
GE_OVERLAY_API void ge_overlay_focus_proxy_instance_destroyed(void);

GE_OVERLAY_API struct ge_overlay_wayland_surface *ge_overlay_wayland_surface_create(
    struct wl_display *display, struct wl_surface *surface);
GE_OVERLAY_API void ge_overlay_wayland_surface_destroy(struct ge_overlay_wayland_surface *surface);
GE_OVERLAY_API void ge_overlay_wayland_surface_dispatch(struct ge_overlay_wayland_surface *surface);
void ge_overlay_wayland_set_cursor_shape(uint32_t shape);
void ge_overlay_wayland_set_cursor_position(int32_t x, int32_t y);
void ge_overlay_wayland_set_overlay_active(int active);

GE_OVERLAY_API void ge_overlay_external_wayland_attach(
    struct wl_display *display, int32_t x, int32_t y,
    uint32_t width, uint32_t height);
GE_OVERLAY_API void ge_overlay_external_wayland_dispatch(
    int32_t x, int32_t y, uint32_t width, uint32_t height);

void ge_overlay_bridge_surface_created(void);
void ge_overlay_bridge_surface_destroyed(void);
void ge_overlay_bridge_enable_opengl_presenter(int32_t x, int32_t y,
                                               uint32_t width,
                                               uint32_t height);
void ge_overlay_bridge_present_opengl(int32_t x, int32_t y,
                                      uint32_t width, uint32_t height);
void ge_overlay_bridge_focus(int focused);
int ge_overlay_bridge_filter_key(uint32_t time, uint32_t key, int pressed,
                                 uint32_t utf32);
int ge_overlay_bridge_filter_pointer_button(uint32_t time, uint32_t button,
                                            int pressed);
int ge_overlay_bridge_filter_pointer_frame(
    const struct ge_steam_overlay_pointer_frame *frame);

#ifdef __cplusplus
}
#endif

#endif
