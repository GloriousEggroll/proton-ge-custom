---
name: Compatibility Report
about: Game compatibility issues.
---

BEFORE FILING AN ISSUE PLEASE CHECK IF THE ISSUE OCCURS ON UPSTREAM PROTON-EXPERIMENTAL FIRST.
IF IT HAPPENS ON PROTON-EXPERIMENTAL, YOU NEED TO FILE THE ISSUE UPSTREAM, NOT HERE.

UPSTREAM BUG TRACKER:
https://github.com/ValveSoftware/Proton/issues

THE GE-PROTON ISSUE TRACKER IS FOR ISSUES THAT ONLY OCCUR ON GE-PROTON BUT WORK PROPERLY UPSTREAM, OR FOR FEATURES THAT ARE ONLY SPECIFIC TO GE-PROTON SUCH AS WINE-WAYLAND.

# Compatibility Report
- Name of the game with compatibility issues:
- Steam AppID of the game:

## System Information
- GPU: <!-- e.g. RX 580, RX 7900 XT, RTX 4070 -->
- Driver/LLVM version: <!-- e.g. Mesa 25.2.6 / LLVM 21.0.0 or NVIDIA 575.64 -->
- Kernel version: <!-- e.g. 6.15.6 -->
- Distro version: <!-- e.g. Fedora 42, Bazzite, Arch, Nobara, Ubuntu 26.04 -->
- Desktop session: <!-- e.g. KDE Wayland, GNOME Wayland, X11 -->
- Link to full system information report as [Gist](https://gist.github.com/):
- Proton version:

## Proton comparison
<!-- Please test with a clean prefix where possible. If a game has DRM/anti-tamper limits, mention that. -->
- Works with Proton Experimental: <!-- yes/no/not tested -->
- Works with Proton Experimental bleeding-edge: <!-- yes/no/not tested -->
- Works with latest Valve Proton stable: <!-- yes/no/not tested/version -->
- Works with previous GE-Proton version: <!-- yes/no/not tested/version -->
- Last known working GE-Proton version, if any:
- First known broken GE-Proton version, if any:

## Launch options
```text
<!-- Paste the exact Steam launch options used, e.g. PROTON_LOG=1 %command% -->
```

## I confirm:
- [ ] that I have verified my problem does NOT happen on proton-experimental and ONLY happens on GE-Proton
- [ ] that I am NOT using the GE-Proton flatpak. (I do not build or provide the GE-Proton flatpak and it is known to have broken codec support.)
- [ ] that I haven't found an existing compatibility report for this game.
- [ ] that I have checked whether there are updates for my system available.
- [ ] that I have tested with a clean prefix or explained why I could not.

For issues with the GE-Proton flatpak, report here:
https://github.com/flathub/com.valvesoftware.Steam.CompatibilityTool.Proton-GE

<!-- Please add `PROTON_LOG=1 %command%` to the game's launch options and drag
and drop the generated `$HOME/steam-$APPID.log` into this issue report. -->

<!-- For media/audio/video regressions, please consider adding a focused WINEDEBUG log if requested by the maintainer, for example:
PROTON_LOG=1 WINEDEBUG="+seh,+dmo,+mfplat,+quartz,+strmbase,+mfreadwrite,+xaudio2,+mmdevapi,+dsound,+winmm,+pulse,+err" %command%
-->

## Symptoms <!-- What's the problem? -->


## Reproduction


<!--
1. You can find the Steam AppID in the URL of the shop page of the game.
   e.g. for `The Witcher 3: Wild Hunt` the AppID is `292030`.
2. You can find your driver and Linux version, as well as your graphics
   processor's name in the system information report of Steam.
3. You can retrieve a full system information report by clicking
   `Help` > `System Information` in the Steam client on your machine.
4. Please copy it to your clipboard by pressing `Ctrl+A` and then `Ctrl+C`.
   Then paste it in a [Gist](https://gist.github.com/) and post the link in
   this issue.
5. Please search for open issues and pull requests by the name of the game and
   find out whether they are relevant and should be referenced above.
-->
