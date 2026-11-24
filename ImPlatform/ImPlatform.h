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
//   IM_GFX_CUSTOM: the APPLICATION provides the graphics backend. ImPlatform compiles no
//     graphics backend; the application links its own implementation of the ImPlatform_*
//     graphics entry points (InitGfxAPI, GfxAPINewFrame/Render/SwapBuffer, textures, shaders,
//     buffers...) and of ImPlatform_Gfx_OnResize_Custom, which the platform layer calls when
//     the window is resized. The platform layer (window, input, DPI, title bar) is ImPlatform's
//     as usual. Feature flags below default to "supported" and can be predefined to 0.
//
// Or use predefined combinations:
//   IM_TARGET_WIN32_DX11, IM_TARGET_WIN32_DX12, IM_TARGET_WIN32_OPENGL3, IM_TARGET_WIN32_VULKAN
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
//          if (!ImPlatform_PlatformEvents()) continue; // false: quit requested or minimized
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

// ImPlatform version
// (Integer encoded as XYYZZ for use in #if preprocessor conditionals, e.g. '#if IMPLATFORM_VERSION_NUM >= 30000')
#define IMPLATFORM_VERSION          "3.0.0"
#define IMPLATFORM_VERSION_NUM      30000
#define IMPLATFORM_VERSION_MAJOR    3
#define IMPLATFORM_VERSION_MINOR    0
#define IMPLATFORM_VERSION_PATCH    0

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
#define IM_GFX_CUSTOM       ( 1u << 8u )

#define IM_GFX_MASK         0x0000FFFFu

// Defined before the IMPLATFORM_APP_SUPPORT_* flags, whose conditions compare IM_CURRENT_PLATFORM with them
#define IM_PLATFORM_WIN32   ( ( 1u << 0u ) << 16u )
#define IM_PLATFORM_GLFW    ( ( 1u << 1u ) << 16u )
#define IM_PLATFORM_SDL2    ( ( 1u << 2u ) << 16u )
#define IM_PLATFORM_SDL3    ( ( 1u << 3u ) << 16u )
#define IM_PLATFORM_APPLE   ( ( 1u << 4u ) << 16u )     // macOS, a Cocoa window (with IM_GFX_METAL)

#define IM_PLATFORM_MASK    0xFFFF0000u

// Graphics API feature support flags
// These indicate which features are supported by each backend
#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_CUSTOM)
    // The application's backend declares what it supports; everything by default.
    #ifndef IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER
        #define IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER 1
    #endif
    #ifndef IMPLATFORM_GFX_SUPPORT_BGRA_FORMATS
        #define IMPLATFORM_GFX_SUPPORT_BGRA_FORMATS 1
    #endif
    #ifndef IMPLATFORM_GFX_SUPPORT_HALF_FLOAT_FORMATS
        #define IMPLATFORM_GFX_SUPPORT_HALF_FLOAT_FORMATS 1
    #endif
    #ifndef IMPLATFORM_GFX_SUPPORT_RGB_EXTENDED
        #define IMPLATFORM_GFX_SUPPORT_RGB_EXTENDED 1
    #endif
    #ifndef IMPLATFORM_GFX_SUPPORT_SRGB_FORMATS
        #define IMPLATFORM_GFX_SUPPORT_SRGB_FORMATS 1
    #endif
    #ifndef IMPLATFORM_GFX_SUPPORT_PACKED_FORMATS
        #define IMPLATFORM_GFX_SUPPORT_PACKED_FORMATS 1
    #endif
    #ifndef IMPLATFORM_GFX_SUPPORT_DEPTH_FORMATS
        #define IMPLATFORM_GFX_SUPPORT_DEPTH_FORMATS 1
    #endif
    #ifndef IMPLATFORM_GFX_SUPPORT_INTEGER_FORMATS
        #define IMPLATFORM_GFX_SUPPORT_INTEGER_FORMATS 1
    #endif
#elif defined(IM_CURRENT_GFX)
    #if (IM_CURRENT_GFX == IM_GFX_OPENGL3) || \
        (IM_CURRENT_GFX == IM_GFX_DIRECTX9) || \
        (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || \
        (IM_CURRENT_GFX == IM_GFX_DIRECTX11) || \
        (IM_CURRENT_GFX == IM_GFX_DIRECTX12) || \
        (IM_CURRENT_GFX == IM_GFX_VULKAN) || \
        (IM_CURRENT_GFX == IM_GFX_METAL) || \
        (IM_CURRENT_GFX == IM_GFX_WGPU)
        // Custom shader API with uniform blocks is implemented for all graphics backends
        #define IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER 1
    #else
        #define IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER 0
    #endif

    // BGRA byte-order format — native on DX/Metal/Vulkan/WGPU, supported via GL_BGRA on OpenGL3
    // Supported by all backends
    #define IMPLATFORM_GFX_SUPPORT_BGRA_FORMATS 1

    // 16-bit float (half-float) texture formats: R16F, RG16F, RGBA16F
    // Not supported by DirectX 9 (no D3DFMT half-float texture support)
    #if (IM_CURRENT_GFX == IM_GFX_OPENGL3) || \
        (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || \
        (IM_CURRENT_GFX == IM_GFX_DIRECTX11) || \
        (IM_CURRENT_GFX == IM_GFX_DIRECTX12) || \
        (IM_CURRENT_GFX == IM_GFX_VULKAN) || \
        (IM_CURRENT_GFX == IM_GFX_METAL) || \
        (IM_CURRENT_GFX == IM_GFX_WGPU)
        #define IMPLATFORM_GFX_SUPPORT_HALF_FLOAT_FORMATS 1
    #else
        #define IMPLATFORM_GFX_SUPPORT_HALF_FLOAT_FORMATS 0
    #endif

    // Extended 3-channel (RGB) formats: RGB16, RGB16F, RGB32F
    // DX and Metal have no native 3-channel texture types; WGPU omits them entirely.
    // On DX10+, only RGB32F (DXGI_FORMAT_R32G32B32_FLOAT) is natively available.
    // On Metal and WGPU these formats are not available at all.
    // Supported natively by: OpenGL3, Vulkan; partially by DX10/11/12 (RGB32F only)
    #if (IM_CURRENT_GFX == IM_GFX_OPENGL3) || \
        (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || \
        (IM_CURRENT_GFX == IM_GFX_DIRECTX11) || \
        (IM_CURRENT_GFX == IM_GFX_DIRECTX12) || \
        (IM_CURRENT_GFX == IM_GFX_VULKAN)
        #define IMPLATFORM_GFX_SUPPORT_RGB_EXTENDED 1
    #else
        #define IMPLATFORM_GFX_SUPPORT_RGB_EXTENDED 0
    #endif

    // sRGB gamma-encoded variants: RGB8_SRGB, RGBA8_SRGB
    // DirectX 9 has no explicit sRGB texture format flag (gamma handled via sampler state)
    #if (IM_CURRENT_GFX == IM_GFX_OPENGL3) || \
        (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || \
        (IM_CURRENT_GFX == IM_GFX_DIRECTX11) || \
        (IM_CURRENT_GFX == IM_GFX_DIRECTX12) || \
        (IM_CURRENT_GFX == IM_GFX_VULKAN) || \
        (IM_CURRENT_GFX == IM_GFX_METAL) || \
        (IM_CURRENT_GFX == IM_GFX_WGPU)
        #define IMPLATFORM_GFX_SUPPORT_SRGB_FORMATS 1
    #else
        #define IMPLATFORM_GFX_SUPPORT_SRGB_FORMATS 0
    #endif

    // Packed HDR format: RGB10A2 (10-bit RGB + 2-bit alpha)
    // DirectX 9 has D3DFMT_A2B10G10R10 but it is rarely supported across hardware
    #if (IM_CURRENT_GFX == IM_GFX_OPENGL3) || \
        (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || \
        (IM_CURRENT_GFX == IM_GFX_DIRECTX11) || \
        (IM_CURRENT_GFX == IM_GFX_DIRECTX12) || \
        (IM_CURRENT_GFX == IM_GFX_VULKAN) || \
        (IM_CURRENT_GFX == IM_GFX_METAL) || \
        (IM_CURRENT_GFX == IM_GFX_WGPU)
        #define IMPLATFORM_GFX_SUPPORT_PACKED_FORMATS 1
    #else
        #define IMPLATFORM_GFX_SUPPORT_PACKED_FORMATS 0
    #endif

    // Depth/stencil formats: D16, D32F, D24S8, D32FS8 (for render targets)
    // DirectX 9 only exposes D16 and D24S8 via fixed D3DFMT values, not as texture formats
    #if (IM_CURRENT_GFX == IM_GFX_OPENGL3) || \
        (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || \
        (IM_CURRENT_GFX == IM_GFX_DIRECTX11) || \
        (IM_CURRENT_GFX == IM_GFX_DIRECTX12) || \
        (IM_CURRENT_GFX == IM_GFX_VULKAN) || \
        (IM_CURRENT_GFX == IM_GFX_METAL) || \
        (IM_CURRENT_GFX == IM_GFX_WGPU)
        #define IMPLATFORM_GFX_SUPPORT_DEPTH_FORMATS 1
    #else
        #define IMPLATFORM_GFX_SUPPORT_DEPTH_FORMATS 0
    #endif

    // Integer texture formats: R8UI/I, R16UI/I, R32UI/I (label maps, index textures, etc.)
    // DirectX 9 has no integer texture support
    #if (IM_CURRENT_GFX == IM_GFX_OPENGL3) || \
        (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || \
        (IM_CURRENT_GFX == IM_GFX_DIRECTX11) || \
        (IM_CURRENT_GFX == IM_GFX_DIRECTX12) || \
        (IM_CURRENT_GFX == IM_GFX_VULKAN) || \
        (IM_CURRENT_GFX == IM_GFX_METAL) || \
        (IM_CURRENT_GFX == IM_GFX_WGPU)
        #define IMPLATFORM_GFX_SUPPORT_INTEGER_FORMATS 1
    #else
        #define IMPLATFORM_GFX_SUPPORT_INTEGER_FORMATS 0
    #endif
#endif

// Optional API groups. The functions of a group the selected backend does not implement are not declared, so code
// calling them does not compile for that backend; the run-time queries (ImPlatform_SupportsReadback, ...) stay and
// report false. Under IM_GFX_CUSTOM every group defaults to 1 and may be predefined to 0: the application implements
// the groups it keeps.
#if defined(IM_CURRENT_GFX)
    #define IMPLATFORM_GFX_IN(gfx_set) ((((IM_CURRENT_GFX) & (IM_GFX_CUSTOM | (gfx_set))) != 0) ? 1 : 0)
    // ImPlatform_SetShaderTexture: textures t0..t8 of custom shaders
    #ifndef IMPLATFORM_GFX_SUPPORT_SHADER_TEXTURES
        #define IMPLATFORM_GFX_SUPPORT_SHADER_TEXTURES IMPLATFORM_GFX_IN(IM_GFX_DIRECTX9 | IM_GFX_DIRECTX10 | IM_GFX_DIRECTX11 | IM_GFX_DIRECTX12 | IM_GFX_OPENGL3 | IM_GFX_VULKAN | IM_GFX_METAL | IM_GFX_WGPU)
    #endif
    // ImPlatform_CreateShaderFromParts: shaders written once as parts, compiled at run time
    #ifndef IMPLATFORM_GFX_SUPPORT_SHADER_PARTS
        #define IMPLATFORM_GFX_SUPPORT_SHADER_PARTS IMPLATFORM_GFX_IN(IM_GFX_DIRECTX9 | IM_GFX_DIRECTX10 | IM_GFX_DIRECTX11 | IM_GFX_DIRECTX12 | IM_GFX_OPENGL3 | IM_GFX_METAL)
    #endif
    // Vertex / index buffers and ImPlatform_DrawIndexed
    #ifndef IMPLATFORM_GFX_SUPPORT_VERTEX_BUFFERS
        #define IMPLATFORM_GFX_SUPPORT_VERTEX_BUFFERS IMPLATFORM_GFX_IN(IM_GFX_DIRECTX9 | IM_GFX_DIRECTX10 | IM_GFX_DIRECTX11 | IM_GFX_DIRECTX12 | IM_GFX_OPENGL3 | IM_GFX_VULKAN | IM_GFX_WGPU | IM_GFX_METAL)
    #endif
    // Direct draws from draw callbacks: ImPlatform_PushShaderConstants / PopShaderConstants, ImPlatform_GetCurrentTargetSize
    #ifndef IMPLATFORM_GFX_SUPPORT_DIRECT_DRAW
        #define IMPLATFORM_GFX_SUPPORT_DIRECT_DRAW IMPLATFORM_GFX_IN(IM_GFX_DIRECTX9 | IM_GFX_DIRECTX11 | IM_GFX_DIRECTX12 | IM_GFX_OPENGL3 | IM_GFX_VULKAN | IM_GFX_WGPU)
    #endif
    // Render-target stack in draw callbacks: ImPlatform_PushRenderTarget / PopRenderTarget, ImPlatform_CopyCurrentTarget,
    // ImPlatform_DrawQuadNow
    #ifndef IMPLATFORM_GFX_SUPPORT_RENDER_TARGETS
        #define IMPLATFORM_GFX_SUPPORT_RENDER_TARGETS IMPLATFORM_GFX_IN(IM_GFX_DIRECTX9 | IM_GFX_DIRECTX11 | IM_GFX_DIRECTX12 | IM_GFX_OPENGL3 | IM_GFX_VULKAN)
    #endif
    // ImPlatform_ReadTexture
    #ifndef IMPLATFORM_GFX_SUPPORT_READBACK
        #define IMPLATFORM_GFX_SUPPORT_READBACK IMPLATFORM_GFX_IN(IM_GFX_DIRECTX9 | IM_GFX_DIRECTX10 | IM_GFX_DIRECTX11 | IM_GFX_DIRECTX12 | IM_GFX_OPENGL3 | IM_GFX_VULKAN | IM_GFX_METAL)
    #endif
    // ImPlatform_BeginReadTexture / PollReadback / MapReadback / ReleaseReadback
    #ifndef IMPLATFORM_GFX_SUPPORT_ASYNC_READBACK
        #define IMPLATFORM_GFX_SUPPORT_ASYNC_READBACK IMPLATFORM_GFX_IN(IM_GFX_DIRECTX11 | IM_GFX_DIRECTX12)
    #endif
    // ImPlatform_CreateStorageTexture, ImPlatform_DispatchCompute (OpenGL: 4.3+, ImPlatform_SupportsCompute at run time)
    #ifndef IMPLATFORM_GFX_SUPPORT_COMPUTE
        #define IMPLATFORM_GFX_SUPPORT_COMPUTE IMPLATFORM_GFX_IN(IM_GFX_DIRECTX11 | IM_GFX_DIRECTX12 | IM_GFX_OPENGL3 | IM_GFX_VULKAN)
    #endif
    // ImPlatform_DispatchComputeAsync / PollComputeDispatch / ReleaseComputeDispatch
    #ifndef IMPLATFORM_GFX_SUPPORT_ASYNC_COMPUTE
        #define IMPLATFORM_GFX_SUPPORT_ASYNC_COMPUTE IMPLATFORM_GFX_IN(IM_GFX_DIRECTX11 | IM_GFX_DIRECTX12 | IM_GFX_OPENGL3 | IM_GFX_VULKAN)
    #endif
    // ImPlatform_CreateRenderTextureWithDepth, ImPlatform_DrawMesh
    #ifndef IMPLATFORM_GFX_SUPPORT_DEPTH_DRAW
        #define IMPLATFORM_GFX_SUPPORT_DEPTH_DRAW IMPLATFORM_GFX_IN(IM_GFX_DIRECTX11 | IM_GFX_DIRECTX12)
    #endif
    // Device loss, recreation, adapter name and testing aids: ImPlatform_GfxDeviceLost ... ImPlatform_GfxDebugGpuTimings
    #ifndef IMPLATFORM_GFX_SUPPORT_DEVICE_CONTROL
        #define IMPLATFORM_GFX_SUPPORT_DEVICE_CONTROL IMPLATFORM_GFX_IN(IM_GFX_DIRECTX11 | IM_GFX_DIRECTX12)
    #endif
#endif

// Platform feature support flags
// These indicate which features are supported by each platform
#ifndef IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
    #if defined(IM_CURRENT_PLATFORM)
        #if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32) || \
            (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW) || \
            (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2) || \
            (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
            #define IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR 1
        #else
            #define IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR 0
        #endif
    #endif
#endif

#ifndef IMPLATFORM_APP_SUPPORT_DROP_FILE
    #if defined(IM_CURRENT_PLATFORM)
        #if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32) || \
            (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)  || \
            (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)  || \
            (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
            #define IMPLATFORM_APP_SUPPORT_DROP_FILE 1
        #else
            #define IMPLATFORM_APP_SUPPORT_DROP_FILE 0
        #endif
    #endif
#endif

// Fullscreen, always on top and the window icon: the desktop platform backends.
#ifndef IMPLATFORM_APP_SUPPORT_WINDOW_STATE
    #if defined(IM_CURRENT_PLATFORM)
        #if !defined(__EMSCRIPTEN__) && \
            ((IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32) || \
             (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)  || \
             (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)  || \
             (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3))
            #define IMPLATFORM_APP_SUPPORT_WINDOW_STATE 1
        #else
            #define IMPLATFORM_APP_SUPPORT_WINDOW_STATE 0
        #endif
    #endif
#endif

// Predefined target combinations
#define IM_TARGET_WIN32_DX10        ( IM_PLATFORM_WIN32 | IM_GFX_DIRECTX10 )
#define IM_TARGET_WIN32_DX11        ( IM_PLATFORM_WIN32 | IM_GFX_DIRECTX11 )
#define IM_TARGET_WIN32_DX12        ( IM_PLATFORM_WIN32 | IM_GFX_DIRECTX12 )
#define IM_TARGET_WIN32_OPENGL3     ( IM_PLATFORM_WIN32 | IM_GFX_OPENGL3 )
#define IM_TARGET_WIN32_VULKAN      ( IM_PLATFORM_WIN32 | IM_GFX_VULKAN )

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

// ============================================================================
// Version Information
// ============================================================================

// Get ImPlatform version string (e.g., "3.0.0")
// Returns: Version string (compile-time constant, do not free)
IMPLATFORM_API const char* ImPlatform_GetVersion(void);

// Get ImPlatform version as integer (e.g., 500 for version 0.5.0)
// Returns: Version number encoded as XYYZZ
IMPLATFORM_API int ImPlatform_GetVersionNum(void);

// ============================================================================
// Texture Creation API
// ============================================================================

// Pixel format enums
typedef enum ImPlatform_PixelFormat {
    // 8-bit unsigned
    ImPlatform_PixelFormat_R8,           // Single channel, 8-bit unsigned
    ImPlatform_PixelFormat_RG8,          // Two channel, 8-bit unsigned per channel
    ImPlatform_PixelFormat_RGB8,         // Three channel, 8-bit unsigned per channel
    ImPlatform_PixelFormat_RGBA8,        // Four channel, 8-bit unsigned per channel (most common)

    // 16-bit unsigned
    ImPlatform_PixelFormat_R16,          // Single channel, 16-bit unsigned
    ImPlatform_PixelFormat_RG16,         // Two channel, 16-bit unsigned per channel
    ImPlatform_PixelFormat_RGBA16,       // Four channel, 16-bit unsigned per channel

    // 32-bit float
    ImPlatform_PixelFormat_R32F,         // Single channel, 32-bit float
    ImPlatform_PixelFormat_RG32F,        // Two channel, 32-bit float per channel
    ImPlatform_PixelFormat_RGBA32F,      // Four channel, 32-bit float per channel

#if IMPLATFORM_GFX_SUPPORT_BGRA_FORMATS
    // BGRA byte-order (native on DX/Metal/Vulkan/WGPU, avoids swizzle on those backends)
    ImPlatform_PixelFormat_BGRA8,        // Four channel, 8-bit unsigned, BGRA order
#endif

#if IMPLATFORM_GFX_SUPPORT_HALF_FLOAT_FORMATS
    // 16-bit float (half-float) — common in HDR/compute pipelines
    // Not available on DirectX 9
    ImPlatform_PixelFormat_R16F,         // Single channel, 16-bit float
    ImPlatform_PixelFormat_RG16F,        // Two channel, 16-bit float per channel
    ImPlatform_PixelFormat_RGBA16F,      // Four channel, 16-bit float per channel
#endif

#if IMPLATFORM_GFX_SUPPORT_RGB_EXTENDED
    // Extended 3-channel formats — no native support on Metal or WGPU; DX10+ supports RGB32F only
    ImPlatform_PixelFormat_RGB16,        // Three channel, 16-bit unsigned per channel
    ImPlatform_PixelFormat_RGB16F,       // Three channel, 16-bit float per channel
    ImPlatform_PixelFormat_RGB32F,       // Three channel, 32-bit float per channel
#endif

#if IMPLATFORM_GFX_SUPPORT_SRGB_FORMATS
    // sRGB gamma-encoded variants — not available on DirectX 9
    ImPlatform_PixelFormat_RGB8_SRGB,    // Three channel, 8-bit unsigned, sRGB encoded
    ImPlatform_PixelFormat_RGBA8_SRGB,   // Four channel, 8-bit unsigned, sRGB encoded
#endif

#if IMPLATFORM_GFX_SUPPORT_PACKED_FORMATS
    // Packed HDR format — not reliably available on DirectX 9
    ImPlatform_PixelFormat_RGB10A2,      // 10-bit RGB + 2-bit alpha, packed 32-bit
#endif

#if IMPLATFORM_GFX_SUPPORT_DEPTH_FORMATS
    // Depth/stencil formats for render targets — not available as textures on DirectX 9
    ImPlatform_PixelFormat_D16,          // 16-bit depth
    ImPlatform_PixelFormat_D32F,         // 32-bit float depth
    ImPlatform_PixelFormat_D24S8,        // 24-bit depth + 8-bit stencil (packed 32-bit)
    ImPlatform_PixelFormat_D32FS8,       // 32-bit float depth + 8-bit stencil
#endif

#if IMPLATFORM_GFX_SUPPORT_INTEGER_FORMATS
    // Integer texture formats (label maps, index textures, picking buffers, etc.)
    // Not available on DirectX 9
    ImPlatform_PixelFormat_R8UI,         // Single channel, 8-bit unsigned integer
    ImPlatform_PixelFormat_R8I,          // Single channel, 8-bit signed integer
    ImPlatform_PixelFormat_R16UI,        // Single channel, 16-bit unsigned integer
    ImPlatform_PixelFormat_R16I,         // Single channel, 16-bit signed integer
    ImPlatform_PixelFormat_R32UI,        // Single channel, 32-bit unsigned integer
    ImPlatform_PixelFormat_R32I,         // Single channel, 32-bit signed integer
#endif
} ImPlatform_PixelFormat;

// ----------------------------------------------------------------------------
// Generic image buffer descriptor (Halide-style)
// ----------------------------------------------------------------------------
// Describes an arbitrary CPU-resident image buffer of any of 11 sample types,
// 1..4 channels, with per-axis byte strides. This decouples "what the bytes
// mean" (sample_type, channels) from "how they are laid out" (strides), so
// callers can describe interleaved, planar, sub-image (ROI), or even mirrored
// (negative-stride) layouts without copying.
//
// Used by widgets that need to read raw image data without going through the
// limited ImPlatform_PixelFormat enum (e.g. DearWidgets ImageInspector).
//
// Sample type taxonomy: 11 scalar types (signed/unsigned 8/16/32/64 + half/float/double).
typedef enum ImSampleType {
    ImSampleType_U8,    // 8-bit unsigned integer
    ImSampleType_I8,    // 8-bit signed integer
    ImSampleType_U16,   // 16-bit unsigned integer
    ImSampleType_I16,   // 16-bit signed integer
    ImSampleType_U32,   // 32-bit unsigned integer
    ImSampleType_I32,   // 32-bit signed integer
    ImSampleType_U64,   // 64-bit unsigned integer
    ImSampleType_I64,   // 64-bit signed integer
    ImSampleType_F16,   // 16-bit IEEE half float
    ImSampleType_F32,   // 32-bit IEEE float
    ImSampleType_F64,   // 64-bit IEEE double
    ImSampleType_COUNT
} ImSampleType;

// Halide-style raw image buffer descriptor.
// All strides are in bytes (not elements) and may be negative for flips.
// `byte_offset` is added to `host` to reach pixel (0, 0, channel 0).
//
// Examples:
//   Tightly packed RGBA8 interleaved (most common):
//     x_stride_bytes = 4, y_stride_bytes = width * 4, c_stride_bytes = 1
//   Planar 16-bit RGB:
//     x_stride_bytes = 2, y_stride_bytes = width * 2, c_stride_bytes = width * height * 2
//   Sub-image (ROI) of an interleaved parent:
//     same strides as parent, byte_offset = parent_y_stride * roi_y + parent_x_stride * roi_x
//   Vertically flipped:
//     y_stride_bytes negative, byte_offset = (height - 1) * |y_stride|
typedef struct ImImageBuffer {
    const void*     host;             // Base pointer to image data (mmap-friendly)
    size_t          byte_offset;      // Offset added to host to reach pixel (0, 0, 0)
    unsigned int    width;            // X extent in pixels
    unsigned int    height;           // Y extent in pixels
    unsigned int    channels;         // 1..4
    ptrdiff_t       x_stride_bytes;   // Bytes from pixel (x, y, c) to (x+1, y, c)
    ptrdiff_t       y_stride_bytes;   // Bytes from pixel (x, y, c) to (x, y+1, c)
    ptrdiff_t       c_stride_bytes;   // Bytes from pixel (x, y, c) to (x, y, c+1)
    ImSampleType    type;             // Scalar type of each channel
    ImU64           version;          // Bump on ANY descriptor change to trigger re-upload
} ImImageBuffer;

// Returns the size in bytes of one scalar of the given sample type.
static inline size_t ImPlatform_SampleTypeSize(ImSampleType t) {
    switch (t) {
    case ImSampleType_U8:  case ImSampleType_I8:  return 1;
    case ImSampleType_U16: case ImSampleType_I16: case ImSampleType_F16: return 2;
    case ImSampleType_U32: case ImSampleType_I32: case ImSampleType_F32: return 4;
    case ImSampleType_U64: case ImSampleType_I64: case ImSampleType_F64: return 8;
    default: return 0;
    }
}

// Returns true if the sample type is a floating-point format (F16/F32/F64).
static inline bool ImPlatform_SampleTypeIsFloat(ImSampleType t) {
    return t == ImSampleType_F16 || t == ImSampleType_F32 || t == ImSampleType_F64;
}

// Returns true if the sample type is signed (signed integer or float).
static inline bool ImPlatform_SampleTypeIsSigned(ImSampleType t) {
    switch (t) {
    case ImSampleType_I8: case ImSampleType_I16: case ImSampleType_I32: case ImSampleType_I64:
    case ImSampleType_F16: case ImSampleType_F32: case ImSampleType_F64:
        return true;
    default:
        return false;
    }
}

// Returns the tight (no-padding) byte size of an ImImageBuffer's pixel data:
// width * height * channels * sample_size. This is the size needed for a
// tight-packed copy regardless of the buffer's actual strides.
static inline size_t ImPlatform_ImageBufferTightByteSize(const ImImageBuffer* buf) {
    if (!buf) return 0;
    return (size_t)buf->width * (size_t)buf->height * (size_t)buf->channels * ImPlatform_SampleTypeSize(buf->type);
}

// Texture filtering modes
typedef enum ImPlatform_TextureFilter {
    ImPlatform_TextureFilter_Nearest,    // Point sampling (sharp, pixelated)
    ImPlatform_TextureFilter_Linear,     // Linear filtering (smooth, blurred)
} ImPlatform_TextureFilter;

// Texture wrap/boundary modes
typedef enum ImPlatform_TextureWrap {
    ImPlatform_TextureWrap_Clamp,        // Clamp to edge (extends edge pixels)
    ImPlatform_TextureWrap_Repeat,       // Repeat/wrap texture coordinates
    ImPlatform_TextureWrap_Mirror,       // Mirror repeat (flips at boundaries)
} ImPlatform_TextureWrap;

// Texture descriptor - defines texture properties.
// The filter and wrap modes are the texture's own sampling state where the graphics API has one (OpenGL). Dear
// ImGui's backends draw textures with their own sampler, linear and clamped: ImGui::Image() and the draw lists'
// images do not follow these four members. ImPlatform_PushSampler() changes that sampler.
typedef struct ImPlatform_TextureDesc {
    unsigned int width;                   // Texture width in pixels
    unsigned int height;                  // Texture height in pixels
    ImPlatform_PixelFormat format;        // Pixel format
    ImPlatform_TextureFilter min_filter;  // Minification filter
    ImPlatform_TextureFilter mag_filter;  // Magnification filter
    ImPlatform_TextureWrap wrap_u;        // Horizontal wrap mode
    ImPlatform_TextureWrap wrap_v;        // Vertical wrap mode
} ImPlatform_TextureDesc;

// Helper function to create a texture descriptor with common defaults
// Default: RGBA8, Linear filtering, Clamp wrapping
IMPLATFORM_API ImPlatform_TextureDesc ImPlatform_TextureDesc_Default(
    unsigned int width,
    unsigned int height
);

// Create a 2D texture from raw pixel data
// pixel_data: Pointer to pixel data (size must match width * height * pixel_format_size)
// desc: Texture descriptor defining texture properties
// Returns: ImTextureID (opaque handle) or NULL on failure
// Note: The pixel data can be freed after this call returns
IMPLATFORM_API ImTextureID ImPlatform_CreateTexture(
    const void* pixel_data,
    const ImPlatform_TextureDesc* desc
);

// ---------------------------------------------------------------
// 3D Texture (Texture3D / volumetric) support
// ---------------------------------------------------------------
typedef struct ImPlatform_TextureDesc3D {
    unsigned int width;
    unsigned int height;
    unsigned int depth;                   // Z dimension (slices)
    ImPlatform_PixelFormat format;
    ImPlatform_TextureFilter min_filter;
    ImPlatform_TextureFilter mag_filter;
    ImPlatform_TextureWrap wrap_u;
    ImPlatform_TextureWrap wrap_v;
    ImPlatform_TextureWrap wrap_w;        // W / Z wrap mode
} ImPlatform_TextureDesc3D;

IMPLATFORM_API ImPlatform_TextureDesc3D ImPlatform_TextureDesc3D_Default(
    unsigned int width, unsigned int height, unsigned int depth
);

// Create a 3D texture from raw voxel data (tightly packed, z-major:
// z*w*h + y*w + x order). Returns ImTextureID_Invalid on failure or if
// the backend does not support 3D textures. Bind it with ImPlatform_SetShaderTexture to a slot the shader declares
// Texture3D (shader parts: a "<slot> <name> 3d" texture line); it is sampled with the program's sampler, so the filters
// and wraps of the desc apply only where the texture keeps them (OpenGL). Destroy with ImPlatform_DestroyTexture.
IMPLATFORM_API ImTextureID ImPlatform_CreateTexture3D(
    const void* voxel_data,
    const ImPlatform_TextureDesc3D* desc
);

// Returns true if the active backend supports Texture3D.
IMPLATFORM_API bool ImPlatform_SupportsTexture3D(void);

// Update existing texture with new pixel data
// texture_id: Previously created texture
// pixel_data: New pixel data (must match original texture format and dimensions)
// x, y: Offset within texture to start update (0, 0 for full update)
// width, height: Size of region to update (use original dimensions for full update)
// Returns: true on success, false on failure
IMPLATFORM_API bool ImPlatform_UpdateTexture(
    ImTextureID texture_id,
    const void* pixel_data,
    unsigned int x,
    unsigned int y,
    unsigned int width,
    unsigned int height
);

// Copy the contents of one texture into another (GPU-to-GPU copy)
// dst: Destination texture (must have been created with ImPlatform_CreateTexture)
// src: Source texture (must have been created with ImPlatform_CreateTexture)
// Both textures must have the same dimensions and format.
// Returns: true on success, false on failure or unsupported backend
IMPLATFORM_API bool ImPlatform_CopyTexture(
    ImTextureID dst,
    ImTextureID src
);

// Copy the current backbuffer contents into a user-created texture.
// dst must have been created with ImPlatform_CreateTexture matching the backbuffer dimensions.
// Useful for post-processing effects (blur, distortion) on the rendered scene.
// Returns: true on success, false on failure or unsupported backend
IMPLATFORM_API bool ImPlatform_CopyBackbuffer(ImTextureID dst);

// Query the current backbuffer dimensions in pixels.
IMPLATFORM_API void ImPlatform_GetBackbufferSize(unsigned int* width, unsigned int* height);

// Create a texture that can also be used as a render target (for offscreen passes).
// Unlike ImPlatform_CreateTexture, no pixel data is needed — the texture starts cleared.
// Returns: ImTextureID that can be used with ImPlatform_BeginRenderToTexture and as a shader input.
IMPLATFORM_API ImTextureID ImPlatform_CreateRenderTexture(const ImPlatform_TextureDesc* desc);

// Set a render texture as the active render target. All subsequent draw calls
// (including ImGui draw commands) will render into this texture instead of the backbuffer.
// texture: must have been created with ImPlatform_CreateRenderTexture.
// Returns: true on success
IMPLATFORM_API bool ImPlatform_BeginRenderToTexture(ImTextureID texture);

// Restore the backbuffer as the active render target.
IMPLATFORM_API void ImPlatform_EndRenderToTexture(void);

// Render-target stack, usable from inside an ImDrawCallback (multi-pass effects such as a bloom
// or blur cascade). Push makes `texture` (from ImPlatform_CreateRenderTexture) the current target
// with a full-size viewport and scissor, clearing it to transparent when `clear` is true; Pop
// restores the previous target. Nests up to 8 deep. Begin/EndRenderToTexture = Push(t, true)/Pop.
#if IMPLATFORM_GFX_SUPPORT_RENDER_TARGETS
IMPLATFORM_API bool ImPlatform_PushRenderTarget(ImTextureID texture, bool clear);
IMPLATFORM_API void ImPlatform_PopRenderTarget(void);

// Copy a rectangle of the current render target (the viewport being rendered, main or secondary,
// or the pushed render texture) into `dst`, from inside an ImDrawCallback. Coordinates are in
// pixels, top-left origin; the rectangle is clipped to both surfaces and MSAA is resolved.
// Returns false when nothing was copied.
IMPLATFORM_API bool ImPlatform_CopyCurrentTarget(ImTextureID dst, int src_x, int src_y, int w, int h, int dst_x, int dst_y);
#endif
#if IMPLATFORM_GFX_SUPPORT_DIRECT_DRAW
// Size in pixels of the current render target.
IMPLATFORM_API void ImPlatform_GetCurrentTargetSize(unsigned int* width, unsigned int* height);
#endif

// Render passes: Dear ImGui draw data rendered into a render texture before the frame, by the backend's own ImGui
// renderer (draw callbacks included), e.g. to cache vector drawings as images.
// The pixel format of pass targets (the format the backend's ImGui pipeline renders to).
IMPLATFORM_API ImPlatform_PixelFormat ImPlatform_GetRenderPassFormat(void);
// Queues `draw_data` to be rendered into `target` (ImPlatform_CreateRenderTexture() in ImPlatform_GetRenderPassFormat())
// at the start of the next ImPlatform_GfxAPIRender(), before the frame: draw_data->DisplayPos / DisplaySize map to the
// whole target (FramebufferScale 1). `clear`: the target is cleared to transparent first. One pass per frame (a later
// call replaces the queued one); `draw_data` must stay valid until then, and pending texture updates of the frame
// (ImGui's ImTextureData) are processed by the pass. On OpenGL its vertices and clip rectangles are flipped in place
// (render textures are stored bottom-up). Colors rendered over transparent come out premultiplied by alpha: draw them
// between ImPlatform_PushPremultipliedAlpha() and ImPlatform_PopPremultipliedAlpha(). False when unsupported.
IMPLATFORM_API bool ImPlatform_QueueRenderPass(ImTextureID target, ImDrawData* draw_data, bool clear);
// Draw commands of the window draw list (or `draw`) between them blend as premultiplied alpha (ONE, ONE_MINUS_SRC_ALPHA
// for color and alpha): texel times vertex color, both premultiplied (fading by a: color (a, a, a, a)). Through draw
// callbacks, like ImPlatform_PushSampler().
IMPLATFORM_API void ImPlatform_PushPremultipliedAlpha(ImDrawList* draw);
IMPLATFORM_API void ImPlatform_PopPremultipliedAlpha(ImDrawList* draw);

// Destroy a texture and free its resources
// texture_id: Texture to destroy
IMPLATFORM_API void ImPlatform_DestroyTexture(
    ImTextureID texture_id
);

// Override the sampler used by ImGui::Image() (and similar) for one or more draw calls.
// Call ImPlatform_PushSampler() before the image call and ImPlatform_PopSampler() after.
// Calls may be nested. On DX12, Vulkan and WGPU this relies on the standard backend draw callbacks
// (ImGuiPlatformIO::DrawCallback_SetSamplerLinear/Nearest): the filter is honored but the wrap mode stays clamp.
// On DX12 the sampler is part of the pipeline: between ImPlatform_BeginCustomShader() and ImPlatform_EndCustomShader()
// the program keeps its own (linear, clamped), and a sampler pushed there applies from the end of the scope.
// A sampler pushed around a custom shader is still the one of the draws after ImPlatform_EndCustomShader().
//
// Example:
//   ImPlatform_PushSampler(ImPlatform_TextureFilter_Nearest, ImPlatform_TextureWrap_Clamp);
//   ImGui::Image(pixelArtTex, size);
//   ImPlatform_PopSampler();
IMPLATFORM_API void ImPlatform_PushSampler(ImPlatform_TextureFilter filter, ImPlatform_TextureWrap wrap);
IMPLATFORM_API void ImPlatform_PopSampler(void);
// The same on a draw list (NULL: the current window's, as above): a render pass's list, a viewport's background list.
// Push and pop on the same list. These two are the ones a graphics backend implements.
IMPLATFORM_API void ImPlatform_PushSamplerEx(ImDrawList* draw, ImPlatform_TextureFilter filter, ImPlatform_TextureWrap wrap);
IMPLATFORM_API void ImPlatform_PopSamplerEx(ImDrawList* draw);

// ============================================================================
// Custom Vertex/Index Buffer Management API
// ============================================================================

// Buffer usage: how often the content changes and how long it holds.
// Static: written at creation, drawn many times; not updatable where it lives in device memory.
// Dynamic: kept across frames and updated at will, every frame if needed (one buffer per item): a draw recorded before
// an update keeps the content it was recorded with, see ImPlatform_UpdateVertexBuffer.
// Stream: content for one frame. It holds for the draws recorded in the frame the buffer was created in, or last
// updated as a whole in. Create it each frame (destroying it once its draws are recorded frees the handle only), or
// keep the handle and update the whole buffer each frame. DirectX 12 takes its memory from the per-frame upload arena:
// no resource is created or released and nothing waits for the GPU; DirectX 9, 10, 11, Vulkan, Metal and WebGPU give
// a destroyed Stream buffer to the next one of a close size (OpenGL leaves that to the driver: creating a buffer there
// is the upload). Drawn in a later frame without an update, a Stream buffer draws nothing on DirectX 12 (the other
// backends keep its content, which is not to be relied on).
typedef enum ImPlatform_BufferUsage {
    ImPlatform_BufferUsage_Static,
    ImPlatform_BufferUsage_Dynamic,
    ImPlatform_BufferUsage_Stream,
} ImPlatform_BufferUsage;

// Vertex attribute format - describes a single vertex element
typedef enum ImPlatform_VertexFormat {
    ImPlatform_VertexFormat_Float,       // Single float (4 bytes)
    ImPlatform_VertexFormat_Float2,      // Two floats (8 bytes) - e.g., UV coordinates
    ImPlatform_VertexFormat_Float3,      // Three floats (12 bytes) - e.g., position, normal
    ImPlatform_VertexFormat_Float4,      // Four floats (16 bytes) - e.g., color, tangent
    ImPlatform_VertexFormat_UByte4,      // Four unsigned bytes (4 bytes) - e.g., packed color
    ImPlatform_VertexFormat_UByte4N,     // Four unsigned bytes normalized to [0,1] (4 bytes)
} ImPlatform_VertexFormat;

// Index format - size of each index
typedef enum ImPlatform_IndexFormat {
    ImPlatform_IndexFormat_UInt16,       // 16-bit unsigned int (supports up to 65535 vertices)
    ImPlatform_IndexFormat_UInt32,       // 32-bit unsigned int (supports billions of vertices)
} ImPlatform_IndexFormat;

// Vertex attribute descriptor - describes one element in a vertex layout
typedef struct ImPlatform_VertexAttribute {
    ImPlatform_VertexFormat format;      // Format of this attribute
    unsigned int offset;                  // Byte offset within vertex structure
    const char* semantic_name;            // Semantic name (e.g., "POSITION", "TEXCOORD", "COLOR")
} ImPlatform_VertexAttribute;

// Vertex buffer descriptor - describes vertex buffer properties
typedef struct ImPlatform_VertexBufferDesc {
    unsigned int vertex_count;            // Number of vertices
    unsigned int vertex_stride;           // Size of one vertex in bytes
    ImPlatform_BufferUsage usage;         // Usage hint
    const ImPlatform_VertexAttribute* attributes; // Array of vertex attributes
    unsigned int attribute_count;         // Number of attributes in array
} ImPlatform_VertexBufferDesc;

// Index buffer descriptor - describes index buffer properties
typedef struct ImPlatform_IndexBufferDesc {
    unsigned int index_count;             // Number of indices
    ImPlatform_IndexFormat format;        // Index format (16-bit or 32-bit)
    ImPlatform_BufferUsage usage;         // Usage hint
} ImPlatform_IndexBufferDesc;

// Opaque handles for buffers
typedef void* ImPlatform_VertexBuffer;
typedef void* ImPlatform_IndexBuffer;

// Create a vertex buffer with initial data
// vertex_data: Pointer to vertex data (size must match vertex_count * vertex_stride)
// desc: Vertex buffer descriptor
// Returns: Vertex buffer handle or NULL on failure
// Note: The vertex data can be freed after this call returns (for Static usage)
#if IMPLATFORM_GFX_SUPPORT_VERTEX_BUFFERS
IMPLATFORM_API ImPlatform_VertexBuffer ImPlatform_CreateVertexBuffer(
    const void* vertex_data,
    const ImPlatform_VertexBufferDesc* desc
);

// Update vertex buffer data (for Dynamic/Stream usage)
// buffer: Vertex buffer to update
// vertex_data: New vertex data
// offset: Offset in vertices (NOT bytes)
// count: Number of vertices to update; the rest of the buffer keeps its content
// Returns: true on success, false on failure
// Draws already recorded, in this frame or in frames the GPU has not finished, keep the content they were recorded
// with, so a buffer may be updated every frame and between two draws of a frame: DirectX 12, Vulkan, Metal and WebGPU
// write another version of the buffer while the current one may still be read (and take it back once those frames
// have completed); DirectX 9, 10, 11 and OpenGL order the update with the draws. A Stream buffer takes an update of
// the whole buffer only once it was drawn (DirectX 9, 10, 11) or once its frame has passed (DirectX 12).
IMPLATFORM_API bool ImPlatform_UpdateVertexBuffer(
    ImPlatform_VertexBuffer buffer,
    const void* vertex_data,
    unsigned int offset,
    unsigned int count
);

// Destroy a vertex buffer and free its resources
// buffer: Vertex buffer to destroy
IMPLATFORM_API void ImPlatform_DestroyVertexBuffer(
    ImPlatform_VertexBuffer buffer
);

// Create an index buffer with initial data
// index_data: Pointer to index data
// desc: Index buffer descriptor
// Returns: Index buffer handle or NULL on failure
// Note: The index data can be freed after this call returns (for Static usage)
IMPLATFORM_API ImPlatform_IndexBuffer ImPlatform_CreateIndexBuffer(
    const void* index_data,
    const ImPlatform_IndexBufferDesc* desc
);

// Update index buffer data (for Dynamic/Stream usage), as ImPlatform_UpdateVertexBuffer
// buffer: Index buffer to update
// index_data: New index data
// offset: Offset in indices (NOT bytes)
// count: Number of indices to update; the rest of the buffer keeps its content
// Returns: true on success, false on failure
IMPLATFORM_API bool ImPlatform_UpdateIndexBuffer(
    ImPlatform_IndexBuffer buffer,
    const void* index_data,
    unsigned int offset,
    unsigned int count
);

// Destroy an index buffer and free its resources
// buffer: Index buffer to destroy
IMPLATFORM_API void ImPlatform_DestroyIndexBuffer(
    ImPlatform_IndexBuffer buffer
);

// Bind vertex and index buffers for rendering
// vertex_buffer: Vertex buffer to bind (or NULL to unbind)
// index_buffer: Index buffer to bind (or NULL to unbind)
IMPLATFORM_API void ImPlatform_BindBuffers(
    ImPlatform_VertexBuffer vertex_buffer,
    ImPlatform_IndexBuffer index_buffer
);

// Draw primitives using currently bound buffers
// primitive_type: Type of primitive (0=triangles, 1=lines, 2=points)
// index_count: Number of indices to draw (0 = draw all indices in buffer)
// start_index: Starting index in index buffer
IMPLATFORM_API void ImPlatform_DrawIndexed(
    unsigned int primitive_type,
    unsigned int index_count,
    unsigned int start_index
);
#endif // IMPLATFORM_GFX_SUPPORT_VERTEX_BUFFERS

// ============================================================================
// Custom Shader System API
// ============================================================================

// Shader stage types
typedef enum ImPlatform_ShaderStage {
    ImPlatform_ShaderStage_Vertex,       // Vertex shader
    ImPlatform_ShaderStage_Fragment,     // Fragment/Pixel shader
    ImPlatform_ShaderStage_Compute,      // Compute shader (if supported)
} ImPlatform_ShaderStage;

// Shader source format
typedef enum ImPlatform_ShaderFormat {
    ImPlatform_ShaderFormat_GLSL,        // OpenGL GLSL source code
    ImPlatform_ShaderFormat_HLSL,        // DirectX HLSL source code
    ImPlatform_ShaderFormat_MSL,         // Metal Shading Language source code
    ImPlatform_ShaderFormat_SPIRV,       // SPIR-V bytecode (Vulkan)
    ImPlatform_ShaderFormat_WGSL,        // WebGPU Shading Language
} ImPlatform_ShaderFormat;

// Shader compile flags — backend hints for the source-compile path.
// Currently only DX11/DX12 honor them; other backends are free to extend.
#define IMPLATFORM_SHADER_COMPILE_DEFAULT             0           // Backend default (moderate optimization)
#define IMPLATFORM_SHADER_COMPILE_SKIP_OPTIMIZATION   (1u << 0)   // Fastest compile, worst runtime perf
#define IMPLATFORM_SHADER_COMPILE_OPTIMIZATION_LOW    (1u << 1)   // Minimum optimization
#define IMPLATFORM_SHADER_COMPILE_OPTIMIZATION_HIGH   (1u << 2)   // Maximum optimization (slowest compile)
#define IMPLATFORM_SHADER_COMPILE_DEBUG               (1u << 3)   // Include debug info in bytecode

// Shader descriptor - describes shader source
typedef struct ImPlatform_ShaderDesc {
    ImPlatform_ShaderStage stage;        // Shader stage (vertex, fragment, etc.)
    ImPlatform_ShaderFormat format;      // Source code format
    const char* source_code;              // Shader source code string
    const void* bytecode;                 // Pre-compiled bytecode (optional, for SPIRV/DXIL)
    unsigned int bytecode_size;           // Size of bytecode in bytes
    const char* entry_point;              // Entry point function name (e.g., "main", "VSMain")

    // Bytecode disk cache (optional). When non-NULL, the backend hashes the
    // source+entry+profile and looks for a cached bytecode file at
    //   ./shaders/bytecode_cache/<cache_key>_<entry>_<hash>.<ext>
    // On cache hit the backend skips its compile step entirely; on miss it
    // compiles from source and saves the resulting bytecode for next launch.
    // Any change to the source invalidates the cache automatically.
    // Ignored when `bytecode` is supplied directly.
    const char* cache_key;

    // Backend compile flags (bitmask of IMPLATFORM_SHADER_COMPILE_*).
    // Only used on the source-compile path (ignored for bytecode input).
    unsigned int compile_flags;

    // Preprocessor definitions put before the source (HLSL, GLSL after its #version line, MSL), ';' or new lines
    // between them: "CLIP;MODE=2" defines CLIP (for #ifdef CLIP) and MODE as 2 (for #if MODE == 2). One shader per
    // combination is a permutation of the same source; the bytecode cache keeps one entry per combination. Compiler
    // line numbers stay those of source_code. Ignored for bytecode, SPIR-V and WGSL (no preprocessor).
    const char* defines;
} ImPlatform_ShaderDesc;

// Opaque handle for shader
typedef void* ImPlatform_Shader;

// Opaque handle for shader program (linked vertex + fragment shaders)
typedef void* ImPlatform_ShaderProgram;

// Create a shader from source code or bytecode
// desc: Shader descriptor
// Returns: Shader handle or NULL on failure
// Note: Compile errors are printed to stderr and kept for ImPlatform_GetShaderLog
IMPLATFORM_API ImPlatform_Shader ImPlatform_CreateShader(
    const ImPlatform_ShaderDesc* desc
);

// The messages of the most recent ImPlatform_CreateShader, ImPlatform_CreateShaderFromParts or
// ImPlatform_CreateShaderProgram call that returned NULL (the compiler's or linker's errors; for shader parts also the
// generated source, with line numbers): text to show the user. Valid until the next failing call; "" before any.
// Programs compiled with ImPlatform_CreateShaderProgramAsync report through ImPlatform_GetShaderJobLog instead.
IMPLATFORM_API const char* ImPlatform_GetShaderLog(void);

// Destroy a shader and free its resources
// shader: Shader to destroy
IMPLATFORM_API void ImPlatform_DestroyShader(
    ImPlatform_Shader shader
);

// Create a shader program by linking vertex and fragment shaders
// vertex_shader: Vertex shader handle
// fragment_shader: Fragment shader handle
// Returns: Shader program handle or NULL on failure
IMPLATFORM_API ImPlatform_ShaderProgram ImPlatform_CreateShaderProgram(
    ImPlatform_Shader vertex_shader,
    ImPlatform_Shader fragment_shader
);

// Destroy a shader program and free its resources
// program: Shader program to destroy
IMPLATFORM_API void ImPlatform_DestroyShaderProgram(
    ImPlatform_ShaderProgram program
);

// Blending of a program's draws: result = op(source * src factor, target * dst factor), for color and for alpha.
typedef enum ImPlatform_BlendFactor {
    ImPlatform_BlendFactor_Zero,
    ImPlatform_BlendFactor_One,
    ImPlatform_BlendFactor_SrcColor,
    ImPlatform_BlendFactor_InvSrcColor,  // 1 - source color
    ImPlatform_BlendFactor_SrcAlpha,
    ImPlatform_BlendFactor_InvSrcAlpha,  // 1 - source alpha
    ImPlatform_BlendFactor_DstColor,
    ImPlatform_BlendFactor_InvDstColor,  // 1 - target color
    ImPlatform_BlendFactor_DstAlpha,
    ImPlatform_BlendFactor_InvDstAlpha,  // 1 - target alpha
} ImPlatform_BlendFactor;

typedef enum ImPlatform_BlendOp {
    ImPlatform_BlendOp_Add,              // source + target
    ImPlatform_BlendOp_Subtract,         // source - target
    ImPlatform_BlendOp_RevSubtract,      // target - source
    ImPlatform_BlendOp_Min,              // min(source, target): the factors are not used
    ImPlatform_BlendOp_Max,              // max(source, target): the factors are not used
} ImPlatform_BlendOp;

typedef struct ImPlatform_BlendDesc {
    bool enable;                          // false: opaque, the source replaces the target
    ImPlatform_BlendFactor src_color;
    ImPlatform_BlendFactor dst_color;
    ImPlatform_BlendOp     op_color;
    ImPlatform_BlendFactor src_alpha;
    ImPlatform_BlendFactor dst_alpha;
    ImPlatform_BlendOp     op_alpha;
} ImPlatform_BlendDesc;

typedef enum ImPlatform_BlendPreset {
    ImPlatform_BlendPreset_Alpha,         // Straight alpha "over" (Dear ImGui's): SrcAlpha, InvSrcAlpha; alpha One, InvSrcAlpha
    ImPlatform_BlendPreset_Premultiplied, // Premultiplied alpha "over": One, InvSrcAlpha
    ImPlatform_BlendPreset_Opaque,        // No blending
    // The following keep the target's alpha (alpha Zero, One). Additive and Subtractive fade with the source alpha;
    // the others take the source color as it is.
    ImPlatform_BlendPreset_Additive,      // target + source: SrcAlpha, One
    ImPlatform_BlendPreset_Subtractive,   // target - source: SrcAlpha, One, RevSubtract
    ImPlatform_BlendPreset_Multiplicative,// target * source: DstColor, Zero
    ImPlatform_BlendPreset_Screen,        // 1 - (1 - target) * (1 - source): One, InvSrcColor
    ImPlatform_BlendPreset_Darken,        // min(source, target)
    ImPlatform_BlendPreset_Lighten,       // max(source, target)
} ImPlatform_BlendPreset;
IMPLATFORM_API ImPlatform_BlendDesc ImPlatform_BlendDesc_Preset(ImPlatform_BlendPreset preset);

// Set the blending of the draws made with a program: Dear ImGui's draws between ImPlatform_BeginCustomShader and
// ImPlatform_EndCustomShader, ImPlatform_DrawIndexed and ImPlatform_DrawQuadNow (ImPlatform_DrawMesh has its own
// alpha_blend). It is read when the program is bound, so set it once after creating the program, or from a draw
// callback before binding it. blend: NULL for Dear ImGui's blending (the default). Where blending is a device state
// (Direct3D 9 to 11, OpenGL) it stays until ImPlatform_EndCustomShader: end one program's draws before another's.
// Returns false when the arguments are invalid.
IMPLATFORM_API bool ImPlatform_SetShaderProgramBlend(
    ImPlatform_ShaderProgram program,
    const ImPlatform_BlendDesc* blend
);
// The same by name: ImPlatform_SetShaderProgramBlendPreset(program, ImPlatform_BlendPreset_Additive).
IMPLATFORM_API bool ImPlatform_SetShaderProgramBlendPreset(
    ImPlatform_ShaderProgram program,
    ImPlatform_BlendPreset preset
);

#if IMPLATFORM_GFX_SUPPORT_VERTEX_BUFFERS
// Create a vertex input layout for a vertex buffer and the program that draws it: call it once after
// ImPlatform_CreateVertexBuffer and ImPlatform_CreateShaderProgram, on every backend (where the layout is built at draw
// time, it only checks its arguments). The layout is derived from the vertex buffer's ImPlatform_VertexAttribute array;
// duplicate semantic names take indices 0, 1, ... in order (TEXCOORD, TEXCOORD -> TEXCOORD0, TEXCOORD1). Direct3D
// matches them to the vertex shader's semantics (Direct3D 9 by usage), the other APIs take attribute i at location i.
// After this call, ImPlatform_BindBuffers activates the layout. Returns true on success.
IMPLATFORM_API bool ImPlatform_CreateVertexInputLayout(
    ImPlatform_VertexBuffer vertex_buffer,
    ImPlatform_ShaderProgram program
);
#endif

// Bind a shader program for rendering
// program: Shader program to bind (or NULL to unbind)
IMPLATFORM_API void ImPlatform_BindShaderProgram(
    ImPlatform_ShaderProgram program
);

// Set shader uniform/constant buffer values
// program: Shader program
// name: Uniform variable name (e.g., "u_ModelViewProjection")
// data: Pointer to uniform data
// size: Size of data in bytes
// Returns: true on success, false if uniform not found
IMPLATFORM_API bool ImPlatform_SetShaderUniform(
    ImPlatform_ShaderProgram program,
    const char* name,
    const void* data,
    unsigned int size
);

// Begin accumulating uniforms for batched upload
// This allows you to set multiple uniforms and upload them together
// Usage:
//   ImPlatform_BeginUniformBlock(program);
//   ImPlatform_SetUniform("ColorStart", &color1, sizeof(ImVec4));
//   ImPlatform_SetUniform("ColorEnd", &color2, sizeof(ImVec4));
//   ImPlatform_EndUniformBlock(program);
// program: Shader program to set uniforms on
IMPLATFORM_API void ImPlatform_BeginUniformBlock(
    ImPlatform_ShaderProgram program
);

// Add a uniform to the current uniform block
// Must be called between BeginUniformBlock and EndUniformBlock
// name: Uniform name in the shader
// data: Pointer to uniform data
// size: Size of data in bytes
// Returns: true on success, false if block is full or not started
IMPLATFORM_API bool ImPlatform_SetUniform(
    const char* name,
    const void* data,
    unsigned int size
);

// Finalize and upload the uniform block to the GPU
// program: Shader program (must match the one from BeginUniformBlock)
IMPLATFORM_API void ImPlatform_EndUniformBlock(
    ImPlatform_ShaderProgram program
);

// Set a texture of a custom shader: HLSL register t<slot>.
// Slots 1..8: the program keeps the texture for its later draws; inside a draw callback with the program bound it also
// applies at once. Slot 0 is the texture of the direct draws that follow (ImPlatform_DrawIndexed, ImPlatform_DrawQuadNow);
// Dear ImGui's draws bind their own.
// program: Shader program
// name: HLSL texture name (used by OpenGL)
// slot: Texture register
// texture: An ImPlatform texture; it must stay alive while set
// Returns: false when the arguments are invalid
#if IMPLATFORM_GFX_SUPPORT_SHADER_TEXTURES
IMPLATFORM_API bool ImPlatform_SetShaderTexture(
    ImPlatform_ShaderProgram program,
    const char* name,
    unsigned int slot,
    ImTextureID texture
);
#endif

// Begin using a custom shader for ImDrawList rendering
// Adds a callback to the drawlist to switch to the custom shader
// draw: ImGui draw list
// shader: Shader program to use for subsequent draw calls
IMPLATFORM_API void ImPlatform_BeginCustomShader(
    ImDrawList* draw,
    ImPlatform_ShaderProgram shader
);
// Activate a custom shader immediately (for use inside draw callbacks), for the application's own draws
// (ImPlatform_DrawIndexed). On DirectX 9 with Dear ImGui 1.93.1 and later, Dear ImGui's own draws take a program from
// ImPlatform_BeginCustomShader() only: their pre-transformed vertices need the declaration it sets.
IMPLATFORM_API void ImPlatform_BeginCustomShader_Render(ImPlatform_ShaderProgram program);

// Upload per-draw-call shader constants and bind them for the next draw.
// data: Pointer to constant data
// size: Size of constant data in bytes
// Returns an opaque handle that must be freed with ImPlatform_PopShaderConstants.
#if IMPLATFORM_GFX_SUPPORT_DIRECT_DRAW
IMPLATFORM_API void* ImPlatform_PushShaderConstants(const void* data, unsigned int size);
IMPLATFORM_API void  ImPlatform_PopShaderConstants(void* handle);
#endif

// End custom shader usage and restore default ImGui rendering state
// Adds a callback to reset render state to ImGui defaults
// draw: ImGui draw list
IMPLATFORM_API void ImPlatform_EndCustomShader(
    ImDrawList* draw
);

// Helper function to draw a fullscreen quad with a custom shader
// Automatically handles BeginCustomShader/EndCustomShader and draws a quad
// program: Shader program to use
// This is equivalent to:
//   ImPlatform_BeginCustomShader(draw, program);
//   draw->AddImageQuad(...);  // fullscreen quad
//   ImPlatform_EndCustomShader(draw);
IMPLATFORM_API void ImPlatform_DrawCustomShaderQuad(
    ImPlatform_ShaderProgram program
);

// Draw a quad now (not queued in a draw list) with `program` and its pending uniforms / textures,
// e.g. one pass of a blur cascade inside an ImDrawCallback:
// dst_rect = (x0, y0, x1, y1) in pixels of the current target, uv_rect = (u0, v0, u1, v1).
// Changes vertex / index buffers and shaders: follow it with ImGui::GetPlatformIO().DrawCallback_ResetRenderState
// (ImPlatform_EndCustomShader adds one) before Dear ImGui draws again.
#if IMPLATFORM_GFX_SUPPORT_RENDER_TARGETS
IMPLATFORM_API void ImPlatform_DrawQuadNow(ImPlatform_ShaderProgram program, ImVec4 dst_rect, ImVec4 uv_rect);
#endif

// ----------------------------------------------------------------------------
// Shader parts: a custom shader written once, completed per shading language at run time (HLSL on Direct3D 9 to 12,
// GLSL 330 on OpenGL, MSL on Metal). The application gives its parameter block, extra varyings, textures, helper
// functions and the vertex / pixel bodies; ImPlatform writes the entry points, inputs, outputs and declarations.
// The text is HLSL plus these macros, which every language maps:
//   IMP_SAMPLE(tex, uv), IMP_SAMPLE_LEVEL(tex, uv, lod) (uv a float3 for a 3D texture), IMP_LOAD(tex, int2 xy) (2D),
//   IMP_MUL(matrix, vector),
//   IMP_CAST(type, x) (instead of C-style casts), IMP_OUT(type) / IMP_INOUT(type) (function parameters written by the
//   function; MSL has no out / inout), IMP_UNROLL / IMP_LOOP / IMP_BRANCH (loop and branch attributes),
//   IMP_HLSL / IMP_GLSL / IMP_MSL (defined for the language being generated), IMP_SM3 (Direct3D 9, shader model 3:
//   224 pixel constant registers, no integer or bit operations, no texel loads, no flat varyings: nointerpolation is
//   ignored there; a shader can take another path).
// lerp, frac, saturate, rcp, rsqrt, atan2, fmod, mad, ddx, ddy, mul, clip (scalar) and discard are available
// everywhere; asuint, asint, asfloat (of a uint) and f16tof32 everywhere but Direct3D 9 (shader model 3 has no bit
// operations). Write float literals (1.0, not 1), construct vectors fully (float2(x, x), not x.xx), give pow a vector
// exponent with a vector base, compare vectors component by component, do not index matrices, keep 'static const' data
// in functions, do not name functions or variables after GLSL / MSL built-ins (cross, distance, step...), write uint
// literals with u (GLSL 330 does not convert int to uint), fill arrays element by element (no {...} initializers), and
// do not 'return' from a body (assign col_out / position).
// Names in scope:
//   both stages  ProjMtx (b0), the parameters by member name (b1), texture0 (t0: the draw's texture) and the named
//                textures (pixel stage), the helper functions
//   vertex body  in_pos, in_uv, in_col (Dear ImGui's vertex); v.col, v.uv and v.<varying> to write; position (clip
//                space, preset to mul(ProjMtx, float4(in_pos, 0, 1))). With vertex_inputs: in_<name> for each input
//                instead, v.col / v.uv preset to 0 and position to float4(0, 0, 0, 1) (the body sets it). A row of
//                ProjMtx is IMP_MUL(float4(1.0, 0.0, 0.0, 0.0), ProjMtx) (matrices are not indexed).
//   pixel body   v.col, v.uv, v.<varying>; col_out (preset to v.col * texture0 at v.uv)
// Parameter block layout, identical in HLSL cbuffers, GLSL std140 and MSL: float, float2, float4, int / uint vectors
// and float4x4 members; arrays of 16-byte types only; a float3 starts a 16-byte row (a scalar may follow it); no bool.
// Set it with ImPlatform_SetShaderUniform / ImPlatform_PushShaderConstants and the same C struct.
typedef struct ImPlatform_ShaderParts {
    const char* params;         // Members of the parameter block (b1): "float4 col0; float4 col1; float2 uv_start;"
    const char* varyings;       // Extra vertex -> pixel values: "float2 world; nointerpolation int4 id;" (up to 7;
                                // integer ones need nointerpolation, which Direct3D 9 does not have)
    const char* vertex_inputs;  // Vertex inputs other than Dear ImGui's (optional, for ImPlatform_DrawIndexed):
                                // "float4 pos : POSITION; float4 tex : TEXCOORD0;", in the vertex buffer's attribute order
    const char* textures[9];    // Names of t0..t8 (index = register); t0 (the draw's texture) is texture0 unless named
    unsigned int textures_3d;   // Bit N: tN is a 3D texture (ImPlatform_CreateTexture3D), sampled with a float3
    const char* defines;        // Preprocessor variants, ';' or new lines: "CLIP=1;MODE=2" or "#define MODE 2"
    const char* functions;      // Helper functions shared by both stages
    const char* vs_body;        // Vertex body (optional): changes position / v.* after the defaults
    const char* ps_body;        // Pixel body (optional): writes col_out
    const char* cache_key;      // As ImPlatform_ShaderDesc::cache_key (optional)
    unsigned int compile_flags; // As ImPlatform_ShaderDesc::compile_flags (IMPLATFORM_SHADER_COMPILE_*)
} ImPlatform_ShaderParts;

// The complete source of one stage in `format` (HLSL, GLSL or MSL), whatever the backend: for inspection, or for an
// IM_GFX_CUSTOM backend. Writes up to out_size - 1 characters plus a terminator; returns the full length, -1 for an
// unsupported format or stage.
IMPLATFORM_API int ImPlatform_GenerateShaderSource(const ImPlatform_ShaderParts* parts, ImPlatform_ShaderStage stage,
                                                   ImPlatform_ShaderFormat format, char* out, int out_size);
// Parts from one text (a shader file): lines "//@params", "//@inputs" (vertex_inputs), "//@varyings", "//@textures",
// "//@defines", "//@functions", "//@vs" and "//@ps" start the sections (text before the first one is ignored); texture lines are "<slot> <name>", or
// "<slot> <name> 3d" for a 3D texture.
// `text` is split in place and must outlive `parts`; fields of absent sections are left as they were. Returns false
// when the text has no section.
IMPLATFORM_API bool ImPlatform_ParseShaderParts(char* text, ImPlatform_ShaderParts* parts);
#if IMPLATFORM_GFX_SUPPORT_SHADER_PARTS
// Generates and compiles one stage for the backend (entry points vs_main / ps_main, or main in GLSL). Destroy with
// ImPlatform_DestroyShader; link two stages with ImPlatform_CreateShaderProgram. NULL when the generated source does
// not compile (the source and the compiler messages are printed).
IMPLATFORM_API ImPlatform_Shader ImPlatform_CreateShaderFromParts(const ImPlatform_ShaderParts* parts, ImPlatform_ShaderStage stage);
#endif

// ----------------------------------------------------------------------------
// Asynchronous compilation: a program compiled without blocking the frame, e.g. to show "shader compilation in
// progress" in its place.
//   static ImPlatform_ShaderJob job = ImPlatform_CreateShaderProgramAsync(&vs_desc, &ps_desc);
//   ImPlatform_ShaderProgram program = NULL;
//   switch (ImPlatform_GetShaderJobStatus(job, &program)) {
//   case ImPlatform_ShaderStatus_Compiling: ImGui::TextUnformatted("Shader compilation in progress"); break;
//   case ImPlatform_ShaderStatus_Failed:    ImGui::TextUnformatted(ImPlatform_GetShaderJobLog(job)); break;
//   case ImPlatform_ShaderStatus_Ready:     ImPlatform_DrawCustomShaderQuad(program); break;
//   }
// Direct3D 10, 11 and 12 compile the HLSL on a worker thread (one for all jobs, in creation order). The other
// backends compile inside ImPlatform_CreateShaderProgramAsync, on the calling thread (OpenGL compiles on the thread of
// its context; Vulkan and WebGPU take SPIR-V / WGSL and build their pipelines at the first draw): the job is Ready or
// Failed at once there. All these functions are called on the graphics thread.
typedef enum ImPlatform_ShaderStatus {
    ImPlatform_ShaderStatus_Compiling,
    ImPlatform_ShaderStatus_Ready,
    ImPlatform_ShaderStatus_Failed,
} ImPlatform_ShaderStatus;

// Opaque handle for a program being compiled
typedef void* ImPlatform_ShaderJob;

// Starts compiling a vertex and a fragment shader into a program and returns at once. The descriptors and what they
// point to (source, bytecode, names, defines) are copied. NULL when a descriptor is missing.
IMPLATFORM_API ImPlatform_ShaderJob ImPlatform_CreateShaderProgramAsync(const ImPlatform_ShaderDesc* vertex_desc,
                                                                       const ImPlatform_ShaderDesc* fragment_desc);
#if IMPLATFORM_GFX_SUPPORT_SHADER_PARTS
// The same from shader parts: both stages are generated here and compiled like the descriptors above.
IMPLATFORM_API ImPlatform_ShaderJob ImPlatform_CreateShaderProgramFromPartsAsync(const ImPlatform_ShaderParts* parts);
#endif
#if IMPLATFORM_GFX_SUPPORT_COMPUTE
// The same for one compute shader (stage ImPlatform_ShaderStage_Compute). Once ImPlatform_GetShaderJobStatus(job, NULL)
// is Ready, ImPlatform_GetShaderJobShader gives the shader for ImPlatform_DispatchCompute (NULL before, and for a
// program's job); the job owns it. Its pipeline is still created by the first dispatch (DirectX 12, Vulkan, OpenGL).
IMPLATFORM_API ImPlatform_ShaderJob ImPlatform_CreateComputeShaderAsync(const ImPlatform_ShaderDesc* desc);
IMPLATFORM_API ImPlatform_Shader ImPlatform_GetShaderJobShader(ImPlatform_ShaderJob job);
#endif
// The state of a job, e.g. once per frame. When the compilation has ended, the first call creates the program (device
// objects are created here, on the graphics thread). Ready: *out_program (optional) receives the program, the same on
// every call and valid until ImPlatform_DestroyShaderJob; otherwise it receives NULL. Failed: see
// ImPlatform_GetShaderJobLog.
IMPLATFORM_API ImPlatform_ShaderStatus ImPlatform_GetShaderJobStatus(ImPlatform_ShaderJob job, ImPlatform_ShaderProgram* out_program);
// The compiler and linker messages of a job, both stages: "" while compiling and when there are none.
IMPLATFORM_API const char* ImPlatform_GetShaderJobLog(ImPlatform_ShaderJob job);
// Destroys a job with its program and its stage shaders (the job owns them: do not call
// ImPlatform_DestroyShaderProgram on its program), waiting for it when it is being compiled.
IMPLATFORM_API void ImPlatform_DestroyShaderJob(ImPlatform_ShaderJob job);

// ============================================================================
// Readback and Compute API (optional capabilities)
// ============================================================================
// Declared where the backend implements them (IMPLATFORM_GFX_SUPPORT_READBACK, _ASYNC_READBACK, _COMPUTE); the
// Supports queries exist everywhere (OpenGL checks 4.3 for compute at run time).

IMPLATFORM_API bool ImPlatform_SupportsReadback(void);

#if IMPLATFORM_GFX_SUPPORT_READBACK
// Copy a width x height region of a 2D texture (from ImPlatform_CreateTexture, ImPlatform_CreateRenderTexture or
// ImPlatform_CreateStorageTexture) to CPU memory: rows tightly packed, in the texture's pixel format.
// Synchronous: returns once the GPU work writing the texture has completed (it doubles as a fence).
// out_size must be at least width * height * bytes per pixel.
IMPLATFORM_API bool ImPlatform_ReadTexture(ImTextureID texture, unsigned int x, unsigned int y,
                                           unsigned int width, unsigned int height, void* out_pixels, size_t out_size);
#endif

// Non-blocking readback: the copy is queued (and submitted) now; poll until it reports ready, typically one or two
// frames later. Poll returns 1 when out_pixels was filled, 0 while the GPU is still working, -1 on failure.
// Release every readback (also after a successful poll). Same pixel layout as ImPlatform_ReadTexture.
typedef struct ImPlatform_Readback_t* ImPlatform_Readback;
#if IMPLATFORM_GFX_SUPPORT_ASYNC_READBACK
IMPLATFORM_API ImPlatform_Readback ImPlatform_BeginReadTexture(ImTextureID texture, unsigned int x, unsigned int y,
                                                               unsigned int width, unsigned int height);
IMPLATFORM_API int  ImPlatform_PollReadback(ImPlatform_Readback readback, void* out_pixels, size_t out_size);
// As PollReadback without the copy: when ready (1), *pixels points at the rows (*row_pitch bytes apart, at least
// width * bytes per pixel) until the readback is released. Reading them needs no further API calls, so worker threads
// may consume them (convert or copy in parallel).
IMPLATFORM_API int  ImPlatform_MapReadback(ImPlatform_Readback readback, const void** pixels, size_t* row_pitch);
IMPLATFORM_API void ImPlatform_ReleaseReadback(ImPlatform_Readback readback);
#endif

#if IMPLATFORM_GFX_SUPPORT_RENDER_TARGETS && IMPLATFORM_GFX_SUPPORT_READBACK
// Screenshots: the image of a viewport as rendered this frame (NULL: the main viewport; with multi-viewports, a
// secondary platform window, except on DirectX 12 and Vulkan, where their targets belong to Dear ImGui's backend: 0).
// ImPlatform_CaptureViewport: call it during the frame, once what is to be captured has been submitted and before
// ImGui::Render(). It adds a draw callback at the end of the viewport's foreground draw list (what that list receives
// afterwards is not captured) and returns a render texture of the viewport's size in pixels (*out_width, *out_height)
// that holds the image once the frame has been rendered: draw it (a thumbnail), read it with ImPlatform_ReadCapture,
// destroy it with ImPlatform_DestroyTexture after the frame. 0 when the viewport has no size.
// ImPlatform_ReadCapture: after the frame was rendered (ImPlatform_GfxAPIRender; ImPlatform_GfxViewportPost for a
// secondary viewport), the pixels as RGBA8, rows top-down, alpha 255. out_size >= width * height * 4. Writing an image
// file from them is left to the application.
IMPLATFORM_API ImTextureID ImPlatform_CaptureViewport(ImGuiViewport* viewport, unsigned int* out_width, unsigned int* out_height);
IMPLATFORM_API bool ImPlatform_ReadCapture(ImTextureID capture, unsigned int width, unsigned int height, void* out_rgba, size_t out_size);
#endif

IMPLATFORM_API bool ImPlatform_SupportsCompute(void);

#if IMPLATFORM_GFX_SUPPORT_COMPUTE
// Create a texture that compute shaders can write (UAV / storage image) and that ImGui can draw.
// pixel_data may be NULL (the texture starts zeroed). Destroy with ImPlatform_DestroyTexture.
IMPLATFORM_API ImTextureID ImPlatform_CreateStorageTexture(const void* pixel_data, const ImPlatform_TextureDesc* desc);

// Run a compute shader (ImPlatform_CreateShader with ImPlatform_ShaderStage_Compute).
// HLSL bindings: inputs -> t0.. (read-only), outputs -> u0.. (storage textures), constants -> b0 (may be NULL),
// s0 = linear clamp sampler. Bindings are cleared afterwards so outputs can be drawn or read back.
// GLSL (OpenGL 4.3): inputs -> sampler2D at binding 0.., outputs -> image2D at binding 0.. (format qualifier
// matching the texture, e.g. rgba8), constants -> std140 uniform block at binding 0.
// SPIR-V (Vulkan), all in set 0: constants -> uniform buffer at binding 0, inputs -> combined image samplers at
// bindings 1..8 (linear clamp), outputs -> storage images at bindings 9..16; entry point "main".
// Call outside the ImGui render callbacks (e.g. before ImGui::Render); work is ordered with later drawing.
// Vulkan waits for the dispatch to complete before returning (ImPlatform_DispatchComputeAsync does not).
IMPLATFORM_API bool ImPlatform_DispatchCompute(ImPlatform_Shader compute_shader,
                                               const ImTextureID* inputs, unsigned int input_count,
                                               const ImTextureID* outputs, unsigned int output_count,
                                               const void* constants, unsigned int constants_size,
                                               unsigned int groups_x, unsigned int groups_y, unsigned int groups_z);
#endif

typedef struct ImPlatform_ComputeDispatch_t* ImPlatform_ComputeDispatch;
#if IMPLATFORM_GFX_SUPPORT_ASYNC_COMPUTE
// ImPlatform_DispatchCompute that never waits for the GPU and returns a handle to ask whether the dispatch has
// completed, e.g. to show "computing" until an output is ready to be drawn or read back:
//   static ImPlatform_ComputeDispatch dispatch = ImPlatform_DispatchComputeAsync(cs, &in, 1, &out, 1, NULL, 0, w / 8, h / 8, 1);
//   if (ImPlatform_PollComputeDispatch(dispatch) == 1) ImGui::Image(out, size); else ImGui::TextUnformatted("Computing");
// NULL when the dispatch fails. GPU work completes in order: for a chain of dispatches, poll the last one. The
// dispatch runs on the queue of the frames, so a long one still delays the frames submitted after it.
IMPLATFORM_API ImPlatform_ComputeDispatch ImPlatform_DispatchComputeAsync(ImPlatform_Shader compute_shader,
                                               const ImTextureID* inputs, unsigned int input_count,
                                               const ImTextureID* outputs, unsigned int output_count,
                                               const void* constants, unsigned int constants_size,
                                               unsigned int groups_x, unsigned int groups_y, unsigned int groups_z);
// 1: completed (the outputs are written), 0: not yet, -1: error (NULL handle, device lost). Does not wait.
IMPLATFORM_API int  ImPlatform_PollComputeDispatch(ImPlatform_ComputeDispatch dispatch);
// Frees the handle, completed or not: the dispatch itself still runs. Vulkan waits for a dispatch that has not completed.
IMPLATFORM_API void ImPlatform_ReleaseComputeDispatch(ImPlatform_ComputeDispatch dispatch);
#endif

// ============================================================================
// Device loss (IMPLATFORM_GFX_SUPPORT_DEVICE_CONTROL: DirectX 11 and DirectX 12)
// ============================================================================
#if IMPLATFORM_GFX_SUPPORT_DEVICE_CONTROL
// True once the graphics device has been removed or reset (driver update, TDR, unplugged adapter): every GPU
// resource is gone and further rendering fails. The application keeps its CPU data and must recreate the device
// (or restart) to draw again.
IMPLATFORM_API bool ImPlatform_GfxDeviceLost(void);
// True while a device can be used: after ImPlatform_InitGfx succeeded, and neither removed, being recreated, nor shut
// down (ImPlatform_ShutdownGfxAPI / ImPlatform_ShutdownWindow). False before initialization, unlike
// !ImPlatform_GfxDeviceLost(). ImPlatform_SupportsCompute / ImPlatform_SupportsReadback report features of the
// backend, not this state. DirectX 11/12; other backends report false.
IMPLATFORM_API bool ImPlatform_GfxDeviceReady(void);
// Testing aid: removes the device on purpose where the API allows it (DirectX 12: ID3D12Device5::RemoveDevice);
// returns false when unsupported.
IMPLATFORM_API bool ImPlatform_GfxDebugRemoveDevice(void);
// Recreates the device, swap chain and ImGui renderer backend on the same window (DirectX 12; other backends return
// false). Call between frames, after destroying every texture, shader, program and buffer created on the old device:
// they are all invalid afterwards and their IDs may be handed out again. Outstanding asynchronous readbacks are released
// by the recreation itself (their handles then report -1 when polled; release them as usual). Returns false when no
// device could be created.
IMPLATFORM_API bool ImPlatform_GfxRecreateDevice(void);
// Testing aid: texture creation fails, as on device memory exhaustion, once the textures created through ImPlatform
// would exceed `bytes` (0 removes the limit). DirectX 12; ignored elsewhere.
IMPLATFORM_API void ImPlatform_GfxDebugSetMemoryLimit(size_t bytes);
// Testing aids for allocation rollback (DirectX 12; ignored elsewhere, counts 0). FailTextureStep: the next texture
// creation fails at allocation step `step` (0 resource, 1 ImGui-visible SRV, 2 CPU SRV, 3 UAV, 4 RTV, 5 depth resource,
// 6 DSV, 7 initial upload; -1 none). LiveCounts drains the GPU, processes deferred releases, then reports the live
// texture resources and allocated descriptors. All testing aids: call on the graphics thread (the one creating textures
// and rendering); disabled by default; an injected failure is consumed by the next texture creation and must be reset
// step leaves it armed: reset it with -1). LiveCounts waits for the GPU to go idle: for tests and diagnostics, never
// for routine per-frame use.
IMPLATFORM_API void ImPlatform_GfxDebugFailTextureStep(int step);
IMPLATFORM_API void ImPlatform_GfxDebugLiveCounts(size_t* resources, size_t* descriptors);
// Name of the graphics adapter the device runs on (UTF-8; "" when unknown). DirectX 12 prefers the high-performance
// adapter on hybrid systems (environment IMPLATFORM_GPU=default keeps the system default).
IMPLATFORM_API const char* ImPlatform_GfxAdapterName(void);
// Testing aid (DirectX 12; elsewhere no timings): GpuTimer(true) starts timestamping every compute dispatch (a GPU timestamp
// pair around it, up to 32768 dispatches); GpuTimings resolves them (waits for the GPU: diagnostics only, never per
// frame), fills up to `max` entries in dispatch order and starts over. Labels are "<cache_key>/<entry>" and stay valid
// until the next call of either function.
typedef struct ImPlatform_GpuTiming { const char* label; double ms; } ImPlatform_GpuTiming;
IMPLATFORM_API void ImPlatform_GfxDebugGpuTimer(bool enable);
IMPLATFORM_API size_t ImPlatform_GfxDebugGpuTimings(ImPlatform_GpuTiming* out, size_t max);
#endif // IMPLATFORM_GFX_SUPPORT_DEVICE_CONTROL

// ============================================================================
// Depth-tested immediate drawing (IMPLATFORM_GFX_SUPPORT_DEPTH_DRAW: DirectX 11 and DirectX 12)
// ============================================================================
// Meshes drawn into a render texture with a depth buffer, outside the ImGui render callbacks (e.g. a 3D viewport
// rendered before ImGui::Render and shown with ImGui::Image).

IMPLATFORM_API bool ImPlatform_SupportsDepthDraw(void);

#if IMPLATFORM_GFX_SUPPORT_DEPTH_DRAW
// Like ImPlatform_CreateRenderTexture, plus a 32-bit depth buffer that ImPlatform_BeginRenderToTexture binds and
// clears to 1 (far). Destroy with ImPlatform_DestroyTexture.
IMPLATFORM_API ImTextureID ImPlatform_CreateRenderTextureWithDepth(const ImPlatform_TextureDesc* desc);
#endif

typedef struct ImPlatform_DrawMeshDesc {
    ImPlatform_ShaderProgram program;       // Vertex + fragment program; HLSL constants at b0 in each stage.
    ImPlatform_VertexBuffer  vertices;      // With an input layout (ImPlatform_CreateVertexInputLayout).
    ImPlatform_IndexBuffer   indices;
    unsigned int index_count;
    const void*  vs_constants;  unsigned int vs_constants_size;
    const void*  ps_constants;  unsigned int ps_constants_size;
    const ImTextureID* textures; unsigned int texture_count;   // Pixel stage t0..; s0 = linear clamp sampler.
    bool depth_test;        // Less-equal against the bound depth buffer.
    bool depth_write;
    bool alpha_blend;       // Straight alpha "over"; otherwise opaque.
    bool cull_back;         // Counterclockwise front faces are kept.
} ImPlatform_DrawMeshDesc;

// One indexed triangle-list draw into the current render target (call between ImPlatform_BeginRenderToTexture and
// ImPlatform_EndRenderToTexture). Returns false on invalid input.
#if IMPLATFORM_GFX_SUPPORT_DEPTH_DRAW
IMPLATFORM_API bool ImPlatform_DrawMesh(const ImPlatform_DrawMeshDesc* desc);
#endif

// ============================================================================
// Core API Functions
// ============================================================================

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
// Returns false when the frame should be skipped (quit requested, window minimized)
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

#ifdef IMGUI_HAS_VIEWPORT
// Update platform windows (for multi-viewport support)
// Call before rendering viewports
IMPLATFORM_API void ImPlatform_GfxViewportPre(void);

// Render platform windows (for multi-viewport support)
// Call after main rendering
IMPLATFORM_API void ImPlatform_GfxViewportPost(void);
#endif

// Swap buffers / present
// Returns true on success
IMPLATFORM_API bool ImPlatform_GfxAPISwapBuffer(void);

// ============================================================================
// Custom Title Bar API
// ============================================================================

#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR

// Check if custom titlebar is enabled
IMPLATFORM_API bool ImPlatform_CustomTitleBarEnabled(void);

// Enable custom titlebar (must be called before ImPlatform_InitPlatform)
IMPLATFORM_API void ImPlatform_EnableCustomTitleBar(void);

// Disable custom titlebar
IMPLATFORM_API void ImPlatform_DisableCustomTitleBar(void);

// Draw a default custom menu bar with minimize/maximize/close buttons
IMPLATFORM_API void ImPlatform_DrawCustomMenuBarDefault(void);

// Window control functions
IMPLATFORM_API void ImPlatform_MinimizeApp(void);
IMPLATFORM_API void ImPlatform_MaximizeApp(void);
IMPLATFORM_API bool ImPlatform_IsMaximized(void);
IMPLATFORM_API void ImPlatform_CloseApp(void);

// Custom titlebar rendering
// Returns true if the titlebar window began successfully
IMPLATFORM_API bool ImPlatform_BeginCustomTitleBar(float height);
IMPLATFORM_API void ImPlatform_EndCustomTitleBar(void);

// Borderless window configuration
// Controls resize border size, minimum window dimensions, and drag/resize behavior.
// Call ImPlatform_SetBorderlessParams() before or after ImPlatform_CreateWindow().
// Changes take effect immediately (minimum size is applied to the current window).
typedef struct   {
    int   resizeBorderSize;     // Resize edge thickness in pixels (default: 5). Used by SDL2/SDL3 hit-test; Win32 uses system metrics.
    int   minWidth;             // Minimum window width in pixels (default: 100, 0 = no limit)
    int   minHeight;            // Minimum window height in pixels (default: 100, 0 = no limit)
    bool  allowResize;          // Allow window resizing from edges (default: true)
    bool  allowMove;            // Allow window dragging from titlebar (default: true)
} ImPlatform_BorderlessParams;

// Get default borderless params (resizeBorderSize=5, min 100x100, resize+move enabled)
IMPLATFORM_API ImPlatform_BorderlessParams ImPlatform_BorderlessParams_Default(void);

// Set borderless params. If called after window creation, minimum size is applied immediately.
IMPLATFORM_API void ImPlatform_SetBorderlessParams(const ImPlatform_BorderlessParams* params);

// Get current borderless params
IMPLATFORM_API ImPlatform_BorderlessParams ImPlatform_GetBorderlessParams(void);

#endif // IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR

// ============================================================================
// Window State API
// ============================================================================

#if IMPLATFORM_APP_SUPPORT_WINDOW_STATE

// Fullscreen without a display mode change: the window covers the monitor it is on, undecorated. false gives it back
// its place, size and decoration.
IMPLATFORM_API void ImPlatform_SetFullscreen(bool fullscreen);
IMPLATFORM_API bool ImPlatform_IsFullscreen(void);

// Keeps the window above the other windows.
IMPLATFORM_API void ImPlatform_SetAlwaysOnTop(bool on_top);

// The window's icon (title bar, task bar, window switcher) from RGBA8 pixels, rows top-down; the pixels are copied.
// Returns false when the icon could not be made. macOS and Wayland show the application's icon whatever is set here.
IMPLATFORM_API bool ImPlatform_SetWindowIcon(const unsigned char* rgba, int width, int height);

#endif // IMPLATFORM_APP_SUPPORT_WINDOW_STATE

// ============================================================================
// Drag-and-Drop API (drags that come from other applications)
// ============================================================================

#if IMPLATFORM_APP_SUPPORT_DROP_FILE

// Callback fired once per dropped file.
// path:      UTF-8 encoded file path.
// pos:       Drop position in client/window coordinates (same space as ImGui::GetMousePos()).
// user_data: Opaque pointer passed to ImPlatform_SetDropFileCallback.
typedef void (*ImPlatform_DropFileCallback)(const char* path, ImVec2 pos, void* user_data);

// Register a callback for OS file-drop events.
// Enables OS drop acceptance on the window when a non-NULL callback is set.
// Pass NULL to unregister and disable drop acceptance.
IMPLATFORM_API void ImPlatform_SetDropFileCallback(ImPlatform_DropFileCallback callback, void* user_data);

// Everything a drag from another application does over the application's windows. A drop is a Begin event, one
// File or Text event per item, then an End event. Windows (every window library: an OLE drop target), Linux on X11
// (every window library: the XDND protocol) and SDL3 report the drag before the button is released: Begin comes when
// it enters a window, Position events follow it, and an End event without items means it left or was cancelled.
// Elsewhere (macOS and Wayland with GLFW or SDL2) a drop is reported at the release only (GLFW: files only). On
// Windows and with SDL3, Dear ImGui's platform windows (multi-viewports) take drops too.
// ImPlatform passes on what the drag carries and nothing more: a link or an image dragged from a browser comes as
// Text holding its URL (downloading it is the application's work), a file from a file manager as File.
typedef enum ImPlatform_DropEventType {
    ImPlatform_DropEvent_Begin,     // A drag entered the window, or a drop starts.
    ImPlatform_DropEvent_Position,  // The drag moved over the window.
    ImPlatform_DropEvent_File,      // text: the UTF-8 path of a dropped file.
    ImPlatform_DropEvent_Text,      // text: dropped UTF-8 text.
    ImPlatform_DropEvent_End        // The drop is complete, or the drag left the window.
} ImPlatform_DropEventType;

typedef struct ImPlatform_DropEvent {
    ImPlatform_DropEventType type;
    const char*              text;  // File, Text: valid during the callback. NULL for the other events.
    ImVec2                   pos;   // Position, File, Text: the space of ImGui::GetMousePos(): client-area coordinates of
                                    // the main window, or desktop coordinates when multi-viewports are enabled.
} ImPlatform_DropEvent;

typedef void (*ImPlatform_DropCallback)(const ImPlatform_DropEvent* event, void* user_data);

// Registers the callback of every drop event (NULL: none). It may be set beside ImPlatform_SetDropFileCallback(),
// which still receives each file. The window accepts drops while either callback is set.
IMPLATFORM_API void ImPlatform_SetDropCallback(ImPlatform_DropCallback callback, void* user_data);

#endif // IMPLATFORM_APP_SUPPORT_DROP_FILE

// Dragging out of the application: files (UTF-8 paths) and / or text that another application takes where the user
// drops them. Call it while a mouse button is held, when a drag that started in the application should leave it (the
// mouse left the window during an ImGui drag, for instance). It returns when the drag ends: true when it was dropped
// on something that took it. The application's frames stop meanwhile. Escape cancels the drag.
// Windows (every window library: OLE, the system draws the drag) and Linux on X11 with GLFW or SDL2 (the XDND
// protocol; nothing is drawn under the pointer; a target takes the files when there are files, the text otherwise).
// ImPlatform_SupportsDragOut() is false elsewhere and the call returns false.
IMPLATFORM_API bool ImPlatform_SupportsDragOut(void);
IMPLATFORM_API bool ImPlatform_DragOut(const char* const* file_paths, int file_count, const char* text);

// ============================================================================
// Pen / Stylus Input API
// ============================================================================
// Pressure and tilt from pen digitizers (Dear ImGui carries no pressure axis). Pen moves still reach ImGui as mouse
// events; this adds the pen-only axes and every hardware sample between frames (coalesced history).
// Implemented on Windows with every window library (Windows Ink pointer messages of the main window: WM_POINTER*,
// GetPointerPenInfoHistory: every hardware sample) and on SDL3 (its pen events, on Windows, Linux and macOS: one
// sample per event, each carrying the last value of the axes SDL reports separately; the main window only). Other
// platforms report ImPlatform_SupportsPen() == false.

typedef struct ImPlatform_PenSample {
    float  x, y;            // Client-area coordinates (same space as ImGui::GetMousePos()).
    float  pressure;        // [0, 1]; -1 when the device reports none.
    float  tilt_x, tilt_y;  // Degrees in [-90, 90] (+x right, +y toward the user); valid when has_tilt.
    double time;            // Seconds on ImPlatform_GetPenClock()'s clock.
    bool   has_tilt;
    bool   in_contact;      // Tip touching the surface (false while hovering).
    bool   eraser;          // Inverted pen / eraser end.
    bool   has_rotation;
    float  rotation;        // Barrel rotation, degrees clockwise in [0, 360); valid when has_rotation.
} ImPlatform_PenSample;

IMPLATFORM_API bool ImPlatform_SupportsPen(void);

// Latest pen state. Returns false when no pen is in range (out is left unchanged).
IMPLATFORM_API bool ImPlatform_GetPenState(ImPlatform_PenSample* out);

// Copies the pen samples received since the previous call, oldest first, and removes them. Returns the count written.
// Samples beyond max_count stay queued; the queue keeps the most recent 1024 when nobody consumes it.
IMPLATFORM_API int ImPlatform_ConsumePenSamples(ImPlatform_PenSample* out, int max_count);

// Current time on the clock of ImPlatform_PenSample::time (e.g. to compute a sample's age).
IMPLATFORM_API double ImPlatform_GetPenClock(void);

// ============================================================================
// Touch Input API
// ============================================================================
// The fingers on a touch screen and what they do together: pan, pinch, turn. Touches still reach ImGui as mouse events
// (the first finger moves and clicks); this adds the other fingers and the gesture, computed the same way on every
// platform from the finger positions. Implemented on Windows with every window library (pointer messages:
// WM_POINTER*; SDL2 and SDL3: their finger events) and on SDL2 and SDL3 elsewhere, for the main window. Other
// platforms report ImPlatform_SupportsTouch() == false.
// A pinch on a trackpad comes as the gesture's zoom with finger_count 0 (Windows with Win32, GLFW or SDL2: a
// precision touchpad, a wheel notch being a zoom of 2^(1/4); macOS: with every window library, and the two-finger turn
// as its rotation; SDL 3.4 and later: its pinch events). The trackpad's two-finger pan is the mouse wheel ImGui
// receives.

typedef struct ImPlatform_TouchPoint {
    unsigned long long id;  // The same from the finger's contact to its lift.
    float x, y;             // Client-area coordinates (same space as ImGui::GetMousePos()).
    float pressure;         // [0, 1]; -1 when the device reports none.
} ImPlatform_TouchPoint;

typedef struct ImPlatform_TouchGesture {
    int   finger_count;     // Fingers in contact now.
    float x, y;             // Their centroid, client-area coordinates; valid when finger_count > 0.
    float pan_x, pan_y;     // Movement of the centroid, pixels.
    float zoom;             // Factor by which the fingers moved apart (above 1) or together (below 1); 1 with one finger.
    float rotation;         // Degrees the first two fingers turned, clockwise; 0 with one finger.
} ImPlatform_TouchGesture;

IMPLATFORM_API bool ImPlatform_SupportsTouch(void);

// Copies the fingers in contact, in the order they touched. Returns the count written (10 fingers are tracked).
IMPLATFORM_API int ImPlatform_GetTouchPoints(ImPlatform_TouchPoint* out, int max_count);

// The gesture since the previous call: pan, zoom and rotation accumulate between two calls (call it once a frame).
// A finger that touches or lifts moves the centroid without panning. Returns false when no finger is in contact and
// none moved since the previous call. One finger pans too, as the mouse drag ImGui sees: test finger_count to tell.
IMPLATFORM_API bool ImPlatform_ConsumeTouchGesture(ImPlatform_TouchGesture* out);

// ============================================================================
// System Theme and Clipboard Image API
// ============================================================================

typedef enum ImPlatform_SystemTheme {
    ImPlatform_SystemTheme_Unknown,
    ImPlatform_SystemTheme_Light,
    ImPlatform_SystemTheme_Dark
} ImPlatform_SystemTheme;

// The light or dark mode the user chose for applications, asked of the system itself: Windows, macOS, and Linux
// desktops with the settings portal (libdbus loaded at run time; SDL3 answers in its place). Unknown where the system
// does not tell. Call it once a frame and compare to follow a change: macOS and Linux are asked once a second at most.
IMPLATFORM_API ImPlatform_SystemTheme ImPlatform_GetSystemTheme(void);

// An image on the system clipboard as RGBA8 pixels, rows top-down (text goes through ImGui::SetClipboardText()).
// Windows and macOS with every window library; Linux through libpng and the X11 clipboard, both loaded at run time
// (SDL3: SDL's clipboard, once the window exists). ImPlatform_SupportsClipboardImage() tells whether this system has
// what it takes; the calls return false otherwise. On Linux the image stays on the clipboard while the application
// runs, as any X11 selection does.
IMPLATFORM_API bool ImPlatform_SupportsClipboardImage(void);

// Replaces the clipboard's content by the image; the pixels are copied.
IMPLATFORM_API bool ImPlatform_SetClipboardImage(const unsigned char* rgba, int width, int height);

// The size of the image on the clipboard. Returns false when the clipboard holds none.
IMPLATFORM_API bool ImPlatform_GetClipboardImageSize(int* width, int* height);

// Copies the image on the clipboard into rgba (width * height * 4 bytes), for the size the call of
// ImPlatform_GetClipboardImageSize() before it gave. Alpha is 255: the clipboard's bitmaps carry none that can be
// relied on.
IMPLATFORM_API bool ImPlatform_GetClipboardImage(unsigned char* rgba, int width, int height);

// ============================================================================
// DPI / High-DPI Support
// ============================================================================

// Get the current DPI scale factor for the application window.
// Returns 1.0f at 96 DPI (100%), 1.25f at 120 DPI (125%), 1.5f at 144 DPI (150%), etc.
// Works on all platforms: Win32, GLFW, SDL2, SDL3, Apple.
IMPLATFORM_API float ImPlatform_GetDpiScale(void);

// Get a pixel size for a given number of Em lines.
// 1 Em = the current font size (ImGui::GetFontSize()).
// Equivalent to: lines * ImGui::GetFontSize()
IMPLATFORM_API float ImPlatform_EmSize(float lines);

// Convert a pixel value back to Em units.
// Inverse of ImPlatform_EmSize.
IMPLATFORM_API float ImPlatform_PixelsToEm(float pixels);

// ---------------------------------------------------------------
// "lp" (logical pixel / DIP) <-> physical pixel helpers.
//
// The scale factor is sourced from ImGui::GetStyle().FontScaleDpi so these
// stay in lockstep with the font/typography scaling already used by Slug
// and the per-style variables downstream. Authored widget sizes expressed
// in lp stay physically consistent across DPI settings.
// ---------------------------------------------------------------
IMPLATFORM_API float ImPlatform_LpPxScale(void);   // current FontScaleDpi, clamped to >=1e-3
IMPLATFORM_API float ImPlatform_LpToPx(float lp);
IMPLATFORM_API float ImPlatform_PxToLp(float px);

// DPI change callback.
// Register a callback that fires when the DPI scale changes at runtime
// (e.g., window moved to a different monitor).
// Set callback to NULL to unregister.
typedef void (*ImPlatform_DpiChangeCallback)(float new_scale, void* user_data);
IMPLATFORM_API void ImPlatform_SetDpiChangeCallback(ImPlatform_DpiChangeCallback callback, void* user_data);

// ============================================================================
// Shutdown functions (call in this order)
// ============================================================================

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

// C++ only: Em-based utility that returns ImVec2 (not C-compatible)
// Convert Em units to pixel coordinates.
// Example: ImPlatform_EmToVec2(10.0f, 5.0f) returns a 10-Em x 5-Em rectangle in pixels.
IMPLATFORM_API ImVec2 ImPlatform_EmToVec2(float em_x, float em_y);

// C++ only: ImVec2 lp <-> px overloads (pair with ImPlatform_LpToPx / ImPlatform_PxToLp).
IMPLATFORM_API ImVec2 ImPlatform_LpToPx(ImVec2 lp);
IMPLATFORM_API ImVec2 ImPlatform_PxToLp(ImVec2 px);

#endif

#ifdef IMPLATFORM_IMPLEMENTATION

// ============================================================================
// Version Information Implementation
// ============================================================================

IMPLATFORM_API const char* ImPlatform_GetVersion(void)
{
    return IMPLATFORM_VERSION;
}

IMPLATFORM_API int ImPlatform_GetVersionNum(void)
{
    return IMPLATFORM_VERSION_NUM;
}

// Borderless params storage (read by platform backends in hit-test callbacks).
// This sits after the extern "C" block above, so it needs its own linkage
// specifier to match the extern declaration in ImPlatform_Internal.h. MSVC
// tolerates the mismatch; GCC and Clang reject it.
#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
#ifdef __cplusplus
extern "C" {
#endif
ImPlatform_BorderlessParams g_BorderlessParams = { 5, 100, 100, true, true };
#ifdef __cplusplus
}
#endif
#endif

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
#elif IM_CURRENT_GFX == IM_GFX_CUSTOM
    // No backend here: the application links its own implementation of the
    // graphics entry points (see IM_GFX_CUSTOM at the top of this file).
#else
    #error "Unknown or unsupported IM_CURRENT_GFX backend"
#endif

// The run-time queries of the optional groups the backend does not implement (IMPLATFORM_GFX_SUPPORT_*); their other
// functions are not declared. IM_GFX_CUSTOM applications provide their own.
#if IM_CURRENT_GFX != IM_GFX_CUSTOM
#if !IMPLATFORM_GFX_SUPPORT_READBACK
IMPLATFORM_API bool ImPlatform_SupportsReadback(void) { return false; }
#endif
#if !IMPLATFORM_GFX_SUPPORT_COMPUTE
IMPLATFORM_API bool ImPlatform_SupportsCompute(void) { return false; }
#endif
#if !IMPLATFORM_GFX_SUPPORT_DEPTH_DRAW
IMPLATFORM_API bool ImPlatform_SupportsDepthDraw(void) { return false; }
#endif
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

// Include custom titlebar implementation (when supported)
#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
    #include "ImPlatform_titlebar.cpp"
#endif

// Shader parts generator (ImPlatform_GenerateShaderSource, ImPlatform_CreateShaderFromParts)
#include "ImPlatform_shader_parts.cpp"

// Shader compile log, blend presets, asynchronous program compilation
#include "ImPlatform_shader_common.cpp"

// The system theme, clipboard images and macOS gestures, taken from the system rather than the window library
#include "ImPlatform_native.cpp"

// ============================================================================
// Borderless Params Implementation
// ============================================================================

#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
IMPLATFORM_API ImPlatform_BorderlessParams ImPlatform_BorderlessParams_Default(void)
{
    ImPlatform_BorderlessParams params;
    params.resizeBorderSize = 5;
    params.minWidth = 100;
    params.minHeight = 100;
    params.allowResize = true;
    params.allowMove = true;
    return params;
}

IMPLATFORM_API ImPlatform_BorderlessParams ImPlatform_GetBorderlessParams(void)
{
    return g_BorderlessParams;
}

IMPLATFORM_API void ImPlatform_SetBorderlessParams(const ImPlatform_BorderlessParams* params)
{
    if (!params)
        return;
    g_BorderlessParams = *params;

    // Apply minimum size to the current window (if it exists and custom titlebar is active)
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    // Win32: Enforced via WM_GETMINMAXINFO which reads g_BorderlessParams each time
    (void)0;
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    {
        ImPlatform_AppData_GLFW* pData = ImPlatform_App_GetData_GLFW();
        if (pData->pWindow && pData->bCustomTitleBar)
        {
            int minW = g_BorderlessParams.minWidth > 0 ? g_BorderlessParams.minWidth : GLFW_DONT_CARE;
            int minH = g_BorderlessParams.minHeight > 0 ? g_BorderlessParams.minHeight : GLFW_DONT_CARE;
            glfwSetWindowSizeLimits(pData->pWindow, minW, minH, GLFW_DONT_CARE, GLFW_DONT_CARE);
        }
    }
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
    {
        ImPlatform_AppData_SDL2* pData = ImPlatform_App_GetData_SDL2();
        if (pData->pWindow && pData->bCustomTitleBar)
            SDL_SetWindowMinimumSize(pData->pWindow, g_BorderlessParams.minWidth, g_BorderlessParams.minHeight);
    }
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
    {
        ImPlatform_AppData_SDL3* pData = ImPlatform_App_GetData_SDL3();
        if (pData->pWindow && pData->bCustomTitleBar)
            SDL_SetWindowMinimumSize(pData->pWindow, g_BorderlessParams.minWidth, g_BorderlessParams.minHeight);
    }
#endif
}
#endif // IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR

// ============================================================================
// Sampler override on the current window's draw list
// ============================================================================

IMPLATFORM_API void ImPlatform_PushSampler(ImPlatform_TextureFilter filter, ImPlatform_TextureWrap wrap)
{
    ImPlatform_PushSamplerEx(NULL, filter, wrap);
}

IMPLATFORM_API void ImPlatform_PopSampler(void)
{
    ImPlatform_PopSamplerEx(NULL);
}

// ============================================================================
// DPI Support Implementation
// ============================================================================

IMPLATFORM_API float ImPlatform_GetDpiScale(void)
{
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    return ImPlatform_App_GetDpiScale_Win32();
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    return ImPlatform_App_GetDpiScale_GLFW();
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
    return ImPlatform_App_GetDpiScale_SDL2();
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
    return ImPlatform_App_GetDpiScale_SDL3();
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_APPLE)
    return ImPlatform_App_GetDpiScale_Apple();
#else
    return 1.0f;
#endif
}

// ============================================================================
// Em-based DPI Utilities Implementation
// ============================================================================

IMPLATFORM_API ImVec2 ImPlatform_EmToVec2(float em_x, float em_y)
{
    float fontSize = ImGui::GetFontSize();
    return ImVec2(em_x * fontSize, em_y * fontSize);
}

IMPLATFORM_API float ImPlatform_EmSize(float lines)
{
    return lines * ImGui::GetFontSize();
}

IMPLATFORM_API float ImPlatform_PixelsToEm(float pixels)
{
    float fontSize = ImGui::GetFontSize();
    return (fontSize > 0.0f) ? (pixels / fontSize) : 0.0f;
}

// ---------------------------------------------------------------
// Lp <-> Px helpers (keyed off ImGui::GetStyle().FontScaleDpi)
// ---------------------------------------------------------------

IMPLATFORM_API float ImPlatform_LpPxScale(void)
{
    float s = ImGui::GetStyle().FontScaleDpi;
    return (s > 1e-3f) ? s : 1.0f;
}
IMPLATFORM_API float ImPlatform_LpToPx(float lp) { return lp * ImPlatform_LpPxScale(); }
IMPLATFORM_API float ImPlatform_PxToLp(float px) { return px / ImPlatform_LpPxScale(); }

IMPLATFORM_API ImVec2 ImPlatform_LpToPx(ImVec2 lp)
{
    float s = ImPlatform_LpPxScale();
    return ImVec2(lp.x * s, lp.y * s);
}
IMPLATFORM_API ImVec2 ImPlatform_PxToLp(ImVec2 px)
{
    float s = ImPlatform_LpPxScale();
    return ImVec2(px.x / s, px.y / s);
}

// ============================================================================
// DPI Change Callback Implementation
// ============================================================================

static ImPlatform_DpiChangeCallback g_DpiChangeCallback = NULL;
static void* g_DpiChangeCallbackUserData = NULL;

IMPLATFORM_API void ImPlatform_SetDpiChangeCallback(ImPlatform_DpiChangeCallback callback, void* user_data)
{
    g_DpiChangeCallback = callback;
    g_DpiChangeCallbackUserData = user_data;
}

// Internal helper: call the user's DPI change callback (if set)
void ImPlatform_NotifyDpiChange(float new_scale)
{
    if (g_DpiChangeCallback)
        g_DpiChangeCallback(new_scale, g_DpiChangeCallbackUserData);
}

// ============================================================================
// File Drop Callback Implementation
// ============================================================================

#if IMPLATFORM_APP_SUPPORT_DROP_FILE
static ImPlatform_DropFileCallback g_DropFileCallback = NULL;
static void* g_DropFileCallbackUserData = NULL;
static ImPlatform_DropCallback g_DropCallback = NULL;
static void* g_DropCallbackUserData = NULL;

IMPLATFORM_API void ImPlatform_SetDropFileCallback(ImPlatform_DropFileCallback callback, void* user_data)
{
    g_DropFileCallback = callback;
    g_DropFileCallbackUserData = user_data;
    ImPlatform_App_OnDropFileCallbackChanged(g_DropFileCallback != NULL || g_DropCallback != NULL);
}

IMPLATFORM_API void ImPlatform_SetDropCallback(ImPlatform_DropCallback callback, void* user_data)
{
    g_DropCallback = callback;
    g_DropCallbackUserData = user_data;
    ImPlatform_App_OnDropFileCallbackChanged(g_DropFileCallback != NULL || g_DropCallback != NULL);
}

// Internal helper: fires the user's drop callbacks (called from platform backends)
void ImPlatform_NotifyDrop(ImPlatform_DropEventType type, const char* utf8, ImVec2 client_pos)
{
    // Dear ImGui's mouse space: desktop coordinates once multi-viewports are enabled
#ifdef IMGUI_HAS_VIEWPORT
    if (ImGui::GetCurrentContext() && (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable))
    {
        client_pos.x += ImGui::GetMainViewport()->Pos.x;
        client_pos.y += ImGui::GetMainViewport()->Pos.y;
    }
#endif
    if (g_DropCallback)
    {
        const ImPlatform_DropEvent event = { type, utf8, client_pos };
        g_DropCallback(&event, g_DropCallbackUserData);
    }
    if (type == ImPlatform_DropEvent_File && g_DropFileCallback)
        g_DropFileCallback(utf8, client_pos, g_DropFileCallbackUserData);
}
#endif // IMPLATFORM_APP_SUPPORT_DROP_FILE

// ============================================================================
// Pen Input Implementation (platform backends feed it through ImPlatform_Pen_* in ImPlatform_Internal.h)
// ============================================================================

#define IMPLATFORM_PEN_QUEUE 1024
static ImPlatform_PenSample g_PenQueue[IMPLATFORM_PEN_QUEUE];
static int                  g_PenQueueFirst = 0, g_PenQueueCount = 0;
static ImPlatform_PenSample g_PenState;
static bool                 g_PenInRange = false;

void ImPlatform_Pen_Push(const ImPlatform_PenSample* sample)
{
    g_PenState = *sample;
    g_PenInRange = true;
    if (g_PenQueueCount == IMPLATFORM_PEN_QUEUE)
    {
        g_PenQueueFirst = (g_PenQueueFirst + 1) % IMPLATFORM_PEN_QUEUE;     // Drop the oldest.
        g_PenQueueCount--;
    }
    g_PenQueue[(g_PenQueueFirst + g_PenQueueCount++) % IMPLATFORM_PEN_QUEUE] = *sample;
}

void ImPlatform_Pen_Leave(void)
{
    g_PenInRange = false;
}

IMPLATFORM_API bool ImPlatform_SupportsPen(void)
{
    // Windows: the pointer messages of the window, whichever library made it (ImPlatform_native.cpp)
#if IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32 || IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3 \
    || (defined(_WIN32) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW || IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2))
    return true;
#else
    return false;
#endif
}

IMPLATFORM_API bool ImPlatform_GetPenState(ImPlatform_PenSample* out)
{
    if (!out || !g_PenInRange)
        return false;
    *out = g_PenState;
    return true;
}

IMPLATFORM_API int ImPlatform_ConsumePenSamples(ImPlatform_PenSample* out, int max_count)
{
    int n = 0;
    while (out && n < max_count && g_PenQueueCount > 0)
    {
        out[n++] = g_PenQueue[g_PenQueueFirst];
        g_PenQueueFirst = (g_PenQueueFirst + 1) % IMPLATFORM_PEN_QUEUE;
        g_PenQueueCount--;
    }
    return n;
}

IMPLATFORM_API double ImPlatform_GetPenClock(void)
{
#if IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3
    return (double)SDL_GetTicksNS() * 1e-9;
#elif defined(_WIN32)
    LARGE_INTEGER now, freq;
    ::QueryPerformanceCounter(&now);
    ::QueryPerformanceFrequency(&freq);
    return (double)now.QuadPart / (double)freq.QuadPart;
#else
    return ImGui::GetTime();
#endif
}

// ============================================================================
// Touch Input Implementation (platform backends feed it through ImPlatform_Touch_Set in ImPlatform_Internal.h)
// ============================================================================

#include <math.h>

#define IMPLATFORM_TOUCH_POINTS 10
static ImPlatform_TouchPoint g_TouchPoints[IMPLATFORM_TOUCH_POINTS];
static int                   g_TouchCount = 0;
static float                 g_TouchPanX = 0.0f, g_TouchPanY = 0.0f, g_TouchZoom = 1.0f, g_TouchRotation = 0.0f;

// m: the centroid of the fingers (x, y), their mean distance to it, the direction from the first to the second (radians).
static void ImPlatform_Touch_Measure(float m[4])
{
    m[0] = m[1] = m[2] = m[3] = 0.0f;
    for (int i = 0; i < g_TouchCount; ++i)
    {
        m[0] += g_TouchPoints[i].x / (float)g_TouchCount;
        m[1] += g_TouchPoints[i].y / (float)g_TouchCount;
    }
    for (int i = 0; i < g_TouchCount; ++i)
    {
        const float dx = g_TouchPoints[i].x - m[0], dy = g_TouchPoints[i].y - m[1];
        m[2] += sqrtf(dx * dx + dy * dy) / (float)g_TouchCount;
    }
    if (g_TouchCount >= 2)
        m[3] = atan2f(g_TouchPoints[1].y - g_TouchPoints[0].y, g_TouchPoints[1].x - g_TouchPoints[0].x);
}

// A finger in contact at a position (down), or lifted. Only a finger that moves feeds the gesture: one that touches
// or lifts changes the centroid and the spread without the hand moving.
void ImPlatform_Touch_Set(unsigned long long id, float x, float y, float pressure, bool down)
{
    int i = 0;
    while (i < g_TouchCount && g_TouchPoints[i].id != id)
        ++i;
    if (i < g_TouchCount && down)
    {
        float before[4], after[4];
        ImPlatform_Touch_Measure(before);
        g_TouchPoints[i].x = x;
        g_TouchPoints[i].y = y;
        g_TouchPoints[i].pressure = pressure;
        ImPlatform_Touch_Measure(after);
        g_TouchPanX += after[0] - before[0];
        g_TouchPanY += after[1] - before[1];
        if (g_TouchCount >= 2 && before[2] > 0.0f && after[2] > 0.0f)
        {
            float turn = (after[3] - before[3]) * 57.29578f;
            turn -= 360.0f * floorf((turn + 180.0f) / 360.0f);     // The short way round: [-180, 180)
            g_TouchZoom *= after[2] / before[2];
            g_TouchRotation += turn;
        }
    }
    else if (i < g_TouchCount)
    {
        for (; i + 1 < g_TouchCount; ++i)       // The fingers after it keep their order.
            g_TouchPoints[i] = g_TouchPoints[i + 1];
        g_TouchCount--;
    }
    else if (down && g_TouchCount < IMPLATFORM_TOUCH_POINTS)
    {
        const ImPlatform_TouchPoint point = { id, x, y, pressure };
        g_TouchPoints[g_TouchCount++] = point;
    }
}

// A pinch or a turn the platform reports itself (trackpads). Fingers on a touch screen give them through their moves.
void ImPlatform_Touch_Zoom(float factor)
{
    if (g_TouchCount == 0 && factor > 0.0f)
        g_TouchZoom *= factor;
}

void ImPlatform_Touch_Rotate(float degrees)
{
    if (g_TouchCount == 0)
        g_TouchRotation += degrees;
}

IMPLATFORM_API bool ImPlatform_SupportsTouch(void)
{
#if IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32 || IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2 || IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3 \
    || (defined(_WIN32) && IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    return true;
#else
    return false;
#endif
}

IMPLATFORM_API int ImPlatform_GetTouchPoints(ImPlatform_TouchPoint* out, int max_count)
{
    int n = 0;
    while (out && n < max_count && n < g_TouchCount)
    {
        out[n] = g_TouchPoints[n];
        n++;
    }
    return n;
}

IMPLATFORM_API bool ImPlatform_ConsumeTouchGesture(ImPlatform_TouchGesture* out)
{
    if (!out)
        return false;
    float m[4];
    ImPlatform_Touch_Measure(m);
    out->finger_count = g_TouchCount;
    out->x = m[0];
    out->y = m[1];
    out->pan_x = g_TouchPanX;
    out->pan_y = g_TouchPanY;
    out->zoom = g_TouchZoom;
    out->rotation = g_TouchRotation;
    g_TouchPanX = g_TouchPanY = g_TouchRotation = 0.0f;
    g_TouchZoom = 1.0f;
    return out->finger_count > 0 || out->pan_x != 0.0f || out->pan_y != 0.0f || out->zoom != 1.0f || out->rotation != 0.0f;
}

// ============================================================================
// Common Helper Functions (C++ inline implementations)
// ============================================================================

#if IMPLATFORM_GFX_SUPPORT_RENDER_TARGETS && IMPLATFORM_GFX_SUPPORT_READBACK
struct ImPlatform_CaptureRequest
{
    ImTextureID texture;
    int         width, height;
};

IMPLATFORM_API ImTextureID ImPlatform_CaptureViewport(ImGuiViewport* viewport, unsigned int* out_width, unsigned int* out_height)
{
    if (!viewport)
        viewport = ImGui::GetMainViewport();
#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_VULKAN || IM_CURRENT_GFX == IM_GFX_DIRECTX12)
    // The targets of secondary viewports belong to Dear ImGui's backend: ImPlatform_CopyCurrentTarget does not reach them.
    if (viewport != ImGui::GetMainViewport())
    {
        if (out_width)  *out_width = 0;
        if (out_height) *out_height = 0;
        return 0;
    }
#endif
    ImPlatform_CaptureRequest request = {};
    request.width = (int)(viewport->Size.x * viewport->FramebufferScale.x);
    request.height = (int)(viewport->Size.y * viewport->FramebufferScale.y);
    if (out_width)  *out_width = (unsigned int)(request.width > 0 ? request.width : 0);
    if (out_height) *out_height = (unsigned int)(request.height > 0 ? request.height : 0);
    if (request.width <= 0 || request.height <= 0)
        return 0;
    ImPlatform_TextureDesc desc = ImPlatform_TextureDesc_Default((unsigned int)request.width, (unsigned int)request.height);
    desc.format = ImPlatform_GetRenderPassFormat();     // The format of the viewport's target: a plain copy
    request.texture = ImPlatform_CreateRenderTexture(&desc);
    if (!request.texture)
        return 0;
    // The request is copied into the draw list: the callback runs when the viewport is rendered.
    ImGui::GetForegroundDrawList(viewport)->AddCallback([](const ImDrawList*, const ImDrawCmd* cmd)
    {
        ImPlatform_CaptureRequest r;
        memcpy(&r, cmd->UserCallbackData, sizeof(r));
        ImPlatform_CopyCurrentTarget(r.texture, 0, 0, r.width, r.height, 0, 0);
    }, &request, sizeof(request));
    return request.texture;
}

IMPLATFORM_API bool ImPlatform_ReadCapture(ImTextureID capture, unsigned int width, unsigned int height, void* out_rgba, size_t out_size)
{
    const size_t count = (size_t)width * height;
    if (!capture || !out_rgba || !count || out_size < count * 4 || !ImPlatform_ReadTexture(capture, 0, 0, width, height, out_rgba, out_size))
        return false;
    const bool bgra = ImPlatform_GetRenderPassFormat() == ImPlatform_PixelFormat_BGRA8;
    unsigned char* p = (unsigned char*)out_rgba;
    for (size_t i = 0; i < count; i++, p += 4)
    {
        if (bgra)
        {
            const unsigned char blue = p[0];
            p[0] = p[2];
            p[2] = blue;
        }
        p[3] = 255;     // The target's alpha is not coverage
    }
    return true;
}
#endif

// Helper function to draw a content-region quad with a custom shader
// This must be called from within an ImGui window context (after ImGui::Begin)
IMPLATFORM_API void ImPlatform_DrawCustomShaderQuad(ImPlatform_ShaderProgram program)
{
    if (!program)
        return;

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 cur = ImGui::GetCursorScreenPos();
    ImVec2 region = ImGui::GetContentRegionAvail();

    if (region.x > 0 && region.y > 0)
    {
        // Get a dummy texture from ImGui's font atlas for the quad
        ImTextureRef tex = ImGui::GetIO().Fonts->TexRef;

        ImPlatform_BeginCustomShader(draw, program);

        // Calculate quad corners
        ImVec2 p_min = cur;
        ImVec2 p_max(cur.x + region.x, cur.y + region.y);
        ImVec2 tl(p_min.x, p_min.y);  // top-left
        ImVec2 tr(p_max.x, p_min.y);  // top-right
        ImVec2 br(p_max.x, p_max.y);  // bottom-right
        ImVec2 bl(p_min.x, p_max.y);  // bottom-left

        // AddImageQuad order: p1, p2, p3, p4 with UV: uv1, uv2, uv3, uv4
        // Standard UV mapping: (0,0)=top-left, (1,0)=top-right, (1,1)=bottom-right, (0,1)=bottom-left
        draw->AddImageQuad(tex, tl, tr, br, bl,
            ImVec2(0, 0), ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1), IM_COL32_WHITE);

        ImPlatform_EndCustomShader(draw);

        // Advance cursor so window doesn't collapse
        ImGui::Dummy(region);
    }
}

#endif // IMPLATFORM_IMPLEMENTATION
