##
## pyroveil
##

ifeq ($(findstring pyroveil,$(WITHOUT_VKLAYERS)),)

PYROVEIL_CMAKE_ARGS = \
  -DCMAKE_BUILD_TYPE=release

$(eval $(call rules-source,pyroveil,$(SRCDIR)/vklayers/pyroveil))
$(eval $(call rules-cmake,pyroveil,x86_64,unix))
$(eval $(call rules-cmake,pyroveil,aarch64,unix))

$(OBJ)/.pyroveil-x86_64-post-build:
	mkdir -p $(DST_DIR)/share/pyroveil/implicit_layer.d/
	cat $(PYROVEIL_SRC)/layer/VkLayer_pyroveil.json.in | sed 's|@PYROVEIL_LAYER_PATH@|libVkLayer_pyroveil_64.so|g' > \
		$(PYROVEIL_x86_64_DST)/share/vulkan/implicit_layer.d/VkLayer_pyroveil_64.json
	cp -a $(PYROVEIL_x86_64_DST)/share/vulkan/implicit_layer.d/VkLayer_pyroveil_64.json $(DST_DIR)/share/pyroveil/implicit_layer.d/
	mkdir -p $(DST_DIR)/share/pyroveil/
	rsync --delete -arx $(PYROVEIL_SRC)/hacks $(DST_DIR)/share/pyroveil/
	touch $@

$(OBJ)/.pyroveil-aarch64-post-build:
	mkdir -p $(DST_DIR)/share/pyroveil/implicit_layer.d/
	cat $(PYROVEIL_SRC)/layer/VkLayer_pyroveil.json.in | sed 's|@PYROVEIL_LAYER_PATH@|libVkLayer_pyroveil_64.so|g' > \
		$(PYROVEIL_aarch64_DST)/share/vulkan/implicit_layer.d/VkLayer_pyroveil_64.json
	cp -a $(PYROVEIL_aarch64_DST)/share/vulkan/implicit_layer.d/VkLayer_pyroveil_64.json $(DST_DIR)/share/pyroveil/implicit_layer.d/
	mkdir -p $(DST_DIR)/share/pyroveil/
	rsync --delete -arx $(PYROVEIL_SRC)/hacks $(DST_DIR)/share/pyroveil/
	touch $@

all-dist: pyroveil

endif # WITHOUT_VKLAYERS
