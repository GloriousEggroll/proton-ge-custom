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
        INT_PTR ret;

        if (*executable_name_end != '\0')
        {
            *executable_name_end = '\0';
            param = executable_name_end+1;
        }

        if ((ret = (INT_PTR)ShellExecuteW(NULL, verb, cmdline, param, NULL, hide_window ? SW_HIDE : SW_SHOWNORMAL)) < 32)
        {
            WINE_ERR("Failed to execute %s, ret %Iu.\n", wine_dbgstr_w(cmdline), ret);
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
    HANDLE wait_handle = INVALID_HANDLE_VALUE;
    HANDLE child = INVALID_HANDLE_VALUE;
    BOOL game_process = FALSE;
    const char *sgi;
    DWORD rc = 0;

    WINE_TRACE("\n");

    if ((sgi = getenv("SteamGameId")))
    {
        WCHAR path[MAX_PATH], *p;

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
