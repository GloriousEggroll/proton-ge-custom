#!/bin/bash

# patch functions
apply_patch() {
    local patch_path="$1"
    patch -Np1 < "$patch_path"
}

apply_all_in_dir() {
    local dir="$1"
    for patch in "$dir"/*.patch; do
        apply_patch "$patch"
    done
}

### (1) PREP SECTION ###

    pushd dxvk
    git reset --hard HEAD
    git clean -xdf
    patch -Np1 < ../patches/dxvk/layered-overlay-dxvk.patch
    popd

    pushd vkd3d-proton
    git reset --hard HEAD
    git clean -xdf
    popd

    pushd dxvk-nvapi
    git reset --hard HEAD
    git clean -xdf
    popd

    pushd protonfixes
    git reset --hard HEAD
    git clean -xdf
    popd

    pushd wineopenxr
    git checkout .
    git clean -xdf
    echo "WINEOPENXR: patch wineopenxr so it can be built as part of wine"
    apply_all_in_dir "../patches/wineopenxr/"
    popd

### END PREP SECTION ###

    git checkout steam_helper
    git checkout umu_helper

    echo "DISCORD: -DISCORD RPC BRIDGE- patch steam/umu helpers"
    apply_all_in_dir "patches/discordrpc/helpers"

### (2) WINE PATCHING ###

    pushd wine
    git reset --hard HEAD
    git clean -xdf

### (2-1) PROBLEMATIC COMMIT REVERT SECTION ###

# Bring back configure files. Staging uses them to regenerate fresh ones
# https://github.com/ValveSoftware/wine/commit/e813ca5771658b00875924ab88d525322e50d39f

    git revert --no-commit e813ca5771658b00875924ab88d525322e50d39f

# This breaks our PS controller patches

    git revert --no-commit f51244bab0701b292ab668bad3aeb1cdcd6ef84b

### END PROBLEMATIC COMMIT REVERT SECTION ###

### (2-2) EM-10/WINE-WAYLAND PATCH SECTION ###

    echo "WINE: -WINEOPENXR- copy files into wine"
    mkdir -p dlls/wineopenxr
    cp -R ../wineopenxr/* dlls/wineopenxr/

    echo "WINE: -CUSTOM- ETAASH WINE-WAYLAND+ PATCHES"
   apply_all_in_dir "../patches/wine-hotfixes/wine-wayland/"

### END EM-10/WINE-WAYLAND PATCH SECTION ###

### (2-3) WINE STAGING APPLY SECTION ###

    echo "WINE: -STAGING- applying staging patches"

    ../wine-staging/staging/patchinstall.py DESTDIR="." --all --no-autoconf\
    -W server-Signal_Thread \
    -W server-Stored_ACLs \
    -W server-File_Permissions \
    -W kernel32-CopyFileEx \
    -W dbghelp-Debug_Symbols \
    -W version-VerQueryValue \
    -W mf_http_support \
    -W server-PeekMessage \
    -W msxml3-FreeThreadedXMLHTTP60 \
    -W ntdll-ForceBottomUpAlloc \
    -W ntdll-NtDevicePath \
    -W user32-rawinput-mouse \
    -W user32-recursive-activation \
    -W d3dx9_36-D3DXStubs \
    -W wined3d-zero-inf-shaders \
    -W ntdll-RtlQueryPackageIdentity \
    -W vkd3d-latest \
    -W loader-KeyboardLayouts \
    -W ntdll-Syscall_Emulation \
    -W ntdll_reg_flush \
    -W ntdll-Hide_Wine_Exports \
    -W kernel32-Debugger \
    -W ntdll-ext4-case-folder \
    -W winex11-Window_Style \
    -W wininet-Cleanup \
    -W winex11-ime-check-thread-data \
    -W winex11-Fixed-scancodes \
    -W Staging

    # NOTE: Some patches are applied manually because they -do- apply, just not cleanly, ie with patch fuzz.
    # A detailed list of why the above patches are disabled is listed below:

    # server-Signal_Thread - breaks steamclient for some games -- notably DBFZ
    # server-Stored_ACLs - requires ntdll-Junction_Points
    # server-File_Permissions - requires ntdll-Junction_Pointsv
    # kernel32-CopyFileEx - breaks various installers
    # dbghelp-Debug_Symbols - Ubisoft Connect games (3/3 I had installed and could test) will crash inside pe_load_debug_info function with this enabled
    # version-VerQueryValue - just a test and doesn't apply cleanly. not relevant for gaming
    # mf_http_support - disabled in favor of custom ffmpeg backend video playback solution

    # server-PeekMessage - already applied
    # msxml3-FreeThreadedXMLHTTP60 - already applied
    # ntdll-ForceBottomUpAlloc - already applied
    # ntdll-NtDevicePath - already applied
    # user32-rawinput-mouse - already applied
    # user32-recursive-activation - already applied
    # d3dx9_36-D3DXStubs - already applied
    # wined3d-zero-inf-shaders - already applied
    # ntdll-RtlQueryPackageIdentity - already applied
    # vkd3d-latest - already applied
    # loader-KeyboardLayouts - already applied
    # ntdll-Syscall_Emulation - already applied
    # ntdll_reg_flush - already applied

    # ntdll-Hide_Wine_Exports - applied manually
    # kernel32-Debugger - applied manually
    # ntdll-ext4-case-folder - applied manually
    # winex11-Window_Style - applied manually
    # wininet-Cleanup - applied manually
    # Staging - applied manually
    # winex11-ime-check-thread-data - applied manually, needed rebase
    # winex11-Fixed-scancodes - applied manually, needed rebase

    # winex11-WM_WINDOWPOSCHANGING - Causes origin to freeze -- currently also disabled in upstream staging
    # ntdll-Junction_Points - breaks CEG drm -- currently also disabled in upstream staging
    # shell32-Progress_Dialog - relies on kernel32-CopyFileEx -- currently also disabled in upstream staging
    # shell32-ACE_Viewer - adds a UI tab, not needed, relies on kernel32-CopyFileEx -- currently also disabled in upstream staging
    # dinput-joy-mappings - disabled in favor of proton's gamepad patches -- currently also disabled in upstream staging
    # mfplat-streaming-support -- interferes with proton's mfplat -- currently also disabled in upstream staging
    # wined3d-SWVP-shaders -- interferes with proton's wined3d -- currently also disabled in upstream staging
    # wined3d-Indexed_Vertex_Blending -- interferes with proton's wined3d -- currently also disabled in upstream staging

    echo "WINE: -STAGING- ntdll-Hide_Wine_Exports manually applied"
    apply_all_in_dir "../wine-staging/patches/ntdll-Hide_Wine_Exports/"

    echo "WINE: -STAGING- kernel32-Debugger manually applied"
    apply_all_in_dir "../wine-staging/patches/kernel32-Debugger/"

    echo "WINE: -STAGING- ntdll-ext4-case-folder manually applied"
    apply_all_in_dir "../wine-staging/patches/ntdll-ext4-case-folder/"

    echo "WINE: -STAGING- winex11-Window_Style manually applied"
    apply_all_in_dir "../wine-staging/patches/winex11-Window_Style/"

    echo "WINE: -STAGING- wininet-Cleanup manually applied"
    apply_all_in_dir "../wine-staging/patches/wininet-Cleanup/"

    echo "WINE: -STAGING- Staging manually applied"
    apply_all_in_dir "../wine-staging/patches/Staging/"

    echo "WINE: -STAGING- winex11-ime-check-thread-data manually applied"
    apply_all_in_dir "../patches/wine-hotfixes/wine-staging/winex11-ime-check-thread-data/"

    echo "WINE: -STAGING- winex11-Fixed-scancodes manually applied"
    apply_all_in_dir "../patches/wine-hotfixes/wine-staging/winex11-Fixed-scancodes/"

    echo "WINE: -STAGING- comctl32_animate_avi cleanup -Werror"
    apply_all_in_dir "../patches/wine-hotfixes/wine-staging/comctl32_animate_avi/"

    echo "WINE: -STAGING- d3drm-starwars cleanup -Werror"
    apply_all_in_dir "../patches/wine-hotfixes/wine-staging/d3drm-starwars/"

    echo "WINE: -STAGING- windowscodecs-TIFF_Support cleanup -Werror"
    apply_all_in_dir "../patches/wine-hotfixes/wine-staging/windowscodecs-TIFF_Support/"

    echo "WINE: -STAGING- mmsystem.dll16-MIDIHDR_Refcount cleanup -Werror"
    apply_all_in_dir "../patches/wine-hotfixes/wine-staging/mmsystem.dll16-MIDIHDR_Refcount/"


### END WINE STAGING APPLY SECTION ###

### (2-4) GAME PATCH SECTION ###

    echo "WINE: -GAME FIXES- assetto corsa hud fix"
    apply_patch "../patches/game-patches/assettocorsa-hud.patch"

    echo "WINE: -GAME FIXES- add file search workaround hack for Phantasy Star Online 2 (WINE_NO_OPEN_FILE_SEARCH)"
    apply_patch "../patches/game-patches/pso2_hack.patch"

    echo "WINE: -GAME FIXES- add set current directory workaround for Vanguard Saga of Heroes"
    apply_patch "../patches/game-patches/vgsoh.patch"

    echo "WINE: -GAME FIXES- add xinput support to Dragon Age Inquisition"
    apply_patch "../patches/game-patches/dai_xinput.patch"

    echo "WINE: -GAME FIXES- add fixes for star citizen"
    apply_patch "../patches/game-patches/silence-starcitizen-unsupported-os.patch"
    apply_patch "../patches/game-patches/eac_60101_timeout.patch"

    echo "WINE: -GAME FIXES- add TBH: Task Bar Hero fixes"
    apply_patch "../patches/game-patches/layered-overlay-wine.patch"

    echo "WINE: -GAME FIXES- force Battle.net Launcher in-process GPU on winewayland"
    apply_patch "../patches/game-patches/battlenet-launcher-in-process-gpu.patch"

    echo "WINE: -GAME FIXES- add fixes Guilty Gear Accent Core Plus R intro video (win32u related)"
    apply_patch "../patches/game-patches/0001-win32u-Avoid-zero-WM_ACTIVATEAPP-lparam-on-first-for.patch"

    echo "WINE: -GAME FIXES- make MapleStory launch: avoid NULL deref in CharPrevA/CharPrevExA"
    apply_patch "../patches/game-patches/maplestory-kernelbase-charprev-null.patch"

    echo "WINE: -GAME FIXES- make MapleStory launch: accept SPI_SETSTICKYKEYS/SPI_SETFILTERKEYS"
    apply_patch "../patches/game-patches/maplestory-spi-stickykeys-filterkeys.patch"

### END GAME PATCH SECTION ###

### (2-5) WINE HOTFIX/BACKPORT SECTION ###
    echo "WINE: -HOTFIX- Fix Smart Tee negotiation and V4L WoW64 media type marshaling"
    apply_all_in_dir "../patches/wine-hotfixes/qcap-dshow-fixes/"

    echo "WINE: -HOTFIX- Add GetFileVersionInfoByHandle version export stub"
    apply_patch "../patches/wine-hotfixes/pending/version-GetFileVersionInfoByHandle-stub.patch"

    echo "WINE: -HOTFIX- Validate Winsock connect address arguments"
    apply_patch "../patches/wine-hotfixes/pending/ws2_32-validate-connect-address.patch"

    echo "WINE: -HOTFIX- Fall back when GnuTLS lacks NO_SHUFFLE_EXTENSIONS"
    apply_patch "../patches/wine-hotfixes/pending/secur32-fallback-without-no-shuffle-extensions.patch"

### END WINE HOTFIX/BACKPORT SECTION ###

### (2-6) WINE PENDING UPSTREAM SECTION ###

    # https://github.com/Frogging-Family/wine-tkg-git/commit/ca0daac62037be72ae5dd7bf87c705c989eba2cb
    echo "WINE: -PENDING- unity crash hotfix"
    apply_patch "../patches/wine-hotfixes/pending/unity_crash_hotfix.patch"

    # https://bugs.winehq.org/show_bug.cgi?id=58476
    echo "WINE: -PENDING- RegGetValueW dwFlags hotfix (R.E.A.L VR mod)"
    apply_patch "../patches/wine-hotfixes/pending/registry_RRF_RT_REG_SZ-RRF_RT_REG_EXPAND_SZ.patch"

    echo "WINE: -PENDING- ncrypt: NCryptDecrypt implementation (PSN Login for Ghost of Tsushima)"
    apply_patch "../patches/wine-hotfixes/pending/NCryptDecrypt_implementation.patch"

    # https://github.com/GloriousEggroll/proton-ge-custom/issues/433
    echo "WINE: -PENDING- add Duet Knight Abyss fixes"
    apply_patch "../patches/wine-hotfixes/pending/0009-HACK-kernel32-Spoof-GetProcAddress-of-KiUserApcDispa.patch"

    # Import upstream icuu forwarders patches to fix broken GoW2Hollow_Setup.exe for Gears of War 2 Hollow
    echo "WINE: -PENDING-  Import upstream icuu forwarders patches to fix broken GoW2Hollow_Setup.exe for Gears of War 2 Hollow"
    apply_patch "../patches/wine-hotfixes/pending/icuuc-icuin-forwarder-dlls.patch"


    # Separate OpenXR steam reliance
    # https://github.com/GloriousEggroll/proton-ge-custom/issues/214
    echo "WINE: -PENDING- add OpenXR patches"
    apply_patch "../patches/wine-hotfixes/pending/0001-decouple-wineopenxr-from-steamvr-and-integrate-it-in.patch"

    echo "WINE: -CUSTOM- Dynamically relocate .exes, improving compatibility with modding / hooking tools"
    apply_patch "../patches/wine-hotfixes/pending/0001-server-Dynamically-relocate-.exes-by-default-too.patch"

### END WINE PENDING UPSTREAM SECTION ###


### (2-7) PROTON-GE ADDITIONAL CUSTOM PATCHES ###

    echo "WINE: Add an env variable to override channel count in winealsa"
    apply_patch "../patches/proton/winealsa-override-channel-count.patch"

    echo "WINE: -FSR- fullscreen hack fsr patch"
    apply_patch "../patches/proton/0001-fshack-Implement-AMD-FSR-upscaler-for-fullscreen-hac.patch"

    echo "WINE: -Nvidia Reflex- Support VK_NV_low_latency2"
    apply_patch "../patches/proton/83-nv_low_latency_wine.patch"

    echo "WINE: -CUSTOM- Add nls to tools"
    apply_patch "../patches/proton/build_failure_prevention-add-nls.patch"

    echo "WINE: -CUSTOM- Add WINE_NO_WM_DECORATION option to disable window decorations so that borders behave properly"
    apply_patch "../patches/proton/0001-win32u-add-env-switch-to-disable-wm-decorations.patch"

    # https://steamcommunity.com/app/2074920/discussions/0/604168604057160448/
    echo "WINE: --CUSTOM-- add WINE_HOSTBLOCK envvar to allow working around some problematic anticheats (notably eac)"
    apply_patch "../patches/proton/wine_host_block_envvar.patch"

    echo "WINE: mutter -> cinnamon detection patch for winex11"
    apply_patch "../patches/proton/winex11-mutter-cinnamon.patch"

    echo "WINE: add optiscaler patch"
    apply_patch "../patches/proton/0001-HACK-kernelbase-allow-overriding-dlls-for-DLSS-XeSS-.patch"

    echo "WINE: -HOTFIX- Implement GE-Proton ffmpeg + winedmo only video playback rework patches"
    apply_all_in_dir "../patches/ge-video-rework/"

    # https://github.com/xzn/proton-ds5-haptic
    echo "WINE: -HOTFIX- Add proton DS5 patches"
    for patch in ../patches/proton-ds5-haptic/*.patch; do
        apply_patch "$patch"
    done

    echo "WINE: RUN AUTOCONF TOOLS/MAKE_REQUESTS"
    autoreconf -f
    ./tools/make_requests

    popd



### END PROTON-GE ADDITIONAL CUSTOM PATCHES ###
### END WINE PATCHING ###
