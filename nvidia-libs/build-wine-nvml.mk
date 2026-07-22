##
## wine-nvml
##

ifeq ($(findstring nvml,$(WITHOUT_NVIDIA_LIBS)),)


NVML_SO_x86_64_MESON_ARGS = \
  --cross-file=$(NVML_SO_SRC)/cross-wine64.txt \
  --libdir=lib/wine/nvidia-libs/nvml

NVML_DLL_x86_64_MESON_ARGS = \
  --cross-file=$(NVML_DLL_SRC)/cross-mingw64.txt \
  --libdir=lib/wine/nvidia-libs/nvml

NVML_SO_DEPENDS = wine
NVML_SO_HOST_DEPENDS = wine
NVML_DLL_DEPENDS = wine
NVML_DLL_HOST_DEPENDS = wine

$(eval $(call rules-source,nvml_so,$(SRCDIR)/nvidia-libs/wine-nvml))
$(eval $(call rules-nvidia-libs,nvml_so,x86_64,unix))
$(eval $(call rules-source,nvml_dll,$(SRCDIR)/nvidia-libs/wine-nvml))
$(eval $(call rules-nvidia-libs,nvml_dll,x86_64,windows))

$(OBJ)/.nvml_so-post-source:
	cd $(NVML_SO_SRC)/src && ./make_nvml
	mkdir -p $(DST_LIBDIR)/wine/nvidia-libs/nvml
	rm -rf $(DST_LIBDIR)/wine/nvidia-libs/nvml/version
	echo "$(shell git -C $(SRCDIR) submodule status -- nvidia-libs/wine-nvml)" > $(DST_LIBDIR)/wine/nvidia-libs/nvml/version
	touch $@

$(OBJ)/.nvml_dll-post-source:
	cd $(NVML_DLL_SRC)/src && ./make_nvml
	mkdir -p $(DST_LIBDIR)/wine/nvidia-libs/nvml
	rm -rf $(DST_LIBDIR)/wine/nvidia-libs/nvml/version
	echo "$(shell git -C $(SRCDIR) submodule status -- nvidia-libs/wine-nvml)" > $(DST_LIBDIR)/wine/nvidia-libs/nvml/version
	touch $@

default_pfx: nvml_so nvml_dll

endif # WITHOUT_NVIDIA_LIBS
