# Wine Wayland Steam overlay bridge

This component keeps the temporary Steam overlay compatibility code outside
`winewayland.drv`. It is an implicit Vulkan layer implemented with `vkroots`.
The layer intercepts `vkCreateWaylandSurfaceKHR`, discovers the Wayland seat
from that surface's client connection, and installs keyboard, pointer, and
relative-pointer listeners on a private queue pumped during presentation.
Keeping those listeners separate prevents Wine's event dispatcher from
ignoring proxies it does not own and preserves mouse input while a game has
locked its pointer. While Steam owns input, the layer renders a non-interactive
Wayland subsurface cursor at Steam's synthetic X11 coordinates; this avoids
depending on Wine-Wayland to release the game's pointer constraint.

The build copies `vkroots.h` and its matching Vulkan-Headers revision from
dxvk-nvapi's vendored checkouts, so generated vkroots declarations never get
compiled against an incompatible Vulkan header version.

The layer owns the Wayland keyboard, pointer, cursor, X11 proxy window, Steam
focus metadata, event translation, and teardown. Wine-Wayland does not load or
call this library and contains no overlay-specific bridge code. For the period
before a launcher creates a Vulkan surface, Proton starts the standalone
`ge-wayland-steam-focus-proxy`. It advertises the focused `steam_app_*` target
needed by Steam Input and exits when the Vulkan layer takes ownership. The
layer itself is never preloaded into Wine processes.

Set `DISABLE_WINE_WAYLAND_STEAM_OVERLAY_LAYER=1` to disable the layer. Once
Steam supports native Wine-Wayland overlay input, removal consists of deleting
this directory and its build include, and removing Proton's activation block.
