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

// Graphics API feature support flags
// These indicate which features are supported by each backend
#if defined(IM_CURRENT_GFX)
    #if (IM_CURRENT_GFX == IM_GFX_OPENGL3) || \
        (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || \
        (IM_CURRENT_GFX == IM_GFX_DIRECTX11) || \
        (IM_CURRENT_GFX == IM_GFX_DIRECTX12) || \
        (IM_CURRENT_GFX == IM_GFX_VULKAN) || \
        (IM_CURRENT_GFX == IM_GFX_METAL) || \
        (IM_CURRENT_GFX == IM_GFX_WGPU)
        #define IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER 1
    #else
        #define IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER 0
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
// Texture Creation API
// ============================================================================

// Pixel format enums
typedef enum ImPlatform_PixelFormat {
    ImPlatform_PixelFormat_R8,           // Single channel, 8-bit unsigned
    ImPlatform_PixelFormat_RG8,          // Two channel, 8-bit unsigned per channel
    ImPlatform_PixelFormat_RGB8,         // Three channel, 8-bit unsigned per channel
    ImPlatform_PixelFormat_RGBA8,        // Four channel, 8-bit unsigned per channel (most common)
    ImPlatform_PixelFormat_R16,          // Single channel, 16-bit unsigned
    ImPlatform_PixelFormat_RG16,         // Two channel, 16-bit unsigned per channel
    ImPlatform_PixelFormat_RGBA16,       // Four channel, 16-bit unsigned per channel
    ImPlatform_PixelFormat_R32F,         // Single channel, 32-bit float
    ImPlatform_PixelFormat_RG32F,        // Two channel, 32-bit float per channel
    ImPlatform_PixelFormat_RGBA32F,      // Four channel, 32-bit float per channel
} ImPlatform_PixelFormat;

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

// Destroy a texture and free its resources
// texture_id: Texture to destroy
IMPLATFORM_API void ImPlatform_DestroyTexture(
    ImTextureID texture_id
);

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

// Shader descriptor - describes shader source
typedef struct ImPlatform_ShaderDesc {
    ImPlatform_ShaderStage stage;        // Shader stage (vertex, fragment, etc.)
    ImPlatform_ShaderFormat format;      // Source code format
    const char* source_code;              // Shader source code string
    const void* bytecode;                 // Pre-compiled bytecode (optional, for SPIRV/DXIL)
    unsigned int bytecode_size;           // Size of bytecode in bytes
    const char* entry_point;              // Entry point function name (e.g., "main", "VSMain")
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
        ImTextureRef tex = ImGui::GetIO().Fonts->TexID;

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
