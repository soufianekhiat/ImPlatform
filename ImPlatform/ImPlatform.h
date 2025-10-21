// dear imgui: Platform/Renderer Abstraction Layer
// This is a higher-level C API that abstracts platform and graphics boilerplate from examples.
//
// Configuration: Define IM_CURRENT_PLATFORM and IM_CURRENT_GFX before including this header:
//   Example: #define IM_CURRENT_PLATFORM IM_PLATFORM_WIN32
//            #define IM_CURRENT_GFX IM_GFX_DIRECTX11
//
// Available platforms:
//   IM_PLATFORM_WIN32, IM_PLATFORM_GLFW, IM_PLATFORM_APPLE
// Available graphics APIs:
//   IM_GFX_OPENGL3, IM_GFX_DIRECTX11, IM_GFX_DIRECTX12, IM_GFX_VULKAN, IM_GFX_METAL, IM_GFX_WGPU
//
// Or use predefined combinations:
//   IM_TARGET_WIN32_DX11, IM_TARGET_WIN32_DX12, IM_TARGET_WIN32_OPENGL3
//   IM_TARGET_GLFW_OPENGL3, IM_TARGET_GLFW_VULKAN, IM_TARGET_GLFW_METAL
//   IM_TARGET_APPLE_METAL
//
// Usage:
//   1. ImPlatform_CreateWindow() - Create your application window
//   2. ImPlatform_InitGfxAPI() - Initialize graphics device
//   3. ImPlatform_ShowWindow() - Display the window
//   4. ImPlatform_InitPlatform() - Initialize ImGui platform backend
//   5. ImPlatform_InitGfx() - Create graphics resources
//   6. Main loop:
//      while (ImPlatform_PlatformContinue())
//      {
//          ImPlatform_PlatformEvents();
//          if (!ImPlatform_GfxCheck()) continue;
//
//          ImPlatform_GfxAPINewFrame();
//          ImPlatform_PlatformNewFrame();
//          ImGui::NewFrame();
//
//          // Your ImGui code here
//
//          ImGui::Render();
//          ImPlatform_GfxAPIClear(clear_color);
//          ImPlatform_GfxAPIRender(clear_color);
//          ImPlatform_GfxViewportPre();
//          ImPlatform_GfxViewportPost();
//          ImPlatform_GfxAPISwapBuffer();
//      }
//   7. Cleanup:
//      ImPlatform_ShutdownGfxAPI();
//      ImPlatform_ShutdownWindow();
//      ImPlatform_ShutdownPostGfxAPI();
//      ImPlatform_DestroyWindow();

#pragma once

#ifndef IMPLATFORM_API
#define IMPLATFORM_API
#endif

// Include imgui for types
#ifndef IMGUI_VERSION
#include "imgui.h"
#endif

// Platform/Graphics API defines
#define IM_GFX_OPENGL3      ( 1u << 0u )
#define IM_GFX_DIRECTX9     ( 1u << 1u )
#define IM_GFX_DIRECTX10    ( 1u << 2u )
#define IM_GFX_DIRECTX11    ( 1u << 3u )
#define IM_GFX_DIRECTX12    ( 1u << 4u )
#define IM_GFX_VULKAN       ( 1u << 5u )
#define IM_GFX_METAL        ( 1u << 6u )
#define IM_GFX_WGPU         ( 1u << 7u )

#define IM_GFX_MASK         0x0000FFFFu

#define IM_PLATFORM_WIN32   ( ( 1u << 0u ) << 16u )
#define IM_PLATFORM_GLFW    ( ( 1u << 1u ) << 16u )
#define IM_PLATFORM_SDL2    ( ( 1u << 2u ) << 16u )
#define IM_PLATFORM_SDL3    ( ( 1u << 3u ) << 16u )
#define IM_PLATFORM_APPLE   ( ( 1u << 4u ) << 16u )

#define IM_PLATFORM_MASK    0xFFFF0000u

// Predefined target combinations
#define IM_TARGET_WIN32_DX10        ( IM_PLATFORM_WIN32 | IM_GFX_DIRECTX10 )
#define IM_TARGET_WIN32_DX11        ( IM_PLATFORM_WIN32 | IM_GFX_DIRECTX11 )
#define IM_TARGET_WIN32_DX12        ( IM_PLATFORM_WIN32 | IM_GFX_DIRECTX12 )
#define IM_TARGET_WIN32_OPENGL3     ( IM_PLATFORM_WIN32 | IM_GFX_OPENGL3 )

#define IM_TARGET_APPLE_METAL       ( IM_PLATFORM_APPLE | IM_GFX_METAL )

#define IM_TARGET_GLFW_OPENGL3      ( IM_PLATFORM_GLFW | IM_GFX_OPENGL3 )
#define IM_TARGET_GLFW_VULKAN       ( IM_PLATFORM_GLFW | IM_GFX_VULKAN )
#define IM_TARGET_GLFW_METAL        ( IM_PLATFORM_GLFW | IM_GFX_METAL )

#if defined(IM_CURRENT_GFX) && defined(IM_CURRENT_PLATFORM)
#elif defined(IM_CURRENT_GFX) && !defined(IM_CURRENT_PLATFORM)
#error ImPlatform if IM_CURRENT_GFX is defined IM_CURRENT_PLATFORM have to be defined too.
#elif !defined(IM_CURRENT_GFX) && defined(IM_CURRENT_PLATFORM)
#error ImPlatform if IM_CURRENT_PLATFORM is defined IM_CURRENT_GFX have to be defined too.
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Core API Functions

// Create a window with specified name, position, and size
// Returns true on success
IMPLATFORM_API bool ImPlatform_CreateWindow(char const* pWindowsName, ImVec2 const vPos, unsigned int uWidth, unsigned int uHeight);

// Initialize the graphics API (creates device, swapchain, etc.)
// Must call after CreateWindow
// Returns true on success
IMPLATFORM_API bool ImPlatform_InitGfxAPI(void);

// Show the window
// Returns true on success
IMPLATFORM_API bool ImPlatform_ShowWindow(void);

// Initialize the platform and renderer backends
// Must call after InitGfxAPI
// Returns true on success
IMPLATFORM_API bool ImPlatform_InitPlatform(void);

// Complete graphics initialization (create device objects, fonts, etc.)
// Returns true on success
IMPLATFORM_API bool ImPlatform_InitGfx(void);

// Main loop functions

// Check if the platform should continue running (window not closed)
// Returns true if should continue, false if should exit
IMPLATFORM_API bool ImPlatform_PlatformContinue(void);

// Process platform events (input, window messages, etc.)
// Returns true on success
IMPLATFORM_API bool ImPlatform_PlatformEvents(void);

// Check graphics state (device lost, window occluded, etc.)
// Returns true if graphics are ready to render
IMPLATFORM_API bool ImPlatform_GfxCheck(void);

// Begin new frame for graphics API backend
IMPLATFORM_API void ImPlatform_GfxAPINewFrame(void);

// Begin new frame for platform backend
IMPLATFORM_API void ImPlatform_PlatformNewFrame(void);

// Clear the framebuffer with specified color
// vClearColor: RGBA clear color
// Returns true on success
IMPLATFORM_API bool ImPlatform_GfxAPIClear(ImVec4 const vClearColor);

// Render ImGui draw data
// vClearColor: RGBA clear color (needed for DX12 command list setup)
// Returns true on success
IMPLATFORM_API bool ImPlatform_GfxAPIRender(ImVec4 const vClearColor);

// Update platform windows (for multi-viewport support)
// Call before rendering viewports
IMPLATFORM_API void ImPlatform_GfxViewportPre(void);

// Render platform windows (for multi-viewport support)
// Call after main rendering
IMPLATFORM_API void ImPlatform_GfxViewportPost(void);

// Swap buffers / present
// Returns true on success
IMPLATFORM_API bool ImPlatform_GfxAPISwapBuffer(void);

// Shutdown functions (call in this order)

// Shutdown graphics API backend (invalidate device objects)
IMPLATFORM_API void ImPlatform_ShutdownGfxAPI(void);

// Shutdown ImGui backends
IMPLATFORM_API void ImPlatform_ShutdownWindow(void);

// Post-graphics API shutdown (cleanup device, swapchain)
IMPLATFORM_API void ImPlatform_ShutdownPostGfxAPI(void);

// Destroy window and platform
IMPLATFORM_API void ImPlatform_DestroyWindow(void);

#ifdef __cplusplus
}
#endif

#ifdef IMPLATFORM_IMPLEMENTATION

// Include graphics backend implementation
#if IM_CURRENT_GFX == IM_GFX_OPENGL3
    #include "ImPlatform_gfx_opengl3.cpp"
#elif IM_CURRENT_GFX == IM_GFX_DIRECTX9
    #include "ImPlatform_gfx_dx9.cpp"
#elif IM_CURRENT_GFX == IM_GFX_DIRECTX10
    #include "ImPlatform_gfx_dx10.cpp"
#elif IM_CURRENT_GFX == IM_GFX_DIRECTX11
    #include "ImPlatform_gfx_dx11.cpp"
#elif IM_CURRENT_GFX == IM_GFX_DIRECTX12
    #include "ImPlatform_gfx_dx12.cpp"
#elif IM_CURRENT_GFX == IM_GFX_VULKAN
    #include "ImPlatform_gfx_vulkan.cpp"
#elif IM_CURRENT_GFX == IM_GFX_METAL
    #include "ImPlatform_gfx_metal.mm"
#elif IM_CURRENT_GFX == IM_GFX_WGPU
    #include "ImPlatform_gfx_wgpu.cpp"
#else
    #error "Unknown or unsupported IM_CURRENT_GFX backend"
#endif

// Include platform backend implementation
#if IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32
    #include "ImPlatform_app_win32.cpp"
#elif IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW
    #include "ImPlatform_app_glfw.cpp"
#elif IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2
    #include "ImPlatform_app_sdl2.cpp"
#elif IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3
    #include "ImPlatform_app_sdl3.cpp"
#elif IM_CURRENT_PLATFORM == IM_PLATFORM_APPLE
    #include "ImPlatform_app_apple.mm"
#else
    #error "Unknown or unsupported IM_CURRENT_PLATFORM backend"
#endif

#endif // IMPLATFORM_IMPLEMENTATION
