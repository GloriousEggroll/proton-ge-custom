cmake_minimum_required(VERSION 3.16)

project(steam_overlay_wayland
    VERSION 1.0.0
    LANGUAGES C
)

include(GNUInstallDirs)

find_package(Threads REQUIRED)
find_package(VulkanHeaders CONFIG REQUIRED)
find_package(X11 REQUIRED)

add_library(VkLayer_GE_Wayland_SteamOverlay SHARED
    src/steam_overlay_bridge.c
    src/vulkan_layer.c
)

target_compile_features(VkLayer_GE_Wayland_SteamOverlay PRIVATE c_std_11)
target_compile_definitions(VkLayer_GE_Wayland_SteamOverlay PRIVATE _GNU_SOURCE)
target_compile_options(VkLayer_GE_Wayland_SteamOverlay PRIVATE
    -Wall
    -Wextra
    -Wformat=2
    -Wshadow
    -Wstrict-prototypes
)
target_include_directories(VkLayer_GE_Wayland_SteamOverlay PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${X11_INCLUDE_DIR}
)
target_link_libraries(VkLayer_GE_Wayland_SteamOverlay PRIVATE
    Vulkan::Headers
    Threads::Threads
    ${X11_LIBRARIES}
    ${CMAKE_DL_LIBS}
    m
)
set_target_properties(VkLayer_GE_Wayland_SteamOverlay PROPERTIES
    C_VISIBILITY_PRESET hidden
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
