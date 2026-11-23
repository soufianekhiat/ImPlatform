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

// Sampler override state - [filter][wrap]: filter 0=Nearest 1=Linear, wrap 0=Clamp 1=Wrap 2=Mirror
static void* g_MetalSamplers[2][3]  = {};  // id<MTLSamplerState>
static void* g_SamplerStack[8]      = {};  // id<MTLSamplerState>
static int   g_SamplerDepth         = 0;

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
static void ImPlatform_RunRenderPass();

IMPLATFORM_API bool ImPlatform_GfxAPIRender(ImVec4 const vClearColor)
{
    @autoreleasepool {
        (void)vClearColor;
        ImPlatform_RunRenderPass();     // Queued by ImPlatform_QueueRenderPass(): committed before the frame.

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

IMPLATFORM_API ImPlatform_TextureDesc3D ImPlatform_TextureDesc3D_Default(unsigned int w, unsigned int h, unsigned int d)
{
    ImPlatform_TextureDesc3D x; x.width = w; x.height = h; x.depth = d;
    x.format = ImPlatform_PixelFormat_RGBA8;
    x.min_filter = ImPlatform_TextureFilter_Linear; x.mag_filter = ImPlatform_TextureFilter_Linear;
    x.wrap_u = x.wrap_v = x.wrap_w = ImPlatform_TextureWrap_Clamp;
    return x;
}
IMPLATFORM_API bool ImPlatform_SupportsTexture3D(void) { return g_GfxData.pMetalDevice != NULL; }

// An id<MTLTexture> of type 3D: SetShaderTexture binds it like the 2D ones (the shader declares texture3d<float>).
IMPLATFORM_API ImTextureID ImPlatform_CreateTexture3D(const void* voxel_data, const ImPlatform_TextureDesc3D* desc)
{
    if (!desc || !voxel_data || !g_GfxData.pMetalDevice)
        return (ImTextureID)0;
    @autoreleasepool {
        int bytes_per_pixel;
        MTLTextureDescriptor* td = [[MTLTextureDescriptor alloc] init];
        td.textureType = MTLTextureType3D;
        td.pixelFormat = ImPlatform_GetMetalFormat(desc->format, &bytes_per_pixel);
        td.width = desc->width;
        td.height = desc->height;
        td.depth = desc->depth;
        td.usage = MTLTextureUsageShaderRead;
        td.storageMode = MTLStorageModeManaged;
        id<MTLTexture> texture = [(__bridge id<MTLDevice>)g_GfxData.pMetalDevice newTextureWithDescriptor:td];
        if (!texture)
            return (ImTextureID)0;
        const NSUInteger row = (NSUInteger)desc->width * (NSUInteger)bytes_per_pixel;
        [texture replaceRegion:MTLRegionMake3D(0, 0, 0, desc->width, desc->height, desc->depth) mipmapLevel:0 slice:0
                     withBytes:voxel_data bytesPerRow:row bytesPerImage:row * desc->height];
        return (ImTextureID)(__bridge_retained void*)texture;
    }
}

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
// Custom Vertex/Index Buffer Management API - Metal
// ============================================================================

// Vertex buffers are with the custom shaders below (they draw on the render encoder of the draw callback).
// shortcut: no direct draws or render target stack on Metal yet (IMPLATFORM_GFX_SUPPORT_DIRECT_DRAW, _RENDER_TARGETS);
// add them with the render command encoder of the draw callback.

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
    void* textures[9];          // ImPlatform_SetShaderTexture: id<MTLTexture> of t1..t8 (retained; slot 0 unused)
    struct { ImGuiID layout; void* pipeline; } layoutPipelines[8];  // ImPlatform_DrawIndexed: id<MTLRenderPipelineState> per layout
    int layoutPipelineCount;
    void* mtlVertexDescriptor;  // MTLVertexDescriptor of ImDrawVert for this program (retained); NULL with its own layout
    unsigned int blend;         // ImPlatform_BlendKey the pipelines are built with; 0: Dear ImGui's blending
};

// Current draw data for custom shader rendering (needed for multi-viewport)
static ImDrawData* g_CurrentDrawData = nullptr;

// Current render encoder for custom shader rendering (set during render pass)
static void* g_CurrentRenderEncoder = nullptr; // id<MTLRenderCommandEncoder>
static ImPlatform_ShaderProgramData_Metal* g_MetalBoundProgram = nullptr;  // Bound by the last custom shader callback

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
    ImPlatform_ShaderDesc with_defines;
    ImGuiTextBuffer defined_source;
    desc = ImPlatform_ShaderApplyDefines(desc, &with_defines, &defined_source);
    if (!desc || !desc->source_code || !g_GfxData.pMetalDevice)
        return NULL;

    @autoreleasepool {
        id<MTLDevice> device = (__bridge id<MTLDevice>)g_GfxData.pMetalDevice;

        // Compile Metal shader from MSL source
        NSError* error = nil;
        NSString* sourceString = [NSString stringWithUTF8String:desc->source_code];

        MTLCompileOptions* options = [[MTLCompileOptions alloc] init];     // The newest language version of the OS

        id<MTLLibrary> library = [device newLibraryWithSource:sourceString options:options error:&error];
        if (!library)
        {
            ImPlatform_ShaderLog("Metal shader compilation failed: %s", error ? error.localizedDescription.UTF8String : "");
            return NULL;
        }

        // Get the entry point function
        NSString* entryPoint = desc->entry_point ? [NSString stringWithUTF8String:desc->entry_point] : @"main0";
        id<MTLFunction> function = [library newFunctionWithName:entryPoint];
        if (!function)
        {
            ImPlatform_ShaderLog("Metal shader entry point '%s' not found", entryPoint.UTF8String);
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

static id<MTLRenderPipelineState> ImPlatform_Metal_CreatePipeline(id<MTLFunction> vertexFunction, id<MTLFunction> fragmentFunction,
                                                                  MTLVertexDescriptor* vertexDescriptor, unsigned int blend);

IMPLATFORM_API ImPlatform_ShaderProgram ImPlatform_CreateShaderProgram(ImPlatform_Shader vertex_shader, ImPlatform_Shader fragment_shader)
{
    if (!vertex_shader || !fragment_shader || !g_GfxData.pMetalDevice)
        return NULL;

    @autoreleasepool {
        ImPlatform_ShaderData_Metal* vs_data = (ImPlatform_ShaderData_Metal*)vertex_shader;
        ImPlatform_ShaderData_Metal* fs_data = (ImPlatform_ShaderData_Metal*)fragment_shader;

        id<MTLFunction> vertexFunction = (__bridge id<MTLFunction>)vs_data->mtlFunction;
        id<MTLFunction> fragmentFunction = (__bridge id<MTLFunction>)fs_data->mtlFunction;

        // Vertex descriptor of ImDrawVert (Dear ImGui binds the vertices at buffer 0). The attributes are told apart by
        // type, in any order: the 4-component one is the color, the first 2-component one the position, the next the
        // UV. HLSL compiled by Slang numbers them in declaration order; shaders without attributes get 0 pos, 1 uv, 2 col.
        MTLVertexDescriptor* vertexDescriptor = [[MTLVertexDescriptor alloc] init];
        int locPos = 0, locUV = 1, locCol = 2;
        bool customLayout = false;      // An input ImDrawVert does not fill: the program's own vertex buffers
        if (vertexFunction.vertexAttributes.count > 0)
        {
            locPos = locUV = locCol = -1;
            for (MTLVertexAttribute* attribute in vertexFunction.vertexAttributes)
            {
                if (!attribute.active)
                    continue;
                const int index = (int)attribute.attributeIndex;
                const bool two = attribute.attributeType == MTLDataTypeFloat2 || attribute.attributeType == MTLDataTypeHalf2;
                if (!two && locCol < 0)         locCol = index;
                else if (two && locPos < 0)     locPos = index;
                else if (two && locUV < 0)      locUV = index;
                else                            customLayout = true;
            }
        }
        if (locPos >= 0)
        {
            vertexDescriptor.attributes[locPos].format = MTLVertexFormatFloat2;
            vertexDescriptor.attributes[locPos].offset = offsetof(ImDrawVert, pos);
            vertexDescriptor.attributes[locPos].bufferIndex = 0;
        }
        if (locUV >= 0)
        {
            vertexDescriptor.attributes[locUV].format = MTLVertexFormatFloat2;
            vertexDescriptor.attributes[locUV].offset = offsetof(ImDrawVert, uv);
            vertexDescriptor.attributes[locUV].bufferIndex = 0;
        }
        if (locCol >= 0)
        {
            vertexDescriptor.attributes[locCol].format = MTLVertexFormatUChar4Normalized;
            vertexDescriptor.attributes[locCol].offset = offsetof(ImDrawVert, col);
            vertexDescriptor.attributes[locCol].bufferIndex = 0;
        }

        // Layout
        vertexDescriptor.layouts[0].stride = sizeof(ImDrawVert);
        vertexDescriptor.layouts[0].stepRate = 1;
        vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

        // A program whose vertex inputs are not ImDrawVert's draws vertex buffers only (ImPlatform_DrawIndexed builds
        // its pipeline per vertex buffer layout).
        id<MTLRenderPipelineState> pipelineState = nil;
        if (!customLayout && !(pipelineState = ImPlatform_Metal_CreatePipeline(vertexFunction, fragmentFunction, vertexDescriptor, 0)))
            return NULL;

        // Create program data
        ImPlatform_ShaderProgramData_Metal* program_data = new ImPlatform_ShaderProgramData_Metal();
        program_data->mtlVertexFunction = (__bridge_retained void*)vertexFunction;
        program_data->mtlFragmentFunction = (__bridge_retained void*)fragmentFunction;
        program_data->mtlRenderPipelineState = pipelineState ? (__bridge_retained void*)pipelineState : nullptr;
        program_data->mtlVertexDescriptor = customLayout ? nullptr : (__bridge_retained void*)vertexDescriptor;
        program_data->mtlUniformBuffer = nullptr;
        program_data->uniformData = nullptr;
        program_data->uniformDataSize = 0;
        program_data->uniformDataDirty = false;

        return program_data;
    }
}

// The pipeline of a program for a vertex layout: the program's blending (ImPlatform_BlendKey; Dear ImGui's for 0) and
// Dear ImGui's target format, through the binary archive.
static id<MTLRenderPipelineState> ImPlatform_Metal_CreatePipeline(id<MTLFunction> vertexFunction, id<MTLFunction> fragmentFunction,
                                                                  MTLVertexDescriptor* vertexDescriptor, unsigned int blend)
{
    @autoreleasepool {
        static const MTLBlendFactor factors[] = { MTLBlendFactorZero, MTLBlendFactorOne, MTLBlendFactorSourceColor,
                                                  MTLBlendFactorOneMinusSourceColor, MTLBlendFactorSourceAlpha,
                                                  MTLBlendFactorOneMinusSourceAlpha, MTLBlendFactorDestinationColor,
                                                  MTLBlendFactorOneMinusDestinationColor, MTLBlendFactorDestinationAlpha,
                                                  MTLBlendFactorOneMinusDestinationAlpha };
        static const MTLBlendOperation ops[] = { MTLBlendOperationAdd, MTLBlendOperationSubtract, MTLBlendOperationReverseSubtract,
                                                 MTLBlendOperationMin, MTLBlendOperationMax };
        const ImPlatform_BlendDesc b = ImPlatform_BlendFromKey(blend);
        id<MTLDevice> device = (__bridge id<MTLDevice>)g_GfxData.pMetalDevice;
        MTLRenderPipelineDescriptor* pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineDescriptor.vertexFunction = vertexFunction;
        pipelineDescriptor.fragmentFunction = fragmentFunction;
        pipelineDescriptor.vertexDescriptor = vertexDescriptor;
        pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        pipelineDescriptor.colorAttachments[0].blendingEnabled = b.enable ? YES : NO;
        pipelineDescriptor.colorAttachments[0].rgbBlendOperation = ops[b.op_color];
        pipelineDescriptor.colorAttachments[0].alphaBlendOperation = ops[b.op_alpha];
        pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = factors[b.src_color];
        pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = factors[b.dst_color];
        pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = factors[b.src_alpha];
        pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = factors[b.dst_alpha];

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
            ImPlatform_ShaderLog("Metal render pipeline creation failed: %s", error ? error.localizedDescription.UTF8String : "");
            return nil;
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
        return pipelineState;
    }
}

// Blending is part of the pipelines: the one for Dear ImGui's vertex layout is built again here, those of the vertex
// buffer layouts at their next draw. Encoders keep the pipelines they were given, so the earlier ones can be released.
IMPLATFORM_API bool ImPlatform_SetShaderProgramBlend(ImPlatform_ShaderProgram program, const ImPlatform_BlendDesc* blend)
{
    ImPlatform_ShaderProgramData_Metal* program_data = (ImPlatform_ShaderProgramData_Metal*)program;
    unsigned int key = 0;
    if (!program_data || !ImPlatform_BlendKey(blend, &key))
        return false;
    if (key == program_data->blend)
        return true;

    @autoreleasepool {
        if (program_data->mtlVertexDescriptor)
        {
            id<MTLRenderPipelineState> pipeline = ImPlatform_Metal_CreatePipeline(
                (__bridge id<MTLFunction>)program_data->mtlVertexFunction, (__bridge id<MTLFunction>)program_data->mtlFragmentFunction,
                (__bridge MTLVertexDescriptor*)program_data->mtlVertexDescriptor, key);
            if (!pipeline)
                return false;
            if (program_data->mtlRenderPipelineState)
                CFRelease(program_data->mtlRenderPipelineState);
            program_data->mtlRenderPipelineState = (__bridge_retained void*)pipeline;
        }
        for (int i = 0; i < program_data->layoutPipelineCount; i++)
            CFRelease(program_data->layoutPipelines[i].pipeline);
        program_data->layoutPipelineCount = 0;
        program_data->blend = key;
    }
    return true;
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
        for (void* texture : program_data->textures)
            if (texture)
                CFRelease(texture);
        for (int i = 0; i < program_data->layoutPipelineCount; i++)
            CFRelease(program_data->layoutPipelines[i].pipeline);
        if (program_data->mtlVertexDescriptor)
            CFRelease(program_data->mtlVertexDescriptor);
        if (g_MetalBoundProgram == program_data)
            g_MetalBoundProgram = nullptr;

        delete program_data;
    }
}

// Binds t1..t8 of a program (fragment and vertex stages) on the current render encoder.
static void ImPlatform_Metal_BindProgramTextures(ImPlatform_ShaderProgramData_Metal* program_data)
{
    id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)g_CurrentRenderEncoder;
    for (NSUInteger slot = 1; encoder && slot < 9; slot++)
        if (program_data->textures[slot])
        {
            [encoder setFragmentTexture:(__bridge id<MTLTexture>)program_data->textures[slot] atIndex:slot];
            [encoder setVertexTexture:(__bridge id<MTLTexture>)program_data->textures[slot] atIndex:slot];
        }
}

// Slots 1..8 are kept by the program and bound with it; applied at once when it is the bound program.
IMPLATFORM_API bool ImPlatform_SetShaderTexture(ImPlatform_ShaderProgram program, const char* /*name*/, unsigned int slot, ImTextureID texture)
{
    ImPlatform_ShaderProgramData_Metal* program_data = (ImPlatform_ShaderProgramData_Metal*)program;
    if (!program_data || slot < 1 || slot > 8)
        return false;
    void* tex = (void*)texture;
    if (tex)
        CFRetain(tex);
    if (program_data->textures[slot])
        CFRelease(program_data->textures[slot]);
    program_data->textures[slot] = tex;
    if (program_data == g_MetalBoundProgram)
        ImPlatform_Metal_BindProgramTextures(program_data);
    return true;
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
// Render passes: their own command buffer, committed before the frame's. Targets have the layer's format
// (MTLPixelFormatBGRA8Unorm); the ImGui backend picks its pipeline from the pass descriptor given to its NewFrame.
static ImTextureID g_PassTarget = 0;
static ImDrawData* g_PassDrawData = NULL;
static bool        g_PassClear = false;

IMPLATFORM_API ImPlatform_PixelFormat ImPlatform_GetRenderPassFormat(void) { return ImPlatform_PixelFormat_BGRA8; }

// Readback: a blit into a shared buffer on the queue, after the work already committed, waited for.
IMPLATFORM_API bool ImPlatform_SupportsReadback(void) { return true; }

IMPLATFORM_API bool ImPlatform_ReadTexture(ImTextureID texture, unsigned int x, unsigned int y, unsigned int width, unsigned int height,
                                           void* out_pixels, size_t out_size)
{
    if (!texture || !out_pixels || !width || !height || !g_GfxData.pMetalDevice || !g_GfxData.pCommandQueue)
        return false;
    @autoreleasepool {
        id<MTLTexture> tex = (__bridge id<MTLTexture>)(void*)texture;
        unsigned int bpp = 0;
        switch (tex.pixelFormat)
        {
        case MTLPixelFormatR8Unorm:                                         bpp = 1; break;
        case MTLPixelFormatRGBA8Unorm: case MTLPixelFormatRGBA8Unorm_sRGB:
        case MTLPixelFormatBGRA8Unorm: case MTLPixelFormatBGRA8Unorm_sRGB:
        case MTLPixelFormatR32Float:                                        bpp = 4; break;
        case MTLPixelFormatRGBA16Float:                                     bpp = 8; break;
        case MTLPixelFormatRGBA32Float:                                     bpp = 16; break;
        default:                                                            return false;
        }
        const size_t row = (size_t)width * bpp;
        if (x + width > tex.width || y + height > tex.height || out_size < row * height)
            return false;
        id<MTLDevice> device = (__bridge id<MTLDevice>)g_GfxData.pMetalDevice;
        id<MTLBuffer> buffer = [device newBufferWithLength:row * height options:MTLResourceStorageModeShared];
        id<MTLCommandBuffer> commandBuffer = [(__bridge id<MTLCommandQueue>)g_GfxData.pCommandQueue commandBuffer];
        id<MTLBlitCommandEncoder> blit = [commandBuffer blitCommandEncoder];
        [blit copyFromTexture:tex sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(x, y, 0) sourceSize:MTLSizeMake(width, height, 1)
                     toBuffer:buffer destinationOffset:0 destinationBytesPerRow:row destinationBytesPerImage:row * height];
        [blit endEncoding];
        [commandBuffer commit];
        [commandBuffer waitUntilCompleted];
        if (!buffer || commandBuffer.status != MTLCommandBufferStatusCompleted)
            return false;
        memcpy(out_pixels, buffer.contents, row * height);
        return true;
    }
}

IMPLATFORM_API bool ImPlatform_QueueRenderPass(ImTextureID target, ImDrawData* draw_data, bool clear)
{
    if (!target || !draw_data || !g_GfxData.pCommandQueue)
        return false;
    id<MTLTexture> texture = (__bridge id<MTLTexture>)(void*)target;
    if (!(texture.usage & MTLTextureUsageRenderTarget) || texture.pixelFormat != MTLPixelFormatBGRA8Unorm)
        return false;
    g_PassTarget = target;
    g_PassDrawData = draw_data;
    g_PassClear = clear;
    return true;
}

static void ImPlatform_RunRenderPass()
{
    ImDrawData* draw_data = g_PassDrawData;
    g_PassDrawData = NULL;
    if (!draw_data)
        return;
    @autoreleasepool {
        MTLRenderPassDescriptor* desc = [MTLRenderPassDescriptor renderPassDescriptor];
        desc.colorAttachments[0].texture = (__bridge id<MTLTexture>)(void*)g_PassTarget;
        desc.colorAttachments[0].loadAction = g_PassClear ? MTLLoadActionClear : MTLLoadActionLoad;
        desc.colorAttachments[0].storeAction = MTLStoreActionStore;
        desc.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 0);
        if (draw_data->Textures == NULL)
            draw_data->Textures = &ImGui::GetPlatformIO().Textures;     // The frame's texture updates, before they are drawn.
        ImGui_ImplMetal_NewFrame(desc);     // The pipeline for the target.
        id<MTLCommandQueue> commandQueue = (__bridge id<MTLCommandQueue>)g_GfxData.pCommandQueue;
        id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
        id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:desc];
        ImDrawData* saved_draw_data = g_CurrentDrawData;
        void* saved_encoder = g_CurrentRenderEncoder;
        ImPlatform_RenderDrawDataWrapper(draw_data, commandBuffer, encoder);    // Its viewport and projection come from the draw data.
        g_CurrentDrawData = saved_draw_data;
        g_CurrentRenderEncoder = saved_encoder;
        [encoder endEncoding];
        [commandBuffer commit];
        ImGui_ImplMetal_NewFrame((__bridge MTLRenderPassDescriptor*)g_GfxData.pRenderPassDescriptor);     // Back to the frame's.
    }
}

// Premultiplied alpha: ImGui's pipeline (its shaders, vertex layout and buffer indices) with premultiplied blending
// (ONE, ONE_MINUS_SRC_ALPHA): texel times the (premultiplied) vertex color.
static void* g_PremultipliedPipeline = NULL;     // id<MTLRenderPipelineState>

static void* ImPlatform_PremultipliedPipeline()
{
    if (g_PremultipliedPipeline || !g_GfxData.pMetalDevice)
        return g_PremultipliedPipeline;
    @autoreleasepool {
        id<MTLDevice> device = (__bridge id<MTLDevice>)g_GfxData.pMetalDevice;
        NSString* source = @""
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "struct Uniforms { float4x4 projectionMatrix; };\n"
            "struct VertexIn { float2 position [[attribute(0)]]; float2 texCoords [[attribute(1)]]; uchar4 color [[attribute(2)]]; };\n"
            "struct VertexOut { float4 position [[position]]; float2 texCoords; float4 color; };\n"
            "vertex VertexOut vertex_main(VertexIn in [[stage_in]], constant Uniforms &uniforms [[buffer(1)]]) {\n"
            "    VertexOut out;\n"
            "    out.position = uniforms.projectionMatrix * float4(in.position, 0, 1);\n"
            "    out.texCoords = in.texCoords;\n"
            "    out.color = float4(in.color) / float4(255.0);\n"
            "    return out;\n"
            "}\n"
            "fragment half4 fragment_main(VertexOut in [[stage_in]], texture2d<half, access::sample> texture [[texture(0)]], sampler textureSampler [[sampler(0)]]) {\n"
            "    half4 t = texture.sample(textureSampler, in.texCoords);\n"
            "    return t * half4(in.color);\n"
            "}\n";
        NSError* error = nil;
        id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
        if (!library)
            return NULL;
        MTLVertexDescriptor* vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
        vertexDescriptor.attributes[0].offset = offsetof(ImDrawVert, pos);
        vertexDescriptor.attributes[0].format = MTLVertexFormatFloat2;
        vertexDescriptor.attributes[0].bufferIndex = 0;
        vertexDescriptor.attributes[1].offset = offsetof(ImDrawVert, uv);
        vertexDescriptor.attributes[1].format = MTLVertexFormatFloat2;
        vertexDescriptor.attributes[1].bufferIndex = 0;
        vertexDescriptor.attributes[2].offset = offsetof(ImDrawVert, col);
        vertexDescriptor.attributes[2].format = MTLVertexFormatUChar4;
        vertexDescriptor.attributes[2].bufferIndex = 0;
        vertexDescriptor.layouts[0].stepRate = 1;
        vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
        vertexDescriptor.layouts[0].stride = sizeof(ImDrawVert);
        MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
        desc.vertexFunction = [library newFunctionWithName:@"vertex_main"];
        desc.fragmentFunction = [library newFunctionWithName:@"fragment_main"];
        desc.vertexDescriptor = vertexDescriptor;
        desc.rasterSampleCount = 1;
        desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        desc.colorAttachments[0].blendingEnabled = YES;
        desc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        desc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
        desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
        desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        id<MTLRenderPipelineState> state = [device newRenderPipelineStateWithDescriptor:desc error:&error];
        if (state)
            g_PremultipliedPipeline = (__bridge_retained void*)state;
    }
    return g_PremultipliedPipeline;
}

IMPLATFORM_API void ImPlatform_PushPremultipliedAlpha(ImDrawList* draw)
{
    if (!ImPlatform_PremultipliedPipeline())
        return;
    (draw ? draw : ImGui::GetWindowDrawList())->AddCallback([](const ImDrawList*, const ImDrawCmd*)
    {
        id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)g_CurrentRenderEncoder;
        if (encoder)
            [encoder setRenderPipelineState:(__bridge id<MTLRenderPipelineState>)g_PremultipliedPipeline];
    }, NULL);
}

IMPLATFORM_API void ImPlatform_PopPremultipliedAlpha(ImDrawList* draw)
{
    if (g_PremultipliedPipeline)
        (draw ? draw : ImGui::GetWindowDrawList())->AddCallback(ImDrawCallback_ResetRenderState, NULL);
}

static void ImPlatform_RenderDrawDataWrapper(ImDrawData* draw_data, id<MTLCommandBuffer> commandBuffer, id<MTLRenderCommandEncoder> renderEncoder)
{
    // Store draw data and render encoder for custom shader callbacks (needed for multi-viewport)
    g_CurrentDrawData = draw_data;
    g_CurrentRenderEncoder = (__bridge void*)renderEncoder;

    ImGui_ImplMetal_RenderDrawData(draw_data, commandBuffer, renderEncoder);

    g_CurrentRenderEncoder = nullptr;
    // NOTE: Don't clear g_CurrentDrawData here - it will be updated by the next render call
}

// Binds a custom program on the current render encoder. Buffer indices are the HLSL registers plus one (Dear ImGui's
// vertices are at buffer 0; compile_shaders shifts Slang's output): b0, the projection, at 1 where Dear ImGui's backend
// also puts it, b1, the parameters, at 2 in both stages. t0 is the draw's texture (texture 0) and s0 sampler 0.
IMPLATFORM_API void ImPlatform_BeginCustomShader_Render(ImPlatform_ShaderProgram program)
{
    @autoreleasepool {
        ImDrawData* draw_data = g_CurrentDrawData ? g_CurrentDrawData : ImGui::GetDrawData();
        if (!program || !g_CurrentRenderEncoder || !draw_data)
            return;
        ImPlatform_ShaderProgramData_Metal* program_data = (ImPlatform_ShaderProgramData_Metal*)program;
        id<MTLRenderCommandEncoder> renderEncoder = (__bridge id<MTLRenderCommandEncoder>)g_CurrentRenderEncoder;

        const float L = draw_data->DisplayPos.x;
        const float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
        const float T = draw_data->DisplayPos.y;
        const float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
        const float ortho_projection[4][4] =
        {
            { 2.0f / (R - L),     0.0f,              0.0f, 0.0f },
            { 0.0f,               2.0f / (T - B),    0.0f, 0.0f },
            { 0.0f,               0.0f,              0.5f, 0.0f },
            { (R + L) / (L - R),  (T + B) / (B - T), 0.5f, 1.0f },
        };

        if (program_data->mtlRenderPipelineState)
            [renderEncoder setRenderPipelineState:(__bridge id<MTLRenderPipelineState>)program_data->mtlRenderPipelineState];
        [renderEncoder setVertexBytes:&ortho_projection length:sizeof(ortho_projection) atIndex:1];
        [renderEncoder setFragmentBytes:&ortho_projection length:sizeof(ortho_projection) atIndex:1];
        // Parameters up to 4 KB through setBytes; larger blocks in a buffer made for this bind (both copy the values now,
        // so each draw keeps the parameters set for it).
        if (program_data->uniformData && program_data->uniformDataSize > 0 && program_data->uniformDataSize <= 4096)
        {
            [renderEncoder setVertexBytes:program_data->uniformData length:program_data->uniformDataSize atIndex:2];
            [renderEncoder setFragmentBytes:program_data->uniformData length:program_data->uniformDataSize atIndex:2];
        }
        else if (program_data->uniformData && program_data->uniformDataSize > 0)
        {
            id<MTLDevice> device = (__bridge id<MTLDevice>)g_GfxData.pMetalDevice;
            id<MTLBuffer> buffer = [device newBufferWithBytes:program_data->uniformData length:program_data->uniformDataSize options:MTLResourceStorageModeShared];
            [renderEncoder setVertexBuffer:buffer offset:0 atIndex:2];
            [renderEncoder setFragmentBuffer:buffer offset:0 atIndex:2];
        }
        ImPlatform_Metal_BindProgramTextures(program_data);
        g_MetalBoundProgram = program_data;
        if (g_MetalSamplers[1][0])      // Linear, clamp: Dear ImGui's shader has its own sampler, a custom one reads s0
        {
            [renderEncoder setFragmentSamplerState:(__bridge id<MTLSamplerState>)g_MetalSamplers[1][0] atIndex:0];
            [renderEncoder setVertexSamplerState:(__bridge id<MTLSamplerState>)g_MetalSamplers[1][0] atIndex:0];
        }
    }
}

// ImDrawCallback handler to activate a custom shader
static void ImPlatform_SetCustomShader(const ImDrawList* /*parent_list*/, const ImDrawCmd* cmd)
{
    ImPlatform_BeginCustomShader_Render((ImPlatform_ShaderProgram)cmd->UserCallbackData);
}

IMPLATFORM_API void ImPlatform_BeginCustomShader(ImDrawList* draw, ImPlatform_ShaderProgram shader)
{
    if (!draw || !shader)
        return;

    ImPlatform_ScissorBeforeCallbacks(draw);
    draw->AddCallback(&ImPlatform_SetCustomShader, shader);
}

IMPLATFORM_API void ImPlatform_EndCustomShader(ImDrawList* draw)
{
    if (!draw)
        return;

    draw->AddCallback([](const ImDrawList*, const ImDrawCmd*) { g_MetalBoundProgram = nullptr; }, NULL);   // Unbound by the reset
    draw->AddCallback(ImGui::GetPlatformIO().DrawCallback_ResetRenderState, NULL);
}

// ============================================================================
// Custom Vertex/Index Buffer Management API - Metal
// ============================================================================

// Shared storage buffers, drawn by ImPlatform_DrawIndexed inside a draw callback with the program bound last there.
// An update of a buffer drawn since it was last written goes to a copy: the frames that drew it keep the previous one.
struct ImPlatform_BufferData_Metal
{
    void* buffer;                               // id<MTLBuffer> (retained)
    bool drawn;                                 // A draw of it was recorded since it was last written
    ImPlatform_VertexBufferDesc vb_desc;        // vertex buffer: its layout (attributes copied below)
    ImPlatform_VertexAttribute attributes[16];
    ImGuiID layout;                             // hash of the attributes and stride: the pipeline cache key
    ImPlatform_IndexFormat index_format;        // index buffer
};

static ImPlatform_BufferData_Metal* g_MetalBoundVB = nullptr;
static ImPlatform_BufferData_Metal* g_MetalBoundIB = nullptr;

static void* ImPlatform_Metal_NewBuffer(const void* data, size_t size)
{
    if (!g_GfxData.pMetalDevice || size == 0)
        return nullptr;
    id<MTLDevice> device = (__bridge id<MTLDevice>)g_GfxData.pMetalDevice;
    id<MTLBuffer> buffer = data ? [device newBufferWithBytes:data length:size options:MTLResourceStorageModeShared]
                                : [device newBufferWithLength:size options:MTLResourceStorageModeShared];
    return buffer ? (__bridge_retained void*)buffer : nullptr;
}

// Leaves in data->buffer memory no recorded draw reads: command buffers retain the buffers they draw, so a drawn one is
// left to them and replaced by a copy.
// shortcut: one MTLBuffer per update of a drawn buffer; pool them if their allocation shows in a profile.
static bool ImPlatform_Metal_WritableBuffer(ImPlatform_BufferData_Metal* data)
{
    if (!data->drawn)
        return true;
    id<MTLBuffer> previous = (__bridge id<MTLBuffer>)data->buffer;
    void* copy = ImPlatform_Metal_NewBuffer(previous.contents, (size_t)previous.length);
    if (!copy)
        return false;
    CFRelease(data->buffer);
    data->buffer = copy;
    data->drawn = false;
    return true;
}

IMPLATFORM_API ImPlatform_VertexBuffer ImPlatform_CreateVertexBuffer(const void* vertex_data, const ImPlatform_VertexBufferDesc* desc)
{
    if (!desc || !desc->vertex_count || !desc->vertex_stride || desc->attribute_count > 16)
        return nullptr;
    void* buffer = ImPlatform_Metal_NewBuffer(vertex_data, (size_t)desc->vertex_count * desc->vertex_stride);
    if (!buffer)
        return nullptr;
    ImPlatform_BufferData_Metal* data = new ImPlatform_BufferData_Metal();
    data->buffer = buffer;
    data->vb_desc = *desc;
    for (unsigned int i = 0; i < desc->attribute_count; i++)
        data->attributes[i] = desc->attributes[i];
    data->vb_desc.attributes = data->attributes;
    ImGuiID layout = 2166136261u ^ desc->vertex_stride;     // FNV-1a style, as the WebGPU backend's layout key
    for (unsigned int i = 0; i < desc->attribute_count; i++)
        layout = (((layout ^ (ImGuiID)desc->attributes[i].format) * 16777619u) ^ desc->attributes[i].offset) * 16777619u;
    data->layout = layout;
    return data;
}

IMPLATFORM_API bool ImPlatform_UpdateVertexBuffer(ImPlatform_VertexBuffer buffer, const void* vertex_data, unsigned int offset, unsigned int count)
{
    ImPlatform_BufferData_Metal* data = (ImPlatform_BufferData_Metal*)buffer;
    if (!data || !vertex_data || offset + count > data->vb_desc.vertex_count || !ImPlatform_Metal_WritableBuffer(data))
        return false;
    id<MTLBuffer> mtl = (__bridge id<MTLBuffer>)data->buffer;
    memcpy((char*)mtl.contents + (size_t)offset * data->vb_desc.vertex_stride, vertex_data, (size_t)count * data->vb_desc.vertex_stride);
    return true;
}

IMPLATFORM_API void ImPlatform_DestroyVertexBuffer(ImPlatform_VertexBuffer buffer)
{
    ImPlatform_BufferData_Metal* data = (ImPlatform_BufferData_Metal*)buffer;
    if (!data)
        return;
    if (g_MetalBoundVB == data) g_MetalBoundVB = nullptr;
    if (g_MetalBoundIB == data) g_MetalBoundIB = nullptr;
    CFRelease(data->buffer);    // Command buffers that use it keep their own reference
    delete data;
}

IMPLATFORM_API ImPlatform_IndexBuffer ImPlatform_CreateIndexBuffer(const void* index_data, const ImPlatform_IndexBufferDesc* desc)
{
    if (!desc || !desc->index_count)
        return nullptr;
    const size_t index_size = desc->format == ImPlatform_IndexFormat_UInt16 ? 2 : 4;
    void* buffer = ImPlatform_Metal_NewBuffer(index_data, (size_t)desc->index_count * index_size);
    if (!buffer)
        return nullptr;
    ImPlatform_BufferData_Metal* data = new ImPlatform_BufferData_Metal();
    data->buffer = buffer;
    data->index_format = desc->format;
    data->vb_desc.vertex_count = desc->index_count;     // The index count, for the bounds of UpdateIndexBuffer
    return data;
}

IMPLATFORM_API bool ImPlatform_UpdateIndexBuffer(ImPlatform_IndexBuffer buffer, const void* index_data, unsigned int offset, unsigned int count)
{
    ImPlatform_BufferData_Metal* data = (ImPlatform_BufferData_Metal*)buffer;
    if (!data || !index_data || offset + count > data->vb_desc.vertex_count || !ImPlatform_Metal_WritableBuffer(data))
        return false;
    const size_t index_size = data->index_format == ImPlatform_IndexFormat_UInt16 ? 2 : 4;
    id<MTLBuffer> mtl = (__bridge id<MTLBuffer>)data->buffer;
    memcpy((char*)mtl.contents + (size_t)offset * index_size, index_data, (size_t)count * index_size);
    return true;
}

IMPLATFORM_API void ImPlatform_DestroyIndexBuffer(ImPlatform_IndexBuffer buffer)
{
    ImPlatform_DestroyVertexBuffer(buffer);
}

IMPLATFORM_API void ImPlatform_BindBuffers(ImPlatform_VertexBuffer vertex_buffer, ImPlatform_IndexBuffer index_buffer)
{
    g_MetalBoundVB = (ImPlatform_BufferData_Metal*)vertex_buffer;
    g_MetalBoundIB = (ImPlatform_BufferData_Metal*)index_buffer;
}

// The layout comes from the vertex buffer's attributes at draw time (attribute i at shader location i).
IMPLATFORM_API bool ImPlatform_CreateVertexInputLayout(ImPlatform_VertexBuffer vertex_buffer, ImPlatform_ShaderProgram program)
{
    return vertex_buffer && program;
}

// The program's pipeline for a vertex buffer's layout: attribute i at location i, the vertices at buffer 0.
static id<MTLRenderPipelineState> ImPlatform_Metal_LayoutPipeline(ImPlatform_ShaderProgramData_Metal* program, ImPlatform_BufferData_Metal* vb)
{
    for (int i = 0; i < program->layoutPipelineCount; i++)
        if (program->layoutPipelines[i].layout == vb->layout)
            return (__bridge id<MTLRenderPipelineState>)program->layoutPipelines[i].pipeline;
    static const MTLVertexFormat formats[] = { MTLVertexFormatFloat, MTLVertexFormatFloat2, MTLVertexFormatFloat3, MTLVertexFormatFloat4,
                                               MTLVertexFormatUChar4, MTLVertexFormatUChar4Normalized };
    MTLVertexDescriptor* vertexDescriptor = [[MTLVertexDescriptor alloc] init];
    for (unsigned int i = 0; i < vb->vb_desc.attribute_count; i++)
    {
        const int format = (int)vb->attributes[i].format;
        vertexDescriptor.attributes[i].format = formats[format >= 0 && format < IM_ARRAYSIZE(formats) ? format : 3];
        vertexDescriptor.attributes[i].offset = vb->attributes[i].offset;
        vertexDescriptor.attributes[i].bufferIndex = 0;
    }
    vertexDescriptor.layouts[0].stride = vb->vb_desc.vertex_stride;
    vertexDescriptor.layouts[0].stepRate = 1;
    vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    id<MTLRenderPipelineState> pipeline = ImPlatform_Metal_CreatePipeline((__bridge id<MTLFunction>)program->mtlVertexFunction,
                                                                         (__bridge id<MTLFunction>)program->mtlFragmentFunction, vertexDescriptor,
                                                                         program->blend);
    if (pipeline && program->layoutPipelineCount < IM_ARRAYSIZE(program->layoutPipelines))
    {
        program->layoutPipelines[program->layoutPipelineCount].layout = vb->layout;
        program->layoutPipelines[program->layoutPipelineCount++].pipeline = (__bridge_retained void*)pipeline;
    }
    return pipeline;
}

// Draws the bound buffers inside a draw callback with the program bound last there (ImPlatform_BeginCustomShader,
// ImPlatform_BeginCustomShader_Render), Dear ImGui's blending, into the current target. primitive_type: 0 triangles,
// 1 lines, 2 points.
IMPLATFORM_API void ImPlatform_DrawIndexed(unsigned int primitive_type, unsigned int index_count, unsigned int start_index)
{
    @autoreleasepool {
        ImPlatform_BufferData_Metal* vb = g_MetalBoundVB;
        ImPlatform_BufferData_Metal* ib = g_MetalBoundIB;
        if (!g_CurrentRenderEncoder || !g_MetalBoundProgram || !vb || !ib || index_count == 0)
            return;
        id<MTLRenderPipelineState> pipeline = ImPlatform_Metal_LayoutPipeline(g_MetalBoundProgram, vb);
        if (!pipeline)
            return;
        vb->drawn = ib->drawn = true;
        id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)g_CurrentRenderEncoder;
        [encoder setRenderPipelineState:pipeline];
        [encoder setVertexBuffer:(__bridge id<MTLBuffer>)vb->buffer offset:0 atIndex:0];
        const bool u16 = ib->index_format == ImPlatform_IndexFormat_UInt16;
        const MTLPrimitiveType type = primitive_type == 1 ? MTLPrimitiveTypeLine : primitive_type == 2 ? MTLPrimitiveTypePoint : MTLPrimitiveTypeTriangle;
        [encoder drawIndexedPrimitives:type indexCount:index_count indexType:u16 ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32
                           indexBuffer:(__bridge id<MTLBuffer>)ib->buffer indexBufferOffset:(NSUInteger)start_index * (u16 ? 2 : 4)];
    }
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
