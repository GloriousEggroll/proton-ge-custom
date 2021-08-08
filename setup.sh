#!/bin/bash

if (! whiptail --title "Proton GE Setup" --yesno "This tool assumes you are running latest Ubuntu or similar. Are you?" 8 78); then
  echo "Sorry, currently we only support Ubuntu, but there are plans to add more distros in the futture."
  exit 1
fi

if (whiptail --title "Proton GE Setup" --yesno "What kind of video card do you have?" 8 78 --no-button "AMD" --yes-button "NVidia"); then
  CARD=NVIDIA
else
  CARD=AMD
fi

echo "Ok, I will install Proton GE, and drivers for ${CARD}. Basically, say yes/hit enter to any questions below."

sudo dpkg --add-architecture i386
wget -nc https://dl.winehq.org/wine-builds/winehq.key
sudo apt-key add winehq.key
sudo apt-add-repository 'https://dl.winehq.org/wine-builds/ubuntu/'
sudo apt install -y --install-recommends winehq-staging
sudo apt install -y winetricks flatpak

flatpak install com.valvesoftware.Steam.CompatibilityTool.Proton-GE

sudo apt update && sudo apt upgrade

if [ "${CARD}" == "NVIDIA" ];then
  sudo add-apt-repository ppa:graphics-drivers/ppa
  sudo apt install -y nvidia-driver-465 libvulkan1 libvulkan1:i386
else
  sudo add-apt-repository ppa:kisak/kisak-mesa
  sudo apt install -y mesa-vulkan-drivers mesa-vulkan-drivers:i386 libgl1-mesa-dri:i386
fi

echo "You will need to reboot for the changes to take effect. Afterwards, you should be able to select Proton GE, in steam compatability layer: https://github.com/GloriousEggroll/proton-ge-custom#enabling"
