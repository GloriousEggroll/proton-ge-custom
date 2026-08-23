# lsteamclient native overlay bridge

This native helper owns the Steam overlay compatibility state shared by the
Wine-Wayland Vulkan and OpenGL paths. It manages the private Wayland input
queue, X11 Steam focus target, cursor forwarding, keyboard and pointer event
translation, and process-local teardown.

The Vulkan layer links to this library and supplies each `wl_display` and
`wl_surface` discovered through `vkCreateWaylandSurfaceKHR`. Generic Wine
`win32u` loads the same library for native EGL and supplies its Wayland display.
No graphics API is implemented or translated by this helper.
