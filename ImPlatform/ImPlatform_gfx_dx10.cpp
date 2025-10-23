// dear imgui: Graphics API Abstraction - DirectX 10 Backend
// This handles D3D10 device creation, rendering, and presentation

#include "ImPlatform_Internal.h"

#ifdef IM_GFX_DIRECTX10

#include "../imgui/backends/imgui_impl_dx10.h"
#include <d3dcompiler.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

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

// ============================================================================
// Texture Creation API - DirectX 10 Implementation
// ============================================================================

// Helper to get D3D10 format from ImPlatform format (same as DX11)
static DXGI_FORMAT ImPlatform_GetD3D10Format(ImPlatform_PixelFormat format, int* out_bytes_per_pixel)
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
    DXGI_FORMAT format = ImPlatform_GetD3D10Format(desc->format, &bytes_per_pixel);

    // Create texture
    D3D10_TEXTURE2D_DESC tex_desc;
    ZeroMemory(&tex_desc, sizeof(tex_desc));
    tex_desc.Width = desc->width;
    tex_desc.Height = desc->height;
    tex_desc.MipLevels = 1;
    tex_desc.ArraySize = 1;
    tex_desc.Format = format;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.Usage = D3D10_USAGE_DEFAULT;
    tex_desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    tex_desc.CPUAccessFlags = 0;

    D3D10_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = pixel_data;
    subResource.SysMemPitch = desc->width * bytes_per_pixel;
    subResource.SysMemSlicePitch = 0;

    ID3D10Texture2D* pTexture = NULL;
    HRESULT hr = g_GfxData.pDevice->CreateTexture2D(&tex_desc, &subResource, &pTexture);
    if (FAILED(hr) || !pTexture)
        return NULL;

    // Create shader resource view
    D3D10_SHADER_RESOURCE_VIEW_DESC srv_desc;
    ZeroMemory(&srv_desc, sizeof(srv_desc));
    srv_desc.Format = format;
    srv_desc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = tex_desc.MipLevels;
    srv_desc.Texture2D.MostDetailedMip = 0;

    ID3D10ShaderResourceView* pSRV = NULL;
    hr = g_GfxData.pDevice->CreateShaderResourceView(pTexture, &srv_desc, &pSRV);
    pTexture->Release();

    if (FAILED(hr) || !pSRV)
        return NULL;

    return (ImTextureID)pSRV;
}

IMPLATFORM_API bool ImPlatform_UpdateTexture(ImTextureID texture_id, const void* pixel_data,
                                              unsigned int x, unsigned int y,
                                              unsigned int width, unsigned int height)
{
    if (!texture_id || !pixel_data || !g_GfxData.pDevice)
        return false;

    ID3D10ShaderResourceView* pSRV = (ID3D10ShaderResourceView*)texture_id;

    // Get the underlying texture
    ID3D10Resource* pResource = NULL;
    pSRV->GetResource(&pResource);
    if (!pResource)
        return false;

    ID3D10Texture2D* pTexture = NULL;
    HRESULT hr = pResource->QueryInterface(__uuidof(ID3D10Texture2D), (void**)&pTexture);
    pResource->Release();

    if (FAILED(hr) || !pTexture)
        return false;

    // Get texture description to determine format
    D3D10_TEXTURE2D_DESC desc;
    pTexture->GetDesc(&desc);

    // Calculate bytes per pixel from format
    int bytes_per_pixel = 4;
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
    D3D10_BOX box;
    box.left = x;
    box.right = x + width;
    box.top = y;
    box.bottom = y + height;
    box.front = 0;
    box.back = 1;

    g_GfxData.pDevice->UpdateSubresource(pTexture, 0, &box, pixel_data, width * bytes_per_pixel, 0);

    pTexture->Release();
    return true;
}

IMPLATFORM_API void ImPlatform_DestroyTexture(ImTextureID texture_id)
{
    if (!texture_id)
        return;

    ID3D10ShaderResourceView* pSRV = (ID3D10ShaderResourceView*)texture_id;
    pSRV->Release();
}

// ============================================================================
// Custom Vertex/Index Buffer Management API - DirectX 10
// ============================================================================

struct ImPlatform_BufferData_DX10
{
    ID3D10Buffer* pVertexBuffer;
    ID3D10Buffer* pIndexBuffer;
    ID3D10InputLayout* pInputLayout;
    ImPlatform_VertexBufferDesc vb_desc;
    ImPlatform_IndexBufferDesc ib_desc;
    ImPlatform_VertexAttribute* attributes;
};

static D3D10_USAGE ImPlatform_GetD3D10Usage(ImPlatform_BufferUsage usage)
{
    switch (usage)
    {
    case ImPlatform_BufferUsage_Static:  return D3D10_USAGE_DEFAULT;
    case ImPlatform_BufferUsage_Dynamic: return D3D10_USAGE_DYNAMIC;
    case ImPlatform_BufferUsage_Stream:  return D3D10_USAGE_DYNAMIC;
    default: return D3D10_USAGE_DEFAULT;
    }
}

static UINT ImPlatform_GetD3D10CPUAccessFlags(ImPlatform_BufferUsage usage)
{
    switch (usage)
    {
    case ImPlatform_BufferUsage_Static:  return 0;
    case ImPlatform_BufferUsage_Dynamic: return D3D10_CPU_ACCESS_WRITE;
    case ImPlatform_BufferUsage_Stream:  return D3D10_CPU_ACCESS_WRITE;
    default: return 0;
    }
}

static DXGI_FORMAT ImPlatform_GetIndexFormat_DX10(ImPlatform_IndexFormat format)
{
    return (format == ImPlatform_IndexFormat_UInt16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
}

IMPLATFORM_API ImPlatform_VertexBuffer ImPlatform_CreateVertexBuffer(const void* vertex_data, const ImPlatform_VertexBufferDesc* desc)
{
    if (!desc || !vertex_data)
        return NULL;

    ImPlatform_BufferData_DX10* buffer = new ImPlatform_BufferData_DX10();
    memset(buffer, 0, sizeof(ImPlatform_BufferData_DX10));
    buffer->vb_desc = *desc;

    if (desc->attribute_count > 0 && desc->attributes)
    {
        buffer->attributes = new ImPlatform_VertexAttribute[desc->attribute_count];
        memcpy(buffer->attributes, desc->attributes, sizeof(ImPlatform_VertexAttribute) * desc->attribute_count);
        buffer->vb_desc.attributes = buffer->attributes;
    }

    D3D10_BUFFER_DESC bd;
    memset(&bd, 0, sizeof(bd));
    bd.Usage = ImPlatform_GetD3D10Usage(desc->usage);
    bd.ByteWidth = desc->vertex_count * desc->vertex_stride;
    bd.BindFlags = D3D10_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = ImPlatform_GetD3D10CPUAccessFlags(desc->usage);

    D3D10_SUBRESOURCE_DATA initData;
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

    ImPlatform_BufferData_DX10* buffer = (ImPlatform_BufferData_DX10*)vertex_buffer;
    void* mapped;
    HRESULT hr = buffer->pVertexBuffer->Map(D3D10_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr))
        return false;

    size_t byte_offset = offset * buffer->vb_desc.vertex_stride;
    size_t byte_size = vertex_count * buffer->vb_desc.vertex_stride;
    memcpy((char*)mapped + byte_offset, vertex_data, byte_size);
    buffer->pVertexBuffer->Unmap();
    return true;
}

IMPLATFORM_API void ImPlatform_DestroyVertexBuffer(ImPlatform_VertexBuffer vertex_buffer)
{
    if (!vertex_buffer)
        return;

    ImPlatform_BufferData_DX10* buffer = (ImPlatform_BufferData_DX10*)vertex_buffer;
    if (buffer->pVertexBuffer) buffer->pVertexBuffer->Release();
    if (buffer->pInputLayout) buffer->pInputLayout->Release();
    delete[] buffer->attributes;
    delete buffer;
}

IMPLATFORM_API ImPlatform_IndexBuffer ImPlatform_CreateIndexBuffer(const void* index_data, const ImPlatform_IndexBufferDesc* desc)
{
    if (!desc || !index_data)
        return NULL;

    ImPlatform_BufferData_DX10* buffer = new ImPlatform_BufferData_DX10();
    memset(buffer, 0, sizeof(ImPlatform_BufferData_DX10));
    buffer->ib_desc = *desc;

    unsigned int index_size = (desc->format == ImPlatform_IndexFormat_UInt16) ? sizeof(uint16_t) : sizeof(uint32_t);

    D3D10_BUFFER_DESC bd;
    memset(&bd, 0, sizeof(bd));
    bd.Usage = ImPlatform_GetD3D10Usage(desc->usage);
    bd.ByteWidth = desc->index_count * index_size;
    bd.BindFlags = D3D10_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = ImPlatform_GetD3D10CPUAccessFlags(desc->usage);

    D3D10_SUBRESOURCE_DATA initData;
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

    ImPlatform_BufferData_DX10* buffer = (ImPlatform_BufferData_DX10*)index_buffer;
    unsigned int index_size = (buffer->ib_desc.format == ImPlatform_IndexFormat_UInt16) ? sizeof(uint16_t) : sizeof(uint32_t);

    void* mapped;
    HRESULT hr = buffer->pIndexBuffer->Map(D3D10_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr))
        return false;

    size_t byte_offset = offset * index_size;
    size_t byte_size = index_count * index_size;
    memcpy((char*)mapped + byte_offset, index_data, byte_size);
    buffer->pIndexBuffer->Unmap();
    return true;
}

IMPLATFORM_API void ImPlatform_DestroyIndexBuffer(ImPlatform_IndexBuffer index_buffer)
{
    if (!index_buffer)
        return;

    ImPlatform_BufferData_DX10* buffer = (ImPlatform_BufferData_DX10*)index_buffer;
    if (buffer->pIndexBuffer) buffer->pIndexBuffer->Release();
    delete buffer;
}

IMPLATFORM_API void ImPlatform_BindVertexBuffer(ImPlatform_VertexBuffer vertex_buffer)
{
    if (!vertex_buffer)
        return;

    ImPlatform_BufferData_DX10* buffer = (ImPlatform_BufferData_DX10*)vertex_buffer;
    UINT stride = buffer->vb_desc.vertex_stride;
    UINT offset = 0;
    g_GfxData.pDevice->IASetVertexBuffers(0, 1, &buffer->pVertexBuffer, &stride, &offset);
    if (buffer->pInputLayout)
        g_GfxData.pDevice->IASetInputLayout(buffer->pInputLayout);
}

IMPLATFORM_API void ImPlatform_BindIndexBuffer(ImPlatform_IndexBuffer index_buffer)
{
    if (!index_buffer)
        return;

    ImPlatform_BufferData_DX10* buffer = (ImPlatform_BufferData_DX10*)index_buffer;
    DXGI_FORMAT format = ImPlatform_GetIndexFormat_DX10(buffer->ib_desc.format);
    g_GfxData.pDevice->IASetIndexBuffer(buffer->pIndexBuffer, format, 0);
}

IMPLATFORM_API void ImPlatform_DrawIndexed(unsigned int primitive_type, unsigned int index_count, unsigned int start_index)
{
    D3D10_PRIMITIVE_TOPOLOGY topology;
    switch (primitive_type)
    {
    case 0: topology = D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
    case 1: topology = D3D10_PRIMITIVE_TOPOLOGY_LINELIST; break;
    case 2: topology = D3D10_PRIMITIVE_TOPOLOGY_POINTLIST; break;
    default: topology = D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
    }

    g_GfxData.pDevice->IASetPrimitiveTopology(topology);
    g_GfxData.pDevice->DrawIndexed(index_count, start_index, 0);
}

// ============================================================================
// Custom Shader System API - DirectX 10
// ============================================================================

struct ImPlatform_ShaderData_DX10
{
    union
    {
        ID3D10VertexShader* pVertexShader;
        ID3D10PixelShader* pPixelShader;
    };
    ImPlatform_ShaderStage stage;
    ID3DBlob* pBlob;
};

struct ImPlatform_ShaderProgramData_DX10
{
    ID3D10VertexShader* pVertexShader;
    ID3D10PixelShader* pPixelShader;
    ID3DBlob* pVSBlob;
    ID3D10InputLayout* pInputLayout;
    ID3D10Buffer* pVertexConstantBuffer;
};

// Constant buffer structure for projection matrix
struct VERTEX_CONSTANT_BUFFER_DX10
{
    float   mvp[4][4];
};

IMPLATFORM_API ImPlatform_Shader ImPlatform_CreateShader(const ImPlatform_ShaderDesc* desc)
{
    if (!desc || !desc->source_code)
        return NULL;

    if (desc->format != ImPlatform_ShaderFormat_HLSL)
        return NULL;

    ImPlatform_ShaderData_DX10* shader_data = new ImPlatform_ShaderData_DX10();
    memset(shader_data, 0, sizeof(ImPlatform_ShaderData_DX10));
    shader_data->stage = desc->stage;

    ID3DBlob* pErrorBlob = NULL;
    const char* target = NULL;

    if (desc->stage == ImPlatform_ShaderStage_Vertex)
        target = "vs_4_0";
    else if (desc->stage == ImPlatform_ShaderStage_Fragment)
        target = "ps_4_0";
    else
    {
        delete shader_data;
        return NULL;
    }

    HRESULT hr = D3DCompile(
        desc->source_code,
        strlen(desc->source_code),
        NULL, NULL, NULL,
        desc->entry_point ? desc->entry_point : "main",
        target,
        D3DCOMPILE_ENABLE_STRICTNESS, 0,
        &shader_data->pBlob,
        &pErrorBlob);

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

    if (desc->stage == ImPlatform_ShaderStage_Vertex)
    {
        hr = g_GfxData.pDevice->CreateVertexShader(
            shader_data->pBlob->GetBufferPointer(),
            shader_data->pBlob->GetBufferSize(),
            &shader_data->pVertexShader);
    }
    else
    {
        hr = g_GfxData.pDevice->CreatePixelShader(
            shader_data->pBlob->GetBufferPointer(),
            shader_data->pBlob->GetBufferSize(),
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

    ImPlatform_ShaderData_DX10* shader_data = (ImPlatform_ShaderData_DX10*)shader;
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

    ImPlatform_ShaderData_DX10* vs_data = (ImPlatform_ShaderData_DX10*)vertex_shader;
    ImPlatform_ShaderData_DX10* ps_data = (ImPlatform_ShaderData_DX10*)fragment_shader;

    if (vs_data->stage != ImPlatform_ShaderStage_Vertex || ps_data->stage != ImPlatform_ShaderStage_Fragment)
        return NULL;

    ImPlatform_ShaderProgramData_DX10* program = new ImPlatform_ShaderProgramData_DX10();
    memset(program, 0, sizeof(ImPlatform_ShaderProgramData_DX10));
    program->pVertexShader = vs_data->pVertexShader;
    program->pPixelShader = ps_data->pPixelShader;
    program->pVSBlob = vs_data->pBlob;

    program->pVertexShader->AddRef();
    program->pPixelShader->AddRef();
    if (program->pVSBlob)
        program->pVSBlob->AddRef();

    // Create input layout for ImGui's vertex format (must match ImDrawVert structure)
    // ImDrawVert layout: pos(8 bytes), uv(8 bytes), col(4 bytes) = offsets 0, 8, 16
    if (program->pVSBlob)
    {
        D3D10_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 0,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 8,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D10_INPUT_PER_VERTEX_DATA, 0 },
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
    D3D10_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(VERTEX_CONSTANT_BUFFER_DX10);
    cbDesc.Usage = D3D10_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
    cbDesc.MiscFlags = 0;
    g_GfxData.pDevice->CreateBuffer(&cbDesc, nullptr, &program->pVertexConstantBuffer);

    return (ImPlatform_ShaderProgram)program;
}

IMPLATFORM_API void ImPlatform_DestroyShaderProgram(ImPlatform_ShaderProgram program)
{
    if (!program)
        return;

    ImPlatform_ShaderProgramData_DX10* program_data = (ImPlatform_ShaderProgramData_DX10*)program;
    if (program_data->pVertexShader) program_data->pVertexShader->Release();
    if (program_data->pPixelShader) program_data->pPixelShader->Release();
    if (program_data->pVSBlob) program_data->pVSBlob->Release();
    if (program_data->pInputLayout) program_data->pInputLayout->Release();
    if (program_data->pVertexConstantBuffer) program_data->pVertexConstantBuffer->Release();
    delete program_data;
}

IMPLATFORM_API void ImPlatform_UseShaderProgram(ImPlatform_ShaderProgram program)
{
    if (!program)
        return;

    ImPlatform_ShaderProgramData_DX10* program_data = (ImPlatform_ShaderProgramData_DX10*)program;
    g_GfxData.pDevice->VSSetShader(program_data->pVertexShader);
    g_GfxData.pDevice->PSSetShader(program_data->pPixelShader);
}

IMPLATFORM_API bool ImPlatform_SetShaderUniform(ImPlatform_ShaderProgram program, const char* name, const void* data, unsigned int size)
{
    return false; // DX10 uses constant buffers
}

IMPLATFORM_API bool ImPlatform_SetShaderTexture(ImPlatform_ShaderProgram program, const char* name, unsigned int slot, ImTextureID texture)
{
    if (!program || !texture)
        return false;

    ID3D10ShaderResourceView* pSRV = (ID3D10ShaderResourceView*)texture;
    g_GfxData.pDevice->PSSetShaderResources(slot, 1, &pSRV);
    return true;
}

// ============================================================================
// Custom Shader DrawList Integration
// ============================================================================

// ImDrawCallback handler to activate a custom shader
static void ImPlatform_SetCustomShader(const ImDrawList* parent_list, const ImDrawCmd* cmd)
{
    ImPlatform_ShaderProgram program = (ImPlatform_ShaderProgram)cmd->UserCallbackData;
    if (program)
    {
        ImPlatform_ShaderProgramData_DX10* program_data = (ImPlatform_ShaderProgramData_DX10*)program;

        // Set custom shaders
        ImPlatform_UseShaderProgram(program);

        // Set the input layout for the custom shader
        if (program_data->pInputLayout)
        {
            g_GfxData.pDevice->IASetInputLayout(program_data->pInputLayout);
        }

        // Note: We don't need to set a constant buffer here!
        // ImGui's backend has already set up the vertex constant buffer with the
        // correct projection matrix before this callback is invoked.
        // Our custom shaders will use that same constant buffer at register(b0).
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

    draw->AddCallback(ImDrawCallback_ResetRenderState, NULL);
}

#endif // IM_GFX_DIRECTX10
