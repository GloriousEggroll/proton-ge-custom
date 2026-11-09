# Stronghold Crusader HD (Steam 40970): cursor speed and size

Stronghold Crusader HD under Proton has two problems on modern displays:

- **The cursor (and the scrolling) is much too fast** on high refresh rate
  panels. The game ties its speed to a 60 Hz vertical blank. Wine's DirectDraw
  presents at the real refresh rate, so on a 240 Hz display the game runs about
  four times too fast. With cnc-ddraw the same game is fine, because cnc-ddraw
  limits the game ticks (`maxgameticks`, 60 by default).
- **The cursor is tiny.** The game ships 32x32 cursors, which look very small on
  a 2560x1600 panel.

Both are fixed with two opt-in Wine settings, enabled only for this game
through its protonfixes entry (`gamefixes-steam/40970.py`). Every other game
keeps the old behaviour.

| Variable | Effect | Set for 40970 |
|---|---|---|
| `WINE_DDRAW_MAXGAMETICKS` | Limits `IDirectDrawSurface::Flip` to this many ticks per second (1 - 1000) | `60` |
| `WINE_CURSOR_SCALE` | Scales X cursors (0.25 - 8, bilinear, hotspot scaled) | `2.0`, only if the display is at least 1440 pixels tall |

The display height is read from `/sys/class/drm` (preferred mode of the first
connected display). If it cannot be read, the cursor size is left alone.

If you set either variable yourself in the Steam launch options, your value is
used instead:

```
WINE_DDRAW_MAXGAMETICKS=30 WINE_CURSOR_SCALE=1.5 %command%
```

## Where the code lives

- `patches/game-patches/ddraw-max-game-ticks.patch`
- `patches/game-patches/winex11-cursor-scale.patch`
- `patches/protonfixes/0006-gamefix-stronghold-crusader-hd-cursor.patch`
- both Wine patches are applied from `patches/protonprep-valve-staging.sh`

## How it was found

With cnc-ddraw's `maxgameticks=0` (emulate a 60 Hz vblank) the cursor speed is
normal, and with `maxgameticks=-1` (disabled) it is fast again. Wine's
`WaitForVerticalBlank` is a stub that returns immediately, and `Flip` waits for
the real vblank of the display.
