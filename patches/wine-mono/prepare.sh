#!/bin/bash
set -euo pipefail
export LC_ALL=C

patch_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source "$patch_dir/source.conf"
cd "$patch_dir/../.."

archive="contrib/wine-mono-${WINEMONO_VER}-src.tar.xz"
url="https://github.com/wine-mono/wine-mono/releases/download/wine-mono-${WINEMONO_VER}/${archive##*/}"

if [[ -L wine-mono || -e wine-mono/.git ]]; then
    echo "WINE-MONO: refusing to replace a symlink or Git checkout at wine-mono/" >&2
    exit 1
fi

rm -f -- wine-mono/.proton-prepared
mkdir -p contrib
if [[ ! -f "$archive" ]]; then
    trap 'rm -f -- "$archive.tmp"' EXIT
    wget --no-use-server-timestamps -O "$archive.tmp" "$url"
    mv -- "$archive.tmp" "$archive"
    trap - EXIT
fi

# The release archive includes the pinned submodules. Re-extraction resets
# patched files and removes generated/untracked files without needing Git.
echo "WINE-MONO: reset source to ${WINEMONO_VER} and clean generated files"
rm -rf -- wine-mono
mkdir wine-mono
tar --no-same-owner -xf "$archive" --strip-components=1 -C wine-mono

echo "WINE-MONO: apply native runtime patches"
(
    cd wine-mono/mono
    for patch_file in "$patch_dir"/*.patch; do
        patch --batch --fuzz=0 -Np1 < "$patch_file"
    done
)

# Publish this only after every patch succeeds, so a failed prep cannot build.
touch wine-mono/.proton-prepared
