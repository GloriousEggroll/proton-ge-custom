##
## wine-nvoptix
##

ifeq ($(findstring nvoptix,$(WITHOUT_NVIDIA_LIBS)),)


NVOPTIX_x86_64_MESON_ARGS = \
  --cross-file=$(NVOPTIX_SRC)/build-wine64.txt \
  --libdir=lib/wine/nvidia-libs/nvoptix/x86_64-windows

NVOPTIX_DEPENDS = wine
NVOPTIX_HOST_DEPENDS = wine

$(eval $(call rules-source,nvoptix,$(SRCDIR)/nvidia-libs/wine-nvoptix))
$(eval $(call rules-nvidia-libs,nvoptix,x86_64,windows))

$(OBJ)/.nvoptix-post-source:
	mkdir -p $(DST_LIBDIR)/wine/nvidia-libs/nvoptix
	rm -rf $(DST_LIBDIR)/wine/nvidia-libs/nvoptix/version
	echo "$(shell git -C $(SRCDIR) submodule status -- nvidia-libs/wine-nvoptix)" > $(DST_LIBDIR)/wine/nvidia-libs/nvoptix/version
	touch $@

default_pfx: nvoptix

endif # WITHOUT_NVIDIA_LIBS
