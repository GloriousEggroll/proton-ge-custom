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

# Download
echo "Downloading ${FILE}"
if [[ ! -f ${FILE} ]]; then
	curl -L -O ${URL}
else
	echo ${FILE} already exists!
fi

# Prepare dir(s)
# Avoid any symlinked path jankiness
# Do not quote path (issues on some systems such as SteamOS)
if [[ ${flatpak} -eq 0 ]]; then
	target=$(readlink -f ~/.var/app/com.valvesoftware.Steam/data/Steam/compatibilitytools.d)
else
	target=$(readlink -f ~/.steam/root/compatibilitytools.d)
fi

mkdir -pv ${target}
if [[ ! -d ${target}/${FOLDER} ]]; then
	echo "Extracting Proton GE to target: ${target}"
	tar -xf ${FILE} -C ~/.steam/root/compatibilitytools.d

elif [[ -d ${target}/${FOLDER} ]]; then
	echo "${FOLDER} already exists in target ${target}!"
	exit 1
fi

echo "${FOLDER} Installed to ${target}!"
ls -la ${target}
