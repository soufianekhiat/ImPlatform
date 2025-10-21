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

#endif // IM_GFX_METAL
