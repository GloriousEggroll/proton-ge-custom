cmake_minimum_required(VERSION 3.16)

include(GNUInstallDirs)

find_package(Threads REQUIRED)
find_package(X11 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
if(NOT X11_Xfixes_FOUND)
    message(FATAL_ERROR "Xfixes is required")
endif()
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
set(FRACTIONAL_SCALE_PROTOCOL_XML
    "${CMAKE_CURRENT_SOURCE_DIR}/protocols/fractional-scale-v1.xml")
set(FRACTIONAL_SCALE_PROTOCOL_HEADER
    "${PROTOCOL_GENERATED_DIR}/fractional-scale-v1-client-protocol.h")
set(FRACTIONAL_SCALE_PROTOCOL_CODE
    "${PROTOCOL_GENERATED_DIR}/fractional-scale-v1-protocol.c")

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
add_custom_command(
    OUTPUT "${FRACTIONAL_SCALE_PROTOCOL_HEADER}"
    COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" client-header
            "${FRACTIONAL_SCALE_PROTOCOL_XML}"
            "${FRACTIONAL_SCALE_PROTOCOL_HEADER}"
    DEPENDS "${FRACTIONAL_SCALE_PROTOCOL_XML}"
    VERBATIM
)
add_custom_command(
    OUTPUT "${FRACTIONAL_SCALE_PROTOCOL_CODE}"
    COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" private-code
            "${FRACTIONAL_SCALE_PROTOCOL_XML}"
            "${FRACTIONAL_SCALE_PROTOCOL_CODE}"
    DEPENDS "${FRACTIONAL_SCALE_PROTOCOL_XML}"
    VERBATIM
)

add_library(lsteamclient_overlay_bridge SHARED
    ${FRACTIONAL_SCALE_PROTOCOL_CODE}
    ${FRACTIONAL_SCALE_PROTOCOL_HEADER}
    ${RELATIVE_POINTER_PROTOCOL_CODE}
    ${RELATIVE_POINTER_PROTOCOL_HEADER}
    src/focus_proxy.c
    src/steam_overlay_bridge.c
    src/wayland_input.c
)

target_compile_features(lsteamclient_overlay_bridge PRIVATE c_std_11)
target_compile_definitions(lsteamclient_overlay_bridge PRIVATE _GNU_SOURCE)
target_compile_options(lsteamclient_overlay_bridge PRIVATE
    -Wall
    -Wextra
    -Wformat=2
    -Wshadow
    -Wstrict-prototypes
)
target_include_directories(lsteamclient_overlay_bridge
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${PROTOCOL_GENERATED_DIR}
        ${X11_INCLUDE_DIR}
)
target_link_libraries(lsteamclient_overlay_bridge
    PUBLIC PkgConfig::Wayland
    PRIVATE
        PkgConfig::Xkbcommon
        Threads::Threads
        OpenGL::GL
        ${X11_LIBRARIES}
        ${X11_Xfixes_LIB}
        ${CMAKE_DL_LIBS}
        m
)
set_target_properties(lsteamclient_overlay_bridge PROPERTIES
    OUTPUT_NAME lsteamclient_overlay_bridge
    C_VISIBILITY_PRESET hidden
    VERSION 1.0.0
    SOVERSION 1
)

install(TARGETS lsteamclient_overlay_bridge
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
