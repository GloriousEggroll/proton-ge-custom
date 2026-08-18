##
## Steam overlay bridge for Wine Wayland
##

ifeq ($(findstring steam-overlay-wayland,$(WITHOUT_VKLAYERS)),)

STEAM_OVERLAY_WAYLAND_CMAKE_ARGS = \
  -DCMAKE_BUILD_TYPE=release

STEAM_OVERLAY_WAYLAND_DEPENDS = vulkan-headers

$(eval $(call rules-source,steam-overlay-wayland,$(SRCDIR)/vklayers/steam-overlay-wayland))
$(eval $(call rules-cmake,steam-overlay-wayland,x86_64,unix))
$(eval $(call rules-cmake,steam-overlay-wayland,aarch64,unix))

$(OBJ)/.steam-overlay-wayland-x86_64-post-build:
	mkdir -p $(DST_DIR)/share/steam-overlay-wayland/implicit_layer.d/
	cp -a $(STEAM_OVERLAY_WAYLAND_x86_64_DST)/share/vulkan/implicit_layer.d/VkLayer_GE_wayland_steam_overlay.json \
		$(DST_DIR)/share/steam-overlay-wayland/implicit_layer.d/
	touch $@

$(OBJ)/.steam-overlay-wayland-aarch64-post-build:
	mkdir -p $(DST_DIR)/share/steam-overlay-wayland/implicit_layer.d/
	cp -a $(STEAM_OVERLAY_WAYLAND_aarch64_DST)/share/vulkan/implicit_layer.d/VkLayer_GE_wayland_steam_overlay.json \
		$(DST_DIR)/share/steam-overlay-wayland/implicit_layer.d/
	touch $@

all-dist: steam-overlay-wayland

endif # WITHOUT_VKLAYERS
