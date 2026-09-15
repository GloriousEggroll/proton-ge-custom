# Warcraft III certificate-chain compatibility fix

This community GE-Proton build fixes the Warcraft III 3.0.0.24268 login failure
that displays "Please check your VPN" when its ClientSdk requests an 88-byte
`CERT_CHAIN_ENGINE_CONFIG` on 64-bit Wine. The pinned Wine revision rejects
that layout with `E_INVALIDARG`, interrupting authentication after TLS connects.

The patch series backports four upstream Wine commits:

- `2012949a0de0b550d221c5514f5632efcd8c3df2`: update the public structure.
- `02bb0a34ad51a7ac4eda1f50d6ac3b7938487185`: trace configuration fields.
- `eef8e97dd335beccb23f3b13d4f4ad715f23f359`: guard access to newer fields.
- `c7cc9be89613cbe21e1af9ffc7b7e8352feac488`: retain the previous layout.

A follow-up preserves `hExclusiveRoot` handling for the older 80-byte layout
and adds guarded-memory and exclusive-root trust regression tests. Supported
64-bit layouts are 64, 80 and 88 bytes; invalid sizes remain rejected.
Certificate validation and game ownership checks remain enabled.

## Using the build

Install `GE-Proton11-WC3-CertFix` alongside your existing compatibility tools.
Close Battle.net, its background agent and Warcraft before switching runners.
Keep the existing Battle.net prefix and run Warcraft from that Battle.net
instance so both use the same Proton build. Switching only a separate Warcraft
shortcut can leave Battle.net running under the previous Wine server.

With Steam, extract the release tarball into Steam's `compatibilitytools.d`
directory, fully restart Steam, and select `GE-Proton11-WC3-CertFix` in the
Battle.net shortcut's compatibility settings. This build declares Steam Linux
Runtime 4 (`appid 4183110`) as its runtime dependency.

For non-Steam launchers, use an umu-enabled setup as described in the upstream
[README](README.md). A direct Proton launcher also worked on the test host, but
requires compatible host libraries; retain its existing prefix and settings.

## Build

Start with a fresh recursive checkout of this fork, Git, GNU Make and a working
Podman installation. The build downloads the SDK container and uses its
compilers. See [README.md](README.md#building) for the GE workflow. From the
checkout root:

```sh
./patches/protonprep-valve-staging.sh > patchlog.txt 2>&1
# Inspect patchlog.txt for failures before proceeding.
mkdir build
cd build
../configure.sh --build-name=GE-Proton11-WC3-CertFix --container-engine=podman
make -j8 redist
```

The tested build used the Steam Runtime 4 SDK image
`registry.gitlab.steamos.cloud/proton/steamrt4/sdk/x86_64:4.0.20260714.251823-0`.
It is based on GE commit `0b8c4d30d76abb74cc5cc4efdbadb410db00455f` and Wine
submodule commit `00e639898e01f8dd6fbee3851bd4c1e412c5b875`.
The distributed binary was built before the packaging commit, so its embedded
version string still refers to that GE base revision.

## Validation

- All five patches apply to the pinned Wine source files in sequence.
- Full GE `make redist` build completed successfully.
- Standalone 64-bit ABI probe: 64/80/88-byte layouts accepted; 96-byte layout
  rejected. The previous runner rejected the 88-byte layout.
- Standalone guarded-memory and exclusive-root trust regression: zero failures.
  The added Wine test exercises these cases; the full crypt32 test suite was
  not run.
- The user confirmed Battle.net and Warcraft III launch and authenticate with
  this build on Arch Linux, using the existing Battle.net prefix.

This is a community build, not an official GE-Proton release. Keep the bundled
licenses and consult the individual source components for their license terms.
