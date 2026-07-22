##
## dxvk-nvapi
##

ifeq ($(findstring dxvk-nvapi,$(WITHOUT_NVIDIA_LIBS)),)

NVLIBS_NVAPI_MESON_ARGS = -Db_ndebug=true
NVLIBS_NVAPI_i386_MESON_ARGS = --bindir=$(NVLIBS_NVAPI_i386_DST)/lib/wine/nvidia-libs/nvapi/i386-windows
NVLIBS_NVAPI_x86_64_MESON_ARGS = --bindir=$(NVLIBS_NVAPI_x86_64_DST)/lib/wine/nvidia-libs/nvapi/x86_64-windows

NVLIBS_NVAPI_i386_CFLAGS = -O3 $(i386_SANITY_FLAGS)
NVLIBS_NVAPI_x86_64_CFLAGS = -O3
NVLIBS_NVAPI_CPPFLAGS = -msse -msse2
NVLIBS_NVAPI_LDFLAGS = -static -static-libgcc -static-libstdc++

$(eval $(call rules-source,nvlibs-nvapi,$(SRCDIR)/nvidia-libs/dxvk-nvapi))
$(eval $(call rules-meson,nvlibs-nvapi,i386,windows))
$(eval $(call rules-meson,nvlibs-nvapi,x86_64,windows))

$(OBJ)/.nvlibs-nvapi-post-source:
	mkdir -p $(DST_LIBDIR)/wine/nvidia-libs/nvapi
	rm -rf $(DST_LIBDIR)/wine/nvidia-libs/nvapi/version
	echo "$(shell git -C $(SRCDIR) submodule status -- nvidia-libs/dxvk-nvapi)" > $(DST_LIBDIR)/wine/nvidia-libs/nvapi/version
	touch $@

default_pfx: nvlibs-nvapi

endif # WITHOUT_NVIDIA_LIBS
