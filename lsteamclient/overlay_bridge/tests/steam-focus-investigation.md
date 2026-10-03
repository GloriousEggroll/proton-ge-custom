# Steam controller focus IPC experiment

Status: the user confirmed Guide closing worked after the September 20 focus
timing correction. Warframe later reproduced a failure after a Steam update.
Support for the inspected September 27 library is now active in the user's
build. Guide toggling recovered after focus reassignment, but fresh-launch
ClientUI recovery is a new source change awaiting integrated testing.
The diagnostic files themselves are not installed or run by Proton.

## Warframe follow-up on 2026-09-27

- `~/steam-230410-guide.log`, collected at 21:15-21:16 using
  `GE-Proton11-7-26-g10ba9e362`, confirms the adapter is enabled, its bridge
  query is loaded, X11 focus is PointerRoot, and the focused/open overlay is
  230410 while controller context is 769. No assignment occurs because the
  existing guard only accepts Desktop (413080).
- Steam's `controller_ui.txt` records the DualSense Edge Guide presses.
  `webhelper_js.txt` routes them to `ControllerConfigurator_uid2382871`.
  The installed desktop overlay's `HandleGamepadGuideButtonEvents` ignores
  events whose `nAppID` differs from the overlay's app ID. This supports an
  app-focus routing mismatch, not missing controller button reports. The
  event's actual `nAppID` is not printed in those logs.
- The first standalone write attempt (21:31:26-21:32:56) timed out on desktop
  focus without writing anything. In the second, overlay state became
  `(230410, True, 0)` at 21:34:23; the probe assigned game 230410/PID 2409498
  and read back that context. No Guide presses occurred during its 60-second
  observation. It restored Desktop and exited at 21:35:23; the integrated
  adapter reassigned Warframe at 21:35:24.
- At 21:36:14, 21:36:19 and 21:36:23, Steam recorded Guide presses under the
  Warframe context. The user confirmed close/open/close all worked. There
  were intervening native focus transitions and Desktop corrections. Thus
  recovery works, but this is NOT proof that forcing a still-active ClientUI
  assignment has been tested or that a fresh launch is fixed.
- The new candidate accepts ClientUI only for this game's focused/open
  overlay with rejected X11 proxy focus, and only when the versioned public
  `SteamClient017` / `SteamUtils009` interface reports non-Big-Picture mode.
  Missing public interfaces fail closed for ClientUI. Overlay state and
  bridge ownership are rechecked after IPC, before assigning focus. Desktop
  behavior, other games, input suppression and focus-loss cleanup remain.
- No build or patch-prep is run by the agent. Next manual checks: fresh
  desktop launch, repeated Guide cycles without Alt+Tab, controller-settings
  navigation, actual Alt+Tab away/back, and a Big Picture regression check.
- The user reports frame-time jitter disappeared after restarting the
  desktop session and explicitly deferred that investigation. No performance
  change is being made for it.

## Steam update on 2026-09-27

- The installed host `linux64/steamclient.so` now has ELF build ID
  `24adc837a4068a882b963cc9531cf05a11a50a0c`. The old adapter correctly rejects
  this previously uninspected build, so its focus correction cannot run.
- Disassembled the five relevant methods and inspected the actual interface
  table through an independent Steam connection. Slots and argument layouts
  are unchanged; addresses are not. The new offsets are:

  | Interface / slot | Method | Offset |
  | --- | --- | --- |
  | ClientEngine / 14 | GetIClientUtils | `0x1611e70` |
  | ClientUtils / 48 | SetFocusedWindow | `0x13b5fd0` |
  | ClientUtils / 54 | GetFocusedGameWindow | `0x1369500` |
  | ClientUtils / 103 | GetFocusedGameID | `0x1337000` |
  | ClientUtils / 104 | GetFocusedWindowPID | `0x12f53a0` |

- Added a separate allowlisted build profile rather than relaxing validation.
  The probe also reports controller-context transitions, independently of
  overlay visibility transitions. No game attachment or focus write was used
  to inspect the new interface table.
- At 19:47:30-19:47:31, the updated probe's read-only path completed with no
  game running. It returned overlay `(0, False, 0)` and controller context
  `(413080, 0)`, then released its connection (exit 0). No focus writes were
  performed. `git diff --check` also passed.
- In the Warframe capture, Steam's `controller_ui.txt` switches from Warframe
  (230410) to ClientUI (769), not Desktop (413080), when the overlay opens.
  Guide presses are still logged. ClientUI must not be overwritten blindly:
  the adapter intentionally preserves Big Picture. See the later follow-up
  above for live evidence and the guarded integration candidate.
- No build or full patch-prep was run. The new allowlist alone is not yet a
  verified fix for Warframe's Guide-close failure.

## Failed integration run and focus timing correction

- The Sep 20 19:35 run in `~/steam-2344520.log` still switched Steam's controller
  context to Desktop when the overlay opened. The installed
  `GE-Proton11-7-x86_64/files/lib/wine/x86_64-unix/lsteamclient.so` contains the
  adapter; its bridge exports the focus query and win32u exports the cursor
  suppression helper. This was not an omitted patch or older installed binary.
  The installed Steam library still matches the inspected build ID below.
- A separate native X11 diagnostic, with no game running, created an invisible
  InputOnly proxy matching the bridge. Immediately after XSetInputFocus,
  XGetInputFocus returned that proxy. At 10 ms, 50 ms, 200 ms and 500 ms it
  returned PointerRoot instead. The diagnostic destroyed its temporary window;
  it did not attach to or modify any game.
- The first candidate cached only the immediate successful reply. Its cached
  `overlay_x11_focus_rejected` therefore suppressed the fallback even after
  Labwc revoked focus. The query now rechecks current X11 focus and per-game
  selection ownership on callback polling, including the pre-assignment
  recheck. It returns -1 on bridge mutex contention, not a false focus loss.
  No Steam IPC occurs while the bridge/X11 locks are held.
- `GE_WAYLAND_STEAM_OVERLAY_DEBUG=1` now also reports adapter initialization,
  bridge query resolution, focus rejection transitions and actual assignments.
  This avoids requiring verbose per-call `+steamclient` tracing for retests.
- These edits do not alter winewayland, wlroots or the cursor implementation.
  Whether duplicate cursors also persisted in this run remains unconfirmed.
  No build, compilation or patch-prep run was performed for this correction.

## Verified on 2026-09-20

- On Labwc/wlroots, the existing InputOnly focus target immediately loses X
  focus to PointerRoot. With desktop Steam, opening the Wayland overlay routes
  controllers to Desktop (413080); Guide events then do not close the game's
  overlay. Keyboard toggling still works.
- The user confirmed the existing build works on KDE. Big Picture also worked
  in earlier testing. Do not revert to an InputOutput proxy: that previously
  caused native Wayland focus loss/flicker on Mutter.
- The host `linux64/steamclient.so`, ELF build ID
  `7b0847f04cf284f01a1a165561df054902df31a7`, exports
  `CLIENTENGINE_INTERFACE_VERSION005` through `CreateInterface`.
- An independent native process successfully created its own Steam pipe,
  connected to the current user, and obtained `IClientUtils`.
- Its `SetFocusedWindow` call changed Steam's controller context to 2344520
  for five seconds, then explicitly restored Desktop. `controller_ui.txt`
  recorded the game assignment at 18:38:55 and restoration at 18:39:00,
  with no intermediate Desktop reassignment. This test did not launch or
  attach to a game, manipulate X focus, or open an overlay.
- This proves that a normal external Steam connection can set controller
  focus. The subsequent live test confirmed one Guide close; the integrated
  implementation still needs runtime validation of repeated cycles/focus loss.
- The saved probe's read-only path (including the slot 54 getter) completed
  successfully at 18:51:48-18:51:49 with no game running, returning
  `(0, False, 0)` and releasing its connection. Its CLI help and the repository
  `git diff --check` also passed. The five-second write used the preliminary
  probe; the saved probe's live write was tested subsequently as described below.

## Live overlay test on 2026-09-20

- Diablo IV ran using `GE-Proton11-7-x86_64` with host game PID 314165.
  Approving commands/replying in the terminal removed actual overlay focus;
  the getter then returned `(0, False, 0)` and the write guard refused the call.
  Added `--wait-focus` so the user can return to the game after approval.
- At 18:56:56, the getter returned `(2344520, True, 0)` and the probe assigned
  controller focus to game 2344520, PID 314165. Steam's `controller_ui.txt`
  confirmed the game profile assignment at the same time. The getter's PID
  field was zero here; it must not be treated as the actual game process PID.
- At 18:57:17, Steam logged `Guide button sent to JS` using the game profile;
  the getter changed to `(2344520, False, 0)`. This is evidence of a successful
  Guide close after the API assignment, with no injected key or game attachment.
- At 18:57:56, the probe completed, restored the closed-overlay game assignment,
  and released its own user/pipe (exit 0).
- At 18:58:05, another Guide press was followed by reassignment to Desktop
  (413080). Further Guide presses at 18:58:06-18:58:08 used Desktop. This
  supports the hypothesis that reopening the overlay invalidates Steam UI's
  focus cache and reinstates the incorrect context. The probe had already
  exited, so that second open/close sequence was not observed by the getter;
  the user subsequently confirmed exactly this behavior: one successful close,
  then failure after reopening.

The implementation must handle each overlay activation and genuine focus
changes, rather than setting focus only at startup. The exact ordering against
Steam's own focus update still needs runtime validation.

## Integration candidate

- `patches/lsteamclient/0010-lsteamclient-maintain-wayland-overlay-controller-focus.patch`
  creates `lsteamclient/steam_overlay_focus.h` and hooks it into `unixlib.cpp`
  after native callback retrieval. The prep reset list includes `unixlib.cpp`
  and removes the generated adapter header before reapplying the patches.
  Only the custom `overlay_bridge/` sources are maintained directly in the
  repository and left untouched by prep. The callback's existing Steam pipe
  is reused and no worker or additional pipe is introduced.
- `ge_overlay_bridge_needs_controller_focus()` uses a trylocked native-focus
  check, then queries current X11 focus and proxy selection ownership. It
  returns -1 when busy; that is not interpreted as focus loss. The initial
  snapshot-only implementation was insufficient; see the timing correction above.
- At most every 250 ms of Steam callback servicing, the adapter queries the
  focused overlay and controller context. Only a focused/open overlay with
  Desktop or eligible non-Big-Picture ClientUI context gets reassigned. It
  does not change Steam Input enablement.
  It reads back context rather than relying on an activation edge, so Steam's
  later Desktop assignment can be corrected without artificial key presses or
  an assumed activation delay. Genuine focus loss releases only the adapter's
  game/PID assignment; other games and Big Picture are left alone.
- Safety guard: only the inspected Linux x86_64 ELF build ID and vtable entries
  are allowed. Other Steam builds/architectures skip this path. It is not a
  version-independent private ABI. Games pausing all Steam callbacks on focus
  loss may need another approach; this candidate does not add a polling thread.
- The user also reported the game cursor overlapping the overlay cursor.
  The bridge previously hid the hardware cursor only once. Wine's independent
  pointer-enter/cursor-update paths could overwrite that request. Added generic
  `__wine_suppress_driver_cursor()` alongside replay via
  `em-fixups/0053-win32u-preserve-overlay-cursor-ownership.patch`. It preserves
  cached game cursor updates while passing NULL to the driver during overlay
  ownership, including the driver's cached pointer-enter state. A generation
  check retries concurrent changes without holding the cache lock in a driver
  call. NULL-window cursor requests retain their original driver calls.
- No winewayland or wlroots edits, no game attachment/modification, no build,
  no compilation, and no patch-prep run were performed.

Validation: `git diff --check`, prep-script `bash -n`, and reverse patch checks
passed. On temporary copies, the new cursor patch and the subsequent existing
cross-process cursor-sharing patch were reversed and reapplied in prep order;
the files matched the edited Wine source byte-for-byte. The adapter's two
additional read-only getters were validated from the standalone process at
19:09:22-19:09:23, returning Desktop (413080), PID 0. This is not a C/C++ build
or a runtime validation of the integrated adapter/cursor suppression.

Next test after the user's build: repeated Guide open/close from desktop Steam
on Labwc, then alt-tab away/back while open and while closed; verify controller
input does not reach the game while open. Check mouse re-entry with no duplicate
cursor and exact custom/hidden game cursor restoration. Also check Shift+Tab,
keyboard text entry, Big Picture, KDE, XWayland and native EGL/OpenGL paths.

Steam was started in desktop mode via the transient user service
`steam-focus-api-test.service`. No build or compilation was run.

## Probe

`steam_focus_probe.py` is standalone Python using ctypes. It is not installed
or invoked by Proton. It checks the inspected ELF build ID and private method
layout, and refuses other builds. Numeric function offsets only validate the
diagnostic's vtable entries; they must not become a production binary patch.

With Steam running, observe the focused overlay instance without changing it:

```sh
python3 lsteamclient/overlay_bridge/tests/steam_focus_probe.py --seconds 30
```

To test a real failure, launch Diablo IV with Wine-Wayland from desktop Steam,
open its overlay, and leave it open. Identify the actual host Diablo IV process
PID, not the gameoverlayui process. Run:

```sh
python3 lsteamclient/overlay_bridge/tests/steam_focus_probe.py \
    --focus-game 2344520 --focus-pid GAME_PID --wait-focus 40 --seconds 60
```

Return to the game while the probe waits for its focused overlay. After the
`SET controller focus` line, keep the game focused and press Guide once.
The probe sends no keyboard/controller events, performs a single focus write,
and only observes afterwards. It refuses to write unless the target game's
overlay was already focused/open. On completion or interruption it attempts to
restore routing appropriate to the resulting overlay state, then releases its
own Steam user/pipe. Avoid unrelated window switching during this first test.
Read the new `OnFocusWindowChanged` and Guide entries in
`~/.local/share/Steam/logs/controller_ui.txt` alongside the probe output.

The diagnostic's cleanup is not a finished cross-process focus ownership
policy. Steam IPC can block, and SIGKILL cannot execute cleanup. If routing
remains wrong after an interrupted test, refocus a Steam/XWayland window or
restart Steam. Do not run unattended loops or add this to game launch options.

## Integration requirements after the real-overlay test

- Use lsteamclient/its native overlay bridge, not winewayland or wlroots.
- Real Wayland keyboard focus must be authoritative. Preserve Vulkan and EGL
  input isolation and cursor handling. Do not manufacture Shift+Tab/Guide keys.
- Verify repeated Guide cycles and that Steam cannot override the assignment
  on an overlay transition. The idle five-second result is insufficient.
- Release input on genuine alt-tab; handle competing games/launcher processes,
  stale callbacks, and process exit without overwriting a newer focus owner.
- Preserve Steam Input's per-game disabled setting. Reporting a focused game
  must not force translation or change controller profiles/config files.
- Avoid IPC under the bridge/Wayland/display locks or on its event dispatcher.
  Do not introduce a worker/pipe that keeps games alive after exit.
- Private `IClientUtils` is not covered by the engine's interface version.
  Obtain a defensible ABI validation strategy and safely disable the path on
  an unsupported Steam build/architecture. Never call guessed vtable slots.
- If changing ordinary lsteamclient sources, synchronize a proper
  `patches/lsteamclient/*.patch` and the reset list in
  `patches/protonprep-valve-staging.sh`. Bridge-owned files currently live in
  the main repository rather than being regenerated from that patch series.
- Do not build or run patch prep: the user builds and tests separately.

## Local ABI evidence (diagnostic only)

Manually inspected generated IPC proxy and server dispatch in the above
native library, not the similarly named `steamrt64/steamclient.so`:

- Engine slot 14: `GetIClientUtils(pipe)`.
- Utils slot 48: `SetFocusedWindow(CGameID, bool force, bool, uint32 pid,
  uint16, uint16)`. CGameID is passed by invisible reference in this ABI.
  The last fields/second bool are not fully named; the probe uses false/zero.
- Utils slot 53: `GetGameOverlayUIInstanceFocusGameID(bool *, uint32 *)`.
  This can fall back to a game when no overlay instance has focus.
- Utils slot 54: `GetFocusedGameWindow(bool *, uint32 *)` reads the actual
  focused overlay instance. Both getters use an implicit CGameID result pointer.
- Utils slot 103: `GetFocusedGameID()`, implicit CGameID result pointer; slot
  104: `GetFocusedWindowPID()`, uint32 return. These read the controller context
  written by SetFocusedWindow, rather than the overlay-instance focus map.
- Steam's desktop UI caches its computed focused game before calling the setter;
  it does not simply overwrite the setter on every poll. Overlay/window changes
  can invalidate that cache, which is why runtime transition tests remain needed.

Historical Open Steamworks declarations were checked, but have different
signatures and are NOT suitable as current ABI definitions.
