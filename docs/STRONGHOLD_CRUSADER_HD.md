# Stronghold Crusader HD (Steam 40970): cursor size and speed

Stronghold Crusader HD under Proton has two cursor problems on modern displays:

- the cursor is tiny (the game ships 32x32 cursors, which look very small on a
  2048x1280 panel with fractional scaling);
- the cursor moves too fast. The game reads raw mouse deltas, which are not
  accelerated or scaled, so it feels much faster than the desktop pointer
  (cnc-ddraw works around this with its mouse sensitivity setting).

Both are fixed with two opt-in Wine settings, enabled by default only for this
game through its protonfixes entry (`gamefixes-steam/40970.py`). Every other
game keeps the old behaviour.

| Variable | Effect | Default for 40970 |
|---|---|---|
| `WINE_CURSOR_SCALE` | Scales X cursors (0.25 - 8, bilinear, hotspot scaled) | `2.0` |
| `WINE_RAW_MOUSE_SCALE` | Scales raw mouse deltas (0 - 16, sub-pixel remainder kept) | `0.6` |

If you set either variable yourself in the Steam launch options, your value is
used instead:

```
WINE_CURSOR_SCALE=1.5 WINE_RAW_MOUSE_SCALE=0.5 %command%
```

## Where the code lives

- `patches/game-patches/winex11-cursor-scale.patch`
- `patches/game-patches/winex11-raw-mouse-scale.patch`
- `patches/protonfixes/0006-gamefix-stronghold-crusader-hd-mouse.patch`
- both Wine patches are applied from `patches/protonprep-valve-staging.sh`

Tested on Wayland (XWayland), 2048x1280 panel, game running at 1920x1080.
