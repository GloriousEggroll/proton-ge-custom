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


### (2) WINE PATCHING ###

    pushd wine
    git reset --hard HEAD
    git clean -xdf


### (2-5) WINE HOTFIX/BACKPORT SECTION ###
    echo "WINE: -HOTFIX- Fix Smart Tee negotiation and V4L WoW64 media type marshaling"
    apply_all_in_dir "../patches/ge-video-rework/"


    echo "WINE: RUN AUTOCONF TOOLS/MAKE_REQUESTS"
    autoreconf -f
    ./tools/make_requests

    popd



### END PROTON-GE ADDITIONAL CUSTOM PATCHES ###
### END WINE PATCHING ###
