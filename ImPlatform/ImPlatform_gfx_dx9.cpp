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

// ============================================================================
// Texture Creation API - DirectX 9 Implementation
// ============================================================================

// Helper to get D3D9 format from ImPlatform format
static D3DFORMAT ImPlatform_GetD3D9Format(ImPlatform_PixelFormat format, int* out_bytes_per_pixel)
{
    switch (format)
    {
    case ImPlatform_PixelFormat_R8:
        *out_bytes_per_pixel = 1;
        return D3DFMT_L8; // Luminance 8-bit
    case ImPlatform_PixelFormat_RG8:
        // D3D9 doesn't have RG8, use A8L8 (alpha+luminance)
        *out_bytes_per_pixel = 2;
        return D3DFMT_A8L8;
    case ImPlatform_PixelFormat_RGB8:
        // D3D9 doesn't have RGB8, use X8R8G8B8 or A8R8G8B8
        *out_bytes_per_pixel = 4;
        return D3DFMT_X8R8G8B8;
    case ImPlatform_PixelFormat_RGBA8:
        *out_bytes_per_pixel = 4;
        return D3DFMT_A8R8G8B8;
    case ImPlatform_PixelFormat_R16:
        *out_bytes_per_pixel = 2;
        return D3DFMT_L16; // Luminance 16-bit
    case ImPlatform_PixelFormat_RG16:
        // D3D9 doesn't have true RG16, use G16R16
        *out_bytes_per_pixel = 4;
        return D3DFMT_G16R16;
    case ImPlatform_PixelFormat_RGBA16:
        *out_bytes_per_pixel = 8;
        return D3DFMT_A16B16G16R16;
    case ImPlatform_PixelFormat_R32F:
        *out_bytes_per_pixel = 4;
        return D3DFMT_R32F;
    case ImPlatform_PixelFormat_RG32F:
        *out_bytes_per_pixel = 8;
        return D3DFMT_G32R32F;
    case ImPlatform_PixelFormat_RGBA32F:
        *out_bytes_per_pixel = 16;
        return D3DFMT_A32B32G32R32F;
    default:
        *out_bytes_per_pixel = 4;
        return D3DFMT_A8R8G8B8;
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
    D3DFORMAT format = ImPlatform_GetD3D9Format(desc->format, &bytes_per_pixel);

    // Create texture
    LPDIRECT3DTEXTURE9 pTexture = NULL;
    HRESULT hr = g_GfxData.pDevice->CreateTexture(
        desc->width, desc->height, 1, 0,
        format, D3DPOOL_MANAGED, &pTexture, NULL);

    if (FAILED(hr) || !pTexture)
        return NULL;

    // Lock and copy data
    D3DLOCKED_RECT rect;
    hr = pTexture->LockRect(0, &rect, NULL, D3DLOCK_DISCARD);
    if (FAILED(hr))
    {
        pTexture->Release();
        return NULL;
    }

    // Copy pixel data row by row
    unsigned char* dest = (unsigned char*)rect.pBits;
    const unsigned char* src = (const unsigned char*)pixel_data;
    for (unsigned int y = 0; y < desc->height; y++)
    {
        memcpy(dest + y * rect.Pitch, src + y * desc->width * bytes_per_pixel, desc->width * bytes_per_pixel);
    }

    pTexture->UnlockRect(0);

    // Set texture to stage 0 (not strictly necessary but matches old implementation)
    g_GfxData.pDevice->SetTexture(0, pTexture);

    // Set sampler states based on descriptor
    D3DTEXTUREFILTERTYPE min_filter = (desc->min_filter == ImPlatform_TextureFilter_Nearest) ? D3DTEXF_POINT : D3DTEXF_LINEAR;
    D3DTEXTUREFILTERTYPE mag_filter = (desc->mag_filter == ImPlatform_TextureFilter_Nearest) ? D3DTEXF_POINT : D3DTEXF_LINEAR;

    g_GfxData.pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, min_filter);
    g_GfxData.pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, mag_filter);

    // Set addressing modes
    D3DTEXTUREADDRESS wrap_u = D3DTADDRESS_CLAMP;
    D3DTEXTUREADDRESS wrap_v = D3DTADDRESS_CLAMP;

    if (desc->wrap_u == ImPlatform_TextureWrap_Repeat)
        wrap_u = D3DTADDRESS_WRAP;
    else if (desc->wrap_u == ImPlatform_TextureWrap_Mirror)
        wrap_u = D3DTADDRESS_MIRROR;

    if (desc->wrap_v == ImPlatform_TextureWrap_Repeat)
        wrap_v = D3DTADDRESS_WRAP;
    else if (desc->wrap_v == ImPlatform_TextureWrap_Mirror)
        wrap_v = D3DTADDRESS_MIRROR;

    g_GfxData.pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, wrap_u);
    g_GfxData.pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, wrap_v);

    return (ImTextureID)pTexture;
}

IMPLATFORM_API bool ImPlatform_UpdateTexture(ImTextureID texture_id, const void* pixel_data,
                                              unsigned int x, unsigned int y,
                                              unsigned int width, unsigned int height)
{
    if (!texture_id || !pixel_data)
        return false;

    LPDIRECT3DTEXTURE9 pTexture = (LPDIRECT3DTEXTURE9)texture_id;

    // Get texture description
    D3DSURFACE_DESC desc;
    HRESULT hr = pTexture->GetLevelDesc(0, &desc);
    if (FAILED(hr))
        return false;

    // Calculate bytes per pixel from format
    int bytes_per_pixel = 4; // Default
    switch (desc.Format)
    {
    case D3DFMT_L8: bytes_per_pixel = 1; break;
    case D3DFMT_A8L8: bytes_per_pixel = 2; break;
    case D3DFMT_X8R8G8B8:
    case D3DFMT_A8R8G8B8: bytes_per_pixel = 4; break;
    case D3DFMT_L16: bytes_per_pixel = 2; break;
    case D3DFMT_G16R16: bytes_per_pixel = 4; break;
    case D3DFMT_A16B16G16R16: bytes_per_pixel = 8; break;
    case D3DFMT_R32F: bytes_per_pixel = 4; break;
    case D3DFMT_G32R32F: bytes_per_pixel = 8; break;
    case D3DFMT_A32B32G32R32F: bytes_per_pixel = 16; break;
    }

    // Lock the sub-rectangle
    RECT rect;
    rect.left = x;
    rect.top = y;
    rect.right = x + width;
    rect.bottom = y + height;

    D3DLOCKED_RECT locked_rect;
    hr = pTexture->LockRect(0, &locked_rect, &rect, 0);
    if (FAILED(hr))
        return false;

    // Copy data
    unsigned char* dest = (unsigned char*)locked_rect.pBits;
    const unsigned char* src = (const unsigned char*)pixel_data;
    for (unsigned int row = 0; row < height; row++)
    {
        memcpy(dest + row * locked_rect.Pitch, src + row * width * bytes_per_pixel, width * bytes_per_pixel);
    }

    pTexture->UnlockRect(0);
    return true;
}

IMPLATFORM_API void ImPlatform_DestroyTexture(ImTextureID texture_id)
{
    if (!texture_id)
        return;

    LPDIRECT3DTEXTURE9 pTexture = (LPDIRECT3DTEXTURE9)texture_id;
    pTexture->Release();
}

#endif // IM_GFX_DIRECTX9
