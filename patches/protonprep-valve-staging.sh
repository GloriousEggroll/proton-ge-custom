#!/bin/bash

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

    pushd gstreamer
    git reset --hard HEAD
    git clean -xdf
    echo "GSTREAMER: fix for unclosable invisible wayland opengl windows in taskbar"
    patch -Np1 < ../patches/gstreamer/5509.patch
    patch -Np1 < ../patches/gstreamer/5511.patch
    popd

    pushd protonfixes
    git reset --hard HEAD
    git clean -xdf
    pushd subprojects
    pushd libmspack
    git reset --hard HEAD
    git clean -xdf
    popd
    pushd python-xlib
    git reset --hard HEAD
    git clean -xdf
    popd
    pushd umu-database
    git reset --hard HEAD
    git clean -xdf
    popd
    pushd unzip
    git reset --hard HEAD
    git clean -xdf
    popd
    pushd winetricks
    git reset --hard HEAD
    git clean -xdf
    popd
    popd
    popd

### END PREP SECTION ###

### (2) WINE PATCHING ###

    pushd wine
    git reset --hard HEAD
    git clean -xdf

### (2-1) PROBLEMATIC COMMIT REVERT SECTION ###

# Bring back configure files. Staging uses them to regenerate fresh ones
# https://github.com/ValveSoftware/wine/commit/e813ca5771658b00875924ab88d525322e50d39f

    git revert --no-commit e813ca5771658b00875924ab88d525322e50d39f

### END PROBLEMATIC COMMIT REVERT SECTION ###

### (2-2) WINE STAGING APPLY SECTION ###

    echo "WINE: -STAGING- applying staging patches"

    ../wine-staging/staging/patchinstall.py DESTDIR="." --all \
    -W winex11-_NET_ACTIVE_WINDOW \
    -W winex11-WM_WINDOWPOSCHANGING \
    -W user32-alttab-focus \
    -W winex11-MWM_Decorations \
    -W server-Signal_Thread \
    -W ntdll-Junction_Points \
    -W server-Stored_ACLs \
    -W server-File_Permissions \
    -W kernel32-CopyFileEx \
    -W shell32-Progress_Dialog \
    -W shell32-ACE_Viewer \
    -W dbghelp-Debug_Symbols \
    -W ntdll-Syscall_Emulation \
    -W eventfd_synchronization \
    -W server-PeekMessage \
    -W server-Realtime_Priority \
    -W msxml3-FreeThreadedXMLHTTP60 \
    -W ntdll-ForceBottomUpAlloc \
    -W ntdll-NtDevicePath \
    -W ntdll_reg_flush \
    -W user32-rawinput-mouse \
    -W user32-recursive-activation \
    -W d3dx11_43-D3DX11CreateTextureFromMemory \
    -W d3dx9_36-D3DXStubs \
    -W wined3d-zero-inf-shaders \
    -W ntdll-RtlQueryPackageIdentity \
    -W loader-KeyboardLayouts \
    -W ntdll-Hide_Wine_Exports \
    -W kernel32-Debugger \
    -W ntdll-ext4-case-folder \
    -W user32-FlashWindowEx \
    -W winex11-Fixed-scancodes \
    -W winex11-Window_Style \
    -W winex11-ime-check-thread-data \
    -W winex11.drv-Query_server_position \
    -W wininet-Cleanup \
    -W cryptext-CryptExtOpenCER \
    -W wineboot-ProxySettings

    # NOTE: Some patches are applied manually because they -do- apply, just not cleanly, ie with patch fuzz.
    # A detailed list of why the above patches are disabled is listed below:

    # winex11-_NET_ACTIVE_WINDOW - Causes origin to freeze
    # winex11-WM_WINDOWPOSCHANGING - Causes origin to freeze
    # user32-alttab-focus - relies on winex11-_NET_ACTIVE_WINDOW -- may be able to be added now that EA Desktop has replaced origin?
    # winex11-MWM_Decorations - not compatible with fullscreen hack
    # server-Signal_Thread - breaks steamclient for some games -- notably DBFZ
    # ntdll-Junction_Points - breaks CEG drm
    # server-Stored_ACLs - requires ntdll-Junction_Points
    # server-File_Permissions - requires ntdll-Junction_Pointsv
    # kernel32-CopyFileEx - breaks various installers
    # shell32-Progress_Dialog - relies on kernel32-CopyFileEx
    # shell32-ACE_Viewer - adds a UI tab, not needed, relies on kernel32-CopyFileEx
    # dbghelp-Debug_Symbols - Ubisoft Connect games (3/3 I had installed and could test) will crash inside pe_load_debug_info function with this enabled

    # ntdll-Syscall_Emulation - already applied
    # eventfd_synchronization - already applied
    # server-PeekMessage - already applied
    # server-Realtime_Priority - already applied
    # msxml3-FreeThreadedXMLHTTP60 - already applied
    # ntdll-ForceBottomUpAlloc - already applied
    # ntdll-NtDevicePath - already applied
    # ntdll_reg_flush - already applied
    # user32-rawinput-mouse - already applied
    # user32-recursive-activation - already applied
    # d3dx11_43-D3DX11CreateTextureFromMemory - already applied
    # d3dx9_36-D3DXStubs - already applied
    # wined3d-zero-inf-shaders - already applied
    # ntdll-RtlQueryPackageIdentity - already applied

    # applied manually:
    # ** loader-KeyboardLayouts - note -- always use and/or rebase this --  needed to prevent Overwatch huge FPS drop
    # ntdll-Hide_Wine_Exports
    # kernel32-Debugger
    # ntdll-ext4-case-folder
    # user32-FlashWindowEx
    # winex11-Fixed-scancodes
    # winex11-Window_Style
    # winex11-ime-check-thread-data
    # winex11.drv-Query_server_position
    # wininet-Cleanup

    # rebase and applied manually:
    # ** loader-KeyboardLayouts - note -- always use and/or rebase this --  needed to prevent Overwatch huge FPS drop
    # cryptext-CryptExtOpenCER
    # wineboot-ProxySettings

    # dinput-joy-mappings - disabled in favor of proton's gamepad patches -- currently also disabled in upstream staging
    # mfplat-streaming-support -- interferes with proton's mfplat -- currently also disabled in upstream staging
    # wined3d-SWVP-shaders -- interferes with proton's wined3d -- currently also disabled in upstream staging
    # wined3d-Indexed_Vertex_Blending -- interferes with proton's wined3d -- currently also disabled in upstream staging


    echo "WINE: -STAGING- loader-KeyboardLayouts manually applied"
    patch -Np1 < ../wine-staging/patches/loader-KeyboardLayouts/0001-loader-Add-Keyboard-Layouts-registry-enteries.patch
    patch -Np1 < ../wine-staging/patches/loader-KeyboardLayouts/0002-user32-Improve-GetKeyboardLayoutList.patch

    echo "WINE: -STAGING- ntdll-Hide_Wine_Exports manually applied"
    patch -Np1 < ../wine-staging/patches/ntdll-Hide_Wine_Exports/0001-ntdll-Add-support-for-hiding-wine-version-informatio.patch

    echo "WINE: -STAGING- kernel32-Debugger manually applied"
    patch -Np1 < ../wine-staging/patches/kernel32-Debugger/0001-kernel32-Always-start-debugger-on-WinSta0.patch

    echo "WINE: -STAGING- ntdll-ext4-case-folder manually applied"
    patch -Np1 < ../wine-staging/patches/ntdll-ext4-case-folder/0002-ntdll-server-Mark-drive_c-as-case-insensitive-when-c.patch

    echo "WINE: -STAGING- user32-FlashWindowEx manually applied"
    patch -Np1 < ../wine-staging/patches/user32-FlashWindowEx/0001-user32-Improve-FlashWindowEx-message-and-return-valu.patch

    echo "WINE: -STAGING- winex11-Fixed-scancodes manually applied"
    patch -Np1 < ../wine-staging/patches/winex11-Fixed-scancodes/0001-winecfg-Move-input-config-options-to-a-dedicated-tab.patch
    patch -Np1 < ../wine-staging/patches/winex11-Fixed-scancodes/0002-winex11-Always-create-the-HKCU-configuration-registr.patch
    patch -Np1 < ../wine-staging/patches/winex11-Fixed-scancodes/0003-winex11-Write-supported-keyboard-layout-list-in-regi.patch
    patch -Np1 < ../wine-staging/patches/winex11-Fixed-scancodes/0004-winecfg-Add-a-keyboard-layout-selection-config-optio.patch
    patch -Np1 < ../wine-staging/patches/winex11-Fixed-scancodes/0005-winex11-Use-the-user-configured-keyboard-layout-if-a.patch
    patch -Np1 < ../wine-staging/patches/winex11-Fixed-scancodes/0006-winecfg-Add-a-keyboard-scancode-detection-toggle-opt.patch
    patch -Np1 < ../wine-staging/patches/winex11-Fixed-scancodes/0007-winex11-Use-scancode-high-bit-to-set-KEYEVENTF_EXTEN.patch
    patch -Np1 < ../wine-staging/patches/winex11-Fixed-scancodes/0008-winex11-Support-fixed-X11-keycode-to-scancode-conver.patch
    patch -Np1 < ../wine-staging/patches/winex11-Fixed-scancodes/0009-winex11-Disable-keyboard-scancode-auto-detection-by-.patch

    echo "WINE: -STAGING- winex11-Window_Style manually applied"
    patch -Np1 < ../wine-staging/patches/winex11-Window_Style/0001-winex11-Fix-handling-of-window-attributes-for-WS_EX_.patch

    echo "WINE: -STAGING- winex11-ime-check-thread-data manually applied"
    patch -Np1 < ../wine-staging/patches/winex11-ime-check-thread-data/0001-winex11.drv-handle-missing-thread-data-in-X11DRV_get_ic.patch

    echo "WINE: -STAGING- winex11.drv-Query_server_position manually applied"
    patch -Np1 < ../wine-staging/patches/winex11.drv-Query_server_position/0001-winex11.drv-window-Query-the-X-server-for-the-actual.patch

    echo "WINE: -STAGING- wininet-Cleanup manually applied"
    patch -Np1 < ../wine-staging/patches/wininet-Cleanup/0001-wininet-tests-Add-more-tests-for-cookies.patch
    patch -Np1 < ../wine-staging/patches/wininet-Cleanup/0002-wininet-tests-Test-auth-credential-reusage-with-host.patch
    patch -Np1 < ../wine-staging/patches/wininet-Cleanup/0003-wininet-tests-Check-cookie-behaviour-when-overriding.patch
    patch -Np1 < ../wine-staging/patches/wininet-Cleanup/0004-wininet-Strip-filename-if-no-path-is-set-in-cookie.patch
    patch -Np1 < ../wine-staging/patches/wininet-Cleanup/0005-wininet-Replacing-header-fields-should-fail-if-they-.patch


    echo "WINE: -STAGING- cryptext-CryptExtOpenCER manually applied"
    patch -Np1 < ../patches/wine-hotfixes/staging/cryptext-CryptExtOpenCER/0001.patch

    echo "WINE: -STAGING- wineboot-ProxySettings manually applied"
    patch -Np1 < ../patches/wine-hotfixes/staging/wineboot-ProxySettings/0001.patch

### END WINE STAGING APPLY SECTION ###

### (2-3) GAME PATCH SECTION ###

    echo "WINE: -GAME FIXES- assetto corsa hud fix"
    patch -Np1 < ../patches/game-patches/assettocorsa-hud.patch

    echo "WINE: -GAME FIXES- add file search workaround hack for Phantasy Star Online 2 (WINE_NO_OPEN_FILE_SEARCH)"
    patch -Np1 < ../patches/game-patches/pso2_hack.patch

    echo "WINE: -GAME FIXES- add set current directory workaround for Vanguard Saga of Heroes"
    patch -Np1 < ../patches/game-patches/vgsoh.patch

    echo "WINE: -GAME FIXES- add xinput support to Dragon Age Inquisition"
    patch -Np1 < ../patches/game-patches/dai_xinput.patch

### END GAME PATCH SECTION ###

### (2-4) WINE HOTFIX/BACKPORT SECTION ###

### END WINE HOTFIX/BACKPORT SECTION ###

### (2-5) WINE PENDING UPSTREAM SECTION ###

    # https://github.com/Frogging-Family/wine-tkg-git/commit/ca0daac62037be72ae5dd7bf87c705c989eba2cb
    echo "WINE: -PENDING- unity crash hotfix"
    patch -Np1 < ../patches/wine-hotfixes/pending/unity_crash_hotfix.patch

    # https://github.com/ValveSoftware/wine/pull/205
    # https://github.com/ValveSoftware/Proton/issues/4625
    echo "WINE: -PENDING- Add WINE_DISABLE_SFN option. (Yakuza 5 cutscenes fix)"
    patch -Np1 < ../patches/wine-hotfixes/pending/ntdll_add_wine_disable_sfn.patch

    echo "WINE: -PENDING- ncrypt: NCryptDecrypt implementation (PSN Login for Ghost of Tsushima)"
    patch -Np1 < ../patches/wine-hotfixes/pending/NCryptDecrypt_implementation.patch

    # https://gitlab.winehq.org/wine/wine/-/merge_requests/7032
    # https://bugs.winehq.org/show_bug.cgi?id=56259
    # https://forum.winehq.org/viewtopic.php?t=38443
    echo "WINE: -PENDING- add webview2 patches for GIRLS' FRONTLINE 2: EXILIUM"
    patch -Np1 < ../patches/wine-hotfixes/pending/webview2.patch

    # https://github.com/ValveSoftware/wine/pull/269
    echo "WINE: -PENDING- Hid multi TLC and Fanatec wheel-bases hidraw white-list"
    patch -Np1 < ../patches/wine-hotfixes/pending/hid-multi-tlc-and-ftec-hidraw-269.patch

### END WINE PENDING UPSTREAM SECTION ###


### (2-6) PROTON-GE ADDITIONAL CUSTOM PATCHES ###

    echo "WINE: -FSR- fullscreen hack fsr patch"
    patch -Np1 < ../patches/proton/0001-fshack-Implement-AMD-FSR-upscaler-for-fullscreen-hac.patch

    echo "WINE: -Nvidia Reflex- Support VK_NV_low_latency2"
    patch -Np1 < ../patches/proton/83-nv_low_latency_wine.patch

    echo "WINE: -CUSTOM- Add nls to tools"
    patch -Np1 < ../patches/proton/build_failure_prevention-add-nls.patch

    echo "WINE: -CUSTOM Add options to disable proton media converter."
    patch -Np1 < ../patches/proton/add-envvar-to-gate-media-converter.patch

    echo "WINE: -CUSTOM- Downgrade MESSAGE to TRACE to remove write_watches spam"
    patch -Np1 < ../patches/proton/0001-ntdll-Downgrade-using-kernel-write-watches-from-MESS.patch

    echo "WINE: -CUSTOM- Add WINE_NO_WM_DECORATION option to disable window decorations so that borders behave properly"
    patch -Np1 < ../patches/proton/0001-win32u-add-env-switch-to-disable-wm-decorations.patch

    # https://gitlab.winehq.org/wine/wine/-/merge_requests/7238
    echo "WINE: -CUSTOM- Add enhanced dualsense patches"
    patch -Np1 < ../patches/proton/dualsense/0001-mmdevapi-correctly-read-and-write-containerid-as-cls.patch
    patch -Np1 < ../patches/proton/dualsense/0002-containerid-helper-to-generate-a-containerid-from-a-.patch
    patch -Np1 < ../patches/proton/dualsense/0003-Implement-SetupDiGetDeviceInterfacePropertyW-for-DEV.patch

    echo "WINE: -CUSTOM- add gstreamer surfaceless option"
    patch -Np1 < ../patches/proton/0001-use-surfaceless-for-GST.patch

    echo "WINE: -CUSTOM- fixup for enable/disable hidraw"
    patch -Np1 < ../patches/proton/1c2f561d80d20fa770c5fd170b9100c94293792d.patch

    echo "WINE: -CUSTOM- wine wayland"
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0001-winewayland-Enable-disable-the-zwp_text_input_v3-obj.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0002-winewayland-Post-IME-update-for-committed-text.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0003-winewayland-Implement-SetIMECompositionRect.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0004-winewayland-Post-IME-update-for-preedit-text.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0005-winewayland-Round-the-Wayland-refresh-rate-to-calcul.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0006-winewayland-Implement-zwlr_data_control_device_v1-in.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0007-winewayland-Support-copying-text-from-win32-clipboar.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0008-winewayland-Generalize-support-for-exporting-clipboa.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0009-winewayland-Support-exporting-various-clipboard-form.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0010-winewayland-Support-copying-data-from-native-clipboa.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0011-winewayland-Normalize-received-MIME-type-strings.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0012-winewayland-Update-locked-pointer-position-hint.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0013-winewayland-Implement-SetCursorPos-via-pointer-lock.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0014-winewayland-Don-t-crash-on-text-input-done-events-wi.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0015-winewayland-Present-EGL-surfaces-opaquely.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0016-winewayland-Treat-fully-transparent-cursors-as-hidde.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0017-win32u-Return-0-from-NtUserGetKeyNameText-if-there-i.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0018-winewayland-Implement-wl_data_device-initialization.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0019-winewayland-Support-wl_data_device-for-copies-from-w.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0020-winewayland-Support-wl_data_device-for-copies-from-n.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0021-winewayland-Warn-about-missing-clipboard-functionali.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0022-winewayland.drv-Implement-support-for-xdg-toplevel-i.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0023-winewayland-Implement-relative-motion-accumulator.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0024-winewayland-Require-wl_pointer-for-pointer-constrain.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0025-support-older-EGL-headers.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0026-winewayland-Always-check-the-role-to-determine-wheth.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0027-winewayland-Introduce-helper-to-check-whether-a-surf.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0028-hack-winewayland-handle-fractional-scaling.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0029-cursor-shape-v1.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0030-Avoid-long-types-on-the-Unix-side.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0031-Create-.gitignore.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0032-winewayland-ensure-egl_display-is-not-EGL_BAD_PARAME.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0033-Add-amdxc-implementation.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0034-wine.inf-Don-t-clobber-UBR-key.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0035-twinapi.appcore-tests-Fix-broken-registry-query.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0036-winecfg-Add-support-for-UBR-key.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0037-winewayland-Don-t-use-a-destroyed-surface-in-text-in.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0038-hack-win32u-fix-opengl-applications-on-winewayland.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0039-winewayland-register-swap-control-tear.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0040-check-if-FSR4_UPGRADE-env-is-nonzero.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0041-fixup-winewayland-handle-locking-with-fractional-sca.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0042-winewayland-Use-ARGB-buffers-for-shaped-windows.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0043-winewayland-Implement-window-surface-shape-and-color.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0044-minor-formatting-fix.patch

    # Fix orientation on steam deck
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/cb400427c7fd6c80664f11b7bbaf128fc46fcd59.patch

    # Game/launcher wayland fixes
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/cursor_and_launchers/0001-ntdll-remove-outdated-workaround-for-rainbow-six-ext.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/cursor_and_launchers/0002-HACK-kernelbase-Add-Wayland-specific-workarounds.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/cursor_and_launchers/0003-fixup-kernelbase-Check-for-wayland-hacks-first.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/cursor_and_launchers/0004-formatting-fix.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/cursor_and_launchers/0005-kernelbase-Enable-in-process-gpu-for-battle.net.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/cursor_and_launchers/0006-kernelbase-Add-in-process-gpu-for-RSI-launcher.patch

    # Touchpad wayland fix
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/cursor_and_launchers/0007-winewayland-Implement-touchpad-scrolling-support.patch

    # Un-accelerated mouse input (rawinput)
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/cursor_and_launchers/0008-winewayland-Use-unaccelerated-relative-motion.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/cursor_and_launchers/0009-winewayland-Use-discrete-event-when-possible.patch

    # Fix for joystick without axis
    patch -Np1 < ../patches/wine-hotfixes/pending/winebus-joystick-no-axis-detection.patch
    popd

### END PROTON-GE ADDITIONAL CUSTOM PATCHES ###
### END WINE PATCHING ###
