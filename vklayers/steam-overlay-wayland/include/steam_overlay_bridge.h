#ifndef GE_STEAM_OVERLAY_BRIDGE_H
#define GE_STEAM_OVERLAY_BRIDGE_H

#include <stdint.h>

#define GE_STEAM_OVERLAY_BRIDGE_ABI_VERSION 1u

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
    GE_STEAM_OVERLAY_FRAME_RELATIVE = 1u << 0,
    GE_STEAM_OVERLAY_FRAME_ABSOLUTE = 1u << 1,
    GE_STEAM_OVERLAY_FRAME_DISCRETE_WHEEL = 1u << 2,
    GE_STEAM_OVERLAY_FRAME_DISCRETE_WHEEL_HORZ = 1u << 3,
    GE_STEAM_OVERLAY_FRAME_AXIS = 1u << 4,
    GE_STEAM_OVERLAY_FRAME_AXIS_HORZ = 1u << 5,
};

struct ge_steam_overlay_pointer_frame_v1
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

struct ge_steam_overlay_host_v1
{
    uint32_t abi_version;
    uint32_t struct_size;
    void *userdata;

    void (*set_overlay_active)(void *userdata, int active);
    void (*set_cursor_shape)(void *userdata, uint32_t shape);
    int (*overlay_event_is_active)(void *userdata);
    void (*set_overlay_event_owned)(void *userdata, int owned);
};

struct ge_steam_overlay_api_v1
{
    uint32_t abi_version;
    uint32_t struct_size;

    void (*enable)(void);
    void (*destroy)(void);
    void (*focus)(int focused);
    int (*filter_key)(uint32_t time, uint32_t key, int pressed);
    int (*filter_pointer_button)(uint32_t time, uint32_t button, int pressed);
    int (*filter_pointer_frame)(const struct ge_steam_overlay_pointer_frame_v1 *frame);
    int (*is_active)(void);
    void (*set_cursor_shape)(uint32_t shape);
};

typedef const struct ge_steam_overlay_api_v1 *
(*ge_steam_overlay_bridge_get_v1_fn)(uint32_t abi_version,
                                     const struct ge_steam_overlay_host_v1 *host);

const struct ge_steam_overlay_api_v1 *
ge_steam_overlay_bridge_get_v1(uint32_t abi_version,
                               const struct ge_steam_overlay_host_v1 *host);

#endif
