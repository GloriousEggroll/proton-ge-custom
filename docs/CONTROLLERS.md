---
How controllers work in Proton
---

There are five methods that Windows games can use to access controllers:
dinput, xinput, winmm, hid, and rawinput. Games can use any combination of all
of these APIs.

rawinput allows direct access to the gamepad hardware. The application must
know the HID protocol, and/or know the device-specific protocol for
non-standard devices like Xbox controllers.

hid is a layer above rawinput, where Windows will talk HID to the controller on
the game's behalf. This turns the raw HID protocol data into usable things like
buttons and joysticks.

dinput is a "legacy" API that allows applications to talk to any type of
joystick. On Windows, it is implemented on top of HID. Notably, dinput allows
easy access to controllers that no other API does, so it is still used by
modern games despite being "legacy."

xinput is the new API that supports only Xbox controllers. On Windows, it is
likely implemented on top of rawinput, as Xbox controllers do not behave like
standard HID devices.

winmm is the very legacy API, for when joysticks were hooked up through the
soundcard. On modern Windows, it is implemented on top of dinput.


Here is a diagram for how these APIs are mapped down to the system by Proton:


                ----------
               | game.exe |
                ----------
               /  | |  |  \
              /   | |  |   \        application
    *********/****|*|**|****\******************
             |    | |   \    \             wine
             |    | |    |    \
         ------   | |   -----  \
        |xinput|  | |  |winmm|  |
         ------   | |   -----   |
             |    |  \    |     |
             |    |   |   |     |
             \    |   ------    |
              \   |  |dinput|   |
               \  |   ------   /
                | |  /        /
                | | |        /
                 ---        /
                |hid|      /
                 ---      /
                  |      /
                  |     |
                 --------
                |rawinput|
                 --------
                    |
               -----------
              |winebus.sys|
               -----------
                |      |                   wine
    ************|******|***********************
                |      |                  linux
                |    ----
                |   |SDL2|
                |    ----
                |    |   \
                |    |    \
                |    |     |
                ------    -----------
               |hidraw|  |input event|
                ------    -----------
                    |       |
                     \     /
                    ========
                   |hardware|
                    ========

Some things to note:

SDL2 provides the controller mapping feature of the Steam client. If you don't
go through SDL2, then you don't get that mapping feature. Also notice that
winebus.sys must turn SDL2 events into usable winebus data (HID protocol). We
also allow direct access to hidraw devices so games which can speak HID (or
other) protocol can talk directly to those devices.

Xbox controllers do not speak real HID. Instead Windows provides a HID
compatibility layer so dinput, which is implemented on top of HID, will present
the Xbox controller to legacy games. Of course some games (Unity) have noticed
that, and talk directly to this internal HID interface, so we need to duplicate
it bit-for-bit in winebus.sys.

Some games support talking directly to certain controller types. For example,
many modern games support PlayStation 4 controllers directly and will provide
layouts and button images in-game specific to DualShock 4 controllers. For this
reason, we don't want to present every controller through xinput, which should
only present Xbox controllers.

However, we also want users to be able to use any controller, even if the game
only supports xinput. Steam provides a controller mapping feature, which is
presented as a virtual Steam Controller. We turn this virtual Steam Controller
into an xinput device. This means any controller which is mapped will appear to
the game as an xinput device, in addition to the other APIs. Controllers which
are not mapped will appear as the real controller, which the game may or may
not support.

One final snag is that many distros do not allow user access to hidraw devices.
Steam ships some udev rules to allow this for certain common controller types,
but not most. In other words, your user may not have access to the hidraw
device for your controller, especially if it is a less well-known controller.
In those cases, we access it through SDL2 via its linux js backend and try to
treat it as an Xbox controller, even if it is not mapped with the Steam client
mapping feature.

## Automatic Sony XInput Fallback

GE-Proton sets `PROTON_SONY_AUTO_XINPUT=1` by default. With Steam Input disabled,
Wine can provide an XInput slot for a native DualShock 4, DualSense, or DualSense
Edge without changing its HID descriptor, VID/PID, or device interfaces. XInput
uses the existing Sony report and conventional rumble translations. Native HID
remains available for games with PlayStation support.

Ownership is per HID collection and Wine process:

- XInput marks its internal file handle before submitting any input reads.
  Reads on that handle cannot claim native ownership, including reads from a
  second XInput DLL in the same process.
- DirectInput marks its own HID transport handle before reading too. Generic
  DirectInput polling is not evidence of a native PlayStation implementation:
  Lunar acquires and polls DirectInput devices even while checking XInput.
  These reads do not withdraw the automatic slot or its Steam identity.
  DirectInput remains usable; a separate game-owned HID/Raw Input reader can
  still take over. The exemption is per open file, not per device or process.
- Two successful input reads on a different handle signal native use. This
  includes `ReadFile`, `HidD_GetInputReport`, and consumed HID Raw Input data.
  Enumeration, capability/feature queries, opening a handle, and one probing
  read do not count as native ownership.
- Only the automatic slot becomes disconnected when native input is consumed.
  Its state, capabilities, rumble, and synthetic Steam identity become
  unavailable. Other controllers and the XInput API remain enabled.
- Closing the last native consumer restores eligibility. Raw Input consumers
  keep their claim across device-list refreshes and release it on unregister
  or device removal. Unplug/replug starts with a fresh device claim.
- A reader in another process cannot withdraw this process's fallback.

The two-read rule is a heuristic, not proof that a game selected a working
native controller. Middleware that continuously reads HID but ignores those
reports can still trigger takeover. Such games may need an explicit override.
Games which never refresh their XInput slots after a disconnect also need
individual testing.

### Priority and Steam Identity

Steam's virtual XInput device and a native Steam Input session remain
authoritative. `SteamVirtualGamepadInfo`, `SteamVirtualGamepadInfo_Proton`, and
`SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD` are discovery hints, not proof
of ownership: Steam can export them with per-game Steam Input disabled. They
do not suppress automatic fallback. An actual live Steam virtual device or
native Steam API controller still takes priority. Native Steam API ownership is
latched for the process lifetime, matching the existing explicit Steam Input
fallback policy. Explicit `PROTON_EMULATE_STEAMINPUT=1` disables automatic Sony
discovery as before.

Existing explicit Sony identity/XInput options disable automatic device
discovery. They retain their previous behavior, including game-specific
defaults such as Diablo IV's Edge identity. The persistent-player-one workaround
does not disable discovery: its placeholder can use an automatic Sony slot and
returns neutral input when that slot is disconnected or native input takes over.
Physical Xbox devices are not converted. Automatic fallback requires a Sony
HID gamepad exposed by Wine; it cannot fix Linux hidraw permission failures.

When lsteamclient is loaded, eligible Sony slots also provide controller type,
XInput index, and button origins through ISteamInput and the PlayStation-capable
legacy ISteamController006/007/008 interfaces. DS4 uses PS4 metadata; DualSense
and Edge use PS5 metadata on SteamInput002 and newer or SteamController008.
SteamInput001 and SteamController006/007 predate PS5 support, so their synthetic
controller type and button origins use PS4-compatible metadata instead. This
lets older games such as Grandia and Grandia II select their PlayStation icons
without a per-game profile override. The underlying controller handle keeps its
physical identity for hotplug, native takeover, and rumble; real Steam handles
are not rewritten. Face buttons follow physical positions: A is Cross,
B is Circle, X is Square, and Y is Triangle. Stale synthetic handles report
unknown/disconnected after native takeover rather than being passed to Steam.

This is API metadata, not an installed Steam configuration or a general Steam
Input action-manifest interpreter. The existing known action bindings also work
for automatic Sony slots, including Ragnarok's game and menu actions. Unknown
action names are not mapped. Action data uses the same availability and physical
type checks as identity queries: native takeover or a stale handle yields
inactive data, and real Steam handles pass through unchanged. Explicit
`PROTON_STEAMINPUT_FALLBACK=1` additionally permits non-Sony XInput controllers.
Games must query the identity/origins and have corresponding artwork to show
PlayStation icons. XInput fallback still works without lsteamclient, but Steam
identity queries naturally require it.

Set `PROTON_SONY_AUTO_XINPUT=0` to restore non-automatic discovery. For games
with broken native controller handling, the existing
`PROTON_SONY_HIDRAW_XINPUT=1` forces translation instead of automatic takeover.
For that explicit mode, the translated HID companion's DirectInput
`DIPROP_VIDPID` also reports Xbox 360 identity, matching XInput capabilities.
Assassin's Creed uses this query to choose its XInput handler; a Sony ID sends
it down its generic joystick path instead.

Automatic fallback now uses that same DirectInput identity while the specific
device has an available automatic XInput slot. The query goes through Wine's
XInput ownership checks, including before the game's first XInput poll. Native
takeover, real Steam Input, an explicit compatibility override, a full slot
table, or failure to create the slot leaves the Sony identity unchanged. The
underlying HID IDs, paths, instance GUIDs, and audio identity remain unchanged;
this is only a DirectInput `DIPROP_VIDPID` projection, not a converted HID device.

The broad per-game `PROTON_SONY_HIDRAW_XINPUT=1` list in the Proton script has
been removed in favor of this default. Do not set that flag globally in automatic
mode: it changes device creation and disables native takeover. The explicit
override remains available for broken native implementations. Castlevania keeps
its persistent-player-one default, but no longer forces Sony translation.
Ragnarok, Lunar, and Horizon Zero Dawn Complete Edition also use automatic
discovery instead of both forced flags. Horizon no longer forces DualSense to
DS4 HID identity: that override disables automatic discovery. Its SteamInput001
interface gets PS4-compatible metadata from the legacy-interface projection,
while the existing known action bindings provide input from the automatic slot.
Other remaining game-specific Steam Input defaults need individual validation.
Assassin's Creed still requires selecting the controller in
its in-game controls menu; it does not switch automatically from keyboard/mouse.
This does not enable native touchpad/motion or adaptive-trigger support through
XInput.

### Hotplug and Speaker Ordering

XInput enumerates present interfaces through cfgmgr32, as DirectInput already
does. It must not wait for native Sony speaker publication while holding the
controller lock. The Castlevania hotplug trace showed a replacement Edge opening
in slot zero, followed by one report processed every three seconds while HID
reports continued arriving. No native takeover occurred in the game process.
Empty-slot polling had repeatedly entered SetupAPI's speaker wait under that
same lock, delaying all input reads. Fresh prefixes with no WINEXINPUT interface
class still proceed to automatic Sony discovery.

Native SetupAPI enumeration and HID arrival retain the existing speaker ordering
for games that need controller audio. When the arrival worker's wait expires,
it now releases the named gate before dispatching the arrival notification.
Otherwise overlapping enumerators can keep an unsignalled event alive and repeat
the timeout indefinitely. This release ends the ordering wait; it does not claim
that a speaker endpoint exists.

The private Sony identity/VID/PID probes used by lsteamclient and DirectInput
read the initialized, hotplug-maintained XInput list rather than rescan missing
slots. Spider-Man Remastered's no-logging slowdown was captured on the main
thread in `GetConnectedControllers` -> `__wine_XInputGetDeviceVidPid` ->
`update_controller_list` -> cfgmgr32 HID registry enumeration. Polling all four
slots repeatedly triggered full scans for the empty ones. Initial discovery,
device notifications, native ownership checks and public XInput's synchronous
rescans are retained; the latter prevent races when games query slots from
their own device-arrival notification.

### Validation Matrix

The Wine `dinput:hid` tests include a synthetic two-controller ownership test:
invalid registration, enumeration/probe exclusion, internal read exclusion,
successful native reads, separate-device isolation, a second fallback handle,
explicit Raw Input activity/release, and restoration after native close.
DirectInput transport tests cover invalid registration, read exclusion, native
ownership priority, late registration, and closing/reopening an exempt reader.

After building, additionally test:

1. XInput-only game, Steam Input disabled: DS4, DS5, and Edge connected before
   startup and hotplugged, over USB and Bluetooth. Verify sticks, D-pad, triggers,
   face buttons, Start/Back, and rumble.
2. Native HID game: verify native input takes over without duplicate actions and
   native controller audio/haptics still work. Include raw HID and Raw Input
   consumers, both buffered and unbuffered.
3. An Xbox plus two Sony controllers: native ownership of one Sony must not
   remove the other slots. Repeat with multiple XInput DLL versions loaded.
4. A second process reading the same Sony device: no takeover in the game.
5. Steam Input enabled: only Steam's mapping should drive the game; no automatic
   duplicate slot or synthetic Steam identity. Repeat with overlay open/close.
6. A game querying Steam Input identity/origins: verify PS4/PS5 types, Square and
   Triangle ordering, and stale-handle behavior after native takeover/hotplug.
7. Explicit existing overrides and `PROTON_SONY_AUTO_XINPUT=0`: retain their old
   behavior on X11 and Wayland. For Castlevania, leave forced Sony translation
   unset: start with no controller, then connect and replace DS4/DS5/Edge/Xbox
   devices. Player one must stay connected, with neutral input while unplugged
   or while its automatic physical slot is withdrawn by native input ownership.
   For Assassin's Creed (15100), leave `PROTON_SONY_HIDRAW_XINPUT` unset and
   select the controller in the game. Verify that `+dinput,+xinput` shows
   `Reporting automatic Sony XInput fallback` followed by XInput calls from
   `AssassinsCreed_Dx10.exe` or `AssassinsCreed_Dx9.exe`, not only from Xalia.
   Test DS4, DS5, and Edge. Repeat with the forced flag and verify the existing
   `Reporting forced Sony XInput companion` path. Native HID games must retain
   Sony HID identity and regain Sony DirectInput VID/PID after takeover.
8. Query DirectInput VID/PID before any XInput API call, after native consumption,
   after closing the native consumer, and while real Steam Input is active.
   Only an available automatic slot should project Xbox identity. Fill all four
   slots with other controllers and verify that an unrepresented Sony stays Sony.
9. With Steam Input disabled, leave Steam's virtual-gamepad environment hints
   present and both `PROTON_SONY_HIDRAW_XINPUT` and `PROTON_STEAMINPUT_FALLBACK`
   unset. Test Sky (2325290) and Assassin's Creed (15100). Verify an
   `automatic Sony fallback 1` slot is opened despite those hints. Repeat with
   Steam Input enabled and verify a real Steam device/session takes priority.
10. On a fresh Castlevania prefix, start with DS4 and replace it with Edge, then
    reconnect DS4. Repeat starting with no controller, and with an Xbox device.
    Input must continue without multi-second stalls or bursts of queued buttons.
    Also retest Spider-Man speaker audio through DS4/DS5/Edge replacement: the
    native HID/audio ordering remains enabled. For timing diagnostics use
    `+timestamp,+pid,+tid,+xinput,+setupapi,+service`; full `+hid` report dumps are
    not needed for the retest and can substantially increase logging overhead.
10. Query a synthetic DualSense/Edge handle through SteamInput001 and
    SteamInput002: the old interface must report PS4 type 5 and PS4 button
    origins, the newer interface PS5 type 13 and PS5 origins. Repeat with
    SteamController007/008 and verify their distinct origin enum values.
    DS4/Xbox types must stay unchanged, withdrawn handles must remain unknown,
    and native Steam handles must pass through. Retest Grandia/Grandia II's
    icons with no layout override; modern games must retain PS5 identity.
11. Ragnarok with both forced flags unset: verify known game/menu action names
    are mapped, buttons and sticks produce active action data, and unknown
    names remain unmapped. Withdraw the automatic slot through native input or
    replace it with a different controller type: stale handles must return
    inactive data. Repeat with real Steam Input enabled to check its priority.
12. Lunar on a fresh prefix, with both forced flags unset: enter the launcher
    and Lunar I/II with DS4, DualSense and Edge. Verify DirectInput acquisition
    does not cause `Sony native takeover` for the game process and that XInput
    and Steam identity queries continue with the appropriate PlayStation icons.
    Check for duplicate actions in mixed DirectInput/XInput games and verify
    DirectInput-only games still work. Real native HID and Raw Input readers
    must still withdraw the automatic slot even with DirectInput acquired.
13. With logging disabled, compare Spider-Man frame rate and Monster Hunter menu
    responsiveness with one Sony controller and three empty slots. Repeat with
    no controller, hotplug, native HID takeover and Steam Input enabled. After
    initialization, private identity probes must not enumerate registry keys.
    Also verify public XInput queries from device-arrival handlers still find
    newly connected controllers (the upstream RiME hotplug case).

Use `PROTON_LOG=1 WINEDEBUG=+timestamp,+pid,+tid,+xinput,+hid,+rawinput,+steamclient`
for a short reproduction. These traces can be large; they are not recommended
for routine gameplay. No build or hardware/game validation was performed when
this change was prepared.

## Death Stranding Controller Speaker Hotplug

The Director's Cut controller-audio compatibility hook keeps the game's
`Default device` selection separate from the concrete Wwise endpoint hash.
Previously, startup replaced the saved selection with the first controller's
hash, and later endpoint arrivals were consumed only when audio settings were
applied again. A DS4/DualSense endpoint could keep working across replacement,
while an Edge with a different endpoint ID received haptics but no speaker
stream. Manually selecting that Edge in the game restored its speaker.

For default selection, a changed controller endpoint now posts a private
message to a message-only window created on the game's audio-settings thread.
That thread validates the endpoint through the existing game selector and
calls the signature-checked native Wwise output transition. The MMDevAPI device
worker never calls game audio routines, and there is no polling thread or
timer. The saved setting remains default. Explicit output choices are not
replaced, and backend endpoint identities and physical stream bindings remain
unchanged. This is gated by `PROTON_DEATH_STRANDING_CONTROLLER_EFFECTS`, like
the existing game-specific hooks; other games do not create this window.

Runtime validation after building is still required: select `Default device`
again if an older build saved a concrete endpoint, then replace DS4, DualSense
and Edge in both directions without reopening settings. Check speaker audio
and haptics, starting both with and without a controller. An explicitly chosen
speaker must remain selected across hotplug. Use
`+timestamp,+pid,+tid,+mmdevapi,+pulse,+plugplay,+service,+xaudio2`; avoid full
`+hid` report logging. The `Refreshed Death Stranding default controller output`
trace means the native transition was requested, not that audible output was
verified. If it is missing, check the output-window creation trace and whether
the game dispatches the posted message before investigating Pulse routing.
