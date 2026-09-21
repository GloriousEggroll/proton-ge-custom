# lsteamclient native overlay bridge

This native helper owns the Steam overlay compatibility state shared by the
Wine-Wayland Vulkan and OpenGL paths. It manages the private Wayland input
queue, X11 Steam focus target, cursor forwarding, keyboard and pointer event
translation, and process-local teardown.

The Vulkan layer links to this library and supplies each `wl_display` and
`wl_surface` discovered through `vkCreateWaylandSurfaceKHR`. Generic Wine
`win32u` loads the same library for native EGL and supplies its Wayland display.
No graphics API is implemented or translated by this helper.

The Steam Input focus identity is an offscreen, root-level X11 `InputOnly`
window, separate from the renderer's Vulkan event target or GLX drawable.
Focusing an `InputOutput` proxy can make Mutter deactivate the native Wayland
game, even with `override_redirect`. Both render paths receive translated
focus events locally; the focus-only window bypasses Steam's rendering hooks.
On keyboard-capable seats, keyboard focus controls this identity, not pointer
hover. See [#754](https://github.com/GloriousEggroll/proton-ge-custom/issues/754).

The Vulkan event target must also be `InputOnly`, with depth zero and no
visual. Steam focuses that target itself when opening the overlay; making only
the separate focus proxy input-only does not prevent this second focus change
from deactivating the Wayland game and routing Guide presses to the desktop.
The OpenGL presenter still requires an `InputOutput` GLX drawable.

On compositors that reject the InputOnly focus request, lsteamclient's
`steam_overlay_focus.h` checks Steam's controller context from its regular
callback path. It restores the game assignment only when the game's overlay
is focused/open and Steam has switched to Desktop. It preserves Big Picture
and other games, and releases its own assignment on real focus loss. The
bridge rechecks current X11 focus and proxy selection ownership, returning
unknown if its mutex is busy. The initial X11 focus reply is insufficient:
the compositor can revoke focus asynchronously after that reply. Steam IPC
runs only after the bridge releases its locks, never from a Wayland listener.
No additional Steam pipe or worker is created.

This uses a private API, currently validated only for the Linux x86_64
`steamclient.so` build ID `7b0847f04cf284f01a1a165561df054902df31a7`. The loaded
ELF build ID and interface entries are checked before use. Unknown libraries
and other architectures retain the existing focus path, not guessed calls.
A Steam update therefore requires inspecting the ABI before extending support.
Games must keep servicing Steam callbacks for this path to update focus.

While the overlay owns the cursor, the bridge uses generic win32u suppression
alongside cursor replay. Wine keeps the latest game cursor cached but supplies
a hidden cursor to its driver until the overlay closes. This also keeps driver
pointer re-entry from drawing the game cursor over the overlay subsurface.
Neither cursor policy nor Steam focus IPC is added to winewayland.

When testing focus changes, check repeated Guide open/close cycles without
moving the mouse, Shift+Tab and text entry, controller navigation, and that
game input stays blocked while the overlay is open. Also check real alt-tab
away/back so the bridge does not retain Steam Input focus on the desktop.
Check pointer leave/re-entry with the overlay open, custom/hidden game cursor
restoration on close, and Vulkan/OpenGL plus XWayland regressions. The current
integrated changes are not yet runtime-validated; see
[the investigation](tests/steam-focus-investigation.md) for the successful
standalone IPC test and remaining checks.
