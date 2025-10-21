// dear imgui: Graphics API Abstraction - DirectX 10 Backend
// This handles D3D10 device creation, rendering, and presentation

#include "ImPlatform_Internal.h"

#ifdef IM_GFX_DIRECTX10

#include "../imgui/backends/imgui_impl_dx10.h"

// Global state
static ImPlatform_GfxData_DX10 g_GfxData = { 0 };

// Helper functions
static void CreateRenderTarget()
{
    ID3D10Texture2D* pBackBuffer = NULL;
    g_GfxData.pSwapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), (void**)&pBackBuffer);
    if (pBackBuffer)
    {
        g_GfxData.pDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_GfxData.pRenderTargetView);
        pBackBuffer->Release();
    }
}

static void CleanupRenderTarget()
{
    if (g_GfxData.pRenderTargetView)
    {
        g_GfxData.pRenderTargetView->Release();
        g_GfxData.pRenderTargetView = NULL;
    }
}

// Internal API - Create DX10 device
bool ImPlatform_Gfx_CreateDevice_DX10(HWND hWnd, ImPlatform_GfxData_DX10* pData)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D10_CREATE_DEVICE_DEBUG;

    HRESULT res = D3D10CreateDeviceAndSwapChain(
        NULL,
        D3D10_DRIVER_TYPE_HARDWARE,
        NULL,
        createDeviceFlags,
        D3D10_SDK_VERSION,
        &sd,
        &pData->pSwapChain,
        &pData->pDevice);

    if (res != S_OK)
        return false;

    pData->bSwapChainOccluded = false;
    pData->uResizeWidth = 0;
    pData->uResizeHeight = 0;

    CreateRenderTarget();
    return true;
}

// Internal API - Cleanup DX10 device
void ImPlatform_Gfx_CleanupDevice_DX10(ImPlatform_GfxData_DX10* pData)
{
    CleanupRenderTarget();

    if (pData->pSwapChain)
    {
        pData->pSwapChain->Release();
        pData->pSwapChain = NULL;
    }
    if (pData->pDevice)
    {
        pData->pDevice->Release();
        pData->pDevice = NULL;
    }
}

// Internal API - Resize handling
void ImPlatform_Gfx_OnResize_DX10(ImPlatform_GfxData_DX10* pData, unsigned int uWidth, unsigned int uHeight)
{
    pData->uResizeWidth = uWidth;
    pData->uResizeHeight = uHeight;
}

// Internal API - Get DX10 gfx data
ImPlatform_GfxData_DX10* ImPlatform_Gfx_GetData_DX10(void)
{
    return &g_GfxData;
}

// ImPlatform API - InitGfxAPI
IMPLATFORM_API bool ImPlatform_InitGfxAPI(void)
{
#ifdef IM_PLATFORM_WIN32
    HWND hWnd = ImPlatform_App_GetHWND();
    if (!ImPlatform_Gfx_CreateDevice_DX10(hWnd, &g_GfxData))
    {
        ImPlatform_Gfx_CleanupDevice_DX10(&g_GfxData);
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
    if (!ImGui_ImplDX10_Init(g_GfxData.pDevice))
        return false;

    return true;
}

// ImPlatform API - GfxCheck
IMPLATFORM_API bool ImPlatform_GfxCheck(void)
{
    // Handle window being minimized or screen locked
    if (g_GfxData.bSwapChainOccluded && g_GfxData.pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
    {
        Sleep(10);
        return false;
    }
    g_GfxData.bSwapChainOccluded = false;

    // Handle window resize
    if (g_GfxData.uResizeWidth != 0 && g_GfxData.uResizeHeight != 0)
    {
        CleanupRenderTarget();
        g_GfxData.pSwapChain->ResizeBuffers(0, g_GfxData.uResizeWidth, g_GfxData.uResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
        g_GfxData.uResizeWidth = g_GfxData.uResizeHeight = 0;
        CreateRenderTarget();
    }

    return true;
}

// ImPlatform API - GfxAPINewFrame
IMPLATFORM_API void ImPlatform_GfxAPINewFrame(void)
{
    ImGui_ImplDX10_NewFrame();
}

// ImPlatform API - GfxAPIClear
IMPLATFORM_API bool ImPlatform_GfxAPIClear(ImVec4 const vClearColor)
{
    const float clear_color_with_alpha[4] = {
        vClearColor.x * vClearColor.w,
        vClearColor.y * vClearColor.w,
        vClearColor.z * vClearColor.w,
        vClearColor.w
    };

    g_GfxData.pDevice->OMSetRenderTargets(1, &g_GfxData.pRenderTargetView, NULL);
    g_GfxData.pDevice->ClearRenderTargetView(g_GfxData.pRenderTargetView, clear_color_with_alpha);
    return true;
}

// ImPlatform API - GfxAPIRender
IMPLATFORM_API bool ImPlatform_GfxAPIRender(ImVec4 const vClearColor)
{
    (void)vClearColor; // Not used for DX10
    ImGui_ImplDX10_RenderDrawData(ImGui::GetDrawData());
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
        ImGui::RenderPlatformWindowsDefault();
    }
}

// ImPlatform API - GfxAPISwapBuffer
IMPLATFORM_API bool ImPlatform_GfxAPISwapBuffer(void)
{
    HRESULT hr = g_GfxData.pSwapChain->Present(1, 0); // Present with vsync
    g_GfxData.bSwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    return true;
}

// ImPlatform API - ShutdownGfxAPI
IMPLATFORM_API void ImPlatform_ShutdownGfxAPI(void)
{
    // Nothing special needed for DX10
}

// ImPlatform API - ShutdownWindow
IMPLATFORM_API void ImPlatform_ShutdownWindow(void)
{
    ImGui_ImplDX10_Shutdown();
    ImPlatform_Gfx_CleanupDevice_DX10(&g_GfxData);
}

#endif // IM_GFX_DIRECTX10
