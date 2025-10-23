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

        ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), commandBuffer, renderEncoder);

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

// Custom Shader System API - Metal (Stubs)
IMPLATFORM_API ImPlatform_Shader ImPlatform_CreateShader(const ImPlatform_ShaderDesc* desc) { return NULL; }
IMPLATFORM_API void ImPlatform_DestroyShader(ImPlatform_Shader shader) {}
IMPLATFORM_API ImPlatform_ShaderProgram ImPlatform_CreateShaderProgram(ImPlatform_Shader vertex_shader, ImPlatform_Shader fragment_shader) { return NULL; }
IMPLATFORM_API void ImPlatform_DestroyShaderProgram(ImPlatform_ShaderProgram program) {}
IMPLATFORM_API void ImPlatform_UseShaderProgram(ImPlatform_ShaderProgram program) {}
IMPLATFORM_API bool ImPlatform_SetShaderUniform(ImPlatform_ShaderProgram program, const char* name, const void* data, unsigned int size) { return false; }
IMPLATFORM_API bool ImPlatform_SetShaderTexture(ImPlatform_ShaderProgram program, const char* name, unsigned int slot, ImTextureID texture) { return false; }

// ============================================================================
// Custom Shader DrawList Integration
// ============================================================================

// ImDrawCallback handler to activate a custom shader
static void ImPlatform_SetCustomShader(const ImDrawList* parent_list, const ImDrawCmd* cmd)
{
    // Metal custom shaders stub - needs render pipeline setup
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

#endif // IM_GFX_METAL
