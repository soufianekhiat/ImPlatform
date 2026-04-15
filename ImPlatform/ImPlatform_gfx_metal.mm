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
static ImPlatform_GfxData_Metal g_GfxData = {};
unsigned int g_ImPlatform_BackbufferW = 0;
unsigned int g_ImPlatform_BackbufferH = 0;

// Global MTLBinaryArchive (macOS 11 / iOS 14+). Caches compiled render pipeline
// state objects across runs. Loaded from disk at device-create time, attached
// to every MTLRenderPipelineDescriptor, populated on successful pipeline
// creation, and serialized back on cleanup. nullptr on older OSes.
static void* g_MetalBinaryArchive = nullptr; // id<MTLBinaryArchive>

// Saved state for Begin/EndRenderToTexture
static id<MTLCommandBuffer>        g_RTCommandBuffer  = nil;
static id<MTLRenderCommandEncoder> g_RTRenderEncoder  = nil;

// Forward declaration for wrapper function
static void ImPlatform_RenderDrawDataWrapper(ImDrawData* draw_data, id<MTLCommandBuffer> commandBuffer, id<MTLRenderCommandEncoder> renderEncoder);

// Uniform block API state
static ImPlatform_ShaderProgram g_CurrentUniformBlockProgram = nullptr;
static void* g_UniformBlockData = nullptr;
static size_t g_UniformBlockSize = 0;

// Internal API - Get Metal gfx data
ImPlatform_GfxData_Metal* ImPlatform_Gfx_GetData_Metal(void)
{
    return &g_GfxData;
}

// ----------------------------------------------------------------------------
// MTLBinaryArchive disk persistence helpers (macOS 11 / iOS 14+)
// ----------------------------------------------------------------------------
// MTLBinaryArchive is Apple's equivalent of VkPipelineCache: it stores
// compiled pipeline state objects in an opaque .metallib blob that Metal
// knows how to consume. We load it once on device create, attach it to every
// MTLRenderPipelineDescriptor so Metal can look up cached pipelines, and
// serialize it back to disk on cleanup. The ShaderCacheLoad helper is NOT
// used to read the bytes (MTLBinaryArchive wants a URL directly); we only
// use ImPlatform_ShaderCacheBuildPath for the directory layout and the
// stdio-level save/serialize path is driven by Metal's serializeToURL:.

static void ImPlatform_Metal_BuildArchivePath(char* out_path, size_t out_size)
{
    // cache_key="metal_global", entry="archive", ext="metallib", hash=0
    ImPlatform_ShaderCacheBuildPath(out_path, out_size,
                                    "metal_global", "archive", "metallib", 0);
}

static bool ImPlatform_Metal_FileExists(const char* path)
{
    if (!path || !*path) return false;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

static void ImPlatform_Metal_InitBinaryArchive(void)
{
    if (g_MetalBinaryArchive != nullptr)
        return;
    if (!g_GfxData.pMetalDevice)
        return;

    if (@available(macOS 11.0, iOS 14.0, *))
    {
        @autoreleasepool {
            id<MTLDevice> device = (__bridge id<MTLDevice>)g_GfxData.pMetalDevice;

            char cache_path[512];
            ImPlatform_Metal_BuildArchivePath(cache_path, sizeof(cache_path));

            MTLBinaryArchiveDescriptor* desc = [[MTLBinaryArchiveDescriptor alloc] init];

            if (ImPlatform_Metal_FileExists(cache_path))
            {
                NSString* nsPath = [NSString stringWithUTF8String:cache_path];
                desc.url = [NSURL fileURLWithPath:nsPath];
            }
            else
            {
                desc.url = nil;
            }

            NSError* error = nil;
            id<MTLBinaryArchive> archive = [device newBinaryArchiveWithDescriptor:desc error:&error];
            if (!archive && desc.url != nil)
            {
                // Driver rejected the on-disk archive (e.g. OS upgrade changed
                // the format). Retry with a fresh empty archive.
                fprintf(stderr, "[ImPlatform shader cache] MTL archive: rejected on-disk data (%s), starting fresh\n",
                        error ? [[error localizedDescription] UTF8String] : "unknown error");
                desc.url = nil;
                error = nil;
                archive = [device newBinaryArchiveWithDescriptor:desc error:&error];
            }

            if (archive)
            {
                g_MetalBinaryArchive = (__bridge_retained void*)archive;
                if (desc.url != nil)
                {
                    // Determine size by stat via fseek for the log line.
                    FILE* f = fopen(cache_path, "rb");
                    size_t sz = 0;
                    if (f) { fseek(f, 0, SEEK_END); long s = ftell(f); if (s > 0) sz = (size_t)s; fclose(f); }
                    fprintf(stderr, "[ImPlatform shader cache] MTL archive: %zu bytes loaded from disk\n", sz);
                }
                else
                {
                    fprintf(stderr, "[ImPlatform shader cache] MTL archive: initialized empty (no cache file)\n");
                }
            }
            else
            {
                fprintf(stderr, "[ImPlatform shader cache] MTL archive: newBinaryArchiveWithDescriptor failed (%s)\n",
                        error ? [[error localizedDescription] UTF8String] : "unknown error");
                g_MetalBinaryArchive = nullptr;
            }
        }
    }
    else
    {
        fprintf(stderr, "[ImPlatform shader cache] MTL archive: unavailable (requires macOS 11 / iOS 14)\n");
    }
}

static void ImPlatform_Metal_SaveBinaryArchive(void)
{
    if (g_MetalBinaryArchive == nullptr)
        return;

    if (@available(macOS 11.0, iOS 14.0, *))
    {
        @autoreleasepool {
            id<MTLBinaryArchive> archive = (__bridge id<MTLBinaryArchive>)g_MetalBinaryArchive;

            char cache_path[512];
            ImPlatform_Metal_BuildArchivePath(cache_path, sizeof(cache_path));

            NSString* nsPath = [NSString stringWithUTF8String:cache_path];
            NSURL* url = [NSURL fileURLWithPath:nsPath];

            NSError* error = nil;
            BOOL ok = [archive serializeToURL:url error:&error];
            if (ok)
            {
                FILE* f = fopen(cache_path, "rb");
                size_t sz = 0;
                if (f) { fseek(f, 0, SEEK_END); long s = ftell(f); if (s > 0) sz = (size_t)s; fclose(f); }
                fprintf(stderr, "[ImPlatform shader cache] MTL archive: saved %zu bytes to disk\n", sz);
            }
            else
            {
                fprintf(stderr, "[ImPlatform shader cache] MTL archive: serializeToURL failed (%s)\n",
                        error ? [[error localizedDescription] UTF8String] : "unknown error");
            }
        }
    }
}

static void ImPlatform_Metal_DestroyBinaryArchive(void)
{
    if (g_MetalBinaryArchive == nullptr)
        return;
    CFRelease(g_MetalBinaryArchive);
    g_MetalBinaryArchive = nullptr;
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

        // Load disk-backed MTLBinaryArchive so subsequent pipeline state
        // creations can reuse compiled pipelines from previous runs.
        ImPlatform_Metal_InitBinaryArchive();

        return pData->pMetalLayer;
    }
}

// Internal API - Cleanup Metal device
void ImPlatform_Gfx_CleanupDevice_Metal(ImPlatform_GfxData_Metal* pData)
{
    @autoreleasepool {
        // Serialize and tear down the binary archive before the device goes away.
        ImPlatform_Metal_SaveBinaryArchive();
        ImPlatform_Metal_DestroyBinaryArchive();

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

        // Create 6 sampler states for all filter/wrap combinations
        static const MTLSamplerMinMagFilter kMinMag[2] = { MTLSamplerMinMagFilterNearest, MTLSamplerMinMagFilterLinear };
        static const MTLSamplerAddressMode  kAddr[3]   = { MTLSamplerAddressModeClampToEdge, MTLSamplerAddressModeRepeat, MTLSamplerAddressModeMirrorRepeat };
        for (int f = 0; f < 2; ++f)
        for (int w = 0; w < 3; ++w)
        {
            MTLSamplerDescriptor* desc = [[MTLSamplerDescriptor alloc] init];
            desc.minFilter  = kMinMag[f];
            desc.magFilter  = kMinMag[f];
            desc.mipFilter  = MTLSamplerMipFilterNotMipmapped;
            desc.sAddressMode = kAddr[w];
            desc.tAddressMode = kAddr[w];
            id<MTLSamplerState> s = [device newSamplerStateWithDescriptor:desc];
            g_MetalSamplers[f][w] = (__bridge_retained void*)s;
        }

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
    for (int f = 0; f < 2; ++f)
    for (int w = 0; w < 3; ++w)
        if (g_MetalSamplers[f][w]) { CFRelease(g_MetalSamplers[f][w]); g_MetalSamplers[f][w] = nullptr; }

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
#if IMPLATFORM_GFX_SUPPORT_BGRA_FORMATS
    case ImPlatform_PixelFormat_BGRA8:
        *out_bytes_per_pixel = 4;
        return MTLPixelFormatBGRA8Unorm;
#endif
#if IMPLATFORM_GFX_SUPPORT_HALF_FLOAT_FORMATS
    case ImPlatform_PixelFormat_R16F:
        *out_bytes_per_pixel = 2;
        return MTLPixelFormatR16Float;
    case ImPlatform_PixelFormat_RG16F:
        *out_bytes_per_pixel = 4;
        return MTLPixelFormatRG16Float;
    case ImPlatform_PixelFormat_RGBA16F:
        *out_bytes_per_pixel = 8;
        return MTLPixelFormatRGBA16Float;
#endif
    // No IMPLATFORM_GFX_SUPPORT_RGB_EXTENDED — Metal has no 3-channel texture formats
#if IMPLATFORM_GFX_SUPPORT_SRGB_FORMATS
    case ImPlatform_PixelFormat_RGB8_SRGB:
        // Metal has no 3-channel sRGB — use RGBA8 sRGB
        *out_bytes_per_pixel = 4;
        return MTLPixelFormatRGBA8Unorm_sRGB;
    case ImPlatform_PixelFormat_RGBA8_SRGB:
        *out_bytes_per_pixel = 4;
        return MTLPixelFormatRGBA8Unorm_sRGB;
#endif
#if IMPLATFORM_GFX_SUPPORT_PACKED_FORMATS
    case ImPlatform_PixelFormat_RGB10A2:
        *out_bytes_per_pixel = 4;
        return MTLPixelFormatRGB10A2Unorm;
#endif
#if IMPLATFORM_GFX_SUPPORT_DEPTH_FORMATS
    case ImPlatform_PixelFormat_D16:
        *out_bytes_per_pixel = 2;
        return MTLPixelFormatDepth16Unorm;
    case ImPlatform_PixelFormat_D32F:
        *out_bytes_per_pixel = 4;
        return MTLPixelFormatDepth32Float;
    case ImPlatform_PixelFormat_D24S8:
        // MTLPixelFormatDepth24Unorm_Stencil8 is macOS-only and unsupported on Apple Silicon
        *out_bytes_per_pixel = 4;
        return MTLPixelFormatDepth24Unorm_Stencil8;
    case ImPlatform_PixelFormat_D32FS8:
        *out_bytes_per_pixel = 8;
        return MTLPixelFormatDepth32Float_Stencil8;
#endif
#if IMPLATFORM_GFX_SUPPORT_INTEGER_FORMATS
    case ImPlatform_PixelFormat_R8UI:
        *out_bytes_per_pixel = 1;
        return MTLPixelFormatR8Uint;
    case ImPlatform_PixelFormat_R8I:
        *out_bytes_per_pixel = 1;
        return MTLPixelFormatR8Sint;
    case ImPlatform_PixelFormat_R16UI:
        *out_bytes_per_pixel = 2;
        return MTLPixelFormatR16Uint;
    case ImPlatform_PixelFormat_R16I:
        *out_bytes_per_pixel = 2;
        return MTLPixelFormatR16Sint;
    case ImPlatform_PixelFormat_R32UI:
        *out_bytes_per_pixel = 4;
        return MTLPixelFormatR32Uint;
    case ImPlatform_PixelFormat_R32I:
        *out_bytes_per_pixel = 4;
        return MTLPixelFormatR32Sint;
#endif
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

// Texture3D stub for Metal.
IMPLATFORM_API ImPlatform_TextureDesc3D ImPlatform_TextureDesc3D_Default(unsigned int w, unsigned int h, unsigned int d)
{
    ImPlatform_TextureDesc3D x; x.width = w; x.height = h; x.depth = d;
    x.format = ImPlatform_PixelFormat_RGBA8;
    x.min_filter = ImPlatform_TextureFilter_Linear; x.mag_filter = ImPlatform_TextureFilter_Linear;
    x.wrap_u = x.wrap_v = x.wrap_w = ImPlatform_TextureWrap_Clamp;
    return x;
}
IMPLATFORM_API bool ImPlatform_SupportsTexture3D(void) { return false; }
IMPLATFORM_API ImTextureID ImPlatform_CreateTexture3D(const void*, const ImPlatform_TextureDesc3D*) { return (ImTextureID)0; }

IMPLATFORM_API ImTextureID ImPlatform_CreateTexture(const void* pixel_data, const ImPlatform_TextureDesc* desc)
{
    if (!desc || !pixel_data || !g_GfxData.pMetalDevice)
        return (ImTextureID)0;

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
        id<MTLTexture> texture = [(__bridge id<MTLDevice>)g_GfxData.pMetalDevice newTextureWithDescriptor:textureDescriptor];
        if (!texture)
            return (ImTextureID)0;

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
        return (ImTextureID)(__bridge_retained void*)texture;
    }
}

IMPLATFORM_API bool ImPlatform_UpdateTexture(ImTextureID texture_id, const void* pixel_data,
                                              unsigned int x, unsigned int y,
                                              unsigned int width, unsigned int height)
{
    if (!texture_id || !pixel_data)
        return false;

    @autoreleasepool {
        // Cast ImTextureID (unsigned long long) -> void* -> id<MTLTexture>
        id<MTLTexture> texture = (__bridge id<MTLTexture>)(void*)(uintptr_t)texture_id;

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
            case MTLPixelFormatBGRA8Unorm: bytes_per_pixel = 4; break;
            case MTLPixelFormatR16Float: bytes_per_pixel = 2; break;
            case MTLPixelFormatRG16Float: bytes_per_pixel = 4; break;
            case MTLPixelFormatRGBA16Float: bytes_per_pixel = 8; break;
            case MTLPixelFormatRGBA8Unorm_sRGB: bytes_per_pixel = 4; break;
            case MTLPixelFormatRGB10A2Unorm: bytes_per_pixel = 4; break;
            case MTLPixelFormatDepth16Unorm: bytes_per_pixel = 2; break;
            case MTLPixelFormatDepth32Float: bytes_per_pixel = 4; break;
            case MTLPixelFormatDepth24Unorm_Stencil8: bytes_per_pixel = 4; break;
            case MTLPixelFormatDepth32Float_Stencil8: bytes_per_pixel = 8; break;
            case MTLPixelFormatR8Uint: bytes_per_pixel = 1; break;
            case MTLPixelFormatR8Sint: bytes_per_pixel = 1; break;
            case MTLPixelFormatR16Uint: bytes_per_pixel = 2; break;
            case MTLPixelFormatR16Sint: bytes_per_pixel = 2; break;
            case MTLPixelFormatR32Uint: bytes_per_pixel = 4; break;
            case MTLPixelFormatR32Sint: bytes_per_pixel = 4; break;
            default: break;
        }

        // Update the sub-region
        NSUInteger bytesPerRow = width * bytes_per_pixel;
        MTLRegion region = MTLRegionMake2D(x, y, width, height);
        [texture replaceRegion:region mipmapLevel:0 withBytes:pixel_data bytesPerRow:bytesPerRow];

        return true;
    }
}

IMPLATFORM_API ImTextureID ImPlatform_CreateRenderTexture(const ImPlatform_TextureDesc* desc)
{
    if (!desc || !g_GfxData.pMetalDevice)
        return (ImTextureID)0;

    @autoreleasepool {
        int bytes_per_pixel;
        MTLPixelFormat format = ImPlatform_GetMetalFormat(desc->format, &bytes_per_pixel);

        MTLTextureDescriptor* textureDescriptor = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:format
                                         width:desc->width
                                        height:desc->height
                                     mipmapped:NO];
        textureDescriptor.usage       = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
        textureDescriptor.storageMode = MTLStorageModePrivate;

        id<MTLTexture> texture = [(__bridge id<MTLDevice>)g_GfxData.pMetalDevice
            newTextureWithDescriptor:textureDescriptor];
        if (!texture)
            return (ImTextureID)0;

        return (ImTextureID)(__bridge_retained void*)texture;
    }
}

IMPLATFORM_API bool ImPlatform_BeginRenderToTexture(ImTextureID texture)
{
    if (!texture || !g_GfxData.pCommandQueue)
        return false;

    @autoreleasepool {
        id<MTLTexture> mtlTexture = (__bridge id<MTLTexture>)(void*)(uintptr_t)texture;

        id<MTLCommandQueue> commandQueue = (__bridge id<MTLCommandQueue>)g_GfxData.pCommandQueue;
        g_RTCommandBuffer = [commandQueue commandBuffer];

        MTLRenderPassDescriptor* passDesc = [MTLRenderPassDescriptor renderPassDescriptor];
        passDesc.colorAttachments[0].texture     = mtlTexture;
        passDesc.colorAttachments[0].loadAction  = MTLLoadActionClear;
        passDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
        passDesc.colorAttachments[0].clearColor  = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);

        g_RTRenderEncoder = [g_RTCommandBuffer renderCommandEncoderWithDescriptor:passDesc];
        if (!g_RTRenderEncoder)
        {
            g_RTCommandBuffer = nil;
            return false;
        }

        return true;
    }
}

IMPLATFORM_API void ImPlatform_EndRenderToTexture(void)
{
    if (!g_RTRenderEncoder) return;

    @autoreleasepool {
        [g_RTRenderEncoder endEncoding];
        [g_RTCommandBuffer commit];
        g_RTRenderEncoder = nil;
        g_RTCommandBuffer = nil;
    }
}

IMPLATFORM_API bool ImPlatform_CopyBackbuffer(ImTextureID dst) { (void)dst; return false; }
IMPLATFORM_API void ImPlatform_GetBackbufferSize(unsigned int* width, unsigned int* height)
{
    if (width)  *width  = g_ImPlatform_BackbufferW;
    if (height) *height = g_ImPlatform_BackbufferH;
}

IMPLATFORM_API bool ImPlatform_CopyTexture(ImTextureID dst, ImTextureID src)
{
    if (!dst || !src || !g_GfxData.pCommandQueue)
        return false;

    @autoreleasepool {
        id<MTLTexture> srcTex = (__bridge id<MTLTexture>)(void*)(uintptr_t)src;
        id<MTLTexture> dstTex = (__bridge id<MTLTexture>)(void*)(uintptr_t)dst;
        id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)g_GfxData.pCommandQueue;

        id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
        if (!cmdBuf)
            return false;

        id<MTLBlitCommandEncoder> blit = [cmdBuf blitCommandEncoder];
        if (!blit)
            return false;

        [blit copyFromTexture:srcTex
                  sourceSlice:0
                  sourceLevel:0
                 sourceOrigin:MTLOriginMake(0, 0, 0)
                   sourceSize:MTLSizeMake(srcTex.width, srcTex.height, 1)
                    toTexture:dstTex
             destinationSlice:0
             destinationLevel:0
            destinationOrigin:MTLOriginMake(0, 0, 0)];

        [blit endEncoding];
        [cmdBuf commit];
        [cmdBuf waitUntilCompleted];
        return true;
    }
}

IMPLATFORM_API void ImPlatform_DestroyTexture(ImTextureID texture_id)
{
    if (!texture_id)
        return;

    @autoreleasepool {
        // Release the bridged texture
        // Cast ImTextureID (unsigned long long) -> void* -> id<MTLTexture>
        id<MTLTexture> texture = (__bridge_transfer id<MTLTexture>)(void*)(uintptr_t)texture_id;
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

// Current render encoder for custom shader rendering (set during render pass)
static void* g_CurrentRenderEncoder = nullptr; // id<MTLRenderCommandEncoder>

// Sampler override state - [filter][wrap]: filter 0=Nearest 1=Linear, wrap 0=Clamp 1=Wrap 2=Mirror
static void* g_MetalSamplers[2][3]  = {};  // id<MTLSamplerState>
static void* g_SamplerStack[8]      = {};  // id<MTLSamplerState>
static int   g_SamplerDepth         = 0;

// Caching strategy:
//   Metal compiles MSL source to an MTLLibrary at ImPlatform_CreateShader
//   time, then builds an MTLRenderPipelineState from (vertex, fragment,
//   vertex descriptor, color attachment formats, blend state) at
//   ImPlatform_CreateShaderProgram time. The expensive work is the pipeline
//   build, not the library compile, so the per-shader cache_key / compile_flags
//   fields are intentionally ignored at the MTLLibrary level.
//
//   Pipeline-level caching IS active via a global MTLBinaryArchive (macOS 11 /
//   iOS 14+) that is loaded from
//       ./shaders/bytecode_cache/metal_global_archive_0000000000000000.metallib
//   at device-create time and serialized back on device cleanup. Every
//   MTLRenderPipelineDescriptor built in this file gets `binaryArchives`
//   set to [archive] so Metal will transparently look up and reuse cached
//   pipelines; on a miss we populate the archive via
//   addRenderPipelineFunctionsWithDescriptor: so next launch sees the hit.
//   See ImPlatform_Metal_InitBinaryArchive / _SaveBinaryArchive above.
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

        // Attach the global binary archive (macOS 11 / iOS 14+) so Metal can
        // look up a cached compiled pipeline instead of compiling from scratch.
        if (@available(macOS 11.0, iOS 14.0, *))
        {
            if (g_MetalBinaryArchive != nullptr)
            {
                id<MTLBinaryArchive> archive = (__bridge id<MTLBinaryArchive>)g_MetalBinaryArchive;
                pipelineDescriptor.binaryArchives = @[ archive ];
            }
        }

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

        // On a cache miss, add the freshly-compiled pipeline to the archive so
        // it will be reused on the next launch. addRenderPipelineFunctions-
        // WithDescriptor: is a no-op if the pipeline is already present.
        if (@available(macOS 11.0, iOS 14.0, *))
        {
            if (g_MetalBinaryArchive != nullptr)
            {
                id<MTLBinaryArchive> archive = (__bridge id<MTLBinaryArchive>)g_MetalBinaryArchive;
                NSError* archiveError = nil;
                BOOL ok = [archive addRenderPipelineFunctionsWithDescriptor:pipelineDescriptor error:&archiveError];
                if (!ok && archiveError)
                {
                    // Non-fatal: pipeline exists, archive just couldn't store it.
                    fprintf(stderr, "[ImPlatform shader cache] MTL archive: addRenderPipelineFunctions failed (%s)\n",
                            [[archiveError localizedDescription] UTF8String]);
                }
            }
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

IMPLATFORM_API bool ImPlatform_SetShaderUniform(ImPlatform_ShaderProgram program, const char* /*name*/, const void* data, unsigned int size)
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

IMPLATFORM_API bool ImPlatform_SetShaderTexture(ImPlatform_ShaderProgram /*program*/, const char* /*name*/, unsigned int /*slot*/, ImTextureID /*texture*/)
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

IMPLATFORM_API bool ImPlatform_SetUniform(const char* /*name*/, const void* data, unsigned int size)
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
    // Store draw data and render encoder for custom shader callbacks (needed for multi-viewport)
    g_CurrentDrawData = draw_data;
    g_CurrentRenderEncoder = (__bridge void*)renderEncoder;

    ImGui_ImplMetal_RenderDrawData(draw_data, commandBuffer, renderEncoder);

    g_CurrentRenderEncoder = nullptr;
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

// Activate a custom shader immediately (for use inside draw callbacks).
IMPLATFORM_API void ImPlatform_BeginCustomShader_Render(ImPlatform_ShaderProgram program)
{
    @autoreleasepool {
        if (!program || !g_CurrentRenderEncoder)
            return;

        ImPlatform_ShaderProgramData_Metal* program_data = (ImPlatform_ShaderProgramData_Metal*)program;
        id<MTLRenderCommandEncoder> renderEncoder = (__bridge id<MTLRenderCommandEncoder>)g_CurrentRenderEncoder;

        // Find draw data for projection matrix calculation
        ImDrawData* draw_data = g_CurrentDrawData;
        if (!draw_data)
            draw_data = ImGui::GetDrawData();
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

        // Bind custom pipeline state
        id<MTLRenderPipelineState> pipelineState = (__bridge id<MTLRenderPipelineState>)program_data->mtlRenderPipelineState;
        if (pipelineState)
            [renderEncoder setRenderPipelineState:pipelineState];

        // Set projection matrix as vertex shader uniforms (buffer index 1, matching ImGui's Metal backend)
        [renderEncoder setVertexBytes:&ortho_projection length:sizeof(ortho_projection) atIndex:1];

        // Set custom fragment shader uniforms
        if (program_data->uniformData && program_data->uniformDataSize > 0)
        {
            id<MTLBuffer> uniformBuffer = (__bridge id<MTLBuffer>)program_data->mtlUniformBuffer;
            if (uniformBuffer && program_data->uniformDataDirty)
            {
                memcpy([uniformBuffer contents], program_data->uniformData, program_data->uniformDataSize);
                program_data->uniformDataDirty = false;
            }

            if (uniformBuffer)
                [renderEncoder setFragmentBuffer:uniformBuffer offset:0 atIndex:1];
        }
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

IMPLATFORM_API void* ImPlatform_PushShaderConstants(const void* data, unsigned int size)
{
    (void)data; (void)size;
    return nullptr;
}

IMPLATFORM_API void ImPlatform_PopShaderConstants(void* handle)
{
    (void)handle;
}

// ============================================================================
// Sampler Override API - Metal
// ============================================================================

IMPLATFORM_API void ImPlatform_PushSampler(ImPlatform_TextureFilter filter, ImPlatform_TextureWrap wrap)
{
    uintptr_t encoded = (uintptr_t)(((int)filter << 8) | (int)wrap);
    ImGui::GetWindowDrawList()->AddCallback(
        [](const ImDrawList*, const ImDrawCmd* cmd) {
            @autoreleasepool {
                id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)g_CurrentRenderEncoder;
                if (!encoder) return;
                // Save current sampler (Metal has no query API — store nil to represent "default")
                if (g_SamplerDepth < 8)
                    g_SamplerStack[g_SamplerDepth++] = nullptr;
                int enc = (int)(uintptr_t)cmd->UserCallbackData;
                int f = (enc >> 8) & 0xFF;
                int w = enc & 0xFF;
                if (f < 2 && w < 3 && g_MetalSamplers[f][w])
                {
                    id<MTLSamplerState> s = (__bridge id<MTLSamplerState>)g_MetalSamplers[f][w];
                    [encoder setFragmentSamplerState:s atIndex:0];
                }
            }
        }, (void*)encoded);
}

IMPLATFORM_API void ImPlatform_PopSampler(void)
{
    ImGui::GetWindowDrawList()->AddCallback(
        [](const ImDrawList*, const ImDrawCmd*) {
            @autoreleasepool {
                id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)g_CurrentRenderEncoder;
                if (!encoder || g_SamplerDepth <= 0) return;
                void* saved = g_SamplerStack[--g_SamplerDepth];
                id<MTLSamplerState> s = (__bridge id<MTLSamplerState>)saved;
                [encoder setFragmentSamplerState:s atIndex:0];
            }
        }, nullptr);
}

#endif // IM_GFX_METAL
