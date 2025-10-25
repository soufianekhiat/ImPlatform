// dear imgui: Graphics API Abstraction - WebGPU Backend
// This handles WebGPU device creation, rendering, and presentation
//
// NOTE: WebGPU implementation requires Dawn/Emscripten WebGPU support

#include "ImPlatform_Internal.h"

#ifdef IM_GFX_WGPU

#include "../imgui.h"
#include "../imgui/backends/imgui_impl_wgpu.h"

#ifdef IM_PLATFORM_GLFW
    #include <GLFW/glfw3.h>
    #include <GLFW/glfw3native.h>
#endif

// Global state
static ImPlatform_GfxData_WebGPU g_GfxData = { 0 };

// Uniform block API state
static ImPlatform_ShaderProgram g_CurrentUniformBlockProgram = nullptr;
static void* g_UniformBlockData = nullptr;
static size_t g_UniformBlockSize = 0;

// Internal API - Get WebGPU gfx data
ImPlatform_GfxData_WebGPU* ImPlatform_Gfx_GetData_WebGPU(void)
{
    return &g_GfxData;
}

// Internal API - Create WebGPU device
bool ImPlatform_Gfx_CreateDevice_WebGPU(void* pWindow, ImPlatform_GfxData_WebGPU* pData)
{
    // NOTE: WebGPU device creation requires:
    // 1. Create WGPUInstance
    // 2. Request WGPUAdapter
    // 3. Request WGPUDevice
    // 4. Create surface from window
    // 5. Create swapchain
    // 6. Setup preferred format
    //
    // This is platform-dependent and requires async operations.
    // See examples/example_glfw_wgpu/main.cpp for complete implementation.

    (void)pWindow;
    (void)pData;

    // TODO: Implement WebGPU device creation
    // Example outline:
    // WGPUInstanceDescriptor instance_desc = {};
    // pData->instance = wgpuCreateInstance(&instance_desc);
    //
    // WGPURequestAdapterOptions adapter_opts = {};
    // // Request adapter (async)
    //
    // WGPUDeviceDescriptor device_desc = {};
    // // Request device (async)
    //
    // Create surface from window...
    // Create swapchain...

    return false; // Not implemented
}

// Internal API - Cleanup WebGPU device
void ImPlatform_Gfx_CleanupDevice_WebGPU(ImPlatform_GfxData_WebGPU* pData)
{
    if (pData->swapChain)
    {
        wgpuSwapChainRelease(pData->swapChain);
        pData->swapChain = nullptr;
    }

    if (pData->surface)
    {
        wgpuSurfaceRelease(pData->surface);
        pData->surface = nullptr;
    }

    if (pData->queue)
    {
        wgpuQueueRelease(pData->queue);
        pData->queue = nullptr;
    }

    if (pData->device)
    {
        wgpuDeviceRelease(pData->device);
        pData->device = nullptr;
    }

    if (pData->adapter)
    {
        wgpuAdapterRelease(pData->adapter);
        pData->adapter = nullptr;
    }

    if (pData->instance)
    {
        wgpuInstanceRelease(pData->instance);
        pData->instance = nullptr;
    }
}

// ImPlatform API - InitGfxAPI
IMPLATFORM_API bool ImPlatform_InitGfxAPI(void)
{
#ifdef IM_PLATFORM_GLFW
    GLFWwindow* pWindow = ImPlatform_App_GetGLFWWindow();
    return ImPlatform_Gfx_CreateDevice_WebGPU(pWindow, &g_GfxData);
#else
    // WebGPU is primarily used with GLFW or Emscripten
    return false;
#endif
}

// ImPlatform API - InitGfx
IMPLATFORM_API bool ImPlatform_InitGfx(void)
{
    // Initialize ImGui WebGPU backend
    ImGui_ImplWGPU_InitInfo init_info = {};
    init_info.Device = g_GfxData.device;
    init_info.NumFramesInFlight = 3; // Typically 3 for WebGPU
    init_info.RenderTargetFormat = g_GfxData.swapChainFormat;

    if (!ImGui_ImplWGPU_Init(&init_info))
        return false;

    return true;
}

// ImPlatform API - GfxCheck
IMPLATFORM_API bool ImPlatform_GfxCheck(void)
{
    // WebGPU doesn't have device loss like D3D9
    return true;
}

// ImPlatform API - GfxAPINewFrame
IMPLATFORM_API void ImPlatform_GfxAPINewFrame(void)
{
    ImGui_ImplWGPU_NewFrame();
}

// ImPlatform API - GfxAPIClear
IMPLATFORM_API bool ImPlatform_GfxAPIClear(ImVec4 const vClearColor)
{
    // Get current texture from swapchain
    WGPUTextureView backbuffer = wgpuSwapChainGetCurrentTextureView(g_GfxData.swapChain);
    if (!backbuffer)
        return false;

    // Setup render pass descriptor
    WGPURenderPassColorAttachment color_attachment = {};
    color_attachment.view = backbuffer;
    color_attachment.loadOp = WGPULoadOp_Clear;
    color_attachment.storeOp = WGPUStoreOp_Store;
    color_attachment.clearValue.r = vClearColor.x * vClearColor.w;
    color_attachment.clearValue.g = vClearColor.y * vClearColor.w;
    color_attachment.clearValue.b = vClearColor.z * vClearColor.w;
    color_attachment.clearValue.a = vClearColor.w;

    // Store in render pass descriptor (would need to be in GfxData)
    // This is simplified - actual implementation needs command encoder

    wgpuTextureViewRelease(backbuffer);
    return true;
}

// ImPlatform API - GfxAPIRender
IMPLATFORM_API bool ImPlatform_GfxAPIRender(ImVec4 const vClearColor)
{
    (void)vClearColor;

    // NOTE: WebGPU rendering requires:
    // 1. Get current texture view from swapchain
    // 2. Create command encoder
    // 3. Begin render pass
    // 4. Call ImGui_ImplWGPU_RenderDrawData
    // 5. End render pass
    // 6. Finish command encoder
    // 7. Submit commands to queue
    //
    // This requires maintaining command encoder state in GfxData

    // ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass_encoder);

    return false; // Not fully implemented
}

// ImPlatform API - GfxViewportPre
IMPLATFORM_API void ImPlatform_GfxViewportPre(void)
{
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
    }
}

// ImPlatform API - GfxViewportPost
IMPLATFORM_API void ImPlatform_GfxViewportPost(void)
{
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::RenderPlatformWindowsDefault();
    }
}

// ImPlatform API - GfxAPISwapBuffer
IMPLATFORM_API bool ImPlatform_GfxAPISwapBuffer(void)
{
    // Present is implicit in WebGPU - swapchain automatically presents
    // when you release the texture view
    wgpuSwapChainPresent(g_GfxData.swapChain);
    return true;
}

// ImPlatform API - ShutdownGfxAPI
IMPLATFORM_API void ImPlatform_ShutdownGfxAPI(void)
{
    // Nothing special needed
}

// ImPlatform API - ShutdownWindow
IMPLATFORM_API void ImPlatform_ShutdownWindow(void)
{
    ImGui_ImplWGPU_Shutdown();
    ImPlatform_Gfx_CleanupDevice_WebGPU(&g_GfxData);
}

// ============================================================================
// Texture Creation API - WebGPU Implementation
// ============================================================================

// Helper to get WebGPU format from ImPlatform format
static WGPUTextureFormat ImPlatform_GetWebGPUFormat(ImPlatform_PixelFormat format, int* out_bytes_per_pixel)
{
    switch (format)
    {
    case ImPlatform_PixelFormat_R8:
        *out_bytes_per_pixel = 1;
        return WGPUTextureFormat_R8Unorm;
    case ImPlatform_PixelFormat_RG8:
        *out_bytes_per_pixel = 2;
        return WGPUTextureFormat_RG8Unorm;
    case ImPlatform_PixelFormat_RGB8:
        // WebGPU doesn't have RGB8, use RGBA8
        *out_bytes_per_pixel = 4;
        return WGPUTextureFormat_RGBA8Unorm;
    case ImPlatform_PixelFormat_RGBA8:
        *out_bytes_per_pixel = 4;
        return WGPUTextureFormat_RGBA8Unorm;
    case ImPlatform_PixelFormat_R16:
        *out_bytes_per_pixel = 2;
        return WGPUTextureFormat_R16Unorm;
    case ImPlatform_PixelFormat_RG16:
        *out_bytes_per_pixel = 4;
        return WGPUTextureFormat_RG16Unorm;
    case ImPlatform_PixelFormat_RGBA16:
        *out_bytes_per_pixel = 8;
        return WGPUTextureFormat_RGBA16Unorm;
    case ImPlatform_PixelFormat_R32F:
        *out_bytes_per_pixel = 4;
        return WGPUTextureFormat_R32Float;
    case ImPlatform_PixelFormat_RG32F:
        *out_bytes_per_pixel = 8;
        return WGPUTextureFormat_RG32Float;
    case ImPlatform_PixelFormat_RGBA32F:
        *out_bytes_per_pixel = 16;
        return WGPUTextureFormat_RGBA32Float;
    default:
        *out_bytes_per_pixel = 4;
        return WGPUTextureFormat_RGBA8Unorm;
    }
}

IMPLATFORM_API ImPlatform_TextureDesc ImPlatform_TextureDesc_Default(unsigned int width, unsigned int height)
{
    ImPlatform_TextureDesc desc;
    desc.width = width;
    desc.height = height;
    desc.format = ImPlatform_PixelFormat_RGBA8;
    desc.min_filter = ImPlatform_TextureFilter_Linear;
    desc.mag_filter = ImPlatform_TextureFilter_Linear;
    desc.wrap_u = ImPlatform_TextureWrap_Clamp;
    desc.wrap_v = ImPlatform_TextureWrap_Clamp;
    return desc;
}

IMPLATFORM_API ImTextureID ImPlatform_CreateTexture(const void* pixel_data, const ImPlatform_TextureDesc* desc)
{
    if (!desc || !pixel_data || !g_GfxData.device)
        return NULL;

    int bytes_per_pixel;
    WGPUTextureFormat format = ImPlatform_GetWebGPUFormat(desc->format, &bytes_per_pixel);

    // Create texture
    WGPUTextureDescriptor tex_desc = {};
    tex_desc.label = "ImPlatform Texture";
    tex_desc.dimension = WGPUTextureDimension_2D;
    tex_desc.size.width = desc->width;
    tex_desc.size.height = desc->height;
    tex_desc.size.depthOrArrayLayers = 1;
    tex_desc.mipLevelCount = 1;
    tex_desc.sampleCount = 1;
    tex_desc.format = format;
    tex_desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;

    WGPUTexture texture = wgpuDeviceCreateTexture(g_GfxData.device, &tex_desc);
    if (!texture)
        return NULL;

    // Upload texture data
    WGPUImageCopyTexture dst = {};
    dst.texture = texture;
    dst.mipLevel = 0;
    dst.origin = {0, 0, 0};
    dst.aspect = WGPUTextureAspect_All;

    WGPUTextureDataLayout layout = {};
    layout.offset = 0;
    layout.bytesPerRow = desc->width * bytes_per_pixel;
    layout.rowsPerImage = desc->height;

    WGPUExtent3D writeSize = {};
    writeSize.width = desc->width;
    writeSize.height = desc->height;
    writeSize.depthOrArrayLayers = 1;

    wgpuQueueWriteTexture(g_GfxData.queue, &dst, pixel_data,
                          desc->width * desc->height * bytes_per_pixel, &layout, &writeSize);

    // Create texture view
    WGPUTextureViewDescriptor view_desc = {};
    view_desc.format = format;
    view_desc.dimension = WGPUTextureViewDimension_2D;
    view_desc.baseMipLevel = 0;
    view_desc.mipLevelCount = 1;
    view_desc.baseArrayLayer = 0;
    view_desc.arrayLayerCount = 1;
    view_desc.aspect = WGPUTextureAspect_All;

    WGPUTextureView texture_view = wgpuTextureCreateView(texture, &view_desc);
    if (!texture_view)
    {
        wgpuTextureRelease(texture);
        return NULL;
    }

    // Create sampler
    WGPUSamplerDescriptor sampler_desc = {};
    sampler_desc.addressModeU = WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeV = WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeW = WGPUAddressMode_ClampToEdge;
    sampler_desc.magFilter = (desc->mag_filter == ImPlatform_TextureFilter_Nearest) ? WGPUFilterMode_Nearest : WGPUFilterMode_Linear;
    sampler_desc.minFilter = (desc->min_filter == ImPlatform_TextureFilter_Nearest) ? WGPUFilterMode_Nearest : WGPUFilterMode_Linear;
    sampler_desc.mipmapFilter = WGPUMipmapFilterMode_Linear;
    sampler_desc.lodMinClamp = 0.0f;
    sampler_desc.lodMaxClamp = 1000.0f;
    sampler_desc.compare = WGPUCompareFunction_Undefined;
    sampler_desc.maxAnisotropy = 1;

    if (desc->wrap_u == ImPlatform_TextureWrap_Repeat)
        sampler_desc.addressModeU = WGPUAddressMode_Repeat;
    else if (desc->wrap_u == ImPlatform_TextureWrap_Mirror)
        sampler_desc.addressModeU = WGPUAddressMode_MirrorRepeat;

    if (desc->wrap_v == ImPlatform_TextureWrap_Repeat)
        sampler_desc.addressModeV = WGPUAddressMode_Repeat;
    else if (desc->wrap_v == ImPlatform_TextureWrap_Mirror)
        sampler_desc.addressModeV = WGPUAddressMode_MirrorRepeat;

    WGPUSampler sampler = wgpuDeviceCreateSampler(g_GfxData.device, &sampler_desc);
    if (!sampler)
    {
        wgpuTextureViewRelease(texture_view);
        wgpuTextureRelease(texture);
        return NULL;
    }

    // Note: We're leaking texture, texture_view, and sampler here
    // A production implementation would need proper resource tracking
    // For WebGPU, ImTextureID is typically the texture view
    return (ImTextureID)texture_view;
}

IMPLATFORM_API bool ImPlatform_UpdateTexture(ImTextureID texture_id, const void* pixel_data,
                                              unsigned int x, unsigned int y,
                                              unsigned int width, unsigned int height)
{
    // WebGPU texture updates would require tracking the original texture
    // from the texture view, which we don't currently do
    // Not implemented
    return false;
}

IMPLATFORM_API void ImPlatform_DestroyTexture(ImTextureID texture_id)
{
    if (!texture_id)
        return;

    // Release the texture view
    WGPUTextureView texture_view = (WGPUTextureView)texture_id;
    wgpuTextureViewRelease(texture_view);

    // Note: We're not cleaning up the texture or sampler because we don't track them
    // A production implementation would need proper resource tracking
}

// ============================================================================
// Custom Vertex/Index Buffer Management API - WebGPU (Stubs)
// ============================================================================

IMPLATFORM_API ImPlatform_VertexBuffer ImPlatform_CreateVertexBuffer(const void* vertex_data, const ImPlatform_VertexBufferDesc* desc) { return NULL; }
IMPLATFORM_API bool ImPlatform_UpdateVertexBuffer(ImPlatform_VertexBuffer vertex_buffer, const void* vertex_data, unsigned int vertex_count, unsigned int offset) { return false; }
IMPLATFORM_API void ImPlatform_DestroyVertexBuffer(ImPlatform_VertexBuffer vertex_buffer) {}
IMPLATFORM_API ImPlatform_IndexBuffer ImPlatform_CreateIndexBuffer(const void* index_data, const ImPlatform_IndexBufferDesc* desc) { return NULL; }
IMPLATFORM_API bool ImPlatform_UpdateIndexBuffer(ImPlatform_IndexBuffer index_buffer, const void* index_data, unsigned int index_count, unsigned int offset) { return false; }
IMPLATFORM_API void ImPlatform_DestroyIndexBuffer(ImPlatform_IndexBuffer index_buffer) {}
IMPLATFORM_API void ImPlatform_BindVertexBuffer(ImPlatform_VertexBuffer vertex_buffer) {}
IMPLATFORM_API void ImPlatform_BindIndexBuffer(ImPlatform_IndexBuffer index_buffer) {}
IMPLATFORM_API void ImPlatform_DrawIndexed(unsigned int primitive_type, unsigned int index_count, unsigned int start_index) {}

// Custom Shader System API - WebGPU (Stubs)
IMPLATFORM_API ImPlatform_Shader ImPlatform_CreateShader(const ImPlatform_ShaderDesc* desc) { return NULL; }
IMPLATFORM_API void ImPlatform_DestroyShader(ImPlatform_Shader shader) {}
IMPLATFORM_API ImPlatform_ShaderProgram ImPlatform_CreateShaderProgram(ImPlatform_Shader vertex_shader, ImPlatform_Shader fragment_shader) { return NULL; }
IMPLATFORM_API void ImPlatform_DestroyShaderProgram(ImPlatform_ShaderProgram program) {}
IMPLATFORM_API void ImPlatform_UseShaderProgram(ImPlatform_ShaderProgram program) {}
IMPLATFORM_API bool ImPlatform_SetShaderUniform(ImPlatform_ShaderProgram program, const char* name, const void* data, unsigned int size) { return false; }
IMPLATFORM_API bool ImPlatform_SetShaderTexture(ImPlatform_ShaderProgram program, const char* name, unsigned int slot, ImTextureID texture) { return false; }

IMPLATFORM_API void ImPlatform_BeginUniformBlock(ImPlatform_ShaderProgram program)
{
    if (!program)
        return;

    g_CurrentUniformBlockProgram = program;

    // Free any previous block data
    if (g_UniformBlockData)
    {
        free(g_UniformBlockData);
        g_UniformBlockData = nullptr;
        g_UniformBlockSize = 0;
    }
}

IMPLATFORM_API bool ImPlatform_SetUniform(const char* name, const void* data, unsigned int size)
{
    if (!g_CurrentUniformBlockProgram || !data || size == 0)
        return false;

    // Accumulate all uniforms into a single buffer
    // Allocate or expand the buffer
    size_t new_size = g_UniformBlockSize + size;
    void* new_data = realloc(g_UniformBlockData, new_size);
    if (!new_data)
        return false;

    g_UniformBlockData = new_data;
    memcpy((char*)g_UniformBlockData + g_UniformBlockSize, data, size);
    g_UniformBlockSize = new_size;

    return true;
}

IMPLATFORM_API void ImPlatform_EndUniformBlock(ImPlatform_ShaderProgram program)
{
    if (!program || program != g_CurrentUniformBlockProgram)
        return;

    if (g_UniformBlockData && g_UniformBlockSize > 0)
    {
        // Upload the accumulated uniform block to the shader
        ImPlatform_SetShaderUniform(program, "uniformBlock", g_UniformBlockData, g_UniformBlockSize);

        // Clean up
        free(g_UniformBlockData);
        g_UniformBlockData = nullptr;
        g_UniformBlockSize = 0;
    }

    g_CurrentUniformBlockProgram = nullptr;
}

// ============================================================================
// Custom Shader DrawList Integration
// ============================================================================

// ImDrawCallback handler to activate a custom shader
static void ImPlatform_SetCustomShader(const ImDrawList* parent_list, const ImDrawCmd* cmd)
{
    // WebGPU custom shaders stub - needs render pipeline setup
    // This is a stub for API consistency
}

IMPLATFORM_API void ImPlatform_BeginCustomShader(ImDrawList* draw, ImPlatform_ShaderProgram shader)
{
    if (!draw || !shader)
        return;

    draw->AddCallback(&ImPlatform_SetCustomShader, shader);
}

IMPLATFORM_API void ImPlatform_EndCustomShader(ImDrawList* draw)
{
    if (!draw)
        return;

    draw->AddCallback(ImDrawCallback_ResetRenderState, NULL);
}

#endif // IM_GFX_WGPU
