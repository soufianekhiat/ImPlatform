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

// ============================================================================
// Custom Shader System API - WebGPU Implementation
// ============================================================================

// Shader data structures
struct ImPlatform_ShaderData_WebGPU
{
    WGPUShaderModule shaderModule;
    ImPlatform_ShaderStage stage;
    char* entryPoint;
};

struct ImPlatform_ShaderProgramData_WebGPU
{
    WGPUShaderModule vertexModule;
    WGPUShaderModule fragmentModule;
    WGPURenderPipeline renderPipeline;
    WGPUBindGroupLayout bindGroupLayout;
    WGPUBindGroup bindGroup;
    WGPUBuffer uniformBuffer;
    void* uniformData;
    size_t uniformDataSize;
    bool uniformDataDirty;
    char* vertexEntryPoint;
    char* fragmentEntryPoint;
};

// Current draw data for custom shader rendering (needed for multi-viewport)
static ImDrawData* g_CurrentDrawData = nullptr;

IMPLATFORM_API ImPlatform_Shader ImPlatform_CreateShader(const ImPlatform_ShaderDesc* desc)
{
    if (!desc || !desc->source_code || !g_GfxData.device)
        return NULL;

    // Create WGSL shader module
    WGPUShaderModuleWGSLDescriptor wgsl_desc = {};
    wgsl_desc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgsl_desc.code = desc->source_code;

    WGPUShaderModuleDescriptor module_desc = {};
    module_desc.nextInChain = (const WGPUChainedStruct*)&wgsl_desc;
    module_desc.label = "ImPlatform Custom Shader";

    WGPUShaderModule shader_module = wgpuDeviceCreateShaderModule(g_GfxData.device, &module_desc);
    if (!shader_module)
        return NULL;

    // Create shader data
    ImPlatform_ShaderData_WebGPU* shader_data = new ImPlatform_ShaderData_WebGPU();
    shader_data->shaderModule = shader_module;
    shader_data->stage = desc->stage;

    // Copy entry point
    const char* entry = desc->entry_point ? desc->entry_point : "main";
    shader_data->entryPoint = (char*)malloc(strlen(entry) + 1);
    strcpy(shader_data->entryPoint, entry);

    return shader_data;
}

IMPLATFORM_API void ImPlatform_DestroyShader(ImPlatform_Shader shader)
{
    if (!shader)
        return;

    ImPlatform_ShaderData_WebGPU* shader_data = (ImPlatform_ShaderData_WebGPU*)shader;

    if (shader_data->shaderModule)
    {
        wgpuShaderModuleRelease(shader_data->shaderModule);
        shader_data->shaderModule = nullptr;
    }

    if (shader_data->entryPoint)
    {
        free(shader_data->entryPoint);
        shader_data->entryPoint = nullptr;
    }

    delete shader_data;
}

IMPLATFORM_API ImPlatform_ShaderProgram ImPlatform_CreateShaderProgram(ImPlatform_Shader vertex_shader, ImPlatform_Shader fragment_shader)
{
    if (!vertex_shader || !fragment_shader || !g_GfxData.device)
        return NULL;

    ImPlatform_ShaderData_WebGPU* vs_data = (ImPlatform_ShaderData_WebGPU*)vertex_shader;
    ImPlatform_ShaderData_WebGPU* fs_data = (ImPlatform_ShaderData_WebGPU*)fragment_shader;

    // Create bind group layout for uniforms
    WGPUBindGroupLayoutEntry bg_layout_entries[2] = {};

    // Uniform buffer binding
    bg_layout_entries[0].binding = 0;
    bg_layout_entries[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    bg_layout_entries[0].buffer.type = WGPUBufferBindingType_Uniform;
    bg_layout_entries[0].buffer.minBindingSize = 256; // Minimum uniform buffer size

    // Texture binding
    bg_layout_entries[1].binding = 1;
    bg_layout_entries[1].visibility = WGPUShaderStage_Fragment;
    bg_layout_entries[1].texture.sampleType = WGPUTextureSampleType_Float;
    bg_layout_entries[1].texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutDescriptor bg_layout_desc = {};
    bg_layout_desc.entryCount = 2;
    bg_layout_desc.entries = bg_layout_entries;

    WGPUBindGroupLayout bind_group_layout = wgpuDeviceCreateBindGroupLayout(g_GfxData.device, &bg_layout_desc);
    if (!bind_group_layout)
        return NULL;

    // Create pipeline layout
    WGPUPipelineLayoutDescriptor pipeline_layout_desc = {};
    pipeline_layout_desc.bindGroupLayoutCount = 1;
    pipeline_layout_desc.bindGroupLayouts = &bind_group_layout;

    WGPUPipelineLayout pipeline_layout = wgpuDeviceCreatePipelineLayout(g_GfxData.device, &pipeline_layout_desc);
    if (!pipeline_layout)
    {
        wgpuBindGroupLayoutRelease(bind_group_layout);
        return NULL;
    }

    // Create vertex state
    WGPUVertexAttribute vertex_attributes[3] = {};

    // Position (float2)
    vertex_attributes[0].format = WGPUVertexFormat_Float32x2;
    vertex_attributes[0].offset = 0;
    vertex_attributes[0].shaderLocation = 0;

    // UV (float2)
    vertex_attributes[1].format = WGPUVertexFormat_Float32x2;
    vertex_attributes[1].offset = 8;
    vertex_attributes[1].shaderLocation = 1;

    // Color (unorm8x4)
    vertex_attributes[2].format = WGPUVertexFormat_Unorm8x4;
    vertex_attributes[2].offset = 16;
    vertex_attributes[2].shaderLocation = 2;

    WGPUVertexBufferLayout vertex_buffer_layout = {};
    vertex_buffer_layout.arrayStride = sizeof(ImDrawVert);
    vertex_buffer_layout.stepMode = WGPUVertexStepMode_Vertex;
    vertex_buffer_layout.attributeCount = 3;
    vertex_buffer_layout.attributes = vertex_attributes;

    WGPUVertexState vertex_state = {};
    vertex_state.module = vs_data->shaderModule;
    vertex_state.entryPoint = vs_data->entryPoint;
    vertex_state.bufferCount = 1;
    vertex_state.buffers = &vertex_buffer_layout;

    // Create fragment state
    WGPUBlendState blend_state = {};
    blend_state.color.operation = WGPUBlendOperation_Add;
    blend_state.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blend_state.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blend_state.alpha.operation = WGPUBlendOperation_Add;
    blend_state.alpha.srcFactor = WGPUBlendFactor_One;
    blend_state.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;

    WGPUColorTargetState color_target = {};
    color_target.format = g_GfxData.swapChainFormat;
    color_target.blend = &blend_state;
    color_target.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragment_state = {};
    fragment_state.module = fs_data->shaderModule;
    fragment_state.entryPoint = fs_data->entryPoint;
    fragment_state.targetCount = 1;
    fragment_state.targets = &color_target;

    // Create render pipeline
    WGPURenderPipelineDescriptor pipeline_desc = {};
    pipeline_desc.layout = pipeline_layout;
    pipeline_desc.vertex = vertex_state;
    pipeline_desc.fragment = &fragment_state;
    pipeline_desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipeline_desc.primitive.cullMode = WGPUCullMode_None;
    pipeline_desc.multisample.count = 1;
    pipeline_desc.multisample.mask = 0xFFFFFFFF;

    WGPURenderPipeline render_pipeline = wgpuDeviceCreateRenderPipeline(g_GfxData.device, &pipeline_desc);

    wgpuPipelineLayoutRelease(pipeline_layout);

    if (!render_pipeline)
    {
        wgpuBindGroupLayoutRelease(bind_group_layout);
        return NULL;
    }

    // Create program data
    ImPlatform_ShaderProgramData_WebGPU* program_data = new ImPlatform_ShaderProgramData_WebGPU();
    program_data->vertexModule = vs_data->shaderModule;
    program_data->fragmentModule = fs_data->shaderModule;
    program_data->renderPipeline = render_pipeline;
    program_data->bindGroupLayout = bind_group_layout;
    program_data->bindGroup = nullptr;
    program_data->uniformBuffer = nullptr;
    program_data->uniformData = nullptr;
    program_data->uniformDataSize = 0;
    program_data->uniformDataDirty = false;

    // Copy entry points
    program_data->vertexEntryPoint = (char*)malloc(strlen(vs_data->entryPoint) + 1);
    strcpy(program_data->vertexEntryPoint, vs_data->entryPoint);
    program_data->fragmentEntryPoint = (char*)malloc(strlen(fs_data->entryPoint) + 1);
    strcpy(program_data->fragmentEntryPoint, fs_data->entryPoint);

    return program_data;
}

IMPLATFORM_API void ImPlatform_DestroyShaderProgram(ImPlatform_ShaderProgram program)
{
    if (!program)
        return;

    ImPlatform_ShaderProgramData_WebGPU* program_data = (ImPlatform_ShaderProgramData_WebGPU*)program;

    if (program_data->bindGroup)
    {
        wgpuBindGroupRelease(program_data->bindGroup);
        program_data->bindGroup = nullptr;
    }

    if (program_data->uniformBuffer)
    {
        wgpuBufferRelease(program_data->uniformBuffer);
        program_data->uniformBuffer = nullptr;
    }

    if (program_data->bindGroupLayout)
    {
        wgpuBindGroupLayoutRelease(program_data->bindGroupLayout);
        program_data->bindGroupLayout = nullptr;
    }

    if (program_data->renderPipeline)
    {
        wgpuRenderPipelineRelease(program_data->renderPipeline);
        program_data->renderPipeline = nullptr;
    }

    if (program_data->uniformData)
    {
        free(program_data->uniformData);
        program_data->uniformData = nullptr;
    }

    if (program_data->vertexEntryPoint)
    {
        free(program_data->vertexEntryPoint);
        program_data->vertexEntryPoint = nullptr;
    }

    if (program_data->fragmentEntryPoint)
    {
        free(program_data->fragmentEntryPoint);
        program_data->fragmentEntryPoint = nullptr;
    }

    delete program_data;
}

IMPLATFORM_API void ImPlatform_UseShaderProgram(ImPlatform_ShaderProgram program)
{
    // WebGPU doesn't have a global "use program" concept
    // Shaders are bound per render pass via callbacks
}

IMPLATFORM_API bool ImPlatform_SetShaderUniform(ImPlatform_ShaderProgram program, const char* name, const void* data, unsigned int size)
{
    if (!program || !data || size == 0)
        return false;

    ImPlatform_ShaderProgramData_WebGPU* program_data = (ImPlatform_ShaderProgramData_WebGPU*)program;

    // Allocate or reallocate uniform data buffer
    if (!program_data->uniformData || program_data->uniformDataSize != size)
    {
        if (program_data->uniformData)
            free(program_data->uniformData);

        program_data->uniformData = malloc(size);
        if (!program_data->uniformData)
            return false;

        program_data->uniformDataSize = size;
    }

    // Copy uniform data
    memcpy(program_data->uniformData, data, size);
    program_data->uniformDataDirty = true;

    return true;
}

IMPLATFORM_API bool ImPlatform_SetShaderTexture(ImPlatform_ShaderProgram program, const char* name, unsigned int slot, ImTextureID texture)
{
    // WebGPU texture binding happens via bind groups in the render callback
    // This is a stub for API consistency
    return true;
}

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
        ImPlatform_ShaderProgramData_WebGPU* program_data = (ImPlatform_ShaderProgramData_WebGPU*)program;

        // Create or update uniform buffer
        if (!program_data->uniformBuffer || program_data->uniformDataSize != g_UniformBlockSize)
        {
            if (program_data->uniformBuffer)
            {
                wgpuBufferRelease(program_data->uniformBuffer);
                program_data->uniformBuffer = nullptr;
            }

            // Align size to 256 bytes (WebGPU uniform buffer alignment requirement)
            size_t aligned_size = (g_UniformBlockSize + 255) & ~255;

            WGPUBufferDescriptor buffer_desc = {};
            buffer_desc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
            buffer_desc.size = aligned_size;
            buffer_desc.mappedAtCreation = false;

            program_data->uniformBuffer = wgpuDeviceCreateBuffer(g_GfxData.device, &buffer_desc);
        }

        // Upload the accumulated uniform block to the buffer
        if (program_data->uniformBuffer)
        {
            wgpuQueueWriteBuffer(g_GfxData.queue, program_data->uniformBuffer, 0, g_UniformBlockData, g_UniformBlockSize);

            // Store uniform data in program data for later use
            if (!program_data->uniformData || program_data->uniformDataSize != g_UniformBlockSize)
            {
                if (program_data->uniformData)
                    free(program_data->uniformData);

                program_data->uniformData = malloc(g_UniformBlockSize);
                program_data->uniformDataSize = g_UniformBlockSize;
            }

            if (program_data->uniformData)
            {
                memcpy(program_data->uniformData, g_UniformBlockData, g_UniformBlockSize);
                program_data->uniformDataDirty = true;
            }
        }

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

// Wrapper for ImGui_ImplWGPU_RenderDrawData that stores draw_data for custom shader callbacks
static void ImPlatform_RenderDrawDataWrapper(ImDrawData* draw_data, WGPURenderPassEncoder pass_encoder)
{
    // Store draw data for custom shader callbacks (needed for multi-viewport)
    g_CurrentDrawData = draw_data;

    ImGui_ImplWGPU_RenderDrawData(draw_data, pass_encoder);

    // NOTE: Don't clear g_CurrentDrawData here - it will be updated by the next render call
}

// ImDrawCallback handler to activate a custom shader
static void ImPlatform_SetCustomShader(const ImDrawList* parent_list, const ImDrawCmd* cmd)
{
    ImPlatform_ShaderProgram program = (ImPlatform_ShaderProgram)cmd->UserCallbackData;
    if (!program)
        return;

    ImPlatform_ShaderProgramData_WebGPU* program_data = (ImPlatform_ShaderProgramData_WebGPU*)program;

    // Find which viewport owns this draw list
    ImDrawData* draw_data = nullptr;

    // First, try the cached draw data
    if (g_CurrentDrawData)
    {
        draw_data = g_CurrentDrawData;
    }
    else
    {
        // Fallback: search through all viewports to find which one owns this draw list
        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        for (int i = 0; i < platform_io.Viewports.Size; i++)
        {
            ImGuiViewport* viewport = platform_io.Viewports[i];
            if (viewport->DrawData)
            {
                for (int j = 0; j < viewport->DrawData->CmdLists.Size; j++)
                {
                    if (viewport->DrawData->CmdLists[j] == parent_list)
                    {
                        draw_data = viewport->DrawData;
                        break;
                    }
                }
                if (draw_data)
                    break;
            }
        }
    }

    if (!draw_data)
        return;

    // Calculate projection matrix for this viewport
    float L = draw_data->DisplayPos.x;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float T = draw_data->DisplayPos.y;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

    float ortho_projection[4][4] =
    {
        { 2.0f / (R - L),     0.0f,              0.0f, 0.0f },
        { 0.0f,               2.0f / (T - B),    0.0f, 0.0f },
        { 0.0f,               0.0f,              0.5f, 0.0f },
        { (R + L) / (L - R),  (T + B) / (B - T), 0.5f, 1.0f },
    };

    // Note: WebGPU backend binding logic would go here
    // This is a simplified implementation that demonstrates the pattern
    // In a full implementation, you would:
    // 1. Get the current render pass encoder from ImGui's WebGPU render state
    // 2. Bind the custom pipeline
    // 3. Create/update bind group with uniform buffer and texture
    // 4. Set the bind group on the render pass encoder
    // 5. The actual draw commands will use this pipeline

    // Since we don't have direct access to the render pass encoder here without modifying ImGui's WebGPU backend,
    // this serves as a reference implementation showing the required logic
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
