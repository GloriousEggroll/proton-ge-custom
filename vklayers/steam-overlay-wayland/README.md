# Wine Wayland Steam overlay bridge

This component keeps the temporary Steam overlay compatibility code outside
`winewayland.drv`. The small Wine-Wayland forwarding shim loads the shared
object directly, so Vulkan, OpenGL, and other applications all use the same
implementation without putting the bridge into their graphics dispatch chain.
The shared object retains transparent Vulkan-layer entry points and its private
manifest for development, but Proton does not register that manifest by
default.

The external component owns all X11 state: the proxy window, Steam focus
metadata, event translation and consumption, cursor tracking, and teardown.
Wine-Wayland only forwards Wayland keyboard and pointer events through the
versioned interface in `include/steam_overlay_bridge.h`; it also supplies the
callbacks that require Wine internals.

Set `DISABLE_WINE_WAYLAND_STEAM_OVERLAY_LAYER=1` to disable both activation
paths. Once Steam supports native Wine-Wayland overlay input, removal consists
of deleting this directory and its build include, removing Proton's activation
block, and dropping the matching `em-fixups` forwarding-shim patch.
