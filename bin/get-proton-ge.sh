#!/bin/bash
# Author Michael DeGuzis
# Description: Simple script to grab the latest Proton GE without a git clone / build.

steam_type=$1
native=1
flatpak=1
steamos=1

if [[ -z ${steam_type} ]]; then
	echo "ERROR: Steam type required as arg1! One of: native, flatpak, steamos"
fi

case ${steam_type} in
	flatpak)
		flatpak=0
		;;
	native)
		native=0
		;;
	steamos)
		steamos=0
		;;
	*)
		echo "ERROR: Unsupported type: \"${steam_type}\""
		exit 1	
		;;
esac

GIT_REPO="GloriousEggroll/proton-ge-custom"
GIT_URL="https://github.com/${GIT_REPO}"
VERSION=$(curl --silent "https://api.github.com/repos/${GIT_REPO}/releases/latest" | grep '"tag_name":' |  sed -E 's/.*"([^"]+)".*/\1/')
FILE="Proton-${VERSION}.tar.gz"
FOLDER="Proton-${VERSION}"
URL="${GIT_URL}/releases/download/${VERSION}/${FILE}"

# Ensure proper user if running SteamOS
THIS_USER=$(whoami)
if [[ ${steamos} -eq 0 ]] && [[ ${USER} != "steam" ]]; then
    echo "ERROR: Please run this script as user 'steam'"
    exit 1
fi

# Check for Steam directory root
# Avoid any symlinked path jankiness
# Do not quote path (issues on some systems such as SteamOS)
if [[ ${flatpak} -eq 0 ]]; then
	steam_root="${HOME}/.var/app/com.valvesoftware.Steam/data/Steam"
else
	steam_root="${HOME}/.steam/root"
fi

steam_root_abs_path=$(readlink -f ${steam_root})
if [[ ! -d ${steam_root_abs_path} ]]; then
    echo "ERROR: Steam root does not exist! Expected path: ${steam_root}"
    exit 1
fi

# Check for compat folder
if [[ ! -d "${steam_root_abs_path}/compatibilitytools.d" ]]; then
    echo "WARNING: compatibilitytools.d folder mising, creating..."
    if ! mkdir -p "${steam_root_abs_path}/compatibilitytools.d"; then
        echo "ERROR: Could not create compatibilitytools.d in Steam root: ${steam_root_abs_path}"
        exit 1
    fi
fi
target_abs_path=$(readlink -f "${steam_root}/compatibilitytools.d")

if [[ ! -d ${target_abs_path} ]]; then
    echo "ERROR: Could not validate target absolute path to: ${steam_root}/compatibilitytools.d"
    exit 1
fi

# Download
echo "Downloading ${FILE}"
if [[ ! -f ${FILE} ]]; then
	curl -L -O ${URL}
else
	echo ${FILE} already exists!
fi

if [[ -d ${target_abs_path}/${FOLDER} ]]; then
	echo "${FOLDER} already exists in target_abs_path ${target_abs_path}!"
    read -erp "Reset installation? (y/N): " reset_install
    if [[ ${reset_install} == "y" || ${reset_install} == "Y" ]]; then
        rm -r ${target_abs_path}
    else
        exit 1
    fi
fi

echo "Extracting Proton GE to target_abs_path: ${target_abs_path}"
if ! tar -xf ${FILE} -C "${target_abs_path}"; then
    echo "ERROR: Could not extact contents of ${FILE}"
    exit 1
fi

echo "${FOLDER} Installed to ${target_abs_path}!"
ls -la ${target_abs_path}
