##
## nvcuda32/nvcuda
##

ifeq ($(findstring nvcuda,$(WITHOUT_NVIDIA_LIBS)),)

##
## nvcuda32
##

ifneq ($(findstring i386,$(unix_ARCHS)),)

NVCUDA32_i386_MESON_ARGS = \
  --cross-file=$(NVCUDA32_SRC)/build-wine32.txt \
  --libdir=lib/wine/nvidia-libs/nvcuda/i386-windows

NVCUDA32_DEPENDS = wine
NVCUDA32_HOST_DEPENDS = wine

$(eval $(call rules-source,nvcuda32,$(SRCDIR)/nvidia-libs/nvcuda32))
$(eval $(call rules-nvidia-libs,nvcuda32,i386,windows))

$(OBJ)/.nvcuda32-post-source: $(OBJ)/.wine-wine-requests
	$(foreach i,$(shell find $(NVCUDA32_SRC)/include/wine -type f | sort),cp $(WINE_SRC)/include/wine/$(notdir $(i)) $(i) &&) true
	mkdir -p $(DST_LIBDIR)/wine/nvidia-libs/nvcuda
	rm -rf $(DST_LIBDIR)/wine/nvidia-libs/nvcuda/version-i386
	echo "$(shell git -C $(SRCDIR) submodule status -- nvidia-libs/nvcuda32)" > $(DST_LIBDIR)/wine/nvidia-libs/nvcuda/version-i386
	touch $@

default_pfx: nvcuda32

endif # nvcuda32


##
## nvcuda
##

NVCUDA_x86_64_MESON_ARGS = \
  --cross-file=$(NVCUDA_SRC)/build-wine64.txt \
  --libdir=lib/wine/nvidia-libs/nvcuda/x86_64-windows

NVCUDA_DEPENDS = wine
NVCUDA_HOST_DEPENDS = wine

$(eval $(call rules-source,nvcuda,$(SRCDIR)/nvidia-libs/nvcuda))
$(eval $(call rules-nvidia-libs,nvcuda,x86_64,windows))

$(OBJ)/.nvcuda-post-source: $(OBJ)/.wine-wine-requests
	$(foreach i,$(shell find $(NVCUDA_SRC)/include/wine -type f | sort),cp $(WINE_SRC)/include/wine/$(notdir $(i)) $(i) &&) true
	mkdir -p $(DST_LIBDIR)/wine/nvidia-libs/nvcuda
	rm -rf $(DST_LIBDIR)/wine/nvidia-libs/nvcuda/version
	echo "$(shell git -C $(SRCDIR) submodule status -- nvidia-libs/nvcuda)" > $(DST_LIBDIR)/wine/nvidia-libs/nvcuda/version
	touch $@

default_pfx: nvcuda

endif # WITHOUT_NVIDIA_LIBS
