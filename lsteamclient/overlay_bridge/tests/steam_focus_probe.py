#!/usr/bin/env python3
"""Probe Steam focus IPC from a separate process, without attaching to a game.

This is a diagnostic, not an installed component. Private ABI calls are limited
to the ELF build manually inspected during the September 2026 investigation.
By default, only read focus. Writing focus requires both --focus-game and
--focus-pid, and the target game's overlay must already be open.
"""

import argparse
import ctypes as c
import os
import signal
import subprocess
import time
from pathlib import Path


BUILD_ID = "7b0847f04cf284f01a1a165561df054902df31a7"
ENGINE_VERSION = b"CLIENTENGINE_INTERFACE_VERSION005"
DESKTOP_GAME_ID = 413080


class DlInfo(c.Structure):
    _fields_ = [("name", c.c_char_p), ("base", c.c_void_p),
                ("symbol", c.c_char_p), ("symbol_address", c.c_void_p)]


def emit(*args):
    print(time.strftime("%H:%M:%S"), *args, flush=True)


class SteamFocus:
    def __init__(self, library):
        notes = subprocess.check_output(["readelf", "-n", str(library)], text=True)
        if f"Build ID: {BUILD_ID}" not in notes:
            raise RuntimeError("Uninspected Steam build; refusing private ABI calls")
        if c.sizeof(c.c_void_p) != 8:
            raise RuntimeError("This diagnostic requires a 64-bit Python process")

        self.library = library.resolve()
        self.lib = c.CDLL(str(self.library), mode=os.RTLD_NOW | os.RTLD_LOCAL)
        self.lib.CreateInterface.argtypes = [c.c_char_p, c.POINTER(c.c_int)]
        self.lib.CreateInterface.restype = c.c_void_p
        self.dladdr = c.CDLL(None).dladdr
        self.dladdr.argtypes = [c.c_void_p, c.POINTER(DlInfo)]
        self.dladdr.restype = c.c_int
        for name, result, args in (
            ("Steam_CreateSteamPipe", c.c_int, []),
            ("Steam_BReleaseSteamPipe", c.c_bool, [c.c_int]),
            ("Steam_ConnectToGlobalUser", c.c_int, [c.c_int]),
            ("Steam_ReleaseUser", None, [c.c_int, c.c_int]),
        ):
            fn = getattr(self.lib, name)
            fn.argtypes, fn.restype = args, result
        self.pipe = self.user = 0

    def member(self, interface, slot, expected_offset, result, *args):
        table = c.cast(interface, c.POINTER(c.POINTER(c.c_void_p)))[0]
        address, info = table[slot], DlInfo()
        if not self.dladdr(address, c.byref(info)) or not info.name:
            raise RuntimeError("Unresolved private interface member")
        if (not os.path.samefile(os.fsdecode(info.name), self.library)
                or address - info.base != expected_offset):
            raise RuntimeError("Private interface layout differs from inspected build")
        # The offset is a guard, not a function address used to make the call.
        return c.CFUNCTYPE(result, *args)(address)

    def connect(self):
        error = c.c_int(-1)
        engine = self.lib.CreateInterface(ENGINE_VERSION, c.byref(error))
        if not engine or error.value:
            raise RuntimeError("Missing expected client engine")
        get_utils = self.member(engine, 14, 0x1610a10, c.c_void_p,
                                c.c_void_p, c.c_int)
        self.pipe = self.lib.Steam_CreateSteamPipe()
        if not self.pipe:
            raise RuntimeError("Steam is unavailable")
        self.user = self.lib.Steam_ConnectToGlobalUser(self.pipe)
        if not self.user:
            raise RuntimeError("No logged-in Steam user")
        self.utils = get_utils(engine, self.pipe)
        if not self.utils:
            raise RuntimeError("IClientUtils is unavailable")
        self.set_focus = self.member(
            self.utils, 48, 0x13b4ea0, None, c.c_void_p, c.POINTER(c.c_uint64),
            c.c_bool, c.c_bool, c.c_uint32, c.c_uint16, c.c_uint16)
        # CGameID has an implicit result pointer in this C++ ABI. Slot 54 reads
        # the actual focused overlay instance, without slot 53's game fallback.
        self.get_focus = self.member(
            self.utils, 54, 0x1366f20, c.c_void_p, c.POINTER(c.c_uint64),
            c.c_void_p, c.POINTER(c.c_bool), c.POINTER(c.c_uint32))
        self.get_context = self.member(
            self.utils, 103, 0x1330f20, c.c_void_p,
            c.POINTER(c.c_uint64), c.c_void_p)
        self.get_context_pid = self.member(
            self.utils, 104, 0x12edcf0, c.c_uint32, c.c_void_p)
        emit("connected to IClientUtils through an independent Steam pipe")

    def state(self):
        game, active, pid = c.c_uint64(), c.c_bool(), c.c_uint32()
        self.get_focus(c.byref(game), self.utils, c.byref(active), c.byref(pid))
        return game.value, active.value, pid.value

    def focus(self, game, pid):
        game_id = c.c_uint64(game)
        self.set_focus(self.utils, c.byref(game_id), True, False, pid, 0, 0)

    def context(self):
        game = c.c_uint64()
        self.get_context(c.byref(game), self.utils)
        return game.value, self.get_context_pid(self.utils)

    def close(self):
        try:
            if self.user:
                self.lib.Steam_ReleaseUser(self.pipe, self.user)
                self.user = 0
        finally:
            if self.pipe:
                self.lib.Steam_BReleaseSteamPipe(self.pipe)
                self.pipe = 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library", type=Path, default=Path.home()
                        / ".local/share/Steam/linux64/steamclient.so")
    parser.add_argument("--seconds", type=float, default=10)
    parser.add_argument("--focus-game", type=int)
    parser.add_argument("--focus-pid", type=int)
    parser.add_argument("--wait-focus", type=float, default=0,
                        help="wait up to this many seconds for the target overlay")
    args = parser.parse_args()
    if not 0 < args.seconds <= 120:
        parser.error("seconds must be greater than zero and at most 120")
    if (args.focus_game is None) != (args.focus_pid is None):
        parser.error("focus writes require both --focus-game and --focus-pid")
    if not 0 <= args.wait_focus <= 120:
        parser.error("wait-focus must be between zero and 120")
    if args.wait_focus and args.focus_game is None:
        parser.error("wait-focus requires a focus-write target")
    if args.focus_game is not None:
        if not 0 < args.focus_game < 2**64 or not 0 < args.focus_pid < 2**32:
            parser.error("invalid game ID or process ID")
        os.kill(args.focus_pid, 0)

    def interrupted(signum, frame):
        raise KeyboardInterrupt

    signal.signal(signal.SIGTERM, interrupted)
    client = SteamFocus(args.library)
    changed = False
    try:
        client.connect()
        initial = client.state()
        emit("focused overlay (game, active, pipe PID):", initial)
        emit("controller context (game, PID):", client.context())
        if args.focus_game is not None:
            deadline = time.monotonic() + args.wait_focus
            while (initial[0] != args.focus_game or not initial[1]) and time.monotonic() < deadline:
                time.sleep(0.25)
                current = client.state()
                if current != initial:
                    emit("focused overlay (game, active, pipe PID):", current)
                initial = current
            if initial[0] != args.focus_game or not initial[1]:
                raise RuntimeError("Target game's overlay must already be focused and open")
            emit("SET controller focus:", args.focus_game, "game PID:", args.focus_pid)
            changed = True
            client.focus(args.focus_game, args.focus_pid)
        deadline = time.monotonic() + args.seconds
        previous = initial
        while time.monotonic() < deadline:
            time.sleep(min(0.25, max(0, deadline - time.monotonic())))
            current = client.state()
            if current != previous:
                emit("focused overlay (game, active, pipe PID):", current)
                previous = current
    finally:
        try:
            if changed:
                game, active, pid = client.state()
                if game == args.focus_game and not active:
                    # A successful Guide close should leave input with the game.
                    emit("RESTORE closed-overlay game focus:", game)
                    client.focus(game, args.focus_pid)
                elif game in (0, args.focus_game):
                    emit("RESTORE Desktop controller focus")
                    client.focus(DESKTOP_GAME_ID, 0)
                else:
                    emit("Another game owns overlay focus; leaving its routing alone")
        finally:
            client.close()
            emit("probe connection closed")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error))
