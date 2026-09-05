# Wine-Wineland child-process rendering

This directory contains a selective rebase of Wine-Wineland's cross-process
DMA-BUF rendering work onto the Wine base used by GE-Proton.

Upstream branch:
https://github.com/nanomatters/wine-salkim/tree/wineland_20260713-reorg

Original author: Erhan Bilgili <erhan.bilgili@gmail.com>

The original author and dates are preserved in every patch's mail header, and
every imported commit carries a `Source:` trailer with its upstream link. The
patches were rebased only where the GE-Proton Wine base had diverged. The first
subject was narrowed to describe the imported rendering portion of an upstream
commit that also bundled SNI work.

GE-Proton's existing SNI implementation is applied before this series. The
older bundled SNI implementation and Wine-Wineland's native Steam-overlay work
are not imported. References to child "overlays" inside this series mean GDI
child-window composition, not the Steam overlay. Direct-toplevel presentation
and other unrelated Wine-Wineland work are also intentionally excluded.

The 84 patches selectively cover the source commits below. Most
pending-producer changes from `64f5e7717f44` are folded into the following
expose-order patch after rebasing; patch 0067 restores its managed Vulkan
producer lifecycle, which must bracket image allocation and channel teardown.
Patch 0068 adds consumer-state tracking and nonblocking managed dma-buf
publishing from `129fa64fc449` without its unrelated minimized-window changes.
Patch 0075 is a GE-Proton follow-up that retires an OpenGL frame callback when
its client surface is detached or reattached, preventing pre-attachment CEF
presents from permanently throttling the attached surface.
Patch 0076 adapts the OpenGL presentation-suspension behavior from
`aa3013f4bfcb` by deferring ordinary EGL swaps until their client surface is
attached to a live toplevel; roleless HWND dma-buf producers remain active.
Patch 0077 enables Wine's manual frame-callback throttle only when disabling
EGL's native swap throttling succeeds, and reports actual EGL swap failures.
Patch 0078 recreates an EGL window surface after its client wl_surface changes
between roleless and subsurface presentation, recovering drivers that return
`EGL_BAD_SURFACE` after that transition.
Patch 0079 mirrors Wine's X11 EGL fallback-context handling for applications
that release their WGL context before calling `SwapBuffers`, preventing Mesa
from rejecting Chromium child-window presents with `EGL_BAD_CONTEXT` and
`EGL_BAD_SURFACE`. It is based on Wine commit `de44bd7234b2c` by Paul Gofman.
Patch 0084 adapts `093181906443` by attaching an active replacement WSI surface
before its first present while retaining the selected surface's last frame.
The imported tree now has the attachment-generation support that was missing
when this commit was first evaluated, so the focused replacement handoff is
portable without the later presentation-coordination rewrite.
`a42482e82bcb` is omitted because its deadlock fix is already present in the
rebased implementation. The portions of `a022197bcbd4` that overlap
GE-Proton's separately applied NVIDIA Reflex patch are restored by patch 0065,
and patch 0066 imports the required present-wait dispatch follow-up by Etaash
Mathamsetty.

## Re-sync onto `wineland_20260713-reorg` (2026-09-01)

22 upstream commits newer than `129fa64fc449` were imported as real
cherry-picks, and the series was renumbered into contiguous `0001`-`0079`
order so that the applied order keeps matching upstream DAG order. Imported
commits, in patch order:

```
0032 c2cca42e16ed  win32u: Avoid extra server call in GetWindowPlacement.
0033 fb569856df6c  win32u: Avoid full hit-test list for mouse tracking.
0034 362e518332ca  server: Avoid unchanged key state serial bumps.
0035 1d434f8af887  server: Avoid heap allocation for small hit-test lists.
0036 d49d421995d6  winewayland: Avoid win_data lookup in set_clip.
0037 7d3c16911a81  winewayland: Keep minimized toplevels mapped.
0038 425b914f8b9b  winewayland: Defer minimized restore until foreground handling.
0045 f70d57bb55d3  winewayland: Keep toplevels mapped during transient visibility changes.
0055 b1bf835bd15d  winewayland: Invalidate cached toplevel size limits.
0056 c4c40fc0febd  winewayland: Finalize processed configures without GDI contents.
0057 f29ef3325862  winewayland: Avoid scaling configure sizes twice.
0058 f84f09ec1ae1  win32u, winewayland: Handle collapsed-caption windows.
0059 37b786b5186d  win32u: Handle maximized collapsed-caption windows.
0060 31a9b2d84a37  win32u, winewayland: Fix maximized frameless window geometry.
0061 42eeaa2e477d  winewayland: Avoid double-counting server decorations.
0062 d2ba92e09faa  winewayland: Restore maximized windows on zero-size configures.
0069 cd4699994c7a  winewayland: Unmap hidden toplevels.
0070 1a4335fd0e14  win32u, winewayland: Preserve fullscreen swapchain image enumeration.
0071 0a255528b206  winewayland.drv: Avoid frame changes for fullscreen configures.
0072 848dd9933fac  win32u: Refresh display topology after monitor changes.
0073 6451d1cf979c  win32u: Remove the optional window move hack.
0074 b68a41a9025a  winewayland.drv: Keep client-rendered popups mapped.
```

Patch 0045 replaces the older local adaptation (previously 0038) with the
upstream commit `f70d57bb55d3`. Patch 0069 (`cd4699994c7a`, "winewayland: Unmap
hidden toplevels") prevents an explicitly hidden launcher window from
remaining mapped and taking focus away from the game window. It does not fix
Purple's separate CEF navigation repaint stall.

Adaptations made while importing:

- Patch 0056 (`c4c40fc0febd`) keeps this base's local `wl_surface_commit()`
  idiom; the `wayland_surface_commit_pending_state()` helper does not exist
  here.
- Patch 0058 (`f84f09ec1ae1`) reads the win32u `__wine_win32u_frameless` window
  property instead of a cached `data->frameless` field, which this base does
  not have.
- Patch 0069 (`cd4699994c7a`) keeps the `swp_flags` parameter of
  `wayland_win_data_create_wayland_surface()`, because GE-Proton's tray-menu
  and fullscreen-hide handling still needs it.
- Patch 0070 (`1a4335fd0e14`) imports only the `vkGetSwapchainImagesKHR` half;
  the fullscreen-request half needs `struct wayland_fullscreen_request`
  tracking, which this base does not have.
- Patch 0068 was rebased: one hunk in `wayland_surface_update_hwnd_dmabufs()`
  moved behind the imported minimized-toplevel early return.
- `../em-fixups/0009-winewayland-do-not-double-scale-toplevel-configures.patch`
  was dropped: it duplicates upstream `f29ef3325862`, now patch 0057.
- `../em-fixups/0019-...preserve-fullscreen-clients-across-transient-hide.patch`
  was rebased onto patch 0069, which deletes the keep-mapped-on-hide helper it
  used to narrow. The same intent is now expressed as a fullscreen-renderer
  exception on the hide path, so minimized windows and non-rendering windows
  such as launchers unmap again.
- `../em-fixups/` patches 0013, 0014, 0016 and 0022 were re-emitted against the
  new tree so they keep applying without fuzz.

## Bug fixes ported from the skipped set (2026-09-01)

Six of the probed commits became portable once their hunks were re-derived
against the tree GE-Proton actually has at each apply step. They are real
cherry-picks with the upstream author, date and `Source:` trailer preserved:

```
0080 da79178bb865  win32u: Ignore empty Vulkan presentation geometry.
0081 b8496ab03011  winewayland: Derive toplevel resizeability from window state.
0082 1261111b60ec  win32u: Fall back to linear dma-buf images.
0083 19d4bf0c51eb  win32u: Request an extra Vulkan swapchain image.
0084 093181906443  winewayland.drv: Attach replacement surfaces before first present.
../em-fixups/0023 b9638444e73d  winewayland.drv: Clear carrier state when replacing contents.
```

## GE-Proton local fixups (2026-09-02)

Patch 0085 is not part of the upstream import; it is a GE-Proton follow-up
for the fast path introduced by patch 0021. Acking a fullscreen configure
from the requested state bypasses `wayland_configure_window()`, so an
application that strips its frame styles before entering the state can keep
stale frame insets in the Win32 client rect. This matches the failed display
mode transitions reported for Black Desert Online in GE issue 721. 0085 asks
the window thread to refresh the client rect with a geometry-neutral
frame-changed `SetWindowPos`. It only acts on a live, non-minimized,
borderless window that remains in the true Wayland fullscreen state and whose
live Win32 client dimensions do not match its live window dimensions.

`../em-fixups/0024` handles a separate shaped-window repaint failure found in
the NCSOFT Purple launcher. A near-full GDI navigation update can leave its
attached accelerated client idle until a native move exposes the child
hierarchy. The fix recognizes that narrow surface state and requests one
Win32 hierarchy repaint, with a latch reset by smaller damage rather than a
timer or executable-specific workaround.

Adaptations made while importing:

- Patch 0080 (`da79178bb865`) imports the empty-geometry guards only. The
  upstream `surface_get_fshack_config()` hunk has no counterpart here: this base
  uses `surface_get_fshack_dpi()` and has no fshack swapchain config cache.
- Patch 0081 (`b8496ab03011`) computes the classification inside
  `wayland_win_data_get_config()` from the style that function already reads,
  because this base caches neither `data->style` nor a present rect. The now
  unused `data->resizeable` field is removed together with it.
- Patch 0082 (`1261111b60ec`) keeps `caps_flags` before the modifier arrays in
  `vk_select_managed_modifiers()` and has no managed-swapchain color space or
  consumer wake eventfd, so the layout plumbing was inserted around those
  differences. `managed_create_image()` is otherwise identical to upstream.
  It also adds `#include <limits.h>`, which upstream has in this file but this
  base does not: the new `UINT_MAX` stride guard does not compile without it
  under `-Werror`.
- Patch 0083 (`19d4bf0c51eb`) adds only the image-count adjustment; the insertion
  point here has no `compositor_scaling` lookup.
- `b9638444e73d` became `../em-fixups/0023-winewaylanddrv-Clear-carrier-state-when-replacing-conte.patch`
  rather than a patch in this directory. Its subject is carrier bookkeeping, and
  `../em-fixups/0013` renames `transparent_carrier_*` to `carrier_*` and adds
  `carrier_opaque`; before that fixup the stale-opacity bug cannot occur, and in
  the middle of this series the upstream hunks have no context to apply to. The
  direct-toplevel promotion hunks were dropped as elsewhere.
- `../em-fixups/` patches 0011 and 0013 were re-emitted with context-only
  changes, because patch 0081 rewrites `wayland_win_data_get_config()` and both
  of them patch that function.

Apply order is what decides placement: `protonprep-valve-staging.sh` applies
`wine-wayland/`, then `em-fixups/0001`, then this directory, then the rest of
`em-fixups/`. A fix whose hunks depend on fixup content has to land after the
fixup loop instead of in the middle of this series.

## Skipped upstream commits

The 43 commits below were probed and deliberately not imported; the six
fixes above came out of this list once they were ported. Almost all of the
remainder are built on subsystems this Wine base predates: cached `win_data`
window state
(`data->style`, `exstyle`, `visible`, `toplevel`, `owner`, `restore_rect`,
`configure_state_serial`), direct-toplevel presentation (`direct_client`,
`evict_direct_client`, `presentation_scaling`), presentation generation and
present-wait tracking, fullscreen-request tracking, the `output_info_array`
rework, and client-presentation timing feedback. Getting those fixes means
re-syncing the whole series onto a newer upstream snapshot, not cherry-picking
onto this one.

```
c60d65ba48fb  needs excluded direct-toplevel/external-commit-owner machinery
3459d62e0e0c  large lock-order refactor; equivalent intent already local in em-fixups
               0010/0012/0020
75930c29daf5  invasive state-request refactor (new struct/fields + reconcile helper);
               prerequisite cluster 81/102/103/104/106 probed independently
8878f044c1b0  needs client_surface begin/end_present_wait + presentation_generation
               tracking (absent from GE base)
88b19ffdd44b  depends on swapchain_wait_for_present from 8878f044 (absent)
a9ce7e02fe50  needs absent subsystems: direct_client, direct_host_surface,
               evict_direct_client, presentation_scaling,
               wayland_toplevel_has_other_client_surface
48bf29c49f2b  needs absent subsystems: commit_pending_state, direct_client,
               direct_host_surface, evict_direct_client, external_commit_owner,
               presentation_scaling, wayland_toplevel_has_other_client_surface
c3f8032ff16a  needs configure_state_serial/restore_rect state cluster absent from base
a56ff94277e4  needs absent subsystems: configure_state_serial, data->style,
               restore_rect_valid, update_restore_rect
b0f2a6220f79  needs absent subsystems: direct_client
f6fd2fe72854  154-line rewrite of managed acquire that presumes upstream consumer-
               stall watchdog; GE managed path differs (producer_device_lock)
bd8e4179c4ad  precondition absent: this base never gated server decorations on
               is_fullscreen, so the style-aware classification has nothing to fix here
3e93081dabee  needs absent subsystems: data->exstyle, data->style, restore_rect_valid,
               update_restore_rect
7a0347963226  needs absent subsystems: data->toplevel, external_commit_owner,
               presentation_scaling
3e24c12dea26  built on cached win_data state + wayland_surface_clear_role()/visual-
               constraint refactor absent from this base
2b06d67aa99d  needs absent subsystems: client_rect_in_toplevel, commit_pending_state,
               data->exstyle, data->owner, data->style, data->toplevel, data->visible,
               external_commit_owner, presentation_scaling, update_restore_rect,
               wayland_win_data_configure_state_applied
832443138072  needs absent subsystems: commit_pending_state, data->visible,
               direct_client, direct_host_surface, evict_direct_client,
               external_commit_owner, wayland_toplevel_has_other_client_surface
cd9deaf11484  requires independent SHM-source/presentation-geometry tracking +
               conf->minimized/explicit_fullscreen config fields absent here
fed4bb6ce02b  needs absent subsystems: wayland_toplevel_has_other_client_surface
63e2a536ab75  needs absent subsystems: restore_rect_valid
8ebd3d14d142  needs absent subsystems: begin_present_wait, presentation_generation
65cdb3395927  needs absent subsystems: data->exstyle, data->style, data->visible,
               direct_client, direct_host_surface, presentation_scaling,
               restore_rect_valid, update_restore_rect,
               wayland_toplevel_has_other_client_surface
c99361d82bbe  needs absent subsystems: data->style, restore_rect_valid,
               update_restore_rect
dd9fc175a8ac  needs absent subsystems: data->visible, direct_client,
               direct_host_surface, wayland_toplevel_has_other_client_surface
3703d65b5877  needs wayland_surface_get_input_rect/fullscreen-rect subsystem
               (65cdb339/c99361d8 chain) that is unportable here
ddbcb11330af  needs surface->fullscreen_requested + mark_pending_commit tracking added
               by unported fullscreen-request chain
e405366c6a23  needs output_info_for_rect/output_rect helpers from the unported output-
               layout rewrite
db07d99ea645  direct-toplevel presentation is intentionally excluded from this series
86cdc51d82c2  toggles the intentionally-excluded direct-toplevel path
49750825574f  12-region rewrite of relative/pointer-mode input that this base predates
8e19ff7fa817  needs application_fullscreen_rect/presentation-geometry tracking
               (unported chain)
9681b586f96e  17-region swapchain retarget chain (VkFullScreenExclusiveEXT + output
               retargeting)
6a00c43c71fe  needs wayland_fullscreen_request tracking list
3c332ec38747  output_info_array primary-selection rewrite; this base predates it
9d4bdad83377  fix gates have_relative, a variable shape this base does not have (pre-
               split pointer path)
41a433b768c2  needs client_rect_in_toplevel cached-state cluster
dc94a72a7ffa  output_info_array primary-origin override rewrite
a4eb16883cc3  needs application_fullscreen_rect/present_rect fields
1a91dda5c8f9  presentation_rects decoupling chain (prerequisite of 8e19ff7)
82dd594113e2  presentation timing feedback: needs host present_results/managed host
               semaphore
13bb88cb72c8  89-region presenter-coordination rewrite across win32u+winewayland
a89aaf2ef60b  41-region presentation-feedback propagation across the managed present
               path
e4a20d77c8e1  adds a compositor-GPU helper with no consumer in this base
```

Original commits, in patch order:

```
78188ad14a63 64f5e7717f44 07fc8d5f15a6 9b6bebd0bbe4
4118360670df 52e7bad2e79e f8714d9001cb ea810ddd9d2c
10160c027097 a055ae2a97d2 bc6dcef4e6e0 ca982936c16c
02c4eff60c0e 8ecb567773e1 4d7ea64b5fb6 1d07ecb63c35
6f5dd4672d62 e593949be430 94731b30007f 064e4dfbb645
faca9084a3ba 98287ff6478b 822480e674f2 11f3f4b345e8
4e6a0a51523d 03f22c74dff4 a3e353247a52 908f7702fbf6
e6273c7edfda 0e6b602c6bb5 149f82607b85 aa37ca012187
735a85878f6d 25fe266deea7 a022197bcbd4 4305c5705863
27a744ebe76d 7dd872c007f4 f70d57bb55d3 acdab18126f5
8c2bddc055af 5ee287fae22a 3cb2dc25ff16 23eeb25347a5 c6bae4688911
3fb774e4b7a4 0846204b119a 9e6c1976ee15 85868e53809b
a42482e82bcb ca39b2cde0a5 1c4e67d85d8f 129fa64fc449
```
