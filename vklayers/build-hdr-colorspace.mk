##
## HDR colorspace layer for Wine Wayland
##

ifeq ($(findstring hdr-colorspace,$(WITHOUT_VKLAYERS)),)

HDR_COLORSPACE_CMAKE_ARGS = \
  -DCMAKE_BUILD_TYPE=release \
  -DVULKAN_HEADERS_INCLUDE_DIR=$(abspath $(SRCDIR))/Vulkan-Headers/include

$(eval $(call rules-source,hdr-colorspace,$(SRCDIR)/vklayers/hdr-colorspace))
# 32-bit games load it too: the manifest names the library without a path (#997)
$(eval $(call rules-cmake,hdr-colorspace,i386,unix))
$(eval $(call rules-cmake,hdr-colorspace,x86_64,unix))
$(eval $(call rules-cmake,hdr-colorspace,aarch64,unix))

$(OBJ)/.hdr-colorspace-x86_64-post-build:
	mkdir -p $(DST_DIR)/share/hdr-colorspace/explicit_layer.d/
	cp -a $(HDR_COLORSPACE_x86_64_DST)/share/vulkan/explicit_layer.d/VkLayer_GE_hdr_colorspace.json \
		$(DST_DIR)/share/hdr-colorspace/explicit_layer.d/
	touch $@

$(OBJ)/.hdr-colorspace-aarch64-post-build:
	mkdir -p $(DST_DIR)/share/hdr-colorspace/explicit_layer.d/
	cp -a $(HDR_COLORSPACE_aarch64_DST)/share/vulkan/explicit_layer.d/VkLayer_GE_hdr_colorspace.json \
		$(DST_DIR)/share/hdr-colorspace/explicit_layer.d/
	touch $@

all-dist: hdr-colorspace

endif # WITHOUT_VKLAYERS
