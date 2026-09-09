# Disabled Wine patches

These files are retained for provenance, not applied by protonprep. Do not
restore them just to fill numbering gaps in an active series.

## EM-11 rebase (2026-09-09)

- `0190-win32u-Reset-variables-to-zero-within-WM_WINE_WINDOW.patch`:
  superseded by the refreshed active EM 0196
  (`795110c914208386ebaa13c5f45065661e004c3d`), which initializes the common
  window-state callback outputs. Do not apply both initialization patches.
- `0287-winewayland-Ignore-null-state-updates.patch`:
  EM `7a8f5e568ba3fb74fc6ce4757b59562595f0339e`, by Etaash Mathamsetty.
  GE `em-fixups/0012` already handles the null unlock callback before a
  window-data lookup. Keep that existing implementation.
- `0294-Revert-HACK-win32u-Implement-window-move-hack.patch`:
  EM `c68ed3899afee2e25da2eec6109076b2577e1b76`, by Etaash Mathamsetty.
  Erhan Bilgili's `wineland-child-rendering/0073` already removes the same
  hack (Wine-Wineland `6451d1cf979c`).
- `0025-winepulse-finish-nonfatal-pre-mainloop-hotplug-delta.patch`:
  moved from `patches/proton-ds5-haptic/`. After repairing the malformed
  hunk counts in active controller patch 0024, all of 0025 is already
  present. Reverse-application checking confirmed the duplicate. This does
  not remove the hotplug-worker behavior.

## Existing exclusions

- 0209 and 0210 target the older WMA decoder / extensionless URL media path.
  The GE video rework supplies the active media implementation; these
  overlapping imports remain disabled.
- `wine-wayland-offscreen-rendering/` contains the EM basic offscreen and
  CPU-readback rendering series. It overlaps the active Wine-Wineland
  cross-process DMA-BUF/EGL implementation. See that subdirectory's README.

Full import snapshot, attribution, and validation notes:
`../wine-wayland/README.md`.
