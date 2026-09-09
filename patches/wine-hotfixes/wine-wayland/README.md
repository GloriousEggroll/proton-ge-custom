# EM-11 patch import

## Source snapshot (2026-09-09)

- Wine bleeding-edge base: `542ca26b64ed53cc61de065ed4c0b0ab8802e7d6`.
- Previous Wine base: `9358696fe9a2261329f4a83aa6a65fd436106154`.
- EM source: local `etaash-wine/`, branch `em-11`, commit
  `5a1ae24b090b43a89b6ab9c116ec1c2795da0f10`.
- Upstream: https://github.com/Etaash-mathamsetty/wine-valve/tree/em-11
- Wine-staging snapshot: `6cc805ea57132eeaf44764e9213823c9b8d0d300`.

Existing patch numbers are retained for review. There are 287 active patches
in this directory; gaps are intentional. Unused imports are kept under
`../disabled/` with their original author headers.

## New commits

The update adds 13 active patches from the 15 new EM commits below. Authors,
dates, and original commit IDs are preserved in the patch mail headers.

| Patch | Source commit | Change | Author / disposition |
| --- | --- | --- | --- |
| 0286 | `3ed93905ceee` | Wayland comments | Etaash Mathamsetty |
| 0287 | `7a8f5e568ba3` | Ignore null window-state updates | Etaash Mathamsetty; disabled, duplicates GE 0012 |
| 0288 | `054c0770c73f` | Terminate clipboard class name | Nikolay Sivov |
| 0289 | `e6526dfc2378` | EVR rendering-preference tests | Nello De Gregoris |
| 0290 | `dd7dfbd95ae2` | Position windows from output enter/leave | Etaash Mathamsetty |
| 0291 | `78524df5beeb` | Image-description lifetime and color fixes | Etaash Mathamsetty |
| 0292 | `f035e2fc6e50` | Reset size hints only on state changes | Etaash Mathamsetty |
| 0293 | `0484cd7b40cd` | Use wl_output version 4 | Etaash Mathamsetty |
| 0294 | `c68ed3899afe` | Remove optional window-move hack | Etaash Mathamsetty; disabled, duplicates Wineland 0073 |
| 0295 | `21f4d8ae1e9e` | Check individual Vulkan returnedonly members | Remi Bernon |
| 0296 | `73e68369d2f2` | Exclude dynamic arrays from returnedonly | Remi Bernon |
| 0297 | `a886bddc2a9d` | Move display flush out of image-description helper | Etaash Mathamsetty |
| 0298 | `cc0e38f99125` | Extract flash-window helper | Etaash Mathamsetty |
| 0299 | `ce0fa7a14ec0` | Activation-token handling | Etaash Mathamsetty |
| 0300 | `5a1ae24b090b` | Probe owner hints above and left separately | Etaash Mathamsetty |

Patch 0196 was refreshed from EM commit
`795110c914208386ebaa13c5f45065661e004c3d`. It now initializes the shared
Win32u window-state callback output. The older, separate 0190 initialization
patch is disabled rather than applying both versions.

## Integration boundaries

The prep script applies this directory first, then GE's SNI patch
(`../em-fixups/0001`), then `../wineland-child-rendering/`, then the remaining
GE EM fixups. Wine-staging and the remaining Wine patches follow in their
existing order. The script is the authoritative application order.

Erhan Bilgili's Wine-Wineland cross-process DMA-BUF/EGL composition and SNI
work remains in use, with its author credit and source references preserved.
The overlapping EM offscreen/readback implementation remains disabled.
Wine-Wineland's native Steam-overlay implementation is not imported; GE's
existing overlay bridge and layer remain separate from Wine-Wayland.

Important rebase resolutions:

- Retain EM output enter/leave tracking and activation changes alongside
  Wineland surface state. Output-placement hints must not overwrite an
  already queued Win32 geometry or foreground-state update.
- Invalidate Wineland's cached size limits inside EM's new state-change
  conditional. Preserve GE's exact-aligned popup fallback after EM's
  separate left/above owner probes.
- Do not restore the old implicit thread-input attachment in
  `set_parent_window`, which the new Wine base removed.
- Preserve the GE Unix-only dispatcher and new upstream ARM64/ARM64EC signal
  handling. Keep EM's dispatcher thread name in the Unix entry point.
- `../em-fixups/0048` integrates output-reference cleanup and startup-token
  lifetime checks for the combined surface lifecycle. It credits the EM
  commits it adapts and does not add an overlay or change activation policy.
- Retain both EM and GE KMT adapter-information cases. Rebase FSR, WineOpenXR,
  and NVIDIA latency changes without dropping the current producer locking;
  latency submit information is attached before the actual queue submission.
- Keep the existing GE FFmpeg media and Sony controller stacks. Repair
  malformed hunk counts and short-context diffs so GNU patch applies their
  complete changes. Deduplicate overlapping media fields/drain logic and the
  controller hotplug delta documented in `../disabled/README.md`.

Four manually applied Wine-staging sets now live in GE-owned directories
under `../wine-staging/`: `ntdll-Hide_Wine_Exports`, `kernel32-Debugger`,
`ntdll-ext4-case-folder`, and `winex11-Window_Style`. This preserves their
rebased context without modifying the Wine-staging submodule. Their original
patch authors are retained.

## Validation

A clean, isolated replay on the pinned Wine base applied all 909 Wine patch
entries in prep order. GE/manual patches were checked and applied with GNU
`patch --fuzz=0`; automatic Wine-staging entries used `git apply`, matching
the staging installer backend. The resulting source tree matched the
resolved rebase tree exactly. The prep script passed `bash -n`.

The 19 component patch entries for DXVK, VKD3D-Proton, protonfixes,
low_latency_layer, WineOpenXR, Steam/UMU helpers, lsteamclient, and Wine-Mono
also applied with zero fuzz in separate clean snapshots. OptiScaler 0001 and
lsteamclient 0003 needed only diff-context normalization, not logic changes.
Wine-Mono was checked against the affected files from its pinned 11.2.0
source archive; no checksum pass was performed.

No builds or runtime tests were run. The working Wine and Wine-staging source
checkouts and unrelated component modifications were left untouched. Build
and runtime verification are still required, particularly window activation,
multi-output placement, child-process launchers, fullscreen transitions, and
the existing overlay/controller/media workflows.
