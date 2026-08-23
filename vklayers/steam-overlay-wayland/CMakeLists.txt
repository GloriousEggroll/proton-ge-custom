cmake_minimum_required(VERSION 3.16)

project(steam_overlay_wayland
    VERSION 1.0.0
    LANGUAGES C CXX
)

include(GNUInstallDirs)

set(LSTEAMCLIENT_OVERLAY_BRIDGE_SOURCE_DIR "" CACHE PATH
    "Path to lsteamclient's native Steam overlay bridge")
if(NOT EXISTS "${LSTEAMCLIENT_OVERLAY_BRIDGE_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "LSTEAMCLIENT_OVERLAY_BRIDGE_SOURCE_DIR must contain CMakeLists.txt")
endif()

add_subdirectory(
    "${LSTEAMCLIENT_OVERLAY_BRIDGE_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/lsteamclient-overlay-bridge"
)

set(VULKAN_HEADERS_INCLUDE_DIR "" CACHE PATH "Directory containing Vulkan headers")
if(NOT EXISTS "${VULKAN_HEADERS_INCLUDE_DIR}/vulkan/vulkan.h")
    message(FATAL_ERROR "VULKAN_HEADERS_INCLUDE_DIR must contain vulkan/vulkan.h")
endif()

set(VKROOTS_INCLUDE_DIR "" CACHE PATH "Directory containing vkroots.h")
if(NOT EXISTS "${VKROOTS_INCLUDE_DIR}/vkroots.h")
    message(FATAL_ERROR "VKROOTS_INCLUDE_DIR must contain vkroots.h")
endif()

add_library(VkLayer_GE_Wayland_SteamOverlay SHARED
    src/vulkan_layer.cpp
)

target_compile_features(VkLayer_GE_Wayland_SteamOverlay PRIVATE
    c_std_11
    cxx_std_20
)
target_compile_definitions(VkLayer_GE_Wayland_SteamOverlay PRIVATE _GNU_SOURCE)
target_compile_options(VkLayer_GE_Wayland_SteamOverlay PRIVATE
    -Wall
    -Wextra
    -Wformat=2
    -Wshadow
    $<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes>
)
target_include_directories(VkLayer_GE_Wayland_SteamOverlay PRIVATE
    ${VULKAN_HEADERS_INCLUDE_DIR}
    ${VKROOTS_INCLUDE_DIR}
)
target_link_libraries(VkLayer_GE_Wayland_SteamOverlay PRIVATE
    lsteamclient_overlay_bridge
)
set_target_properties(VkLayer_GE_Wayland_SteamOverlay PROPERTIES
    C_VISIBILITY_PRESET hidden
    INSTALL_RPATH "$ORIGIN"
    VISIBILITY_INLINES_HIDDEN YES
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/VkLayer_GE_wayland_steam_overlay.json.in
    ${CMAKE_CURRENT_BINARY_DIR}/VkLayer_GE_wayland_steam_overlay.json
    @ONLY
)

install(TARGETS VkLayer_GE_Wayland_SteamOverlay
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/VkLayer_GE_wayland_steam_overlay.json
    DESTINATION ${CMAKE_INSTALL_DATADIR}/vulkan/implicit_layer.d
)
