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

// ImPlatform version
// (Integer encoded as XYYZZ for use in #if preprocessor conditionals, e.g. '#if IMPLATFORM_VERSION_NUM >= 500')
#define IMPLATFORM_VERSION          "0.5.0"
#define IMPLATFORM_VERSION_NUM      500
#define IMPLATFORM_VERSION_MAJOR    0
#define IMPLATFORM_VERSION_MINOR    5
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

#define IM_GFX_MASK         0x0000FFFFu

// Graphics API feature support flags
// These indicate which features are supported by each backend
#if defined(IM_CURRENT_GFX)
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

// ============================================================================
// Version Information
// ============================================================================

// Get ImPlatform version string (e.g., "0.5.0")
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

// Texture descriptor - defines texture properties
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
// the backend does not support 3D textures.
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

// Destroy a texture and free its resources
// texture_id: Texture to destroy
IMPLATFORM_API void ImPlatform_DestroyTexture(
    ImTextureID texture_id
);

// Override the sampler used by ImGui::Image() (and similar) for one or more draw calls.
// Call ImPlatform_PushSampler() before the image call and ImPlatform_PopSampler() after.
// Calls may be nested. Safe no-op on backends that don't support dynamic sampler override
// (DX12, Vulkan, WGPU — where the sampler is baked into the pipeline/descriptor set).
//
// Example:
//   ImPlatform_PushSampler(ImPlatform_TextureFilter_Nearest, ImPlatform_TextureWrap_Clamp);
//   ImGui::Image(pixelArtTex, size);
//   ImPlatform_PopSampler();
IMPLATFORM_API void ImPlatform_PushSampler(ImPlatform_TextureFilter filter, ImPlatform_TextureWrap wrap);
IMPLATFORM_API void ImPlatform_PopSampler(void);

// ============================================================================
// Custom Vertex/Index Buffer Management API
// ============================================================================

// Buffer usage hints - tells the graphics API how the buffer will be used
typedef enum ImPlatform_BufferUsage {
    ImPlatform_BufferUsage_Static,       // Data will not change (upload once, use many times)
    ImPlatform_BufferUsage_Dynamic,      // Data changes occasionally (re-upload per frame)
    ImPlatform_BufferUsage_Stream,       // Data changes every frame (streaming/transient data)
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
IMPLATFORM_API ImPlatform_VertexBuffer ImPlatform_CreateVertexBuffer(
    const void* vertex_data,
    const ImPlatform_VertexBufferDesc* desc
);

// Update vertex buffer data (for Dynamic/Stream usage)
// buffer: Vertex buffer to update
// vertex_data: New vertex data
// offset: Offset in vertices (NOT bytes)
// count: Number of vertices to update
// Returns: true on success, false on failure
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

// Update index buffer data (for Dynamic/Stream usage)
// buffer: Index buffer to update
// index_data: New index data
// offset: Offset in indices (NOT bytes)
// count: Number of indices to update
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
} ImPlatform_ShaderDesc;

// Opaque handle for shader
typedef void* ImPlatform_Shader;

// Opaque handle for shader program (linked vertex + fragment shaders)
typedef void* ImPlatform_ShaderProgram;

// Create a shader from source code or bytecode
// desc: Shader descriptor
// Returns: Shader handle or NULL on failure
// Note: Compile errors will be logged to console/debug output
IMPLATFORM_API ImPlatform_Shader ImPlatform_CreateShader(
    const ImPlatform_ShaderDesc* desc
);

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

// Create a vertex input layout for a vertex buffer using the VS bytecode from a shader program.
// Must be called after both ImPlatform_CreateVertexBuffer and ImPlatform_CreateShaderProgram.
// The layout is derived from the vertex buffer's ImPlatform_VertexAttribute array.
// Duplicate semantic names are disambiguated by auto-incrementing SemanticIndex.
// After this call, ImPlatform_BindBuffers will activate the custom layout automatically.
// Returns: true on success, false on failure
IMPLATFORM_API bool ImPlatform_CreateVertexInputLayout(
    ImPlatform_VertexBuffer vertex_buffer,
    ImPlatform_ShaderProgram program
);

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

// Set shader texture binding
// program: Shader program
// name: Texture uniform name (e.g., "u_Texture")
// slot: Texture slot/unit (0-15 typically)
// texture: Texture ID to bind
// Returns: true on success, false if uniform not found
IMPLATFORM_API bool ImPlatform_SetShaderTexture(
    ImPlatform_ShaderProgram program,
    const char* name,
    unsigned int slot,
    ImTextureID texture
);

// Begin using a custom shader for ImDrawList rendering
// Adds a callback to the drawlist to switch to the custom shader
// draw: ImGui draw list
// shader: Shader program to use for subsequent draw calls
IMPLATFORM_API void ImPlatform_BeginCustomShader(
    ImDrawList* draw,
    ImPlatform_ShaderProgram shader
);
// Activate a custom shader immediately (for use inside draw callbacks)
IMPLATFORM_API void ImPlatform_BeginCustomShader_Render(ImPlatform_ShaderProgram program);

// Upload per-draw-call shader constants and bind them for the next draw.
// data: Pointer to constant data
// size: Size of constant data in bytes
// Returns an opaque handle that must be freed with ImPlatform_PopShaderConstants.
IMPLATFORM_API void* ImPlatform_PushShaderConstants(const void* data, unsigned int size);
IMPLATFORM_API void  ImPlatform_PopShaderConstants(void* handle);

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
// File Drag-and-Drop API
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

#endif // IMPLATFORM_APP_SUPPORT_DROP_FILE

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

// Borderless params storage (read by platform backends in hit-test callbacks)
#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
ImPlatform_BorderlessParams g_BorderlessParams = { 5, 100, 100, true, true };
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

// Include custom titlebar implementation (when supported)
#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
    #include "ImPlatform_titlebar.cpp"
#endif

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

IMPLATFORM_API void ImPlatform_SetDropFileCallback(ImPlatform_DropFileCallback callback, void* user_data)
{
    g_DropFileCallback = callback;
    g_DropFileCallbackUserData = user_data;
    ImPlatform_App_OnDropFileCallbackChanged(callback != NULL);
}

// Internal helper: fire the user's drop callback (called from platform backends)
void ImPlatform_NotifyFileDrop(const char* utf8_path, ImVec2 client_pos)
{
    if (g_DropFileCallback)
        g_DropFileCallback(utf8_path, client_pos, g_DropFileCallbackUserData);
}
#endif // IMPLATFORM_APP_SUPPORT_DROP_FILE

// ============================================================================
// Common Helper Functions (C++ inline implementations)
// ============================================================================

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
