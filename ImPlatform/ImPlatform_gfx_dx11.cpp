// dear imgui: Graphics API Abstraction - DirectX 11 Backend
// This handles D3D11 device creation, rendering, and presentation

#include "ImPlatform_Internal.h"

#ifdef IM_GFX_DIRECTX11

#include "../imgui/backends/imgui_impl_dx11.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// Global state
static ImPlatform_GfxData_DX11 g_GfxData = { 0 };
unsigned int g_ImPlatform_BackbufferW = 0;
unsigned int g_ImPlatform_BackbufferH = 0;

// Render texture tracking (SRV ↔ RTV pairs)
struct ImPlatform_RTTracking_DX11 {
    ID3D11ShaderResourceView*  pSRV;
    ID3D11RenderTargetView*    pRTV;
    unsigned int               width, height;
    ImPlatform_RTTracking_DX11* next;
};
static ImPlatform_RTTracking_DX11* g_RTTrackingHead = NULL;

// Saved render target for Begin/EndRenderToTexture
static ID3D11RenderTargetView* g_SavedRTV = NULL;
static D3D11_VIEWPORT          g_SavedViewport = {};
static D3D11_RECT              g_SavedScissor = {};
static UINT                    g_SavedScissorCount = 0;

// Sampler table: [filter][wrap] — filter: 0=Nearest,1=Linear  wrap: 0=Clamp,1=Repeat,2=Mirror
static ID3D11SamplerState* g_Samplers[2][3]  = {};
static ID3D11SamplerState* g_SamplerStack[8] = {};
static int                 g_SamplerDepth    = 0;

// Helper functions
static void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = NULL;
    g_GfxData.pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    if (pBackBuffer)
    {
        g_GfxData.pDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_GfxData.pRenderTargetView);

        // Update stored backbuffer size
        D3D11_TEXTURE2D_DESC bbDesc;
        pBackBuffer->GetDesc(&bbDesc);
        g_ImPlatform_BackbufferW = bbDesc.Width;
        g_ImPlatform_BackbufferH = bbDesc.Height;

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
#ifdef _WIN32
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

    // Pre-create all filter×wrap sampler combinations
    static const D3D11_FILTER      kFilters[2] = { D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_FILTER_MIN_MAG_MIP_LINEAR };
    static const D3D11_TEXTURE_ADDRESS_MODE kAddr[3] = { D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_MIRROR };
    for (int f = 0; f < 2; ++f)
    for (int w = 0; w < 3; ++w)
    {
        D3D11_SAMPLER_DESC sd = {};
        sd.Filter         = kFilters[f];
        sd.AddressU       = kAddr[w];
        sd.AddressV       = kAddr[w];
        sd.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sd.MaxLOD         = D3D11_FLOAT32_MAX;
        g_GfxData.pDevice->CreateSamplerState(&sd, &g_Samplers[f][w]);
    }

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
    for (int f = 0; f < 2; ++f)
        for (int w = 0; w < 3; ++w)
            if (g_Samplers[f][w]) { g_Samplers[f][w]->Release(); g_Samplers[f][w] = nullptr; }
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

// ------------------- 3D Texture (D3D11 native) -------------------
IMPLATFORM_API ImPlatform_TextureDesc3D ImPlatform_TextureDesc3D_Default(
    unsigned int width, unsigned int height, unsigned int depth)
{
    ImPlatform_TextureDesc3D d;
    d.width = width; d.height = height; d.depth = depth;
    d.format = ImPlatform_PixelFormat_RGBA8;
    d.min_filter = ImPlatform_TextureFilter_Linear;
    d.mag_filter = ImPlatform_TextureFilter_Linear;
    d.wrap_u = ImPlatform_TextureWrap_Clamp;
    d.wrap_v = ImPlatform_TextureWrap_Clamp;
    d.wrap_w = ImPlatform_TextureWrap_Clamp;
    return d;
}

IMPLATFORM_API bool ImPlatform_SupportsTexture3D(void) { return true; }

IMPLATFORM_API ImTextureID ImPlatform_CreateTexture3D(const void* voxel_data,
                                                      const ImPlatform_TextureDesc3D* desc)
{
    if (!desc || !voxel_data || !g_GfxData.pDevice) return NULL;
    int bytes_per_pixel = 0;
    DXGI_FORMAT format = ImPlatform_GetD3D11Format(desc->format, &bytes_per_pixel);

    D3D11_TEXTURE3D_DESC td;
    ZeroMemory(&td, sizeof(td));
    td.Width = desc->width;
    td.Height = desc->height;
    td.Depth = desc->depth;
    td.MipLevels = 1;
    td.Format = format;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA sub;
    sub.pSysMem = voxel_data;
    sub.SysMemPitch = desc->width * bytes_per_pixel;
    sub.SysMemSlicePitch = desc->width * desc->height * bytes_per_pixel;

    ID3D11Texture3D* pTex = NULL;
    HRESULT hr = g_GfxData.pDevice->CreateTexture3D(&td, &sub, &pTex);
    if (FAILED(hr) || !pTex) return NULL;

    D3D11_SHADER_RESOURCE_VIEW_DESC sv;
    ZeroMemory(&sv, sizeof(sv));
    sv.Format = format;
    sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
    sv.Texture3D.MipLevels = 1;
    sv.Texture3D.MostDetailedMip = 0;

    ID3D11ShaderResourceView* pSRV = NULL;
    hr = g_GfxData.pDevice->CreateShaderResourceView(pTex, &sv, &pSRV);
    pTex->Release();
    if (FAILED(hr) || !pSRV) return NULL;
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

IMPLATFORM_API ImTextureID ImPlatform_CreateRenderTexture(const ImPlatform_TextureDesc* desc)
{
    if (!desc || !g_GfxData.pDevice)
        return NULL;

    int bytes_per_pixel;
    DXGI_FORMAT format = ImPlatform_GetD3D11Format(desc->format, &bytes_per_pixel);

    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.Width = desc->width;
    tex_desc.Height = desc->height;
    tex_desc.MipLevels = 1;
    tex_desc.ArraySize = 1;
    tex_desc.Format = format;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.Usage = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

    ID3D11Texture2D* pTexture = NULL;
    HRESULT hr = g_GfxData.pDevice->CreateTexture2D(&tex_desc, NULL, &pTexture);
    if (FAILED(hr) || !pTexture)
        return NULL;

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format = format;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;

    ID3D11ShaderResourceView* pSRV = NULL;
    hr = g_GfxData.pDevice->CreateShaderResourceView(pTexture, &srv_desc, &pSRV);
    if (FAILED(hr) || !pSRV) { pTexture->Release(); return NULL; }

    D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
    rtv_desc.Format = format;
    rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    ID3D11RenderTargetView* pRTV = NULL;
    hr = g_GfxData.pDevice->CreateRenderTargetView(pTexture, &rtv_desc, &pRTV);
    pTexture->Release();
    if (FAILED(hr) || !pRTV) { pSRV->Release(); return NULL; }

    // Track the pair
    ImPlatform_RTTracking_DX11* entry = new ImPlatform_RTTracking_DX11();
    entry->pSRV = pSRV;
    entry->pRTV = pRTV;
    entry->width = desc->width;
    entry->height = desc->height;
    entry->next = g_RTTrackingHead;
    g_RTTrackingHead = entry;

    return (ImTextureID)pSRV;
}

IMPLATFORM_API bool ImPlatform_BeginRenderToTexture(ImTextureID texture)
{
    if (!texture || !g_GfxData.pDeviceContext)
        return false;

    ID3D11ShaderResourceView* pSRV = (ID3D11ShaderResourceView*)texture;

    // Find tracked RTV
    ImPlatform_RTTracking_DX11* entry = g_RTTrackingHead;
    while (entry) { if (entry->pSRV == pSRV) break; entry = entry->next; }
    if (!entry) return false;

    // Save current render target, viewport, and scissor
    UINT numViewports = 1;
    g_GfxData.pDeviceContext->OMGetRenderTargets(1, &g_SavedRTV, NULL);
    g_GfxData.pDeviceContext->RSGetViewports(&numViewports, &g_SavedViewport);
    g_SavedScissorCount = 1;
    g_GfxData.pDeviceContext->RSGetScissorRects(&g_SavedScissorCount, &g_SavedScissor);

    // Unbind the SRV from pixel shader so it can be used as RT (D3D11 validation)
    ID3D11ShaderResourceView* nullSRV = NULL;
    g_GfxData.pDeviceContext->PSSetShaderResources(0, 1, &nullSRV);

    // Set new render target
    g_GfxData.pDeviceContext->OMSetRenderTargets(1, &entry->pRTV, NULL);

    // Set viewport and scissor to match texture size
    D3D11_VIEWPORT vp = {};
    vp.Width = (float)entry->width;
    vp.Height = (float)entry->height;
    vp.MaxDepth = 1.0f;
    g_GfxData.pDeviceContext->RSSetViewports(1, &vp);

    D3D11_RECT scissor = { 0, 0, (LONG)entry->width, (LONG)entry->height };
    g_GfxData.pDeviceContext->RSSetScissorRects(1, &scissor);

    // Clear the render texture
    float clearColor[4] = { 0, 0, 0, 0 };
    g_GfxData.pDeviceContext->ClearRenderTargetView(entry->pRTV, clearColor);

    return true;
}

IMPLATFORM_API void ImPlatform_EndRenderToTexture(void)
{
    if (!g_GfxData.pDeviceContext) return;

    // Restore saved render target and viewport
    if (g_SavedRTV)
    {
        g_GfxData.pDeviceContext->OMSetRenderTargets(1, &g_SavedRTV, NULL);
        g_GfxData.pDeviceContext->RSSetViewports(1, &g_SavedViewport);
        if (g_SavedScissorCount > 0)
            g_GfxData.pDeviceContext->RSSetScissorRects(1, &g_SavedScissor);
        g_SavedRTV->Release();
        g_SavedRTV = NULL;
    }
}

IMPLATFORM_API bool ImPlatform_CopyBackbuffer(ImTextureID dst)
{
    if (!dst || !g_GfxData.pSwapChain || !g_GfxData.pDeviceContext)
        return false;

    ID3D11Texture2D* pBackBuffer = NULL;
    HRESULT hr = g_GfxData.pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    if (FAILED(hr) || !pBackBuffer)
        return false;

    ID3D11ShaderResourceView* pDstSRV = (ID3D11ShaderResourceView*)dst;
    ID3D11Resource* pDstRes = NULL;
    pDstSRV->GetResource(&pDstRes);

    if (!pDstRes)
    {
        pBackBuffer->Release();
        return false;
    }

    g_GfxData.pDeviceContext->CopyResource(pDstRes, pBackBuffer);

    pDstRes->Release();
    pBackBuffer->Release();
    return true;
}

IMPLATFORM_API void ImPlatform_GetBackbufferSize(unsigned int* width, unsigned int* height)
{
    if (width)  *width  = g_ImPlatform_BackbufferW;
    if (height) *height = g_ImPlatform_BackbufferH;
}

IMPLATFORM_API bool ImPlatform_CopyTexture(ImTextureID dst, ImTextureID src)
{
    if (!dst || !src || !g_GfxData.pDeviceContext)
        return false;

    ID3D11ShaderResourceView* pDstSRV = (ID3D11ShaderResourceView*)dst;
    ID3D11ShaderResourceView* pSrcSRV = (ID3D11ShaderResourceView*)src;

    ID3D11Resource* pDstRes = NULL;
    ID3D11Resource* pSrcRes = NULL;
    pDstSRV->GetResource(&pDstRes);
    pSrcSRV->GetResource(&pSrcRes);

    if (!pDstRes || !pSrcRes)
    {
        if (pDstRes) pDstRes->Release();
        if (pSrcRes) pSrcRes->Release();
        return false;
    }

    g_GfxData.pDeviceContext->CopyResource(pDstRes, pSrcRes);

    pDstRes->Release();
    pSrcRes->Release();
    return true;
}

IMPLATFORM_API void ImPlatform_DestroyTexture(ImTextureID texture_id)
{
    if (!texture_id)
        return;

    ID3D11ShaderResourceView* pSRV = (ID3D11ShaderResourceView*)texture_id;
    pSRV->Release();
}

IMPLATFORM_API void ImPlatform_PushSampler(ImPlatform_TextureFilter filter, ImPlatform_TextureWrap wrap)
{
    int f = (filter == ImPlatform_TextureFilter_Linear) ? 1 : 0;
    int w = (int)wrap;
    if (w < 0 || w > 2) w = 0;
    ID3D11SamplerState* s = g_Samplers[f][w];
    if (!s) return;

    ImGui::GetWindowDrawList()->AddCallback(
        [](const ImDrawList*, const ImDrawCmd* cmd) {
            auto* rs = static_cast<ImGui_ImplDX11_RenderState*>(
                ImGui::GetPlatformIO().Renderer_RenderState);
            if (!rs) return;
            ID3D11SamplerState* newS = static_cast<ID3D11SamplerState*>(cmd->UserCallbackData);
            // Save current sampler onto the stack
            ID3D11SamplerState* old = nullptr;
            rs->DeviceContext->PSGetSamplers(0, 1, &old); // adds a ref
            if (g_SamplerDepth < 8) g_SamplerStack[g_SamplerDepth++] = old;
            else if (old) old->Release();
            rs->DeviceContext->PSSetSamplers(0, 1, &newS);
        }, static_cast<void*>(s));
}

IMPLATFORM_API void ImPlatform_PopSampler(void)
{
    ImGui::GetWindowDrawList()->AddCallback(
        [](const ImDrawList*, const ImDrawCmd*) {
            auto* rs = static_cast<ImGui_ImplDX11_RenderState*>(
                ImGui::GetPlatformIO().Renderer_RenderState);
            if (!rs || g_SamplerDepth <= 0) return;
            ID3D11SamplerState* saved = g_SamplerStack[--g_SamplerDepth];
            rs->DeviceContext->PSSetSamplers(0, 1, &saved);
            if (saved) saved->Release(); // balance PSGetSamplers ref
        }, nullptr);
}

// ============================================================================
// Custom Vertex/Index Buffer Management API - DirectX 11
// ============================================================================

// Internal buffer structure for DX11
struct ImPlatform_BufferData_DX11
{
    ID3D11Buffer* pVertexBuffer;
    ID3D11Buffer* pIndexBuffer;
    ID3D11InputLayout* pInputLayout;
    ImPlatform_VertexBufferDesc vb_desc;
    ImPlatform_IndexBufferDesc ib_desc;
    ImPlatform_VertexAttribute* attributes;
};

// Helper to convert ImPlatform usage to D3D11 usage
static D3D11_USAGE ImPlatform_GetD3D11Usage(ImPlatform_BufferUsage usage)
{
    switch (usage)
    {
    case ImPlatform_BufferUsage_Static:  return D3D11_USAGE_DEFAULT;
    case ImPlatform_BufferUsage_Dynamic: return D3D11_USAGE_DYNAMIC;
    case ImPlatform_BufferUsage_Stream:  return D3D11_USAGE_DYNAMIC;
    default: return D3D11_USAGE_DEFAULT;
    }
}

// Helper to convert ImPlatform usage to D3D11 CPU access flags
static UINT ImPlatform_GetD3D11CPUAccessFlags(ImPlatform_BufferUsage usage)
{
    switch (usage)
    {
    case ImPlatform_BufferUsage_Static:  return 0;
    case ImPlatform_BufferUsage_Dynamic: return D3D11_CPU_ACCESS_WRITE;
    case ImPlatform_BufferUsage_Stream:  return D3D11_CPU_ACCESS_WRITE;
    default: return 0;
    }
}

// Helper to convert ImPlatform vertex format to DXGI format
static DXGI_FORMAT ImPlatform_GetDXGIFormat(ImPlatform_VertexFormat format)
{
    switch (format)
    {
    case ImPlatform_VertexFormat_Float:     return DXGI_FORMAT_R32_FLOAT;
    case ImPlatform_VertexFormat_Float2:    return DXGI_FORMAT_R32G32_FLOAT;
    case ImPlatform_VertexFormat_Float3:    return DXGI_FORMAT_R32G32B32_FLOAT;
    case ImPlatform_VertexFormat_Float4:    return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case ImPlatform_VertexFormat_UByte4:    return DXGI_FORMAT_R8G8B8A8_UINT;
    default: return DXGI_FORMAT_R32G32B32A32_FLOAT;
    }
}

// Helper to convert ImPlatform index format to DXGI format
static DXGI_FORMAT ImPlatform_GetIndexFormat_DX11(ImPlatform_IndexFormat format)
{
    return (format == ImPlatform_IndexFormat_UInt16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
}

IMPLATFORM_API ImPlatform_VertexBuffer ImPlatform_CreateVertexBuffer(const void* vertex_data, const ImPlatform_VertexBufferDesc* desc)
{
    if (!desc || !vertex_data)
        return NULL;

    ImPlatform_BufferData_DX11* buffer = new ImPlatform_BufferData_DX11();
    memset(buffer, 0, sizeof(ImPlatform_BufferData_DX11));
    buffer->vb_desc = *desc;

    // Copy attribute array
    if (desc->attribute_count > 0 && desc->attributes)
    {
        buffer->attributes = new ImPlatform_VertexAttribute[desc->attribute_count];
        memcpy(buffer->attributes, desc->attributes, sizeof(ImPlatform_VertexAttribute) * desc->attribute_count);
        buffer->vb_desc.attributes = buffer->attributes;
    }

    // Create vertex buffer
    D3D11_BUFFER_DESC bd;
    memset(&bd, 0, sizeof(bd));
    bd.Usage = ImPlatform_GetD3D11Usage(desc->usage);
    bd.ByteWidth = desc->vertex_count * desc->vertex_stride;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = ImPlatform_GetD3D11CPUAccessFlags(desc->usage);

    D3D11_SUBRESOURCE_DATA initData;
    memset(&initData, 0, sizeof(initData));
    initData.pSysMem = vertex_data;

    HRESULT hr = g_GfxData.pDevice->CreateBuffer(&bd, &initData, &buffer->pVertexBuffer);
    if (FAILED(hr))
    {
        delete[] buffer->attributes;
        delete buffer;
        return NULL;
    }

    return (ImPlatform_VertexBuffer)buffer;
}

IMPLATFORM_API bool ImPlatform_UpdateVertexBuffer(ImPlatform_VertexBuffer vertex_buffer, const void* vertex_data, unsigned int vertex_count, unsigned int offset)
{
    if (!vertex_buffer || !vertex_data)
        return false;

    ImPlatform_BufferData_DX11* buffer = (ImPlatform_BufferData_DX11*)vertex_buffer;

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = g_GfxData.pDeviceContext->Map(buffer->pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr))
        return false;

    size_t byte_offset = offset * buffer->vb_desc.vertex_stride;
    size_t byte_size = vertex_count * buffer->vb_desc.vertex_stride;
    memcpy((char*)mapped.pData + byte_offset, vertex_data, byte_size);

    g_GfxData.pDeviceContext->Unmap(buffer->pVertexBuffer, 0);
    return true;
}

IMPLATFORM_API void ImPlatform_DestroyVertexBuffer(ImPlatform_VertexBuffer vertex_buffer)
{
    if (!vertex_buffer)
        return;

    ImPlatform_BufferData_DX11* buffer = (ImPlatform_BufferData_DX11*)vertex_buffer;

    if (buffer->pVertexBuffer)
        buffer->pVertexBuffer->Release();
    if (buffer->pInputLayout)
        buffer->pInputLayout->Release();

    delete[] buffer->attributes;
    delete buffer;
}

IMPLATFORM_API ImPlatform_IndexBuffer ImPlatform_CreateIndexBuffer(const void* index_data, const ImPlatform_IndexBufferDesc* desc)
{
    if (!desc || !index_data)
        return NULL;

    ImPlatform_BufferData_DX11* buffer = new ImPlatform_BufferData_DX11();
    memset(buffer, 0, sizeof(ImPlatform_BufferData_DX11));
    buffer->ib_desc = *desc;

    // Create index buffer
    unsigned int index_size = (desc->format == ImPlatform_IndexFormat_UInt16) ? sizeof(uint16_t) : sizeof(uint32_t);

    D3D11_BUFFER_DESC bd;
    memset(&bd, 0, sizeof(bd));
    bd.Usage = ImPlatform_GetD3D11Usage(desc->usage);
    bd.ByteWidth = desc->index_count * index_size;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = ImPlatform_GetD3D11CPUAccessFlags(desc->usage);

    D3D11_SUBRESOURCE_DATA initData;
    memset(&initData, 0, sizeof(initData));
    initData.pSysMem = index_data;

    HRESULT hr = g_GfxData.pDevice->CreateBuffer(&bd, &initData, &buffer->pIndexBuffer);
    if (FAILED(hr))
    {
        delete buffer;
        return NULL;
    }

    return (ImPlatform_IndexBuffer)buffer;
}

IMPLATFORM_API bool ImPlatform_UpdateIndexBuffer(ImPlatform_IndexBuffer index_buffer, const void* index_data, unsigned int index_count, unsigned int offset)
{
    if (!index_buffer || !index_data)
        return false;

    ImPlatform_BufferData_DX11* buffer = (ImPlatform_BufferData_DX11*)index_buffer;
    unsigned int index_size = (buffer->ib_desc.format == ImPlatform_IndexFormat_UInt16) ? sizeof(uint16_t) : sizeof(uint32_t);

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = g_GfxData.pDeviceContext->Map(buffer->pIndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr))
        return false;

    size_t byte_offset = offset * index_size;
    size_t byte_size = index_count * index_size;
    memcpy((char*)mapped.pData + byte_offset, index_data, byte_size);

    g_GfxData.pDeviceContext->Unmap(buffer->pIndexBuffer, 0);
    return true;
}

IMPLATFORM_API void ImPlatform_DestroyIndexBuffer(ImPlatform_IndexBuffer index_buffer)
{
    if (!index_buffer)
        return;

    ImPlatform_BufferData_DX11* buffer = (ImPlatform_BufferData_DX11*)index_buffer;

    if (buffer->pIndexBuffer)
        buffer->pIndexBuffer->Release();

    delete buffer;
}

IMPLATFORM_API void ImPlatform_BindVertexBuffer(ImPlatform_VertexBuffer vertex_buffer)
{
    if (!vertex_buffer)
        return;

    ImPlatform_BufferData_DX11* buffer = (ImPlatform_BufferData_DX11*)vertex_buffer;

    UINT stride = buffer->vb_desc.vertex_stride;
    UINT offset = 0;
    g_GfxData.pDeviceContext->IASetVertexBuffers(0, 1, &buffer->pVertexBuffer, &stride, &offset);

    if (buffer->pInputLayout)
        g_GfxData.pDeviceContext->IASetInputLayout(buffer->pInputLayout);
}

IMPLATFORM_API void ImPlatform_BindIndexBuffer(ImPlatform_IndexBuffer index_buffer)
{
    if (!index_buffer)
        return;

    ImPlatform_BufferData_DX11* buffer = (ImPlatform_BufferData_DX11*)index_buffer;
    DXGI_FORMAT format = ImPlatform_GetIndexFormat_DX11(buffer->ib_desc.format);

    g_GfxData.pDeviceContext->IASetIndexBuffer(buffer->pIndexBuffer, format, 0);
}

IMPLATFORM_API void ImPlatform_BindBuffers(ImPlatform_VertexBuffer vertex_buffer, ImPlatform_IndexBuffer index_buffer)
{
    ImPlatform_BindVertexBuffer(vertex_buffer);
    ImPlatform_BindIndexBuffer(index_buffer);
}

IMPLATFORM_API void ImPlatform_DrawIndexed(unsigned int primitive_type, unsigned int index_count, unsigned int start_index)
{
    D3D11_PRIMITIVE_TOPOLOGY topology;
    switch (primitive_type)
    {
    case 0: topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break; // ImPlatform_PrimitiveType_Triangles
    case 1: topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST; break;     // ImPlatform_PrimitiveType_Lines
    case 2: topology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST; break;    // ImPlatform_PrimitiveType_Points
    default: topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
    }

    g_GfxData.pDeviceContext->IASetPrimitiveTopology(topology);
    g_GfxData.pDeviceContext->DrawIndexed(index_count, start_index, 0);
}

// ============================================================================
// Custom Shader System API - DirectX 11
// ============================================================================

// Internal shader structure for DX11
struct ImPlatform_ShaderData_DX11
{
    union
    {
        ID3D11VertexShader* pVertexShader;
        ID3D11PixelShader* pPixelShader;
    };
    ImPlatform_ShaderStage stage;
    ID3DBlob* pBlob; // Keep blob for input layout creation
};

// Internal shader program structure for DX11
struct ImPlatform_ShaderProgramData_DX11
{
    ID3D11VertexShader* pVertexShader;
    ID3D11PixelShader* pPixelShader;
    ID3DBlob* pVSBlob;
    ID3D11InputLayout* pInputLayout;
    ID3D11Buffer* pVertexConstantBuffer;
    ID3D11Buffer* pPixelConstantBuffer;
    void* pPixelConstantData;  // CPU-side copy of pixel shader constants
    size_t pixelConstantDataSize;
    bool pixelConstantDataDirty;
};

IMPLATFORM_API bool ImPlatform_CreateVertexInputLayout(ImPlatform_VertexBuffer vertex_buffer, ImPlatform_ShaderProgram program)
{
    if (!vertex_buffer || !program)
        return false;

    ImPlatform_BufferData_DX11*        vb   = (ImPlatform_BufferData_DX11*)vertex_buffer;
    ImPlatform_ShaderProgramData_DX11* prog = (ImPlatform_ShaderProgramData_DX11*)program;

    if (!prog->pVSBlob || !vb->vb_desc.attributes || vb->vb_desc.attribute_count == 0)
        return false;

    // Release any previously created layout
    if (vb->pInputLayout)
    {
        vb->pInputLayout->Release();
        vb->pInputLayout = nullptr;
    }

    unsigned int count = vb->vb_desc.attribute_count;

    // Build D3D11_INPUT_ELEMENT_DESC array.
    // Auto-increment SemanticIndex for attributes sharing the same semantic name
    // (e.g., two "TEXCOORD" entries get indices 0 and 1).
    D3D11_INPUT_ELEMENT_DESC* layout = new D3D11_INPUT_ELEMENT_DESC[count];
    for (unsigned int i = 0; i < count; i++)
    {
        const ImPlatform_VertexAttribute& attr = vb->vb_desc.attributes[i];

        // Count how many earlier attributes share this semantic name
        unsigned int semantic_index = 0;
        for (unsigned int j = 0; j < i; j++)
        {
            if (strcmp(vb->vb_desc.attributes[j].semantic_name, attr.semantic_name) == 0)
                semantic_index++;
        }

        layout[i].SemanticName         = attr.semantic_name;
        layout[i].SemanticIndex        = semantic_index;
        layout[i].Format               = ImPlatform_GetDXGIFormat(attr.format);
        layout[i].InputSlot            = 0;
        layout[i].AlignedByteOffset    = attr.offset;
        layout[i].InputSlotClass       = D3D11_INPUT_PER_VERTEX_DATA;
        layout[i].InstanceDataStepRate = 0;
    }

    HRESULT hr = g_GfxData.pDevice->CreateInputLayout(
        layout, count,
        prog->pVSBlob->GetBufferPointer(),
        prog->pVSBlob->GetBufferSize(),
        &vb->pInputLayout);

    delete[] layout;
    return SUCCEEDED(hr);
}

// Global state for uniform block batching
static ImPlatform_ShaderProgram g_CurrentUniformBlockProgram = nullptr;
static void* g_UniformBlockData = nullptr;
static size_t g_UniformBlockSize = 0;

// Constant buffer structure for projection matrix
struct VERTEX_CONSTANT_BUFFER_DX11
{
    float   mvp[4][4];
};

// Translate IMPLATFORM_SHADER_COMPILE_* flags to D3DCompile flags.
static UINT ImPlatform_DX11_TranslateCompileFlags(unsigned int compile_flags)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
    if (compile_flags & IMPLATFORM_SHADER_COMPILE_SKIP_OPTIMIZATION) flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
    if (compile_flags & IMPLATFORM_SHADER_COMPILE_OPTIMIZATION_LOW)  flags |= D3DCOMPILE_OPTIMIZATION_LEVEL0;
    if (compile_flags & IMPLATFORM_SHADER_COMPILE_OPTIMIZATION_HIGH) flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
    if (compile_flags & IMPLATFORM_SHADER_COMPILE_DEBUG)             flags |= D3DCOMPILE_DEBUG;
    return flags;
}

// Try to load cached DXBC bytecode for this shader into a fresh ID3DBlob.
// Returns NULL on cache miss or I/O error. out_cache_path receives the
// computed cache path so the caller can save after a successful compile.
static ID3DBlob* ImPlatform_DX11_TryLoadCachedBytecode(
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

    ImPlatform_ShaderData_DX11* shader_data = new ImPlatform_ShaderData_DX11();
    memset(shader_data, 0, sizeof(ImPlatform_ShaderData_DX11));
    shader_data->stage = desc->stage;

    HRESULT hr = S_OK;

    // Resolve target profile (also used as part of the cache key hash)
    const char* target = NULL;
    if (desc->stage == ImPlatform_ShaderStage_Vertex)
        target = "vs_5_0";
    else if (desc->stage == ImPlatform_ShaderStage_Fragment)
        target = "ps_5_0";
    else
    {
        delete shader_data;
        return NULL;
    }

    if (desc->bytecode && desc->bytecode_size > 0)
    {
        // Caller-supplied bytecode: wrap in an ID3DBlob so the rest of the
        // pipeline (CreateVertexShader/PixelShader, CreateInputLayout) works
        // uniformly. Cache and compile_flags are ignored on this path.
        hr = D3DCreateBlob(desc->bytecode_size, &shader_data->pBlob);
        if (FAILED(hr) || !shader_data->pBlob)
        {
            fprintf(stderr, "Shader bytecode blob allocation failed (size=%u)\n", desc->bytecode_size);
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
        shader_data->pBlob = ImPlatform_DX11_TryLoadCachedBytecode(
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
            UINT flags = ImPlatform_DX11_TranslateCompileFlags(desc->compile_flags);
            hr = D3DCompile(
                desc->source_code, source_len,
                NULL, NULL, NULL,
                entry, target, flags, 0,
                &shader_data->pBlob, &pErrorBlob);

            if (FAILED(hr))
            {
                if (pErrorBlob)
                {
                    fprintf(stderr, "Shader compilation failed: %s\n", (char*)pErrorBlob->GetBufferPointer());
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

    // Create shader object
    if (desc->stage == ImPlatform_ShaderStage_Vertex)
    {
        hr = g_GfxData.pDevice->CreateVertexShader(
            shader_data->pBlob->GetBufferPointer(),
            shader_data->pBlob->GetBufferSize(),
            NULL,
            &shader_data->pVertexShader);
    }
    else
    {
        hr = g_GfxData.pDevice->CreatePixelShader(
            shader_data->pBlob->GetBufferPointer(),
            shader_data->pBlob->GetBufferSize(),
            NULL,
            &shader_data->pPixelShader);
    }

    if (FAILED(hr))
    {
        shader_data->pBlob->Release();
        delete shader_data;
        return NULL;
    }

    return (ImPlatform_Shader)shader_data;
}

IMPLATFORM_API void ImPlatform_DestroyShader(ImPlatform_Shader shader)
{
    if (!shader)
        return;

    ImPlatform_ShaderData_DX11* shader_data = (ImPlatform_ShaderData_DX11*)shader;

    if (shader_data->stage == ImPlatform_ShaderStage_Vertex && shader_data->pVertexShader)
        shader_data->pVertexShader->Release();
    else if (shader_data->stage == ImPlatform_ShaderStage_Fragment && shader_data->pPixelShader)
        shader_data->pPixelShader->Release();

    if (shader_data->pBlob)
        shader_data->pBlob->Release();

    delete shader_data;
}

IMPLATFORM_API ImPlatform_ShaderProgram ImPlatform_CreateShaderProgram(ImPlatform_Shader vertex_shader, ImPlatform_Shader fragment_shader)
{
    if (!vertex_shader || !fragment_shader)
        return NULL;

    ImPlatform_ShaderData_DX11* vs_data = (ImPlatform_ShaderData_DX11*)vertex_shader;
    ImPlatform_ShaderData_DX11* ps_data = (ImPlatform_ShaderData_DX11*)fragment_shader;

    if (vs_data->stage != ImPlatform_ShaderStage_Vertex || ps_data->stage != ImPlatform_ShaderStage_Fragment)
        return NULL;

    ImPlatform_ShaderProgramData_DX11* program = new ImPlatform_ShaderProgramData_DX11();
    memset(program, 0, sizeof(ImPlatform_ShaderProgramData_DX11));
    program->pVertexShader = vs_data->pVertexShader;
    program->pPixelShader = ps_data->pPixelShader;
    program->pVSBlob = vs_data->pBlob;

    // Add ref to keep shaders alive
    program->pVertexShader->AddRef();
    program->pPixelShader->AddRef();
    if (program->pVSBlob)
        program->pVSBlob->AddRef();

    // Create input layout for ImGui's vertex format (must match ImDrawVert structure)
    // ImDrawVert layout: pos(8 bytes), uv(8 bytes), col(4 bytes) = offsets 0, 8, 16
    if (program->pVSBlob)
    {
        D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 8,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        g_GfxData.pDevice->CreateInputLayout(
            layout,
            3,
            program->pVSBlob->GetBufferPointer(),
            program->pVSBlob->GetBufferSize(),
            &program->pInputLayout
        );
    }

    // Create constant buffer for projection matrix
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(VERTEX_CONSTANT_BUFFER_DX11);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbDesc.MiscFlags = 0;
    g_GfxData.pDevice->CreateBuffer(&cbDesc, nullptr, &program->pVertexConstantBuffer);

    return (ImPlatform_ShaderProgram)program;
}

IMPLATFORM_API void ImPlatform_DestroyShaderProgram(ImPlatform_ShaderProgram program)
{
    if (!program)
        return;

    ImPlatform_ShaderProgramData_DX11* program_data = (ImPlatform_ShaderProgramData_DX11*)program;

    if (program_data->pVertexShader)
        program_data->pVertexShader->Release();
    if (program_data->pPixelShader)
        program_data->pPixelShader->Release();
    if (program_data->pVSBlob)
        program_data->pVSBlob->Release();
    if (program_data->pInputLayout)
        program_data->pInputLayout->Release();
    if (program_data->pVertexConstantBuffer)
        program_data->pVertexConstantBuffer->Release();
    if (program_data->pPixelConstantBuffer)
        program_data->pPixelConstantBuffer->Release();
    if (program_data->pPixelConstantData)
        free(program_data->pPixelConstantData);

    delete program_data;
}

IMPLATFORM_API void ImPlatform_UseShaderProgram(ImPlatform_ShaderProgram program)
{
    if (!program)
        return;

    ImPlatform_ShaderProgramData_DX11* program_data = (ImPlatform_ShaderProgramData_DX11*)program;

    g_GfxData.pDeviceContext->VSSetShader(program_data->pVertexShader, NULL, 0);
    g_GfxData.pDeviceContext->PSSetShader(program_data->pPixelShader, NULL, 0);
}

IMPLATFORM_API bool ImPlatform_SetShaderUniform(ImPlatform_ShaderProgram program, const char* /*name*/, const void* data, unsigned int size)
{
    if (!program || !data || size == 0)
        return false;

    ImPlatform_ShaderProgramData_DX11* program_data = (ImPlatform_ShaderProgramData_DX11*)program;

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

        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.ByteWidth = (size + 15) & ~15; // Round up to 16-byte boundary
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        cbDesc.MiscFlags = 0;
        g_GfxData.pDevice->CreateBuffer(&cbDesc, nullptr, &program_data->pPixelConstantBuffer);
    }

    // Copy data to CPU storage
    memcpy(program_data->pPixelConstantData, data, size);
    program_data->pixelConstantDataDirty = true;

    return true;
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

IMPLATFORM_API bool ImPlatform_SetShaderTexture(ImPlatform_ShaderProgram program, const char* /*name*/, unsigned int slot, ImTextureID texture)
{
    if (!program || !texture)
        return false;

    ID3D11ShaderResourceView* pSRV = (ID3D11ShaderResourceView*)texture;
    g_GfxData.pDeviceContext->PSSetShaderResources(slot, 1, &pSRV);

    return true;
}

// ============================================================================
// Custom Shader DrawList Integration
// ============================================================================

// ImDrawCallback handler to activate a custom shader
static void ImPlatform_SetCustomShader(const ImDrawList* /*parent_list*/, const ImDrawCmd* cmd)
{
    ImPlatform_ShaderProgram program = (ImPlatform_ShaderProgram)cmd->UserCallbackData;
    if (program)
    {
        ImPlatform_ShaderProgramData_DX11* program_data = (ImPlatform_ShaderProgramData_DX11*)program;

        // Set custom shaders
        ImPlatform_UseShaderProgram(program);

        // Set the input layout for the custom shader
        if (program_data->pInputLayout)
        {
            g_GfxData.pDeviceContext->IASetInputLayout(program_data->pInputLayout);
        }

        // Note: Vertex constant buffer (b0) is already set by ImGui's backend with projection matrix

        // Update pixel shader constant buffer if dirty
        if (program_data->pPixelConstantBuffer && program_data->pixelConstantDataDirty && program_data->pPixelConstantData)
        {
            D3D11_MAPPED_SUBRESOURCE mapped_resource;
            if (g_GfxData.pDeviceContext->Map(program_data->pPixelConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource) == S_OK)
            {
                memcpy(mapped_resource.pData, program_data->pPixelConstantData, program_data->pixelConstantDataSize);
                g_GfxData.pDeviceContext->Unmap(program_data->pPixelConstantBuffer, 0);
                program_data->pixelConstantDataDirty = false;
            }

            // Bind pixel shader constant buffer to register b1
            g_GfxData.pDeviceContext->PSSetConstantBuffers(1, 1, &program_data->pPixelConstantBuffer);
        }
    }
}

// Activate a custom shader immediately (for use inside draw callbacks).
IMPLATFORM_API void ImPlatform_BeginCustomShader_Render(ImPlatform_ShaderProgram program)
{
    if (!program) return;
    ImPlatform_ShaderProgramData_DX11* program_data = (ImPlatform_ShaderProgramData_DX11*)program;
    ImPlatform_UseShaderProgram(program);
    if (program_data->pInputLayout)
        g_GfxData.pDeviceContext->IASetInputLayout(program_data->pInputLayout);
    if (program_data->pPixelConstantBuffer && program_data->pixelConstantDataDirty && program_data->pPixelConstantData)
    {
        D3D11_MAPPED_SUBRESOURCE mapped_resource;
        if (g_GfxData.pDeviceContext->Map(program_data->pPixelConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource) == S_OK)
        {
            memcpy(mapped_resource.pData, program_data->pPixelConstantData, program_data->pixelConstantDataSize);
            g_GfxData.pDeviceContext->Unmap(program_data->pPixelConstantBuffer, 0);
            program_data->pixelConstantDataDirty = false;
        }
        g_GfxData.pDeviceContext->PSSetConstantBuffers(1, 1, &program_data->pPixelConstantBuffer);
    }
    else if (program_data->pPixelConstantBuffer)
    {
        // Even if not dirty, ensure the constant buffer is bound
        g_GfxData.pDeviceContext->PSSetConstantBuffers(1, 1, &program_data->pPixelConstantBuffer);
    }
}

IMPLATFORM_API void* ImPlatform_PushShaderConstants(const void* data, unsigned int size)
{
    if (!data || size == 0) return nullptr;

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = (size + 15) & ~15;
    cbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = data;

    ID3D11Buffer* pBuffer = nullptr;
    HRESULT hr = g_GfxData.pDevice->CreateBuffer(&cbDesc, &initData, &pBuffer);
    if (FAILED(hr) || !pBuffer) return nullptr;

    g_GfxData.pDeviceContext->PSSetConstantBuffers(1, 1, &pBuffer);
    return (void*)pBuffer;
}

IMPLATFORM_API void ImPlatform_PopShaderConstants(void* handle)
{
    if (handle)
        ((ID3D11Buffer*)handle)->Release();
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

    draw->AddCallback(ImDrawCallback_ResetRenderState, NULL);
}

#endif // IM_GFX_DIRECTX11
