#!/bin/bash

### (1) PREP SECTION ###

    pushd dxvk
    git reset --hard HEAD
    git clean -xdf
    popd

    pushd vkd3d-proton
    git reset --hard HEAD
    git clean -xdf
    patch -Np1 < ../patches/vkd3d/vkd3d-proton-0001-vkd3d-Load-amdxc64-when-using-AMD.patch
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
    -W wineboot-ProxySettings \
    -W version-VerQueryValue \
    -W setupapi-DiskSpaceList

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
    # version-VerQueryValue - just a test and doesn't apply cleanly. not relevant for gaming

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
    # setupapi-DiskSpaceList -- upstream commits were brought in for dualsense fixes, the staging patches are no longer needed


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

    echo "WINE: -CUSTOM- Fix a crash in ID2D1DeviceContext if no target is set"
    patch -Np1 < ../patches/proton/fix-a-crash-in-ID2D1DeviceContext-if-no-target-is-set.patch

    echo "WINE: -CUSTOM- wine wayland"
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0001-use-surfaceless-for-GST.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0002-winewayland-Enable-disable-the-zwp_text_input_v3-obj.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0003-winewayland-Post-IME-update-for-committed-text.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0004-winewayland-Implement-SetIMECompositionRect.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0005-winewayland-Post-IME-update-for-preedit-text.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0006-winewayland-Round-the-Wayland-refresh-rate-to-calcul.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0007-winewayland-Implement-zwlr_data_control_device_v1-in.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0008-winewayland-Support-copying-text-from-win32-clipboar.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0009-winewayland-Generalize-support-for-exporting-clipboa.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0010-winewayland-Support-exporting-various-clipboard-form.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0011-winewayland-Support-copying-data-from-native-clipboa.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0012-winewayland-Normalize-received-MIME-type-strings.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0013-winewayland-Update-locked-pointer-position-hint.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0014-winewayland-Implement-SetCursorPos-via-pointer-lock.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0015-winewayland-Don-t-crash-on-text-input-done-events-wi.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0016-winewayland-Present-EGL-surfaces-opaquely.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0017-winewayland-Treat-fully-transparent-cursors-as-hidde.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0018-win32u-Return-0-from-NtUserGetKeyNameText-if-there-i.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0019-winewayland-Implement-wl_data_device-initialization.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0020-winewayland-Support-wl_data_device-for-copies-from-w.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0021-winewayland-Support-wl_data_device-for-copies-from-n.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0022-winewayland-Warn-about-missing-clipboard-functionali.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0023-winewayland.drv-Implement-support-for-xdg-toplevel-i.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0024-winewayland-Implement-relative-motion-accumulator.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0025-winewayland-Require-wl_pointer-for-pointer-constrain.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0026-support-older-EGL-headers.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0027-winewayland-Always-check-the-role-to-determine-wheth.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0028-winewayland-Introduce-helper-to-check-whether-a-surf.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0029-hack-winewayland-handle-fractional-scaling.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0030-cursor-shape-v1.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0031-Avoid-long-types-on-the-Unix-side.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0032-Create-.gitignore.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0033-winewayland-ensure-egl_display-is-not-EGL_BAD_PARAME.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0034-Add-amdxc-implementation.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0035-wine.inf-Don-t-clobber-UBR-key.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0036-twinapi.appcore-tests-Fix-broken-registry-query.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0037-winecfg-Add-support-for-UBR-key.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0038-winewayland-Don-t-use-a-destroyed-surface-in-text-in.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0039-hack-win32u-fix-opengl-applications-on-winewayland.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0040-winewayland-register-swap-control-tear.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0041-check-if-FSR4_UPGRADE-env-is-nonzero.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0042-fixup-winewayland-handle-locking-with-fractional-sca.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0043-winewayland-Use-ARGB-buffers-for-shaped-windows.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0044-winewayland-Implement-window-surface-shape-and-color.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0045-minor-formatting-fix.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0046-ntdll-remove-outdated-workaround-for-rainbow-six-ext.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0047-HACK-kernelbase-Add-Wayland-specific-workarounds.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0048-fixup-kernelbase-Check-for-wayland-hacks-first.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0049-formatting-fix.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0050-kernelbase-Enable-in-process-gpu-for-battle.net.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0051-kernelbase-Add-in-process-gpu-for-RSI-launcher.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0052-winewayland-Implement-touchpad-scrolling-support.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0053-winewayland-Use-unaccelerated-relative-motion.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0054-winewayland-Use-discrete-event-when-possible.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0055-winewayland-Handle-rotated-screens.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0056-fixup-winewayland-Handle-rotated-screens.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0057-winewayland-make-wp_fractional_scale_handle_scale-st.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0058-winewayland-Place-fullscreen-windows-on-associated-o.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0059-HACK-ntdll-Ignore-SDL_VIDEODRIVER-env.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0060-fixup-winewayland-Place-fullscreen-windows-on-associ.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0061-HACK-winewayland-Support-output-switching.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0062-WIP-winewayland-emulate-some-edid-data.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0063-winedmo-Fix-double-free.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0064-winewayland-Set-edid-chromaticity-values.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0065-fixup-winewayland-Support-output-switching.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0066-HACK-winewayland-Add-option-to-offset-monitor-positi.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0067-winewayland-Add-error-message-when-wp_color_manager_.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0068-amdxc-Only-load-amdxcffx-when-FSR4-upgrade-is-enable.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0069-winebus-Ignore-steam-input-virtual-controller-when-P.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0070-winewayland-Refactor-output-selection.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0071-winebus-PROTON_PREFER_SDL-PROTON_USE_SDL.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0072-HACK-winewayland-Send-relative-event-with-absolute-e.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0073-winewayland-Update-min-max-window-based-on-window-re.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0074-winewayland-add-opcode-3-of-zwlr_data_control_device.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0075-winewayland-systray-skeleton.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0076-fixup-HACK-winewayland-Send-relative-event-with-abso.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0077-winewayland-ensure-flush-when-setting-fullscreen-sta.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0078-winewayland-Don-t-scale-raw-input-values.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0079-HACK-winewayland-add-support-for-picking-primary-mon.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0080-fixup-winewayland-emulate-edid-data.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0081-winewayland-round-relative-pointer-timestamp.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0082-winebus-Allow-disabling-steam-input-seperately.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0083-winegstreamer-disable-media-converter.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0084-winegstreamer-Add-env-to-enable-media-converter.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0085-winewayland-Send-no-raw-with-normal-pointer-events.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0086-winewayland-Allow-disabling-rawinput-through-env.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0087-fixup-winebus-Allow-disabling-steam-input-seperately.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0088-fixup-HACK-winewayland-add-support-for-picking-prima.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0089-fixup-winewayland-add-missing-locking-to-pointer-upd.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0090-winewayland-Rewrite-wayland-pointer-implementation.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0091-winewayland-Always-use-rawinput.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0092-winewayland-immediately-send-input-when-entering.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0093-winewayland-Implement-xdg-system-bell-v1.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0094-winewayland-Add-support-for-xdg-activation-v1.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0095-fixup-winewayland-Rewrite-wayland-pointer-implementa.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0096-winewayland-Implement-HasWindowManager-fully.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0097-HACK-winewayland-confine-to-parent-surface-on-kwin.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0098-amdxc-minor-code-cleanups.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0099-winewayland-small-cleanups.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0100-winewayland-Set-process-name-for-xdg-toplevel-icon.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0101-Import-WIP-atiadlxx-and-atidxx-implementations.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0102-wine.inf-Add-override-for-diabotical.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0103-winewayland-Implement-support-for-HDR-static-metadat.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0104-atiadlxx-Remove-outdated-comment.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0105-kernel32-Implement-timeGetTime.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0106-kernelbase-Implement-HeapSummary.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0107-ntdll-Add-a-stub-for-NtCreateSectionEx.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0108-kernelbase-Implement-CreateFileMapping2.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0109-winewayland-Don-t-drop-pointer-frames-in-relative-on.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0110-winewayland-Prefer-using-target-primaries.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0111-winewayland-implement-support-for-color-management-d.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0112-winebus-remove-duplicate-code.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0113-opengl32-Improve-wow64-mapping-performance-by-20x.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0114-HACK-opengl32-Reuse-allocated-memory.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0115-fixup-opengl32-Support-map-buffer-offsets.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0116-winewayland-Use-an-empty-string-to-clear-the-composi.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0117-win32u-Preserve-result-string-from-multiple-WINE_IME.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0118-win32u-Support-WM_IME_KEYDOWN-message-during-ImeProc.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0119-win32u-Add-more-CompAttr-CompClause-implementation-u.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0120-winewayland-Extend-cursor_pos-using-cursor_begin-cur.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0121-winemac-Extend-cursor_pos-using-cursor_begin-cursor_.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0122-winex11-Extend-cursor_pos-using-cursor_begin-cursor_.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0123-winex11-Update-only-when-caret-pos-changed-in-xic_pr.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0124-winewayland-Improve-cleanup-of-text-input-pending-st.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0125-winewayland-Ignore-text-input-done-events-that-don-t.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0126-imm32-Fix-the-WM_IME_COMPOSITION-messages-to-be-betw.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0127-opengl32-Use-VirtualAlloc-instead-of-NtAllocateVirtu.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0128-setupapi-Use-wide-character-string-literals.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0129-ntoskrnl.exe-tests-Improve-device-properties-test-av.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0130-ntoskrnl.exe-tests-Test-some-Io-functions-with-FDO-a.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0131-ntoskrnl.exe-tests-Test-that-calling-some-Io-functio.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0132-ntoskrnl.exe-Fix-getting-DevicePropertyEnumeratorNam.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0133-Revert-setupapi-Don-t-use-NULL-as-key-value-in-get_d.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0134-setupapi-tests-Add-tests-for-SetupDi-Set-Get-DeviceI.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0135-setupapi-Implement-SetupDiSetDeviceInterfaceProperty.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0136-setupapi-Implement-SetupDiGetDeviceInterfaceProperty.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0137-ntoskrnl.exe-test-Add-tests-for-SetupDiGetDeviceInte.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0138-setupapi-Don-t-use-NULL-as-key-value-in-get_device_r.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0139-setupapi-Implement-DEVPKEY_Device_InstanceId-in-Setu.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0140-ntdll-tests-Add-tests-for-ret_len-on-NtQueryInformat.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0141-ntdll-Return-STATUS_ACCESS_VIOLATION-from-NtQueryInf.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0142-kernelbase-Allocate-a-new-buffer-for-the-module-name.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0143-ntdll-Also-trap-syscalls-in-the-top-down-reserved-ar.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0144-winebus-Fix-PROTON_ENABLE-DISABLE_HIDRAW.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0145-mmdevapi-correctly-read-and-write-containerid-as-cls.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0146-containerid-helper-to-generate-a-containerid-from-a-.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0147-ntoskrnl.exe-Implement-KeAcquireGuardedMutex.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0148-ntoskrnl.exe-Implement-KeReleaseGuardedMutex.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0149-tdh-Add-stub-for-TdhEnumerateProviders.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0154-winewayland-Use-SEND_HWMSG_NO_RAW-for-keyboard.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0155-winewayland-Add-more-logging-for-keyboard.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0156-fixup-Revert-winewayland-Use-SEND_HWMSG_NO_RAW-for-k.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0159-amdxc-Add-support-for-anti-lag-2.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0160-amdxc-code-cleanups.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0161-winewayland-Add-error-message-for-cross-process-rend.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0162-HACK-win32u-Place-windows-on-the-vscreen.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0163-HACK-winewayland-Always-create-a-new-client-surface-.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/0164-fixup-flickering.patch
    patch -Np1 < ../patches/wine-hotfixes/pending/wine-wayland/ac97b117bd8fb3bd6b08d48d8808471189c1e9d5.patch

    echo "WINE: -CUSTOM- General fixes to help meet certain Anti-cheat engines' requirements"
    # https://gitlab.winehq.org/wine/wine-staging/-/commit/d88d44f1d9d94cb11aff9e2f0ce37d0d67fe1e95
    patch -Np1 < ../patches/wine-hotfixes/staging/trap-syscalls-in-reserved-area/0001-ntdll-Also-trap-syscalls-in-the-top-down-reserved-ar.patch
    # https://gitlab.winehq.org/wine/wine/-/merge_requests/7579
    patch -Np1 < ../patches/wine-hotfixes/pending/ntdll-return-access-violation-if-address-not-writable.patch
    # https://gitlab.winehq.org/wine/wine/-/merge_requests/8324
    patch -Np1 < ../patches/wine-hotfixes/pending/kernelbase-allocate-new-buffer-for-module-name.patch
    popd

### END PROTON-GE ADDITIONAL CUSTOM PATCHES ###
### END WINE PATCHING ###
