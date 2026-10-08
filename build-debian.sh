#!/bin/bash
#
# Build GE-Proton from this checkout on Debian (tested target: Debian 14 "forky").
#
# The actual compile happens inside Valve's Steam Runtime SDK container, exactly
# like the official build; this script only prepares the Debian host for it:
#
#   1. installs the host tools (git, patch, autoconf, perl, python3, wget, ...)
#      and a container engine (rootless podman by default, or docker)
#   2. fetches all git submodules
#   3. applies the GE patch set (patches/protonprep-valve-staging.sh) and
#      stops if any patch fails
#   4. runs configure.sh + `make redist` in a build directory
#   5. installs the resulting tarball into Steam's compatibilitytools.d
#
# Usage:
#   git clone --recurse-submodules <this repo> proton-ge-custom
#   cd proton-ge-custom
#   ./build-debian.sh [options]
#
# Run it as your normal user (not root); it calls sudo only for apt.
# Expect several hours of build time, ~80 GB of free disk and 16 GB of RAM.

set -euo pipefail

SRC="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

# Defaults
ENGINE="podman"
BUILD_NAME=""
BUILD_DIR="$SRC/build"
JOBS=""
SKIP_DEPS=0
SKIP_SUBMODULES=0
SKIP_PATCH=0
DO_INSTALL=1
USE_CCACHE=0
ALLOW_ROOT=0

if [[ -t 2 ]]; then
    C_ERR=$'\e[31;1m'; C_OK=$'\e[32;1m'; C_WARN=$'\e[33;1m'; C_CLR=$'\e[0m'
else
    C_ERR=""; C_OK=""; C_WARN=""; C_CLR=""
fi
step() { echo >&2 "${C_OK}==>${C_CLR} $*"; }
warn() { echo >&2 "${C_WARN}warning:${C_CLR} $*"; }
die()  { echo >&2 "${C_ERR}error:${C_CLR} $*"; exit 1; }

usage() {
    cat <<EOF
Usage: $0 [options]

Options:
  --engine=podman|docker  Container engine to build with (default: podman)
  --name=NAME             Build name shown in Steam (default: \$(cat VERSION)-local)
  --build-dir=DIR         Out-of-tree build directory (default: ./build)
  --jobs=N                Parallel jobs (default: nproc, capped by RAM)
  --ccache                Enable ccache (speeds up rebuilds, uses ~/.ccache)
  --skip-deps             Don't apt-install host packages
  --skip-submodules       Don't run 'git submodule update'
  --skip-patch            Don't (re)apply the GE patch set
  --no-install            Only produce the tarball, don't install it into Steam
  --allow-root            Allow running as root (not recommended)
  -h, --help              Show this help
EOF
}

for arg in "$@"; do
    case "$arg" in
        --engine=*)        ENGINE="${arg#*=}" ;;
        --name=*)          BUILD_NAME="${arg#*=}" ;;
        --build-dir=*)     BUILD_DIR="${arg#*=}" ;;
        --jobs=*)          JOBS="${arg#*=}" ;;
        --ccache)          USE_CCACHE=1 ;;
        --skip-deps)       SKIP_DEPS=1 ;;
        --skip-submodules) SKIP_SUBMODULES=1 ;;
        --skip-patch)      SKIP_PATCH=1 ;;
        --no-install)      DO_INSTALL=0 ;;
        --allow-root)      ALLOW_ROOT=1 ;;
        -h|--help)         usage; exit 0 ;;
        *)                 usage >&2; die "unknown option: $arg" ;;
    esac
done

[[ "$ENGINE" == podman || "$ENGINE" == docker ]] || die "--engine must be podman or docker"
[[ -z "$JOBS" || "$JOBS" =~ ^[1-9][0-9]*$ ]] || die "--jobs must be a positive integer"

if [[ $EUID -eq 0 && $ALLOW_ROOT -eq 0 ]]; then
    die "run this as your normal user (it uses sudo for apt), or pass --allow-root"
fi

SUDO=""
[[ $EUID -ne 0 ]] && SUDO="sudo"

#
# Sanity checks
#

if [[ -r /etc/os-release ]]; then
    # shellcheck disable=SC1091
    . /etc/os-release
    if [[ "${ID:-}" != debian && " ${ID_LIKE:-} " != *" debian "* ]]; then
        warn "this script targets Debian; detected '${PRETTY_NAME:-unknown}'. Continuing anyway."
    else
        step "Host: ${PRETTY_NAME:-Debian}"
    fi
fi

[[ "$(uname -m)" == x86_64 ]] || die "building requires an x86_64 host (found $(uname -m))"
[[ -f "$SRC/configure.sh" && -f "$SRC/patches/protonprep-valve-staging.sh" ]] \
    || die "$SRC does not look like a proton-ge-custom checkout"
[[ -d "$SRC/.git" || -f "$SRC/.git" ]] || die "$SRC is not a git checkout; clone it with git first"

mkdir -p "$BUILD_DIR"
BUILD_DIR="$(cd -- "$BUILD_DIR" && pwd)"
[[ "$BUILD_DIR" != "$SRC" ]] || die "--build-dir must not be the source directory itself"

free_gb=$(df -P -BG "$BUILD_DIR" | awk 'NR==2 {gsub("G","",$4); print $4}')
if (( free_gb < 80 )); then
    warn "only ${free_gb} GB free under $BUILD_DIR; a full build typically needs ~80 GB."
fi

mem_gb=$(awk '/^MemTotal:/ {print int($2/1024/1024)}' /proc/meminfo)
if [[ -z "$JOBS" ]]; then
    JOBS=$(nproc)
    # Roughly 2 GB per job keeps the linker steps from OOMing.
    max_jobs=$(( mem_gb / 2 ))
    (( max_jobs < 1 )) && max_jobs=1
    (( JOBS > max_jobs )) && JOBS=$max_jobs
fi
step "Using $JOBS parallel jobs (${mem_gb} GB RAM, $(nproc) CPUs)"

if [[ -z "$BUILD_NAME" ]]; then
    BUILD_NAME="$(tr -d '[:space:]' < "$SRC/VERSION")-local"
fi
[[ "$BUILD_NAME" =~ ^[A-Za-z0-9._-]+$ ]] || die "--name may only contain letters, digits, '.', '_' and '-'"
step "Build name: $BUILD_NAME"

#
# 1. Host dependencies
#

if (( SKIP_DEPS == 0 )); then
    step "Installing host packages (sudo apt-get)"
    pkgs=(git make patch wget ca-certificates tar xz-utils gzip
          autoconf automake perl python3 rsync coreutils)
    if [[ "$ENGINE" == podman ]]; then
        pkgs+=(podman uidmap)
    else
        pkgs+=(docker.io)
    fi
    $SUDO apt-get update
    $SUDO apt-get install -y --no-install-recommends "${pkgs[@]}"
    if [[ "$ENGINE" == podman ]]; then
        # Rootless networking/storage helpers; names vary a bit between releases,
        # so install whichever are available.
        for opt in passt slirp4netns fuse-overlayfs containers-storage; do
            $SUDO apt-get install -y --no-install-recommends "$opt" >/dev/null 2>&1 \
                || warn "optional package '$opt' not available, skipping"
        done
    fi
fi

for tool in git make patch wget autoreconf perl python3 tar xz "$ENGINE"; do
    command -v "$tool" >/dev/null 2>&1 || die "'$tool' not found; install it or drop --skip-deps"
done

#
# 2. Container engine setup
#

if [[ "$ENGINE" == podman ]]; then
    user="$(id -un)"
    if (( EUID != 0 )) && ! grep -q "^${user}:" /etc/subuid 2>/dev/null; then
        step "Adding subordinate UID/GID ranges for $user (needed by rootless podman)"
        $SUDO usermod --add-subuids 100000-165535 --add-subgids 100000-165535 "$user"
        podman system migrate >/dev/null 2>&1 || true
    fi
else
    if ! docker info >/dev/null 2>&1; then
        $SUDO systemctl enable --now docker >/dev/null 2>&1 || true
        if ! docker info >/dev/null 2>&1; then
            if (( EUID != 0 )) && ! id -nG | tr ' ' '\n' | grep -qx docker; then
                $SUDO usermod -aG docker "$(id -un)"
                die "added $(id -un) to the 'docker' group; log out and back in (or run 'newgrp docker'), then re-run this script"
            fi
            die "cannot talk to the docker daemon; check 'docker info'"
        fi
    fi
fi

STEAMRT_IMAGE="$(make --silent "SRCDIR=$SRC" --file "$SRC/Makefile.in" TARGET_ARCH=x86_64 get-steamrt-image)"
step "Pulling Steam Runtime SDK image: $STEAMRT_IMAGE"
"$ENGINE" pull "$STEAMRT_IMAGE"

#
# 3. Sources
#

cd "$SRC"

if (( SKIP_SUBMODULES == 0 )); then
    step "Fetching git submodules (first run downloads several GB)"
    git submodule sync --recursive
    git submodule update --init --recursive --jobs 4
fi

if git submodule status --recursive | grep -q '^-'; then
    die "some submodules are not checked out; run without --skip-submodules"
fi

#
# 4. Apply the GE patch set
#

if (( SKIP_PATCH == 0 )); then
    step "Applying GE patches (log: $SRC/patchlog.txt)"
    # The prep script resets each patched tree before patching, so re-running is safe.
    ./patches/protonprep-valve-staging.sh > patchlog.txt 2>&1 || true

    fatal_re='Hunk #[0-9]+ FAILED|can.t find file to patch|malformed patch|^error: |patch: \*\*\*|Traceback \(most recent call last\)'
    if grep -E -n "$fatal_re" patchlog.txt >&2; then
        die "patching failed; see the lines above and $SRC/patchlog.txt"
    fi
    rejects=$(find . \( -path ./build -o -path './obj-*' \) -prune -o -name '*.rej' -print 2>/dev/null | head -20)
    if [[ -n "$rejects" ]]; then
        echo >&2 "$rejects"
        die "patching left .rej files behind; see $SRC/patchlog.txt"
    fi
    if grep -q 'Reversed (or previously applied)' patchlog.txt; then
        warn "some patches were already applied/reversed; check patchlog.txt if the build misbehaves"
    fi
    [[ -f wine-mono/.proton-prepared ]] || die "wine-mono was not prepared; see $SRC/patchlog.txt"
fi

#
# 5. Configure and build
#

cd "$BUILD_DIR"

configure_args=(--build-name="$BUILD_NAME" --container-engine="$ENGINE")
step "Configuring in $BUILD_DIR"
"$SRC/configure.sh" "${configure_args[@]}"

# `make redist` renames redist/ to $BUILD_NAME/, which breaks if a previous
# result with the same name is still there.
rm -rf -- "./$BUILD_NAME" "./$BUILD_NAME.tar.gz" "./$BUILD_NAME.sha512sum" ./redist

make_args=(-j"$JOBS")
(( USE_CCACHE )) && make_args+=(ENABLE_CCACHE=1)

step "Building (this takes hours; log: $BUILD_DIR/build.log)"
start=$SECONDS
if ! make "${make_args[@]}" redist > build.log 2>&1; then
    echo >&2 "---- last 40 lines of build.log ----"
    tail -n 40 build.log >&2
    die "build failed; full log in $BUILD_DIR/build.log"
fi
elapsed=$(( SECONDS - start ))
TARBALL="$BUILD_DIR/$BUILD_NAME.tar.gz"
[[ -f "$TARBALL" ]] || die "build finished but $TARBALL is missing; see build.log"
step "Built $TARBALL in $(( elapsed / 3600 ))h $(( elapsed % 3600 / 60 ))m"

#
# 6. Install into Steam
#

if (( DO_INSTALL )); then
    steam_root=""
    for candidate in "$HOME/.steam/root" "$HOME/.steam/steam" "$HOME/.local/share/Steam" \
                     "$HOME/.var/app/com.valvesoftware.Steam/data/Steam"; do
        if [[ -d "$candidate" ]]; then
            steam_root="$(cd -- "$candidate" && pwd -P)"
            break
        fi
    done

    if [[ -z "$steam_root" ]]; then
        warn "no Steam directory found; start Steam once, then install manually with:"
        echo "  mkdir -p ~/.steam/root/compatibilitytools.d"
        echo "  tar -xf '$TARBALL' -C ~/.steam/root/compatibilitytools.d/"
    else
        dest="$steam_root/compatibilitytools.d"
        mkdir -p "$dest"
        rm -rf -- "${dest:?}/$BUILD_NAME"
        tar -xf "$TARBALL" -C "$dest"
        step "Installed to $dest/$BUILD_NAME"
        echo "Restart Steam, then pick '$BUILD_NAME' under a game's"
        echo "Properties -> Compatibility -> 'Force the use of a specific Steam Play compatibility tool'."
    fi
fi
