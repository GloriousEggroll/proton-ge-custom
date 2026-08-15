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
    echo "PROTONFIXES: add optiscaler support"
    apply_all_in_dir "../patches/protonfixes/"
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

    git checkout -- \
        lsteamclient/Makefile.in \
        lsteamclient/gen_wrapper.py \
        lsteamclient/steam_input_manual.c \
        lsteamclient/steamclient_private.h \
        lsteamclient/winISteamInput.c

    echo "LSTEAMCLIENT: add XInput-backed Steam Input fallback"
    apply_all_in_dir "patches/lsteamclient"

### (2) WINE PATCHING ###

    pushd wine
    git reset --hard HEAD
    git clean -xdf

### (2-1) PROBLEMATIC COMMIT REVERT SECTION ###

# Bring back configure files. Staging uses them to regenerate fresh ones
# https://github.com/ValveSoftware/wine/commit/e813ca5771658b00875924ab88d525322e50d39f

    git revert --no-commit e813ca5771658b00875924ab88d525322e50d39f

### END PROBLEMATIC COMMIT REVERT SECTION ###

### (2-2) EM-10/WINE-WAYLAND PATCH SECTION ###

    echo "WINE: -WINEOPENXR- copy files into wine"
    mkdir -p dlls/wineopenxr
    cp -R ../wineopenxr/* dlls/wineopenxr/

    echo "WINE: -CUSTOM- ETAASH WINE-WAYLAND+ PATCHES"
   apply_all_in_dir "../patches/wine-hotfixes/wine-wayland/"

    echo "WINE: -CUSTOM- ETAASH WINE-WAYLAND+ FIXUPS"
    apply_all_in_dir "../patches/wine-hotfixes/em-fixups/"

### END EM-10/WINE-WAYLAND PATCH SECTION ###

### (2-3) WINE STAGING APPLY SECTION ###

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
    -W wintrust-WTHelperGetProvCertFromChain \
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
    # wintrust-WTHelperGetProvCertFromChain - already applied by Wine upstream

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

### (2-4) GAME PATCH SECTION ###

### END GAME PATCH SECTION ###

### (2-5) WINE HOTFIX/BACKPORT SECTION ###

    echo "WINE: -PENDING- add OpenXR patches"
    apply_patch "../patches/wine-hotfixes/pending/0001-decouple-wineopenxr-from-steamvr-and-integrate-it-in.patch"

### END WINE HOTFIX/BACKPORT SECTION ###

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
