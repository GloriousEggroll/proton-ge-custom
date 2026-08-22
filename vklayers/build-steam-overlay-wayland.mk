##
## Steam overlay bridge for Wine Wayland
##

ifeq ($(findstring steam-overlay-wayland,$(WITHOUT_VKLAYERS)),)

STEAM_OVERLAY_WAYLAND_CMAKE_ARGS = \
  -DCMAKE_BUILD_TYPE=release \
  -DVKROOTS_INCLUDE_DIR=$(STEAM_OVERLAY_WAYLAND_SRC)/external/vkroots \
  -DVULKAN_HEADERS_INCLUDE_DIR=$(STEAM_OVERLAY_WAYLAND_SRC)/external/Vulkan-Headers/include

STEAM_OVERLAY_WAYLAND_DEPENDS = libxkbcommon

$(eval $(call rules-source,steam-overlay-wayland,$(SRCDIR)/vklayers/steam-overlay-wayland))
$(eval $(call rules-cmake,steam-overlay-wayland,x86_64,unix))
$(eval $(call rules-cmake,steam-overlay-wayland,aarch64,unix))

$(OBJ)/.steam-overlay-wayland-post-source: dxvk-nvapi-source $(MAKEFILE_LIST)
	mkdir -p $(STEAM_OVERLAY_WAYLAND_SRC)/external/vkroots \
		$(STEAM_OVERLAY_WAYLAND_SRC)/external/Vulkan-Headers
	cp -a $(DXVK_NVAPI_SRC)/external/vkroots/vkroots.h \
		$(STEAM_OVERLAY_WAYLAND_SRC)/external/vkroots/vkroots.h
	rsync -arx $(DXVK_NVAPI_SRC)/external/Vulkan-Headers/include/ \
		$(STEAM_OVERLAY_WAYLAND_SRC)/external/Vulkan-Headers/include/
	touch $@

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
