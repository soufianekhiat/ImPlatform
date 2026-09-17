// dear imgui: Graphics API Abstraction - WebGPU Backend
// This handles WebGPU device creation, rendering, and presentation
//
// Supports Dawn, wgpu-native and WGVK via IMGUI_IMPL_WEBGPU_BACKEND_DAWN / _WGPU / _WGVK defines,
// and Emscripten via the Dawn-based '--use-port=emdawnwebgpu' port (Emscripten 4.0.10+, requires -sASYNCIFY).
// Platform surface creation: GLFW, SDL2, SDL3, Win32 (native) and HTML canvas (Emscripten).
// Device and surface setup follow imgui/examples/example_glfw_wgpu, example_sdl2_wgpu and example_sdl3_wgpu.

#include "ImPlatform_Internal.h"

#ifdef IM_GFX_WGPU

#include "../imgui/imgui.h"
#include "../imgui/backends/imgui_impl_wgpu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Platform-specific includes for surface creation
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    #include <GLFW/glfw3.h>
    #ifndef __EMSCRIPTEN__
        #if defined(_WIN32)
            #undef APIENTRY
            #define GLFW_EXPOSE_NATIVE_WIN32        // for glfwGetWin32Window()
        #elif defined(__APPLE__)
            #define GLFW_EXPOSE_NATIVE_COCOA        // for glfwGetCocoaWindow()
        #elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
            #define GLFW_EXPOSE_NATIVE_X11          // for glfwGetX11Display(), glfwGetX11Window()
            #if defined(__has_include) && __has_include(<wayland-client.h>)
                #define GLFW_EXPOSE_NATIVE_WAYLAND  // for glfwGetWaylandDisplay(), glfwGetWaylandWindow()
            #endif
        #endif
        #include <GLFW/glfw3native.h>
        #undef Status                               // X11 headers are leaking this, also used in Dawn API
    #endif
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
    #include <SDL.h>
    #ifndef __EMSCRIPTEN__
        #include <SDL_syswm.h>
    #endif
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
    #include <SDL3/SDL.h>
#endif

#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
    #include <emscripten/html5.h>
#endif

// String view helper (WebGPU API uses WGPUStringView instead of const char*)
static inline WGPUStringView ImPlatform_WGPU_Str(const char* s) { WGPUStringView sv; sv.data = s; sv.length = WGPU_STRLEN; return sv; }
#define WGPU_STR(s) ImPlatform_WGPU_Str(s)

#ifndef WGPU_DEPTH_SLICE_UNDEFINED
#define WGPU_DEPTH_SLICE_UNDEFINED 0xffffffffu
#endif

// Global state
static ImPlatform_GfxData_WebGPU g_GfxData = {};
unsigned int g_ImPlatform_BackbufferW = 0;
unsigned int g_ImPlatform_BackbufferH = 0;

// Uniform block API state
static ImPlatform_ShaderProgram g_CurrentUniformBlockProgram = nullptr;
static void* g_UniformBlockData = nullptr;
static size_t g_UniformBlockSize = 0;

// Current draw data for custom shader rendering
static ImDrawData* g_CurrentDrawData = nullptr;

// Set when the surface reported an unrecoverable status (upstream examples abort())
static bool g_SurfaceLost = false;

// Default sampler for custom shader bind groups (created on first use)
static WGPUSampler g_DefaultSampler = nullptr;

// Active render-to-texture state
static WGPUCommandEncoder     g_RTEncoder = nullptr;
static WGPURenderPassEncoder  g_RTPass    = nullptr;

// ============================================================================
// Texture Resource Tracking
// ============================================================================

struct ImPlatform_TextureTracking_WebGPU
{
    WGPUTexture texture;
    WGPUTextureView textureView;
    WGPUSampler sampler;
    unsigned int width;
    unsigned int height;
    ImPlatform_PixelFormat format;
    ImPlatform_TextureTracking_WebGPU* next;
};

static ImPlatform_TextureTracking_WebGPU* g_TextureTrackingHead = nullptr;

static void ImPlatform_TrackTexture(WGPUTexture texture, WGPUTextureView view, WGPUSampler sampler,
                                     unsigned int width, unsigned int height, ImPlatform_PixelFormat format)
{
    ImPlatform_TextureTracking_WebGPU* entry = new ImPlatform_TextureTracking_WebGPU();
    entry->texture = texture;
    entry->textureView = view;
    entry->sampler = sampler;
    entry->width = width;
    entry->height = height;
    entry->format = format;
    entry->next = g_TextureTrackingHead;
    g_TextureTrackingHead = entry;
}

static ImPlatform_TextureTracking_WebGPU* ImPlatform_FindTrackedTexture(WGPUTextureView view)
{
    for (ImPlatform_TextureTracking_WebGPU* entry = g_TextureTrackingHead; entry; entry = entry->next)
    {
        if (entry->textureView == view)
            return entry;
    }
    return nullptr;
}

static void ImPlatform_UntrackTexture(WGPUTextureView view)
{
    ImPlatform_TextureTracking_WebGPU** prev = &g_TextureTrackingHead;
    for (ImPlatform_TextureTracking_WebGPU* entry = g_TextureTrackingHead; entry; entry = entry->next)
    {
        if (entry->textureView == view)
        {
            *prev = entry->next;
            delete entry;
            return;
        }
        prev = &entry->next;
    }
}

static void ImPlatform_ReleaseAllTrackedTextures()
{
    ImPlatform_TextureTracking_WebGPU* entry = g_TextureTrackingHead;
    while (entry)
    {
        ImPlatform_TextureTracking_WebGPU* next = entry->next;
        if (entry->sampler) wgpuSamplerRelease(entry->sampler);
        if (entry->textureView) wgpuTextureViewRelease(entry->textureView);
        if (entry->texture) wgpuTextureRelease(entry->texture);
        delete entry;
        entry = next;
    }
    g_TextureTrackingHead = nullptr;
}

// ============================================================================
// Internal API - Get WebGPU gfx data
// ============================================================================

ImPlatform_GfxData_WebGPU* ImPlatform_Gfx_GetData_WebGPU(void)
{
    return &g_GfxData;
}

// ============================================================================
// Surface Management
// ============================================================================

static void ImPlatform_ConfigureSurface(unsigned int width, unsigned int height)
{
    g_GfxData.uSurfaceWidth = width;
    g_GfxData.uSurfaceHeight = height;
    g_ImPlatform_BackbufferW = width;
    g_ImPlatform_BackbufferH = height;

    WGPUSurfaceConfiguration config = {};
    config.device = g_GfxData.device;
    config.format = g_GfxData.swapChainFormat;
    config.usage = WGPUTextureUsage_RenderAttachment;
    config.width = width;
    config.height = height;
    config.presentMode = WGPUPresentMode_Fifo;
    config.alphaMode = WGPUCompositeAlphaMode_Auto;
    wgpuSurfaceConfigure(g_GfxData.surface, &config);
}

// Current framebuffer size of the application window, in pixels
static void ImPlatform_GetFramebufferSize(unsigned int* out_width, unsigned int* out_height)
{
    int w = 0, h = 0;
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    glfwGetFramebufferSize(ImPlatform_App_GetGLFWWindow(), &w, &h);
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    RECT rect;
    ::GetClientRect(ImPlatform_App_GetHWND(), &rect);
    w = (int)(rect.right - rect.left);
    h = (int)(rect.bottom - rect.top);
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
    SDL_GetWindowSizeInPixels(ImPlatform_App_GetSDL2Window(), &w, &h);
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
    SDL_GetWindowSizeInPixels(ImPlatform_App_GetSDL3Window(), &w, &h);
#endif
    *out_width = (unsigned int)(w > 0 ? w : 0);
    *out_height = (unsigned int)(h > 0 ? h : 0);
}

// ============================================================================
// Internal API - Create WebGPU device
// ============================================================================

static void ImPlatform_WGPU_UncapturedErrorCallback(WGPUDevice const*, WGPUErrorType error_type, WGPUStringView message, void*, void*)
{
    fprintf(stderr, "WebGPU %s error: %.*s\n", ImGui_ImplWGPU_GetErrorTypeName(error_type), (int)message.length, message.data);
}

static void ImPlatform_WGPU_DeviceLostCallback(WGPUDevice const*, WGPUDeviceLostReason reason, WGPUStringView message, void*, void*)
{
    if (reason == WGPUDeviceLostReason_Destroyed)
        return;
    fprintf(stderr, "WebGPU device lost (%s): %.*s\n", ImGui_ImplWGPU_GetDeviceLostReasonName(reason), (int)message.length, message.data);
}

// Callbacks may only fire from wgpuInstanceWaitAny() on native, the browser resolves requests from its event loop.
#ifdef __EMSCRIPTEN__
static const WGPUCallbackMode k_ImPlatform_WGPURequestCallbackMode = WGPUCallbackMode_AllowSpontaneous;
#else
static const WGPUCallbackMode k_ImPlatform_WGPURequestCallbackMode = WGPUCallbackMode_WaitAnyOnly;
#endif

// Wait for an asynchronous adapter/device request to complete
static void ImPlatform_WGPU_WaitRequest(WGPUInstance instance, WGPUFuture future, const bool* done)
{
#if defined(__EMSCRIPTEN__)
    // Yield to the browser event loop so JS promises can resolve (requires -sASYNCIFY)
    IM_UNUSED(instance); IM_UNUSED(future);
    while (!*done)
        emscripten_sleep(0);
#elif defined(IMGUI_IMPL_WEBGPU_BACKEND_WGPU)
    // wgpu-native resolves requests synchronously (and doesn't implement wgpuInstanceWaitAny)
    IM_UNUSED(instance); IM_UNUSED(future); IM_UNUSED(done);
#else
    IM_UNUSED(done);
    WGPUFutureWaitInfo wait_info = { future, false };
    wgpuInstanceWaitAny(instance, 1, &wait_info, UINT64_MAX);
#endif
}

struct ImPlatform_WGPU_AdapterRequest { WGPUAdapter adapter; bool done; };
struct ImPlatform_WGPU_DeviceRequest  { WGPUDevice device; bool done; };

static WGPUAdapter ImPlatform_WGPU_RequestAdapter(WGPUInstance instance, WGPUSurface compatible_surface)
{
    ImPlatform_WGPU_AdapterRequest request = { nullptr, false };

    WGPURequestAdapterOptions adapter_options = {};
    adapter_options.compatibleSurface = compatible_surface;
    adapter_options.powerPreference = WGPUPowerPreference_HighPerformance;

    WGPURequestAdapterCallbackInfo callback_info = {};
    callback_info.mode = k_ImPlatform_WGPURequestCallbackMode;
    callback_info.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata1, void*)
    {
        ImPlatform_WGPU_AdapterRequest* req = (ImPlatform_WGPU_AdapterRequest*)userdata1;
        if (status == WGPURequestAdapterStatus_Success)
            req->adapter = adapter;
        else
            fprintf(stderr, "WebGPU: Failed to get adapter: %.*s\n", (int)message.length, message.data);
        req->done = true;
    };
    callback_info.userdata1 = &request;

    WGPUFuture future = wgpuInstanceRequestAdapter(instance, &adapter_options, callback_info);
    ImPlatform_WGPU_WaitRequest(instance, future, &request.done);
    return request.adapter;
}

static WGPUDevice ImPlatform_WGPU_RequestDevice(WGPUInstance instance, WGPUAdapter adapter)
{
    ImPlatform_WGPU_DeviceRequest request = { nullptr, false };

    WGPUDeviceDescriptor device_desc = {};
    device_desc.label = WGPU_STR("ImPlatform Device");
    device_desc.uncapturedErrorCallbackInfo.callback = ImPlatform_WGPU_UncapturedErrorCallback;
    device_desc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    device_desc.deviceLostCallbackInfo.callback = ImPlatform_WGPU_DeviceLostCallback;

    WGPURequestDeviceCallbackInfo callback_info = {};
    callback_info.mode = k_ImPlatform_WGPURequestCallbackMode;
    callback_info.callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* userdata1, void*)
    {
        ImPlatform_WGPU_DeviceRequest* req = (ImPlatform_WGPU_DeviceRequest*)userdata1;
        if (status == WGPURequestDeviceStatus_Success)
            req->device = device;
        else
            fprintf(stderr, "WebGPU: Failed to get device: %.*s\n", (int)message.length, message.data);
        req->done = true;
    };
    callback_info.userdata1 = &request;

    WGPUFuture future = wgpuAdapterRequestDevice(adapter, &device_desc, callback_info);
    ImPlatform_WGPU_WaitRequest(instance, future, &request.done);
    return request.device;
}

// Create a WGPUSurface for the application window
static WGPUSurface ImPlatform_WGPU_CreateSurface(WGPUInstance instance, void* pWindow)
{
#if defined(__EMSCRIPTEN__)
    // Emscripten: surface from HTML canvas element
    IM_UNUSED(pWindow);
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvas_desc = {};
    canvas_desc.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
    canvas_desc.selector = WGPU_STR("#canvas");

    WGPUSurfaceDescriptor surface_desc = {};
    surface_desc.nextInChain = &canvas_desc.chain;
    return wgpuInstanceCreateSurface(instance, &surface_desc);
#else
    ImGui_ImplWGPU_CreateSurfaceInfo create_info = {};
    create_info.Instance = instance;

  #if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    GLFWwindow* window = (GLFWwindow*)pWindow;
    #if defined(GLFW_EXPOSE_NATIVE_COCOA)
    create_info.System = "cocoa";
    create_info.RawWindow = (void*)glfwGetCocoaWindow(window);
    return ImGui_ImplWGPU_CreateWGPUSurfaceHelper(&create_info);
    #elif defined(GLFW_EXPOSE_NATIVE_WIN32)
    create_info.System = "win32";
    create_info.RawWindow = (void*)glfwGetWin32Window(window);
    create_info.RawInstance = (void*)::GetModuleHandle(nullptr);
    return ImGui_ImplWGPU_CreateWGPUSurfaceHelper(&create_info);
    #elif defined(GLFW_EXPOSE_NATIVE_X11)
      #if defined(GLFW_EXPOSE_NATIVE_WAYLAND) && (GLFW_VERSION_MAJOR > 3 || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 4))
    if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND)
    {
        create_info.System = "wayland";
        create_info.RawDisplay = (void*)glfwGetWaylandDisplay();
        create_info.RawSurface = (void*)glfwGetWaylandWindow(window);
        return ImGui_ImplWGPU_CreateWGPUSurfaceHelper(&create_info);
    }
      #endif
    create_info.System = "x11";
    create_info.RawWindow = (void*)glfwGetX11Window(window);
    create_info.RawDisplay = (void*)glfwGetX11Display();
    return ImGui_ImplWGPU_CreateWGPUSurfaceHelper(&create_info);
    #else
    IM_UNUSED(window);
    return nullptr;
    #endif

  #elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    create_info.System = "win32";
    create_info.RawWindow = pWindow;
    create_info.RawInstance = (void*)::GetModuleHandle(nullptr);
    return ImGui_ImplWGPU_CreateWGPUSurfaceHelper(&create_info);

  #elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
    SDL_SysWMinfo wm_info;
    SDL_VERSION(&wm_info.version);
    if (!SDL_GetWindowWMInfo((SDL_Window*)pWindow, &wm_info))
        return nullptr;
    #if defined(SDL_VIDEO_DRIVER_COCOA)
    create_info.System = "cocoa";
    create_info.RawWindow = (void*)wm_info.info.cocoa.window;
    return ImGui_ImplWGPU_CreateWGPUSurfaceHelper(&create_info);
    #elif defined(SDL_VIDEO_DRIVER_WAYLAND) || defined(SDL_VIDEO_DRIVER_X11)
    const char* sdl_driver = SDL_GetCurrentVideoDriver();
    if (sdl_driver && strcmp(sdl_driver, "wayland") == 0)
    {
        create_info.System = "wayland";
        create_info.RawDisplay = (void*)wm_info.info.wl.display;
        create_info.RawSurface = (void*)wm_info.info.wl.surface;
        return ImGui_ImplWGPU_CreateWGPUSurfaceHelper(&create_info);
    }
    create_info.System = "x11";
    create_info.RawWindow = (void*)wm_info.info.x11.window;
    create_info.RawDisplay = (void*)wm_info.info.x11.display;
    return ImGui_ImplWGPU_CreateWGPUSurfaceHelper(&create_info);
    #elif defined(SDL_VIDEO_DRIVER_WINDOWS)
    create_info.System = "win32";
    create_info.RawWindow = (void*)wm_info.info.win.window;
    create_info.RawInstance = (void*)wm_info.info.win.hinstance;
    return ImGui_ImplWGPU_CreateWGPUSurfaceHelper(&create_info);
    #else
    return nullptr;
    #endif

  #elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
    SDL_PropertiesID properties = SDL_GetWindowProperties((SDL_Window*)pWindow);
    #if defined(SDL_PLATFORM_MACOS)
    create_info.System = "cocoa";
    create_info.RawWindow = (void*)SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
    return ImGui_ImplWGPU_CreateWGPUSurfaceHelper(&create_info);
    #elif defined(SDL_PLATFORM_LINUX)
    if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "wayland") == 0)
    {
        create_info.System = "wayland";
        create_info.RawDisplay = (void*)SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
        create_info.RawSurface = (void*)SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
        return ImGui_ImplWGPU_CreateWGPUSurfaceHelper(&create_info);
    }
    create_info.System = "x11";
    create_info.RawWindow = (void*)(intptr_t)SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    create_info.RawDisplay = (void*)SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
    return ImGui_ImplWGPU_CreateWGPUSurfaceHelper(&create_info);
    #elif defined(SDL_PLATFORM_WIN32)
    create_info.System = "win32";
    create_info.RawWindow = (void*)SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    create_info.RawInstance = (void*)::GetModuleHandle(nullptr);
    return ImGui_ImplWGPU_CreateWGPUSurfaceHelper(&create_info);
    #else
    return nullptr;
    #endif

  #else
    IM_UNUSED(pWindow);
    return nullptr;
  #endif
#endif // __EMSCRIPTEN__
}

bool ImPlatform_Gfx_CreateDevice_WebGPU(void* pWindow, ImPlatform_GfxData_WebGPU* pData)
{
    // 1. Create WGPUInstance
    WGPUInstanceDescriptor instance_desc = {};
#ifndef __EMSCRIPTEN__
    static const WGPUInstanceFeatureName timed_wait_any = WGPUInstanceFeatureName_TimedWaitAny;
    instance_desc.requiredFeatureCount = 1;
    instance_desc.requiredFeatures = &timed_wait_any;
#endif
    pData->instance = wgpuCreateInstance(&instance_desc);
    if (!pData->instance)
    {
        fprintf(stderr, "WebGPU: Failed to create instance\n");
        return false;
    }

#if defined(IMGUI_IMPL_WEBGPU_BACKEND_WGPU) && !defined(__EMSCRIPTEN__)
    wgpuSetLogCallback([](WGPULogLevel level, WGPUStringView msg, void*) { fprintf(stderr, "WebGPU %s: %.*s\n", ImGui_ImplWGPU_GetLogLevelName(level), (int)msg.length, msg.data); }, nullptr);
    wgpuSetLogLevel(WGPULogLevel_Warn);
#endif

    // 2. Create surface from platform window
    pData->surface = ImPlatform_WGPU_CreateSurface(pData->instance, pWindow);
    if (!pData->surface)
    {
        fprintf(stderr, "WebGPU: Failed to create surface\n");
        return false;
    }

    // 3. Request adapter and device
    pData->adapter = ImPlatform_WGPU_RequestAdapter(pData->instance, pData->surface);
    if (!pData->adapter)
    {
        fprintf(stderr, "WebGPU: No suitable adapter found\n");
        return false;
    }

    pData->device = ImPlatform_WGPU_RequestDevice(pData->instance, pData->adapter);
    if (!pData->device)
    {
        fprintf(stderr, "WebGPU: Failed to create device\n");
        return false;
    }

    // 4. Get queue
    pData->queue = wgpuDeviceGetQueue(pData->device);

    // 5. Select surface format from the surface capabilities.
    // Prefer a non-sRGB 8-bit format: Dear ImGui colors are authored in gamma space.
    WGPUSurfaceCapabilities surface_capabilities = {};
    wgpuSurfaceGetCapabilities(pData->surface, pData->adapter, &surface_capabilities);
    pData->swapChainFormat = (surface_capabilities.formatCount > 0) ? surface_capabilities.formats[0] : WGPUTextureFormat_BGRA8Unorm;
    for (size_t n = 0; n < surface_capabilities.formatCount; n++)
    {
        if (surface_capabilities.formats[n] == WGPUTextureFormat_BGRA8Unorm || surface_capabilities.formats[n] == WGPUTextureFormat_RGBA8Unorm)
        {
            pData->swapChainFormat = surface_capabilities.formats[n];
            break;
        }
    }
    wgpuSurfaceCapabilitiesFreeMembers(surface_capabilities);

    return true;
}

// Internal API - Cleanup WebGPU device
void ImPlatform_Gfx_CleanupDevice_WebGPU(ImPlatform_GfxData_WebGPU* pData)
{
    ImPlatform_ReleaseAllTrackedTextures();

    if (pData->surfaceTexture.texture)
    {
        wgpuTextureRelease(pData->surfaceTexture.texture);
        pData->surfaceTexture.texture = nullptr;
    }

    if (pData->surface)
    {
        if (pData->device)
            wgpuSurfaceUnconfigure(pData->surface);
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

// ============================================================================
// ImPlatform API - Core Functions
// ============================================================================

// ImPlatform API - InitGfxAPI
IMPLATFORM_API bool ImPlatform_InitGfxAPI(void)
{
    void* pWindow = nullptr;
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    pWindow = ImPlatform_App_GetGLFWWindow();
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    pWindow = ImPlatform_App_GetHWND();
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
    pWindow = ImPlatform_App_GetSDL2Window();
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
    pWindow = ImPlatform_App_GetSDL3Window();
#endif

    if (!pWindow)
        return false;

    if (!ImPlatform_Gfx_CreateDevice_WebGPU(pWindow, &g_GfxData))
    {
        ImPlatform_Gfx_CleanupDevice_WebGPU(&g_GfxData);
        return false;
    }

    // Configure the surface
    unsigned int width = 0, height = 0;
    ImPlatform_GetFramebufferSize(&width, &height);
    if (width > 0 && height > 0)
        ImPlatform_ConfigureSurface(width, height);

    return true;
}

// ImPlatform API - InitGfx
IMPLATFORM_API bool ImPlatform_InitGfx(void)
{
    ImGui_ImplWGPU_InitInfo init_info;
    init_info.Device = g_GfxData.device;
    init_info.NumFramesInFlight = 3;
    init_info.RenderTargetFormat = g_GfxData.swapChainFormat;
    init_info.DepthStencilFormat = WGPUTextureFormat_Undefined;

    if (!ImGui_ImplWGPU_Init(&init_info))
        return false;

    return true;
}

// ImPlatform API - GfxCheck
// Handles surface resize and acquires the surface texture used for this frame.
IMPLATFORM_API bool ImPlatform_GfxCheck(void)
{
    // Release a texture acquired by a previous check which didn't reach presentation
    if (g_GfxData.surfaceTexture.texture)
    {
        wgpuTextureRelease(g_GfxData.surfaceTexture.texture);
        g_GfxData.surfaceTexture.texture = nullptr;
    }

    // React to changes in screen size
    unsigned int width = 0, height = 0;
    ImPlatform_GetFramebufferSize(&width, &height);

    // Skip rendering if minimized
    if (width == 0 || height == 0)
        return false;

    if (width != g_GfxData.uSurfaceWidth || height != g_GfxData.uSurfaceHeight)
        ImPlatform_ConfigureSurface(width, height);

    if (g_SurfaceLost)
        return false;

    // Check surface status for error. If texture is not optimal, try to reconfigure the surface.
    WGPUSurfaceTexture surface_texture = {};
    wgpuSurfaceGetCurrentTexture(g_GfxData.surface, &surface_texture);
    if (ImGui_ImplWGPU_IsSurfaceStatusError(surface_texture.status))
    {
        fprintf(stderr, "WebGPU: Unrecoverable surface texture status=%#.8x, rendering stopped\n", (unsigned int)surface_texture.status);
        if (surface_texture.texture)
            wgpuTextureRelease(surface_texture.texture);
        g_SurfaceLost = true;
        return false;
    }
    if (ImGui_ImplWGPU_IsSurfaceStatusSubOptimal(surface_texture.status))
    {
        if (surface_texture.texture)
            wgpuTextureRelease(surface_texture.texture);
        ImPlatform_ConfigureSurface(width, height);
        return false;
    }

    g_GfxData.surfaceTexture = surface_texture;
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
    // Clear is handled as part of the render pass in GfxAPIRender
    (void)vClearColor;
    return true;
}

// ImPlatform API - GfxAPIRender
IMPLATFORM_API bool ImPlatform_GfxAPIRender(ImVec4 const vClearColor)
{
    if (!g_GfxData.surfaceTexture.texture)
        return false;

    WGPUTextureViewDescriptor view_desc = {};
    view_desc.format = g_GfxData.swapChainFormat;
    view_desc.dimension = WGPUTextureViewDimension_2D;
    view_desc.mipLevelCount = WGPU_MIP_LEVEL_COUNT_UNDEFINED;
    view_desc.arrayLayerCount = WGPU_ARRAY_LAYER_COUNT_UNDEFINED;
    view_desc.aspect = WGPUTextureAspect_All;
    WGPUTextureView backbuffer = wgpuTextureCreateView(g_GfxData.surfaceTexture.texture, &view_desc);
    if (!backbuffer)
        return false;

    // Create command encoder
    WGPUCommandEncoderDescriptor enc_desc = {};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(g_GfxData.device, &enc_desc);

    // Begin render pass with clear
    WGPURenderPassColorAttachment color_attachment = {};
    color_attachment.view = backbuffer;
    color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    color_attachment.loadOp = WGPULoadOp_Clear;
    color_attachment.storeOp = WGPUStoreOp_Store;
    color_attachment.clearValue.r = vClearColor.x * vClearColor.w;
    color_attachment.clearValue.g = vClearColor.y * vClearColor.w;
    color_attachment.clearValue.b = vClearColor.z * vClearColor.w;
    color_attachment.clearValue.a = vClearColor.w;

    WGPURenderPassDescriptor render_pass_desc = {};
    render_pass_desc.colorAttachmentCount = 1;
    render_pass_desc.colorAttachments = &color_attachment;
    render_pass_desc.depthStencilAttachment = nullptr;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &render_pass_desc);

    // Render ImGui
    ImDrawData* draw_data = ImGui::GetDrawData();
    g_CurrentDrawData = draw_data;
    ImGui_ImplWGPU_RenderDrawData(draw_data, pass);

    wgpuRenderPassEncoderEnd(pass);

    // Submit commands
    WGPUCommandBufferDescriptor cmd_desc = {};
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, &cmd_desc);
    wgpuQueueSubmit(g_GfxData.queue, 1, &cmd);

    // Cleanup per-frame resources
    wgpuCommandBufferRelease(cmd);
    wgpuRenderPassEncoderRelease(pass);
    wgpuCommandEncoderRelease(encoder);
    wgpuTextureViewRelease(backbuffer);

    return true;
}

// ImPlatform API - GfxViewportPre
#ifdef IMGUI_HAS_VIEWPORT
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
#endif

// ImPlatform API - GfxAPISwapBuffer
IMPLATFORM_API bool ImPlatform_GfxAPISwapBuffer(void)
{
    if (!g_GfxData.surfaceTexture.texture)
        return false;

#ifndef __EMSCRIPTEN__
    wgpuSurfacePresent(g_GfxData.surface);
  #if defined(IMGUI_IMPL_WEBGPU_BACKEND_DAWN)
    // Tick needs to be called in Dawn to display validation errors
    wgpuDeviceTick(g_GfxData.device);
  #endif
#endif
    wgpuTextureRelease(g_GfxData.surfaceTexture.texture);
    g_GfxData.surfaceTexture.texture = nullptr;
    return true;
}

// ImPlatform API - ShutdownGfxAPI
IMPLATFORM_API void ImPlatform_ShutdownGfxAPI(void)
{
    if (g_DefaultSampler)
    {
        wgpuSamplerRelease(g_DefaultSampler);
        g_DefaultSampler = nullptr;
    }
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
#if IMPLATFORM_GFX_SUPPORT_BGRA_FORMATS
    case ImPlatform_PixelFormat_BGRA8:
        *out_bytes_per_pixel = 4;
        return WGPUTextureFormat_BGRA8Unorm;
#endif
#if IMPLATFORM_GFX_SUPPORT_HALF_FLOAT_FORMATS
    case ImPlatform_PixelFormat_R16F:
        *out_bytes_per_pixel = 2;
        return WGPUTextureFormat_R16Float;
    case ImPlatform_PixelFormat_RG16F:
        *out_bytes_per_pixel = 4;
        return WGPUTextureFormat_RG16Float;
    case ImPlatform_PixelFormat_RGBA16F:
        *out_bytes_per_pixel = 8;
        return WGPUTextureFormat_RGBA16Float;
#endif
    // No IMPLATFORM_GFX_SUPPORT_RGB_EXTENDED — WebGPU has no 3-channel texture formats
#if IMPLATFORM_GFX_SUPPORT_SRGB_FORMATS
    case ImPlatform_PixelFormat_RGB8_SRGB:
        // WebGPU has no 3-channel sRGB — use RGBA8 sRGB
        *out_bytes_per_pixel = 4;
        return WGPUTextureFormat_RGBA8UnormSrgb;
    case ImPlatform_PixelFormat_RGBA8_SRGB:
        *out_bytes_per_pixel = 4;
        return WGPUTextureFormat_RGBA8UnormSrgb;
#endif
#if IMPLATFORM_GFX_SUPPORT_PACKED_FORMATS
    case ImPlatform_PixelFormat_RGB10A2:
        *out_bytes_per_pixel = 4;
        return WGPUTextureFormat_RGB10A2Unorm;
#endif
#if IMPLATFORM_GFX_SUPPORT_DEPTH_FORMATS
    case ImPlatform_PixelFormat_D16:
        *out_bytes_per_pixel = 2;
        return WGPUTextureFormat_Depth16Unorm;
    case ImPlatform_PixelFormat_D32F:
        *out_bytes_per_pixel = 4;
        return WGPUTextureFormat_Depth32Float;
    case ImPlatform_PixelFormat_D24S8:
        *out_bytes_per_pixel = 4;
        return WGPUTextureFormat_Depth24PlusStencil8;
    case ImPlatform_PixelFormat_D32FS8:
        *out_bytes_per_pixel = 8;
        return WGPUTextureFormat_Depth32FloatStencil8;
#endif
#if IMPLATFORM_GFX_SUPPORT_INTEGER_FORMATS
    case ImPlatform_PixelFormat_R8UI:
        *out_bytes_per_pixel = 1;
        return WGPUTextureFormat_R8Uint;
    case ImPlatform_PixelFormat_R8I:
        *out_bytes_per_pixel = 1;
        return WGPUTextureFormat_R8Sint;
    case ImPlatform_PixelFormat_R16UI:
        *out_bytes_per_pixel = 2;
        return WGPUTextureFormat_R16Uint;
    case ImPlatform_PixelFormat_R16I:
        *out_bytes_per_pixel = 2;
        return WGPUTextureFormat_R16Sint;
    case ImPlatform_PixelFormat_R32UI:
        *out_bytes_per_pixel = 4;
        return WGPUTextureFormat_R32Uint;
    case ImPlatform_PixelFormat_R32I:
        *out_bytes_per_pixel = 4;
        return WGPUTextureFormat_R32Sint;
#endif
    default:
        *out_bytes_per_pixel = 4;
        return WGPUTextureFormat_RGBA8Unorm;
    }
}

static int ImPlatform_GetBytesPerPixelFromWGPUFormat(WGPUTextureFormat format)
{
    switch (format)
    {
    case WGPUTextureFormat_R8Unorm:           return 1;
    case WGPUTextureFormat_RG8Unorm:          return 2;
    case WGPUTextureFormat_RGBA8Unorm:        return 4;
    case WGPUTextureFormat_R16Unorm:          return 2;
    case WGPUTextureFormat_RG16Unorm:         return 4;
    case WGPUTextureFormat_RGBA16Unorm:       return 8;
    case WGPUTextureFormat_R32Float:          return 4;
    case WGPUTextureFormat_RG32Float:         return 8;
    case WGPUTextureFormat_RGBA32Float:       return 16;
    case WGPUTextureFormat_BGRA8Unorm:        return 4;
    case WGPUTextureFormat_R16Float:          return 2;
    case WGPUTextureFormat_RG16Float:         return 4;
    case WGPUTextureFormat_RGBA16Float:       return 8;
    case WGPUTextureFormat_RGBA8UnormSrgb:    return 4;
    case WGPUTextureFormat_RGB10A2Unorm:      return 4;
    case WGPUTextureFormat_Depth16Unorm:      return 2;
    case WGPUTextureFormat_Depth32Float:      return 4;
    case WGPUTextureFormat_Depth24PlusStencil8:  return 4;
    case WGPUTextureFormat_Depth32FloatStencil8: return 8;
    case WGPUTextureFormat_R8Uint:            return 1;
    case WGPUTextureFormat_R8Sint:            return 1;
    case WGPUTextureFormat_R16Uint:           return 2;
    case WGPUTextureFormat_R16Sint:           return 2;
    case WGPUTextureFormat_R32Uint:           return 4;
    case WGPUTextureFormat_R32Sint:           return 4;
    default:                                  return 4;
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

// Texture3D stub for WebGPU.
IMPLATFORM_API ImPlatform_TextureDesc3D ImPlatform_TextureDesc3D_Default(unsigned int w, unsigned int h, unsigned int d)
{
    ImPlatform_TextureDesc3D x; x.width = w; x.height = h; x.depth = d;
    x.format = ImPlatform_PixelFormat_RGBA8;
    x.min_filter = ImPlatform_TextureFilter_Linear; x.mag_filter = ImPlatform_TextureFilter_Linear;
    x.wrap_u = x.wrap_v = x.wrap_w = ImPlatform_TextureWrap_Clamp;
    return x;
}
IMPLATFORM_API bool ImPlatform_SupportsTexture3D(void) { return false; }
IMPLATFORM_API ImTextureID ImPlatform_CreateTexture3D(const void*, const ImPlatform_TextureDesc3D*) { return NULL; }

IMPLATFORM_API ImTextureID ImPlatform_CreateTexture(const void* pixel_data, const ImPlatform_TextureDesc* desc)
{
    if (!desc || !pixel_data || !g_GfxData.device)
        return NULL;

    int bytes_per_pixel;
    WGPUTextureFormat format = ImPlatform_GetWebGPUFormat(desc->format, &bytes_per_pixel);

    // Handle RGB8 → RGBA8 conversion
    const void* upload_data = pixel_data;
    unsigned char* converted_data = nullptr;

    if (desc->format == ImPlatform_PixelFormat_RGB8)
    {
        size_t pixel_count = (size_t)desc->width * desc->height;
        converted_data = (unsigned char*)malloc(pixel_count * 4);
        const unsigned char* src = (const unsigned char*)pixel_data;
        for (size_t i = 0; i < pixel_count; i++)
        {
            converted_data[i * 4 + 0] = src[i * 3 + 0];
            converted_data[i * 4 + 1] = src[i * 3 + 1];
            converted_data[i * 4 + 2] = src[i * 3 + 2];
            converted_data[i * 4 + 3] = 255;
        }
        upload_data = converted_data;
    }

    // Create texture
    WGPUTextureDescriptor tex_desc = {};
    tex_desc.label = WGPU_STR("ImPlatform Texture");
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
    {
        free(converted_data);
        return NULL;
    }

    // Upload texture data
    WGPUTexelCopyTextureInfo dst = {};
    dst.texture = texture;
    dst.mipLevel = 0;
    dst.origin = {0, 0, 0};
    dst.aspect = WGPUTextureAspect_All;

    WGPUTexelCopyBufferLayout layout = {};
    layout.offset = 0;
    layout.bytesPerRow = desc->width * bytes_per_pixel;
    layout.rowsPerImage = desc->height;

    WGPUExtent3D writeSize = {};
    writeSize.width = desc->width;
    writeSize.height = desc->height;
    writeSize.depthOrArrayLayers = 1;

    size_t data_size = (size_t)desc->width * desc->height * bytes_per_pixel;
    wgpuQueueWriteTexture(g_GfxData.queue, &dst, upload_data, data_size, &layout, &writeSize);

    free(converted_data);

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

    // Track for proper cleanup
    ImPlatform_TrackTexture(texture, texture_view, sampler, desc->width, desc->height, desc->format);

    return (ImTextureID)texture_view;
}

IMPLATFORM_API bool ImPlatform_UpdateTexture(ImTextureID texture_id, const void* pixel_data,
                                              unsigned int x, unsigned int y,
                                              unsigned int width, unsigned int height)
{
    if (!texture_id || !pixel_data || !g_GfxData.queue)
        return false;

    WGPUTextureView view = (WGPUTextureView)texture_id;
    ImPlatform_TextureTracking_WebGPU* tracking = ImPlatform_FindTrackedTexture(view);
    if (!tracking)
        return false;

    int bytes_per_pixel;
    WGPUTextureFormat format = ImPlatform_GetWebGPUFormat(tracking->format, &bytes_per_pixel);

    // Handle RGB8 → RGBA8 conversion
    const void* upload_data = pixel_data;
    unsigned char* converted_data = nullptr;

    if (tracking->format == ImPlatform_PixelFormat_RGB8)
    {
        size_t pixel_count = (size_t)width * height;
        converted_data = (unsigned char*)malloc(pixel_count * 4);
        const unsigned char* src = (const unsigned char*)pixel_data;
        for (size_t i = 0; i < pixel_count; i++)
        {
            converted_data[i * 4 + 0] = src[i * 3 + 0];
            converted_data[i * 4 + 1] = src[i * 3 + 1];
            converted_data[i * 4 + 2] = src[i * 3 + 2];
            converted_data[i * 4 + 3] = 255;
        }
        upload_data = converted_data;
    }

    WGPUTexelCopyTextureInfo dst = {};
    dst.texture = tracking->texture;
    dst.mipLevel = 0;
    dst.origin = {x, y, 0};
    dst.aspect = WGPUTextureAspect_All;

    WGPUTexelCopyBufferLayout layout = {};
    layout.offset = 0;
    layout.bytesPerRow = width * bytes_per_pixel;
    layout.rowsPerImage = height;

    WGPUExtent3D writeSize = {};
    writeSize.width = width;
    writeSize.height = height;
    writeSize.depthOrArrayLayers = 1;

    size_t data_size = (size_t)width * height * bytes_per_pixel;
    wgpuQueueWriteTexture(g_GfxData.queue, &dst, upload_data, data_size, &layout, &writeSize);

    free(converted_data);
    return true;
}

IMPLATFORM_API ImTextureID ImPlatform_CreateRenderTexture(const ImPlatform_TextureDesc* desc)
{
    if (!desc || !g_GfxData.device)
        return NULL;

    int bytes_per_pixel;
    WGPUTextureFormat format = ImPlatform_GetWebGPUFormat(desc->format, &bytes_per_pixel);

    WGPUTextureDescriptor tex_desc = {};
    tex_desc.label                    = WGPU_STR("ImPlatform RenderTexture");
    tex_desc.dimension                = WGPUTextureDimension_2D;
    tex_desc.size.width               = desc->width;
    tex_desc.size.height              = desc->height;
    tex_desc.size.depthOrArrayLayers  = 1;
    tex_desc.mipLevelCount            = 1;
    tex_desc.sampleCount              = 1;
    tex_desc.format                   = format;
    tex_desc.usage                    = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_RenderAttachment;

    WGPUTexture texture = wgpuDeviceCreateTexture(g_GfxData.device, &tex_desc);
    if (!texture)
        return NULL;

    WGPUTextureViewDescriptor view_desc = {};
    view_desc.format          = format;
    view_desc.dimension       = WGPUTextureViewDimension_2D;
    view_desc.mipLevelCount   = 1;
    view_desc.arrayLayerCount = 1;
    view_desc.aspect          = WGPUTextureAspect_All;

    WGPUTextureView texture_view = wgpuTextureCreateView(texture, &view_desc);
    if (!texture_view)
    {
        wgpuTextureRelease(texture);
        return NULL;
    }

    WGPUSamplerDescriptor sampler_desc = {};
    sampler_desc.addressModeU = WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeV = WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeW = WGPUAddressMode_ClampToEdge;
    sampler_desc.magFilter    = WGPUFilterMode_Linear;
    sampler_desc.minFilter    = WGPUFilterMode_Linear;
    sampler_desc.mipmapFilter = WGPUMipmapFilterMode_Linear;
    sampler_desc.lodMaxClamp  = 1000.0f;
    sampler_desc.maxAnisotropy = 1;

    WGPUSampler sampler = wgpuDeviceCreateSampler(g_GfxData.device, &sampler_desc);
    if (!sampler)
    {
        wgpuTextureViewRelease(texture_view);
        wgpuTextureRelease(texture);
        return NULL;
    }

    ImPlatform_TrackTexture(texture, texture_view, sampler, desc->width, desc->height, desc->format);

    return (ImTextureID)texture_view;
}

IMPLATFORM_API bool ImPlatform_BeginRenderToTexture(ImTextureID texture)
{
    if (!texture || !g_GfxData.device)
        return false;

    WGPUTextureView view = (WGPUTextureView)texture;
    ImPlatform_TextureTracking_WebGPU* tracking = ImPlatform_FindTrackedTexture(view);
    if (!tracking) return false;

    WGPUCommandEncoderDescriptor enc_desc = {};
    g_RTEncoder = wgpuDeviceCreateCommandEncoder(g_GfxData.device, &enc_desc);

    WGPURenderPassColorAttachment color_attachment = {};
    color_attachment.view              = view;
    color_attachment.depthSlice        = WGPU_DEPTH_SLICE_UNDEFINED;
    color_attachment.loadOp            = WGPULoadOp_Clear;
    color_attachment.storeOp           = WGPUStoreOp_Store;
    color_attachment.clearValue.r      = 0.0;
    color_attachment.clearValue.g      = 0.0;
    color_attachment.clearValue.b      = 0.0;
    color_attachment.clearValue.a      = 0.0;

    WGPURenderPassDescriptor pass_desc = {};
    pass_desc.colorAttachmentCount    = 1;
    pass_desc.colorAttachments        = &color_attachment;

    g_RTPass = wgpuCommandEncoderBeginRenderPass(g_RTEncoder, &pass_desc);

    return true;
}

IMPLATFORM_API void ImPlatform_EndRenderToTexture(void)
{
    if (!g_RTPass || !g_RTEncoder) return;

    wgpuRenderPassEncoderEnd(g_RTPass);

    WGPUCommandBufferDescriptor cmd_desc = {};
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(g_RTEncoder, &cmd_desc);
    wgpuQueueSubmit(g_GfxData.queue, 1, &cmd);

    wgpuCommandBufferRelease(cmd);
    wgpuRenderPassEncoderRelease(g_RTPass);
    wgpuCommandEncoderRelease(g_RTEncoder);
    g_RTPass    = nullptr;
    g_RTEncoder = nullptr;
}

IMPLATFORM_API bool ImPlatform_CopyBackbuffer(ImTextureID dst) { (void)dst; return false; }
IMPLATFORM_API void ImPlatform_GetBackbufferSize(unsigned int* width, unsigned int* height)
{
    if (width)  *width  = g_ImPlatform_BackbufferW;
    if (height) *height = g_ImPlatform_BackbufferH;
}

IMPLATFORM_API bool ImPlatform_CopyTexture(ImTextureID dst, ImTextureID src)
{
    if (!dst || !src || !g_GfxData.device)
        return false;

    WGPUTextureView dstView = (WGPUTextureView)dst;
    WGPUTextureView srcView = (WGPUTextureView)src;

    ImPlatform_TextureTracking_WebGPU* dstTracking = ImPlatform_FindTrackedTexture(dstView);
    ImPlatform_TextureTracking_WebGPU* srcTracking = ImPlatform_FindTrackedTexture(srcView);
    if (!dstTracking || !srcTracking)
        return false;

    WGPUCommandEncoderDescriptor encDesc = {};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(g_GfxData.device, &encDesc);
    if (!encoder)
        return false;

    WGPUTexelCopyTextureInfo srcCopy = {};
    srcCopy.texture = srcTracking->texture;
    srcCopy.mipLevel = 0;
    srcCopy.origin = { 0, 0, 0 };

    WGPUTexelCopyTextureInfo dstCopy = {};
    dstCopy.texture = dstTracking->texture;
    dstCopy.mipLevel = 0;
    dstCopy.origin = { 0, 0, 0 };

    WGPUExtent3D extent = { srcTracking->width, srcTracking->height, 1 };
    wgpuCommandEncoderCopyTextureToTexture(encoder, &srcCopy, &dstCopy, &extent);

    WGPUCommandBufferDescriptor cbDesc = {};
    WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(encoder, &cbDesc);
    wgpuQueueSubmit(g_GfxData.queue, 1, &cmdBuf);

    wgpuCommandBufferRelease(cmdBuf);
    wgpuCommandEncoderRelease(encoder);
    return true;
}

IMPLATFORM_API void ImPlatform_DestroyTexture(ImTextureID texture_id)
{
    if (!texture_id)
        return;

    WGPUTextureView view = (WGPUTextureView)texture_id;
    ImPlatform_TextureTracking_WebGPU* tracking = ImPlatform_FindTrackedTexture(view);
    if (tracking)
    {
        wgpuSamplerRelease(tracking->sampler);
        wgpuTextureViewRelease(tracking->textureView);
        wgpuTextureRelease(tracking->texture);
        ImPlatform_UntrackTexture(view);
    }
    else
    {
        // Fallback: just release the view
        wgpuTextureViewRelease(view);
    }
}

// ============================================================================
// Custom Vertex/Index Buffer Management API - WebGPU
// ============================================================================

struct ImPlatform_VertexBufferData_WebGPU
{
    WGPUBuffer buffer;
    ImPlatform_VertexBufferDesc desc;
    ImPlatform_VertexAttribute* attributes;
};

struct ImPlatform_IndexBufferData_WebGPU
{
    WGPUBuffer buffer;
    ImPlatform_IndexBufferDesc desc;
};

// Currently bound buffers (for DrawIndexed)
static ImPlatform_VertexBufferData_WebGPU* g_BoundVertexBuffer = nullptr;
static ImPlatform_IndexBufferData_WebGPU* g_BoundIndexBuffer = nullptr;

static WGPUBufferUsage ImPlatform_GetWGPUBufferUsage(ImPlatform_BufferUsage usage, WGPUBufferUsage bind)
{
    WGPUBufferUsage flags = bind | WGPUBufferUsage_CopyDst;
    (void)usage; // WebGPU doesn't distinguish Static/Dynamic/Stream in buffer creation
    return flags;
}

IMPLATFORM_API ImPlatform_VertexBuffer ImPlatform_CreateVertexBuffer(const void* vertex_data, const ImPlatform_VertexBufferDesc* desc)
{
    if (!desc || !vertex_data || !g_GfxData.device)
        return NULL;

    ImPlatform_VertexBufferData_WebGPU* vb = new ImPlatform_VertexBufferData_WebGPU();
    memset(vb, 0, sizeof(*vb));
    vb->desc = *desc;

    // Copy attribute array
    if (desc->attribute_count > 0 && desc->attributes)
    {
        vb->attributes = new ImPlatform_VertexAttribute[desc->attribute_count];
        memcpy(vb->attributes, desc->attributes, sizeof(ImPlatform_VertexAttribute) * desc->attribute_count);
        vb->desc.attributes = vb->attributes;
    }

    size_t byte_size = (size_t)desc->vertex_count * desc->vertex_stride;

    WGPUBufferDescriptor buf_desc = {};
    buf_desc.usage = ImPlatform_GetWGPUBufferUsage(desc->usage, WGPUBufferUsage_Vertex);
    buf_desc.size = byte_size;
    buf_desc.mappedAtCreation = true;

    vb->buffer = wgpuDeviceCreateBuffer(g_GfxData.device, &buf_desc);
    if (!vb->buffer)
    {
        delete[] vb->attributes;
        delete vb;
        return NULL;
    }

    // Copy initial data
    void* mapped = wgpuBufferGetMappedRange(vb->buffer, 0, byte_size);
    if (mapped)
        memcpy(mapped, vertex_data, byte_size);
    wgpuBufferUnmap(vb->buffer);

    return (ImPlatform_VertexBuffer)vb;
}

IMPLATFORM_API bool ImPlatform_UpdateVertexBuffer(ImPlatform_VertexBuffer vertex_buffer, const void* vertex_data, unsigned int vertex_count, unsigned int offset)
{
    if (!vertex_buffer || !vertex_data || !g_GfxData.queue)
        return false;

    ImPlatform_VertexBufferData_WebGPU* vb = (ImPlatform_VertexBufferData_WebGPU*)vertex_buffer;
    size_t byte_offset = (size_t)offset * vb->desc.vertex_stride;
    size_t byte_size = (size_t)vertex_count * vb->desc.vertex_stride;

    wgpuQueueWriteBuffer(g_GfxData.queue, vb->buffer, byte_offset, vertex_data, byte_size);
    return true;
}

IMPLATFORM_API void ImPlatform_DestroyVertexBuffer(ImPlatform_VertexBuffer vertex_buffer)
{
    if (!vertex_buffer)
        return;

    ImPlatform_VertexBufferData_WebGPU* vb = (ImPlatform_VertexBufferData_WebGPU*)vertex_buffer;
    if (vb->buffer) wgpuBufferRelease(vb->buffer);
    delete[] vb->attributes;
    delete vb;
}

IMPLATFORM_API ImPlatform_IndexBuffer ImPlatform_CreateIndexBuffer(const void* index_data, const ImPlatform_IndexBufferDesc* desc)
{
    if (!desc || !index_data || !g_GfxData.device)
        return NULL;

    ImPlatform_IndexBufferData_WebGPU* ib = new ImPlatform_IndexBufferData_WebGPU();
    memset(ib, 0, sizeof(*ib));
    ib->desc = *desc;

    unsigned int index_size = (desc->format == ImPlatform_IndexFormat_UInt16) ? sizeof(uint16_t) : sizeof(uint32_t);
    size_t byte_size = (size_t)desc->index_count * index_size;

    WGPUBufferDescriptor buf_desc = {};
    buf_desc.usage = ImPlatform_GetWGPUBufferUsage(desc->usage, WGPUBufferUsage_Index);
    buf_desc.size = byte_size;
    buf_desc.mappedAtCreation = true;

    ib->buffer = wgpuDeviceCreateBuffer(g_GfxData.device, &buf_desc);
    if (!ib->buffer)
    {
        delete ib;
        return NULL;
    }

    void* mapped = wgpuBufferGetMappedRange(ib->buffer, 0, byte_size);
    if (mapped)
        memcpy(mapped, index_data, byte_size);
    wgpuBufferUnmap(ib->buffer);

    return (ImPlatform_IndexBuffer)ib;
}

IMPLATFORM_API bool ImPlatform_UpdateIndexBuffer(ImPlatform_IndexBuffer index_buffer, const void* index_data, unsigned int index_count, unsigned int offset)
{
    if (!index_buffer || !index_data || !g_GfxData.queue)
        return false;

    ImPlatform_IndexBufferData_WebGPU* ib = (ImPlatform_IndexBufferData_WebGPU*)index_buffer;
    unsigned int index_size = (ib->desc.format == ImPlatform_IndexFormat_UInt16) ? sizeof(uint16_t) : sizeof(uint32_t);
    size_t byte_offset = (size_t)offset * index_size;
    size_t byte_size = (size_t)index_count * index_size;

    wgpuQueueWriteBuffer(g_GfxData.queue, ib->buffer, byte_offset, index_data, byte_size);
    return true;
}

IMPLATFORM_API void ImPlatform_DestroyIndexBuffer(ImPlatform_IndexBuffer index_buffer)
{
    if (!index_buffer)
        return;

    ImPlatform_IndexBufferData_WebGPU* ib = (ImPlatform_IndexBufferData_WebGPU*)index_buffer;
    if (ib->buffer) wgpuBufferRelease(ib->buffer);
    delete ib;
}

IMPLATFORM_API void ImPlatform_BindVertexBuffer(ImPlatform_VertexBuffer vertex_buffer)
{
    g_BoundVertexBuffer = vertex_buffer ? (ImPlatform_VertexBufferData_WebGPU*)vertex_buffer : nullptr;
}

IMPLATFORM_API void ImPlatform_BindIndexBuffer(ImPlatform_IndexBuffer index_buffer)
{
    g_BoundIndexBuffer = index_buffer ? (ImPlatform_IndexBufferData_WebGPU*)index_buffer : nullptr;
}

IMPLATFORM_API void ImPlatform_DrawIndexed(unsigned int primitive_type, unsigned int index_count, unsigned int start_index)
{
    // WebGPU draw commands require a render pass encoder.
    // Get the current render pass from ImGui's render state (available during draw callbacks).
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    ImGui_ImplWGPU_RenderState* render_state = (ImGui_ImplWGPU_RenderState*)platform_io.Renderer_RenderState;
    if (!render_state || !render_state->RenderPassEncoder)
        return;

    WGPURenderPassEncoder pass = render_state->RenderPassEncoder;

    if (g_BoundVertexBuffer && g_BoundVertexBuffer->buffer)
    {
        size_t vb_size = (size_t)g_BoundVertexBuffer->desc.vertex_count * g_BoundVertexBuffer->desc.vertex_stride;
        wgpuRenderPassEncoderSetVertexBuffer(pass, 0, g_BoundVertexBuffer->buffer, 0, vb_size);
    }

    if (g_BoundIndexBuffer && g_BoundIndexBuffer->buffer)
    {
        WGPUIndexFormat fmt = (g_BoundIndexBuffer->desc.format == ImPlatform_IndexFormat_UInt16)
            ? WGPUIndexFormat_Uint16 : WGPUIndexFormat_Uint32;
        unsigned int index_size = (g_BoundIndexBuffer->desc.format == ImPlatform_IndexFormat_UInt16) ? 2 : 4;
        size_t ib_size = (size_t)g_BoundIndexBuffer->desc.index_count * index_size;
        wgpuRenderPassEncoderSetIndexBuffer(pass, g_BoundIndexBuffer->buffer, fmt, 0, ib_size);
        wgpuRenderPassEncoderDrawIndexed(pass, index_count, 1, start_index, 0, 0);
    }
}

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
    size_t uniformBufferCapacity; // Allocated size of uniformBuffer (aligned)
    void* uniformData;       // Custom uniforms (not including projection matrix)
    size_t uniformDataSize;
    bool uniformDataDirty;
    char* vertexEntryPoint;
    char* fragmentEntryPoint;
};

// Caching strategy:
//   Verified against imgui_impl_wgpu.h and the current webgpu.h headers
//   bundled with this project: no pipeline cache type, no binary archive
//   type, no serialization/deserialization entry points exist. Concretely:
//
//     (a) WGSL has no standardized binary representation -- the spec only
//         defines a textual source form, so there is nothing analogous to
//         SPIR-V or DXBC to persist at the shader-module level.
//     (b) The WebGPU spec currently has no pipeline cache extension. There
//         is no WGPUPipelineCache, no WGPUBinaryArchive, and no equivalent
//         of VK_EXT_pipeline_cache / MTLBinaryArchive / glProgramBinary.
//     (c) Browsers (Chrome's Dawn backend, Firefox's wgpu-core backend) may
//         cache compiled pipelines internally across page loads, but that
//         cache is opaque to the application -- there is no JS/C API to
//         opt in to it, inspect it, or persist it to application storage.
//     (d) wgpu-native and Dawn both expose native device handles that
//         *could* be bridged to VkPipelineCache on Vulkan-backed builds,
//         but that escape hatch is backend-specific and not part of the
//         portable ImPlatform WebGPU path.
//
//   Consequence: the `cache_key` and `compile_flags` fields of
//   ImPlatform_ShaderDesc are silently ignored on this backend. When (and
//   if) a standardized WGPUPipelineCache ever lands, wire it up here the
//   same way ImPlatform_gfx_vulkan.cpp wires VkPipelineCache: create one at
//   device-create, pass it into every wgpuDeviceCreateRenderPipeline call,
//   serialize it back on cleanup. Until then, this is a documented no-op.
IMPLATFORM_API ImPlatform_Shader ImPlatform_CreateShader(const ImPlatform_ShaderDesc* desc)
{
    if (!desc || !desc->source_code || !g_GfxData.device)
        return NULL;

    // Create WGSL shader module
    WGPUShaderSourceWGSL wgsl_desc = {};
    wgsl_desc.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgsl_desc.code = WGPU_STR(desc->source_code);

    WGPUShaderModuleDescriptor module_desc = {};
    module_desc.nextInChain = (WGPUChainedStruct*)&wgsl_desc;
    module_desc.label = WGPU_STR("ImPlatform Custom Shader");

    WGPUShaderModule shader_module = wgpuDeviceCreateShaderModule(g_GfxData.device, &module_desc);
    if (!shader_module)
        return NULL;

    ImPlatform_ShaderData_WebGPU* shader_data = new ImPlatform_ShaderData_WebGPU();
    shader_data->shaderModule = shader_module;
    shader_data->stage = desc->stage;

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
        wgpuShaderModuleRelease(shader_data->shaderModule);
    if (shader_data->entryPoint)
        free(shader_data->entryPoint);

    delete shader_data;
}

IMPLATFORM_API ImPlatform_ShaderProgram ImPlatform_CreateShaderProgram(ImPlatform_Shader vertex_shader, ImPlatform_Shader fragment_shader)
{
    if (!vertex_shader || !fragment_shader || !g_GfxData.device)
        return NULL;

    ImPlatform_ShaderData_WebGPU* vs_data = (ImPlatform_ShaderData_WebGPU*)vertex_shader;
    ImPlatform_ShaderData_WebGPU* fs_data = (ImPlatform_ShaderData_WebGPU*)fragment_shader;

    // Create bind group layout:
    // binding 0: uniform buffer (VS+FS) - projection matrix + custom uniforms
    // binding 1: sampler (FS)
    // binding 2: texture (FS)
    WGPUBindGroupLayoutEntry bg_layout_entries[3] = {};

    bg_layout_entries[0].binding = 0;
    bg_layout_entries[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    bg_layout_entries[0].buffer.type = WGPUBufferBindingType_Uniform;
    bg_layout_entries[0].buffer.minBindingSize = 0;

    bg_layout_entries[1].binding = 1;
    bg_layout_entries[1].visibility = WGPUShaderStage_Fragment;
    bg_layout_entries[1].sampler.type = WGPUSamplerBindingType_Filtering;

    bg_layout_entries[2].binding = 2;
    bg_layout_entries[2].visibility = WGPUShaderStage_Fragment;
    bg_layout_entries[2].texture.sampleType = WGPUTextureSampleType_Float;
    bg_layout_entries[2].texture.viewDimension = WGPUTextureViewDimension_2D;

    WGPUBindGroupLayoutDescriptor bg_layout_desc = {};
    bg_layout_desc.entryCount = 3;
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

    // Create vertex state matching ImDrawVert layout
    WGPUVertexAttribute vertex_attributes[3] = {};

    // Position (float2) - offset 0
    vertex_attributes[0].format = WGPUVertexFormat_Float32x2;
    vertex_attributes[0].offset = 0;
    vertex_attributes[0].shaderLocation = 0;

    // UV (float2) - offset 8
    vertex_attributes[1].format = WGPUVertexFormat_Float32x2;
    vertex_attributes[1].offset = 8;
    vertex_attributes[1].shaderLocation = 1;

    // Color (unorm8x4) - offset 16
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
    vertex_state.entryPoint = WGPU_STR(vs_data->entryPoint);
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
    fragment_state.entryPoint = WGPU_STR(fs_data->entryPoint);
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
    memset(program_data, 0, sizeof(*program_data));
    program_data->vertexModule = vs_data->shaderModule;
    program_data->fragmentModule = fs_data->shaderModule;
    program_data->renderPipeline = render_pipeline;
    program_data->bindGroupLayout = bind_group_layout;

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

    if (program_data->bindGroup) wgpuBindGroupRelease(program_data->bindGroup);
    if (program_data->uniformBuffer) wgpuBufferRelease(program_data->uniformBuffer);
    if (program_data->bindGroupLayout) wgpuBindGroupLayoutRelease(program_data->bindGroupLayout);
    if (program_data->renderPipeline) wgpuRenderPipelineRelease(program_data->renderPipeline);
    if (program_data->uniformData) free(program_data->uniformData);
    if (program_data->vertexEntryPoint) free(program_data->vertexEntryPoint);
    if (program_data->fragmentEntryPoint) free(program_data->fragmentEntryPoint);

    delete program_data;
}

IMPLATFORM_API void ImPlatform_UseShaderProgram(ImPlatform_ShaderProgram program)
{
    // WebGPU doesn't have a global "use program" concept
    // Shaders are bound per render pass via callbacks
    (void)program;
}

IMPLATFORM_API bool ImPlatform_SetShaderUniform(ImPlatform_ShaderProgram program, const char* /*name*/, const void* data, unsigned int size)
{
    if (!program || !data || size == 0)
        return false;

    ImPlatform_ShaderProgramData_WebGPU* program_data = (ImPlatform_ShaderProgramData_WebGPU*)program;

    if (!program_data->uniformData || program_data->uniformDataSize != size)
    {
        if (program_data->uniformData)
            free(program_data->uniformData);
        program_data->uniformData = malloc(size);
        if (!program_data->uniformData)
            return false;
        program_data->uniformDataSize = size;
    }

    memcpy(program_data->uniformData, data, size);
    program_data->uniformDataDirty = true;
    return true;
}

IMPLATFORM_API bool ImPlatform_SetShaderTexture(ImPlatform_ShaderProgram program, const char* name, unsigned int slot, ImTextureID texture)
{
    // Texture binding happens via bind groups in the render callback
    (void)program; (void)name; (void)slot; (void)texture;
    return true;
}

IMPLATFORM_API void ImPlatform_BeginUniformBlock(ImPlatform_ShaderProgram program)
{
    if (!program)
        return;

    g_CurrentUniformBlockProgram = program;

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

        // Store custom uniform data for later use in the render callback
        // The projection matrix will be prepended in the callback
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

        free(g_UniformBlockData);
        g_UniformBlockData = nullptr;
        g_UniformBlockSize = 0;
    }

    g_CurrentUniformBlockProgram = nullptr;
}

// ============================================================================
// Custom Shader DrawList Integration
// ============================================================================

// ImDrawCallback handler to activate a custom shader during rendering
static void ImPlatform_SetCustomShader(const ImDrawList* parent_list, const ImDrawCmd* cmd)
{
    ImPlatform_ShaderProgram program = (ImPlatform_ShaderProgram)cmd->UserCallbackData;
    if (!program)
        return;

    ImPlatform_ShaderProgramData_WebGPU* program_data = (ImPlatform_ShaderProgramData_WebGPU*)program;

    // Get the current render pass encoder from ImGui's render state
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    ImGui_ImplWGPU_RenderState* render_state = (ImGui_ImplWGPU_RenderState*)platform_io.Renderer_RenderState;
    if (!render_state || !render_state->RenderPassEncoder)
        return;

    WGPURenderPassEncoder pass = render_state->RenderPassEncoder;

    // Find draw data for projection matrix calculation
    ImDrawData* draw_data = g_CurrentDrawData;
    if (!draw_data)
    {
        ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
        for (int i = 0; i < pio.Viewports.Size && !draw_data; i++)
        {
            ImGuiViewport* viewport = pio.Viewports[i];
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
            }
        }
    }

    if (!draw_data)
        return;

    // Calculate projection matrix
    float L = draw_data->DisplayPos.x;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float T = draw_data->DisplayPos.y;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

    float mvp[4][4] =
    {
        { 2.0f / (R - L),     0.0f,              0.0f, 0.0f },
        { 0.0f,               2.0f / (T - B),    0.0f, 0.0f },
        { 0.0f,               0.0f,              0.5f, 0.0f },
        { (R + L) / (L - R),  (T + B) / (B - T), 0.5f, 1.0f },
    };

    // Build combined uniform data: [projection_matrix | custom_uniforms]
    size_t mvp_size = sizeof(mvp);
    size_t custom_size = program_data->uniformData ? program_data->uniformDataSize : 0;
    size_t total_size = mvp_size + custom_size;

    // Align to 256 bytes (WebGPU requirement)
    size_t aligned_size = (total_size + 255) & ~(size_t)255;

    // Create or recreate uniform buffer if needed (grow if too small)
    size_t required_capacity = aligned_size < 256 ? 256 : aligned_size;
    if (!program_data->uniformBuffer || program_data->uniformBufferCapacity < required_capacity)
    {
        if (program_data->uniformBuffer)
            wgpuBufferRelease(program_data->uniformBuffer);
        WGPUBufferDescriptor buf_desc = {};
        buf_desc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
        buf_desc.size = required_capacity;
        program_data->uniformBuffer = wgpuDeviceCreateBuffer(g_GfxData.device, &buf_desc);
        program_data->uniformBufferCapacity = program_data->uniformBuffer ? required_capacity : 0;
    }

    if (!program_data->uniformBuffer)
        return;

    // Upload projection matrix
    wgpuQueueWriteBuffer(g_GfxData.queue, program_data->uniformBuffer, 0, mvp, mvp_size);

    // Upload custom uniforms (if any)
    if (program_data->uniformData && custom_size > 0)
    {
        wgpuQueueWriteBuffer(g_GfxData.queue, program_data->uniformBuffer, mvp_size, program_data->uniformData, custom_size);
    }

    // Get default texture/sampler from ImGui's font atlas for shaders that don't use textures
    ImTextureRef font_tex = ImGui::GetIO().Fonts->TexRef;
    WGPUTextureView default_view = (WGPUTextureView)(void*)(intptr_t)font_tex.GetTexID();

    // Create a default sampler if we haven't yet
    if (!g_DefaultSampler)
    {
        WGPUSamplerDescriptor sampler_desc = {};
        sampler_desc.addressModeU = WGPUAddressMode_ClampToEdge;
        sampler_desc.addressModeV = WGPUAddressMode_ClampToEdge;
        sampler_desc.addressModeW = WGPUAddressMode_ClampToEdge;
        sampler_desc.magFilter = WGPUFilterMode_Linear;
        sampler_desc.minFilter = WGPUFilterMode_Linear;
        sampler_desc.mipmapFilter = WGPUMipmapFilterMode_Linear;
        sampler_desc.maxAnisotropy = 1;
        g_DefaultSampler = wgpuDeviceCreateSampler(g_GfxData.device, &sampler_desc);
    }

    // Create bind group
    WGPUBindGroupEntry bg_entries[3] = {};

    bg_entries[0].binding = 0;
    bg_entries[0].buffer = program_data->uniformBuffer;
    bg_entries[0].offset = 0;
    bg_entries[0].size = program_data->uniformBufferCapacity;

    bg_entries[1].binding = 1;
    bg_entries[1].sampler = g_DefaultSampler;

    bg_entries[2].binding = 2;
    bg_entries[2].textureView = default_view;

    WGPUBindGroupDescriptor bg_desc = {};
    bg_desc.layout = program_data->bindGroupLayout;
    bg_desc.entryCount = 3;
    bg_desc.entries = bg_entries;

    // Release previous bind group
    if (program_data->bindGroup)
        wgpuBindGroupRelease(program_data->bindGroup);

    program_data->bindGroup = wgpuDeviceCreateBindGroup(g_GfxData.device, &bg_desc);

    if (!program_data->bindGroup)
        return;

    // Set the custom pipeline and bind group on the render pass
    wgpuRenderPassEncoderSetPipeline(pass, program_data->renderPipeline);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, program_data->bindGroup, 0, nullptr);
}

// Activate a custom shader immediately (for use inside draw callbacks).
IMPLATFORM_API void ImPlatform_BeginCustomShader_Render(ImPlatform_ShaderProgram program)
{
    if (!program)
        return;

    ImPlatform_ShaderProgramData_WebGPU* program_data = (ImPlatform_ShaderProgramData_WebGPU*)program;

    // Get the current render pass encoder from ImGui's render state
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    ImGui_ImplWGPU_RenderState* render_state = (ImGui_ImplWGPU_RenderState*)platform_io.Renderer_RenderState;
    if (!render_state || !render_state->RenderPassEncoder)
        return;

    WGPURenderPassEncoder pass = render_state->RenderPassEncoder;

    // Find draw data for projection matrix calculation
    ImDrawData* draw_data = g_CurrentDrawData;
    if (!draw_data)
        draw_data = ImGui::GetDrawData();
    if (!draw_data)
        return;

    // Calculate projection matrix
    float L = draw_data->DisplayPos.x;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float T = draw_data->DisplayPos.y;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

    float mvp[4][4] =
    {
        { 2.0f / (R - L),     0.0f,              0.0f, 0.0f },
        { 0.0f,               2.0f / (T - B),    0.0f, 0.0f },
        { 0.0f,               0.0f,              0.5f, 0.0f },
        { (R + L) / (L - R),  (T + B) / (B - T), 0.5f, 1.0f },
    };

    // Build combined uniform data: [projection_matrix | custom_uniforms]
    size_t mvp_size = sizeof(mvp);
    size_t custom_size = program_data->uniformData ? program_data->uniformDataSize : 0;
    size_t total_size = mvp_size + custom_size;

    // Align to 256 bytes (WebGPU requirement)
    size_t aligned_size = (total_size + 255) & ~(size_t)255;

    // Create or recreate uniform buffer if needed (grow if too small)
    size_t required_capacity = aligned_size < 256 ? 256 : aligned_size;
    if (!program_data->uniformBuffer || program_data->uniformBufferCapacity < required_capacity)
    {
        if (program_data->uniformBuffer)
            wgpuBufferRelease(program_data->uniformBuffer);
        WGPUBufferDescriptor buf_desc = {};
        buf_desc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
        buf_desc.size = required_capacity;
        program_data->uniformBuffer = wgpuDeviceCreateBuffer(g_GfxData.device, &buf_desc);
        program_data->uniformBufferCapacity = program_data->uniformBuffer ? required_capacity : 0;
    }

    if (!program_data->uniformBuffer)
        return;

    // Upload projection matrix
    wgpuQueueWriteBuffer(g_GfxData.queue, program_data->uniformBuffer, 0, mvp, mvp_size);

    // Upload custom uniforms (if any)
    if (program_data->uniformData && custom_size > 0)
    {
        wgpuQueueWriteBuffer(g_GfxData.queue, program_data->uniformBuffer, mvp_size, program_data->uniformData, custom_size);
    }

    // Get default texture/sampler from ImGui's font atlas for shaders that don't use textures
    ImTextureRef font_tex = ImGui::GetIO().Fonts->TexRef;
    WGPUTextureView default_view = (WGPUTextureView)(void*)(intptr_t)font_tex.GetTexID();

    // Create a default sampler if we haven't yet
    if (!g_DefaultSampler)
    {
        WGPUSamplerDescriptor sampler_desc = {};
        sampler_desc.addressModeU = WGPUAddressMode_ClampToEdge;
        sampler_desc.addressModeV = WGPUAddressMode_ClampToEdge;
        sampler_desc.addressModeW = WGPUAddressMode_ClampToEdge;
        sampler_desc.magFilter = WGPUFilterMode_Linear;
        sampler_desc.minFilter = WGPUFilterMode_Linear;
        sampler_desc.mipmapFilter = WGPUMipmapFilterMode_Linear;
        sampler_desc.maxAnisotropy = 1;
        g_DefaultSampler = wgpuDeviceCreateSampler(g_GfxData.device, &sampler_desc);
    }

    // Create bind group
    WGPUBindGroupEntry bg_entries[3] = {};

    bg_entries[0].binding = 0;
    bg_entries[0].buffer = program_data->uniformBuffer;
    bg_entries[0].offset = 0;
    bg_entries[0].size = program_data->uniformBufferCapacity;

    bg_entries[1].binding = 1;
    bg_entries[1].sampler = g_DefaultSampler;

    bg_entries[2].binding = 2;
    bg_entries[2].textureView = default_view;

    WGPUBindGroupDescriptor bg_desc = {};
    bg_desc.layout = program_data->bindGroupLayout;
    bg_desc.entryCount = 3;
    bg_desc.entries = bg_entries;

    // Release previous bind group
    if (program_data->bindGroup)
        wgpuBindGroupRelease(program_data->bindGroup);

    program_data->bindGroup = wgpuDeviceCreateBindGroup(g_GfxData.device, &bg_desc);

    if (!program_data->bindGroup)
        return;

    // Set the custom pipeline and bind group on the render pass
    wgpuRenderPassEncoderSetPipeline(pass, program_data->renderPipeline);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, program_data->bindGroup, 0, nullptr);
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

    draw->AddCallback(ImGui::GetPlatformIO().DrawCallback_ResetRenderState, NULL);
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
// Sampler Override API - WebGPU
// ============================================================================
// Uses the standard DrawCallback_SetSamplerLinear/Nearest callbacks of the ImGui WebGPU backend,
// which switches between its linear/nearest common bind groups.
// Only the filter is honored: the wrap mode stays clamp.

IMPLATFORM_API void ImPlatform_PushSampler(ImPlatform_TextureFilter filter, ImPlatform_TextureWrap /*wrap*/)
{
    ImPlatform_PushSampler_StdCallbacks(filter);
}

IMPLATFORM_API void ImPlatform_PopSampler(void)
{
    ImPlatform_PopSampler_StdCallbacks();
}

#endif // IM_GFX_WGPU
