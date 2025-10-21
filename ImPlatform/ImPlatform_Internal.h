// dear imgui: Platform/Renderer Abstraction Layer - Internal Header
// This file contains shared data structures and internal function declarations

#pragma once

#include "ImPlatform.h"

// Platform-specific includes
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    #include <windows.h>
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    struct GLFWwindow; // Forward declaration
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
    struct SDL_Window; // Forward declaration
    typedef void* SDL_GLContext; // Forward declaration
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
    struct SDL_Window; // Forward declaration (SDL3 uses same name)
    typedef void* SDL_GLContext; // Forward declaration
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
    #if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
        #include <GL/GL.h>
    #endif
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
};
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
struct ImPlatform_AppData_GLFW {
    GLFWwindow* pWindow;
};
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
struct ImPlatform_AppData_SDL2 {
    SDL_Window* pWindow;
    SDL_GLContext glContext;
};
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
struct ImPlatform_AppData_SDL3 {
    SDL_Window* pWindow;
    SDL_GLContext glContext;
};
#endif

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
    WGPUSwapChain swapChain;
    WGPUTextureFormat swapChainFormat;
    WGPURenderPassDescriptor renderPassDesc;
};
#endif

// ============================================================================
// Internal App Functions
// ============================================================================

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
HWND ImPlatform_App_GetHWND(void);
struct ImPlatform_AppData_Win32* ImPlatform_App_GetData_Win32(void);
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
GLFWwindow* ImPlatform_App_GetGLFWWindow(void);
struct ImPlatform_AppData_GLFW* ImPlatform_App_GetData_GLFW(void);
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
SDL_Window* ImPlatform_App_GetSDL2Window(void);
struct ImPlatform_AppData_SDL2* ImPlatform_App_GetData_SDL2(void);
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
SDL_Window* ImPlatform_App_GetSDL3Window(void);
struct ImPlatform_AppData_SDL3* ImPlatform_App_GetData_SDL3(void);
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_APPLE)
void* ImPlatform_App_GetNSWindow(void);     // Returns NSWindow* on macOS
void* ImPlatform_App_GetNSView(void);       // Returns NSView* on macOS
void* ImPlatform_App_GetUIWindow(void);     // Returns UIWindow* on iOS
void* ImPlatform_App_GetUIView(void);       // Returns UIView* on iOS
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
