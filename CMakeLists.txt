cmake_minimum_required(VERSION 3.25)

project(vulkan_restir_renderer VERSION 0.1.0 LANGUAGES CXX)

option(VRR_ENABLE_VALIDATION "Enable Vulkan validation layers" ON)

find_package(Vulkan REQUIRED)

add_executable(vulkan-restir
    src/main.cpp
    src/platform/Win32Window.cpp
    src/vulkan/GpuProfiler.cpp
    src/vulkan/VulkanContext.cpp
)

target_compile_features(vulkan-restir PRIVATE cxx_std_20)
target_compile_definitions(vulkan-restir PRIVATE
    VK_USE_PLATFORM_WIN32_KHR
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
    $<$<BOOL:${VRR_ENABLE_VALIDATION}>:VRR_ENABLE_VALIDATION=1>
)
target_include_directories(vulkan-restir PRIVATE src)
target_link_libraries(vulkan-restir PRIVATE Vulkan::Vulkan)

find_program(SLANGC_EXECUTABLE
    NAMES slangc
    HINTS "$ENV{VULKAN_SDK}/Bin"
    REQUIRED
)

set(SHADER_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(SMOKE_SHADER_OUTPUT "${SHADER_OUTPUT_DIRECTORY}/smoke_test.spv")
add_custom_command(
    OUTPUT "${SMOKE_SHADER_OUTPUT}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_OUTPUT_DIRECTORY}"
    COMMAND "${SLANGC_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/smoke_test.slang"
            -entry computeMain
            -stage compute
            -target spirv
            -profile sm_6_6
            -o "${SMOKE_SHADER_OUTPUT}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/smoke_test.slang"
    COMMENT "Compiling smoke_test.slang to SPIR-V"
    VERBATIM
)
add_custom_target(compile-shaders ALL DEPENDS "${SMOKE_SHADER_OUTPUT}")
add_dependencies(vulkan-restir compile-shaders)

if(MSVC)
    target_compile_options(vulkan-restir PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(vulkan-restir PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)

if(BUILD_TESTING)
    find_program(SPIRV_VAL_EXECUTABLE
        NAMES spirv-val
        HINTS "$ENV{VULKAN_SDK}/Bin"
        REQUIRED
    )
    add_test(
        NAME shader.spirv-validation
        COMMAND "${SPIRV_VAL_EXECUTABLE}" "${SMOKE_SHADER_OUTPUT}"
    )
endif()
