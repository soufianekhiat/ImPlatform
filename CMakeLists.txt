cmake_minimum_required(VERSION 3.20)
project(ImPlatform VERSION 0.5.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(IMPLATFORM_BUILD_DEMO "Build ImPlatformDemo" ON)
option(IMPLATFORM_FETCH_DEPENDENCIES "Fetch dependencies if not found" ON)

# Platform detection
if(WIN32)
    set(IMPLATFORM_WINDOWS TRUE)
elseif(APPLE)
    set(IMPLATFORM_MACOS TRUE)
    enable_language(OBJCXX)
elseif(UNIX)
    set(IMPLATFORM_LINUX TRUE)
endif()

# =============================================================================
# Dependencies
# =============================================================================
include(FetchContent)

# Find or fetch GLFW
find_package(glfw3 3.3 QUIET)
if(NOT glfw3_FOUND AND IMPLATFORM_FETCH_DEPENDENCIES)
    message(STATUS "GLFW not found, fetching...")
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
        GIT_SHALLOW    TRUE
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
    set(GLFW_AVAILABLE TRUE)
elseif(glfw3_FOUND)
    set(GLFW_AVAILABLE TRUE)
else()
    set(GLFW_AVAILABLE FALSE)
endif()

# Find SDL2
find_package(SDL2 QUIET)
if(SDL2_FOUND)
    set(SDL2_AVAILABLE TRUE)
else()
    set(SDL2_AVAILABLE FALSE)
    message(STATUS "SDL2 not found - SDL2 configurations will be skipped")
endif()

# Find SDL3
find_package(SDL3 QUIET)
if(SDL3_FOUND)
    set(SDL3_AVAILABLE TRUE)
else()
    set(SDL3_AVAILABLE FALSE)
    message(STATUS "SDL3 not found - SDL3 configurations will be skipped")
endif()

# Find Vulkan
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    set(VULKAN_AVAILABLE TRUE)
else()
    set(VULKAN_AVAILABLE FALSE)
    message(STATUS "Vulkan not found - Vulkan configurations will be skipped")
endif()

# Find OpenGL
find_package(OpenGL QUIET)
if(OpenGL_FOUND)
    set(OPENGL_AVAILABLE TRUE)
else()
    set(OPENGL_AVAILABLE FALSE)
    message(STATUS "OpenGL not found - OpenGL configurations will be skipped")
endif()

# =============================================================================
# ImGui Library
# =============================================================================
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR})

# =============================================================================
# ImPlatform Source Files
# =============================================================================
set(IMPLATFORM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ImPlatform)

set(IMPLATFORM_HEADERS
    ${IMPLATFORM_DIR}/ImPlatform.h
    ${IMPLATFORM_DIR}/ImPlatform_Internal.h
)

set(IMPLATFORM_COMMON_SOURCES
    ${IMPLATFORM_DIR}/ImPlatform_titlebar.cpp
)

# Platform-specific sources
set(IMPLATFORM_SRC_WIN32     ${IMPLATFORM_DIR}/ImPlatform_app_win32.cpp)
set(IMPLATFORM_SRC_GLFW      ${IMPLATFORM_DIR}/ImPlatform_app_glfw.cpp)
set(IMPLATFORM_SRC_SDL2      ${IMPLATFORM_DIR}/ImPlatform_app_sdl2.cpp)
set(IMPLATFORM_SRC_SDL3      ${IMPLATFORM_DIR}/ImPlatform_app_sdl3.cpp)
set(IMPLATFORM_SRC_APPLE     ${IMPLATFORM_DIR}/ImPlatform_app_apple.mm)

# Graphics-specific sources
set(IMPLATFORM_SRC_DX9       ${IMPLATFORM_DIR}/ImPlatform_gfx_dx9.cpp)
set(IMPLATFORM_SRC_DX10      ${IMPLATFORM_DIR}/ImPlatform_gfx_dx10.cpp)
set(IMPLATFORM_SRC_DX11      ${IMPLATFORM_DIR}/ImPlatform_gfx_dx11.cpp)
set(IMPLATFORM_SRC_DX12      ${IMPLATFORM_DIR}/ImPlatform_gfx_dx12.cpp)
set(IMPLATFORM_SRC_OPENGL3   ${IMPLATFORM_DIR}/ImPlatform_gfx_opengl3.cpp)
set(IMPLATFORM_SRC_VULKAN    ${IMPLATFORM_DIR}/ImPlatform_gfx_vulkan.cpp)
set(IMPLATFORM_SRC_METAL     ${IMPLATFORM_DIR}/ImPlatform_gfx_metal.mm)
set(IMPLATFORM_SRC_WGPU      ${IMPLATFORM_DIR}/ImPlatform_gfx_wgpu.cpp)

# ImGui backend sources
set(IMGUI_BACKEND_WIN32      ${IMGUI_DIR}/backends/imgui_impl_win32.cpp)
set(IMGUI_BACKEND_GLFW       ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp)
set(IMGUI_BACKEND_SDL2       ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp)
set(IMGUI_BACKEND_SDL3       ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp)
set(IMGUI_BACKEND_DX9        ${IMGUI_DIR}/backends/imgui_impl_dx9.cpp)
set(IMGUI_BACKEND_DX10       ${IMGUI_DIR}/backends/imgui_impl_dx10.cpp)
set(IMGUI_BACKEND_DX11       ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp)
set(IMGUI_BACKEND_DX12       ${IMGUI_DIR}/backends/imgui_impl_dx12.cpp)
set(IMGUI_BACKEND_OPENGL3    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)
set(IMGUI_BACKEND_VULKAN     ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp)
set(IMGUI_BACKEND_METAL      ${IMGUI_DIR}/backends/imgui_impl_metal.mm)

# =============================================================================
# Helper function to create ImPlatform configurations
# =============================================================================
function(implatform_add_config TARGET_NAME PLATFORM GFX)
    set(SOURCES
        ${IMPLATFORM_HEADERS}
        ${IMPLATFORM_COMMON_SOURCES}
    )

    # Add platform sources
    if(PLATFORM STREQUAL "WIN32")
        list(APPEND SOURCES ${IMPLATFORM_SRC_WIN32} ${IMGUI_BACKEND_WIN32})
        set(PLATFORM_DEFINE "IM_CURRENT_PLATFORM=IM_PLATFORM_WIN32")
        set(CONFIG_PLATFORM_DEFINE "IM_CONFIG_PLATFORM=IM_PLATFORM_WIN32")
    elseif(PLATFORM STREQUAL "GLFW")
        list(APPEND SOURCES ${IMPLATFORM_SRC_GLFW} ${IMGUI_BACKEND_GLFW})
        set(PLATFORM_DEFINE "IM_CURRENT_PLATFORM=IM_PLATFORM_GLFW")
        set(CONFIG_PLATFORM_DEFINE "IM_CONFIG_PLATFORM=IM_PLATFORM_GLFW")
    elseif(PLATFORM STREQUAL "SDL2")
        list(APPEND SOURCES ${IMPLATFORM_SRC_SDL2} ${IMGUI_BACKEND_SDL2})
        set(PLATFORM_DEFINE "IM_CURRENT_PLATFORM=IM_PLATFORM_SDL2")
        set(CONFIG_PLATFORM_DEFINE "IM_CONFIG_PLATFORM=IM_PLATFORM_SDL2")
    elseif(PLATFORM STREQUAL "SDL3")
        list(APPEND SOURCES ${IMPLATFORM_SRC_SDL3} ${IMGUI_BACKEND_SDL3})
        set(PLATFORM_DEFINE "IM_CURRENT_PLATFORM=IM_PLATFORM_SDL3")
        set(CONFIG_PLATFORM_DEFINE "IM_CONFIG_PLATFORM=IM_PLATFORM_SDL3")
    elseif(PLATFORM STREQUAL "APPLE")
        list(APPEND SOURCES ${IMPLATFORM_SRC_APPLE})
        set(PLATFORM_DEFINE "IM_CURRENT_PLATFORM=IM_PLATFORM_APPLE")
        set(CONFIG_PLATFORM_DEFINE "IM_CONFIG_PLATFORM=IM_PLATFORM_APPLE")
    endif()

    # Add graphics sources
    if(GFX STREQUAL "DX9")
        list(APPEND SOURCES ${IMPLATFORM_SRC_DX9} ${IMGUI_BACKEND_DX9})
        set(GFX_DEFINE "IM_CURRENT_GFX=IM_GFX_DIRECTX9")
        set(CONFIG_GFX_DEFINE "IM_CONFIG_GFX=IM_GFX_DIRECTX9")
    elseif(GFX STREQUAL "DX10")
        list(APPEND SOURCES ${IMPLATFORM_SRC_DX10} ${IMGUI_BACKEND_DX10})
        set(GFX_DEFINE "IM_CURRENT_GFX=IM_GFX_DIRECTX10")
        set(CONFIG_GFX_DEFINE "IM_CONFIG_GFX=IM_GFX_DIRECTX10")
    elseif(GFX STREQUAL "DX11")
        list(APPEND SOURCES ${IMPLATFORM_SRC_DX11} ${IMGUI_BACKEND_DX11})
        set(GFX_DEFINE "IM_CURRENT_GFX=IM_GFX_DIRECTX11")
        set(CONFIG_GFX_DEFINE "IM_CONFIG_GFX=IM_GFX_DIRECTX11")
    elseif(GFX STREQUAL "DX12")
        list(APPEND SOURCES ${IMPLATFORM_SRC_DX12} ${IMGUI_BACKEND_DX12})
        set(GFX_DEFINE "IM_CURRENT_GFX=IM_GFX_DIRECTX12")
        set(CONFIG_GFX_DEFINE "IM_CONFIG_GFX=IM_GFX_DIRECTX12")
    elseif(GFX STREQUAL "OPENGL3")
        list(APPEND SOURCES ${IMPLATFORM_SRC_OPENGL3} ${IMGUI_BACKEND_OPENGL3})
        set(GFX_DEFINE "IM_CURRENT_GFX=IM_GFX_OPENGL3")
        set(CONFIG_GFX_DEFINE "IM_CONFIG_GFX=IM_GFX_OPENGL3")
    elseif(GFX STREQUAL "VULKAN")
        list(APPEND SOURCES ${IMPLATFORM_SRC_VULKAN} ${IMGUI_BACKEND_VULKAN})
        set(GFX_DEFINE "IM_CURRENT_GFX=IM_GFX_VULKAN")
        set(CONFIG_GFX_DEFINE "IM_CONFIG_GFX=IM_GFX_VULKAN")
    elseif(GFX STREQUAL "METAL")
        list(APPEND SOURCES ${IMPLATFORM_SRC_METAL} ${IMGUI_BACKEND_METAL})
        set(GFX_DEFINE "IM_CURRENT_GFX=IM_GFX_METAL")
        set(CONFIG_GFX_DEFINE "IM_CONFIG_GFX=IM_GFX_METAL")
    endif()

    # Create library
    add_library(${TARGET_NAME} STATIC ${SOURCES})

    target_include_directories(${TARGET_NAME} PUBLIC
        ${IMPLATFORM_DIR}
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )

    target_compile_definitions(${TARGET_NAME} PUBLIC
        ${PLATFORM_DEFINE}
        ${GFX_DEFINE}
        ${CONFIG_PLATFORM_DEFINE}
        ${CONFIG_GFX_DEFINE}
    )

    target_link_libraries(${TARGET_NAME} PUBLIC imgui)

    # Platform-specific dependencies
    if(PLATFORM STREQUAL "WIN32")
        target_link_libraries(${TARGET_NAME} PUBLIC dwmapi)
    elseif(PLATFORM STREQUAL "GLFW")
        target_link_libraries(${TARGET_NAME} PUBLIC glfw)
        # GLFW headers are needed for titlebar.cpp
        target_include_directories(${TARGET_NAME} PUBLIC ${glfw_SOURCE_DIR}/include)
    elseif(PLATFORM STREQUAL "SDL2")
        target_link_libraries(${TARGET_NAME} PUBLIC SDL2::SDL2)
    elseif(PLATFORM STREQUAL "SDL3")
        target_link_libraries(${TARGET_NAME} PUBLIC SDL3::SDL3)
    endif()

    # Graphics-specific dependencies
    if(GFX STREQUAL "DX9")
        target_link_libraries(${TARGET_NAME} PUBLIC d3d9)
    elseif(GFX STREQUAL "DX10")
        target_link_libraries(${TARGET_NAME} PUBLIC d3d10 dxgi d3dcompiler)
    elseif(GFX STREQUAL "DX11")
        target_link_libraries(${TARGET_NAME} PUBLIC d3d11 dxgi d3dcompiler)
    elseif(GFX STREQUAL "DX12")
        target_link_libraries(${TARGET_NAME} PUBLIC d3d12 dxgi d3dcompiler)
    elseif(GFX STREQUAL "OPENGL3")
        target_link_libraries(${TARGET_NAME} PUBLIC OpenGL::GL)
        # imgui_impl_opengl3_loader.h provides the OpenGL loader
    elseif(GFX STREQUAL "VULKAN")
        target_link_libraries(${TARGET_NAME} PUBLIC Vulkan::Vulkan)
    elseif(GFX STREQUAL "METAL")
        find_library(METAL_FRAMEWORK Metal REQUIRED)
        find_library(METALKIT_FRAMEWORK MetalKit REQUIRED)
        find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
        target_link_libraries(${TARGET_NAME} PUBLIC
            ${METAL_FRAMEWORK}
            ${METALKIT_FRAMEWORK}
            ${QUARTZCORE_FRAMEWORK}
        )
    endif()

    # macOS frameworks
    if(IMPLATFORM_MACOS)
        find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
        find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
        find_library(COREVIDEO_FRAMEWORK CoreVideo REQUIRED)
        target_link_libraries(${TARGET_NAME} PUBLIC
            ${COCOA_FRAMEWORK}
            ${IOKIT_FRAMEWORK}
            ${COREVIDEO_FRAMEWORK}
        )
    endif()

    # Linux dependencies
    if(IMPLATFORM_LINUX)
        target_link_libraries(${TARGET_NAME} PUBLIC dl pthread)
    endif()
endfunction()

# =============================================================================
# Helper function to create demo executable
# =============================================================================
function(implatform_add_demo TARGET_NAME IMPLATFORM_LIB)
    add_executable(${TARGET_NAME}
        ${CMAKE_CURRENT_SOURCE_DIR}/ImPlatformDemo/main.cpp
    )
    target_link_libraries(${TARGET_NAME} PRIVATE ${IMPLATFORM_LIB})

    # Windows: Set subsystem to windows for non-console app
    if(WIN32)
        set_target_properties(${TARGET_NAME} PROPERTIES
            WIN32_EXECUTABLE FALSE
        )
    endif()
endfunction()

# =============================================================================
# Build configurations based on available dependencies
# =============================================================================

# Windows-only configurations (DirectX)
if(IMPLATFORM_WINDOWS)
    # Win32 + DirectX configurations
    implatform_add_config(implatform_win32_dx9 WIN32 DX9)
    implatform_add_config(implatform_win32_dx10 WIN32 DX10)
    implatform_add_config(implatform_win32_dx11 WIN32 DX11)
    implatform_add_config(implatform_win32_dx12 WIN32 DX12)

    if(OPENGL_AVAILABLE)
        implatform_add_config(implatform_win32_opengl3 WIN32 OPENGL3)
    endif()

    if(IMPLATFORM_BUILD_DEMO)
        implatform_add_demo(ImPlatformDemo_Win32_DX9 implatform_win32_dx9)
        implatform_add_demo(ImPlatformDemo_Win32_DX10 implatform_win32_dx10)
        implatform_add_demo(ImPlatformDemo_Win32_DX11 implatform_win32_dx11)
        implatform_add_demo(ImPlatformDemo_Win32_DX12 implatform_win32_dx12)
        if(OPENGL_AVAILABLE)
            implatform_add_demo(ImPlatformDemo_Win32_OpenGL3 implatform_win32_opengl3)
        endif()
    endif()

    # SDL2 + DX11 on Windows
    if(SDL2_AVAILABLE)
        implatform_add_config(implatform_sdl2_dx11 SDL2 DX11)
        if(IMPLATFORM_BUILD_DEMO)
            implatform_add_demo(ImPlatformDemo_SDL2_DX11 implatform_sdl2_dx11)
        endif()
    endif()

    # SDL3 + DX11 on Windows
    if(SDL3_AVAILABLE)
        implatform_add_config(implatform_sdl3_dx11 SDL3 DX11)
        if(IMPLATFORM_BUILD_DEMO)
            implatform_add_demo(ImPlatformDemo_SDL3_DX11 implatform_sdl3_dx11)
        endif()
    endif()
endif()

# GLFW configurations (cross-platform)
if(GLFW_AVAILABLE)
    if(OPENGL_AVAILABLE)
        implatform_add_config(implatform_glfw_opengl3 GLFW OPENGL3)
        if(IMPLATFORM_BUILD_DEMO)
            implatform_add_demo(ImPlatformDemo_GLFW_OpenGL3 implatform_glfw_opengl3)
        endif()
    endif()

    if(VULKAN_AVAILABLE)
        implatform_add_config(implatform_glfw_vulkan GLFW VULKAN)
        if(IMPLATFORM_BUILD_DEMO)
            implatform_add_demo(ImPlatformDemo_GLFW_Vulkan implatform_glfw_vulkan)
        endif()
    endif()

    if(IMPLATFORM_MACOS)
        implatform_add_config(implatform_glfw_metal GLFW METAL)
        if(IMPLATFORM_BUILD_DEMO)
            implatform_add_demo(ImPlatformDemo_GLFW_Metal implatform_glfw_metal)
        endif()
    endif()
endif()

# SDL2 configurations (cross-platform)
if(SDL2_AVAILABLE)
    if(OPENGL_AVAILABLE)
        implatform_add_config(implatform_sdl2_opengl3 SDL2 OPENGL3)
        if(IMPLATFORM_BUILD_DEMO)
            implatform_add_demo(ImPlatformDemo_SDL2_OpenGL3 implatform_sdl2_opengl3)
        endif()
    endif()

    if(VULKAN_AVAILABLE)
        implatform_add_config(implatform_sdl2_vulkan SDL2 VULKAN)
        if(IMPLATFORM_BUILD_DEMO)
            implatform_add_demo(ImPlatformDemo_SDL2_Vulkan implatform_sdl2_vulkan)
        endif()
    endif()

    if(IMPLATFORM_MACOS)
        implatform_add_config(implatform_sdl2_metal SDL2 METAL)
        if(IMPLATFORM_BUILD_DEMO)
            implatform_add_demo(ImPlatformDemo_SDL2_Metal implatform_sdl2_metal)
        endif()
    endif()
endif()

# SDL3 configurations (cross-platform)
if(SDL3_AVAILABLE)
    if(OPENGL_AVAILABLE)
        implatform_add_config(implatform_sdl3_opengl3 SDL3 OPENGL3)
        if(IMPLATFORM_BUILD_DEMO)
            implatform_add_demo(ImPlatformDemo_SDL3_OpenGL3 implatform_sdl3_opengl3)
        endif()
    endif()

    if(VULKAN_AVAILABLE)
        implatform_add_config(implatform_sdl3_vulkan SDL3 VULKAN)
        if(IMPLATFORM_BUILD_DEMO)
            implatform_add_demo(ImPlatformDemo_SDL3_Vulkan implatform_sdl3_vulkan)
        endif()
    endif()

    if(IMPLATFORM_MACOS)
        implatform_add_config(implatform_sdl3_metal SDL3 METAL)
        if(IMPLATFORM_BUILD_DEMO)
            implatform_add_demo(ImPlatformDemo_SDL3_Metal implatform_sdl3_metal)
        endif()
    endif()
endif()

# =============================================================================
# Print configuration summary
# =============================================================================
message(STATUS "")
message(STATUS "=== ImPlatform Configuration ===")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "")
message(STATUS "Dependencies found:")
message(STATUS "  GLFW:    ${GLFW_AVAILABLE}")
message(STATUS "  SDL2:    ${SDL2_AVAILABLE}")
message(STATUS "  SDL3:    ${SDL3_AVAILABLE}")
message(STATUS "  OpenGL:  ${OPENGL_AVAILABLE}")
message(STATUS "  Vulkan:  ${VULKAN_AVAILABLE}")
message(STATUS "")
