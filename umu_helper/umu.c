/*
 * Copyright (c) 2015, 2019, 2020, 2021, 2022 Valve Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* This is a stub steam.exe implementation for use inside Proton. It provides
 * a small subset of the actual Steam functionality for games that expect
 * Windows version of Steam running. */

#include <stddef.h>
#include <stdarg.h>

#include <wchar.h>
#include <wctype.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include <windows.h>
#include <tlhelp32.h>
#include <winsvc.h>
#include <winternl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdbool.h>

#include "wine/debug.h"

#include <msi.h>

WINE_DEFAULT_DEBUG_CHANNEL(umu);

static bool env_nonzero(const char *env)
{
    const char *v = getenv(env);
    return v != NULL && *v && v[0] != '0';
}

/* XBox Game Studios titles run GamingRepair.exe unless it has previously succeeded and set a registry value
 * to flag success. The program uses unimplemented AppInstallManager and therefore will not return success.
 * It causes a delay of around 20 seconds, so we set the registry key here to prevent it from running. */
static void set_gamingrepair_succeeded(const char *sgi)
{
    DWORD succeeded = 1;
    HKEY appkey, idkey;

    if (RegOpenKeyExW( HKEY_LOCAL_MACHINE, L"SOFTWARE\\Valve\\Steam\\Apps", 0, KEY_ALL_ACCESS | KEY_WOW64_32KEY, &appkey ))
        return;

    if (!RegOpenKeyExA( appkey, sgi, 0, KEY_ALL_ACCESS, &idkey ))
    {
        RegSetValueExW( idkey, L"GamingRepair", 0, REG_DWORD, (BYTE *)&succeeded, sizeof(succeeded) );
        RegCloseKey(idkey);
    }

    RegCloseKey(appkey);
}

size_t strappend( char **buf, size_t *buf_size, size_t pos, const char *fmt, ... )
{
    size_t size;
    va_list ap;
    char *ptr;
    int n;

    if (*buf)
    {
        size = *buf_size;
        ptr = *buf;
    }
    else
    {
        size = 100;
        ptr = malloc( size );
    }

    for (;;)
    {
        va_start( ap, fmt );
        n = vsnprintf( ptr + pos, size - pos, fmt, ap );
        va_end( ap );
        if (n == -1) size *= 2;
        else if (pos + (size_t)n >= size) size = pos + n + 1;
        else break;
        ptr = realloc( ptr, size );
    }

    *buf_size = size;
    *buf = ptr;
    return n;
}

static void setup_vr_registry(void)
{
    BOOL (CDECL *init)(void);
    HMODULE vrclient;

#ifdef _WIN64
    if (!(vrclient = LoadLibraryW( L"vrclient_x64" )))
#else
    if (!(vrclient = LoadLibraryW( L"vrclient" )))
#endif
    {
        ERR( "Failed to load vrclient module, skipping initialization\n" );
        return;
    }

    if ((init = (void *)GetProcAddress( vrclient, "vrclient_init_registry" ))) init();
    else ERR( "Failed to find vrclient_init_registry export\n" );

    TRACE( "Queued VR info initialization.\n" );
    FreeLibrary(vrclient);
}

static WCHAR *strchrW(WCHAR *h, WCHAR n)
{
    do
    {
        if(*h == n)
            return h;
    } while (*h++);

    return NULL;
}

int strncmpW(const WCHAR *l, const WCHAR *r, int n)
{
    if(n <= 0)
        return 0;

    while(--n > 0 && *l && *l == *r){
        l++;
        r++;
    }

    return *l - *r;
}

static WCHAR *find_quote(WCHAR *str)
{
    WCHAR *end = strchrW(str, '"'), *ch;
    int odd;
    while (end)
    {
        odd = 0;
        ch = end - 1;
        while (ch >= str && *ch == '\\')
        {
            odd = !odd;
            --ch;
        }
        if (!odd)
            return end;
        end = strchrW(end + 1, '"');
    }
    return NULL;
}

static BOOL WINAPI console_ctrl_handler(DWORD dwCtrlType)
{
    return TRUE;
}

static BOOL streq_niw(const WCHAR *l, const WCHAR *r, size_t len)
{
    while(len > 0){
        if(towlower(*l) != towlower(*r))
            return FALSE;
        ++l; ++r; --len;
    }
    return TRUE;
}

static WCHAR* get_end_of_excutable_name(WCHAR *cmdline)
{
    BOOL quoted = FALSE;
    WCHAR *executable_name_end = cmdline;

    /* find the end of the first arg...*/
    while (*executable_name_end != '\0' &&
           (*executable_name_end != ' ' || quoted))
    {
        quoted ^= *executable_name_end == '"';

        executable_name_end++;
    }

    return executable_name_end;
}

static BOOL should_use_shell_execute(WCHAR *cmdline)
{
    BOOL use_shell_execute = TRUE;
    const WCHAR *executable_name_end = get_end_of_excutable_name(cmdline);

    /* if the executable is quoted backtrack a bit */
    if (*(executable_name_end - 1) == '"')
        --executable_name_end;


    /* backtrack to before the end of the arg
     * and check if we end in .exe or not
     * and determine whether to use ShellExecute
     * based on that */
    executable_name_end -= strlen(".exe");

    if (executable_name_end >= cmdline)
    {
        if (streq_niw( executable_name_end, L".exe", 4 )) use_shell_execute = FALSE;
    }

    return use_shell_execute;
}

static BOOL try_recover_eadesktop_symlink(void)
{
    WIN32_FIND_DATAA ff;
    HANDLE handle, file;
    char path[MAX_PATH];

    handle = CreateFileA( "C:\\Program Files\\Electronic Arts\\EA Desktop\\EA Desktop", GENERIC_READ,
                          FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    if (handle != INVALID_HANDLE_VALUE || GetLastError() != ERROR_FILE_NOT_FOUND)
    {
        TRACE( "directory handle %p, err %ld.\n", handle, GetLastError() );
        if (handle != INVALID_HANDLE_VALUE) CloseHandle( handle );
        return FALSE;
    }

    if ((handle = FindFirstFileA( "C:\\Program Files\\Electronic Arts\\EA Desktop\\*.*", &ff )) == INVALID_HANDLE_VALUE)
        return FALSE;

    do
    {
        if (!(ff.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (!strcmp( ff.cFileName, "." ) || !strcmp( ff.cFileName, ".." )) continue;
        sprintf( path, "C:\\Program Files\\Electronic Arts\\EA Desktop\\%s\\EA Desktop\\Link2EA.exe", ff.cFileName );
        file = CreateFileA( path, GENERIC_READ,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0 );
        if (file == INVALID_HANDLE_VALUE) continue;
        CloseHandle( file );
        sprintf( path, "%s\\EA Desktop", ff.cFileName );
        if (CreateSymbolicLinkA( "C:\\Program Files\\Electronic Arts\\EA Desktop\\EA Desktop", path,
                                 SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE ))
        {
            TRACE( "linked to %s.\n", debugstr_a(path) );
            FindClose( handle );
            return TRUE;
        }
        else TRACE( "CreateSymbolicLinkA to %s failed, err %ld.\n", debugstr_a(path), GetLastError() );
    } while (FindNextFileA( handle, &ff ));
    FindClose( handle );
    return FALSE;
}

static HANDLE run_process(BOOL *should_await, BOOL game_process)
{
    WCHAR *cmdline = GetCommandLineW();
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    DWORD flags = CREATE_UNICODE_ENVIRONMENT;
    BOOL use_shell_execute = TRUE, link2ea = FALSE;
    BOOL hide_window;

    /* skip argv[0] */
    if (*cmdline == '"')
    {
        cmdline = find_quote(cmdline + 1);
        if (cmdline) cmdline++;
    }
    else
    {
        cmdline = strchrW(cmdline, ' ');
    }
    if (!cmdline)
    {
        WINE_ERR("Invalid command\n");
        return INVALID_HANDLE_VALUE;
    }
    while (*cmdline == ' ') cmdline++;

    /* convert absolute unix path to dos */
    if (cmdline[0] == '/' ||
            (cmdline[0] == '"' && cmdline[1] == '/'))
    {
        WCHAR *scratchW;
        char *scratchA;
        WCHAR *start, *end, *dos, *remainder, *new_cmdline;
        size_t argv0_len;
        int r;
        DWORD_PTR console;
        SHFILEINFOW sfi;

        static const WCHAR dquoteW[] = {'"',0};

        WINE_TRACE("Converting unix command: %s\n", wine_dbgstr_w(cmdline));

        if (cmdline[0] == '"')
        {
            start = cmdline + 1;
            end = find_quote(start);
            if (!end)
            {
                WINE_ERR("Unmatched quote? %s\n", wine_dbgstr_w(cmdline));
                goto run;
            }
            remainder = end + 1;
        }
        else
        {
            start = cmdline;
            end = strchrW(start, ' ');
            if (!end)
                end = strchrW(start, '\0');
            remainder = end;
        }

        argv0_len = end - start;

        scratchW = malloc((argv0_len + 1) * sizeof(WCHAR));
        memcpy(scratchW, start, argv0_len * sizeof(WCHAR));
        scratchW[argv0_len] = '\0';

        r = WideCharToMultiByte(CP_UNIXCP, 0, scratchW, -1,
                NULL, 0, NULL, NULL);
        if (!r)
        {
            WINE_ERR("Char conversion size failed?\n");
            goto run;
        }

        scratchA = malloc(r);

        r = WideCharToMultiByte(CP_UNIXCP, 0, scratchW, -1,
                scratchA, r, NULL, NULL);
        if (!r)
        {
            WINE_ERR("Char conversion failed?\n");
            goto run;
        }

        dos = wine_get_dos_file_name(scratchA);

        CoInitialize(NULL);

        console = SHGetFileInfoW(dos, 0, &sfi, sizeof(sfi), SHGFI_EXETYPE);
        if (console)
        {
            if (!HIWORD(console))
                flags |= CREATE_NEW_CONSOLE;
        }

        new_cmdline = malloc((wcslen(dos) + 3 + wcslen(remainder) + 1) * sizeof(WCHAR));
        wcscpy(new_cmdline, dquoteW);
        wcscat(new_cmdline, dos);
        wcscat(new_cmdline, dquoteW);
        wcscat(new_cmdline, remainder);

        cmdline = new_cmdline;
    }

run:
    WINE_TRACE("Running command %s\n", wine_dbgstr_w(cmdline));

    SetConsoleCtrlHandler( console_ctrl_handler, TRUE );

    use_shell_execute = should_use_shell_execute(cmdline);
    if (use_shell_execute && wcslen( cmdline ) > 10 && !memcmp( cmdline, L"link2ea://", 10 * sizeof(WCHAR) ))
    {
        HDESK desktop = GetThreadDesktop(GetCurrentThreadId());
        DWORD is_unavailable, type, size;
        SC_HANDLE manager, service;
        SERVICE_STATUS status;
        DWORD timeout = 3000;
        HKEY eakey;
        BOOL ret;

        link2ea = TRUE;
        if (!SetUserObjectInformationA(desktop, 1000, &timeout, sizeof(timeout)))
            WINE_ERR("Failed to set desktop timeout, err %lu.\n", GetLastError());

        if (!RegOpenKeyExW( HKEY_LOCAL_MACHINE, L"Software\\Electronic Arts\\EA Desktop", 0, KEY_ALL_ACCESS, &eakey ))
        {
            size = sizeof(is_unavailable);
            if (!RegQueryValueExW( eakey, L"IsUnavailable", NULL, &type, (BYTE *)&is_unavailable, &size ) &&
                type == REG_DWORD && is_unavailable)
            {
                WINE_ERR("EA Desktop\\IsUnavailable is set, clearing.\n");
                is_unavailable = 0;
                RegSetValueExW( eakey, L"IsUnavailable", 0, REG_DWORD, (BYTE *)&is_unavailable,
                                sizeof(is_unavailable) );
            }
            RegCloseKey(eakey);
        }
        if ((manager = OpenSCManagerW( NULL, SERVICES_ACTIVE_DATABASEW, SERVICE_QUERY_STATUS )))
        {
            if ((service = OpenServiceW( manager, L"EABackgroundService", SERVICE_QUERY_STATUS )))
            {
                if (QueryServiceStatus(service, &status))
                {
                    TRACE("dwCurrentState %#lx.\n", status.dwCurrentState);
                    if (status.dwCurrentState == SERVICE_STOP_PENDING || status.dwCurrentState == SERVICE_STOPPED)
                    {
                        ret = DeleteFileW( L"C:\\ProgramData\\EA Desktop\\backgroundservice.ini" );
                        WARN("Tried to delete backgroundservice.ini, ret %d, error %lu.\n", ret, GetLastError());
                    }
                }
                else ERR("Could not query service status, error %lu.\n", GetLastError());
                CloseServiceHandle(service);
            }
            else TRACE("Could not open EABackgroundService, error %lu.\n", GetLastError());
            CloseServiceHandle(manager);
        }
        else ERR("Could not open service manager, error %lu.\n", GetLastError());
    }
    hide_window = env_nonzero("PROTON_HIDE_PROCESS_WINDOW");

    /* only await the process finishing if we launch a process directly...
     * Steam simply calls ShellExecuteA with the same parameters.
     * this avoids the edge case where we could ShellExecute and
     * then that process ends up ShellExecuting something as a throw away */
    *should_await = !use_shell_execute;

    WINE_TRACE("Executing via %s\n",
        wine_dbgstr_a(use_shell_execute ? "ShellExecuteW" : "CreateProcessW"));

    if (use_shell_execute)
    {
        WCHAR *param = NULL;
        WCHAR *executable_name_end = get_end_of_excutable_name(cmdline);
        static const WCHAR verb[] = { 'o', 'p', 'e', 'n', 0 };
        unsigned int retry_count = 0;
        INT_PTR ret;

        if (*executable_name_end != '\0')
        {
            *executable_name_end = '\0';
            param = executable_name_end+1;
        }

        while ((ret = (INT_PTR)ShellExecuteW(NULL, verb, cmdline, param, NULL, hide_window ? SW_HIDE : SW_SHOWNORMAL)) < 32)
        {
            WINE_ERR("Failed to execute %s, ret %Iu.\n", wine_dbgstr_w(cmdline), ret);
            if (game_process && ret == SE_ERR_FNF && link2ea)
            {
                if (!try_recover_eadesktop_symlink()) break;
                if (retry_count++) break;
                continue;
            }
            if (game_process && ret == SE_ERR_NOASSOC && link2ea)
            {
                /* Try to uninstall EA desktop so it is set up from prerequisites on the next run. */
                UINT ret = MsiConfigureProductExW( L"{C2622085-ABD2-49E5-8AB9-D3D6A642C091}", 0,
                                                   INSTALLSTATE_DEFAULT, L"REMOVE=ALL" );

                WINE_TRACE("MsiConfigureProductExW ret %u.\n", ret);
                /* If uninstall failed this should trigger interactive repair window on the EA setup run. */
                RegDeleteTreeW( HKEY_LOCAL_MACHINE, L"Software\\Classes\\link2ea" );
                RegDeleteTreeW( HKEY_LOCAL_MACHINE, L"Software\\Electronic Arts\\EA Desktop" );
                RegDeleteTreeW( HKEY_LOCAL_MACHINE, L"Software\\Electronic Arts\\EA Core" );
            }
            break;
        }
        return INVALID_HANDLE_VALUE;
    }
    else
    {
        if (hide_window)
        {
            si.dwFlags |= STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
        }

        if (!CreateProcessW(NULL, cmdline, NULL, NULL, FALSE, flags, NULL, NULL, &si, &pi))
        {
            WINE_ERR("Failed to create process %s: %lu\n", wine_dbgstr_w(cmdline), GetLastError());
            return INVALID_HANDLE_VALUE;
        }

        CloseHandle(pi.hThread);

        return pi.hProcess;
    }
}

#ifndef DIRECTORY_QUERY
#define DIRECTORY_QUERY 0x0001
#endif

/* Ubisoft Connect (upc.exe) is started by the game through a bridge process which lives exactly as long
 * as the game, but upc.exe and its helpers keep running after the game exits. That keeps the wine session
 * and so the game alive from Steam's point of view, so close them once the bridge is gone. */
static const WCHAR *ubisoft_bridge_names[] =
{
    L"UbisoftGameLauncher.exe",
    L"UbisoftGameLauncher64.exe",
    L"UbisoftConnectLauncher.exe",
};

/* Seconds without the bridge or a visible Ubisoft Connect window before closing it, checked once a second. */
#define UBISOFT_QUIET_SECONDS 3

struct ubisoft_processes
{
    const WCHAR *dir;   /* Ubisoft Connect's install directory, with a trailing backslash */
    BOOL bridge;
    unsigned int count;
    DWORD pids[MAXIMUM_WAIT_OBJECTS];
};

/* Reads Ubisoft Connect's install directory into dir, returns FALSE if it isn't installed. */
static BOOL get_ubisoft_install_dir( WCHAR *dir, DWORD dir_size )
{
    DWORD size = (dir_size - 1) * sizeof(WCHAR);
    size_t len;

    if (RegGetValueW( HKEY_LOCAL_MACHINE, L"Software\\Ubisoft\\Launcher", L"InstallDir",
                      RRF_RT_REG_SZ | RRF_SUBKEY_WOW6432KEY, NULL, dir, &size ))
        return FALSE;
    if (!(len = wcslen( dir ))) return FALSE;
    if (dir[len - 1] != '\\') wcscpy( dir + len, L"\\" );
    TRACE( "Ubisoft Connect installed in %s.\n", debugstr_w(dir) );
    return TRUE;
}

/* Looks for the bridge and, if want_pids is set, for the processes running from Ubisoft Connect's directory. */
static BOOL scan_ubisoft_processes( struct ubisoft_processes *procs, BOOL want_pids )
{
    PROCESSENTRY32W entry = { sizeof(entry) };
    unsigned int i, dir_len = wcslen( procs->dir );
    HANDLE snapshot, process;
    WCHAR path[MAX_PATH];
    DWORD size;

    procs->bridge = FALSE;
    procs->count = 0;

    snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        ERR( "Failed to create process snapshot, err %lu.\n", GetLastError() );
        return FALSE;
    }

    if (Process32FirstW( snapshot, &entry ))
    {
        do
        {
            for (i = 0; i < ARRAY_SIZE(ubisoft_bridge_names); ++i)
                if (!lstrcmpiW( entry.szExeFile, ubisoft_bridge_names[i] )) procs->bridge = TRUE;
        } while (!procs->bridge && Process32NextW( snapshot, &entry ));
    }

    if (!procs->bridge && want_pids && Process32FirstW( snapshot, &entry ))
    {
        do
        {
            if (!(process = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID ))) continue;
            size = ARRAY_SIZE(path);
            if (QueryFullProcessImageNameW( process, 0, path, &size ) && size > dir_len
                    && CompareStringOrdinal( path, dir_len, procs->dir, dir_len, TRUE ) == CSTR_EQUAL)
                procs->pids[procs->count++] = entry.th32ProcessID;
            CloseHandle( process );
        } while (procs->count < ARRAY_SIZE(procs->pids) && Process32NextW( snapshot, &entry ));
    }

    CloseHandle( snapshot );
    return TRUE;
}

static BOOL is_ubisoft_window( HWND hwnd, const struct ubisoft_processes *procs )
{
    unsigned int i;
    DWORD pid;

    GetWindowThreadProcessId( hwnd, &pid );
    for (i = 0; i < procs->count; ++i)
        if (procs->pids[i] == pid) return TRUE;
    return FALSE;
}

struct ubisoft_visible_params
{
    const struct ubisoft_processes *procs;
    BOOL visible;
};

static BOOL CALLBACK find_visible_ubisoft_window( HWND hwnd, LPARAM lparam )
{
    struct ubisoft_visible_params *params = (struct ubisoft_visible_params *)lparam;

    if (!is_ubisoft_window( hwnd, params->procs ) || !IsWindowVisible( hwnd )) return TRUE;
    params->visible = TRUE;
    return FALSE;
}

/* upc.exe shows a window while it syncs cloud saves after the game exits, or when it needs the user,
 * e.g. to resolve a cloud save conflict. */
static BOOL ubisoft_window_visible( const struct ubisoft_processes *procs )
{
    struct ubisoft_visible_params params = { procs, FALSE };

    EnumWindows( find_visible_ubisoft_window, (LPARAM)&params );
    return params.visible;
}

struct ubisoft_message_params
{
    const struct ubisoft_processes *procs;
    UINT msg;
    WPARAM wparam;
    BOOL accepted;
};

static BOOL CALLBACK send_ubisoft_window_message( HWND hwnd, LPARAM lparam )
{
    struct ubisoft_message_params *params = (struct ubisoft_message_params *)lparam;
    DWORD_PTR result;

    if (!is_ubisoft_window( hwnd, params->procs )) return TRUE;
    if (!SendMessageTimeoutW( hwnd, params->msg, params->wparam, 0, SMTO_ABORTIFHUNG, 5000, &result ))
    {
        /* Not responding, likely still busy, so ask again later. */
        TRACE( "Message %#x to hwnd %p timed out.\n", params->msg, hwnd );
        params->accepted = FALSE;
    }
    else if (params->msg == WM_QUERYENDSESSION && !result)
        params->accepted = FALSE;
    return TRUE;
}

/* Sends msg to every Ubisoft Connect window. Returns FALSE if any of them doesn't respond or,
 * for WM_QUERYENDSESSION, refuses. */
static BOOL send_ubisoft_message( const struct ubisoft_processes *procs, UINT msg, WPARAM wparam )
{
    struct ubisoft_message_params params = { procs, msg, wparam, TRUE };

    EnumWindows( send_ubisoft_window_message, (LPARAM)&params );
    return params.accepted;
}

/* Returns FALSE if upc.exe isn't ready to quit yet, e.g. it refused, didn't respond or shows a window. */
static BOOL close_ubisoft_launcher( struct ubisoft_processes *procs )
{
    HANDLE handles[ARRAY_SIZE(procs->pids)];
    unsigned int i, count, pass;
    BOOL accepted, visible;

    /* upc.exe ignores WM_CLOSE but handles session end. WM_ENDSESSION is sent either way, to end the session
     * or to tell it that it was cancelled. */
    accepted = send_ubisoft_message( procs, WM_QUERYENDSESSION, 0 );
    if (!send_ubisoft_message( procs, WM_ENDSESSION, accepted ) || !accepted) return FALSE;

    /* It doesn't exit on its own after that, and some of its windowless helpers may outlive it. */
    for (pass = 0;; ++pass)
    {
        if (!scan_ubisoft_processes( procs, TRUE ) || procs->bridge) return FALSE;
        if (!procs->count) return TRUE;
        /* Something keeps coming back, e.g. a respawning helper, so retry later. */
        if (pass == 3) return FALSE;

        for (i = count = 0; i < procs->count; ++i)
        {
            if ((handles[count] = OpenProcess( SYNCHRONIZE | PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION,
                                               FALSE, procs->pids[i] )))
                ++count;
        }
        /* WM_ENDSESSION returned, so it's done cleaning up, but give helpers a moment to exit on their own. */
        if (count) WaitForMultipleObjects( count, handles, TRUE, 500 );

        /* Back off if it showed a window in the meantime, e.g. while cleaning up. */
        visible = ubisoft_window_visible( procs );
        for (i = 0; i < count; ++i)
        {
            if (!visible && WaitForSingleObject( handles[i], 0 ) == WAIT_TIMEOUT)
            {
                TRACE( "Terminating process %04lx.\n", GetProcessId( handles[i] ) );
                TerminateProcess( handles[i], 0 );
            }
            CloseHandle( handles[i] );
        }
        if (visible) return FALSE;
    }
}

/* arg is Ubisoft Connect's install directory, from get_ubisoft_install_dir(). */
static DWORD WINAPI ubisoft_launcher_thread(void *arg)
{
    struct ubisoft_processes procs = { arg };
    unsigned int quiet = 0;
    BOOL armed = FALSE;

    for (;;)
    {
        Sleep( 1000 );
        if (!scan_ubisoft_processes( &procs, armed )) continue;

        if (procs.bridge)
        {
            if (!armed) TRACE( "Ubisoft game launcher started.\n" );
            armed = TRUE;
            quiet = 0;
            continue;
        }
        if (!armed) continue;
        if (!procs.count)
        {
            TRACE( "Ubisoft Connect exited.\n" );
            armed = FALSE;
            continue;
        }

        /* Tolerate the bridge being briefly restarted, and never close Ubisoft Connect while it shows a window. */
        if (ubisoft_window_visible( &procs )) quiet = 0;
        else ++quiet;
        if (quiet < UBISOFT_QUIET_SECONDS) continue;

        quiet = 0;
        if (close_ubisoft_launcher( &procs ))
        {
            TRACE( "Closed Ubisoft Connect.\n" );
            armed = FALSE;
        }
        else TRACE( "Ubisoft Connect isn't ready to quit, retrying.\n" );
    }

    return 0;
}


/* Only close Ubisoft Connect if it was started as a dependency of the launched game. If the user launched
 * Ubisoft Connect itself, it should keep running. Games install to a "games" subdirectory of Ubisoft Connect's
 * directory by default, so only executables directly in that directory count. Names are also compared because
 * the same file can be reached through a different path, e.g. Z:\. */
static BOOL launched_ubisoft_connect( HANDLE child, const WCHAR *dir )
{
    static const WCHAR *names[] = { L"upc.exe", L"UbisoftConnect.exe" };
    unsigned int i, dir_len = wcslen( dir );
    WCHAR path[MAX_PATH];
    const WCHAR *name;
    DWORD size = ARRAY_SIZE(path);

    if (child == INVALID_HANDLE_VALUE) return FALSE;
    if (!QueryFullProcessImageNameW( child, 0, path, &size ))
    {
        ERR( "Failed to get launched executable path, err %lu.\n", GetLastError() );
        return FALSE;
    }

    if (size > dir_len && CompareStringOrdinal( path, dir_len, dir, dir_len, TRUE ) == CSTR_EQUAL
            && !wcschr( path + dir_len, '\\' ))
        return TRUE;

    name = PathFindFileNameW( path );
    for (i = 0; i < ARRAY_SIZE(names); ++i)
        if (!lstrcmpiW( name, names[i] )) return TRUE;
    for (i = 0; i < ARRAY_SIZE(ubisoft_bridge_names); ++i)
        if (!lstrcmpiW( name, ubisoft_bridge_names[i] )) return TRUE;
    return FALSE;
}

BOOL is_ptraced(void)
{
    DWORD len;
    int pid;

    if (NtQueryInformationProcess( GetCurrentProcess(), 1100 /* ProcessWineUnixDebuggerPid */,
                                   &pid, sizeof(pid), &len ))
        return FALSE;

    return !!pid;
}

int main(int argc, char *argv[])
{
    static WCHAR ubisoft_dir[MAX_PATH];
    HANDLE wait_handle = INVALID_HANDLE_VALUE;
    HANDLE child = INVALID_HANDLE_VALUE;
    BOOL game_process = FALSE;
    const char *sgi;
    DWORD rc = 0;

    WINE_TRACE("\n");

    if ((sgi = getenv("SteamGameId")))
    {
        /* Create desktop window in main thread to prevent race with background thread. */
        GetDesktopWindow();

        set_gamingrepair_succeeded(sgi);

        if (env_nonzero("PROTON_WAIT_ATTACH"))
        {
            unsigned int sleep_count = 0;
            WINE_TRACE("PROTON_WAIT_ATTACH is set, waiting for debugger...\n");
            while (!IsDebuggerPresent() && !is_ptraced())
            {
                Sleep(100);
                ++sleep_count;
                if (sleep_count >= 10)
                {
                    WINE_TRACE("still waiting for debugger...\n");
                    sleep_count = 0;
                }
            }
        }

        game_process = TRUE;
    }

    if (argc > 1)
    {
        BOOL should_await;

        if (game_process)
            setup_vr_registry();

        child = run_process(&should_await, game_process);

        if (should_await)
        {
            if (child == INVALID_HANDLE_VALUE)
                return 1;

            wait_handle = child;
        }

        if (game_process && !env_nonzero("PROTON_KEEP_UBISOFT_LAUNCHER")
                && get_ubisoft_install_dir( ubisoft_dir, ARRAY_SIZE(ubisoft_dir) )
                && !launched_ubisoft_connect( child, ubisoft_dir ))
            CreateThread(NULL, 0, ubisoft_launcher_thread, ubisoft_dir, 0, NULL);
    }

    if (game_process)
        NtSetInformationProcess( GetCurrentProcess(), 1000 /* ProcessWineMakeProcessSystem */,
                                 &wait_handle, sizeof(HANDLE *) );

    if(wait_handle != INVALID_HANDLE_VALUE)
    {
        HANDLE waits[2];
        DWORD ret;
        int wait_count;

        waits[0] = wait_handle;
        waits[1] = NULL;
        wait_count = 1;
        if (game_process)
        {
            if ((waits[1] = CreateEventW( NULL, FALSE, FALSE, L"PROTON_STEAM_EXE_RESTART_APP" )))
            {
                ++wait_count;
            }
            else
            {
                ERR("Failed to create restart event, err %lu.\n", GetLastError());
            }
        }
        FreeConsole();
        while ((ret = WaitForMultipleObjects(wait_count, waits, FALSE, INFINITE) != WAIT_OBJECT_0))
        {
            BOOL should_await;

            if (ret != WAIT_OBJECT_0 + 1)
            {
                WINE_ERR("Wait failed.\n");
                break;
            }
            if (child != INVALID_HANDLE_VALUE)
            {
                if (WaitForSingleObject(child, 0) == WAIT_TIMEOUT)
                {
                    WINE_ERR("Child is still running, not restarting.\n");
                    continue;
                }
                CloseHandle(child);
            }
            child = run_process(&should_await, game_process);
        }
        CloseHandle(waits[1]);
    }

    if (child != INVALID_HANDLE_VALUE)
    {
        GetExitCodeProcess(child, &rc);
        CloseHandle(child);
    }

    return rc;
}
