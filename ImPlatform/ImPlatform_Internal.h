// dear imgui: Platform/Renderer Abstraction Layer - Internal Header
// This file contains shared data structures and internal function declarations

#pragma once

#include "ImPlatform.h"

// Renders a render pass one draw list at a time (immediate backends: DX9, DX10, DX11, OpenGL): their vertex and index
// buffers grow to the largest list instead of the whole pass, and are remapped whole each frame (a pass of millions of
// vertices made every later frame stall on that remap). The frame's texture updates go with the first list.
template <typename RenderFn>
static inline void ImPlatform_RenderPassLists(ImDrawData* draw_data, RenderFn render)
{
    if (draw_data->CmdLists.Size <= 1)
    {
        render(draw_data);
        return;
    }
    ImDrawData one;
    one.Valid = true;
    one.DisplayPos = draw_data->DisplayPos;
    one.DisplaySize = draw_data->DisplaySize;
    one.FramebufferScale = draw_data->FramebufferScale;
    one.OwnerViewport = draw_data->OwnerViewport;
    one.Textures = draw_data->Textures;
    one.CmdLists.resize(1);
#ifndef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    one.CmdListsCount = 1;
#endif
    for (ImDrawList* list : draw_data->CmdLists)
    {
        one.CmdLists[0] = list;
        one.TotalVtxCount = list->VtxBuffer.Size;
        one.TotalIdxCount = list->IdxBuffer.Size;
        render(&one);
        one.Textures = NULL;
    }
}

// A window rectangle (x, y, width, height) dragged by (dx, dy) from where it was at the mouse press. edges: 1 left,
// 2 right, 4 top, 8 bottom (the dragged edges move, the opposite ones stay, the size never goes below the minimum);
// 16: the whole window moves.
static inline void ImPlatform_DragRect(int edges, const int start[4], int dx, int dy, int min_w, int min_h, int out[4])
{
    int l = start[0], t = start[1], r = start[0] + start[2], b = start[1] + start[3];
    if (edges & 16) { l += dx; r += dx; t += dy; b += dy; }
    if (edges & 1)  l = l + dx < r - min_w ? l + dx : r - min_w;
    if (edges & 2)  r = r + dx > l + min_w ? r + dx : l + min_w;
    if (edges & 4)  t = t + dy < b - min_h ? t + dy : b - min_h;
    if (edges & 8)  b = b + dy > t + min_h ? b + dy : t + min_h;
    out[0] = l;
    out[1] = t;
    out[2] = r - l;
    out[3] = b - t;
}

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
    #include <dxgi1_5.h>
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

    // Software move and resize of the borderless window (without TheCherno's GLFW fork): ImPlatform_DragRect edges
    int iDragEdges;                     // Being dragged since the mouse press; 0: none
    int iHoverEdges;                    // Resize edges under the cursor or being dragged (the mouse cursor's shape)
    bool bMouseWasDown;
    double fDragStartX, fDragStartY;    // Cursor at the press, screen coordinates
    int iWinStart[4];                   // Window x, y, width, height at the press

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
    bool bSwapChainTearingSupport;
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
#else
    // GLFW and SDL manage the GL context
    int unused; // An empty struct is size 0 in C, 1 in C++ (-Wextern-c-compat)
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
    VkDescriptorPool descriptorPool;    // Shared with Dear ImGui's backend
    VkSurfaceKHR surface;
    int minImageCount;
    // The swapchain, its render pass and the frames belong to Dear ImGui's ImGui_ImplVulkanH_Window
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
    WGPUSurfaceTexture surfaceTexture;  // Surface texture acquired for the current frame (in ImPlatform_GfxCheck)
    WGPUTextureFormat swapChainFormat;  // Configured surface format
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
// A drop event for the application's callbacks (utf8: the path of a File event, the text of a Text event).
void ImPlatform_NotifyDrop(ImPlatform_DropEventType type, const char* utf8, ImVec2 client_pos);
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
void ImPlatform_Native_NotifyDroppedFiles(void* drop, ImVec2 pos);  // drop: an HDROP
#endif
#endif

// ImPlatform_native.cpp on Windows, for the Win32, GLFW and SDL2 backends (SDL3 reads its drops itself).
#ifdef _WIN32
void* ImPlatform_Native_MainWindow(void);       // The main window's HWND
void* ImPlatform_Native_GetDropTarget(void);    // The IDropTarget of the application's windows
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
bool  ImPlatform_Native_WindowMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif
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
void* ImPlatform_App_GetNSWindow(void);     // The NSWindow*
void* ImPlatform_App_GetNSView(void);       // Its content view, an NSView*
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

// Pen input queue (ImPlatform.h implementation), fed by platform backends (every graphics API).
void ImPlatform_Pen_Push(const struct ImPlatform_PenSample* sample);
void ImPlatform_Pen_Leave(void);

// Touch input (ImPlatform.h implementation), fed by platform backends: a finger in contact at a client-area position
// (down), or lifted.
void ImPlatform_Touch_Set(unsigned long long id, float x, float y, float pressure, bool down);
// A pinch reported as a zoom factor, a turn in degrees clockwise (trackpads); ignored while fingers are in contact.
void ImPlatform_Touch_Zoom(float factor);
void ImPlatform_Touch_Rotate(float degrees);
// ImPlatform_native.cpp: what the system libraries need between two frames (called by ImPlatform_PlatformEvents),
// and before the main window is destroyed.
void ImPlatform_Native_Pump(void);
void ImPlatform_Native_Shutdown(void);
// The application's windows take the drags the system itself reports (Windows: OLE; Linux: XDND), or stop taking them.
void ImPlatform_Native_AcceptDrops(bool accept);
#if defined(__linux__)
unsigned long ImPlatform_App_GetX11Window(void);    // The main window's X11 id; 0 when it is not an X11 window
#endif

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
bool ImPlatform_Gfx_CreateDevice_DX11(void* hWnd, struct ImPlatform_GfxData_DX11* pData);
void ImPlatform_Gfx_CleanupDevice_DX11(struct ImPlatform_GfxData_DX11* pData);
void ImPlatform_Gfx_OnResize_DX11(struct ImPlatform_GfxData_DX11* pData, unsigned int uWidth, unsigned int uHeight);
struct ImPlatform_GfxData_DX11* ImPlatform_Gfx_GetData_DX11(void);
#endif

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_CUSTOM)
// Provided by the application's graphics backend: the window's client area changed size.
void ImPlatform_Gfx_OnResize_Custom(unsigned int uWidth, unsigned int uHeight);
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
// Sampler override using standard backend draw callbacks
// ============================================================================
// Since Dear ImGui 1.92.8 renderer backends provide backend-agnostic draw callbacks
// in ImGuiPlatformIO (DrawCallback_SetSamplerLinear / DrawCallback_SetSamplerNearest).
// Backends that cannot swap arbitrary sampler states mid-frame (DX12, Vulkan, WebGPU)
// use them to implement ImPlatform_PushSampler()/ImPlatform_PopSampler():
// the filter is honored, the wrap mode stays at the backend default (clamp).
// The stack is resolved at submission time, restoring Linear (backend default) when empty.
// 'apply' false records the push or the pop without a callback (DirectX 12 inside a custom shader scope).
// ImPlatform_EndCustomShader resets the render state, sampler included: it sets the pushed sampler again.

#ifdef __cplusplus
static inline ImVector<ImDrawCallback>& ImPlatform_StdSamplerStack()
{
    static ImVector<ImDrawCallback> stack;
    return stack;
}

static inline void ImPlatform_PushSampler_StdCallbacks(ImPlatform_TextureFilter filter, bool apply = true)
{
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    ImDrawCallback callback = (filter == ImPlatform_TextureFilter_Nearest) ? platform_io.DrawCallback_SetSamplerNearest : platform_io.DrawCallback_SetSamplerLinear;
    ImPlatform_StdSamplerStack().push_back(callback);
    if (callback && apply)
        ImGui::GetWindowDrawList()->AddCallback(callback);
}

static inline void ImPlatform_PopSampler_StdCallbacks(bool apply = true)
{
    ImVector<ImDrawCallback>& stack = ImPlatform_StdSamplerStack();
    if (stack.Size == 0)
        return;
    stack.pop_back();
    ImDrawCallback callback = (stack.Size > 0) ? stack.back() : ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear;
    if (callback && apply)
        ImGui::GetWindowDrawList()->AddCallback(callback);
}

static inline void ImPlatform_ReapplySampler_StdCallbacks(ImDrawList* draw)
{
    const ImVector<ImDrawCallback>& stack = ImPlatform_StdSamplerStack();
    if (stack.Size > 0 && stack.back())
        draw->AddCallback(stack.back());
}

// The ImGui backends set the scissor for regular draws only: a callback that draws by itself (ImPlatform_DrawIndexed,
// DrawQuadNow) would run with the previous list's scissor, or none at the start of a render pass. One zero-area,
// transparent triangle in the current command makes the backend set its clip rect, the one of the callbacks that follow
// (no extra draw call when that command already has geometry).
static inline void ImPlatform_ScissorBeforeCallbacks(ImDrawList* draw)
{
    const ImDrawIdx idx = (ImDrawIdx)draw->_VtxCurrentIdx;
    draw->PrimReserve(3, 1);
    for (int i = 0; i < 3; i++)
        draw->PrimWriteIdx(idx);
    draw->PrimWriteVtx(ImVec2(draw->_CmdHeader.ClipRect.x, draw->_CmdHeader.ClipRect.y), ImVec2(0.0f, 0.0f), 0);
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

// ============================================================================
// Shader compile log, defines and blending (shared across graphics backends)
// ============================================================================

// The text of ImPlatform_GetShaderLog (one instance for every translation unit).
inline ImGuiTextBuffer& ImPlatform_ShaderLogBuffer()
{
    static ImGuiTextBuffer log;
    return log;
}

// Replaces the log with the message of a failing call and prints it to stderr. Graphics thread only.
static inline void ImPlatform_ShaderLog(const char* fmt, ...) IM_FMTARGS(1);
static inline void ImPlatform_ShaderLog(const char* fmt, ...)
{
    ImGuiTextBuffer& log = ImPlatform_ShaderLogBuffer();
    log.clear();
    va_list args;
    va_start(args, fmt);
    log.appendfv(fmt, args);
    va_end(args);
    fprintf(stderr, "[ImPlatform] %s\n", log.c_str());
}

// Appends a source with its line numbers, as the compiler's messages refer to them.
static inline void ImPlatform_AppendNumberedSource(ImGuiTextBuffer& out, const char* source)
{
    int line = 1;
    for (const char* p = source; *p; line++)
    {
        const char* end = p + strcspn(p, "\n");
        out.appendf("%4d: %.*s\n", line, (int)(end - p), p);
        p = *end ? end + 1 : end;
    }
}

// ImPlatform_CreateShaderProgramAsync; list_source: the log of a failure ends with the numbered source of the stage
// (generated shaders).
ImPlatform_ShaderJob ImPlatform_ShaderJobCreate(const ImPlatform_ShaderDesc* vertex_desc, const ImPlatform_ShaderDesc* fragment_desc,
                                                bool list_source);

// "NAME=VALUE;OTHER" (';' or new lines) -> #define lines; a line starting with '#' is kept as written.
static inline void ImPlatform_AppendDefines(ImGuiTextBuffer& out, const char* defines)
{
    for (const char* p = defines; p && *p; )
    {
        const char* end = p + strcspn(p, ";\n");
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r'))
            p++;
        const char* eq = (const char*)memchr(p, '=', (size_t)(end - p));
        if (p < end && *p == '#')
            out.appendf("%.*s\n", (int)(end - p), p);
        else if (p < end)
        {
            if (eq)
                out.appendf("#define %.*s %.*s\n", (int)(eq - p), p, (int)(end - eq - 1), eq + 1);
            else
                out.appendf("#define %.*s\n", (int)(end - p), p);
        }
        p = *end ? end + 1 : end;
    }
}

// The descriptor to compile: `desc` itself, or `with` when desc->defines applies (its text is kept in `source`). The
// #define lines go before the source, after the line of a GLSL #version, followed by a #line so that the compiler
// numbers the lines of the source as written.
static inline const ImPlatform_ShaderDesc* ImPlatform_ShaderApplyDefines(const ImPlatform_ShaderDesc* desc, ImPlatform_ShaderDesc* with,
                                                                        ImGuiTextBuffer* source)
{
    if (!desc || !desc->source_code || !desc->defines || !desc->defines[0]
        || desc->format == ImPlatform_ShaderFormat_SPIRV || desc->format == ImPlatform_ShaderFormat_WGSL)
        return desc;
    const char* body = desc->source_code;
    int line = 1;
    const char* version = desc->format == ImPlatform_ShaderFormat_GLSL ? strstr(body, "#version") : NULL;
    if (version)
    {
        const char* eol = strchr(version, '\n');
        eol = eol ? eol + 1 : version + strlen(version);
        source->append(body, eol);
        if (eol[-1] != '\n')
            source->append("\n");
        for (const char* p = body; p < eol; p++)
            line += *p == '\n';
        // "#line N" numbers the next line N from desktop GLSL 3.30, N + 1 before it and in GLSL ES.
        const char* es = strstr(version, " es");
        if (atoi(version + 8) < 330 || (es && es < eol))
            line--;
        body = eol;
    }
    ImPlatform_AppendDefines(*source, desc->defines);
    source->appendf("#line %d\n%s", line, body);
    *with = *desc;
    with->source_code = source->c_str();
    with->defines = NULL;
    return with;
}

// A blend descriptor packed in an integer, as programs keep it and pipelines are keyed by it: 0 is "none set" (Dear
// ImGui's blending). False when a member is out of range. A color factor of the alpha blend becomes its alpha factor:
// the same value in the alpha channel, and the only form Direct3D takes.
static inline bool ImPlatform_BlendKey(const ImPlatform_BlendDesc* b, unsigned int* key)
{
    if (!b)
    {
        *key = 0;
        return true;
    }
    if ((unsigned)b->src_color > ImPlatform_BlendFactor_InvDstAlpha || (unsigned)b->dst_color > ImPlatform_BlendFactor_InvDstAlpha
        || (unsigned)b->src_alpha > ImPlatform_BlendFactor_InvDstAlpha || (unsigned)b->dst_alpha > ImPlatform_BlendFactor_InvDstAlpha
        || (unsigned)b->op_color > ImPlatform_BlendOp_Max || (unsigned)b->op_alpha > ImPlatform_BlendOp_Max)
        return false;
    unsigned alpha[2] = { (unsigned)b->src_alpha, (unsigned)b->dst_alpha };
    for (unsigned& f : alpha)
        if (f == ImPlatform_BlendFactor_SrcColor || f == ImPlatform_BlendFactor_InvSrcColor
            || f == ImPlatform_BlendFactor_DstColor || f == ImPlatform_BlendFactor_InvDstColor)
            f += ImPlatform_BlendFactor_SrcAlpha - ImPlatform_BlendFactor_SrcColor;
    *key = 1u | (b->enable ? 2u : 0u) | ((unsigned)b->src_color << 2) | ((unsigned)b->dst_color << 6) | ((unsigned)b->op_color << 10)
         | (alpha[0] << 13) | (alpha[1] << 17) | ((unsigned)b->op_alpha << 21);
    return true;
}

// The descriptor of a key; Dear ImGui's blending for 0.
static inline ImPlatform_BlendDesc ImPlatform_BlendFromKey(unsigned int key)
{
    ImPlatform_BlendDesc b = { true, ImPlatform_BlendFactor_SrcAlpha, ImPlatform_BlendFactor_InvSrcAlpha, ImPlatform_BlendOp_Add,
                               ImPlatform_BlendFactor_One, ImPlatform_BlendFactor_InvSrcAlpha, ImPlatform_BlendOp_Add };
    if (key)
    {
        b.enable = (key & 2u) != 0;
        b.src_color = (ImPlatform_BlendFactor)((key >> 2) & 15u);
        b.dst_color = (ImPlatform_BlendFactor)((key >> 6) & 15u);
        b.op_color = (ImPlatform_BlendOp)((key >> 10) & 7u);
        b.src_alpha = (ImPlatform_BlendFactor)((key >> 13) & 15u);
        b.dst_alpha = (ImPlatform_BlendFactor)((key >> 17) & 15u);
        b.op_alpha = (ImPlatform_BlendOp)((key >> 21) & 7u);
    }
    return b;
}

// ============================================================================
// HLSL compilation shared by Direct3D 10, 11 and 12
// ============================================================================
#if IM_CURRENT_GFX == IM_GFX_DIRECTX10 || IM_CURRENT_GFX == IM_GFX_DIRECTX11 || IM_CURRENT_GFX == IM_GFX_DIRECTX12
#include <d3dcompiler.h>

// ImPlatform_CreateShaderProgramAsync compiles on a worker thread: ImPlatform_D3DCompileShader needs no device.
#define IMPLATFORM_SHADER_COMPILE_THREAD 1

static inline const char* ImPlatform_D3DShaderTarget(ImPlatform_ShaderStage stage)
{
#if IM_CURRENT_GFX == IM_GFX_DIRECTX10
    // Direct3D 10 hardware stops at shader model 4.
    return stage == ImPlatform_ShaderStage_Vertex ? "vs_4_0" : stage == ImPlatform_ShaderStage_Fragment ? "ps_4_0" : NULL;
#else
    return stage == ImPlatform_ShaderStage_Vertex ? "vs_5_0" : stage == ImPlatform_ShaderStage_Fragment ? "ps_5_0"
         : stage == ImPlatform_ShaderStage_Compute ? "cs_5_0" : NULL;
#endif
}

// HLSL source (its defines already applied) to bytecode, through the disk cache when desc->cache_key is set. NULL on
// failure, with the compiler's messages in *out_log (to free(); may stay NULL). Uses no device and no shared state, so
// it runs on any thread.
static inline ID3DBlob* ImPlatform_D3DCompileShader(const ImPlatform_ShaderDesc* desc, char** out_log)
{
    *out_log = NULL;
    const char* target = ImPlatform_D3DShaderTarget(desc->stage);
    if (!target || !desc->source_code || desc->format != ImPlatform_ShaderFormat_HLSL)
        return NULL;
    const char* entry = desc->entry_point ? desc->entry_point : "main";
    const size_t source_len = strlen(desc->source_code);
    const bool cached = desc->cache_key && desc->cache_key[0];
    char cache_path[512] = "";
    ID3DBlob* blob = NULL;
    if (cached)
    {
        ImPlatform_ShaderCacheBuildPath(cache_path, sizeof(cache_path), desc->cache_key, entry, "dxbc",
                                        ImPlatform_ShaderCacheHashSource(desc->source_code, source_len, entry, target));
        size_t size = 0;
        void* data = ImPlatform_ShaderCacheLoad(cache_path, &size);
        if (data && SUCCEEDED(D3DCreateBlob(size, &blob)) && blob)
        {
            memcpy(blob->GetBufferPointer(), data, size);
            fprintf(stderr, "[ImPlatform shader cache] HIT  %s/%s (%zu bytes)\n", desc->cache_key, entry, size);
        }
        free(data);
        if (blob)
            return blob;
        fprintf(stderr, "[ImPlatform shader cache] MISS %s/%s -- compiling...\n", desc->cache_key, entry);
    }

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
    if (desc->compile_flags & IMPLATFORM_SHADER_COMPILE_SKIP_OPTIMIZATION) flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
    if (desc->compile_flags & IMPLATFORM_SHADER_COMPILE_OPTIMIZATION_LOW)  flags |= D3DCOMPILE_OPTIMIZATION_LEVEL0;
    if (desc->compile_flags & IMPLATFORM_SHADER_COMPILE_OPTIMIZATION_HIGH) flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
    if (desc->compile_flags & IMPLATFORM_SHADER_COMPILE_DEBUG)             flags |= D3DCOMPILE_DEBUG;
    ID3DBlob* errors = NULL;
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);
    const HRESULT hr = D3DCompile(desc->source_code, source_len, NULL, NULL, NULL, entry, target, flags, 0, &blob, &errors);
    QueryPerformanceCounter(&t1);
    if (FAILED(hr) || !blob)
    {
        if (errors)
        {
            const size_t n = errors->GetBufferSize();
            *out_log = (char*)malloc(n + 1);
            if (*out_log)
            {
                memcpy(*out_log, errors->GetBufferPointer(), n);
                (*out_log)[n] = 0;
            }
            errors->Release();
        }
        if (blob)
            blob->Release();
        return NULL;
    }
    if (errors)
        errors->Release();
    fprintf(stderr, "[ImPlatform shader compile] %-22s %s  target=%s  %8.2f ms  (%zu bytes, %zu src chars)\n",
            cached ? desc->cache_key : "(no-key)", entry, target, (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)freq.QuadPart,
            (size_t)blob->GetBufferSize(), source_len);
    if (cached && ImPlatform_ShaderCacheSave(cache_path, blob->GetBufferPointer(), blob->GetBufferSize()))
        fprintf(stderr, "[ImPlatform shader cache] SAVE %s/%s (%zu bytes)\n", desc->cache_key, entry, (size_t)blob->GetBufferSize());
    return blob;
}
#endif // Direct3D 10, 11, 12

#endif  // IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER
