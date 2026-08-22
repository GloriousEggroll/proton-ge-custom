cmake_minimum_required(VERSION 3.16)

project(steam_overlay_wayland
    VERSION 1.0.0
    LANGUAGES C CXX
)

include(GNUInstallDirs)

find_package(Threads REQUIRED)
find_package(X11 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(Wayland REQUIRED IMPORTED_TARGET wayland-client wayland-cursor)
pkg_check_modules(Xkbcommon REQUIRED IMPORTED_TARGET xkbcommon)

find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner)
if(NOT WAYLAND_SCANNER_EXECUTABLE)
    message(FATAL_ERROR "wayland-scanner is required")
endif()

set(PROTOCOL_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/protocols")
set(RELATIVE_POINTER_PROTOCOL_XML
    "${CMAKE_CURRENT_SOURCE_DIR}/protocols/relative-pointer-unstable-v1.xml")
set(RELATIVE_POINTER_PROTOCOL_HEADER
    "${PROTOCOL_GENERATED_DIR}/relative-pointer-unstable-v1-client-protocol.h")
set(RELATIVE_POINTER_PROTOCOL_CODE
    "${PROTOCOL_GENERATED_DIR}/relative-pointer-unstable-v1-protocol.c")

file(MAKE_DIRECTORY "${PROTOCOL_GENERATED_DIR}")
add_custom_command(
    OUTPUT "${RELATIVE_POINTER_PROTOCOL_HEADER}"
    COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" client-header
            "${RELATIVE_POINTER_PROTOCOL_XML}"
            "${RELATIVE_POINTER_PROTOCOL_HEADER}"
    DEPENDS "${RELATIVE_POINTER_PROTOCOL_XML}"
    VERBATIM
)
add_custom_command(
    OUTPUT "${RELATIVE_POINTER_PROTOCOL_CODE}"
    COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" private-code
            "${RELATIVE_POINTER_PROTOCOL_XML}"
            "${RELATIVE_POINTER_PROTOCOL_CODE}"
    DEPENDS "${RELATIVE_POINTER_PROTOCOL_XML}"
    VERBATIM
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
    ${RELATIVE_POINTER_PROTOCOL_CODE}
    ${RELATIVE_POINTER_PROTOCOL_HEADER}
    src/focus_proxy.c
    src/steam_overlay_bridge.c
    src/vulkan_layer.cpp
    src/wayland_input.c
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
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROTOCOL_GENERATED_DIR}
    ${VULKAN_HEADERS_INCLUDE_DIR}
    ${VKROOTS_INCLUDE_DIR}
    ${X11_INCLUDE_DIR}
)
target_link_libraries(VkLayer_GE_Wayland_SteamOverlay PRIVATE
    PkgConfig::Wayland
    PkgConfig::Xkbcommon
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
