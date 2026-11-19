cmake_minimum_required(VERSION 3.16)

project(ge_hdr_colorspace
    VERSION 1.0.0
    LANGUAGES C
)

include(GNUInstallDirs)

set(VULKAN_HEADERS_INCLUDE_DIR "" CACHE PATH "Directory containing Vulkan headers")
if(NOT EXISTS "${VULKAN_HEADERS_INCLUDE_DIR}/vulkan/vk_layer.h")
    message(FATAL_ERROR "VULKAN_HEADERS_INCLUDE_DIR must contain vulkan/vk_layer.h")
endif()

add_library(VkLayer_GE_hdr_colorspace SHARED
    src/layer.c
)

target_compile_features(VkLayer_GE_hdr_colorspace PRIVATE c_std_11)
target_compile_options(VkLayer_GE_hdr_colorspace PRIVATE
    -Wall
    -Wextra
    -Wstrict-prototypes
)
target_include_directories(VkLayer_GE_hdr_colorspace PRIVATE
    ${VULKAN_HEADERS_INCLUDE_DIR}
)
target_link_libraries(VkLayer_GE_hdr_colorspace PRIVATE pthread)
set_target_properties(VkLayer_GE_hdr_colorspace PROPERTIES
    C_VISIBILITY_PRESET hidden
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/VkLayer_GE_hdr_colorspace.json.in
    ${CMAKE_CURRENT_BINARY_DIR}/VkLayer_GE_hdr_colorspace.json
    @ONLY
)

install(TARGETS VkLayer_GE_hdr_colorspace
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/VkLayer_GE_hdr_colorspace.json
    DESTINATION ${CMAKE_INSTALL_DATADIR}/vulkan/explicit_layer.d
)
