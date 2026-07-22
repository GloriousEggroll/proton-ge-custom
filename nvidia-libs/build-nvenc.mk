##
## nvenc32/nvenc
##

ifeq ($(findstring nvenc,$(WITHOUT_NVIDIA_LIBS)),)

##
## nvenc32
##

ifneq ($(findstring i386,$(unix_ARCHS)),)

NVENC32_i386_MESON_ARGS = \
  --cross-file=$(NVENC32_SRC)/build-wine32.txt \
  --libdir=lib/wine/nvidia-libs/nvenc/i386-windows

NVENC32_DEPENDS = wine
NVENC32_HOST_DEPENDS = wine

$(eval $(call rules-source,nvenc32,$(SRCDIR)/nvidia-libs/nvenc32))
$(eval $(call rules-nvidia-libs,nvenc32,i386,windows))

$(OBJ)/.nvenc32-post-source: $(OBJ)/.wine-wine-requests
	$(foreach i,$(shell find $(NVENC32_SRC)/include/wine -type f | sort),cp $(WINE_SRC)/include/wine/$(notdir $(i)) $(i) &&) true
	mkdir -p $(DST_LIBDIR)/wine/nvidia-libs/nvenc
	rm -rf $(DST_LIBDIR)/wine/nvidia-libs/nvenc/version-i386
	echo "$(shell git -C $(SRCDIR) submodule status -- nvidia-libs/nvenc32)" > $(DST_LIBDIR)/wine/nvidia-libs/nvenc/version-i386
	touch $@

default_pfx: nvenc32

endif # nvenc32


##
## nvenc
##

NVENC_x86_64_MESON_ARGS = \
  --cross-file=$(NVENC_SRC)/build-wine64.txt \
  --libdir=lib/wine/nvidia-libs/nvenc/x86_64-windows

NVENC_DEPENDS = wine
NVENC_HOST_DEPENDS = wine

$(eval $(call rules-source,nvenc,$(SRCDIR)/nvidia-libs/nvenc))
$(eval $(call rules-nvidia-libs,nvenc,x86_64,windows))


$(OBJ)/.nvenc-post-source: $(OBJ)/.wine-wine-requests
	$(foreach i,$(shell find $(NVENC_SRC)/include/wine -type f | sort),cp $(WINE_SRC)/include/wine/$(notdir $(i)) $(i) &&) true
	mkdir -p $(DST_LIBDIR)/wine/nvidia-libs/nvenc
	rm -rf $(DST_LIBDIR)/wine/nvidia-libs/nvenc/version
	echo "$(shell git -C $(SRCDIR) submodule status -- nvidia-libs/nvenc)" > $(DST_LIBDIR)/wine/nvidia-libs/nvenc/version
	touch $@


default_pfx: nvenc

endif # WITHOUT_NVIDIA_LIBS
