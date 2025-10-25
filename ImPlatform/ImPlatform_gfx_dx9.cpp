// dear imgui: Graphics API Abstraction - DirectX 9 Backend
// This handles D3D9 device creation, rendering, and presentation

#include "ImPlatform_Internal.h"

#ifdef IM_GFX_DIRECTX9

#include "../imgui/backends/imgui_impl_dx9.h"
// Note: D3DX is deprecated and not available in modern Windows SDK
// For shader compilation, would need to use D3DCompile from d3dcompiler.lib
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// Link with d3d9.lib
#pragma comment(lib, "d3d9")

// Global state
static ImPlatform_GfxData_DX9 g_GfxData = { 0 };

// Uniform block API state
static ImPlatform_ShaderProgram g_CurrentUniformBlockProgram = nullptr;
static void* g_UniformBlockData = nullptr;
static size_t g_UniformBlockSize = 0;

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
// Note: DirectX 9 shader support is limited because D3DX is deprecated
// For full shader support, consider using D3DCompile from d3dcompiler.lib
// or pre-compiled shader bytecode

IMPLATFORM_API ImPlatform_Shader ImPlatform_CreateShader(const ImPlatform_ShaderDesc* desc)
{
    // Stub: D3DX is not available in modern Windows SDK
    // Would need to use D3DCompile or pre-compiled bytecode
    return NULL;
}

IMPLATFORM_API void ImPlatform_DestroyShader(ImPlatform_Shader shader)
{
    // Stub
}

IMPLATFORM_API ImPlatform_ShaderProgram ImPlatform_CreateShaderProgram(ImPlatform_Shader vertex_shader, ImPlatform_Shader fragment_shader)
{
    // Stub
    return NULL;
}

IMPLATFORM_API void ImPlatform_DestroyShaderProgram(ImPlatform_ShaderProgram program)
{
    // Stub
}

IMPLATFORM_API void ImPlatform_UseShaderProgram(ImPlatform_ShaderProgram program)
{
    // Stub
}

IMPLATFORM_API bool ImPlatform_SetShaderUniform(ImPlatform_ShaderProgram program, const char* name, const void* data, unsigned int size)
{
    return false;
}

IMPLATFORM_API bool ImPlatform_SetShaderTexture(ImPlatform_ShaderProgram program, const char* name, unsigned int slot, ImTextureID texture)
{
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

IMPLATFORM_API bool ImPlatform_SetUniform(const char* name, const void* data, unsigned int size)
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
        ImPlatform_SetShaderUniform(program, "pixelBuffer", g_UniformBlockData, g_UniformBlockSize);

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
    // DX9 custom shaders not supported (D3DX deprecated)
    // This is a stub for API consistency
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

#endif // IM_GFX_DIRECTX9
