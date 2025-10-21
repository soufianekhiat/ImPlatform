// dear imgui: Graphics API Abstraction - DirectX 9 Backend
// This handles D3D9 device creation, rendering, and presentation

#include "ImPlatform_Internal.h"

#ifdef IM_GFX_DIRECTX9

#include "../imgui/backends/imgui_impl_dx9.h"

// Link with d3d9.lib
#pragma comment(lib, "d3d9")

// Global state
static ImPlatform_GfxData_DX9 g_GfxData = { 0 };

// Helper functions
static void CreateRenderTarget()
{
    // DX9 doesn't need explicit render target view creation
    // The backbuffer is accessed through GetBackBuffer()
}

static void CleanupRenderTarget()
{
    // Nothing to clean up for DX9
}

// Internal API - Create D3D9 device
bool ImPlatform_Gfx_CreateDevice_DX9(HWND hWnd, ImPlatform_GfxData_DX9* pData)
{
    if ((pData->pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL)
        return false;

    // Create the D3DDevice
    ZeroMemory(&pData->d3dpp, sizeof(pData->d3dpp));
    pData->d3dpp.Windowed = TRUE;
    pData->d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pData->d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    pData->d3dpp.EnableAutoDepthStencil = TRUE;
    pData->d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    pData->d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE; // Vsync

    if (pData->pD3D->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        hWnd,
        D3DCREATE_HARDWARE_VERTEXPROCESSING,
        &pData->d3dpp,
        &pData->pDevice) < 0)
    {
        return false;
    }

    pData->bDeviceLost = false;
    pData->uResizeWidth = 0;
    pData->uResizeHeight = 0;

    return true;
}

// Internal API - Cleanup D3D9 device
void ImPlatform_Gfx_CleanupDevice_DX9(ImPlatform_GfxData_DX9* pData)
{
    if (pData->pDevice)
    {
        pData->pDevice->Release();
        pData->pDevice = NULL;
    }
    if (pData->pD3D)
    {
        pData->pD3D->Release();
        pData->pD3D = NULL;
    }
}

// Internal API - Reset D3D9 device
void ImPlatform_Gfx_ResetDevice_DX9(ImPlatform_GfxData_DX9* pData)
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = pData->pDevice->Reset(&pData->d3dpp);
    if (hr == D3DERR_INVALIDCALL)
    {
        // Failed to reset
        return;
    }
    ImGui_ImplDX9_CreateDeviceObjects();
}

// Internal API - Handle resize
void ImPlatform_Gfx_OnResize_DX9(ImPlatform_GfxData_DX9* pData, unsigned int uWidth, unsigned int uHeight)
{
    pData->uResizeWidth = uWidth;
    pData->uResizeHeight = uHeight;
}

// Internal API - Get DX9 gfx data
ImPlatform_GfxData_DX9* ImPlatform_Gfx_GetData_DX9(void)
{
    return &g_GfxData;
}

// ImPlatform API - InitGfxAPI
IMPLATFORM_API bool ImPlatform_InitGfxAPI(void)
{
#ifdef IM_PLATFORM_WIN32
    HWND hWnd = ImPlatform_App_GetHWND();
    if (!ImPlatform_Gfx_CreateDevice_DX9(hWnd, &g_GfxData))
    {
        ImPlatform_Gfx_CleanupDevice_DX9(&g_GfxData);
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
    if (!ImGui_ImplDX9_Init(g_GfxData.pDevice))
        return false;

    return true;
}

// ImPlatform API - GfxCheck
IMPLATFORM_API bool ImPlatform_GfxCheck(void)
{
    // Handle lost D3D9 device
    if (g_GfxData.bDeviceLost)
    {
        HRESULT hr = g_GfxData.pDevice->TestCooperativeLevel();
        if (hr == D3DERR_DEVICELOST)
        {
            ::Sleep(10);
            return false; // Skip this frame
        }
        if (hr == D3DERR_DEVICENOTRESET)
        {
            ImPlatform_Gfx_ResetDevice_DX9(&g_GfxData);
        }
        g_GfxData.bDeviceLost = false;
    }

    // Handle window resize
    if (g_GfxData.uResizeWidth != 0 && g_GfxData.uResizeHeight != 0)
    {
        g_GfxData.d3dpp.BackBufferWidth = g_GfxData.uResizeWidth;
        g_GfxData.d3dpp.BackBufferHeight = g_GfxData.uResizeHeight;
        g_GfxData.uResizeWidth = g_GfxData.uResizeHeight = 0;
        ImPlatform_Gfx_ResetDevice_DX9(&g_GfxData);
    }

    return true;
}

// ImPlatform API - GfxAPINewFrame
IMPLATFORM_API void ImPlatform_GfxAPINewFrame(void)
{
    ImGui_ImplDX9_NewFrame();
}

// ImPlatform API - GfxAPIClear
IMPLATFORM_API bool ImPlatform_GfxAPIClear(ImVec4 const vClearColor)
{
    // DX9 uses EndFrame before rendering, so this is called before BeginScene
    ImGui::EndFrame();

    g_GfxData.pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_GfxData.pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    g_GfxData.pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

    D3DCOLOR clear_col_dx = D3DCOLOR_RGBA(
        (int)(vClearColor.x * vClearColor.w * 255.0f),
        (int)(vClearColor.y * vClearColor.w * 255.0f),
        (int)(vClearColor.z * vClearColor.w * 255.0f),
        (int)(vClearColor.w * 255.0f));

    g_GfxData.pDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);

    if (g_GfxData.pDevice->BeginScene() >= 0)
    {
        ImGui::Render();
        return true;
    }

    return false;
}

// ImPlatform API - GfxAPIRender
IMPLATFORM_API bool ImPlatform_GfxAPIRender(ImVec4 const vClearColor)
{
    (void)vClearColor;

    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    g_GfxData.pDevice->EndScene();

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
    HRESULT result = g_GfxData.pDevice->Present(NULL, NULL, NULL, NULL);
    if (result == D3DERR_DEVICELOST)
    {
        g_GfxData.bDeviceLost = true;
    }

    return true;
}

// ImPlatform API - ShutdownGfxAPI
IMPLATFORM_API void ImPlatform_ShutdownGfxAPI(void)
{
    // Nothing special needed
}

// ImPlatform API - ShutdownWindow
IMPLATFORM_API void ImPlatform_ShutdownWindow(void)
{
    ImGui_ImplDX9_Shutdown();
    ImPlatform_Gfx_CleanupDevice_DX9(&g_GfxData);
}

#endif // IM_GFX_DIRECTX9
