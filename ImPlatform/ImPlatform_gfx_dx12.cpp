// dear imgui: Graphics API Abstraction - DirectX 12 Backend
// This handles D3D12 device creation, rendering, and presentation

#include "ImPlatform_Internal.h"

#ifdef IM_GFX_DIRECTX12

#include <stdio.h>
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

#include "../imgui/backends/imgui_impl_dx12.h"

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

#define APP_SRV_HEAP_SIZE 8192     // ImGui textures plus application textures (ImPlatform_CreateTexture and friends)

// Descriptor heap allocator (from example_win32_directx12)
struct ExampleDescriptorHeapAllocator
{
    ID3D12DescriptorHeap* Heap;
    D3D12_DESCRIPTOR_HEAP_TYPE HeapType;
    D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
    D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
    UINT HeapHandleIncrement;
    int* FreeIndices;
    int FreeIndicesCount;
    int FreeIndicesCapacity;

    ExampleDescriptorHeapAllocator() : Heap(NULL), HeapType(D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES),
        HeapHandleIncrement(0), FreeIndices(NULL), FreeIndicesCount(0), FreeIndicesCapacity(0) {}

    void Create(ID3D12Device* device, ID3D12DescriptorHeap* heap)
    {
        Heap = heap;
        D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
        HeapType = desc.Type;
        HeapStartCpu = Heap->GetCPUDescriptorHandleForHeapStart();
        HeapStartGpu = Heap->GetGPUDescriptorHandleForHeapStart();
        HeapHandleIncrement = device->GetDescriptorHandleIncrementSize(HeapType);

        FreeIndicesCapacity = (int)desc.NumDescriptors;
        FreeIndices = new int[FreeIndicesCapacity];
        for (int n = desc.NumDescriptors; n > 0; n--)
            FreeIndices[FreeIndicesCount++] = n - 1;
    }

    void Destroy()
    {
        if (FreeIndices)
        {
            delete[] FreeIndices;
            FreeIndices = NULL;
        }
        Heap = NULL;
        FreeIndicesCount = 0;
        FreeIndicesCapacity = 0;
    }

    void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu)
    {
        IM_ASSERT(FreeIndicesCount > 0 && "SRV descriptor heap is full (APP_SRV_HEAP_SIZE)");
        int idx = FreeIndices[--FreeIndicesCount];
        out_cpu->ptr = HeapStartCpu.ptr + (idx * HeapHandleIncrement);
        out_gpu->ptr = HeapStartGpu.ptr + (idx * HeapHandleIncrement);
    }

    void Free(D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu)
    {
        int cpu_idx = (int)((cpu.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
        int gpu_idx = (int)((gpu.ptr - HeapStartGpu.ptr) / HeapHandleIncrement);
        IM_ASSERT(cpu_idx == gpu_idx);
        IM_ASSERT(FreeIndicesCount < FreeIndicesCapacity);
        FreeIndices[FreeIndicesCount++] = cpu_idx;
        (void)gpu_idx;
    }
};

// Global state
static ImPlatform_GfxData_DX12 g_GfxData = { 0 };
unsigned int g_ImPlatform_BackbufferW = 0;
unsigned int g_ImPlatform_BackbufferH = 0;
static ExampleDescriptorHeapAllocator g_SrvDescHeapAlloc;
static void ImPlatform_DX12_FlushWork();     // Resource layer (end of this file).
static void ImPlatform_DX12_ReleaseAll();
static void ImPlatform_DX12_DeferRelease(IUnknown* object);

// Render texture tracking
struct ImPlatform_RTTracking_DX12 {
    ID3D12Resource*             pTexture;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvCpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle;
    unsigned int                width, height;
    DXGI_FORMAT                 format;
    ImPlatform_RTTracking_DX12* next;
};
static ImPlatform_RTTracking_DX12* g_RTTrackingHead = NULL;

// Small RTV descriptor heap dedicated to render textures (created lazily)
static ID3D12DescriptorHeap*   g_RTVTexDescHeap     = NULL;
static UINT                    g_RTVTexDescOffset    = 0;
static UINT                    g_RTVTexDescStride    = 0;
static const UINT              g_RTVTexDescCapacity  = 16;

// The entry that is currently acting as render target (set by BeginRenderToTexture)
static ImPlatform_RTTracking_DX12* g_ActiveRTEntry = NULL;

// Uniform block API state
static ImPlatform_ShaderProgram g_CurrentUniformBlockProgram = nullptr;
static void* g_UniformBlockData = nullptr;
static size_t g_UniformBlockSize = 0;

// Current draw data for custom shader rendering (needed for multi-viewport)
static ImDrawData* g_CurrentDrawData = nullptr;

// Helper functions
static void CreateRenderTarget()
{
    for (UINT i = 0; i < IM_DX12_NUM_BACK_BUFFERS; i++)
    {
        ID3D12Resource* pBackBuffer = NULL;
        g_GfxData.pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
        g_GfxData.pDevice->CreateRenderTargetView(pBackBuffer, NULL, g_GfxData.renderTargetDescriptor[i]);
        g_GfxData.pRenderTargetResource[i] = pBackBuffer;
    }

    // Update stored backbuffer size
    if (g_GfxData.pRenderTargetResource[0])
    {
        D3D12_RESOURCE_DESC bbDesc = g_GfxData.pRenderTargetResource[0]->GetDesc();
        g_ImPlatform_BackbufferW = (unsigned int)bbDesc.Width;
        g_ImPlatform_BackbufferH = (unsigned int)bbDesc.Height;
    }
}

// Wait for all command lists submitted to the queue so far to complete (same as upstream example_win32_directx12)
static void WaitForPendingOperations()
{
    if (!g_GfxData.pCommandQueue || !g_GfxData.pFence || !g_GfxData.hFenceEvent)
        return; // Device creation failed half-way
    g_GfxData.pCommandQueue->Signal(g_GfxData.pFence, ++g_GfxData.uFenceLastSignaledValue);

    g_GfxData.pFence->SetEventOnCompletion(g_GfxData.uFenceLastSignaledValue, g_GfxData.hFenceEvent);
    ::WaitForSingleObject(g_GfxData.hFenceEvent, INFINITE);
}

static void CleanupRenderTarget()
{
    WaitForPendingOperations();

    for (UINT i = 0; i < IM_DX12_NUM_BACK_BUFFERS; i++)
    {
        if (g_GfxData.pRenderTargetResource[i])
        {
            g_GfxData.pRenderTargetResource[i]->Release();
            g_GfxData.pRenderTargetResource[i] = NULL;
        }
    }
}

// Wait for the swapchain and for the GPU to be done with the next frame context (same as upstream example_win32_directx12)
static ImPlatform_FrameContext_DX12* WaitForNextFrameContext()
{
    ImPlatform_FrameContext_DX12* frameCtx = &g_GfxData.frameContext[g_GfxData.uFrameIndex % IM_DX12_NUM_FRAMES_IN_FLIGHT];
    if (g_GfxData.pFence->GetCompletedValue() < frameCtx->uFenceValue)
    {
        g_GfxData.pFence->SetEventOnCompletion(frameCtx->uFenceValue, g_GfxData.hFenceEvent);
        HANDLE waitableObjects[] = { g_GfxData.hSwapChainWaitableObject, g_GfxData.hFenceEvent };
        ::WaitForMultipleObjects(2, waitableObjects, TRUE, INFINITE);
    }
    else
    {
        ::WaitForSingleObject(g_GfxData.hSwapChainWaitableObject, INFINITE);
    }

    return frameCtx;
}

// C callbacks for descriptor allocator
static void DescriptorAllocCallback(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu)
{
    g_SrvDescHeapAlloc.Alloc(out_cpu, out_gpu);
}

static void DescriptorFreeCallback(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu)
{
    g_SrvDescHeapAlloc.Free(cpu, gpu);
}

// Internal API - Create DX12 device
bool ImPlatform_Gfx_CreateDevice_DX12(HWND hWnd, ImPlatform_GfxData_DX12* pData)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC1 sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = IM_DX12_NUM_BACK_BUFFERS;
    sd.Width = 0;
    sd.Height = 0;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    sd.Scaling = DXGI_SCALING_STRETCH;
    sd.Stereo = FALSE;

    // Enable debug layer
#ifdef DX12_ENABLE_DEBUG_LAYER
    ID3D12Debug* pdx12Debug = NULL;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pdx12Debug))))
        pdx12Debug->EnableDebugLayer();
#endif

    // Create device
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    if (D3D12CreateDevice(NULL, featureLevel, IID_PPV_ARGS(&pData->pDevice)) != S_OK)
        return false;

#ifdef DX12_ENABLE_DEBUG_LAYER
    if (pdx12Debug != NULL)
    {
        pdx12Debug->Release();
        pdx12Debug = NULL;
    }
#endif

    // Create command queue
    {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 1;
        if (pData->pDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&pData->pCommandQueue)) != S_OK)
            return false;
    }

    // Create command allocators
    for (UINT i = 0; i < IM_DX12_NUM_FRAMES_IN_FLIGHT; i++)
        if (pData->pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pData->frameContext[i].pCommandAllocator)) != S_OK)
            return false;

    // Create command list
    if (pData->pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pData->frameContext[0].pCommandAllocator, NULL, IID_PPV_ARGS(&pData->pCommandList)) != S_OK ||
        pData->pCommandList->Close() != S_OK)
        return false;

    // Create fence
    if (pData->pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pData->pFence)) != S_OK)
        return false;

    pData->hFenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (pData->hFenceEvent == NULL)
        return false;

    // Create swap chain
    {
        IDXGIFactory5* dxgiFactory = NULL;
        IDXGISwapChain1* swapChain1 = NULL;
        if (CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)) != S_OK)
            return false;

        BOOL allow_tearing = FALSE;
        dxgiFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(allow_tearing));
        pData->bSwapChainTearingSupport = (allow_tearing == TRUE);
        if (pData->bSwapChainTearingSupport)
            sd.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

        if (dxgiFactory->CreateSwapChainForHwnd(pData->pCommandQueue, hWnd, &sd, NULL, NULL, &swapChain1) != S_OK)
            return false;
        if (swapChain1->QueryInterface(IID_PPV_ARGS(&pData->pSwapChain)) != S_OK)
            return false;
        if (pData->bSwapChainTearingSupport)
            dxgiFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);

        swapChain1->Release();
        dxgiFactory->Release();
        pData->pSwapChain->SetMaximumFrameLatency(IM_DX12_NUM_BACK_BUFFERS);
        pData->hSwapChainWaitableObject = pData->pSwapChain->GetFrameLatencyWaitableObject();
    }

    // Create descriptor heaps
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = IM_DX12_NUM_BACK_BUFFERS;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask = 1;
        if (pData->pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pData->pRtvDescHeap)) != S_OK)
            return false;

        SIZE_T rtvDescriptorSize = pData->pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = pData->pRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < IM_DX12_NUM_BACK_BUFFERS; i++)
        {
            pData->renderTargetDescriptor[i] = rtvHandle;
            rtvHandle.ptr += rtvDescriptorSize;
        }
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = APP_SRV_HEAP_SIZE;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (pData->pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pData->pSrvDescHeap)) != S_OK)
            return false;
    }

    // Create descriptor heap allocator
    g_SrvDescHeapAlloc.Create(pData->pDevice, pData->pSrvDescHeap);
    pData->pSrvDescHeapAlloc = &g_SrvDescHeapAlloc;

    pData->bSwapChainOccluded = false;
    pData->uFrameIndex = 0;
    pData->uFenceLastSignaledValue = 0;

    CreateRenderTarget();
    return true;
}

// Internal API - Cleanup DX12 device
void ImPlatform_Gfx_CleanupDevice_DX12(ImPlatform_GfxData_DX12* pData)
{
    CleanupRenderTarget();

    if (pData->pSwapChain) { pData->pSwapChain->SetFullscreenState(false, NULL); pData->pSwapChain->Release(); pData->pSwapChain = NULL; }
    if (pData->hSwapChainWaitableObject != NULL) { CloseHandle(pData->hSwapChainWaitableObject); }
    for (UINT i = 0; i < IM_DX12_NUM_FRAMES_IN_FLIGHT; i++)
        if (pData->frameContext[i].pCommandAllocator) { pData->frameContext[i].pCommandAllocator->Release(); pData->frameContext[i].pCommandAllocator = NULL; }
    if (pData->pCommandQueue) { pData->pCommandQueue->Release(); pData->pCommandQueue = NULL; }
    if (pData->pCommandList) { pData->pCommandList->Release(); pData->pCommandList = NULL; }
    if (pData->pRtvDescHeap) { pData->pRtvDescHeap->Release(); pData->pRtvDescHeap = NULL; }
    if (pData->pSrvDescHeap) { g_SrvDescHeapAlloc.Destroy(); pData->pSrvDescHeap->Release(); pData->pSrvDescHeap = NULL; }
    if (pData->pFence) { pData->pFence->Release(); pData->pFence = NULL; }
    if (pData->hFenceEvent) { CloseHandle(pData->hFenceEvent); pData->hFenceEvent = NULL; }
    if (pData->pDevice) { pData->pDevice->Release(); pData->pDevice = NULL; }

#ifdef DX12_ENABLE_DEBUG_LAYER
    IDXGIDebug1* pDebug = NULL;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug))))
    {
        pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
        pDebug->Release();
    }
#endif
}

// Internal API - Resize handling
void ImPlatform_Gfx_OnResize_DX12(ImPlatform_GfxData_DX12* pData, unsigned int uWidth, unsigned int uHeight)
{
    if (pData->pDevice != NULL && pData->pSwapChain != NULL && uWidth > 0 && uHeight > 0)
    {
        CleanupRenderTarget(); // Waits for pending operations
        DXGI_SWAP_CHAIN_DESC1 desc = {};
        pData->pSwapChain->GetDesc1(&desc);
        HRESULT result = pData->pSwapChain->ResizeBuffers(0, uWidth, uHeight, desc.Format, desc.Flags);
        IM_ASSERT(SUCCEEDED(result) && "Failed to resize swapchain.");
        CreateRenderTarget();
        (void)result;
    }
}

// Internal API - Get DX12 gfx data
ImPlatform_GfxData_DX12* ImPlatform_Gfx_GetData_DX12(void)
{
    return &g_GfxData;
}

// ImPlatform API - InitGfxAPI
IMPLATFORM_API bool ImPlatform_InitGfxAPI(void)
{
#ifdef _WIN32
    HWND hWnd = ImPlatform_App_GetHWND();
    if (!ImPlatform_Gfx_CreateDevice_DX12(hWnd, &g_GfxData))
    {
        ImPlatform_Gfx_CleanupDevice_DX12(&g_GfxData);
        return false;
    }
    return true;
#else
    return false;
#endif
}

// ImPlatform API - InitGfx
IMPLATFORM_API bool ImPlatform_InitGfx(void)
{
    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = g_GfxData.pDevice;
    init_info.CommandQueue = g_GfxData.pCommandQueue;
    init_info.NumFramesInFlight = IM_DX12_NUM_FRAMES_IN_FLIGHT;
    init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;
    init_info.SrvDescriptorHeap = g_GfxData.pSrvDescHeap;
    init_info.SrvDescriptorAllocFn = DescriptorAllocCallback;
    init_info.SrvDescriptorFreeFn = DescriptorFreeCallback;

    if (!ImGui_ImplDX12_Init(&init_info))
        return false;

    return true;
}

// ImPlatform API - GfxCheck
IMPLATFORM_API bool ImPlatform_GfxCheck(void)
{
    // Handle window screen locked or minimized (flip model swapchains don't report DXGI_STATUS_OCCLUDED when minimized)
    if ((g_GfxData.bSwapChainOccluded && g_GfxData.pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) || ::IsIconic(ImPlatform_App_GetHWND()))
    {
        ::Sleep(10);
        return false;
    }
    g_GfxData.bSwapChainOccluded = false;

    return true;
}

// ImPlatform API - GfxAPINewFrame
IMPLATFORM_API void ImPlatform_GfxAPINewFrame(void)
{
    ImGui_ImplDX12_NewFrame();
}

// Frame context used by the frame being recorded (between GfxAPIClear and GfxAPISwapBuffer)
static ImPlatform_FrameContext_DX12* g_CurrentFrameCtx = NULL;

// ImPlatform API - GfxAPIClear
IMPLATFORM_API bool ImPlatform_GfxAPIClear(ImVec4 const vClearColor)
{
    ImPlatform_FrameContext_DX12* frameCtx = WaitForNextFrameContext();
    g_CurrentFrameCtx = frameCtx;
    UINT backBufferIdx = g_GfxData.pSwapChain->GetCurrentBackBufferIndex();
    frameCtx->pCommandAllocator->Reset();

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = g_GfxData.pRenderTargetResource[backBufferIdx];
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    g_GfxData.pCommandList->Reset(frameCtx->pCommandAllocator, NULL);
    g_GfxData.pCommandList->ResourceBarrier(1, &barrier);

    // Clear
    const float clear_color_with_alpha[4] = {
        vClearColor.x * vClearColor.w,
        vClearColor.y * vClearColor.w,
        vClearColor.z * vClearColor.w,
        vClearColor.w
    };
    g_GfxData.pCommandList->ClearRenderTargetView(g_GfxData.renderTargetDescriptor[backBufferIdx], clear_color_with_alpha, 0, NULL);
    g_GfxData.pCommandList->OMSetRenderTargets(1, &g_GfxData.renderTargetDescriptor[backBufferIdx], FALSE, NULL);
    g_GfxData.pCommandList->SetDescriptorHeaps(1, &g_GfxData.pSrvDescHeap);
    return true;
}

// Wrapper for ImGui_ImplDX12_RenderDrawData that stores draw_data for custom shader callbacks
static void ImPlatform_RenderDrawDataWrapper(ImDrawData* draw_data, ID3D12GraphicsCommandList* command_list)
{
    // Store draw data for custom shader callbacks (needed for multi-viewport)
    g_CurrentDrawData = draw_data;

    ImGui_ImplDX12_RenderDrawData(draw_data, command_list);

    // NOTE: Don't clear g_CurrentDrawData here - it will be updated by the next render call
}

// ImPlatform API - GfxAPIRender
IMPLATFORM_API bool ImPlatform_GfxAPIRender(ImVec4 const vClearColor)
{
    (void)vClearColor;

    ImPlatform_RenderDrawDataWrapper(ImGui::GetDrawData(), g_GfxData.pCommandList);
    ImPlatform_DX12_FlushWork();        // Uploads, dispatches and offscreen draws recorded this frame run first.

    // Submit the main viewport before rendering secondary viewports (same order as upstream example_win32_directx12)
    UINT backBufferIdx = g_GfxData.pSwapChain->GetCurrentBackBufferIndex();
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = g_GfxData.pRenderTargetResource[backBufferIdx];
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    g_GfxData.pCommandList->ResourceBarrier(1, &barrier);
    g_GfxData.pCommandList->Close();

    g_GfxData.pCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&g_GfxData.pCommandList);

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

// Custom viewport render callback that wraps ImGui's callback to use our wrapper
static void (*g_OriginalRendererRenderWindow)(ImGuiViewport*, void*) = nullptr;

static void ImPlatform_RendererRenderWindowWrapper(ImGuiViewport* viewport, void* renderer_arg)
{
    // Store the draw data before calling the original renderer
    // NOTE: We DON'T clear this after - it will be cleared when the next viewport renders
    // or when the frame is done. This ensures custom shader callbacks have access to it.
    g_CurrentDrawData = viewport->DrawData;

    // Call the original ImGui DX12 renderer
    if (g_OriginalRendererRenderWindow)
        g_OriginalRendererRenderWindow(viewport, renderer_arg);

    // NOTE: Don't clear g_CurrentDrawData here - callbacks need it!
}

// ImPlatform API - GfxViewportPost
IMPLATFORM_API void ImPlatform_GfxViewportPost(void)
{
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        // Replace the renderer callback with our wrapper (only once)
        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        if (!g_OriginalRendererRenderWindow && platform_io.Renderer_RenderWindow)
        {
            g_OriginalRendererRenderWindow = platform_io.Renderer_RenderWindow;
            platform_io.Renderer_RenderWindow = ImPlatform_RendererRenderWindowWrapper;
        }

        ImGui::RenderPlatformWindowsDefault();
    }
}
#endif

// ImPlatform API - GfxAPISwapBuffer
IMPLATFORM_API bool ImPlatform_GfxAPISwapBuffer(void)
{
    // The main viewport command list was submitted by ImPlatform_GfxAPIRender, secondary viewports by RenderPlatformWindowsDefault()
    g_GfxData.pCommandQueue->Signal(g_GfxData.pFence, ++g_GfxData.uFenceLastSignaledValue);
    if (g_CurrentFrameCtx)
        g_CurrentFrameCtx->uFenceValue = g_GfxData.uFenceLastSignaledValue;
    g_CurrentFrameCtx = NULL;

    // Present
    HRESULT hr = g_GfxData.pSwapChain->Present(1, 0); // Present with vsync
    //HRESULT hr = g_GfxData.pSwapChain->Present(0, g_GfxData.bSwapChainTearingSupport ? DXGI_PRESENT_ALLOW_TEARING : 0); // Present without vsync
    g_GfxData.bSwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    g_GfxData.uFrameIndex++;

    return true;
}

// ImPlatform API - ShutdownGfxAPI
IMPLATFORM_API void ImPlatform_ShutdownGfxAPI(void)
{
    ImPlatform_DX12_FlushWork();
    WaitForPendingOperations();
    ImPlatform_DX12_ReleaseAll();
}

// ImPlatform API - ShutdownWindow
IMPLATFORM_API void ImPlatform_ShutdownWindow(void)
{
    ImGui_ImplDX12_Shutdown();
    ImPlatform_Gfx_CleanupDevice_DX12(&g_GfxData);
}

// ============================================================================
// Texture Creation API - DirectX 12 Implementation
// ============================================================================

// Helper to get D3D12 format from ImPlatform format
static DXGI_FORMAT ImPlatform_GetD3D12Format(ImPlatform_PixelFormat format, int* out_bytes_per_pixel)
{
    switch (format)
    {
    case ImPlatform_PixelFormat_R8:
        *out_bytes_per_pixel = 1;
        return DXGI_FORMAT_R8_UNORM;
    case ImPlatform_PixelFormat_RG8:
        *out_bytes_per_pixel = 2;
        return DXGI_FORMAT_R8G8_UNORM;
    case ImPlatform_PixelFormat_RGB8:
        // D3D12 doesn't have RGB8, use RGBA8 instead
        *out_bytes_per_pixel = 4;
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case ImPlatform_PixelFormat_RGBA8:
        *out_bytes_per_pixel = 4;
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case ImPlatform_PixelFormat_R16:
        *out_bytes_per_pixel = 2;
        return DXGI_FORMAT_R16_UNORM;
    case ImPlatform_PixelFormat_RG16:
        *out_bytes_per_pixel = 4;
        return DXGI_FORMAT_R16G16_UNORM;
    case ImPlatform_PixelFormat_RGBA16:
        *out_bytes_per_pixel = 8;
        return DXGI_FORMAT_R16G16B16A16_UNORM;
    case ImPlatform_PixelFormat_R32F:
        *out_bytes_per_pixel = 4;
        return DXGI_FORMAT_R32_FLOAT;
    case ImPlatform_PixelFormat_RG32F:
        *out_bytes_per_pixel = 8;
        return DXGI_FORMAT_R32G32_FLOAT;
    case ImPlatform_PixelFormat_RGBA32F:
        *out_bytes_per_pixel = 16;
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
#if IMPLATFORM_GFX_SUPPORT_BGRA_FORMATS
    case ImPlatform_PixelFormat_BGRA8:
        *out_bytes_per_pixel = 4;
        return DXGI_FORMAT_B8G8R8A8_UNORM;
#endif
#if IMPLATFORM_GFX_SUPPORT_HALF_FLOAT_FORMATS
    case ImPlatform_PixelFormat_R16F:
        *out_bytes_per_pixel = 2;
        return DXGI_FORMAT_R16_FLOAT;
    case ImPlatform_PixelFormat_RG16F:
        *out_bytes_per_pixel = 4;
        return DXGI_FORMAT_R16G16_FLOAT;
    case ImPlatform_PixelFormat_RGBA16F:
        *out_bytes_per_pixel = 8;
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
#endif
#if IMPLATFORM_GFX_SUPPORT_RGB_EXTENDED
    case ImPlatform_PixelFormat_RGB16:
        // No DXGI 3-channel 16-bit unorm — expand to RGBA16
        *out_bytes_per_pixel = 8;
        return DXGI_FORMAT_R16G16B16A16_UNORM;
    case ImPlatform_PixelFormat_RGB16F:
        // No DXGI 3-channel 16F — expand to RGBA16F
        *out_bytes_per_pixel = 8;
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case ImPlatform_PixelFormat_RGB32F:
        *out_bytes_per_pixel = 12;
        return DXGI_FORMAT_R32G32B32_FLOAT;
#endif
#if IMPLATFORM_GFX_SUPPORT_SRGB_FORMATS
    case ImPlatform_PixelFormat_RGB8_SRGB:
        // No DXGI 3-channel sRGB — expand to RGBA8_SRGB
        *out_bytes_per_pixel = 4;
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case ImPlatform_PixelFormat_RGBA8_SRGB:
        *out_bytes_per_pixel = 4;
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
#endif
#if IMPLATFORM_GFX_SUPPORT_PACKED_FORMATS
    case ImPlatform_PixelFormat_RGB10A2:
        *out_bytes_per_pixel = 4;
        return DXGI_FORMAT_R10G10B10A2_UNORM;
#endif
#if IMPLATFORM_GFX_SUPPORT_DEPTH_FORMATS
    case ImPlatform_PixelFormat_D16:
        *out_bytes_per_pixel = 2;
        return DXGI_FORMAT_D16_UNORM;
    case ImPlatform_PixelFormat_D32F:
        *out_bytes_per_pixel = 4;
        return DXGI_FORMAT_D32_FLOAT;
    case ImPlatform_PixelFormat_D24S8:
        *out_bytes_per_pixel = 4;
        return DXGI_FORMAT_D24_UNORM_S8_UINT;
    case ImPlatform_PixelFormat_D32FS8:
        *out_bytes_per_pixel = 8;
        return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
#endif
#if IMPLATFORM_GFX_SUPPORT_INTEGER_FORMATS
    case ImPlatform_PixelFormat_R8UI:
        *out_bytes_per_pixel = 1;
        return DXGI_FORMAT_R8_UINT;
    case ImPlatform_PixelFormat_R8I:
        *out_bytes_per_pixel = 1;
        return DXGI_FORMAT_R8_SINT;
    case ImPlatform_PixelFormat_R16UI:
        *out_bytes_per_pixel = 2;
        return DXGI_FORMAT_R16_UINT;
    case ImPlatform_PixelFormat_R16I:
        *out_bytes_per_pixel = 2;
        return DXGI_FORMAT_R16_SINT;
    case ImPlatform_PixelFormat_R32UI:
        *out_bytes_per_pixel = 4;
        return DXGI_FORMAT_R32_UINT;
    case ImPlatform_PixelFormat_R32I:
        *out_bytes_per_pixel = 4;
        return DXGI_FORMAT_R32_SINT;
#endif
    default:
        *out_bytes_per_pixel = 4;
        return DXGI_FORMAT_R8G8B8A8_UNORM;
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

// Texture3D stub for D3D12.
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

IMPLATFORM_API bool ImPlatform_CopyBackbuffer(ImTextureID dst) { (void)dst; return false; }
IMPLATFORM_API void ImPlatform_GetBackbufferSize(unsigned int* width, unsigned int* height)
{
    if (width)  *width  = g_ImPlatform_BackbufferW;
    if (height) *height = g_ImPlatform_BackbufferH;
}

// ============================================================================
// Custom Vertex/Index Buffer Management API - DirectX 12
// ============================================================================
// Note: DX12 implementations are stubs - full implementation requires:
// - Command allocator/list management
// - Upload heaps for buffer data
// - Resource barriers and transitions
// - Descriptor heaps for SRVs/CBVs

// ============================================================================
// Custom Shader System API - DirectX 12
// ============================================================================

// Internal shader structure for DX12
struct ImPlatform_ShaderData_DX12
{
    ID3DBlob* pBlob;                    // Compiled shader bytecode
    ImPlatform_ShaderStage stage;       // Shader stage (vertex/pixel/compute)
    ID3D12PipelineState* pComputePSO;   // Compute stage: created on the first dispatch
};

// Internal shader program structure for DX12
struct ImPlatform_ShaderProgramData_DX12
{
    ID3DBlob* pVSBlob;                  // Vertex shader bytecode
    ID3DBlob* pPSBlob;                  // Pixel shader bytecode
    ID3D12PipelineState* pPSO;          // Pipeline State Object
    ID3D12RootSignature* pRootSignature; // Root signature
    ID3D12Resource* pPixelConstantBuffer; // GPU constant buffer for pixel shader
    void* pPixelConstantData;           // CPU-side copy of pixel shader constants
    size_t pixelConstantDataSize;       // Size of pixel constant data
    bool pixelConstantDataDirty;        // Whether constant data needs upload
};

// Constant buffer structure for projection matrix (vertex shader)
struct VERTEX_CONSTANT_BUFFER_DX12
{
    float mvp[4][4];
};

// Translate IMPLATFORM_SHADER_COMPILE_* flags to D3DCompile flags.
static UINT ImPlatform_DX12_TranslateCompileFlags(unsigned int compile_flags)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
    if (compile_flags & IMPLATFORM_SHADER_COMPILE_SKIP_OPTIMIZATION) flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
    if (compile_flags & IMPLATFORM_SHADER_COMPILE_OPTIMIZATION_LOW)  flags |= D3DCOMPILE_OPTIMIZATION_LEVEL0;
    if (compile_flags & IMPLATFORM_SHADER_COMPILE_OPTIMIZATION_HIGH) flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
    if (compile_flags & IMPLATFORM_SHADER_COMPILE_DEBUG)             flags |= D3DCOMPILE_DEBUG;
    return flags;
}

// Try to load cached DXBC bytecode into a fresh ID3DBlob. Returns NULL on miss.
static ID3DBlob* ImPlatform_DX12_TryLoadCachedBytecode(
    const ImPlatform_ShaderDesc* desc,
    const char* entry, const char* target, size_t source_len,
    char* out_cache_path, size_t out_cache_path_size)
{
    out_cache_path[0] = '\0';
    if (!desc->cache_key || !desc->cache_key[0])
        return NULL;

    unsigned long long hash = ImPlatform_ShaderCacheHashSource(
        desc->source_code, source_len, entry, target);
    ImPlatform_ShaderCacheBuildPath(out_cache_path, out_cache_path_size,
                                    desc->cache_key, entry, "dxbc", hash);

    size_t cached_size = 0;
    void* cached = ImPlatform_ShaderCacheLoad(out_cache_path, &cached_size);
    if (!cached || cached_size == 0)
    {
        if (cached) free(cached);
        return NULL;
    }

    ID3DBlob* blob = NULL;
    HRESULT hr = D3DCreateBlob(cached_size, &blob);
    if (FAILED(hr) || !blob)
    {
        free(cached);
        return NULL;
    }
    memcpy(blob->GetBufferPointer(), cached, cached_size);
    free(cached);
    fprintf(stderr, "[ImPlatform shader cache] HIT  %s/%s (%zu bytes)\n",
            desc->cache_key, entry, cached_size);
    return blob;
}

IMPLATFORM_API ImPlatform_Shader ImPlatform_CreateShader(const ImPlatform_ShaderDesc* desc)
{
    if (!desc || (!desc->source_code && !desc->bytecode))
        return NULL;

    // Accept HLSL source OR pre-compiled DXBC bytecode.
    if (desc->source_code && desc->format != ImPlatform_ShaderFormat_HLSL)
        return NULL;

    ImPlatform_ShaderData_DX12* shader_data = new ImPlatform_ShaderData_DX12();
    memset(shader_data, 0, sizeof(ImPlatform_ShaderData_DX12));
    shader_data->stage = desc->stage;

    HRESULT hr = S_OK;

    // Resolve target profile
    const char* target = NULL;
    if (desc->stage == ImPlatform_ShaderStage_Vertex)
        target = "vs_5_0";
    else if (desc->stage == ImPlatform_ShaderStage_Fragment)
        target = "ps_5_0";
    else if (desc->stage == ImPlatform_ShaderStage_Compute)
        target = "cs_5_0";
    else
    {
        delete shader_data;
        return NULL;
    }

    if (desc->bytecode && desc->bytecode_size > 0)
    {
        // Bytecode path: wrap user bytes in an ID3DBlob.
        hr = D3DCreateBlob(desc->bytecode_size, &shader_data->pBlob);
        if (FAILED(hr) || !shader_data->pBlob)
        {
            fprintf(stderr, "DX12 shader bytecode blob allocation failed (size=%u)\n", desc->bytecode_size);
            delete shader_data;
            return NULL;
        }
        memcpy(shader_data->pBlob->GetBufferPointer(), desc->bytecode, desc->bytecode_size);
    }
    else
    {
        // Source-compile path with optional disk cache.
        const char* entry = desc->entry_point ? desc->entry_point : "main";
        size_t source_len = strlen(desc->source_code);
        char cache_path[512];

        // 1) Try the cache first
        shader_data->pBlob = ImPlatform_DX12_TryLoadCachedBytecode(
            desc, entry, target, source_len, cache_path, sizeof(cache_path));

        // 2) Cache miss (or no cache_key): compile from source
        if (!shader_data->pBlob)
        {
            if (desc->cache_key && desc->cache_key[0])
            {
                fprintf(stderr, "[ImPlatform shader cache] MISS %s/%s -- compiling...\n",
                        desc->cache_key, entry);
            }

            ID3DBlob* pErrorBlob = NULL;
            UINT flags = ImPlatform_DX12_TranslateCompileFlags(desc->compile_flags);
            hr = D3DCompile(
                desc->source_code, source_len,
                NULL, NULL, NULL,
                entry, target, flags, 0,
                &shader_data->pBlob, &pErrorBlob);

            if (FAILED(hr))
            {
                if (pErrorBlob)
                {
                    fprintf(stderr, "DX12 Shader compilation failed: %s\n", (char*)pErrorBlob->GetBufferPointer());
                    pErrorBlob->Release();
                }
                delete shader_data;
                return NULL;
            }
            if (pErrorBlob) pErrorBlob->Release();

            // 3) Save to cache for next launch
            if (desc->cache_key && desc->cache_key[0] && shader_data->pBlob && cache_path[0])
            {
                if (ImPlatform_ShaderCacheSave(cache_path,
                                               shader_data->pBlob->GetBufferPointer(),
                                               shader_data->pBlob->GetBufferSize()))
                {
                    fprintf(stderr, "[ImPlatform shader cache] SAVE %s/%s (%zu bytes)\n",
                            desc->cache_key, entry, (size_t)shader_data->pBlob->GetBufferSize());
                }
            }
        }
    }

    return (ImPlatform_Shader)shader_data;
}

IMPLATFORM_API void ImPlatform_DestroyShader(ImPlatform_Shader shader)
{
    if (!shader)
        return;

    ImPlatform_ShaderData_DX12* shader_data = (ImPlatform_ShaderData_DX12*)shader;

    if (shader_data->pBlob)
        shader_data->pBlob->Release();
    if (shader_data->pComputePSO)
        ImPlatform_DX12_DeferRelease(shader_data->pComputePSO);     // Dispatches recorded but not yet submitted still use it.

    delete shader_data;
}

IMPLATFORM_API ImPlatform_ShaderProgram ImPlatform_CreateShaderProgram(ImPlatform_Shader vertex_shader, ImPlatform_Shader fragment_shader)
{
    if (!vertex_shader || !fragment_shader)
        return NULL;

    ImPlatform_ShaderData_DX12* vs_data = (ImPlatform_ShaderData_DX12*)vertex_shader;
    ImPlatform_ShaderData_DX12* ps_data = (ImPlatform_ShaderData_DX12*)fragment_shader;

    if (vs_data->stage != ImPlatform_ShaderStage_Vertex || ps_data->stage != ImPlatform_ShaderStage_Fragment)
        return NULL;

    ImPlatform_ShaderProgramData_DX12* program = new ImPlatform_ShaderProgramData_DX12();
    memset(program, 0, sizeof(ImPlatform_ShaderProgramData_DX12));

    program->pVSBlob = vs_data->pBlob;
    program->pPSBlob = ps_data->pBlob;

    // Add ref to keep shader blobs alive
    if (program->pVSBlob)
        program->pVSBlob->AddRef();
    if (program->pPSBlob)
        program->pPSBlob->AddRef();

    // Create Root Signature
    // Root signature MUST be compatible with ImGui's DX12 backend root signature!
    // ImGui's layout:
    // - Root parameter 0: 32-bit constants (16 values) for projection matrix - register(b0) VS
    // - Root parameter 1: Descriptor table for texture SRV - register(t0) PS
    //
    // Our layout (compatible superset):
    // - Root parameter 0: 32-bit constants (16 values) for projection matrix - register(b0) VS [MATCHES ImGui]
    // - Root parameter 1: Descriptor table for texture SRV - register(t0) PS [MATCHES ImGui]
    // - Root parameter 2: CBV for custom pixel shader uniforms - register(b1) PS [OUR ADDITION]

    D3D12_DESCRIPTOR_RANGE descRange = {};
    descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descRange.NumDescriptors = 1;
    descRange.BaseShaderRegister = 0; // register(t0)
    descRange.RegisterSpace = 0;
    descRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER rootParameters[3];

    // Root parameter 0: 32-bit constants for vertex shader (projection matrix)
    // MATCHES ImGui's backend parameter 0
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParameters[0].Constants.ShaderRegister = 0; // register(b0)
    rootParameters[0].Constants.RegisterSpace = 0;
    rootParameters[0].Constants.Num32BitValues = 16; // 4x4 matrix = 16 floats
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    // Root parameter 1: Descriptor table for texture SRV
    // MATCHES ImGui's backend parameter 1
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &descRange;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Root parameter 2: Pixel shader CBV for custom uniforms (OUR ADDITION)
    // Uses register(b1) to avoid conflict with vertex shader's register(b0)
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[2].Descriptor.ShaderRegister = 1; // register(b1) NOT b0!
    rootParameters[2].Descriptor.RegisterSpace = 0;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Static sampler
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.MipLODBias = 0.f;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.MinLOD = 0.f;
    sampler.MaxLOD = 0.f;
    sampler.ShaderRegister = 0; // register(s0)
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = 3;
    rootSigDesc.pParameters = rootParameters;
    rootSigDesc.NumStaticSamplers = 1;
    rootSigDesc.pStaticSamplers = &sampler;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ID3DBlob* pSignatureBlob = NULL;
    ID3DBlob* pErrorBlob = NULL;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignatureBlob, &pErrorBlob);
    if (FAILED(hr))
    {
        if (pErrorBlob)
        {
            fprintf(stderr, "DX12 Root signature serialization failed: %s\n", (char*)pErrorBlob->GetBufferPointer());
            pErrorBlob->Release();
        }
        delete program;
        return NULL;
    }

    hr = g_GfxData.pDevice->CreateRootSignature(
        0,
        pSignatureBlob->GetBufferPointer(),
        pSignatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&program->pRootSignature)
    );

    pSignatureBlob->Release();

    if (FAILED(hr))
    {
        fprintf(stderr, "DX12 CreateRootSignature failed\n");
        if (program->pVSBlob) program->pVSBlob->Release();
        if (program->pPSBlob) program->pPSBlob->Release();
        delete program;
        return NULL;
    }

    // Create Pipeline State Object (PSO)
    // Define input layout matching ImDrawVert structure
    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0,  8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = program->pRootSignature;
    psoDesc.VS = { program->pVSBlob->GetBufferPointer(), program->pVSBlob->GetBufferSize() };
    psoDesc.PS = { program->pPSBlob->GetBufferPointer(), program->pPSBlob->GetBufferSize() };
    psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    psoDesc.RasterizerState.MultisampleEnable = FALSE;
    psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.InputLayout = { inputLayout, 3 };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;  // Match the swapchain format
    psoDesc.SampleDesc.Count = 1;

    // The ImGui-layout pipeline serves the ImGui draw callbacks only; a program written for ImPlatform_DrawMesh (its own
    // vertex layout and root signature) does not fit it and keeps pPSO NULL.
    hr = g_GfxData.pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&program->pPSO));
    if (FAILED(hr))
        program->pPSO = NULL;

    return (ImPlatform_ShaderProgram)program;
}

IMPLATFORM_API void ImPlatform_DestroyShaderProgram(ImPlatform_ShaderProgram program)
{
    if (!program)
        return;

    ImPlatform_ShaderProgramData_DX12* program_data = (ImPlatform_ShaderProgramData_DX12*)program;

    // Wait for GPU to finish using these resources
    // This is important to avoid crashes during shutdown
    if (g_GfxData.pDevice && g_GfxData.pCommandQueue)
    {
        ImPlatform_DX12_FlushWork();        // Draws recorded but not yet submitted use its pipelines.
        // Flush the command queue to ensure GPU is done with these resources
        ID3D12Fence* pFence = nullptr;
        if (SUCCEEDED(g_GfxData.pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence))))
        {
            HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            if (hEvent)
            {
                g_GfxData.pCommandQueue->Signal(pFence, 1);
                pFence->SetEventOnCompletion(1, hEvent);
                WaitForSingleObject(hEvent, INFINITE);
                CloseHandle(hEvent);
            }
            pFence->Release();
        }
    }

    if (program_data->pPixelConstantBuffer)
        program_data->pPixelConstantBuffer->Release();
    if (program_data->pPixelConstantData)
        free(program_data->pPixelConstantData);
    if (program_data->pPSO)
        program_data->pPSO->Release();
    if (program_data->pRootSignature)
        program_data->pRootSignature->Release();
    if (program_data->pVSBlob)
        program_data->pVSBlob->Release();
    if (program_data->pPSBlob)
        program_data->pPSBlob->Release();

    delete program_data;
}

IMPLATFORM_API void ImPlatform_UseShaderProgram(ImPlatform_ShaderProgram /*program*/)
{
    // In DX12, shader binding is done via PSO in the draw callback
    // This function is a no-op for DX12
    // The actual PSO binding happens in ImPlatform_SetCustomShader callback
}

IMPLATFORM_API bool ImPlatform_SetShaderUniform(ImPlatform_ShaderProgram program, const char* /*name*/, const void* data, unsigned int size)
{
    if (!program || !data || size == 0)
        return false;

    ImPlatform_ShaderProgramData_DX12* program_data = (ImPlatform_ShaderProgramData_DX12*)program;

    // Allocate or reallocate pixel constant buffer storage if needed
    if (program_data->pPixelConstantData == nullptr || program_data->pixelConstantDataSize < size)
    {
        if (program_data->pPixelConstantData)
            free(program_data->pPixelConstantData);

        program_data->pPixelConstantData = malloc(size);
        program_data->pixelConstantDataSize = size;

        // Recreate GPU constant buffer with new size
        if (program_data->pPixelConstantBuffer)
            program_data->pPixelConstantBuffer->Release();

        // Create upload heap for constant buffer
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment = 0;
        resourceDesc.Width = (size + 255) & ~255; // Round up to 256-byte boundary for CBVs
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        g_GfxData.pDevice->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&program_data->pPixelConstantBuffer)
        );
    }

    // Copy data to CPU storage
    memcpy(program_data->pPixelConstantData, data, size);
    program_data->pixelConstantDataDirty = true;

    return true;
}

IMPLATFORM_API bool ImPlatform_SetShaderTexture(ImPlatform_ShaderProgram /*program*/, const char* /*name*/, unsigned int /*slot*/, ImTextureID /*texture*/)
{
    // Texture binding in DX12 is done via descriptor tables in the draw callback
    // For now, this is a stub - texture support can be added later
    return false;
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

    // For DirectX, we accumulate all uniforms into a single buffer
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
        // Upload the accumulated uniform block to the shader
        ImPlatform_SetShaderUniform(program, "pixelBuffer", g_UniformBlockData, (unsigned int)g_UniformBlockSize);

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

// ImDrawCallback handler to activate a custom shader
static void ImPlatform_SetCustomShader(const ImDrawList* parent_list, const ImDrawCmd* cmd)
{
    ImPlatform_ShaderProgram program = (ImPlatform_ShaderProgram)cmd->UserCallbackData;
    if (!program)
        return;

    ImPlatform_ShaderProgramData_DX12* program_data = (ImPlatform_ShaderProgramData_DX12*)program;

    // Get the command list from ImGui's render state (works for all viewports)
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    ImGui_ImplDX12_RenderState* render_state = (ImGui_ImplDX12_RenderState*)platform_io.Renderer_RenderState;
    if (!render_state || !render_state->CommandList)
        return;

    ID3D12GraphicsCommandList* cmdList = render_state->CommandList;
    if (!program_data->pPSO)
        return;     // Not an ImGui-layout program.

    // Set Pipeline State Object and Root Signature
    cmdList->SetPipelineState(program_data->pPSO);
    cmdList->SetGraphicsRootSignature(program_data->pRootSignature);

    // IMPORTANT: SetGraphicsRootSignature() invalidates ALL root parameter bindings!
    // We must rebind everything, even though our root signature is compatible with ImGui's.

    // Rebind root parameter 0: projection matrix (32-bit constants)
    // Find which viewport owns this draw list
    ImDrawData* draw_data = nullptr;

    // First, try the cached draw data
    if (g_CurrentDrawData)
    {
        draw_data = g_CurrentDrawData;
    }
#ifdef IMGUI_HAS_VIEWPORT
    else
    {
        // Fallback: search through all viewports to find which one owns this draw list
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
#endif
    if (!draw_data)
        draw_data = ImGui::GetDrawData();

    if (!draw_data)
        return;

    float L = draw_data->DisplayPos.x;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float T = draw_data->DisplayPos.y;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
    float mvp[4][4] =
    {
        { 2.0f/(R-L),   0.0f,           0.0f,       0.0f },
        { 0.0f,         2.0f/(T-B),     0.0f,       0.0f },
        { 0.0f,         0.0f,           0.5f,       0.0f },
        { (R+L)/(L-R),  (T+B)/(B-T),    0.5f,       1.0f },
    };
    cmdList->SetGraphicsRoot32BitConstants(0, 16, mvp, 0);

    // Rebind root parameter 1: texture descriptor table
    D3D12_GPU_DESCRIPTOR_HANDLE texture_handle = {};
    texture_handle.ptr = (UINT64)cmd->GetTexID();
    cmdList->SetGraphicsRootDescriptorTable(1, texture_handle);

    // Upload and bind pixel shader constant buffer if dirty (root parameter 2)
    if (program_data->pPixelConstantBuffer && program_data->pixelConstantDataDirty && program_data->pPixelConstantData)
    {
        // Map and upload constant buffer data
        void* pMappedData = nullptr;
        D3D12_RANGE readRange = { 0, 0 }; // We don't read from this resource on the CPU
        if (SUCCEEDED(program_data->pPixelConstantBuffer->Map(0, &readRange, &pMappedData)))
        {
            memcpy(pMappedData, program_data->pPixelConstantData, program_data->pixelConstantDataSize);
            program_data->pPixelConstantBuffer->Unmap(0, nullptr);
            program_data->pixelConstantDataDirty = false;
        }

        // Bind pixel shader constant buffer to root parameter 2 (our custom uniforms)
        D3D12_GPU_VIRTUAL_ADDRESS cbAddress = program_data->pPixelConstantBuffer->GetGPUVirtualAddress();
        cmdList->SetGraphicsRootConstantBufferView(2, cbAddress);
    }
}

// Activate a custom shader immediately (for use inside draw callbacks).
IMPLATFORM_API void ImPlatform_BeginCustomShader_Render(ImPlatform_ShaderProgram program)
{
    if (!program)
        return;

    ImPlatform_ShaderProgramData_DX12* program_data = (ImPlatform_ShaderProgramData_DX12*)program;

    // Get the command list from ImGui's render state (works for all viewports)
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    ImGui_ImplDX12_RenderState* render_state = (ImGui_ImplDX12_RenderState*)platform_io.Renderer_RenderState;
    if (!render_state || !render_state->CommandList)
        return;

    ID3D12GraphicsCommandList* cmdList = render_state->CommandList;
    if (!program_data->pPSO)
        return;     // Not an ImGui-layout program.

    // Set Pipeline State Object and Root Signature
    cmdList->SetPipelineState(program_data->pPSO);
    cmdList->SetGraphicsRootSignature(program_data->pRootSignature);

    // IMPORTANT: SetGraphicsRootSignature() invalidates ALL root parameter bindings!
    // We must rebind everything, even though our root signature is compatible with ImGui's.

    // Rebind root parameter 0: projection matrix (32-bit constants)
    ImDrawData* draw_data = g_CurrentDrawData;
    if (!draw_data)
        draw_data = ImGui::GetDrawData();
    if (!draw_data)
        return;

    float L = draw_data->DisplayPos.x;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float T = draw_data->DisplayPos.y;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
    float mvp[4][4] =
    {
        { 2.0f/(R-L),   0.0f,           0.0f,       0.0f },
        { 0.0f,         2.0f/(T-B),     0.0f,       0.0f },
        { 0.0f,         0.0f,           0.5f,       0.0f },
        { (R+L)/(L-R),  (T+B)/(B-T),    0.5f,       1.0f },
    };
    cmdList->SetGraphicsRoot32BitConstants(0, 16, mvp, 0);

    // Note: Texture descriptor table (root parameter 1) is NOT rebound here
    // because we don't have access to the current texture ID outside a draw command.
    // The caller is responsible for binding textures if needed.

    // Upload and bind pixel shader constant buffer if dirty (root parameter 2)
    if (program_data->pPixelConstantBuffer && program_data->pixelConstantDataDirty && program_data->pPixelConstantData)
    {
        // Map and upload constant buffer data
        void* pMappedData = nullptr;
        D3D12_RANGE readRange = { 0, 0 }; // We don't read from this resource on the CPU
        if (SUCCEEDED(program_data->pPixelConstantBuffer->Map(0, &readRange, &pMappedData)))
        {
            memcpy(pMappedData, program_data->pPixelConstantData, program_data->pixelConstantDataSize);
            program_data->pPixelConstantBuffer->Unmap(0, nullptr);
            program_data->pixelConstantDataDirty = false;
        }

        // Bind pixel shader constant buffer to root parameter 2 (our custom uniforms)
        D3D12_GPU_VIRTUAL_ADDRESS cbAddress = program_data->pPixelConstantBuffer->GetGPUVirtualAddress();
        cmdList->SetGraphicsRootConstantBufferView(2, cbAddress);
    }
    else if (program_data->pPixelConstantBuffer)
    {
        // Even if not dirty, ensure the constant buffer is bound
        D3D12_GPU_VIRTUAL_ADDRESS cbAddress = program_data->pPixelConstantBuffer->GetGPUVirtualAddress();
        cmdList->SetGraphicsRootConstantBufferView(2, cbAddress);
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
// Sampler Override API - DirectX 12
// ============================================================================
// Uses the standard DrawCallback_SetSamplerLinear/Nearest callbacks of the ImGui DX12 backend.
// Only the filter is honored: the wrap mode stays clamp.

IMPLATFORM_API void ImPlatform_PushSampler(ImPlatform_TextureFilter filter, ImPlatform_TextureWrap /*wrap*/)
{
    ImPlatform_PushSampler_StdCallbacks(filter);
}

IMPLATFORM_API void ImPlatform_PopSampler(void)
{
    ImPlatform_PopSampler_StdCallbacks();
}


// ============================================================================
// DX12 resources, work submission, compute, readback and depth-tested drawing
// ============================================================================
// Work outside the frame command list (texture uploads, compute dispatches, offscreen draws, readback copies) is
// recorded into a work command list on the same queue. It is submitted before the frame's ImGui command list
// (ImPlatform_GfxAPIRender) or when a readback needs it, so GPU order matches call order. Upload memory comes from a
// paged arena recycled by fence value; destroyed resources and descriptors are released once the GPU is done with
// them. Every texture created here rests in a shader-readable state between operations.

#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    // ---- Fences and deferred release ---------------------------------------------------------------------------
    UINT64 ImP12_NextFence() { return g_GfxData.uFenceLastSignaledValue + 1; }
    bool   ImP12_Done(UINT64 v) { return g_GfxData.pFence && g_GfxData.pFence->GetCompletedValue() >= v; }
    void   ImP12_Wait(UINT64 v)
    {
        if (ImP12_Done(v))
            return;
        g_GfxData.pFence->SetEventOnCompletion(v, g_GfxData.hFenceEvent);
        ::WaitForSingleObject(g_GfxData.hFenceEvent, INFINITE);
    }

    struct ImP12_Simple_Alloc       // Fixed-size descriptor heap with a free list (CPU-only or shader-visible).
    {
        ID3D12DescriptorHeap* heap = NULL;
        UINT stride = 0, capacity = 0;
        std::vector<UINT> free_list;
        bool create(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT count, bool shader_visible)
        {
            D3D12_DESCRIPTOR_HEAP_DESC d = {};
            d.Type = type;
            d.NumDescriptors = count;
            d.Flags = shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            if (FAILED(g_GfxData.pDevice->CreateDescriptorHeap(&d, IID_PPV_ARGS(&heap))))
                return false;
            stride = g_GfxData.pDevice->GetDescriptorHandleIncrementSize(type);
            capacity = count;
            free_list.resize(count);
            for (UINT i = 0; i < count; i++)
                free_list[i] = count - 1 - i;
            return true;
        }
        bool alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out)
        {
            if (free_list.empty())
                return false;
            const UINT i = free_list.back();
            free_list.pop_back();
            out->ptr = heap->GetCPUDescriptorHandleForHeapStart().ptr + (SIZE_T)i * stride;
            return true;
        }
        void free(D3D12_CPU_DESCRIPTOR_HANDLE h)
        {
            if (h.ptr)
                free_list.push_back((UINT)((h.ptr - heap->GetCPUDescriptorHandleForHeapStart().ptr) / stride));
        }
    };

    struct ImP12_Texture
    {
        ID3D12Resource* res = NULL;
        ID3D12Resource* depth = NULL;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        UINT w = 0, h = 0, bpp = 0;
        D3D12_RESOURCE_STATES rest = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, cur = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        D3D12_CPU_DESCRIPTOR_HANDLE srv_vis_cpu = {};       // ImGui's shader-visible heap (the ImTextureID).
        D3D12_GPU_DESCRIPTOR_HANDLE srv_vis_gpu = {};
        D3D12_CPU_DESCRIPTOR_HANDLE srv = {}, uav = {}, rtv = {}, dsv = {};   // CPU-only copies for tables / targets.
    };

    struct ImP12_Release { UINT64 fence; IUnknown* res; ImP12_Texture tex; bool texture; };

    struct ImP12_State
    {
        bool ready = false, failed = false;
        // Work list.
        ID3D12CommandAllocator* allocators[4] = {};
        UINT64 allocator_fence[4] = {};
        int allocator = 0;
        ID3D12GraphicsCommandList* list = NULL;
        bool open = false;
        // Upload arena.
        struct page { ID3D12Resource* res; uint8_t* cpu; UINT64 size, used, fence; };
        std::vector<page> pages;
        int page_index = -1;
        // Descriptors.
        ImP12_Simple_Alloc cpu_srv;     // SRV/UAV CPU copies.
        ImP12_Simple_Alloc rtv, dsv;
        ID3D12DescriptorHeap* transient = NULL;     // Shader-visible ring for per-op tables.
        UINT transient_stride = 0, transient_cap = 0, transient_head = 0;
        UINT64 transient_wrap_fence = 0;
        D3D12_CPU_DESCRIPTOR_HANDLE null_srv = {}, null_uav = {};
        // Root signatures.
        ID3D12RootSignature* compute_rs = NULL;
        ID3D12RootSignature* graphics_rs = NULL;
        // Registry and deferred releases.
        std::unordered_map<UINT64, ImP12_Texture> textures;
        std::vector<ImP12_Release> releases, pending_releases;
        ImP12_Texture* active_rt = NULL;
    };
    ImP12_State g_P12;

    // ---- Root signatures ---------------------------------------------------------------------------------------
    ID3D12RootSignature* ImP12_MakeRootSignature(const D3D12_ROOT_SIGNATURE_DESC& d)
    {
        ID3DBlob* blob = NULL;
        ID3DBlob* err = NULL;
        ID3D12RootSignature* rs = NULL;
        if (SUCCEEDED(D3D12SerializeRootSignature(&d, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err)))
            g_GfxData.pDevice->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rs));
        else if (err)
            fprintf(stderr, "[ImPlatform DX12] root signature: %s\n", (const char*)err->GetBufferPointer());
        if (blob) blob->Release();
        if (err) err->Release();
        return rs;
    }

    D3D12_STATIC_SAMPLER_DESC ImP12_LinearClamp()
    {
        D3D12_STATIC_SAMPLER_DESC s = {};
        s.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        s.AddressU = s.AddressV = s.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        s.MaxLOD = D3D12_FLOAT32_MAX;
        s.ShaderRegister = 0;
        s.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        return s;
    }

    bool ImP12_Init()
    {
        if (g_P12.ready || g_P12.failed)
            return g_P12.ready;
        g_P12.failed = true;
        ID3D12Device* dev = g_GfxData.pDevice;
        if (!dev || !g_GfxData.pCommandQueue || !g_GfxData.pFence)
            return false;
        for (auto& a : g_P12.allocators)
            if (FAILED(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&a))))
                return false;
        if (FAILED(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_P12.allocators[0], NULL, IID_PPV_ARGS(&g_P12.list))))
            return false;
        g_P12.list->Close();
        if (!g_P12.cpu_srv.create(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 16384, false) || !g_P12.rtv.create(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 256, false)
            || !g_P12.dsv.create(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 256, false))
            return false;
        D3D12_DESCRIPTOR_HEAP_DESC td = {};
        td.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        td.NumDescriptors = 65536;
        td.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(dev->CreateDescriptorHeap(&td, IID_PPV_ARGS(&g_P12.transient))))
            return false;
        g_P12.transient_stride = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        g_P12.transient_cap = td.NumDescriptors;
        // Null descriptors fill unused table slots.
        g_P12.cpu_srv.alloc(&g_P12.null_srv);
        g_P12.cpu_srv.alloc(&g_P12.null_uav);
        D3D12_SHADER_RESOURCE_VIEW_DESC ns = {};
        ns.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        ns.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        ns.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        ns.Texture2D.MipLevels = 1;
        dev->CreateShaderResourceView(NULL, &ns, g_P12.null_srv);
        D3D12_UNORDERED_ACCESS_VIEW_DESC nu = {};
        nu.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        nu.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        dev->CreateUnorderedAccessView(NULL, NULL, &nu, g_P12.null_uav);
        // Compute: b0 root CBV, t0..t7 table, u0..u7 table, s0 linear clamp.
        {
            D3D12_DESCRIPTOR_RANGE ranges[2] = {};
            ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            ranges[0].NumDescriptors = 8;
            ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            ranges[1].NumDescriptors = 8;
            D3D12_ROOT_PARAMETER p[3] = {};
            p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            p[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            for (int i = 0; i < 2; i++)
            {
                p[1 + i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                p[1 + i].DescriptorTable.NumDescriptorRanges = 1;
                p[1 + i].DescriptorTable.pDescriptorRanges = &ranges[i];
                p[1 + i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            }
            const D3D12_STATIC_SAMPLER_DESC smp = ImP12_LinearClamp();
            D3D12_ROOT_SIGNATURE_DESC d = {};
            d.NumParameters = 3;
            d.pParameters = p;
            d.NumStaticSamplers = 1;
            d.pStaticSamplers = &smp;
            g_P12.compute_rs = ImP12_MakeRootSignature(d);
        }
        // Graphics: VS b0 CBV, PS b0 CBV, PS t0..t3 table, s0 linear clamp.
        {
            D3D12_DESCRIPTOR_RANGE range = {};
            range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            range.NumDescriptors = 4;
            D3D12_ROOT_PARAMETER p[3] = {};
            p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            p[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
            p[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            p[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
            p[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            p[2].DescriptorTable.NumDescriptorRanges = 1;
            p[2].DescriptorTable.pDescriptorRanges = &range;
            p[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
            D3D12_STATIC_SAMPLER_DESC smp = ImP12_LinearClamp();
            smp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
            D3D12_ROOT_SIGNATURE_DESC d = {};
            d.NumParameters = 3;
            d.pParameters = p;
            d.NumStaticSamplers = 1;
            d.pStaticSamplers = &smp;
            d.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
            g_P12.graphics_rs = ImP12_MakeRootSignature(d);
        }
        if (!g_P12.compute_rs || !g_P12.graphics_rs)
            return false;
        g_P12.failed = false;
        g_P12.ready = true;
        return true;
    }

    // ---- Work list ---------------------------------------------------------------------------------------------
    void ImP12_ProcessReleases()
    {
        for (size_t i = 0; i < g_P12.releases.size();)
        {
            ImP12_Release& r = g_P12.releases[i];
            if (!ImP12_Done(r.fence))
            {
                i++;
                continue;
            }
            if (r.res)
                r.res->Release();
            if (r.texture)
            {
                if (r.tex.res) r.tex.res->Release();
                if (r.tex.depth) r.tex.depth->Release();
                if (r.tex.srv_vis_cpu.ptr) g_SrvDescHeapAlloc.Free(r.tex.srv_vis_cpu, r.tex.srv_vis_gpu);
                g_P12.cpu_srv.free(r.tex.srv);
                g_P12.cpu_srv.free(r.tex.uav);
                g_P12.rtv.free(r.tex.rtv);
                g_P12.dsv.free(r.tex.dsv);
            }
            r = g_P12.releases.back();
            g_P12.releases.pop_back();
        }
    }

    ID3D12GraphicsCommandList* ImP12_List()
    {
        if (!ImP12_Init())
            return NULL;
        if (!g_P12.open)
        {
            const int a = g_P12.allocator;
            ImP12_Wait(g_P12.allocator_fence[a]);
            g_P12.allocators[a]->Reset();
            g_P12.list->Reset(g_P12.allocators[a], NULL);
            g_P12.list->SetDescriptorHeaps(1, &g_P12.transient);
            g_P12.open = true;
        }
        return g_P12.list;
    }

    // Debug layer messages (debug builds): printed and cleared at each submission.
    void ImP12_DumpMessages()
    {
#ifdef DX12_ENABLE_DEBUG_LAYER
        ID3D12InfoQueue* q = NULL;
        if (FAILED(g_GfxData.pDevice->QueryInterface(IID_PPV_ARGS(&q))) || !q)
            return;
        for (UINT64 i = 0, n = q->GetNumStoredMessages(); i < n; i++)
        {
            SIZE_T len = 0;
            q->GetMessage(i, NULL, &len);
            std::vector<char> buf(len);
            D3D12_MESSAGE* m = (D3D12_MESSAGE*)buf.data();
            if (SUCCEEDED(q->GetMessage(i, m, &len)) && m->Severity <= D3D12_MESSAGE_SEVERITY_WARNING)
                fprintf(stderr, "[D3D12] %s\n", m->pDescription);
        }
        q->ClearStoredMessages();
        q->Release();
#endif
    }

    // Submits recorded work; returns the fence value that marks its completion (0 when nothing was recorded).
    UINT64 ImP12_Flush()
    {
        if (!g_P12.open)
            return 0;
        ImP12_DumpMessages();
        g_P12.list->Close();
        g_GfxData.pCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&g_P12.list);
        const UINT64 fence = ++g_GfxData.uFenceLastSignaledValue;
        g_GfxData.pCommandQueue->Signal(g_GfxData.pFence, fence);
        g_P12.allocator_fence[g_P12.allocator] = fence;
        g_P12.allocator = (g_P12.allocator + 1) % 4;
        g_P12.open = false;
        for (auto& p : g_P12.pages)
            if (p.fence == UINT64_MAX)
                p.fence = fence;            // Used by this submission.
        if (g_P12.page_index >= 0)
            g_P12.page_index = -1;          // Next allocation picks a free page.
        for (ImP12_Release& r : g_P12.pending_releases)
        {
            r.fence = fence;
            g_P12.releases.push_back(r);
        }
        g_P12.pending_releases.clear();
        ImP12_ProcessReleases();
        return fence;
    }

    void ImP12_DeferRelease(IUnknown* res)
    {
        ImP12_Release r = {};
        r.res = res;
        r.fence = ImP12_NextFence();
        (g_P12.open ? g_P12.pending_releases : g_P12.releases).push_back(r);
    }

    // ---- Upload arena ------------------------------------------------------------------------------------------
    struct ImP12_Upload { ID3D12Resource* res; UINT64 offset; uint8_t* cpu; D3D12_GPU_VIRTUAL_ADDRESS gpu; };

    bool ImP12_Alloc(UINT64 size, UINT64 align, ImP12_Upload* out)
    {
        auto fits = [&](const ImP12_State::page& p) { return ((p.used + align - 1) & ~(align - 1)) + size <= p.size; };
        if (g_P12.page_index < 0 || !fits(g_P12.pages[(size_t)g_P12.page_index]))
        {
            g_P12.page_index = -1;
            for (size_t i = 0; i < g_P12.pages.size(); i++)
            {
                ImP12_State::page& p = g_P12.pages[i];
                if (p.fence != UINT64_MAX && ImP12_Done(p.fence) && p.size >= size)
                {
                    p.used = 0;
                    p.fence = UINT64_MAX;
                    g_P12.page_index = (int)i;
                    break;
                }
            }
            if (g_P12.page_index < 0)
            {
                ImP12_State::page p = {};
                p.size = std::max<UINT64>(32ull << 20, (size + 0xFFFF) & ~0xFFFFull);
                D3D12_HEAP_PROPERTIES hp = {};
                hp.Type = D3D12_HEAP_TYPE_UPLOAD;
                D3D12_RESOURCE_DESC rd = {};
                rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
                rd.Width = p.size;
                rd.Height = 1;
                rd.DepthOrArraySize = 1;
                rd.MipLevels = 1;
                rd.SampleDesc.Count = 1;
                rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
                if (FAILED(g_GfxData.pDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                                                      IID_PPV_ARGS(&p.res))))
                    return false;
                D3D12_RANGE none = { 0, 0 };
                p.res->Map(0, &none, (void**)&p.cpu);
                p.fence = UINT64_MAX;
                g_P12.pages.push_back(p);
                g_P12.page_index = (int)g_P12.pages.size() - 1;
            }
        }
        ImP12_State::page& p = g_P12.pages[(size_t)g_P12.page_index];
        p.fence = UINT64_MAX;
        const UINT64 at = (p.used + align - 1) & ~(align - 1);
        p.used = at + size;
        out->res = p.res;
        out->offset = at;
        out->cpu = p.cpu + at;
        out->gpu = p.res->GetGPUVirtualAddress() + at;
        return true;
    }

    // ---- Transient descriptor tables ---------------------------------------------------------------------------
    D3D12_GPU_DESCRIPTOR_HANDLE ImP12_Table(const D3D12_CPU_DESCRIPTOR_HANDLE* src, UINT count)
    {
        if (g_P12.transient_head + count > g_P12.transient_cap)
        {
            // Wrap: everything recorded before the previous wrap must be complete.
            ImP12_Wait(g_P12.transient_wrap_fence);
            g_P12.transient_wrap_fence = ImP12_NextFence();
            g_P12.transient_head = 0;
        }
        const UINT at = g_P12.transient_head;
        g_P12.transient_head += count;
        D3D12_CPU_DESCRIPTOR_HANDLE dst = g_P12.transient->GetCPUDescriptorHandleForHeapStart();
        dst.ptr += (SIZE_T)at * g_P12.transient_stride;
        for (UINT i = 0; i < count; i++)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE d = dst;
            d.ptr += (SIZE_T)i * g_P12.transient_stride;
            g_GfxData.pDevice->CopyDescriptorsSimple(1, d, src[i], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }
        D3D12_GPU_DESCRIPTOR_HANDLE gpu = g_P12.transient->GetGPUDescriptorHandleForHeapStart();
        gpu.ptr += (UINT64)at * g_P12.transient_stride;
        return gpu;
    }

    // ---- Textures ----------------------------------------------------------------------------------------------
    ImP12_Texture* ImP12_Find(ImTextureID id)
    {
        auto it = g_P12.textures.find((UINT64)id);
        return it == g_P12.textures.end() ? NULL : &it->second;
    }

    void ImP12_Transition(ID3D12GraphicsCommandList* list, ImP12_Texture& t, D3D12_RESOURCE_STATES to)
    {
        if (t.cur == to)
            return;
        D3D12_RESOURCE_BARRIER b = {};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = t.res;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = t.cur;
        b.Transition.StateAfter = to;
        list->ResourceBarrier(1, &b);
        t.cur = to;
    }

    int ImP12_Bpp(DXGI_FORMAT f)
    {
        switch (f)
        {
        case DXGI_FORMAT_R8_UNORM: return 1;
        case DXGI_FORMAT_R8G8_UNORM: case DXGI_FORMAT_R16_UNORM: case DXGI_FORMAT_R16_FLOAT: return 2;
        case DXGI_FORMAT_R8G8B8A8_UNORM: case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: case DXGI_FORMAT_B8G8R8A8_UNORM: case DXGI_FORMAT_R16G16_UNORM:
        case DXGI_FORMAT_R16G16_FLOAT: case DXGI_FORMAT_R32_FLOAT: case DXGI_FORMAT_R10G10B10A2_UNORM: return 4;
        case DXGI_FORMAT_R16G16B16A16_UNORM: case DXGI_FORMAT_R16G16B16A16_FLOAT: case DXGI_FORMAT_R32G32_FLOAT: return 8;
        case DXGI_FORMAT_R32G32B32_FLOAT: return 12;
        case DXGI_FORMAT_R32G32B32A32_FLOAT: return 16;
        default: return 0;
        }
    }

    // Creates a texture with its ImGui SRV and CPU-side views. flags: UAV and/or render target.
    ImTextureID ImP12_CreateTexture(const ImPlatform_TextureDesc* desc, D3D12_RESOURCE_FLAGS flags, bool depth)
    {
        if (!desc || !desc->width || !desc->height || !ImP12_Init())
            return NULL;
        int bpp_unused = 0;
        const DXGI_FORMAT format = ImPlatform_GetD3D12Format(desc->format, &bpp_unused);
        ImP12_Texture t;
        t.format = format;
        t.w = desc->width;
        t.h = desc->height;
        t.bpp = (UINT)ImP12_Bpp(format);
        t.rest = t.cur = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        D3D12_HEAP_PROPERTIES hp = {};
        hp.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC rd = {};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        rd.Width = t.w;
        rd.Height = t.h;
        rd.DepthOrArraySize = 1;
        rd.MipLevels = 1;
        rd.Format = format;
        rd.SampleDesc.Count = 1;
        rd.Flags = flags;
        D3D12_CLEAR_VALUE cv = {};
        cv.Format = format;
        if (FAILED(g_GfxData.pDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, t.cur,
                                                              (flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) ? &cv : NULL, IID_PPV_ARGS(&t.res))))
            return NULL;
        g_SrvDescHeapAlloc.Alloc(&t.srv_vis_cpu, &t.srv_vis_gpu);
        D3D12_SHADER_RESOURCE_VIEW_DESC sd = {};
        sd.Format = format;
        sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        sd.Texture2D.MipLevels = 1;
        g_GfxData.pDevice->CreateShaderResourceView(t.res, &sd, t.srv_vis_cpu);
        if (!g_P12.cpu_srv.alloc(&t.srv))
            return NULL;
        g_GfxData.pDevice->CreateShaderResourceView(t.res, &sd, t.srv);
        if (flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
        {
            g_P12.cpu_srv.alloc(&t.uav);
            D3D12_UNORDERED_ACCESS_VIEW_DESC ud = {};
            ud.Format = format;
            ud.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            g_GfxData.pDevice->CreateUnorderedAccessView(t.res, NULL, &ud, t.uav);
        }
        if (flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
        {
            g_P12.rtv.alloc(&t.rtv);
            g_GfxData.pDevice->CreateRenderTargetView(t.res, NULL, t.rtv);
        }
        if (depth)
        {
            D3D12_RESOURCE_DESC dd = rd;
            dd.Format = DXGI_FORMAT_D32_FLOAT;
            dd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
            D3D12_CLEAR_VALUE dv = {};
            dv.Format = DXGI_FORMAT_D32_FLOAT;
            dv.DepthStencil.Depth = 1.0f;
            if (FAILED(g_GfxData.pDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &dd, D3D12_RESOURCE_STATE_DEPTH_WRITE, &dv, IID_PPV_ARGS(&t.depth))))
                return NULL;
            g_P12.dsv.alloc(&t.dsv);
            g_GfxData.pDevice->CreateDepthStencilView(t.depth, NULL, t.dsv);
        }
        const ImTextureID id = (ImTextureID)t.srv_vis_gpu.ptr;
        g_P12.textures[(UINT64)id] = t;
        return id;
    }

    bool ImP12_Upload_Region(ImP12_Texture& t, const void* data, UINT x, UINT y, UINT w, UINT h)
    {
        if (!data || !w || !h || x + w > t.w || y + h > t.h || !t.bpp)
            return false;
        ID3D12GraphicsCommandList* list = ImP12_List();
        if (!list)
            return false;
        const UINT row = w * t.bpp, pitch = (row + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
        ImP12_Upload up;
        if (!ImP12_Alloc((UINT64)pitch * h, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, &up))
            return false;
        for (UINT r = 0; r < h; r++)
            memcpy(up.cpu + (size_t)r * pitch, (const uint8_t*)data + (size_t)r * row, row);
        D3D12_TEXTURE_COPY_LOCATION src = {}, dst = {};
        src.pResource = up.res;
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint.Offset = up.offset;
        src.PlacedFootprint.Footprint.Format = t.format;
        src.PlacedFootprint.Footprint.Width = w;
        src.PlacedFootprint.Footprint.Height = h;
        src.PlacedFootprint.Footprint.Depth = 1;
        src.PlacedFootprint.Footprint.RowPitch = pitch;
        dst.pResource = t.res;
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        ImP12_Transition(list, t, D3D12_RESOURCE_STATE_COPY_DEST);
        list->CopyTextureRegion(&dst, x, y, 0, &src, NULL);
        ImP12_Transition(list, t, t.rest);
        return true;
    }

    // ---- Readback ----------------------------------------------------------------------------------------------
    struct ImP12_Readback { ID3D12Resource* buffer; UINT64 fence; UINT w, h, bpp, pitch; };

    ImP12_Readback* ImP12_StartRead(ImTextureID texture, UINT x, UINT y, UINT w, UINT h)
    {
        ImP12_Texture* t = ImP12_Find(texture);
        if (!t || !w || !h || x + w > t->w || y + h > t->h || !t->bpp)
            return NULL;
        ID3D12GraphicsCommandList* list = ImP12_List();
        if (!list)
            return NULL;
        const UINT pitch = (w * t->bpp + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
        D3D12_HEAP_PROPERTIES hp = {};
        hp.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC rd = {};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = (UINT64)pitch * h;
        rd.Height = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels = 1;
        rd.SampleDesc.Count = 1;
        rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ID3D12Resource* buffer = NULL;
        if (FAILED(g_GfxData.pDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(&buffer))))
            return NULL;
        D3D12_TEXTURE_COPY_LOCATION src = {}, dst = {};
        src.pResource = t->res;
        src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.pResource = buffer;
        dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dst.PlacedFootprint.Footprint.Format = t->format;
        dst.PlacedFootprint.Footprint.Width = w;
        dst.PlacedFootprint.Footprint.Height = h;
        dst.PlacedFootprint.Footprint.Depth = 1;
        dst.PlacedFootprint.Footprint.RowPitch = pitch;
        const D3D12_BOX box = { x, y, 0, x + w, y + h, 1 };
        ImP12_Transition(list, *t, D3D12_RESOURCE_STATE_COPY_SOURCE);
        list->CopyTextureRegion(&dst, 0, 0, 0, &src, &box);
        ImP12_Transition(list, *t, t->rest);
        ImP12_Readback* r = new ImP12_Readback{ buffer, ImP12_Flush(), w, h, t->bpp, pitch };
        return r;
    }

    bool ImP12_FinishRead(ImP12_Readback* r, void* out, size_t out_size)
    {
        const size_t row = (size_t)r->w * r->bpp;
        if (out_size < row * r->h)
            return false;
        uint8_t* mapped = NULL;
        D3D12_RANGE range = { 0, (SIZE_T)r->pitch * r->h };
        if (FAILED(r->buffer->Map(0, &range, (void**)&mapped)))
            return false;
        for (UINT y = 0; y < r->h; y++)
            memcpy((uint8_t*)out + y * row, mapped + (size_t)y * r->pitch, row);
        D3D12_RANGE none = { 0, 0 };
        r->buffer->Unmap(0, &none);
        return true;
    }
}

// Submits pending work ahead of the frame's command list (called by ImPlatform_GfxAPIRender).
static void ImPlatform_DX12_FlushWork() { ImP12_Flush(); }
static void ImPlatform_DX12_DeferRelease(IUnknown* object) { ImP12_DeferRelease(object); }

IMPLATFORM_API ImTextureID ImPlatform_CreateTexture(const void* pixel_data, const ImPlatform_TextureDesc* desc)
{
    if (!desc || !pixel_data)
        return NULL;
    const ImTextureID id = ImP12_CreateTexture(desc, D3D12_RESOURCE_FLAG_NONE, false);
    ImP12_Texture* t = id ? ImP12_Find(id) : NULL;
    if (t && !ImP12_Upload_Region(*t, pixel_data, 0, 0, t->w, t->h))
        return NULL;
    return id;
}

IMPLATFORM_API bool ImPlatform_UpdateTexture(ImTextureID texture_id, const void* pixel_data, unsigned int x, unsigned int y,
                                             unsigned int width, unsigned int height)
{
    ImP12_Texture* t = ImP12_Find(texture_id);
    return t && ImP12_Upload_Region(*t, pixel_data, x, y, width, height);
}

IMPLATFORM_API bool ImPlatform_CopyTexture(ImTextureID dst_id, ImTextureID src_id)
{
    ImP12_Texture* dst = ImP12_Find(dst_id);
    ImP12_Texture* src = ImP12_Find(src_id);
    ID3D12GraphicsCommandList* list = dst && src && dst != src ? ImP12_List() : NULL;
    if (!list || dst->w != src->w || dst->h != src->h || dst->format != src->format)
        return false;
    ImP12_Transition(list, *dst, D3D12_RESOURCE_STATE_COPY_DEST);
    ImP12_Transition(list, *src, D3D12_RESOURCE_STATE_COPY_SOURCE);
    list->CopyResource(dst->res, src->res);
    ImP12_Transition(list, *dst, dst->rest);
    ImP12_Transition(list, *src, src->rest);
    return true;
}

IMPLATFORM_API void ImPlatform_DestroyTexture(ImTextureID texture_id)
{
    auto it = g_P12.textures.find((UINT64)texture_id);
    if (it == g_P12.textures.end())
        return;
    if (g_P12.active_rt == &it->second)
        g_P12.active_rt = NULL;
    ImP12_Release r = {};
    r.texture = true;
    r.tex = it->second;
    r.fence = ImP12_NextFence();        // Frame lists may still sample it: released after the next submission completes.
    (g_P12.open ? g_P12.pending_releases : g_P12.releases).push_back(r);
    g_P12.textures.erase(it);
}

IMPLATFORM_API ImTextureID ImPlatform_CreateRenderTexture(const ImPlatform_TextureDesc* desc)
{
    return ImP12_CreateTexture(desc, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, false);
}

IMPLATFORM_API ImTextureID ImPlatform_CreateRenderTextureWithDepth(const ImPlatform_TextureDesc* desc)
{
    return ImP12_CreateTexture(desc, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, true);
}

IMPLATFORM_API bool ImPlatform_BeginRenderToTexture(ImTextureID texture)
{
    ImP12_Texture* t = ImP12_Find(texture);
    ID3D12GraphicsCommandList* list = t && t->rtv.ptr ? ImP12_List() : NULL;
    if (!list)
        return false;
    ImP12_Transition(list, *t, D3D12_RESOURCE_STATE_RENDER_TARGET);
    const float clear[4] = { 0, 0, 0, 0 };
    list->ClearRenderTargetView(t->rtv, clear, 0, NULL);
    if (t->dsv.ptr)
        list->ClearDepthStencilView(t->dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);
    list->OMSetRenderTargets(1, &t->rtv, FALSE, t->dsv.ptr ? &t->dsv : NULL);
    D3D12_VIEWPORT vp = { 0, 0, (float)t->w, (float)t->h, 0, 1 };
    D3D12_RECT sc = { 0, 0, (LONG)t->w, (LONG)t->h };
    list->RSSetViewports(1, &vp);
    list->RSSetScissorRects(1, &sc);
    g_P12.active_rt = t;
    return true;
}

IMPLATFORM_API void ImPlatform_EndRenderToTexture(void)
{
    if (!g_P12.active_rt || !g_P12.open)
        return;
    ImP12_Transition(g_P12.list, *g_P12.active_rt, g_P12.active_rt->rest);
    g_P12.active_rt = NULL;
}

// ---- Vertex / index buffers (upload heap, persistently mapped) -------------------------------------------------
struct ImPlatform_BufferData_DX12
{
    ID3D12Resource* res;
    uint8_t* cpu;
    UINT64 size;
    ImPlatform_VertexBufferDesc vb_desc;
    ImPlatform_IndexBufferDesc ib_desc;
    std::vector<ImPlatform_VertexAttribute> attributes;
    std::vector<std::string> semantics;
    bool is_index;
};

static ImPlatform_BufferData_DX12* ImP12_CreateBuffer(const void* data, UINT64 size)
{
    if (!data || !size || !ImP12_Init())
        return NULL;
    ImPlatform_BufferData_DX12* b = new ImPlatform_BufferData_DX12();
    b->size = size;
    D3D12_HEAP_PROPERTIES hp = {};
    hp.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC rd = {};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = size;
    rd.Height = 1;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (FAILED(g_GfxData.pDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&b->res))))
    {
        delete b;
        return NULL;
    }
    D3D12_RANGE none = { 0, 0 };
    b->res->Map(0, &none, (void**)&b->cpu);
    memcpy(b->cpu, data, (size_t)size);
    return b;
}

IMPLATFORM_API ImPlatform_VertexBuffer ImPlatform_CreateVertexBuffer(const void* vertex_data, const ImPlatform_VertexBufferDesc* desc)
{
    if (!desc || !desc->attributes || !desc->attribute_count)
        return NULL;
    ImPlatform_BufferData_DX12* b = ImP12_CreateBuffer(vertex_data, (UINT64)desc->vertex_count * desc->vertex_stride);
    if (!b)
        return NULL;
    b->vb_desc = *desc;
    for (unsigned i = 0; i < desc->attribute_count; i++)
    {
        b->attributes.push_back(desc->attributes[i]);
        b->semantics.push_back(desc->attributes[i].semantic_name ? desc->attributes[i].semantic_name : "");
    }
    b->vb_desc.attributes = b->attributes.data();
    return (ImPlatform_VertexBuffer)b;
}

IMPLATFORM_API bool ImPlatform_UpdateVertexBuffer(ImPlatform_VertexBuffer vertex_buffer, const void* vertex_data, unsigned int vertex_count, unsigned int offset)
{
    ImPlatform_BufferData_DX12* b = (ImPlatform_BufferData_DX12*)vertex_buffer;
    const UINT64 stride = b ? b->vb_desc.vertex_stride : 0, at = (UINT64)offset * stride, bytes = (UINT64)vertex_count * stride;
    if (!b || !vertex_data || at + bytes > b->size)
        return false;
    memcpy(b->cpu + at, vertex_data, (size_t)bytes);    // Callers must not update a buffer the GPU still reads.
    return true;
}

IMPLATFORM_API void ImPlatform_DestroyVertexBuffer(ImPlatform_VertexBuffer vertex_buffer)
{
    ImPlatform_BufferData_DX12* b = (ImPlatform_BufferData_DX12*)vertex_buffer;
    if (!b)
        return;
    ImP12_DeferRelease(b->res);
    delete b;
}

IMPLATFORM_API ImPlatform_IndexBuffer ImPlatform_CreateIndexBuffer(const void* index_data, const ImPlatform_IndexBufferDesc* desc)
{
    if (!desc)
        return NULL;
    const UINT64 isize = desc->format == ImPlatform_IndexFormat_UInt16 ? 2 : 4;
    ImPlatform_BufferData_DX12* b = ImP12_CreateBuffer(index_data, (UINT64)desc->index_count * isize);
    if (!b)
        return NULL;
    b->ib_desc = *desc;
    b->is_index = true;
    return (ImPlatform_IndexBuffer)b;
}

IMPLATFORM_API bool ImPlatform_UpdateIndexBuffer(ImPlatform_IndexBuffer index_buffer, const void* index_data, unsigned int index_count, unsigned int offset)
{
    ImPlatform_BufferData_DX12* b = (ImPlatform_BufferData_DX12*)index_buffer;
    const UINT64 isize = b && b->ib_desc.format == ImPlatform_IndexFormat_UInt16 ? 2 : 4, at = (UINT64)offset * isize, bytes = (UINT64)index_count * isize;
    if (!b || !index_data || at + bytes > b->size)
        return false;
    memcpy(b->cpu + at, index_data, (size_t)bytes);
    return true;
}

IMPLATFORM_API void ImPlatform_DestroyIndexBuffer(ImPlatform_IndexBuffer index_buffer)
{
    ImPlatform_DestroyVertexBuffer((ImPlatform_VertexBuffer)index_buffer);
}

IMPLATFORM_API bool ImPlatform_CreateVertexInputLayout(ImPlatform_VertexBuffer vertex_buffer, ImPlatform_ShaderProgram program)
{
    return vertex_buffer && program;    // The layout is part of the pipeline state, built from the attributes at draw time.
}

IMPLATFORM_API void ImPlatform_BindVertexBuffer(ImPlatform_VertexBuffer) {}
IMPLATFORM_API void ImPlatform_BindIndexBuffer(ImPlatform_IndexBuffer) {}
IMPLATFORM_API void ImPlatform_BindBuffers(ImPlatform_VertexBuffer, ImPlatform_IndexBuffer) {}
IMPLATFORM_API void ImPlatform_DrawIndexed(unsigned int, unsigned int, unsigned int) {}     // Use ImPlatform_DrawMesh on DX12.

// ---- Readback ---------------------------------------------------------------------------------------------------
IMPLATFORM_API bool ImPlatform_SupportsReadback(void) { return true; }

IMPLATFORM_API bool ImPlatform_ReadTexture(ImTextureID texture, unsigned int x, unsigned int y, unsigned int width, unsigned int height,
                                           void* out_pixels, size_t out_size)
{
    if (!out_pixels)
        return false;
    ImP12_Readback* r = ImP12_StartRead(texture, x, y, width, height);
    if (!r)
        return false;
    ImP12_Wait(r->fence);
    const bool ok = ImP12_FinishRead(r, out_pixels, out_size);
    r->buffer->Release();
    delete r;
    return ok;
}

IMPLATFORM_API ImPlatform_Readback ImPlatform_BeginReadTexture(ImTextureID texture, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
    return (ImPlatform_Readback)ImP12_StartRead(texture, x, y, width, height);
}

IMPLATFORM_API int ImPlatform_PollReadback(ImPlatform_Readback readback, void* out_pixels, size_t out_size)
{
    ImP12_Readback* r = (ImP12_Readback*)readback;
    if (!r || !out_pixels)
        return -1;
    if (!ImP12_Done(r->fence))
        return 0;
    return ImP12_FinishRead(r, out_pixels, out_size) ? 1 : -1;
}

IMPLATFORM_API void ImPlatform_ReleaseReadback(ImPlatform_Readback readback)
{
    ImP12_Readback* r = (ImP12_Readback*)readback;
    if (!r)
        return;
    ImP12_DeferRelease(r->buffer);      // The copy may still be in flight.
    delete r;
}

// ---- Compute ----------------------------------------------------------------------------------------------------
IMPLATFORM_API bool ImPlatform_SupportsCompute(void) { return true; }

IMPLATFORM_API ImTextureID ImPlatform_CreateStorageTexture(const void* pixel_data, const ImPlatform_TextureDesc* desc)
{
    const ImTextureID id = ImP12_CreateTexture(desc, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, false);     // Committed: starts zeroed.
    ImP12_Texture* t = id ? ImP12_Find(id) : NULL;
    if (t && pixel_data && !ImP12_Upload_Region(*t, pixel_data, 0, 0, t->w, t->h))
        return NULL;
    return id;
}

IMPLATFORM_API bool ImPlatform_DispatchCompute(ImPlatform_Shader compute_shader, const ImTextureID* inputs, unsigned int input_count,
                                               const ImTextureID* outputs, unsigned int output_count, const void* constants,
                                               unsigned int constants_size, unsigned int groups_x, unsigned int groups_y, unsigned int groups_z)
{
    ImPlatform_ShaderData_DX12* cs = (ImPlatform_ShaderData_DX12*)compute_shader;
    if (!cs || cs->stage != ImPlatform_ShaderStage_Compute || !cs->pBlob || input_count > 8 || output_count > 8 || (input_count && !inputs)
        || (output_count && !outputs) || (constants_size && !constants))
        return false;
    ID3D12GraphicsCommandList* list = ImP12_List();
    if (!list)
        return false;
    if (!cs->pComputePSO)
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC pd = {};
        pd.pRootSignature = g_P12.compute_rs;
        pd.CS = { cs->pBlob->GetBufferPointer(), cs->pBlob->GetBufferSize() };
        if (FAILED(g_GfxData.pDevice->CreateComputePipelineState(&pd, IID_PPV_ARGS(&cs->pComputePSO))))
            return false;
    }
    ImP12_Texture* ins[8] = {};
    ImP12_Texture* outs[8] = {};
    D3D12_CPU_DESCRIPTOR_HANDLE srvs[8], uavs[8];
    for (int i = 0; i < 8; i++)
    {
        srvs[i] = g_P12.null_srv;
        uavs[i] = g_P12.null_uav;
    }
    for (unsigned i = 0; i < input_count; i++)
    {
        ins[i] = ImP12_Find(inputs[i]);
        if (!ins[i])
            return false;
        srvs[i] = ins[i]->srv;
    }
    for (unsigned i = 0; i < output_count; i++)
    {
        outs[i] = ImP12_Find(outputs[i]);
        if (!outs[i] || !outs[i]->uav.ptr)
            return false;       // Not a storage texture.
        uavs[i] = outs[i]->uav;
    }
    ImP12_Upload cb = {};
    if (constants_size)
    {
        if (!ImP12_Alloc((constants_size + 255u) & ~255u, 256, &cb))
            return false;
        memcpy(cb.cpu, constants, constants_size);
    }
    else if (!ImP12_Alloc(256, 256, &cb))
        return false;
    for (unsigned i = 0; i < input_count; i++)
        ImP12_Transition(list, *ins[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    for (unsigned i = 0; i < output_count; i++)
        ImP12_Transition(list, *outs[i], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    list->SetComputeRootSignature(g_P12.compute_rs);
    list->SetPipelineState(cs->pComputePSO);
    list->SetComputeRootConstantBufferView(0, cb.gpu);
    list->SetComputeRootDescriptorTable(1, ImP12_Table(srvs, 8));
    list->SetComputeRootDescriptorTable(2, ImP12_Table(uavs, 8));
    list->Dispatch(groups_x, groups_y, groups_z);
    for (unsigned i = 0; i < output_count; i++)
        ImP12_Transition(list, *outs[i], outs[i]->rest);
    for (unsigned i = 0; i < input_count; i++)
        ImP12_Transition(list, *ins[i], ins[i]->rest);
    return true;
}

// ---- Depth-tested drawing ---------------------------------------------------------------------------------------
IMPLATFORM_API bool ImPlatform_GfxDeviceLost(void)
{
    return g_GfxData.pDevice && g_GfxData.pDevice->GetDeviceRemovedReason() != S_OK;
}

IMPLATFORM_API bool ImPlatform_GfxDebugRemoveDevice(void)
{
    ID3D12Device5* d5 = NULL;
    if (!g_GfxData.pDevice || FAILED(g_GfxData.pDevice->QueryInterface(IID_PPV_ARGS(&d5))))
        return false;
    d5->RemoveDevice();
    d5->Release();
    return true;
}

IMPLATFORM_API bool ImPlatform_SupportsDepthDraw(void) { return true; }

IMPLATFORM_API bool ImPlatform_DrawMesh(const ImPlatform_DrawMeshDesc* desc)
{
    if (!desc || !desc->program || !desc->vertices || !desc->indices || desc->texture_count > 4 || (desc->texture_count && !desc->textures)
        || !g_P12.active_rt)
        return false;
    ImPlatform_ShaderProgramData_DX12* prog = (ImPlatform_ShaderProgramData_DX12*)desc->program;
    ImPlatform_BufferData_DX12* vb = (ImPlatform_BufferData_DX12*)desc->vertices;
    ImPlatform_BufferData_DX12* ib = (ImPlatform_BufferData_DX12*)desc->indices;
    if (!prog->pVSBlob || !prog->pPSBlob || vb->is_index || !ib->is_index || desc->index_count > ib->ib_desc.index_count)
        return false;
    ID3D12GraphicsCommandList* list = ImP12_List();
    if (!list)
        return false;
    // Pipeline per (program, layout, target format, depth, blend, cull).
    static std::unordered_map<std::string, ID3D12PipelineState*> pso_cache;
    ImP12_Texture& rt = *g_P12.active_rt;
    char key[256];
    snprintf(key, sizeof(key), "%p|%u|%d|%d%d%d%d|%d", (void*)prog, vb->vb_desc.vertex_stride, (int)rt.format, desc->depth_test, desc->depth_write,
             desc->alpha_blend, desc->cull_back, rt.dsv.ptr != 0);
    std::string k = key;
    for (const ImPlatform_VertexAttribute& a : vb->attributes)
        k += "|" + std::to_string((int)a.format) + ":" + std::to_string(a.offset);
    ID3D12PipelineState*& pso = pso_cache[k];
    if (!pso)
    {
        std::vector<D3D12_INPUT_ELEMENT_DESC> layout;
        std::vector<std::pair<std::string, UINT>> names;     // Semantic name and index ("TEXCOORD1" -> TEXCOORD, 1).
        for (size_t i = 0; i < vb->attributes.size(); i++)
        {
            std::string s = vb->semantics[i];
            UINT index = 0;
            while (!s.empty() && s.back() >= '0' && s.back() <= '9')
                s.pop_back();
            if (s.size() < vb->semantics[i].size())
                index = (UINT)atoi(vb->semantics[i].c_str() + s.size());
            names.push_back({ s, index });
        }
        for (size_t i = 0; i < vb->attributes.size(); i++)
        {
            DXGI_FORMAT f = DXGI_FORMAT_R32G32B32_FLOAT;
            switch (vb->attributes[i].format)
            {
            case ImPlatform_VertexFormat_Float: f = DXGI_FORMAT_R32_FLOAT; break;
            case ImPlatform_VertexFormat_Float2: f = DXGI_FORMAT_R32G32_FLOAT; break;
            case ImPlatform_VertexFormat_Float3: f = DXGI_FORMAT_R32G32B32_FLOAT; break;
            case ImPlatform_VertexFormat_Float4: f = DXGI_FORMAT_R32G32B32A32_FLOAT; break;
            case ImPlatform_VertexFormat_UByte4: f = DXGI_FORMAT_R8G8B8A8_UINT; break;
            case ImPlatform_VertexFormat_UByte4N: f = DXGI_FORMAT_R8G8B8A8_UNORM; break;
            default: break;
            }
            layout.push_back({ names[i].first.c_str(), names[i].second, f, 0, vb->attributes[i].offset, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
        }
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pd = {};
        pd.pRootSignature = g_P12.graphics_rs;
        pd.VS = { prog->pVSBlob->GetBufferPointer(), prog->pVSBlob->GetBufferSize() };
        pd.PS = { prog->pPSBlob->GetBufferPointer(), prog->pPSBlob->GetBufferSize() };
        pd.InputLayout = { layout.data(), (UINT)layout.size() };
        pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pd.NumRenderTargets = 1;
        pd.RTVFormats[0] = rt.format;
        pd.DSVFormat = rt.dsv.ptr ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_UNKNOWN;
        pd.SampleDesc.Count = 1;
        pd.SampleMask = UINT_MAX;
        pd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pd.RasterizerState.CullMode = desc->cull_back ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_NONE;
        pd.RasterizerState.FrontCounterClockwise = TRUE;
        pd.RasterizerState.DepthClipEnable = TRUE;
        D3D12_RENDER_TARGET_BLEND_DESC& b = pd.BlendState.RenderTarget[0];
        b.BlendEnable = desc->alpha_blend;
        b.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        b.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        b.BlendOp = D3D12_BLEND_OP_ADD;
        b.SrcBlendAlpha = D3D12_BLEND_ONE;
        b.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        b.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        b.LogicOp = D3D12_LOGIC_OP_NOOP;
        b.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        pd.DepthStencilState.DepthEnable = rt.dsv.ptr && (desc->depth_test || desc->depth_write);
        pd.DepthStencilState.DepthWriteMask = desc->depth_write ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        pd.DepthStencilState.DepthFunc = desc->depth_test ? D3D12_COMPARISON_FUNC_LESS_EQUAL : D3D12_COMPARISON_FUNC_ALWAYS;
        if (FAILED(g_GfxData.pDevice->CreateGraphicsPipelineState(&pd, IID_PPV_ARGS(&pso))))
        {
            pso = NULL;
            pso_cache.erase(k);
            return false;
        }
    }
    ImP12_Upload vcb = {}, pcb = {};
    if (!ImP12_Alloc(std::max(256u, (desc->vs_constants_size + 255u) & ~255u), 256, &vcb) || !ImP12_Alloc(std::max(256u, (desc->ps_constants_size + 255u) & ~255u), 256, &pcb))
        return false;
    if (desc->vs_constants_size)
        memcpy(vcb.cpu, desc->vs_constants, desc->vs_constants_size);
    if (desc->ps_constants_size)
        memcpy(pcb.cpu, desc->ps_constants, desc->ps_constants_size);
    D3D12_CPU_DESCRIPTOR_HANDLE srvs[4] = { g_P12.null_srv, g_P12.null_srv, g_P12.null_srv, g_P12.null_srv };
    for (unsigned i = 0; i < desc->texture_count; i++)
    {
        ImP12_Texture* t = ImP12_Find(desc->textures[i]);
        if (!t || t == &rt)
            return false;
        srvs[i] = t->srv;       // Resting textures are shader-readable.
    }
    list->SetGraphicsRootSignature(g_P12.graphics_rs);
    list->SetPipelineState(pso);
    list->SetGraphicsRootConstantBufferView(0, vcb.gpu);
    list->SetGraphicsRootConstantBufferView(1, pcb.gpu);
    list->SetGraphicsRootDescriptorTable(2, ImP12_Table(srvs, 4));
    D3D12_VERTEX_BUFFER_VIEW vbv = { vb->res->GetGPUVirtualAddress(), (UINT)vb->size, vb->vb_desc.vertex_stride };
    D3D12_INDEX_BUFFER_VIEW ibv = { ib->res->GetGPUVirtualAddress(), (UINT)ib->size,
                                    ib->ib_desc.format == ImPlatform_IndexFormat_UInt16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT };
    list->IASetVertexBuffers(0, 1, &vbv);
    list->IASetIndexBuffer(&ibv);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->DrawIndexedInstanced(desc->index_count, 1, 0, 0, 0);
    return true;
}

// Releases everything the resource layer still holds (after the GPU is idle, at shutdown).
static void ImPlatform_DX12_ReleaseAll()
{
    if (!g_P12.ready)
        return;
    for (ImP12_Release& r : g_P12.pending_releases)
        g_P12.releases.push_back(r);
    g_P12.pending_releases.clear();
    for (ImP12_Release& r : g_P12.releases)
        r.fence = 0;
    ImP12_ProcessReleases();
    for (auto& kv : g_P12.textures)
    {
        if (kv.second.res) kv.second.res->Release();
        if (kv.second.depth) kv.second.depth->Release();
    }
    g_P12.textures.clear();
    for (auto& p : g_P12.pages)
        p.res->Release();
    g_P12.pages.clear();
    for (auto& a : g_P12.allocators)
        if (a) a->Release();
    if (g_P12.list) g_P12.list->Release();
    if (g_P12.transient) g_P12.transient->Release();
    for (ImP12_Simple_Alloc* h : { &g_P12.cpu_srv, &g_P12.rtv, &g_P12.dsv })
        if (h->heap) h->heap->Release();
    if (g_P12.compute_rs) g_P12.compute_rs->Release();
    if (g_P12.graphics_rs) g_P12.graphics_rs->Release();
    g_P12 = ImP12_State();
}

#endif // IM_GFX_DIRECTX12
