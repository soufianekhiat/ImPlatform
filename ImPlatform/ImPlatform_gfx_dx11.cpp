// dear imgui: Graphics API Abstraction - DirectX 11 Backend
// This handles D3D11 device creation, rendering, and presentation

#include "ImPlatform_Internal.h"

#ifdef IM_GFX_DIRECTX11

#include "../imgui/backends/imgui_impl_dx11.h"
#include <d3d11.h>

// Global state
static ImPlatform_GfxData_DX11 g_GfxData = { 0 };

// Helper functions
static void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = NULL;
    g_GfxData.pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
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

// Internal API - Create DX11 device
bool ImPlatform_Gfx_CreateDevice_DX11(void* hWnd, ImPlatform_GfxData_DX11* pData)
{
    HWND hwnd = (HWND)hWnd;

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
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };

    ID3D11Device* pDevice = NULL;
    ID3D11DeviceContext* pDeviceContext = NULL;
    IDXGISwapChain* pSwapChain = NULL;

    HRESULT res = D3D11CreateDeviceAndSwapChain(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        createDeviceFlags,
        featureLevelArray,
        2,
        D3D11_SDK_VERSION,
        &sd,
        &pSwapChain,
        &pDevice,
        &featureLevel,
        &pDeviceContext);

    // Try WARP software driver if hardware is not available
    if (res == DXGI_ERROR_UNSUPPORTED)
    {
        res = D3D11CreateDeviceAndSwapChain(
            NULL,
            D3D_DRIVER_TYPE_WARP,
            NULL,
            createDeviceFlags,
            featureLevelArray,
            2,
            D3D11_SDK_VERSION,
            &sd,
            &pSwapChain,
            &pDevice,
            &featureLevel,
            &pDeviceContext);
    }

    if (res != S_OK)
        return false;

    pData->pDevice = pDevice;
    pData->pDeviceContext = pDeviceContext;
    pData->pSwapChain = pSwapChain;
    pData->bSwapChainOccluded = false;
    pData->uResizeWidth = 0;
    pData->uResizeHeight = 0;

    CreateRenderTarget();
    return true;
}

// Internal API - Cleanup DX11 device
void ImPlatform_Gfx_CleanupDevice_DX11(ImPlatform_GfxData_DX11* pData)
{
    CleanupRenderTarget();

    if (pData->pSwapChain)
    {
        pData->pSwapChain->Release();
        pData->pSwapChain = NULL;
    }
    if (pData->pDeviceContext)
    {
        pData->pDeviceContext->Release();
        pData->pDeviceContext = NULL;
    }
    if (pData->pDevice)
    {
        pData->pDevice->Release();
        pData->pDevice = NULL;
    }
}

// Internal API - Resize handling
void ImPlatform_Gfx_OnResize_DX11(ImPlatform_GfxData_DX11* pData, unsigned int uWidth, unsigned int uHeight)
{
    pData->uResizeWidth = uWidth;
    pData->uResizeHeight = uHeight;
}

// Internal API - Get DX11 gfx data
ImPlatform_GfxData_DX11* ImPlatform_Gfx_GetData_DX11(void)
{
    return &g_GfxData;
}

// ImPlatform API - InitGfxAPI
IMPLATFORM_API bool ImPlatform_InitGfxAPI(void)
{
#ifdef IM_PLATFORM_WIN32
    HWND hWnd = ImPlatform_App_GetHWND();
    if (!ImPlatform_Gfx_CreateDevice_DX11(hWnd, &g_GfxData))
    {
        ImPlatform_Gfx_CleanupDevice_DX11(&g_GfxData);
        return false;
    }
    return true;
#else
    return false;
#endif
}

// ImPlatform API - InitPlatform (gfx-specific part)
// This is called after InitPlatform in app backend
// We need to declare it here but the app backend calls ImGui_ImplWin32_Init first
// Then we need to init our graphics backend
bool ImPlatform_InitGfx_Internal_DX11(void)
{
    if (!ImGui_ImplDX11_Init(g_GfxData.pDevice, g_GfxData.pDeviceContext))
        return false;

    return true;
}

// ImPlatform API - InitGfx
IMPLATFORM_API bool ImPlatform_InitGfx(void)
{
    return ImPlatform_InitGfx_Internal_DX11();
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
    ImGui_ImplDX11_NewFrame();
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

    g_GfxData.pDeviceContext->OMSetRenderTargets(1, &g_GfxData.pRenderTargetView, NULL);
    g_GfxData.pDeviceContext->ClearRenderTargetView(g_GfxData.pRenderTargetView, clear_color_with_alpha);
    return true;
}

// ImPlatform API - GfxAPIRender
IMPLATFORM_API bool ImPlatform_GfxAPIRender(ImVec4 const vClearColor)
{
    (void)vClearColor; // Not used for DX11
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
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
    // Nothing special needed for DX11
}

// ImPlatform API - ShutdownWindow (gfx-specific part)
IMPLATFORM_API void ImPlatform_ShutdownWindow(void)
{
    ImGui_ImplDX11_Shutdown();
    ImPlatform_Gfx_CleanupDevice_DX11(&g_GfxData);
}

// ============================================================================
// Texture Creation API - DirectX 11 Implementation
// ============================================================================

// Helper to get D3D11 format from ImPlatform format
static DXGI_FORMAT ImPlatform_GetD3D11Format(ImPlatform_PixelFormat format, int* out_bytes_per_pixel)
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
        // D3D11 doesn't have RGB8, use RGBA8 instead
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
    if (!desc || !pixel_data || !g_GfxData.pDevice)
        return NULL;

    int bytes_per_pixel;
    DXGI_FORMAT format = ImPlatform_GetD3D11Format(desc->format, &bytes_per_pixel);

    // Create texture
    D3D11_TEXTURE2D_DESC tex_desc;
    ZeroMemory(&tex_desc, sizeof(tex_desc));
    tex_desc.Width = desc->width;
    tex_desc.Height = desc->height;
    tex_desc.MipLevels = 1;
    tex_desc.ArraySize = 1;
    tex_desc.Format = format;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.Usage = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    tex_desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = pixel_data;
    subResource.SysMemPitch = desc->width * bytes_per_pixel;
    subResource.SysMemSlicePitch = 0;

    ID3D11Texture2D* pTexture = NULL;
    HRESULT hr = g_GfxData.pDevice->CreateTexture2D(&tex_desc, &subResource, &pTexture);
    if (FAILED(hr) || !pTexture)
        return NULL;

    // Create shader resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
    ZeroMemory(&srv_desc, sizeof(srv_desc));
    srv_desc.Format = format;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = tex_desc.MipLevels;
    srv_desc.Texture2D.MostDetailedMip = 0;

    ID3D11ShaderResourceView* pSRV = NULL;
    hr = g_GfxData.pDevice->CreateShaderResourceView(pTexture, &srv_desc, &pSRV);
    pTexture->Release();

    if (FAILED(hr) || !pSRV)
        return NULL;

    // Note: Filtering and wrapping are set via samplers in D3D11, not per-texture
    // The renderer backend manages sampler states

    return (ImTextureID)pSRV;
}

IMPLATFORM_API bool ImPlatform_UpdateTexture(ImTextureID texture_id, const void* pixel_data,
                                              unsigned int x, unsigned int y,
                                              unsigned int width, unsigned int height)
{
    if (!texture_id || !pixel_data || !g_GfxData.pDeviceContext)
        return false;

    ID3D11ShaderResourceView* pSRV = (ID3D11ShaderResourceView*)texture_id;

    // Get the underlying texture
    ID3D11Resource* pResource = NULL;
    pSRV->GetResource(&pResource);
    if (!pResource)
        return false;

    ID3D11Texture2D* pTexture = NULL;
    HRESULT hr = pResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&pTexture);
    pResource->Release();

    if (FAILED(hr) || !pTexture)
        return false;

    // Get texture description to determine format
    D3D11_TEXTURE2D_DESC desc;
    pTexture->GetDesc(&desc);

    // Calculate bytes per pixel from format
    int bytes_per_pixel = 4; // Default to RGBA8
    switch (desc.Format)
    {
    case DXGI_FORMAT_R8_UNORM: bytes_per_pixel = 1; break;
    case DXGI_FORMAT_R8G8_UNORM: bytes_per_pixel = 2; break;
    case DXGI_FORMAT_R8G8B8A8_UNORM: bytes_per_pixel = 4; break;
    case DXGI_FORMAT_R16_UNORM: bytes_per_pixel = 2; break;
    case DXGI_FORMAT_R16G16_UNORM: bytes_per_pixel = 4; break;
    case DXGI_FORMAT_R16G16B16A16_UNORM: bytes_per_pixel = 8; break;
    case DXGI_FORMAT_R32_FLOAT: bytes_per_pixel = 4; break;
    case DXGI_FORMAT_R32G32_FLOAT: bytes_per_pixel = 8; break;
    case DXGI_FORMAT_R32G32B32A32_FLOAT: bytes_per_pixel = 16; break;
    }

    // Update texture sub-region
    D3D11_BOX box;
    box.left = x;
    box.right = x + width;
    box.top = y;
    box.bottom = y + height;
    box.front = 0;
    box.back = 1;

    g_GfxData.pDeviceContext->UpdateSubresource(pTexture, 0, &box, pixel_data, width * bytes_per_pixel, 0);

    pTexture->Release();
    return true;
}

IMPLATFORM_API void ImPlatform_DestroyTexture(ImTextureID texture_id)
{
    if (!texture_id)
        return;

    ID3D11ShaderResourceView* pSRV = (ID3D11ShaderResourceView*)texture_id;
    pSRV->Release();
}

#endif // IM_GFX_DIRECTX11
