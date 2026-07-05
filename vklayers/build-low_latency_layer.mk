##
## low_latency_layer
##

ifeq ($(findstring low_latency_layer,$(WITHOUT_VKLAYERS)),)

include $(SRC)/vklayers/build-vulkan-utility-libraries.mk

LOW_LATENCY_LAYER_CMAKE_ARGS = \
  -DCMAKE_BUILD_TYPE=release

LOW_LATENCY_LAYER_DEPENDS = vulkan-headers vulkan-utility-libraries

$(eval $(call rules-source,low_latency_layer,$(SRCDIR)/vklayers/low_latency_layer))
$(eval $(call rules-cmake,low_latency_layer,x86_64,unix))
$(eval $(call rules-cmake,low_latency_layer,aarch64,unix))

$(OBJ)/.low_latency_layer-x86_64-post-build:
	mkdir -p $(DST_DIR)/share/low_latency_layer/implicit_layer.d/
	cp -a $(LOW_LATENCY_LAYER_x86_64_DST)/share/vulkan/implicit_layer.d/low_latency_layer.json $(DST_DIR)/share/low_latency_layer/implicit_layer.d/
	touch $@

$(OBJ)/.low_latency_layer-aarch64-post-build:
	mkdir -p $(DST_DIR)/share/low_latency_layer/implicit_layer.d/
	cp -a $(LOW_LATENCY_LAYER_aarch64_DST)/share/vulkan/implicit_layer.d/low_latency_layer.json $(DST_DIR)/share/low_latency_layer/implicit_layer.d/
	touch $@

all-dist: low_latency_layer

endif # WITHOUT_VKLAYERS
