# Wine Wayland Steam overlay bridge

This component keeps Vulkan interception outside `winewayland.drv`. It is an
implicit Vulkan layer implemented with `vkroots` and consumes the native
Wayland/X11 input bridge owned by `lsteamclient/overlay_bridge`.
The layer intercepts `vkCreateWaylandSurfaceKHR`, discovers the Wayland seat
from that surface's client connection, and asks the shared bridge to install
keyboard, pointer, and relative-pointer listeners on a private queue pumped
during presentation. Native OpenGL uses the same bridge from generic `win32u`;
it remains native OpenGL and does not use Zink.
Keeping those listeners separate prevents Wine's event dispatcher from
ignoring proxies it does not own and preserves mouse input while a game has
locked its pointer. While Steam owns input, the layer renders a non-interactive
Wayland subsurface cursor at Steam's synthetic X11 coordinates; this avoids
depending on Wine-Wayland to release the game's pointer constraint.

The build copies `vkroots.h` and its matching Vulkan-Headers revision from
dxvk-nvapi's vendored checkouts, so generated vkroots declarations never get
compiled against an incompatible Vulkan header version.

The lsteamclient helper owns the Wayland keyboard, pointer, cursor, X11 proxy
window, Steam focus metadata, event translation, and teardown. Wine-Wayland
does not load or call either library and contains no overlay-specific bridge
code. Proton makes the private manifest discoverable when Wine-Wayland and Steam's injected
overlay are in use. After win32u loads a real display driver, it bootstraps a
Vulkan instance so non-Vulkan launchers also reach the layer's
`vkCreateInstance` hook and start the in-process `steam_app_*` focus proxy.
The proxy exits when the focused Wayland surface takes ownership. The layer is
not selected for Wine-X11 processes or when Steam's overlay is disabled.

Set `DISABLE_WINE_WAYLAND_STEAM_OVERLAY_LAYER=1` to disable the layer. Once
Steam supports native Wine-Wayland overlay input, removal consists of deleting
this directory and `lsteamclient/overlay_bridge`, removing their build include,
removing Proton's activation block, and dropping the `win32u` overlay patches.
