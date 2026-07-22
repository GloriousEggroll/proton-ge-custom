##
## vkbasalt
##

ifeq ($(findstring vkbasalt,$(WITHOUT_VKLAYERS)),)

VKBASALT_MESON_ARGS = \
  -Dwith_so=true \
  -Dwith_json=true \
  -Dappend_libdir_vkbasalt=false

VKBASALT_DEPENDS = vulkan-headers spirv-headers

$(eval $(call rules-source,vkbasalt,$(SRCDIR)/vklayers/vkBasalt))
$(eval $(call rules-meson,vkbasalt,i386,unix))
$(eval $(call rules-meson,vkbasalt,x86_64,unix))
$(eval $(call rules-meson,vkbasalt,aarch64,unix))

$(OBJ)/.vkbasalt-x86_64-post-build:
	mkdir -p $(DST_DIR)/share/vkbasalt/implicit_layer.d/
	cp -a $(VKBASALT_x86_64_DST)/share/vulkan/implicit_layer.d/vkBasalt.json $(DST_DIR)/share/vkbasalt/implicit_layer.d/
	touch $@

$(OBJ)/.vkbasalt-aarch64-post-build:
	mkdir -p $(DST_DIR)/share/vkbasalt/implicit_layer.d/
	cp -a $(VKBASALT_aarch64_DST)/share/vulkan/implicit_layer.d/vkBasalt.json $(DST_DIR)/share/vkbasalt/implicit_layer.d/
	touch $@

all-dist: vkbasalt

endif # WITHOUT_VKLAYERS
