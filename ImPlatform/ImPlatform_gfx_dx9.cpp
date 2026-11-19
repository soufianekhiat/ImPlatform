// dear imgui: Graphics API Abstraction - DirectX 9 Backend
// This handles D3D9 device creation, rendering, and presentation

#include "ImPlatform_Internal.h"

#ifdef IM_GFX_DIRECTX9

#include "../imgui/backends/imgui_impl_dx9.h"
#include <d3dcompiler.h>
#include <d3d11shader.h>     // ID3D11ShaderReflection: cbuffer layouts of custom shaders
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// Link with d3d9.lib
#pragma comment(lib, "d3d9")
#pragma comment(lib, "d3dcompiler")

// Global state
static ImPlatform_GfxData_DX9 g_GfxData = { 0 };
unsigned int g_ImPlatform_BackbufferW = 0;
unsigned int g_ImPlatform_BackbufferH = 0;

// Render texture tracking (texture ↔ render target surface)
struct ImPlatform_RTTracking_DX9 {
    LPDIRECT3DTEXTURE9  pTexture;
    IDirect3DSurface9*  pSurface;
    ImPlatform_RTTracking_DX9* next;
};
static ImPlatform_RTTracking_DX9* g_RTTrackingHead = NULL;

static ImPlatform_RTTracking_DX9* ImPlatform_FindRT_DX9(ImTextureID texture)
{
    ImPlatform_RTTracking_DX9* entry = g_RTTrackingHead;
    while (entry && entry->pTexture != (LPDIRECT3DTEXTURE9)texture)
        entry = entry->next;
    return entry;
}

// Textures created from RGBA8 data: stored as D3DFMT_A8R8G8B8 (B, G, R, A), so uploads and readbacks swap red and blue.
static ImVector<IDirect3DTexture9*> g_P9_RgbaTextures;

static bool ImP9_IsRgba(IDirect3DTexture9* texture) { return g_P9_RgbaTextures.contains(texture); }

// Copies pixels 4-byte pixels, swapping bytes 0 and 2 when swap.
static void ImP9_CopyPixels(unsigned char* dst, const unsigned char* src, unsigned int pixels, bool swap)
{
    if (!swap)
    {
        memcpy(dst, src, (size_t)pixels * 4);
        return;
    }
    for (unsigned int i = 0; i < pixels; i++, dst += 4, src += 4)
    {
        dst[0] = src[2]; dst[1] = src[1]; dst[2] = src[0]; dst[3] = src[3];
    }
}

// Render pass queued by ImPlatform_QueueRenderPass(), run at the start of ImPlatform_GfxAPIRender() (inside the scene)
static ImTextureID g_PassTarget = NULL;
static ImDrawData* g_PassDrawData = NULL;
static bool        g_PassClear = false;
static void        ImPlatform_RunRenderPass();

// Sampler stack for PushSampler/PopSampler — saves D3D9 per-stage state
struct ImPlatform_SamplerStackEntry_DX9 { DWORD minF, magF, addrU, addrV; };
static ImPlatform_SamplerStackEntry_DX9 g_SamplerStack[8];
static int                              g_SamplerDepth = 0;

// Uniform block API state
static ImPlatform_ShaderProgram g_CurrentUniformBlockProgram = nullptr;
static void* g_UniformBlockData = nullptr;
static size_t g_UniformBlockSize = 0;

// Update stored backbuffer size from the device backbuffer (d3dpp sizes may be 0 = window client size)
static void ImPlatform_UpdateBackbufferSize_DX9(ImPlatform_GfxData_DX9* pData)
{
    IDirect3DSurface9* pBackBuffer = NULL;
    if (pData->pDevice && SUCCEEDED(pData->pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer)) && pBackBuffer)
    {
        D3DSURFACE_DESC bbDesc;
        if (SUCCEEDED(pBackBuffer->GetDesc(&bbDesc)))
        {
            g_ImPlatform_BackbufferW = bbDesc.Width;
            g_ImPlatform_BackbufferH = bbDesc.Height;
        }
        pBackBuffer->Release();
    }
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

    const HRESULT hr = pData->pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                                 &pData->d3dpp, &pData->pDevice);
    if (FAILED(hr))
    {
        // D3DERR_NOTAVAILABLE / DEVICELOST also while the session is locked or the display is off
        fprintf(stderr, "[ImPlatform] DX9: CreateDevice failed (0x%08lx)\n", (unsigned long)hr);
        return false;
    }

    pData->bDeviceLost = false;
    pData->uResizeWidth = 0;
    pData->uResizeHeight = 0;
    ImPlatform_UpdateBackbufferSize_DX9(pData);

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
        IM_ASSERT(0); // Reset fails while D3DPOOL_DEFAULT resources are still alive (e.g. ImPlatform render textures)
    // Always recreate device objects, same as upstream example_win32_directx9 (also recreates secondary viewports swapchains)
    ImGui_ImplDX9_CreateDeviceObjects();
    ImPlatform_UpdateBackbufferSize_DX9(pData);
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
#ifdef _WIN32
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

    ImPlatform_RunRenderPass();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    g_GfxData.pDevice->EndScene();

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
        // Stored B, G, R, A (A8B8G8R8 is missing on common drivers): the rows are swizzled (ImP9_RgbaTextures).
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

IMPLATFORM_API ImPlatform_TextureDesc3D ImPlatform_TextureDesc3D_Default(unsigned int w, unsigned int h, unsigned int d)
{
    ImPlatform_TextureDesc3D x;
    x.width = w; x.height = h; x.depth = d;
    x.format = ImPlatform_PixelFormat_RGBA8;
    x.min_filter = ImPlatform_TextureFilter_Linear;
    x.mag_filter = ImPlatform_TextureFilter_Linear;
    x.wrap_u = x.wrap_v = x.wrap_w = ImPlatform_TextureWrap_Clamp;
    return x;
}
IMPLATFORM_API bool ImPlatform_SupportsTexture3D(void)
{
    D3DCAPS9 caps;
    return g_GfxData.pDevice && SUCCEEDED(g_GfxData.pDevice->GetDeviceCaps(&caps)) && (caps.TextureCaps & D3DPTEXTURECAPS_VOLUMEMAP);
}

// A volume texture (sampled as a Texture3D by shader model 3); RGBA8 is stored as B, G, R, A like the 2D textures.
IMPLATFORM_API ImTextureID ImPlatform_CreateTexture3D(const void* voxel_data, const ImPlatform_TextureDesc3D* desc)
{
    if (!desc || !voxel_data || !ImPlatform_SupportsTexture3D())
        return NULL;
    int bytes_per_pixel;
    D3DFORMAT format = ImPlatform_GetD3D9Format(desc->format, &bytes_per_pixel);
    IDirect3DVolumeTexture9* tex = NULL;
    if (FAILED(g_GfxData.pDevice->CreateVolumeTexture(desc->width, desc->height, desc->depth, 1, 0, format, D3DPOOL_MANAGED, &tex, NULL)) || !tex)
        return NULL;
    D3DLOCKED_BOX box;
    if (FAILED(tex->LockBox(0, &box, NULL, 0)))
    {
        tex->Release();
        return NULL;
    }
    const unsigned char* src = (const unsigned char*)voxel_data;
    const unsigned int row = desc->width * (unsigned int)bytes_per_pixel;
    for (unsigned int z = 0; z < desc->depth; z++)
        for (unsigned int y = 0; y < desc->height; y++)
        {
            unsigned char* dst = (unsigned char*)box.pBits + z * box.SlicePitch + y * box.RowPitch;
            const unsigned char* s = src + ((size_t)z * desc->height + y) * row;
            if (desc->format == ImPlatform_PixelFormat_RGBA8)
                ImP9_CopyPixels(dst, s, desc->width, true);
            else
                memcpy(dst, s, row);
        }
    tex->UnlockBox(0);
    return (ImTextureID)tex;
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
    const bool rgba = desc->format == ImPlatform_PixelFormat_RGBA8;
    for (unsigned int y = 0; y < desc->height; y++)
    {
        if (rgba)
            ImP9_CopyPixels(dest + y * rect.Pitch, src + y * desc->width * 4, desc->width, true);
        else
            memcpy(dest + y * rect.Pitch, src + y * desc->width * bytes_per_pixel, desc->width * bytes_per_pixel);
    }

    pTexture->UnlockRect(0);
    if (rgba)
        g_P9_RgbaTextures.push_back(pTexture);

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
    const bool rgba = ImP9_IsRgba(pTexture);
    for (unsigned int row = 0; row < height; row++)
    {
        if (rgba)
            ImP9_CopyPixels(dest + row * locked_rect.Pitch, src + row * width * 4, width, true);
        else
            memcpy(dest + row * locked_rect.Pitch, src + row * width * bytes_per_pixel, width * bytes_per_pixel);
    }

    pTexture->UnlockRect(0);
    return true;
}

IMPLATFORM_API ImTextureID ImPlatform_CreateRenderTexture(const ImPlatform_TextureDesc* desc)
{
    if (!desc || !g_GfxData.pDevice)
        return NULL;

    int bytes_per_pixel;
    D3DFORMAT format = ImPlatform_GetD3D9Format(desc->format, &bytes_per_pixel);

    // Render targets require D3DPOOL_DEFAULT
    LPDIRECT3DTEXTURE9 pTexture = NULL;
    HRESULT hr = g_GfxData.pDevice->CreateTexture(
        desc->width, desc->height, 1,
        D3DUSAGE_RENDERTARGET, format,
        D3DPOOL_DEFAULT, &pTexture, NULL);
    if (FAILED(hr) || !pTexture)
        return NULL;

    IDirect3DSurface9* pSurface = NULL;
    hr = pTexture->GetSurfaceLevel(0, &pSurface);
    if (FAILED(hr) || !pSurface) { pTexture->Release(); return NULL; }

    ImPlatform_RTTracking_DX9* entry = new ImPlatform_RTTracking_DX9();
    entry->pTexture = pTexture;
    entry->pSurface = pSurface;
    entry->next     = g_RTTrackingHead;
    g_RTTrackingHead = entry;

    return (ImTextureID)pTexture;
}

IMPLATFORM_API bool ImPlatform_CopyBackbuffer(ImTextureID dst) { (void)dst; return false; }
IMPLATFORM_API void ImPlatform_GetBackbufferSize(unsigned int* width, unsigned int* height)
{
    if (width)  *width  = g_ImPlatform_BackbufferW;
    if (height) *height = g_ImPlatform_BackbufferH;
}

IMPLATFORM_API bool ImPlatform_CopyTexture(ImTextureID dst, ImTextureID src)
{
    if (!dst || !src || !g_GfxData.pDevice)
        return false;

    LPDIRECT3DTEXTURE9 pDst = (LPDIRECT3DTEXTURE9)dst;
    LPDIRECT3DTEXTURE9 pSrc = (LPDIRECT3DTEXTURE9)src;

    IDirect3DSurface9* pDstSurf = NULL;
    IDirect3DSurface9* pSrcSurf = NULL;
    pDst->GetSurfaceLevel(0, &pDstSurf);
    pSrc->GetSurfaceLevel(0, &pSrcSurf);

    if (!pDstSurf || !pSrcSurf)
    {
        if (pDstSurf) pDstSurf->Release();
        if (pSrcSurf) pSrcSurf->Release();
        return false;
    }

    HRESULT hr = g_GfxData.pDevice->StretchRect(pSrcSurf, NULL, pDstSurf, NULL, D3DTEXF_NONE);

    pDstSurf->Release();
    pSrcSurf->Release();
    return SUCCEEDED(hr);
}

// D3DFMT_A8R8G8B8 render targets: BGRA in memory.
IMPLATFORM_API ImPlatform_PixelFormat ImPlatform_GetRenderPassFormat(void) { return ImPlatform_PixelFormat_BGRA8; }

IMPLATFORM_API bool ImPlatform_QueueRenderPass(ImTextureID target, ImDrawData* draw_data, bool clear)
{
    if (!ImPlatform_FindRT_DX9(target) || !draw_data)
        return false;
    g_PassTarget = target;
    g_PassDrawData = draw_data;
    g_PassClear = clear;
    return true;
}

static void ImPlatform_RunRenderPass()
{
    ImDrawData* draw_data = g_PassDrawData;
    g_PassDrawData = NULL;
    ImPlatform_RTTracking_DX9* entry = ImPlatform_FindRT_DX9(g_PassTarget);
    if (!draw_data || !entry)       // Destroyed since it was queued.
        return;
    LPDIRECT3DDEVICE9 dev = g_GfxData.pDevice;
    IDirect3DSurface9* saved = NULL;
    dev->GetRenderTarget(0, &saved);
    dev->SetTexture(0, NULL);       // Not bound as an input while it is the target.
    dev->SetRenderTarget(0, entry->pSurface);
    if (g_PassClear)
        dev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
    if (draw_data->Textures == NULL)
        draw_data->Textures = &ImGui::GetPlatformIO().Textures;     // The frame's texture updates, before they are drawn.
    ImPlatform_RenderPassLists(draw_data, [](ImDrawData* d) { ImGui_ImplDX9_RenderDrawData(d); });      // Viewport and projection from the draw data.
    dev->SetRenderTarget(0, saved);
    if (saved)
        saved->Release();
}

static DWORD g_BlendStack[8] = {};
static int   g_BlendDepth = 0;

IMPLATFORM_API void ImPlatform_PushPremultipliedAlpha(ImDrawList* draw)
{
    (draw ? draw : ImGui::GetWindowDrawList())->AddCallback([](const ImDrawList*, const ImDrawCmd*)
    {
        DWORD src = D3DBLEND_SRCALPHA;
        g_GfxData.pDevice->GetRenderState(D3DRS_SRCBLEND, &src);
        if (g_BlendDepth < IM_ARRAYSIZE(g_BlendStack))
            g_BlendStack[g_BlendDepth++] = src;
        g_GfxData.pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);    // Alpha already blends ONE, INV_SRC_ALPHA.
    }, NULL);
}

IMPLATFORM_API void ImPlatform_PopPremultipliedAlpha(ImDrawList* draw)
{
    (draw ? draw : ImGui::GetWindowDrawList())->AddCallback([](const ImDrawList*, const ImDrawCmd*)
    {
        if (g_BlendDepth > 0)
            g_GfxData.pDevice->SetRenderState(D3DRS_SRCBLEND, g_BlendStack[--g_BlendDepth]);
    }, NULL);
}

// Readback (synchronous): render targets through a system memory copy, other textures locked directly. Rows come out
// as stored (BGRA for render targets), RGBA for textures created from RGBA8 data.
IMPLATFORM_API bool ImPlatform_SupportsReadback(void) { return g_GfxData.pDevice != NULL; }
IMPLATFORM_API bool ImPlatform_ReadTexture(ImTextureID texture, unsigned int x, unsigned int y,
                                           unsigned int width, unsigned int height, void* out_pixels, size_t out_size)
{
    LPDIRECT3DTEXTURE9 tex = (LPDIRECT3DTEXTURE9)texture;
    D3DSURFACE_DESC desc;
    if (!tex || !out_pixels || !width || !height || FAILED(tex->GetLevelDesc(0, &desc)))
        return false;
    const unsigned int bpp = desc.Format == D3DFMT_A8R8G8B8 || desc.Format == D3DFMT_X8R8G8B8 ? 4 : desc.Format == D3DFMT_L8 || desc.Format == D3DFMT_A8 ? 1 : 0;
    if (!bpp || x + width > desc.Width || y + height > desc.Height || out_size < (size_t)width * height * bpp)
        return false;
    IDirect3DSurface9* src = NULL;
    IDirect3DSurface9* sys = NULL;
    if (FAILED(tex->GetSurfaceLevel(0, &src)))
        return false;
    IDirect3DSurface9* lock = src;
    if (desc.Pool == D3DPOOL_DEFAULT)       // Render targets: copied to system memory first.
    {
        if (FAILED(g_GfxData.pDevice->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &sys, NULL))
            || FAILED(g_GfxData.pDevice->GetRenderTargetData(src, sys)))
        {
            if (sys)
                sys->Release();
            src->Release();
            return false;
        }
        lock = sys;
    }
    RECT r = { (LONG)x, (LONG)y, (LONG)(x + width), (LONG)(y + height) };
    D3DLOCKED_RECT locked;
    const bool ok = SUCCEEDED(lock->LockRect(&locked, &r, D3DLOCK_READONLY));
    if (ok)
    {
        const bool rgba = ImP9_IsRgba(tex);
        for (unsigned int row = 0; row < height; ++row)
        {
            unsigned char* dst = (unsigned char*)out_pixels + (size_t)row * width * bpp;
            const unsigned char* row_bits = (const unsigned char*)locked.pBits + (size_t)row * locked.Pitch;
            if (rgba)
                ImP9_CopyPixels(dst, row_bits, width, true);
            else
                memcpy(dst, row_bits, (size_t)width * bpp);
        }
        lock->UnlockRect();
    }
    if (sys)
        sys->Release();
    src->Release();
    return ok;
}

IMPLATFORM_API void ImPlatform_DestroyTexture(ImTextureID texture_id)
{
    if (!texture_id)
        return;

    LPDIRECT3DTEXTURE9 pTexture = (LPDIRECT3DTEXTURE9)texture_id;
    g_P9_RgbaTextures.find_erase_unsorted(pTexture);
    pTexture->Release();
}

IMPLATFORM_API void ImPlatform_PushSampler(ImPlatform_TextureFilter filter, ImPlatform_TextureWrap wrap)
{
    // Encode filter+wrap as a small int passed through UserCallbackData
    uintptr_t encoded = (uintptr_t)(((int)filter << 8) | (int)wrap);
    ImGui::GetWindowDrawList()->AddCallback(
        [](const ImDrawList*, const ImDrawCmd* cmd) {
            if (!g_GfxData.pDevice) return;
            // Save current state
            if (g_SamplerDepth < 8)
            {
                auto& e = g_SamplerStack[g_SamplerDepth++];
                g_GfxData.pDevice->GetSamplerState(0, D3DSAMP_MINFILTER, &e.minF);
                g_GfxData.pDevice->GetSamplerState(0, D3DSAMP_MAGFILTER, &e.magF);
                g_GfxData.pDevice->GetSamplerState(0, D3DSAMP_ADDRESSU,  &e.addrU);
                g_GfxData.pDevice->GetSamplerState(0, D3DSAMP_ADDRESSV,  &e.addrV);
            }
            // Decode and apply new state
            int enc   = (int)(uintptr_t)cmd->UserCallbackData;
            int f     = (enc >> 8) & 0xFF;
            int w     = enc & 0xFF;
            static const DWORD kFilter[2] = { D3DTEXF_POINT, D3DTEXF_LINEAR };
            static const DWORD kAddr[3]   = { D3DTADDRESS_CLAMP, D3DTADDRESS_WRAP, D3DTADDRESS_MIRROR };
            DWORD df = kFilter[f & 1];
            DWORD da = kAddr[w < 3 ? w : 0];
            g_GfxData.pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, df);
            g_GfxData.pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, df);
            g_GfxData.pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
            g_GfxData.pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU,  da);
            g_GfxData.pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV,  da);
        }, (void*)encoded);
}

IMPLATFORM_API void ImPlatform_PopSampler(void)
{
    ImGui::GetWindowDrawList()->AddCallback(
        [](const ImDrawList*, const ImDrawCmd*) {
            if (!g_GfxData.pDevice || g_SamplerDepth <= 0) return;
            const auto& e = g_SamplerStack[--g_SamplerDepth];
            g_GfxData.pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, e.minF);
            g_GfxData.pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, e.magF);
            g_GfxData.pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU,  e.addrU);
            g_GfxData.pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV,  e.addrV);
        }, nullptr);
}

// ============================================================================
// Custom Vertex/Index Buffer Management API - DirectX 9
// ============================================================================

struct ImPlatform_BufferData_DX9
{
    LPDIRECT3DVERTEXBUFFER9 pVertexBuffer;
    LPDIRECT3DINDEXBUFFER9 pIndexBuffer;
    LPDIRECT3DVERTEXDECLARATION9 pVertexDeclaration;
    ImPlatform_VertexBufferDesc vb_desc;
    ImPlatform_IndexBufferDesc ib_desc;
    ImPlatform_VertexAttribute* attributes;
};

static D3DPOOL ImPlatform_GetD3D9Pool(ImPlatform_BufferUsage usage)
{
    return (usage == ImPlatform_BufferUsage_Static) ? D3DPOOL_MANAGED : D3DPOOL_DEFAULT;
}

static DWORD ImPlatform_GetD3D9Usage(ImPlatform_BufferUsage usage)
{
    return (usage == ImPlatform_BufferUsage_Dynamic || usage == ImPlatform_BufferUsage_Stream) ? D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY : 0;
}

static D3DFORMAT ImPlatform_GetD3D9IndexFormat(ImPlatform_IndexFormat format)
{
    return (format == ImPlatform_IndexFormat_UInt16) ? D3DFMT_INDEX16 : D3DFMT_INDEX32;
}

// The vertex buffer's declaration from its attributes (shader model 3 matches usages, not the shader): semantic names
// map to D3DDECLUSAGE_*, attributes sharing one take usage indices 0, 1, ... ImPlatform_BindBuffers sets it.
IMPLATFORM_API bool ImPlatform_CreateVertexInputLayout(ImPlatform_VertexBuffer vertex_buffer, ImPlatform_ShaderProgram program)
{
    ImPlatform_BufferData_DX9* vb = (ImPlatform_BufferData_DX9*)vertex_buffer;
    if (!vb || !program || !vb->vb_desc.attributes || vb->vb_desc.attribute_count == 0 || !g_GfxData.pDevice)
        return false;
    static const BYTE types[] = { D3DDECLTYPE_FLOAT1, D3DDECLTYPE_FLOAT2, D3DDECLTYPE_FLOAT3, D3DDECLTYPE_FLOAT4, D3DDECLTYPE_UBYTE4, D3DDECLTYPE_UBYTE4N };
    static const struct { const char* name; BYTE usage; } usages[] = {
        { "POSITION", D3DDECLUSAGE_POSITION }, { "TEXCOORD", D3DDECLUSAGE_TEXCOORD }, { "COLOR", D3DDECLUSAGE_COLOR },
        { "NORMAL", D3DDECLUSAGE_NORMAL }, { "TANGENT", D3DDECLUSAGE_TANGENT }, { "BINORMAL", D3DDECLUSAGE_BINORMAL },
        { "BLENDWEIGHT", D3DDECLUSAGE_BLENDWEIGHT }, { "BLENDINDICES", D3DDECLUSAGE_BLENDINDICES } };
    ImVector<D3DVERTEXELEMENT9> elements;
    for (unsigned int i = 0; i < vb->vb_desc.attribute_count; i++)
    {
        const ImPlatform_VertexAttribute& attr = vb->vb_desc.attributes[i];
        BYTE usage = D3DDECLUSAGE_TEXCOORD;
        for (const auto& u : usages)
            if (strcmp(attr.semantic_name, u.name) == 0)
                usage = u.usage;
        BYTE index = 0;
        for (unsigned int j = 0; j < i; j++)
            index += strcmp(vb->vb_desc.attributes[j].semantic_name, attr.semantic_name) == 0 ? 1 : 0;
        D3DVERTEXELEMENT9 e = { 0, (WORD)attr.offset, types[(int)attr.format < IM_ARRAYSIZE(types) ? (int)attr.format : 3],
                                D3DDECLMETHOD_DEFAULT, usage, index };
        elements.push_back(e);
    }
    const D3DVERTEXELEMENT9 end = D3DDECL_END();
    elements.push_back(end);
    if (vb->pVertexDeclaration)
        vb->pVertexDeclaration->Release();
    vb->pVertexDeclaration = NULL;
    return SUCCEEDED(g_GfxData.pDevice->CreateVertexDeclaration(elements.Data, &vb->pVertexDeclaration));
}

IMPLATFORM_API ImPlatform_VertexBuffer ImPlatform_CreateVertexBuffer(const void* vertex_data, const ImPlatform_VertexBufferDesc* desc)
{
    if (!desc || !vertex_data)
        return NULL;

    ImPlatform_BufferData_DX9* buffer = new ImPlatform_BufferData_DX9();
    memset(buffer, 0, sizeof(ImPlatform_BufferData_DX9));
    buffer->vb_desc = *desc;

    if (desc->attribute_count > 0 && desc->attributes)
    {
        buffer->attributes = new ImPlatform_VertexAttribute[desc->attribute_count];
        memcpy(buffer->attributes, desc->attributes, sizeof(ImPlatform_VertexAttribute) * desc->attribute_count);
        buffer->vb_desc.attributes = buffer->attributes;
    }

    DWORD usage = ImPlatform_GetD3D9Usage(desc->usage);
    D3DPOOL pool = ImPlatform_GetD3D9Pool(desc->usage);

    HRESULT hr = g_GfxData.pDevice->CreateVertexBuffer(
        desc->vertex_count * desc->vertex_stride,
        usage,
        0,
        pool,
        &buffer->pVertexBuffer,
        NULL);

    if (FAILED(hr))
    {
        delete[] buffer->attributes;
        delete buffer;
        return NULL;
    }

    void* pData;
    if (SUCCEEDED(buffer->pVertexBuffer->Lock(0, 0, &pData, 0)))
    {
        memcpy(pData, vertex_data, desc->vertex_count * desc->vertex_stride);
        buffer->pVertexBuffer->Unlock();
    }

    return (ImPlatform_VertexBuffer)buffer;
}

IMPLATFORM_API bool ImPlatform_UpdateVertexBuffer(ImPlatform_VertexBuffer vertex_buffer, const void* vertex_data, unsigned int vertex_count, unsigned int offset)
{
    if (!vertex_buffer || !vertex_data)
        return false;

    ImPlatform_BufferData_DX9* buffer = (ImPlatform_BufferData_DX9*)vertex_buffer;
    void* pData;
    HRESULT hr = buffer->pVertexBuffer->Lock(0, 0, &pData, D3DLOCK_DISCARD);
    if (FAILED(hr))
        return false;

    size_t byte_offset = offset * buffer->vb_desc.vertex_stride;
    size_t byte_size = vertex_count * buffer->vb_desc.vertex_stride;
    memcpy((char*)pData + byte_offset, vertex_data, byte_size);
    buffer->pVertexBuffer->Unlock();
    return true;
}

IMPLATFORM_API void ImPlatform_DestroyVertexBuffer(ImPlatform_VertexBuffer vertex_buffer)
{
    if (!vertex_buffer)
        return;

    ImPlatform_BufferData_DX9* buffer = (ImPlatform_BufferData_DX9*)vertex_buffer;
    if (buffer->pVertexBuffer) buffer->pVertexBuffer->Release();
    if (buffer->pVertexDeclaration) buffer->pVertexDeclaration->Release();
    delete[] buffer->attributes;
    delete buffer;
}

IMPLATFORM_API ImPlatform_IndexBuffer ImPlatform_CreateIndexBuffer(const void* index_data, const ImPlatform_IndexBufferDesc* desc)
{
    if (!desc || !index_data)
        return NULL;

    ImPlatform_BufferData_DX9* buffer = new ImPlatform_BufferData_DX9();
    memset(buffer, 0, sizeof(ImPlatform_BufferData_DX9));
    buffer->ib_desc = *desc;

    unsigned int index_size = (desc->format == ImPlatform_IndexFormat_UInt16) ? sizeof(uint16_t) : sizeof(uint32_t);
    DWORD usage = ImPlatform_GetD3D9Usage(desc->usage);
    D3DPOOL pool = ImPlatform_GetD3D9Pool(desc->usage);
    D3DFORMAT format = ImPlatform_GetD3D9IndexFormat(desc->format);

    HRESULT hr = g_GfxData.pDevice->CreateIndexBuffer(
        desc->index_count * index_size,
        usage,
        format,
        pool,
        &buffer->pIndexBuffer,
        NULL);

    if (FAILED(hr))
    {
        delete buffer;
        return NULL;
    }

    void* pData;
    if (SUCCEEDED(buffer->pIndexBuffer->Lock(0, 0, &pData, 0)))
    {
        memcpy(pData, index_data, desc->index_count * index_size);
        buffer->pIndexBuffer->Unlock();
    }

    return (ImPlatform_IndexBuffer)buffer;
}

IMPLATFORM_API bool ImPlatform_UpdateIndexBuffer(ImPlatform_IndexBuffer index_buffer, const void* index_data, unsigned int index_count, unsigned int offset)
{
    if (!index_buffer || !index_data)
        return false;

    ImPlatform_BufferData_DX9* buffer = (ImPlatform_BufferData_DX9*)index_buffer;
    unsigned int index_size = (buffer->ib_desc.format == ImPlatform_IndexFormat_UInt16) ? sizeof(uint16_t) : sizeof(uint32_t);

    void* pData;
    HRESULT hr = buffer->pIndexBuffer->Lock(0, 0, &pData, D3DLOCK_DISCARD);
    if (FAILED(hr))
        return false;

    size_t byte_offset = offset * index_size;
    size_t byte_size = index_count * index_size;
    memcpy((char*)pData + byte_offset, index_data, byte_size);
    buffer->pIndexBuffer->Unlock();
    return true;
}

IMPLATFORM_API void ImPlatform_DestroyIndexBuffer(ImPlatform_IndexBuffer index_buffer)
{
    if (!index_buffer)
        return;

    ImPlatform_BufferData_DX9* buffer = (ImPlatform_BufferData_DX9*)index_buffer;
    if (buffer->pIndexBuffer) buffer->pIndexBuffer->Release();
    delete buffer;
}

IMPLATFORM_API void ImPlatform_BindVertexBuffer(ImPlatform_VertexBuffer vertex_buffer)
{
    if (!vertex_buffer)
        return;

    ImPlatform_BufferData_DX9* buffer = (ImPlatform_BufferData_DX9*)vertex_buffer;
    g_GfxData.pDevice->SetStreamSource(0, buffer->pVertexBuffer, 0, buffer->vb_desc.vertex_stride);
    if (buffer->pVertexDeclaration)
        g_GfxData.pDevice->SetVertexDeclaration(buffer->pVertexDeclaration);
}

IMPLATFORM_API void ImPlatform_BindIndexBuffer(ImPlatform_IndexBuffer index_buffer)
{
    if (!index_buffer)
        return;

    ImPlatform_BufferData_DX9* buffer = (ImPlatform_BufferData_DX9*)index_buffer;
    g_GfxData.pDevice->SetIndices(buffer->pIndexBuffer);
}

IMPLATFORM_API void ImPlatform_BindBuffers(ImPlatform_VertexBuffer vertex_buffer, ImPlatform_IndexBuffer index_buffer)
{
    ImPlatform_BindVertexBuffer(vertex_buffer);
    ImPlatform_BindIndexBuffer(index_buffer);
}

IMPLATFORM_API void ImPlatform_DrawIndexed(unsigned int primitive_type, unsigned int index_count, unsigned int start_index)
{
    D3DPRIMITIVETYPE d3d_prim;
    UINT prim_count;

    switch (primitive_type)
    {
    case 0: // Triangles
        d3d_prim = D3DPT_TRIANGLELIST;
        prim_count = index_count / 3;
        break;
    case 1: // Lines
        d3d_prim = D3DPT_LINELIST;
        prim_count = index_count / 2;
        break;
    case 2: // Points
        d3d_prim = D3DPT_POINTLIST;
        prim_count = index_count;
        break;
    default:
        d3d_prim = D3DPT_TRIANGLELIST;
        prim_count = index_count / 3;
        break;
    }

    g_GfxData.pDevice->DrawIndexedPrimitive(d3d_prim, 0, 0, index_count, start_index, prim_count);
}

// ============================================================================
// Custom Shader System API - DirectX 9
// ============================================================================
// The same HLSL as the other Direct3D backends, compiled for vs_3_0 / ps_3_0. Shader model 3 has no constant buffers:
// the compiler gives each used cbuffer member its own float4 registers (listed in the bytecode's constant table), so
// the backend uploads b0 (the projection Dear ImGui's DX9 backend set as D3DTS_PROJECTION) and b1 (the program's
// parameters) register by register, at the member offsets a shader model 4 compile of the same source reflects.
// Textures: the compiler numbers combined samplers from s1, so the bytecode is renumbered to sampler N for texture tN
// (the draw's texture, t0, is on Dear ImGui's stage 0).
// shortcut: no cache_key / compile_flags shader cache on DX9; add it with a DX9 application that needs it.

struct ImP9_Constant
{
    unsigned int reg, count;    // First float4 register and register count
    unsigned int cb;            // Constant buffer slot: 0 projection, 1 parameters
    unsigned int offset, size;  // Byte range in that constant buffer
    bool integer;               // int / uint / bool member: shader model 3 reads floats, the values are converted
};

struct ImPlatform_ShaderData_DX9
{
    IUnknown* shader;           // IDirect3DVertexShader9 or IDirect3DPixelShader9
    ImPlatform_ShaderStage stage;
    ImVector<ImP9_Constant> constants;
};

struct ImPlatform_ShaderProgramData_DX9
{
    ImPlatform_ShaderData_DX9* vs;
    ImPlatform_ShaderData_DX9* ps;
    ImVector<unsigned char> params; // b1
    IDirect3DBaseTexture9* textures[9]; // ImPlatform_SetShaderTexture: t0 for direct draws, t1..t8 for every draw (referenced)
};

// b1 of the direct draws while pushed (ImPlatform_PushShaderConstants), a stack.
struct ImP9_PushedConstants
{
    ImVector<unsigned char> data;
    ImP9_PushedConstants* prev;
};
static ImP9_PushedConstants* g_P9_Pushed = NULL;
static ImPlatform_ShaderProgramData_DX9* g_P9_Bound = NULL;     // Program bound by the last draw callback or direct draw
static D3DMATRIX g_P9_BoundProjection = {};

// The constant table of shader model 3 bytecode (D3DXSHADER_CONSTANTTABLE / D3DXSHADER_CONSTANTINFO, d3dx9shader.h).
struct ImP9_CTab { DWORD Size, Creator, Version, Constants, ConstantInfo, Flags, Target; };
struct ImP9_CInfo { DWORD Name; WORD RegisterSet, RegisterIndex, RegisterCount, Reserved; DWORD TypeInfo, DefaultValue; };

static const ImP9_CTab* ImP9_FindCTab(const DWORD* code, size_t words, size_t* ctab_bytes)
{
    // Comments follow the version token: 0xFFFE with the length in the high word.
    for (size_t i = 1; i < words && (code[i] & 0xFFFF) == 0xFFFE; i += 1 + ((code[i] >> 16) & 0x7FFF))
        if (i + 1 < words && code[i + 1] == MAKEFOURCC('C', 'T', 'A', 'B'))
        {
            *ctab_bytes = (((code[i] >> 16) & 0x7FFF) - 1) * 4;
            return (const ImP9_CTab*)&code[i + 2];
        }
    return NULL;
}

// Sampler register s -> remap[s] in every instruction (dcl's usage token and def's literals are not registers).
static void ImP9_RemapSamplers(DWORD* code, size_t words, const unsigned char remap[16])
{
    for (size_t i = 1; i < words && code[i] != 0x0000FFFF; )
    {
        const DWORD op = code[i] & 0xFFFF;
        const size_t len = op == 0xFFFE ? ((code[i] >> 16) & 0x7FFF) : ((code[i] >> 24) & 0xF);
        if (op != 0xFFFE && op != 0x51 && op != 0x52 && op != 0x53)      // comment, def, defi, defb
            for (size_t p = op == 0x1F ? 2 : 1; p <= len && i + p < words; p++)
            {
                DWORD& t = code[i + p];
                const DWORD type = ((t >> 28) & 7) | ((t >> 8) & 0x18);
                if (type == 10 && (t & 0x7FF) < 16)        // D3DSPR_SAMPLER
                    t = (t & ~0x7FFu) | remap[t & 0x7FF];
            }
        i += 1 + len;
    }
}

IMPLATFORM_API ImPlatform_Shader ImPlatform_CreateShader(const ImPlatform_ShaderDesc* desc)
{
    if (!desc || !g_GfxData.pDevice || (desc->stage != ImPlatform_ShaderStage_Vertex && desc->stage != ImPlatform_ShaderStage_Fragment))
        return NULL;
    const bool vertex = desc->stage == ImPlatform_ShaderStage_Vertex;
    ID3DBlob* blob = NULL;
    ID3D11ShaderReflection* reflect = NULL;     // Of the shader model 4 compile: cbuffer member offsets, texture slots
    ImVector<DWORD> code;
    if (desc->format == ImPlatform_ShaderFormat_HLSL && desc->source_code)
    {
        const char* entry = desc->entry_point ? desc->entry_point : "main";
        const size_t len = strlen(desc->source_code);
        ID3DBlob* errors = NULL;
        if (FAILED(D3DCompile(desc->source_code, len, NULL, NULL, NULL, entry, vertex ? "vs_3_0" : "ps_3_0",
                              D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &errors)))
        {
            fprintf(stderr, "[ImPlatform] DX9: shader compilation failed: %s\n", errors ? (const char*)errors->GetBufferPointer() : "");
            if (errors) errors->Release();
            return NULL;
        }
        ID3DBlob* sm4 = NULL;
        if (SUCCEEDED(D3DCompile(desc->source_code, len, NULL, NULL, NULL, entry, vertex ? "vs_4_0" : "ps_4_0", 0, 0, &sm4, NULL)))
        {
            D3DReflect(sm4->GetBufferPointer(), sm4->GetBufferSize(), __uuidof(ID3D11ShaderReflection), (void**)&reflect);
            sm4->Release();
        }
        code.resize((int)(blob->GetBufferSize() / 4));
        memcpy(code.Data, blob->GetBufferPointer(), blob->GetBufferSize());
        blob->Release();
    }
    else if (desc->bytecode && desc->bytecode_size >= 4)
    {
        // shortcut: precompiled bytecode has no source to reflect; its constants are read as b1 at 16 bytes per
        // register and its samplers stay as compiled.
        code.resize((int)(desc->bytecode_size / 4));
        memcpy(code.Data, desc->bytecode, desc->bytecode_size);
    }
    else
    {
        fprintf(stderr, "[ImPlatform] DX9: custom shaders take HLSL source or shader model 3 bytecode\n");
        return NULL;
    }

    ImPlatform_ShaderData_DX9* sd = IM_NEW(ImPlatform_ShaderData_DX9)();
    sd->stage = desc->stage;
    size_t ctab_bytes = 0;
    const ImP9_CTab* ctab = ImP9_FindCTab(code.Data, (size_t)code.Size, &ctab_bytes);
    unsigned char remap[16];
    for (int s = 0; s < 16; s++)
        remap[s] = (unsigned char)s;
    for (DWORD c = 0; ctab && c < ctab->Constants && ctab->ConstantInfo + (c + 1) * sizeof(ImP9_CInfo) <= ctab_bytes; c++)
    {
        const ImP9_CInfo& info = ((const ImP9_CInfo*)((const char*)ctab + ctab->ConstantInfo))[c];
        const char* name = info.Name < ctab_bytes ? (const char*)ctab + info.Name : "";
        if (info.RegisterSet == 3 && info.RegisterIndex < 16)      // Sampler "<sampler>+<texture>": to its t slot
        {
            const char* texture = strchr(name, '+');
            D3D11_SHADER_INPUT_BIND_DESC bind;
            if (reflect && texture && SUCCEEDED(reflect->GetResourceBindingDescByName(texture + 1, &bind)) && bind.BindPoint < 16)
                remap[info.RegisterIndex] = (unsigned char)bind.BindPoint;
            continue;
        }
        if (info.RegisterSet != 2)                                  // D3DXRS_FLOAT4 only
            continue;
        // D3DXSHADER_TYPEINFO: WORD Class, Type (1 bool, 2 int, 3 float), ...
        const WORD type = info.TypeInfo + 4 <= ctab_bytes ? *(const WORD*)((const char*)ctab + info.TypeInfo + 2) : 3;
        ImP9_Constant k = { info.RegisterIndex, info.RegisterCount, 1, info.RegisterIndex * 16u, info.RegisterCount * 16u, type == 1 || type == 2 };
        if (reflect)
        {
            ID3D11ShaderReflectionVariable* var = reflect->GetVariableByName(name);
            D3D11_SHADER_VARIABLE_DESC vd;
            D3D11_SHADER_BUFFER_DESC bd;
            D3D11_SHADER_INPUT_BIND_DESC bind;
            if (!var || FAILED(var->GetDesc(&vd)) || FAILED(var->GetBuffer()->GetDesc(&bd))
                || FAILED(reflect->GetResourceBindingDescByName(bd.Name, &bind)) || bind.BindPoint > 1)
                continue;
            k.cb = bind.BindPoint;
            k.offset = vd.StartOffset;
            k.size = vd.Size;
        }
        sd->constants.push_back(k);
    }
    if (reflect)
        reflect->Release();
    ImP9_RemapSamplers(code.Data, (size_t)code.Size, remap);

    const HRESULT hr = vertex ? g_GfxData.pDevice->CreateVertexShader(code.Data, (IDirect3DVertexShader9**)&sd->shader)
                              : g_GfxData.pDevice->CreatePixelShader(code.Data, (IDirect3DPixelShader9**)&sd->shader);
    if (FAILED(hr))
    {
        fprintf(stderr, "[ImPlatform] DX9: CreateVertexShader / CreatePixelShader failed (0x%08lx)\n", (unsigned long)hr);
        IM_DELETE(sd);
        return NULL;
    }
    return sd;
}

IMPLATFORM_API void ImPlatform_DestroyShader(ImPlatform_Shader shader)
{
    ImPlatform_ShaderData_DX9* sd = (ImPlatform_ShaderData_DX9*)shader;
    if (!sd)
        return;
    if (sd->shader)
        sd->shader->Release();
    IM_DELETE(sd);
}

IMPLATFORM_API ImPlatform_ShaderProgram ImPlatform_CreateShaderProgram(ImPlatform_Shader vertex_shader, ImPlatform_Shader fragment_shader)
{
    ImPlatform_ShaderData_DX9* vs = (ImPlatform_ShaderData_DX9*)vertex_shader;
    ImPlatform_ShaderData_DX9* ps = (ImPlatform_ShaderData_DX9*)fragment_shader;
    if (!vs || !ps || vs->stage != ImPlatform_ShaderStage_Vertex || ps->stage != ImPlatform_ShaderStage_Fragment)
        return NULL;
    // The program keeps its own copy of each stage: the application may destroy the shaders.
    ImPlatform_ShaderProgramData_DX9* program = IM_NEW(ImPlatform_ShaderProgramData_DX9)();
    program->vs = IM_NEW(ImPlatform_ShaderData_DX9)(*vs);
    program->ps = IM_NEW(ImPlatform_ShaderData_DX9)(*ps);
    program->vs->shader->AddRef();
    program->ps->shader->AddRef();
    return program;
}

IMPLATFORM_API void ImPlatform_DestroyShaderProgram(ImPlatform_ShaderProgram program)
{
    ImPlatform_ShaderProgramData_DX9* pd = (ImPlatform_ShaderProgramData_DX9*)program;
    if (!pd)
        return;
    ImPlatform_DestroyShader(pd->vs);
    ImPlatform_DestroyShader(pd->ps);
    for (IDirect3DBaseTexture9* tex : pd->textures)
        if (tex)
            tex->Release();
    if (g_P9_Bound == pd)
        g_P9_Bound = NULL;
    IM_DELETE(pd);
}

// Uploads each constant of a stage, register by register, from b0 (the projection) or b1 (the parameters).
static void ImP9_UploadConstants(const ImPlatform_ShaderData_DX9* sd, const D3DMATRIX& projection, const ImVector<unsigned char>& params)
{
    for (const ImP9_Constant& k : sd->constants)
        for (unsigned int r = 0; r < k.count; r++)
        {
            float value[4] = {};
            const unsigned char* src = k.cb == 0 ? (const unsigned char*)&projection : params.Data;
            const unsigned int src_size = k.cb == 0 ? (unsigned int)sizeof(projection) : (unsigned int)params.Size;
            const unsigned int at = k.offset + r * 16, end = k.offset + k.size;
            if (at < end && at < src_size)
            {
                unsigned int n = end - at < 16u ? end - at : 16u;
                n = src_size - at < n ? src_size - at : n;
                memcpy(value, src + at, n);
                if (k.integer)
                    for (int c = 0; c < 4; c++)
                    {
                        int bits;
                        memcpy(&bits, &value[c], sizeof(bits));
                        value[c] = (float)bits;
                    }
            }
            if (sd->stage == ImPlatform_ShaderStage_Vertex)
                g_GfxData.pDevice->SetVertexShaderConstantF(k.reg + r, value, 1);
            else
                g_GfxData.pDevice->SetPixelShaderConstantF(k.reg + r, value, 1);
        }
}

// Binds a program with the projection of its target: shaders, b0 and b1 (the pushed constants while pushed) in both
// stages, and the textures t1..t8 (t0 too for direct draws) with Dear ImGui's sampler state (linear, clamp).
static void ImP9_BindProgram(ImPlatform_ShaderProgramData_DX9* pd, const D3DMATRIX& projection, bool direct)
{
    LPDIRECT3DDEVICE9 dev = g_GfxData.pDevice;
    dev->SetVertexShader((IDirect3DVertexShader9*)pd->vs->shader);
    dev->SetPixelShader((IDirect3DPixelShader9*)pd->ps->shader);
    const ImVector<unsigned char>& params = g_P9_Pushed ? g_P9_Pushed->data : pd->params;
    ImP9_UploadConstants(pd->vs, projection, params);
    ImP9_UploadConstants(pd->ps, projection, params);
    for (DWORD slot = direct ? 0 : 1; slot < 9; slot++)
        if (pd->textures[slot])
        {
            dev->SetTexture(slot, pd->textures[slot]);
            dev->SetSamplerState(slot, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
            dev->SetSamplerState(slot, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
            dev->SetSamplerState(slot, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            dev->SetSamplerState(slot, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
            dev->SetSamplerState(slot, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);
        }
    g_P9_Bound = pd;
    g_P9_BoundProjection = projection;
}

IMPLATFORM_API void ImPlatform_UseShaderProgram(ImPlatform_ShaderProgram program)
{
    ImPlatform_ShaderProgramData_DX9* pd = (ImPlatform_ShaderProgramData_DX9*)program;
    if (!pd || !g_GfxData.pDevice)
        return;
    D3DMATRIX projection;
    g_GfxData.pDevice->GetTransform(D3DTS_PROJECTION, &projection);    // Set by Dear ImGui's DX9 backend for the target
    ImP9_BindProgram(pd, projection, false);
}

IMPLATFORM_API bool ImPlatform_SetShaderUniform(ImPlatform_ShaderProgram program, const char* /*name*/, const void* data, unsigned int size)
{
    ImPlatform_ShaderProgramData_DX9* pd = (ImPlatform_ShaderProgramData_DX9*)program;
    if (!pd || !data || size == 0)
        return false;
    pd->params.resize((int)size);
    memcpy(pd->params.Data, data, size);
    return true;
}

// Slot N is sampler N (CreateShader renumbers the samplers so tN reads sampler N). Kept by the program; applied at once
// when the program is bound (inside a draw callback).
IMPLATFORM_API bool ImPlatform_SetShaderTexture(ImPlatform_ShaderProgram program, const char* /*name*/, unsigned int slot, ImTextureID texture)
{
    ImPlatform_ShaderProgramData_DX9* pd = (ImPlatform_ShaderProgramData_DX9*)program;
    if (!pd || slot > 8)
        return false;
    IDirect3DBaseTexture9* tex = (IDirect3DBaseTexture9*)texture; // 2D or volume
    if (tex)
        tex->AddRef();
    if (pd->textures[slot])
        pd->textures[slot]->Release();
    pd->textures[slot] = tex;
    if (pd == g_P9_Bound && g_GfxData.pDevice)
        g_GfxData.pDevice->SetTexture(slot, tex);
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

// ============================================================================
// Custom Shader DrawList Integration
// ============================================================================

// ImDrawCallback handler to activate a custom shader; Dear ImGui's DrawCallback_ResetRenderState unbinds it.
static void ImPlatform_SetCustomShader(const ImDrawList* /*parent_list*/, const ImDrawCmd* cmd)
{
    ImPlatform_UseShaderProgram((ImPlatform_ShaderProgram)cmd->UserCallbackData);
}

// Activate a custom shader immediately (for use inside draw callbacks).
IMPLATFORM_API void ImPlatform_BeginCustomShader_Render(ImPlatform_ShaderProgram program)
{
    ImPlatform_UseShaderProgram(program);
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

    draw->AddCallback([](const ImDrawList*, const ImDrawCmd*) { g_P9_Bound = NULL; }, NULL);      // Unbound by the reset
    draw->AddCallback(ImGui::GetPlatformIO().DrawCallback_ResetRenderState, NULL);
}

// b1 of the direct draws (and of the bound program) until popped.
IMPLATFORM_API void* ImPlatform_PushShaderConstants(const void* data, unsigned int size)
{
    if (!data || size == 0)
        return nullptr;
    ImP9_PushedConstants* pushed = IM_NEW(ImP9_PushedConstants)();
    pushed->data.resize((int)size);
    memcpy(pushed->data.Data, data, size);
    pushed->prev = g_P9_Pushed;
    g_P9_Pushed = pushed;
    if (g_P9_Bound)
    {
        ImP9_UploadConstants(g_P9_Bound->vs, g_P9_BoundProjection, pushed->data);
        ImP9_UploadConstants(g_P9_Bound->ps, g_P9_BoundProjection, pushed->data);
    }
    return pushed;
}

IMPLATFORM_API void ImPlatform_PopShaderConstants(void* handle)
{
    ImP9_PushedConstants* pushed = (ImP9_PushedConstants*)handle;
    if (!pushed || pushed != g_P9_Pushed)       // Pops are in push order
        return;
    g_P9_Pushed = pushed->prev;
    IM_DELETE(pushed);
}

// Render-target stack: the target, viewport and scissor of each level. A pushed target is unbound from the texture
// stages first (a texture cannot be read while it is the target).
struct ImP9_SavedTarget
{
    IDirect3DSurface9* surface;
    D3DVIEWPORT9 viewport;
    RECT scissor;
};
static ImP9_SavedTarget g_P9_Targets[8];
static int g_P9_TargetDepth = 0;

IMPLATFORM_API bool ImPlatform_PushRenderTarget(ImTextureID texture, bool clear)
{
    ImPlatform_RTTracking_DX9* entry = ImPlatform_FindRT_DX9(texture);
    if (!entry || !g_GfxData.pDevice || g_P9_TargetDepth >= IM_ARRAYSIZE(g_P9_Targets))
        return false;
    LPDIRECT3DDEVICE9 dev = g_GfxData.pDevice;
    ImP9_SavedTarget& saved = g_P9_Targets[g_P9_TargetDepth++];
    dev->GetRenderTarget(0, &saved.surface);
    dev->GetViewport(&saved.viewport);
    dev->GetScissorRect(&saved.scissor);
    for (DWORD stage = 0; stage < 9; stage++)
        dev->SetTexture(stage, NULL);
    dev->SetRenderTarget(0, entry->pSurface);       // Also sets the viewport to the whole surface
    D3DSURFACE_DESC desc;
    entry->pSurface->GetDesc(&desc);
    const RECT scissor = { 0, 0, (LONG)desc.Width, (LONG)desc.Height };
    dev->SetScissorRect(&scissor);
    if (clear)
        dev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
    return true;
}

IMPLATFORM_API void ImPlatform_PopRenderTarget(void)
{
    if (!g_GfxData.pDevice || g_P9_TargetDepth <= 0)
        return;
    ImP9_SavedTarget& saved = g_P9_Targets[--g_P9_TargetDepth];
    g_GfxData.pDevice->SetRenderTarget(0, saved.surface);
    g_GfxData.pDevice->SetViewport(&saved.viewport);
    g_GfxData.pDevice->SetScissorRect(&saved.scissor);
    if (saved.surface)
        saved.surface->Release();
    saved.surface = NULL;
}

IMPLATFORM_API bool ImPlatform_BeginRenderToTexture(ImTextureID texture) { return ImPlatform_PushRenderTarget(texture, true); }
IMPLATFORM_API void ImPlatform_EndRenderToTexture(void)                  { ImPlatform_PopRenderTarget(); }

IMPLATFORM_API void ImPlatform_GetCurrentTargetSize(unsigned int* width, unsigned int* height)
{
    D3DSURFACE_DESC desc = {};
    IDirect3DSurface9* surface = NULL;
    if (g_GfxData.pDevice && SUCCEEDED(g_GfxData.pDevice->GetRenderTarget(0, &surface)) && surface)
    {
        surface->GetDesc(&desc);
        surface->Release();
    }
    if (width)  *width  = desc.Width;
    if (height) *height = desc.Height;
}

// StretchRect from the current target into a render texture (ImPlatform_CreateRenderTexture), clipped to both.
IMPLATFORM_API bool ImPlatform_CopyCurrentTarget(ImTextureID dst, int src_x, int src_y, int w, int h, int dst_x, int dst_y)
{
    ImPlatform_RTTracking_DX9* entry = ImPlatform_FindRT_DX9(dst);
    IDirect3DSurface9* src = NULL;
    if (!entry || !g_GfxData.pDevice || FAILED(g_GfxData.pDevice->GetRenderTarget(0, &src)) || !src)
        return false;
    D3DSURFACE_DESC sd, dd;
    src->GetDesc(&sd);
    entry->pSurface->GetDesc(&dd);
    if (src_x < 0) { w += src_x; dst_x -= src_x; src_x = 0; }
    if (src_y < 0) { h += src_y; dst_y -= src_y; src_y = 0; }
    if (dst_x < 0) { w += dst_x; src_x -= dst_x; dst_x = 0; }
    if (dst_y < 0) { h += dst_y; src_y -= dst_y; dst_y = 0; }
    const int sw = (int)sd.Width - src_x, dw = (int)dd.Width - dst_x, sh = (int)sd.Height - src_y, dh = (int)dd.Height - dst_y;
    w = w < sw ? (w < dw ? w : dw) : (sw < dw ? sw : dw);
    h = h < sh ? (h < dh ? h : dh) : (sh < dh ? sh : dh);
    bool copied = false;
    if (w > 0 && h > 0)
    {
        const RECT from = { src_x, src_y, src_x + w, src_y + h };
        const RECT to = { dst_x, dst_y, dst_x + w, dst_y + h };
        copied = SUCCEEDED(g_GfxData.pDevice->StretchRect(src, &from, entry->pSurface, &to, D3DTEXF_NONE));
    }
    src->Release();
    return copied;
}

// A quad in the current target's pixels: Dear ImGui's DX9 vertex format and projection (half-pixel offset), the
// program's t0 as the texture.
IMPLATFORM_API void ImPlatform_DrawQuadNow(ImPlatform_ShaderProgram program, ImVec4 dst_rect, ImVec4 uv_rect)
{
    ImPlatform_ShaderProgramData_DX9* pd = (ImPlatform_ShaderProgramData_DX9*)program;
    unsigned int tw = 0, th = 0;
    ImPlatform_GetCurrentTargetSize(&tw, &th);
    if (!pd || !g_GfxData.pDevice || tw == 0 || th == 0)
        return;
    const float L = 0.5f, R = (float)tw + 0.5f, T = 0.5f, B = (float)th + 0.5f;
    const D3DMATRIX projection = { { {
        2.0f / (R - L),    0.0f,              0.0f, 0.0f,
        0.0f,              2.0f / (T - B),    0.0f, 0.0f,
        0.0f,              0.0f,              0.5f, 0.0f,
        (L + R) / (L - R), (T + B) / (B - T), 0.5f, 1.0f } } };
    ImP9_BindProgram(pd, projection, true);
    struct Vertex { float pos[3]; D3DCOLOR col; float uv[2]; };
    const Vertex quad[4] = {
        { { dst_rect.x, dst_rect.y, 0.0f }, 0xFFFFFFFF, { uv_rect.x, uv_rect.y } },
        { { dst_rect.z, dst_rect.y, 0.0f }, 0xFFFFFFFF, { uv_rect.z, uv_rect.y } },
        { { dst_rect.z, dst_rect.w, 0.0f }, 0xFFFFFFFF, { uv_rect.z, uv_rect.w } },
        { { dst_rect.x, dst_rect.w, 0.0f }, 0xFFFFFFFF, { uv_rect.x, uv_rect.w } },
    };
    g_GfxData.pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    g_GfxData.pDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, quad, sizeof(Vertex));
}

#endif // IM_GFX_DIRECTX9
