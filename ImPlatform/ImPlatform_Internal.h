// dear imgui: Platform/Renderer Abstraction Layer - Internal Header
// This file contains shared data structures and internal function declarations

#pragma once

#include "ImPlatform.h"

// Platform-specific includes
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    #include <windows.h>
#endif

// Include Windows.h for SDL2/SDL3 on Windows (needed for HWND type)
#if defined(_WIN32) && defined(IM_CURRENT_PLATFORM) && ((IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2) || (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3))
    #ifndef _WINDOWS_
        #include <windows.h>
    #endif
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    #if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_VULKAN)
        // For Vulkan builds, include Vulkan headers through GLFW
        #define GLFW_INCLUDE_VULKAN
    #else
        // Prevent GLFW from including OpenGL headers - imgui has its own loader
        #define GLFW_INCLUDE_NONE
    #endif
    #include <GLFW/glfw3.h>
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
    #include <SDL.h>
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
    #include <SDL3/SDL.h>
#endif

// Graphics API includes
#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_DIRECTX9)
    #include <d3d9.h>
#endif

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_DIRECTX10)
    #include <d3d10.h>
    #include <dxgi.h>
#endif

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
    #include <d3d11.h>
    #include <dxgi.h>
#endif

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
    #include <d3d12.h>
    #include <dxgi1_4.h>
#endif

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_OPENGL3)
    // Include ImGui's OpenGL loader for modern OpenGL functions
    // This must be included BEFORE any system OpenGL headers to avoid conflicts
    #if !defined(IMGUI_IMPL_OPENGL_LOADER_CUSTOM)
        #include "../imgui/backends/imgui_impl_opengl3_loader.h"
    #endif
    // Note: Additional GL constants not in the stripped loader are defined in ImPlatform_gfx_opengl3.cpp
#endif

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_VULKAN)
    #include <vulkan/vulkan.h>
#endif

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_METAL)
    // Metal headers are included in the .mm implementation file
#endif

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_WGPU)
    #include <webgpu/webgpu.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// App Data Structures
// ============================================================================

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
struct ImPlatform_AppData_Win32 {
    WNDCLASSEXW wc;
    HWND hWnd;
    bool bDone;
    float fDpiScale;

    // Custom TitleBar support
    bool bCustomTitleBar;
    bool bTitleBarHovered;
    ImVec2 vEndCustomToolBar;
    float fCustomTitleBarHeight;

    // File drop support
    bool bDropFileEnabled;
};
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
struct ImPlatform_AppData_GLFW {
    GLFWwindow* pWindow;
    float fDpiScale;

    // Custom TitleBar support
    bool bCustomTitleBar;
    bool bTitleBarHovered;
    ImVec2 vEndCustomToolBar;
    float fCustomTitleBarHeight;

    // Software drag state (for borderless without TheCherno's GLFW fork)
    bool bDragging;
    double fDragStartX, fDragStartY;
    int iWinStartX, iWinStartY;

    // File drop support
    bool bDropFileEnabled;
};
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
struct ImPlatform_AppData_SDL2 {
    SDL_Window* pWindow;
    SDL_GLContext glContext;
    bool bDone;
    float fDpiScale;

    // Custom TitleBar support
    bool bCustomTitleBar;
    bool bTitleBarHovered;
    ImVec2 vEndCustomToolBar;
    float fCustomTitleBarHeight;
};
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
struct ImPlatform_AppData_SDL3 {
    SDL_Window* pWindow;
    SDL_GLContext glContext;
    bool bDone;
    float fDpiScale;

    // Custom TitleBar support
    bool bCustomTitleBar;
    bool bTitleBarHovered;
    ImVec2 vEndCustomToolBar;
    float fCustomTitleBarHeight;
};
#endif

// ============================================================================
// Shared backbuffer size (set by backends on create/resize, read by ImPlatform_GetBackbufferSize)
// ============================================================================
extern unsigned int g_ImPlatform_BackbufferW;
extern unsigned int g_ImPlatform_BackbufferH;

// ============================================================================
// Gfx Data Structures
// ============================================================================

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_DIRECTX9)
struct ImPlatform_GfxData_DX9 {
    LPDIRECT3D9 pD3D;
    LPDIRECT3DDEVICE9 pDevice;
    D3DPRESENT_PARAMETERS d3dpp;
    bool bDeviceLost;
    unsigned int uResizeWidth;
    unsigned int uResizeHeight;
};
#endif

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_DIRECTX10)
struct ImPlatform_GfxData_DX10 {
    ID3D10Device* pDevice;
    IDXGISwapChain* pSwapChain;
    ID3D10RenderTargetView* pRenderTargetView;
    bool bSwapChainOccluded;
    unsigned int uResizeWidth;
    unsigned int uResizeHeight;
};
#endif

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
struct ImPlatform_GfxData_DX11 {
    ID3D11Device* pDevice;
    ID3D11DeviceContext* pDeviceContext;
    IDXGISwapChain* pSwapChain;
    ID3D11RenderTargetView* pRenderTargetView;
    bool bSwapChainOccluded;
    unsigned int uResizeWidth;
    unsigned int uResizeHeight;
};
#endif

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
// Forward declare the allocator (defined in gfx_dx12.cpp)
struct ExampleDescriptorHeapAllocator;

struct ImPlatform_FrameContext_DX12 {
    ID3D12CommandAllocator* pCommandAllocator;
    UINT64 uFenceValue;
};

#define IM_DX12_NUM_FRAMES_IN_FLIGHT 2
#define IM_DX12_NUM_BACK_BUFFERS 2

struct ImPlatform_GfxData_DX12 {
    ImPlatform_FrameContext_DX12 frameContext[IM_DX12_NUM_FRAMES_IN_FLIGHT];
    UINT uFrameIndex;
    ID3D12Device* pDevice;
    ID3D12DescriptorHeap* pRtvDescHeap;
    ID3D12DescriptorHeap* pSrvDescHeap;
    ExampleDescriptorHeapAllocator* pSrvDescHeapAlloc;
    ID3D12CommandQueue* pCommandQueue;
    ID3D12GraphicsCommandList* pCommandList;
    ID3D12Fence* pFence;
    HANDLE hFenceEvent;
    UINT64 uFenceLastSignaledValue;
    IDXGISwapChain3* pSwapChain;
    bool bSwapChainOccluded;
    HANDLE hSwapChainWaitableObject;
    ID3D12Resource* pRenderTargetResource[IM_DX12_NUM_BACK_BUFFERS];
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetDescriptor[IM_DX12_NUM_BACK_BUFFERS];
};
#endif

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_OPENGL3)
struct ImPlatform_GfxData_OpenGL3 {
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    HDC hDC;
    HGLRC hRC;
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    // GLFW manages the GL context
    int unused; // Avoid empty struct
#endif
};
#endif

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_VULKAN)
struct ImPlatform_GfxData_Vulkan {
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue queue;
    VkDebugReportCallbackEXT debugReport;
    VkPipelineCache pipelineCache;
    VkDescriptorPool descriptorPool;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkRenderPass renderPass;
    VkCommandPool commandPool;
    VkCommandBuffer* commandBuffers;
    VkSemaphore imageAcquiredSemaphore;
    VkSemaphore renderCompleteSemaphore;
    VkFence* fences;
    uint32_t imageCount;
    uint32_t currentFrameIndex;
    int minImageCount;
    VkSampler defaultSampler;        // Default sampler for custom shaders
    VkImageView defaultImageView;    // Default white 1x1 texture for custom shaders
    VkImage defaultImage;            // Default white 1x1 image
    VkDeviceMemory defaultImageMemory; // Memory for default image
};
#endif

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_METAL)
struct ImPlatform_GfxData_Metal {
    void* pMetalLayer;      // CAMetalLayer*
    void* pMetalDevice;     // id<MTLDevice>
    void* pCommandQueue;    // id<MTLCommandQueue>
    void* pRenderPassDescriptor; // MTLRenderPassDescriptor*
};
#endif

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_WGPU)
struct ImPlatform_GfxData_WebGPU {
    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUQueue queue;
    WGPUSurface surface;
#if !(defined(__EMSCRIPTEN__) && defined(IMGUI_IMPL_WEBGPU_BACKEND_DAWN))
    WGPUSwapChain swapChain;
#endif
    WGPUTextureFormat swapChainFormat;
    unsigned int uSurfaceWidth;
    unsigned int uSurfaceHeight;
};
#endif

// ============================================================================
// Internal App Functions
// ============================================================================

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
HWND ImPlatform_App_GetHWND(void);
float ImPlatform_App_GetDpiScale_Win32(void);
struct ImPlatform_AppData_Win32* ImPlatform_App_GetData_Win32(void);
#endif

// Internal helpers defined in ImPlatform.h (IMPLATFORM_IMPLEMENTATION section).
// Declared here so standalone-compiled platform backend .cpp files can call them.
void ImPlatform_NotifyDpiChange(float new_scale);

#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
extern ImPlatform_BorderlessParams g_BorderlessParams;
#endif

#if IMPLATFORM_APP_SUPPORT_DROP_FILE
void ImPlatform_NotifyFileDrop(const char* utf8_path, ImVec2 client_pos);
#endif

// Called when the drop file callback is registered or cleared.
// Platform backends use this to enable/disable OS drop acceptance.
#if IMPLATFORM_APP_SUPPORT_DROP_FILE
void ImPlatform_App_OnDropFileCallbackChanged(bool has_callback);
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
GLFWwindow* ImPlatform_App_GetGLFWWindow(void);
float ImPlatform_App_GetDpiScale_GLFW(void);
struct ImPlatform_AppData_GLFW* ImPlatform_App_GetData_GLFW(void);
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
SDL_Window* ImPlatform_App_GetSDL2Window(void);
float ImPlatform_App_GetDpiScale_SDL2(void);
struct ImPlatform_AppData_SDL2* ImPlatform_App_GetData_SDL2(void);
#ifdef _WIN32
HWND ImPlatform_App_GetHWND(void);  // Get native HWND from SDL2 window
#endif
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
SDL_Window* ImPlatform_App_GetSDL3Window(void);
float ImPlatform_App_GetDpiScale_SDL3(void);
struct ImPlatform_AppData_SDL3* ImPlatform_App_GetData_SDL3(void);
#ifdef _WIN32
HWND ImPlatform_App_GetHWND(void);  // Get native HWND from SDL3 window
#endif
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_APPLE)
void* ImPlatform_App_GetNSWindow(void);     // Returns NSWindow* on macOS
void* ImPlatform_App_GetNSView(void);       // Returns NSView* on macOS
void* ImPlatform_App_GetUIWindow(void);     // Returns UIWindow* on iOS
void* ImPlatform_App_GetUIView(void);       // Returns UIView* on iOS
float ImPlatform_App_GetDpiScale_Apple(void);
#endif

// ============================================================================
// Internal Gfx Functions
// ============================================================================

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_DIRECTX9)
bool ImPlatform_Gfx_CreateDevice_DX9(void* hWnd, struct ImPlatform_GfxData_DX9* pData);
void ImPlatform_Gfx_CleanupDevice_DX9(struct ImPlatform_GfxData_DX9* pData);
void ImPlatform_Gfx_OnResize_DX9(struct ImPlatform_GfxData_DX9* pData, unsigned int uWidth, unsigned int uHeight);
void ImPlatform_Gfx_ResetDevice_DX9(struct ImPlatform_GfxData_DX9* pData);
struct ImPlatform_GfxData_DX9* ImPlatform_Gfx_GetData_DX9(void);
#endif

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_DIRECTX10)
bool ImPlatform_Gfx_CreateDevice_DX10(void* hWnd, struct ImPlatform_GfxData_DX10* pData);
void ImPlatform_Gfx_CleanupDevice_DX10(struct ImPlatform_GfxData_DX10* pData);
void ImPlatform_Gfx_OnResize_DX10(struct ImPlatform_GfxData_DX10* pData, unsigned int uWidth, unsigned int uHeight);
struct ImPlatform_GfxData_DX10* ImPlatform_Gfx_GetData_DX10(void);
#endif

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
bool ImPlatform_Gfx_CreateDevice_DX11(void* hWnd, struct ImPlatform_GfxData_DX11* pData);
void ImPlatform_Gfx_CleanupDevice_DX11(struct ImPlatform_GfxData_DX11* pData);
void ImPlatform_Gfx_OnResize_DX11(struct ImPlatform_GfxData_DX11* pData, unsigned int uWidth, unsigned int uHeight);
struct ImPlatform_GfxData_DX11* ImPlatform_Gfx_GetData_DX11(void);
#endif

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
bool ImPlatform_Gfx_CreateDevice_DX12(void* hWnd, struct ImPlatform_GfxData_DX12* pData);
void ImPlatform_Gfx_CleanupDevice_DX12(struct ImPlatform_GfxData_DX12* pData);
void ImPlatform_Gfx_OnResize_DX12(struct ImPlatform_GfxData_DX12* pData, unsigned int uWidth, unsigned int uHeight);
struct ImPlatform_GfxData_DX12* ImPlatform_Gfx_GetData_DX12(void);
#endif

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_OPENGL3)
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
bool ImPlatform_Gfx_CreateDevice_OpenGL3(void* hWnd, struct ImPlatform_GfxData_OpenGL3* pData);
void ImPlatform_Gfx_CleanupDevice_OpenGL3(void* hWnd, struct ImPlatform_GfxData_OpenGL3* pData);
void ImPlatform_Gfx_SetSize_OpenGL3(int width, int height);
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
bool ImPlatform_Gfx_CreateDevice_OpenGL3(void* pWindow, struct ImPlatform_GfxData_OpenGL3* pData);
void ImPlatform_Gfx_CleanupDevice_OpenGL3(void* pWindow, struct ImPlatform_GfxData_OpenGL3* pData);
#elif defined(IM_CURRENT_PLATFORM) && ((IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2) || (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3))
bool ImPlatform_Gfx_CreateDevice_OpenGL3(void* pWindow, void* glContext, struct ImPlatform_GfxData_OpenGL3* pData);
void ImPlatform_Gfx_CleanupDevice_OpenGL3(void* pWindow, void* glContext, struct ImPlatform_GfxData_OpenGL3* pData);
#endif
struct ImPlatform_GfxData_OpenGL3* ImPlatform_Gfx_GetData_OpenGL3(void);
#endif

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_VULKAN)
// Vulkan device creation signature depends on platform
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
bool ImPlatform_Gfx_CreateDevice_Vulkan(void* hWnd, struct ImPlatform_GfxData_Vulkan* pData);
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
bool ImPlatform_Gfx_CreateDevice_Vulkan(void* pWindow, struct ImPlatform_GfxData_Vulkan* pData);
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
bool ImPlatform_Gfx_CreateDevice_Vulkan(void* pWindow, struct ImPlatform_GfxData_Vulkan* pData);
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
bool ImPlatform_Gfx_CreateDevice_Vulkan(void* pWindow, struct ImPlatform_GfxData_Vulkan* pData);
#endif
void ImPlatform_Gfx_CleanupDevice_Vulkan(struct ImPlatform_GfxData_Vulkan* pData);
struct ImPlatform_GfxData_Vulkan* ImPlatform_Gfx_GetData_Vulkan(void);
#endif

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_METAL)
// Metal is Objective-C++, device creation handled in .mm file
void* ImPlatform_Gfx_CreateDevice_Metal(void* pNSWindow, struct ImPlatform_GfxData_Metal* pData);
void ImPlatform_Gfx_CleanupDevice_Metal(struct ImPlatform_GfxData_Metal* pData);
struct ImPlatform_GfxData_Metal* ImPlatform_Gfx_GetData_Metal(void);
#endif

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_WGPU)
bool ImPlatform_Gfx_CreateDevice_WebGPU(void* pWindow, struct ImPlatform_GfxData_WebGPU* pData);
void ImPlatform_Gfx_CleanupDevice_WebGPU(struct ImPlatform_GfxData_WebGPU* pData);
struct ImPlatform_GfxData_WebGPU* ImPlatform_Gfx_GetData_WebGPU(void);
#endif

#ifdef __cplusplus
}
#endif

// ============================================================================
// Shader bytecode disk cache (shared across graphics backends)
// ============================================================================
// Backend-agnostic helpers for storing pre-compiled shader bytecode on disk
// keyed by a hash of the source + entry + profile. Each graphics backend
// (DX11, DX12, Vulkan SPIR-V, Metal, WebGPU) picks its own extension and
// compiles into its own native bytecode format; this header only provides
// hashing, path generation, and file I/O.
//
// Typical flow per backend CreateShader:
//   1. If desc->bytecode is set -> use it directly (skip cache)
//   2. Else if desc->cache_key is set:
//       hash = FNV64(source + entry + profile)
//       path = BuildPath(cache_key, entry, ext, hash)
//       cached = CacheLoad(path)
//       if cached: wrap in native blob, done
//       else: compile (with desc->compile_flags), CacheSave(path, blob), done
//   3. Else: compile without cache (legacy path)

#if IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <direct.h>
    #define IMPLATFORM_MKDIR_(path) _mkdir(path)
#else
    #include <sys/stat.h>
    #include <sys/types.h>
    #define IMPLATFORM_MKDIR_(path) mkdir((path), 0755)
#endif

// FNV-1a 64-bit hash of a buffer.
static inline unsigned long long ImPlatform_ShaderCacheHashBytes(const void* data, size_t len)
{
    unsigned long long h = 14695981039346656037ull;
    const unsigned char* p = (const unsigned char*)data;
    for (size_t i = 0; i < len; i++)
    {
        h ^= (unsigned long long)p[i];
        h *= 1099511628211ull;
    }
    return h;
}

// Combine source + entry + profile into a single 64-bit cache key.
// Any change to source, entry, or profile invalidates the cache entry.
static inline unsigned long long ImPlatform_ShaderCacheHashSource(
    const char* source, size_t source_len,
    const char* entry, const char* profile)
{
    unsigned long long h = ImPlatform_ShaderCacheHashBytes(source, source_len);
    if (entry && *entry)
        h ^= ImPlatform_ShaderCacheHashBytes(entry, strlen(entry)) * 1099511628211ull;
    if (profile && *profile)
        h ^= ImPlatform_ShaderCacheHashBytes(profile, strlen(profile)) * 1099511628211ull;
    return h;
}

// Build the on-disk cache path and ensure the parent directories exist.
// Layout: ./shaders/bytecode_cache/<cache_key>_<entry>_<16-hex-hash>.<ext>
// Parent dirs are created silently; mkdir errors are ignored.
static inline void ImPlatform_ShaderCacheBuildPath(
    char* out_path, size_t out_size,
    const char* cache_key, const char* entry, const char* ext,
    unsigned long long hash)
{
    IMPLATFORM_MKDIR_("./shaders");
    IMPLATFORM_MKDIR_("./shaders/bytecode_cache");
    snprintf(out_path, out_size, "./shaders/bytecode_cache/%s_%s_%016llx.%s",
             cache_key ? cache_key : "shader",
             entry ? entry : "main",
             hash,
             ext ? ext : "bin");
}

// Load a cached bytecode blob from disk. Returns a newly-allocated buffer
// (caller must free() it) and writes the size into *out_size. Returns NULL
// on cache miss or I/O error.
static inline void* ImPlatform_ShaderCacheLoad(const char* path, size_t* out_size)
{
    if (!path || !out_size) return NULL;
    *out_size = 0;
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz <= 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    void* data = malloc((size_t)sz);
    if (!data) { fclose(f); return NULL; }
    size_t read = fread(data, 1, (size_t)sz, f);
    fclose(f);
    if (read != (size_t)sz) { free(data); return NULL; }
    *out_size = (size_t)sz;
    return data;
}

// Save a bytecode blob to disk. Returns true on success.
static inline bool ImPlatform_ShaderCacheSave(const char* path, const void* data, size_t size)
{
    if (!path || !data || size == 0) return false;
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    return written == size;
}

#endif  // IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER
