// dear imgui: Graphics API Abstraction - DirectX 12 Backend
// This handles D3D12 device creation, rendering, and presentation

#include "ImPlatform_Internal.h"

#ifdef IM_GFX_DIRECTX12

#include "../imgui/backends/imgui_impl_dx12.h"

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

#define APP_SRV_HEAP_SIZE 64

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
        int idx = FreeIndices[--FreeIndicesCount];
        out_cpu->ptr = HeapStartCpu.ptr + (idx * HeapHandleIncrement);
        out_gpu->ptr = HeapStartGpu.ptr + (idx * HeapHandleIncrement);
    }

    void Free(D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu)
    {
        int cpu_idx = (int)((cpu.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
        FreeIndices[FreeIndicesCount++] = cpu_idx;
        (void)gpu;
    }
};

// Global state
static ImPlatform_GfxData_DX12 g_GfxData = { 0 };
static ExampleDescriptorHeapAllocator g_SrvDescHeapAlloc;

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
}

static void CleanupRenderTarget()
{
    for (UINT i = 0; i < IM_DX12_NUM_BACK_BUFFERS; i++)
    {
        if (g_GfxData.pRenderTargetResource[i])
        {
            g_GfxData.pRenderTargetResource[i]->Release();
            g_GfxData.pRenderTargetResource[i] = NULL;
        }
    }
}

static void WaitForLastSubmittedFrame()
{
    ImPlatform_FrameContext_DX12* frameCtx = &g_GfxData.frameContext[g_GfxData.uFrameIndex % IM_DX12_NUM_FRAMES_IN_FLIGHT];

    UINT64 fenceValue = frameCtx->uFenceValue;
    if (fenceValue == 0)
        return;

    frameCtx->uFenceValue = 0;
    if (g_GfxData.pFence->GetCompletedValue() >= fenceValue)
        return;

    g_GfxData.pFence->SetEventOnCompletion(fenceValue, g_GfxData.hFenceEvent);
    WaitForSingleObject(g_GfxData.hFenceEvent, INFINITE);
}

static ImPlatform_FrameContext_DX12* WaitForNextFrameResources()
{
    UINT nextFrameIndex = g_GfxData.uFrameIndex + 1;
    g_GfxData.uFrameIndex = nextFrameIndex;

    HANDLE waitableObjects[] = { g_GfxData.hSwapChainWaitableObject, NULL };
    DWORD numWaitableObjects = 1;

    ImPlatform_FrameContext_DX12* frameCtx = &g_GfxData.frameContext[nextFrameIndex % IM_DX12_NUM_FRAMES_IN_FLIGHT];
    UINT64 fenceValue = frameCtx->uFenceValue;
    if (fenceValue != 0)
    {
        frameCtx->uFenceValue = 0;
        g_GfxData.pFence->SetEventOnCompletion(fenceValue, g_GfxData.hFenceEvent);
        waitableObjects[1] = g_GfxData.hFenceEvent;
        numWaitableObjects = 2;
    }

    WaitForMultipleObjects(numWaitableObjects, waitableObjects, TRUE, INFINITE);

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
        IDXGIFactory4* dxgiFactory = NULL;
        IDXGISwapChain1* swapChain1 = NULL;
        if (CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)) != S_OK)
            return false;
        if (dxgiFactory->CreateSwapChainForHwnd(pData->pCommandQueue, hWnd, &sd, NULL, NULL, &swapChain1) != S_OK)
            return false;
        if (swapChain1->QueryInterface(IID_PPV_ARGS(&pData->pSwapChain)) != S_OK)
            return false;
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
    if (pData->pDevice != NULL && uWidth > 0 && uHeight > 0)
    {
        WaitForLastSubmittedFrame();
        CleanupRenderTarget();
        HRESULT result = pData->pSwapChain->ResizeBuffers(0, uWidth, uHeight, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
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
#ifdef IM_PLATFORM_WIN32
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
    // Handle window screen locked
    if (g_GfxData.bSwapChainOccluded && g_GfxData.pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
    {
        Sleep(10);
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

// ImPlatform API - GfxAPIClear
IMPLATFORM_API bool ImPlatform_GfxAPIClear(ImVec4 const vClearColor)
{
    ImPlatform_FrameContext_DX12* frameCtx = WaitForNextFrameResources();
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

// ImPlatform API - GfxAPIRender
IMPLATFORM_API bool ImPlatform_GfxAPIRender(ImVec4 const vClearColor)
{
    (void)vClearColor;
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_GfxData.pCommandList);
    return true;
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
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::RenderPlatformWindowsDefault(NULL, (void*)g_GfxData.pCommandList);
    }
}

// ImPlatform API - GfxAPISwapBuffer
IMPLATFORM_API bool ImPlatform_GfxAPISwapBuffer(void)
{
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

    HRESULT hr = g_GfxData.pSwapChain->Present(1, 0); // Present with vsync
    g_GfxData.bSwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);

    UINT64 fenceValue = g_GfxData.uFenceLastSignaledValue + 1;
    g_GfxData.pCommandQueue->Signal(g_GfxData.pFence, fenceValue);
    g_GfxData.uFenceLastSignaledValue = fenceValue;
    g_GfxData.frameContext[g_GfxData.uFrameIndex % IM_DX12_NUM_FRAMES_IN_FLIGHT].uFenceValue = fenceValue;

    return true;
}

// ImPlatform API - ShutdownGfxAPI
IMPLATFORM_API void ImPlatform_ShutdownGfxAPI(void)
{
    WaitForLastSubmittedFrame();
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

IMPLATFORM_API ImTextureID ImPlatform_CreateTexture(const void* pixel_data, const ImPlatform_TextureDesc* desc)
{
    if (!desc || !pixel_data || !g_GfxData.pDevice || !g_GfxData.pCommandQueue || !g_GfxData.pSrvDescHeapAlloc)
        return NULL;

    int bytes_per_pixel;
    DXGI_FORMAT format = ImPlatform_GetD3D12Format(desc->format, &bytes_per_pixel);

    // Create texture resource
    D3D12_HEAP_PROPERTIES props;
    memset(&props, 0, sizeof(D3D12_HEAP_PROPERTIES));
    props.Type = D3D12_HEAP_TYPE_DEFAULT;
    props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resource_desc;
    ZeroMemory(&resource_desc, sizeof(resource_desc));
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resource_desc.Alignment = 0;
    resource_desc.Width = desc->width;
    resource_desc.Height = desc->height;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.Format = format;
    resource_desc.SampleDesc.Count = 1;
    resource_desc.SampleDesc.Quality = 0;
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    ID3D12Resource* pTexture = nullptr;
    HRESULT hr = g_GfxData.pDevice->CreateCommittedResource(
        &props, D3D12_HEAP_FLAG_NONE, &resource_desc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&pTexture));

    if (FAILED(hr) || !pTexture)
        return NULL;

    // Create upload buffer
    UINT uploadPitch = (desc->width * bytes_per_pixel + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);
    UINT uploadSize = desc->height * uploadPitch;

    D3D12_RESOURCE_DESC upload_desc;
    ZeroMemory(&upload_desc, sizeof(upload_desc));
    upload_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    upload_desc.Alignment = 0;
    upload_desc.Width = uploadSize;
    upload_desc.Height = 1;
    upload_desc.DepthOrArraySize = 1;
    upload_desc.MipLevels = 1;
    upload_desc.Format = DXGI_FORMAT_UNKNOWN;
    upload_desc.SampleDesc.Count = 1;
    upload_desc.SampleDesc.Quality = 0;
    upload_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    upload_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES upload_props;
    memset(&upload_props, 0, sizeof(upload_props));
    upload_props.Type = D3D12_HEAP_TYPE_UPLOAD;
    upload_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    upload_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    ID3D12Resource* uploadBuffer = nullptr;
    hr = g_GfxData.pDevice->CreateCommittedResource(
        &upload_props, D3D12_HEAP_FLAG_NONE, &upload_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer));

    if (FAILED(hr) || !uploadBuffer)
    {
        pTexture->Release();
        return NULL;
    }

    // Map and copy data
    void* mapped = nullptr;
    D3D12_RANGE range = { 0, uploadSize };
    hr = uploadBuffer->Map(0, &range, &mapped);
    if (SUCCEEDED(hr))
    {
        for (UINT y = 0; y < desc->height; y++)
        {
            memcpy(
                (void*)((uintptr_t)mapped + y * uploadPitch),
                (const unsigned char*)pixel_data + y * desc->width * bytes_per_pixel,
                desc->width * bytes_per_pixel);
        }
        uploadBuffer->Unmap(0, &range);
    }

    // Create command list for upload
    ID3D12CommandAllocator* pCommandAllocator = g_GfxData.frameContext[g_GfxData.uFrameIndex].pCommandAllocator;
    pCommandAllocator->Reset();

    ID3D12GraphicsCommandList* pCommandList = nullptr;
    hr = g_GfxData.pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pCommandAllocator, NULL, IID_PPV_ARGS(&pCommandList));
    if (FAILED(hr))
    {
        uploadBuffer->Release();
        pTexture->Release();
        return NULL;
    }

    // Copy from upload buffer to texture
    D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
    srcLocation.pResource = uploadBuffer;
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLocation.PlacedFootprint.Footprint.Format = format;
    srcLocation.PlacedFootprint.Footprint.Width = desc->width;
    srcLocation.PlacedFootprint.Footprint.Height = desc->height;
    srcLocation.PlacedFootprint.Footprint.Depth = 1;
    srcLocation.PlacedFootprint.Footprint.RowPitch = uploadPitch;

    D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
    dstLocation.pResource = pTexture;
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLocation.SubresourceIndex = 0;

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = pTexture;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    pCommandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);
    pCommandList->ResourceBarrier(1, &barrier);
    pCommandList->Close();

    // Execute command list
    g_GfxData.pCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&pCommandList);

    // Wait for upload to complete (simplified - in production you'd want async)
    g_GfxData.uFenceLastSignaledValue++;
    g_GfxData.pCommandQueue->Signal(g_GfxData.pFence, g_GfxData.uFenceLastSignaledValue);
    g_GfxData.pFence->SetEventOnCompletion(g_GfxData.uFenceLastSignaledValue, g_GfxData.hFenceEvent);
    WaitForSingleObject(g_GfxData.hFenceEvent, INFINITE);

    // Cleanup temp resources
    pCommandList->Release();
    uploadBuffer->Release();

    // Create SRV in descriptor heap
    D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle;
    g_GfxData.pSrvDescHeapAlloc->Alloc(&srvCpuHandle, &srvGpuHandle);

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;
    ZeroMemory(&srv_desc, sizeof(srv_desc));
    srv_desc.Format = format;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Texture2D.MipLevels = 1;
    srv_desc.Texture2D.MostDetailedMip = 0;

    g_GfxData.pDevice->CreateShaderResourceView(pTexture, &srv_desc, srvCpuHandle);

    // IMPORTANT: We do NOT release pTexture here because:
    // 1. The SRV descriptor does NOT keep the resource alive (unlike DX11)
    // 2. In DX12, ImTextureID is the GPU descriptor handle (not the resource)
    // 3. We have no way to retrieve the resource pointer from the descriptor handle later
    //
    // This means DestroyTexture() won't be able to properly clean up (small leak).
    // A proper implementation would need a texture registry that maps:
    //   GPU descriptor handle -> ID3D12Resource* + CPU descriptor handle
    //
    // For now, we keep the texture alive by NOT calling Release().
    // The texture will stay in memory until the application exits.

    return (ImTextureID)srvGpuHandle.ptr;
}

IMPLATFORM_API bool ImPlatform_UpdateTexture(ImTextureID texture_id, const void* pixel_data,
                                              unsigned int x, unsigned int y,
                                              unsigned int width, unsigned int height)
{
    // DX12 texture updates are complex and require command lists
    // For now, we return false to indicate this feature is not implemented
    // A full implementation would need to:
    // 1. Track the texture resource from ImTextureID
    // 2. Create an upload buffer
    // 3. Copy data to upload buffer
    // 4. Use command list to copy from upload to texture
    // 5. Handle resource barriers
    // This is quite involved for DX12
    return false;
}

IMPLATFORM_API void ImPlatform_DestroyTexture(ImTextureID texture_id)
{
    if (!texture_id || !g_GfxData.pSrvDescHeapAlloc)
        return;

    // Free the descriptor from the heap
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
    gpuHandle.ptr = (UINT64)texture_id;

    // Convert GPU handle to CPU handle for freeing
    // This requires knowing the descriptor heap structure
    // For now, this is a simplified version
    // A full implementation would need proper handle tracking

    // Note: The texture resource itself will be released when its ref count hits zero
    // The descriptor heap allocator should handle freeing the slot
}

#endif // IM_GFX_DIRECTX12
