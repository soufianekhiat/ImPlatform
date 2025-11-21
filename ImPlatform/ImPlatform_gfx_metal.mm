// dear imgui: Graphics API Abstraction - Metal Backend
// This handles Metal device creation, rendering, and presentation
//
// NOTE: Metal is Apple-only and requires Objective-C++.
// This file must be compiled as .mm (Objective-C++)

#include "ImPlatform_Internal.h"

#ifdef IM_GFX_METAL

#include "../imgui.h"
#include "../backends/imgui_impl_metal.h"

#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

#ifdef IM_PLATFORM_APPLE
    #import <Cocoa/Cocoa.h>
#endif

// Global state
static ImPlatform_GfxData_Metal g_GfxData = { 0 };

// Uniform block API state
static ImPlatform_ShaderProgram g_CurrentUniformBlockProgram = nullptr;
static void* g_UniformBlockData = nullptr;
static size_t g_UniformBlockSize = 0;

// Internal API - Get Metal gfx data
ImPlatform_GfxData_Metal* ImPlatform_Gfx_GetData_Metal(void)
{
    return &g_GfxData;
}

// Internal API - Create Metal device
void* ImPlatform_Gfx_CreateDevice_Metal(void* pNSWindow, ImPlatform_GfxData_Metal* pData)
{
    @autoreleasepool {
        NSWindow* window = (__bridge NSWindow*)pNSWindow;

        // Create Metal device
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device)
            return nullptr;

        // Create Metal layer
        CAMetalLayer* layer = [CAMetalLayer layer];
        layer.device = device;
        layer.pixelFormat = MTLPixelFormatBGRA8Unorm;

        // Set layer on window's content view
        NSView* contentView = window.contentView;
        contentView.layer = layer;
        contentView.wantsLayer = YES;

        // Create command queue
        id<MTLCommandQueue> commandQueue = [device newCommandQueue];

        // Store in data structure (bridged to void*)
        pData->pMetalDevice = (__bridge_retained void*)device;
        pData->pMetalLayer = (__bridge_retained void*)layer;
        pData->pCommandQueue = (__bridge_retained void*)commandQueue;

        // Create render pass descriptor
        MTLRenderPassDescriptor* renderPassDesc = [MTLRenderPassDescriptor new];
        pData->pRenderPassDescriptor = (__bridge_retained void*)renderPassDesc;

        return pData->pMetalLayer;
    }
}

// Internal API - Cleanup Metal device
void ImPlatform_Gfx_CleanupDevice_Metal(ImPlatform_GfxData_Metal* pData)
{
    @autoreleasepool {
        if (pData->pRenderPassDescriptor)
        {
            CFRelease(pData->pRenderPassDescriptor);
            pData->pRenderPassDescriptor = nullptr;
        }

        if (pData->pCommandQueue)
        {
            CFRelease(pData->pCommandQueue);
            pData->pCommandQueue = nullptr;
        }

        if (pData->pMetalLayer)
        {
            CFRelease(pData->pMetalLayer);
            pData->pMetalLayer = nullptr;
        }

        if (pData->pMetalDevice)
        {
            CFRelease(pData->pMetalDevice);
            pData->pMetalDevice = nullptr;
        }
    }
}

// ImPlatform API - InitGfxAPI
IMPLATFORM_API bool ImPlatform_InitGfxAPI(void)
{
#ifdef IM_PLATFORM_APPLE
    // TODO: Get NSWindow from Apple platform backend
    // void* pNSWindow = ImPlatform_App_GetNSWindow();
    // void* pLayer = ImPlatform_Gfx_CreateDevice_Metal(pNSWindow, &g_GfxData);
    // return (pLayer != nullptr);
    return false; // Not implemented - need Apple platform backend
#else
    return false;
#endif
}

// ImPlatform API - InitGfx
IMPLATFORM_API bool ImPlatform_InitGfx(void)
{
    @autoreleasepool {
        id<MTLDevice> device = (__bridge id<MTLDevice>)g_GfxData.pMetalDevice;
        id<MTLCommandQueue> commandQueue = (__bridge id<MTLCommandQueue>)g_GfxData.pCommandQueue;

        if (!ImGui_ImplMetal_Init(device))
            return false;

        return true;
    }
}

// ImPlatform API - GfxCheck
IMPLATFORM_API bool ImPlatform_GfxCheck(void)
{
    // Metal doesn't have device loss
    return true;
}

// ImPlatform API - GfxAPINewFrame
IMPLATFORM_API void ImPlatform_GfxAPINewFrame(void)
{
    @autoreleasepool {
        CAMetalLayer* layer = (__bridge CAMetalLayer*)g_GfxData.pMetalLayer;
        ImGui_ImplMetal_NewFrame((__bridge MTLRenderPassDescriptor*)g_GfxData.pRenderPassDescriptor);
    }
}

// ImPlatform API - GfxAPIClear
IMPLATFORM_API bool ImPlatform_GfxAPIClear(ImVec4 const vClearColor)
{
    @autoreleasepool {
        CAMetalLayer* layer = (__bridge CAMetalLayer*)g_GfxData.pMetalLayer;
        id<CAMetalDrawable> drawable = [layer nextDrawable];
        if (!drawable)
            return false;

        MTLRenderPassDescriptor* renderPassDesc = (__bridge MTLRenderPassDescriptor*)g_GfxData.pRenderPassDescriptor;
        renderPassDesc.colorAttachments[0].texture = drawable.texture;
        renderPassDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
        renderPassDesc.colorAttachments[0].clearColor = MTLClearColorMake(
            vClearColor.x * vClearColor.w,
            vClearColor.y * vClearColor.w,
            vClearColor.z * vClearColor.w,
            vClearColor.w);

        return true;
    }
}

// ImPlatform API - GfxAPIRender
IMPLATFORM_API bool ImPlatform_GfxAPIRender(ImVec4 const vClearColor)
{
    @autoreleasepool {
        (void)vClearColor;

        id<MTLCommandQueue> commandQueue = (__bridge id<MTLCommandQueue>)g_GfxData.pCommandQueue;
        id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];

        MTLRenderPassDescriptor* renderPassDesc = (__bridge MTLRenderPassDescriptor*)g_GfxData.pRenderPassDescriptor;
        id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDesc];

        // Use wrapper to store draw data for custom shader callbacks
        ImPlatform_RenderDrawDataWrapper(ImGui::GetDrawData(), commandBuffer, renderEncoder);

        [renderEncoder endEncoding];

        return true;
    }
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
    @autoreleasepool {
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::RenderPlatformWindowsDefault();
        }
    }
}

// ImPlatform API - GfxAPISwapBuffer
IMPLATFORM_API bool ImPlatform_GfxAPISwapBuffer(void)
{
    @autoreleasepool {
        CAMetalLayer* layer = (__bridge CAMetalLayer*)g_GfxData.pMetalLayer;
        id<CAMetalDrawable> drawable = [layer nextDrawable];

        id<MTLCommandQueue> commandQueue = (__bridge id<MTLCommandQueue>)g_GfxData.pCommandQueue;
        id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];

        [commandBuffer presentDrawable:drawable];
        [commandBuffer commit];

        return true;
    }
}

// ImPlatform API - ShutdownGfxAPI
IMPLATFORM_API void ImPlatform_ShutdownGfxAPI(void)
{
    // Nothing special needed
}

// ImPlatform API - ShutdownWindow
IMPLATFORM_API void ImPlatform_ShutdownWindow(void)
{
    ImGui_ImplMetal_Shutdown();
    ImPlatform_Gfx_CleanupDevice_Metal(&g_GfxData);
}

// ============================================================================
// Texture Creation API - Metal Implementation
// ============================================================================

// Helper to get Metal pixel format from ImPlatform format
static MTLPixelFormat ImPlatform_GetMetalFormat(ImPlatform_PixelFormat format, int* out_bytes_per_pixel)
{
    switch (format)
    {
    case ImPlatform_PixelFormat_R8:
        *out_bytes_per_pixel = 1;
        return MTLPixelFormatR8Unorm;
    case ImPlatform_PixelFormat_RG8:
        *out_bytes_per_pixel = 2;
        return MTLPixelFormatRG8Unorm;
    case ImPlatform_PixelFormat_RGB8:
        // Metal doesn't have RGB8, use RGBA8
        *out_bytes_per_pixel = 4;
        return MTLPixelFormatRGBA8Unorm;
    case ImPlatform_PixelFormat_RGBA8:
        *out_bytes_per_pixel = 4;
        return MTLPixelFormatRGBA8Unorm;
    case ImPlatform_PixelFormat_R16:
        *out_bytes_per_pixel = 2;
        return MTLPixelFormatR16Unorm;
    case ImPlatform_PixelFormat_RG16:
        *out_bytes_per_pixel = 4;
        return MTLPixelFormatRG16Unorm;
    case ImPlatform_PixelFormat_RGBA16:
        *out_bytes_per_pixel = 8;
        return MTLPixelFormatRGBA16Unorm;
    case ImPlatform_PixelFormat_R32F:
        *out_bytes_per_pixel = 4;
        return MTLPixelFormatR32Float;
    case ImPlatform_PixelFormat_RG32F:
        *out_bytes_per_pixel = 8;
        return MTLPixelFormatRG32Float;
    case ImPlatform_PixelFormat_RGBA32F:
        *out_bytes_per_pixel = 16;
        return MTLPixelFormatRGBA32Float;
    default:
        *out_bytes_per_pixel = 4;
        return MTLPixelFormatRGBA8Unorm;
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

    @autoreleasepool {
        int bytes_per_pixel;
        MTLPixelFormat format = ImPlatform_GetMetalFormat(desc->format, &bytes_per_pixel);

        // Create texture descriptor
        MTLTextureDescriptor* textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                                                                                                      width:desc->width
                                                                                                     height:desc->height
                                                                                                  mipmapped:NO];
        textureDescriptor.usage = MTLTextureUsageShaderRead;
        textureDescriptor.storageMode = MTLStorageModeManaged;

        // Create the texture
        id<MTLTexture> texture = [(__bridge id<MTLDevice>)g_GfxData.device newTextureWithDescriptor:textureDescriptor];
        if (!texture)
            return NULL;

        // Upload pixel data
        NSUInteger bytesPerRow = desc->width * bytes_per_pixel;
        MTLRegion region = MTLRegionMake2D(0, 0, desc->width, desc->height);
        [texture replaceRegion:region mipmapLevel:0 withBytes:pixel_data bytesPerRow:bytesPerRow];

        // Create sampler descriptor
        MTLSamplerDescriptor* samplerDescriptor = [[MTLSamplerDescriptor alloc] init];
        samplerDescriptor.minFilter = (desc->min_filter == ImPlatform_TextureFilter_Nearest) ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear;
        samplerDescriptor.magFilter = (desc->mag_filter == ImPlatform_TextureFilter_Nearest) ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear;
        samplerDescriptor.mipFilter = MTLSamplerMipFilterNotMipmapped;

        MTLSamplerAddressMode wrap_s = MTLSamplerAddressModeClampToEdge;
        MTLSamplerAddressMode wrap_t = MTLSamplerAddressModeClampToEdge;

        if (desc->wrap_u == ImPlatform_TextureWrap_Repeat)
            wrap_s = MTLSamplerAddressModeRepeat;
        else if (desc->wrap_u == ImPlatform_TextureWrap_Mirror)
            wrap_s = MTLSamplerAddressModeMirrorRepeat;

        if (desc->wrap_v == ImPlatform_TextureWrap_Repeat)
            wrap_t = MTLSamplerAddressModeRepeat;
        else if (desc->wrap_v == ImPlatform_TextureWrap_Mirror)
            wrap_t = MTLSamplerAddressModeMirrorRepeat;

        samplerDescriptor.sAddressMode = wrap_s;
        samplerDescriptor.tAddressMode = wrap_t;
        samplerDescriptor.rAddressMode = MTLSamplerAddressModeClampToEdge;

        // For Metal, ImTextureID is just the texture pointer
        // We don't create a sampler object as Metal binds samplers separately
        // ImGui's Metal backend handles samplers internally
        return (__bridge_retained void*)texture;
    }
}

IMPLATFORM_API bool ImPlatform_UpdateTexture(ImTextureID texture_id, const void* pixel_data,
                                              unsigned int x, unsigned int y,
                                              unsigned int width, unsigned int height)
{
    if (!texture_id || !pixel_data)
        return false;

    @autoreleasepool {
        id<MTLTexture> texture = (__bridge id<MTLTexture>)texture_id;

        // Determine bytes per pixel from texture format
        MTLPixelFormat format = texture.pixelFormat;
        int bytes_per_pixel = 4; // Default RGBA8

        switch (format)
        {
            case MTLPixelFormatR8Unorm: bytes_per_pixel = 1; break;
            case MTLPixelFormatRG8Unorm: bytes_per_pixel = 2; break;
            case MTLPixelFormatRGBA8Unorm: bytes_per_pixel = 4; break;
            case MTLPixelFormatR16Unorm: bytes_per_pixel = 2; break;
            case MTLPixelFormatRG16Unorm: bytes_per_pixel = 4; break;
            case MTLPixelFormatRGBA16Unorm: bytes_per_pixel = 8; break;
            case MTLPixelFormatR32Float: bytes_per_pixel = 4; break;
            case MTLPixelFormatRG32Float: bytes_per_pixel = 8; break;
            case MTLPixelFormatRGBA32Float: bytes_per_pixel = 16; break;
            default: break;
        }

        // Update the sub-region
        NSUInteger bytesPerRow = width * bytes_per_pixel;
        MTLRegion region = MTLRegionMake2D(x, y, width, height);
        [texture replaceRegion:region mipmapLevel:0 withBytes:pixel_data bytesPerRow:bytesPerRow];

        return true;
    }
}

IMPLATFORM_API void ImPlatform_DestroyTexture(ImTextureID texture_id)
{
    if (!texture_id)
        return;

    @autoreleasepool {
        // Release the bridged texture
        id<MTLTexture> texture = (__bridge_transfer id<MTLTexture>)texture_id;
        texture = nil; // Explicitly release
    }
}

// ============================================================================
// Custom Vertex/Index Buffer Management API - Metal (Stubs)
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
// Custom Shader System API - Metal Implementation
// ============================================================================

// Shader data structures
struct ImPlatform_ShaderData_Metal
{
    void* mtlFunction; // id<MTLFunction> (bridged)
    ImPlatform_ShaderStage stage;
};

struct ImPlatform_ShaderProgramData_Metal
{
    void* mtlVertexFunction;   // id<MTLFunction>
    void* mtlFragmentFunction; // id<MTLFunction>
    void* mtlRenderPipelineState; // id<MTLRenderPipelineState>
    void* mtlUniformBuffer; // id<MTLBuffer>
    void* uniformData;
    size_t uniformDataSize;
    bool uniformDataDirty;
};

// Current draw data for custom shader rendering (needed for multi-viewport)
static ImDrawData* g_CurrentDrawData = nullptr;

IMPLATFORM_API ImPlatform_Shader ImPlatform_CreateShader(const ImPlatform_ShaderDesc* desc)
{
    if (!desc || !desc->source_code || !g_GfxData.pMetalDevice)
        return NULL;

    @autoreleasepool {
        id<MTLDevice> device = (__bridge id<MTLDevice>)g_GfxData.pMetalDevice;

        // Compile Metal shader from MSL source
        NSError* error = nil;
        NSString* sourceString = [NSString stringWithUTF8String:desc->source_code];

        MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
        options.languageVersion = MTLLanguageVersion2_0;

        id<MTLLibrary> library = [device newLibraryWithSource:sourceString options:options error:&error];
        if (!library)
        {
            if (error)
            {
                NSLog(@"Metal shader compilation failed: %@", error.localizedDescription);
            }
            return NULL;
        }

        // Get the entry point function
        NSString* entryPoint = desc->entry_point ? [NSString stringWithUTF8String:desc->entry_point] : @"main0";
        id<MTLFunction> function = [library newFunctionWithName:entryPoint];
        if (!function)
        {
            NSLog(@"Metal shader entry point '%@' not found", entryPoint);
            return NULL;
        }

        // Create shader data
        ImPlatform_ShaderData_Metal* shader_data = new ImPlatform_ShaderData_Metal();
        shader_data->mtlFunction = (__bridge_retained void*)function;
        shader_data->stage = desc->stage;

        return shader_data;
    }
}

IMPLATFORM_API void ImPlatform_DestroyShader(ImPlatform_Shader shader)
{
    if (!shader)
        return;

    @autoreleasepool {
        ImPlatform_ShaderData_Metal* shader_data = (ImPlatform_ShaderData_Metal*)shader;

        if (shader_data->mtlFunction)
        {
            CFRelease(shader_data->mtlFunction);
            shader_data->mtlFunction = nullptr;
        }

        delete shader_data;
    }
}

IMPLATFORM_API ImPlatform_ShaderProgram ImPlatform_CreateShaderProgram(ImPlatform_Shader vertex_shader, ImPlatform_Shader fragment_shader)
{
    if (!vertex_shader || !fragment_shader || !g_GfxData.pMetalDevice)
        return NULL;

    @autoreleasepool {
        ImPlatform_ShaderData_Metal* vs_data = (ImPlatform_ShaderData_Metal*)vertex_shader;
        ImPlatform_ShaderData_Metal* fs_data = (ImPlatform_ShaderData_Metal*)fragment_shader;

        id<MTLDevice> device = (__bridge id<MTLDevice>)g_GfxData.pMetalDevice;
        id<MTLFunction> vertexFunction = (__bridge id<MTLFunction>)vs_data->mtlFunction;
        id<MTLFunction> fragmentFunction = (__bridge id<MTLFunction>)fs_data->mtlFunction;

        // Create vertex descriptor matching ImDrawVert
        MTLVertexDescriptor* vertexDescriptor = [[MTLVertexDescriptor alloc] init];

        // Position (float2)
        vertexDescriptor.attributes[0].format = MTLVertexFormatFloat2;
        vertexDescriptor.attributes[0].offset = 0;
        vertexDescriptor.attributes[0].bufferIndex = 0;

        // UV (float2)
        vertexDescriptor.attributes[1].format = MTLVertexFormatFloat2;
        vertexDescriptor.attributes[1].offset = 8;
        vertexDescriptor.attributes[1].bufferIndex = 0;

        // Color (uchar4 normalized)
        vertexDescriptor.attributes[2].format = MTLVertexFormatUChar4Normalized;
        vertexDescriptor.attributes[2].offset = 16;
        vertexDescriptor.attributes[2].bufferIndex = 0;

        // Layout
        vertexDescriptor.layouts[0].stride = sizeof(ImDrawVert);
        vertexDescriptor.layouts[0].stepRate = 1;
        vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

        // Create render pipeline descriptor
        MTLRenderPipelineDescriptor* pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineDescriptor.vertexFunction = vertexFunction;
        pipelineDescriptor.fragmentFunction = fragmentFunction;
        pipelineDescriptor.vertexDescriptor = vertexDescriptor;
        pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
        pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
        pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

        // Create pipeline state
        NSError* error = nil;
        id<MTLRenderPipelineState> pipelineState = [device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
        if (!pipelineState)
        {
            if (error)
            {
                NSLog(@"Metal render pipeline creation failed: %@", error.localizedDescription);
            }
            return NULL;
        }

        // Create program data
        ImPlatform_ShaderProgramData_Metal* program_data = new ImPlatform_ShaderProgramData_Metal();
        program_data->mtlVertexFunction = (__bridge_retained void*)vertexFunction;
        program_data->mtlFragmentFunction = (__bridge_retained void*)fragmentFunction;
        program_data->mtlRenderPipelineState = (__bridge_retained void*)pipelineState;
        program_data->mtlUniformBuffer = nullptr;
        program_data->uniformData = nullptr;
        program_data->uniformDataSize = 0;
        program_data->uniformDataDirty = false;

        return program_data;
    }
}

IMPLATFORM_API void ImPlatform_DestroyShaderProgram(ImPlatform_ShaderProgram program)
{
    if (!program)
        return;

    @autoreleasepool {
        ImPlatform_ShaderProgramData_Metal* program_data = (ImPlatform_ShaderProgramData_Metal*)program;

        if (program_data->mtlUniformBuffer)
        {
            CFRelease(program_data->mtlUniformBuffer);
            program_data->mtlUniformBuffer = nullptr;
        }

        if (program_data->mtlRenderPipelineState)
        {
            CFRelease(program_data->mtlRenderPipelineState);
            program_data->mtlRenderPipelineState = nullptr;
        }

        if (program_data->mtlFragmentFunction)
        {
            CFRelease(program_data->mtlFragmentFunction);
            program_data->mtlFragmentFunction = nullptr;
        }

        if (program_data->mtlVertexFunction)
        {
            CFRelease(program_data->mtlVertexFunction);
            program_data->mtlVertexFunction = nullptr;
        }

        if (program_data->uniformData)
        {
            free(program_data->uniformData);
            program_data->uniformData = nullptr;
        }

        delete program_data;
    }
}

IMPLATFORM_API void ImPlatform_UseShaderProgram(ImPlatform_ShaderProgram program)
{
    // Metal doesn't have a global "use program" concept
    // Shaders are bound per render pass via callbacks
}

IMPLATFORM_API bool ImPlatform_SetShaderUniform(ImPlatform_ShaderProgram program, const char* name, const void* data, unsigned int size)
{
    if (!program || !data || size == 0)
        return false;

    @autoreleasepool {
        ImPlatform_ShaderProgramData_Metal* program_data = (ImPlatform_ShaderProgramData_Metal*)program;

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
}

IMPLATFORM_API bool ImPlatform_SetShaderTexture(ImPlatform_ShaderProgram program, const char* name, unsigned int slot, ImTextureID texture)
{
    // Metal texture binding happens in the render callback
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

    @autoreleasepool {
        if (g_UniformBlockData && g_UniformBlockSize > 0)
        {
            ImPlatform_ShaderProgramData_Metal* program_data = (ImPlatform_ShaderProgramData_Metal*)program;
            id<MTLDevice> device = (__bridge id<MTLDevice>)g_GfxData.pMetalDevice;

            // Create or update uniform buffer
            if (!program_data->mtlUniformBuffer || program_data->uniformDataSize != g_UniformBlockSize)
            {
                if (program_data->mtlUniformBuffer)
                {
                    CFRelease(program_data->mtlUniformBuffer);
                    program_data->mtlUniformBuffer = nullptr;
                }

                id<MTLBuffer> uniformBuffer = [device newBufferWithLength:g_UniformBlockSize options:MTLResourceStorageModeShared];
                if (uniformBuffer)
                {
                    program_data->mtlUniformBuffer = (__bridge_retained void*)uniformBuffer;
                }
            }

            // Upload the accumulated uniform block to the buffer
            if (program_data->mtlUniformBuffer)
            {
                id<MTLBuffer> uniformBuffer = (__bridge id<MTLBuffer>)program_data->mtlUniformBuffer;
                memcpy([uniformBuffer contents], g_UniformBlockData, g_UniformBlockSize);

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
}

// ============================================================================
// Custom Shader DrawList Integration
// ============================================================================

// Wrapper for ImGui_ImplMetal_RenderDrawData that stores draw_data for custom shader callbacks
static void ImPlatform_RenderDrawDataWrapper(ImDrawData* draw_data, id<MTLCommandBuffer> commandBuffer, id<MTLRenderCommandEncoder> renderEncoder)
{
    // Store draw data for custom shader callbacks (needed for multi-viewport)
    g_CurrentDrawData = draw_data;

    ImGui_ImplMetal_RenderDrawData(draw_data, commandBuffer, renderEncoder);

    // NOTE: Don't clear g_CurrentDrawData here - it will be updated by the next render call
}

// ImDrawCallback handler to activate a custom shader
static void ImPlatform_SetCustomShader(const ImDrawList* parent_list, const ImDrawCmd* cmd)
{
    @autoreleasepool {
        ImPlatform_ShaderProgram program = (ImPlatform_ShaderProgram)cmd->UserCallbackData;
        if (!program)
            return;

        ImPlatform_ShaderProgramData_Metal* program_data = (ImPlatform_ShaderProgramData_Metal*)program;

        // Get the render encoder from ImGui's Metal backend
        // Note: This requires access to ImGui's internal Metal state
        // For now, we'll use a simplified approach that assumes we're in the middle of a render pass

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

        // Note: Metal backend binding logic would go here
        // This is a simplified implementation that demonstrates the pattern
        // In a full implementation, you would:
        // 1. Get the current render encoder from ImGui's Metal render state
        // 2. Bind the custom pipeline state
        // 3. Set the projection matrix as vertex shader uniforms
        // 4. Set the custom fragment shader uniforms from program_data->uniformData

        // Since we don't have direct access to the render encoder here without modifying ImGui's Metal backend,
        // this serves as a reference implementation showing the required logic
    }
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

#endif // IM_GFX_METAL
