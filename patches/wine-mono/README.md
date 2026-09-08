# Wine-Mono Runtime Fixes

These patches apply to Wine-Mono's Mono submodule, not Wine.
`patches/protonprep-valve-staging.sh` prepares the official Wine-Mono 11.2.0
source release and applies them. The Proton `Makefile.in` then builds
`libmono-2.0-x86.dll` and `libmono-2.0-x86_64.dll` using the SDK's
MinGW toolchains. Both runtime DLLs replace their prebuilt counterparts in
the distribution. Managed libraries, WPF, WinForms, support MSI and other
native helpers remain from the matching official binary release.

The runtime patches do not change winewayland, mscoree, executable metadata,
or game prefixes. The separate Wine SQM registry initialization described
below addresses a later failure. The layer/graphics work is unrelated.

## Source and Build

- Upstream: <https://github.com/wine-mono/wine-mono>
- Release: `wine-mono-11.2.0`, commit
  `2973e4af02eeec2f2727e88e4559cb149271775d`.
- Mono submodule: `775a29a0d864061a40438e38830c0afaadf9ae79`.
- Source artifact: `wine-mono-11.2.0-src.tar.xz` from the official release.
- Native runtime implementation remains upstream Wine-Mono/Mono, with the
  local patches authored by GloriousEggroll. No ReactOS source is used.

The first prep downloads the approximately 307 MB source archive to
`contrib/`. Cached archives are reused without checksum verification.
Every prep removes the generated `wine-mono/` source tree, re-extracts
the pristine source including pinned
submodules, and applies `patches/wine-mono/*.patch` in filename order.
Re-extraction is the archive equivalent of reset/clean for this component;
there is no Wine-Mono Git submodule. A symlink or Git checkout at that path
is rejected rather than deleted.

Only successful prep publishes `wine-mono/.proton-prepared`. The build
copies that prepared tree into `build/src-wine-mono-11.2.0/` and builds the
native runtimes there; it does not apply patches a second time. Re-running
prep invalidates those native build outputs. Builds without another prep
reuse them. Re-run prep whenever changing the patch stack. Missing or stale
prep stops the build with an explicit instruction to run protonprep.

No extra LLVM toolchain download or full managed framework rebuild is
requested. The generated source tree is ignored by the main Git repository.
The native runtime link explicitly uses `-static-libgcc`: the SDK's x86
GCC otherwise imports its integer-division helpers from
`libgcc_s_dw2-1.dll`, which is not included in the distribution. Pass this
through `PDB_LDFLAGS_LIBMONO_x86` / `PDB_LDFLAGS_LIBMONO_x86_64`, because
`mono.make` replaces `LDFLAGS` for the final runtime link. These variables
also suppress the unwanted PDB filename when generating DWARF symbols.

When updating Wine-Mono, update the version in `source.conf`
(shared by the prep script and Makefile), rebase the runtime patch, and
keep the binary and source release versions matched.

The agent has not compiled Proton or Mono. The user builds separately.
Source/metadata inspection, patch validation, and diagnostic launches of
the user's rebuilt runtime have been performed. The regression fixtures
have not been assembled or run.

The user's first rebuilt capture (September 7, 23:48,
`GE-Proton11-6-27-g80365ed3c`) failed earlier than the managed initializer:
`mscoree` could not load Mono because `libgcc_s_dw2-1.dll` was missing.
Inspecting the installed x86 DLL confirmed that import; the x86_64 DLL did
not have it. The September 8 rebuilt capture confirms that the static-link
correction fixed this: Mono now loads, and the installed x86 DLL no longer
imports libgcc. Import-table inspection is not an archive checksum check.

That capture also contains a separate `explorer.exe` heap-corruption abort
before Purple starts, followed by repeated exception handling failures.
Its cause has not been established; do not attribute it to Mono, which
had not loaded. That earlier exception loop was not present in the new
capture.

## Purple Failure

Observed in `Purple.exe` 2.26.907.19 with Wine-Mono 11.2.0:

- `9637AABA.B71B1EA0` is a static readonly byte with HasFieldRVA.
- The FieldRVA table contains matching rows with RVA **zero**. The earlier
  protonfix description saying the row is missing is not correct for this
  version. A zero RVA and a missing row are distinct cases.
- `9637AABA::0B3E3192` takes this field's address (`ldsflda`).
- Mono treats it as absent data, emits the "should have RVA data" warning,
  and uses ordinary zero-initialized static storage. That is not the PE
  image base that an explicit zero RVA represents in a mapped module.
- The initializer itself contains the `newobj BadImageFormatException` /
  `throw` sequence. The crash log shows that exception followed by
  `TypeInitializationException` and module-constructor failure.

Microsoft's public implementation uses `GetRvaData(field, NULL_OK)` in
[PEFile::GetRvaField](https://github.com/dotnet/coreclr/blob/master/src/vm/pefile.inl).
[PEDecoder::GetRvaData](https://github.com/dotnet/runtime/blob/main/src/coreclr/utilcode/pedecoder.cpp)
returns the image base plus the RVA in that case, including RVA zero.
The patch implements that distinction only for Win32 module-backed field
data. It does not make the generic RVA mapper accept zero for other uses,
suppress exceptions, or change missing-row/static-storage behavior.

This is a concrete compatibility mismatch, but fixing it is not yet proof
that all of Purple's WPF/CLR requirements are satisfied. Retain the existing
`l2` protonfix's `dotnet48` fallback until end-to-end testing succeeds.

For runtime validation, use the existing Mono-only `purple` prefix and a
neutral GAMEID such as `umu-purple`, not `umu-l2` (which automatically
installs dotnet48). Do not uninstall .NET from the working prefix. Confirm
the log loads the rebuilt `libmono-2.0-x86.dll`, and capture managed stacks
if another initializer fails:

```sh
WINE_MONO_TRACE=E:System.BadImageFormatException,E:System.TypeInitializationException \
WINEDEBUG=+timestamp,+pid,+tid,+seh,+mscoree,+loaddll \
WINEPREFIX="$HOME/Games/purple" GAMEID=umu-purple \
PROTONPATH=GE-Proton11-6-x86_64 PROTON_LOG=1 PROTON_ENABLE_WAYLAND=1 \
umu-run "$HOME/Games/purple/drive_c/Program Files (x86)/NC/Purple/PurpleLauncher.exe"
```

The captures inspected were `/home/tcrider/steam-purple.log` (Mono crash)
and `/home/tcrider/steam-0.log` (the `purple-dotnet` prefix using native CLR
and WPF). The originally supplied `steam-purple-dotnet.log` path did not
exist during inspection.

## Reflection Follow-Up

The September 8 capture gets past `9637AABA` with patch 0001. A diagnostic
JIT trace confirms `0B3E3192` returns the actual module base, `0x00400000`.
The next failure is `EEB42305..cctor`: it calls
`Marshal.GetDelegateForFunctionPointer(IntPtr.Zero, ...)`, resulting in
`ArgumentNullException("ptr")`. Keep that validation intact.

The program trace and read-only metadata inspection identify an earlier
type resolution error:

- TypeSpec `0x1b000993` has signature `10 05`, meaning `System.Byte&`.
- The runtime resolves it as `System.Byte`. Purple's reference conversion
  consequently truncates an address to a byte instead of dereferencing it.
- For the first byte, address `0x0076e8ea` becomes `0xea`; the actual byte
  there is `0xf3`. Following the logged arithmetic with the actual byte
  instead of the truncated address reconstructs `ntdll.dll`, whereas the
  failing run produces an invalid module name.
- `module_resolve_type_token()` converts every TypeSpec to `MonoClass`
  and returns its `byval_arg`, discarding the byref bit. Both
  `Module.ResolveType` and `Module.ResolveMember` use that helper.

Patch 0002 uses the existing `mono_type_get_checked()` path for loaded
TypeSpecs, preserving the full signature. It also preserves byref when
that helper canonicalizes inflated generic types. TypeDef/TypeRef paths,
dynamic images, token validation, and exception handling remain unchanged.
This is a generic Mono fix, with no Purple executable-name checks.
The API must return the type identified by the token, including its
generic context; see Microsoft's
[Module.ResolveType documentation](https://learn.microsoft.com/en-us/dotnet/api/system.reflection.module.resolvetype?view=netframework-4.8.1).

Diagnostic captures are in `/tmp/purple-mono-{lookup,pointer,startup}/`.
The full `startup/steam-purple.log` is approximately 59 MB. Those runs
exited during initialization; no game was launched or modified. Do not
enable `WINE_MONO_TRACE=program` for an ordinary user retest: it is much
heavier than exception tracing. The user's `~/steam-purple.log` was kept
separate from these diagnostic captures.

An interpreter-only comparison (`WINE_MONO_AOT=interp`) still hit the
earlier RVA initializer failure. It did not reach the new reflection
failure and does not establish whether patch 0002 fixes interpreter mode.
Normal JIT startup is the current target. The user's next rebuilt-runtime
test gets past both initializers and opens the client. Full Purple/WPF
compatibility is not yet verified. Retain the dotnet48 fallback for now.

After rebuilding, a suitable lightweight capture is:

```sh
WINE_MONO_TRACE=E:System.ArgumentNullException,E:System.TypeInitializationException,E:System.BadImageFormatException \
WINEDEBUG=+timestamp,+pid,+tid,+mscoree,+loaddll \
WINEPREFIX="$HOME/Games/purple" GAMEID=umu-purple \
PROTONPATH=GE-Proton11-6-x86_64 PROTON_LOG=1 PROTON_ENABLE_WAYLAND=1 \
umu-run "$HOME/Games/purple/drive_c/Program Files (x86)/NC/Purple/PurpleLauncher.exe"
```

## Missing SQM Machine Identifier

The September 8 `01:33` capture in `~/steam-purple.log` gets past both
initializers. The next popup is `ArgumentNullException("machineId")` in
`Formula.Infrastructure.Logis.LogisService`, also reached from
`SignInWebViewController.WriteResultLog` after sign-in.

Read-only inspection of the running factory and `Formula.Core.dll` shows
that Purple calls `RegistryHelper.GetMachineId`, which reads
`HKLM\Software\Microsoft\SQMClient\MachineId` using `Registry64` on a
64-bit OS. This key is absent in the Mono prefix but present in the
working `purple-dotnet` prefix. The installed `mscorlib.dll` uses CoreFX's
registry implementation, which preserves and forwards the registry view.
The legacy `Microsoft.Win32/RegistryKey.cs` implementation that ignores
views is excluded from this Wine-Mono profile; do not patch it for this
failure.

This is missing Wine environment data, not a further Mono runtime bug.
`patches/wine-hotfixes/pending/wineboot-create-sqm-machine-id.patch` adds
generic boot-time initialization with `UuidCreate`, following Microsoft's
[documented SQM registry behavior](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-sqmcs2/0442f736-8670-4ae4-8651-5732f3ba18dd).
It preserves an existing value and neither opts into telemetry nor starts
any telemetry service. The pending patch is explicitly applied by
`protonprep-valve-staging.sh`. `default_pfx.py` strips only the template's
SQM `MachineId`, so new prefixes get separate IDs instead of inheriting
one shared build-machine value. User prefixes are not filtered.

The same log has a later `ArgumentNullException("type")` / platform-not-
supported exception in WPF's WinRT `InputPane` activation while focusing
the error dialog's textbox. That is downstream of the logging-service
failure, not evidence that touch-keyboard support caused the first popup.
It has not been patched here.

After rebuilding, fully exit Purple and relaunch the same Mono-only prefix
so the new wineboot runs. No prefix recreation or dotnet48 installation
should be needed for the missing key. Verify sign-in, navigation, and game
launch separately; end-to-end success is still pending. Registry validation
should check a nonempty braced GUID in the native view, preservation after
another boot, different values in two fresh prefixes, and no SQM MachineId
in the packaged default prefix. The Wine patch has not been compiled or
runtime-tested by the agent. No-fuzz patch applicability, Git whitespace
checks, and prep-script syntax checks passed. Three in-memory Python tests
also passed: SQM-only filtering (including idempotence and preservation of
other values), existing font filtering, and preservation of the separate
Cryptography `MachineGuid`. These checks did not run a build or alter a
user prefix. The currently open client was briefly attached
for a read-only method inspection and detached, not restarted or modified.

### TypeSpec Regression Fixture

`tests/resolve-typespec.il` is independent of Purple. It obtains tokens
from method bodies instead of assuming their assigned row numbers, and
compares `ResolveType` / `ResolveMember` results with the corresponding
method return types. Cases cover byte/int byrefs, generic type/method
byrefs, pointers, arrays, a plain value type, and a TypeDef control.

```sh
ilasm /exe /output:ResolveTypeSpec.exe tests/resolve-typespec.il
```

Run with both baseline and patched Mono, and native .NET Framework as a
comparison. No executable metadata rewriting is necessary for this test.
Expected success is exit 0; exit 1 indicates ResolveType mismatch, exit 2
ResolveMember mismatch, and exit 3 a malformed fixture. Test names print
before each comparison. This fixture has not yet been assembled or run.

## Standalone Regression Fixture

The test does not use or modify Purple. Assemble `tests/rva-zero.il` with
Mono or Microsoft `ilasm`, then use the `dnfile` parser to set the test's
Zero field to an explicit RVA of zero and mark Missing as HasFieldRVA
without a row. Ordinary nonzero RVA data stays unchanged.

Example preparation (development tools only, not Proton build dependencies):

```sh
ilasm /exe /output:MonoRvaZero-template.exe tests/rva-zero.il
python3 -m venv /tmp/mono-rva-tools
/tmp/mono-rva-tools/bin/pip install dnfile==0.18.0
/tmp/mono-rva-tools/bin/python tests/prepare-rva-fixture.py \
    MonoRvaZero-template.exe MonoRvaZero.exe
```

Run the prepared EXE directly through Wine/mscoree in a separate Mono-only
prefix, not via `mono.exe`, so GetModuleHandle(NULL) identifies the test
assembly. Test x86 and x86_64 builds separately. Exit codes:

- 0: passed.
- 1: ldsflda did not return the loaded executable's image base.
- 2: ldsfld did not read the DOS signature.
- 3: ordinary nonzero RVA data changed.
- 4: RuntimeHelpers.InitializeArray did not read the DOS signature.
- 5: optional missing-row static-storage control failed.

Pass any argument to additionally test Mono's existing missing-row
fallback. Without that argument, the same test can be used as a .NET
Framework comparison. Baseline and patched runtime results are still to
be collected; do not report this fixture as having passed yet.
