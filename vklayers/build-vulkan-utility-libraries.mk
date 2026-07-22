##
## vulkan-utility-libraries
##

VULKAN_UTILITY_LIBRARIES_CMAKE_ARGS = \
  -DCMAKE_BUILD_TYPE=release

VULKAN_UTILITY_LIBRARIES_DEPENDS = vulkan-headers

$(eval $(call rules-source,vulkan-utility-libraries,$(SRCDIR)/vklayers/Vulkan-Utility-Libraries))
$(eval $(call rules-cmake,vulkan-utility-libraries,x86_64,unix))
$(eval $(call rules-cmake,vulkan-utility-libraries,aarch64,unix))

## Only use vulkan-utility-libraries to build low_latency_layer; we don't ship it.
$(OBJ)/.vulkan-utility-libraries-x86_64-dist:
	touch $@
$(OBJ)/.vulkan-utility-libraries-aarch64-dist:
	touch $@
