// dear imgui: Graphics API Abstraction - OpenGL 3 Backend
// This handles OpenGL context creation, rendering, and presentation

#include "ImPlatform_Internal.h"

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_OPENGL3)

#include <stdint.h>  // For uint16_t, uint32_t
#include "../imgui/backends/imgui_impl_opengl3.h"

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    // Need to link with opengl32.lib
    #pragma comment(lib, "opengl32")
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    #include <GLFW/glfw3.h>
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
    #include <SDL.h>
    #include <SDL_opengl.h>
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
    #include <SDL3/SDL.h>
    #include <SDL3/SDL_opengl.h>
#endif

// Global state
static ImPlatform_GfxData_OpenGL3 g_GfxData = { 0 };

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
static HGLRC g_hRC = NULL; // Shared GL context for main window
static int g_Width = 1280;
static int g_Height = 800;

// Data stored per platform window (for multi-viewports)
struct WGL_WindowData { HDC hDC; };
static WGL_WindowData g_MainWindow;

// Forward declarations for viewport hooks
static void Hook_Renderer_CreateWindow(ImGuiViewport* viewport);
static void Hook_Renderer_DestroyWindow(ImGuiViewport* viewport);
static void Hook_Platform_RenderWindow(ImGuiViewport* viewport, void*);
static void Hook_Renderer_SwapBuffers(ImGuiViewport* viewport, void*);
#endif

// Internal API - Create OpenGL context
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
bool ImPlatform_Gfx_CreateDevice_OpenGL3(void* hWnd, ImPlatform_GfxData_OpenGL3* pData)
{
    HWND hwnd = (HWND)hWnd;
    HDC hDc = ::GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = { 0 };
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;

    const int pf = ::ChoosePixelFormat(hDc, &pfd);
    if (pf == 0)
        return false;
    if (::SetPixelFormat(hDc, pf, &pfd) == FALSE)
        return false;
    ::ReleaseDC(hwnd, hDc);

    pData->hDC = ::GetDC(hwnd);
    if (!g_hRC)
        g_hRC = wglCreateContext(pData->hDC);
    pData->hRC = g_hRC;

    wglMakeCurrent(pData->hDC, pData->hRC);

    // Store main window HDC for viewport system (only for the first window created)
    // Viewport windows will also call this function, but we don't want to overwrite the main window HDC
    if (g_MainWindow.hDC == NULL)
        g_MainWindow.hDC = pData->hDC;

    return true;
}

void ImPlatform_Gfx_CleanupDevice_OpenGL3(void* hWnd, ImPlatform_GfxData_OpenGL3* pData)
{
    HWND hwnd = (HWND)hWnd;
    wglMakeCurrent(NULL, NULL);
    ::ReleaseDC(hwnd, pData->hDC);
}
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
bool ImPlatform_Gfx_CreateDevice_OpenGL3(void* pWindow, ImPlatform_GfxData_OpenGL3* pData)
{
    // GLFW creates and manages the OpenGL context automatically
    (void)pWindow;
    (void)pData;
    return true;
}

void ImPlatform_Gfx_CleanupDevice_OpenGL3(void* pWindow, ImPlatform_GfxData_OpenGL3* pData)
{
    // GLFW destroys the context automatically
    (void)pWindow;
    (void)pData;
}
#elif defined(IM_CURRENT_PLATFORM) && ((IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2) || (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3))
bool ImPlatform_Gfx_CreateDevice_OpenGL3(void* pWindow, void* glContext, ImPlatform_GfxData_OpenGL3* pData)
{
    // SDL creates and manages the OpenGL context
    (void)pWindow;
    (void)glContext;
    (void)pData;
    return true;
}

void ImPlatform_Gfx_CleanupDevice_OpenGL3(void* pWindow, void* glContext, ImPlatform_GfxData_OpenGL3* pData)
{
    // SDL destroys the context in the app backend
    (void)pWindow;
    (void)glContext;
    (void)pData;
}
#endif

// Internal API - Get OpenGL3 gfx data
ImPlatform_GfxData_OpenGL3* ImPlatform_Gfx_GetData_OpenGL3(void)
{
    return &g_GfxData;
}

// ImPlatform API - InitGfxAPI
IMPLATFORM_API bool ImPlatform_InitGfxAPI(void)
{
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    HWND hWnd = ImPlatform_App_GetHWND();
    if (!ImPlatform_Gfx_CreateDevice_OpenGL3(hWnd, &g_GfxData))
    {
        return false;
    }
    return true;
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    GLFWwindow* pWindow = ImPlatform_App_GetGLFWWindow();
    if (!ImPlatform_Gfx_CreateDevice_OpenGL3(pWindow, &g_GfxData))
    {
        return false;
    }
    return true;
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
    SDL_Window* pWindow = ImPlatform_App_GetSDL2Window();
    SDL_GLContext glContext = ImPlatform_App_GetData_SDL2()->glContext;
    if (!ImPlatform_Gfx_CreateDevice_OpenGL3(pWindow, glContext, &g_GfxData))
    {
        return false;
    }
    return true;
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
    SDL_Window* pWindow = ImPlatform_App_GetSDL3Window();
    SDL_GLContext glContext = ImPlatform_App_GetData_SDL3()->glContext;
    if (!ImPlatform_Gfx_CreateDevice_OpenGL3(pWindow, glContext, &g_GfxData))
    {
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
    // Decide GL+GLSL versions
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    const char* glsl_version = "#version 130";
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    const char* glsl_version = "#version 130";
#elif defined(IM_CURRENT_PLATFORM) && ((IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2) || (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3))
    // Use same versioning as SDL examples
#if defined(IMGUI_IMPL_OPENGL_ES2)
    const char* glsl_version = "#version 100";
#elif defined(IMGUI_IMPL_OPENGL_ES3)
    const char* glsl_version = "#version 300 es";
#elif defined(__APPLE__)
    const char* glsl_version = "#version 150";
#else
    const char* glsl_version = "#version 130";
#endif
#endif

    if (!ImGui_ImplOpenGL3_Init(glsl_version))
        return false;

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    // Win32+GL needs specific hooks for viewport, as there are specific things needed to tie Win32 and GL api.
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        IM_ASSERT(platform_io.Renderer_CreateWindow == NULL);
        IM_ASSERT(platform_io.Renderer_DestroyWindow == NULL);
        IM_ASSERT(platform_io.Renderer_SwapBuffers == NULL);
        IM_ASSERT(platform_io.Platform_RenderWindow == NULL);
        platform_io.Renderer_CreateWindow = Hook_Renderer_CreateWindow;
        platform_io.Renderer_DestroyWindow = Hook_Renderer_DestroyWindow;
        platform_io.Renderer_SwapBuffers = Hook_Renderer_SwapBuffers;
        platform_io.Platform_RenderWindow = Hook_Platform_RenderWindow;
    }
#endif

    return true;
}

// ImPlatform API - GfxCheck
IMPLATFORM_API bool ImPlatform_GfxCheck(void)
{
    // No special checks needed for OpenGL
    return true;
}

// ImPlatform API - GfxAPINewFrame
IMPLATFORM_API void ImPlatform_GfxAPINewFrame(void)
{
    ImGui_ImplOpenGL3_NewFrame();
}

// ImPlatform API - GfxAPIClear
IMPLATFORM_API bool ImPlatform_GfxAPIClear(ImVec4 const vClearColor)
{
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    glViewport(0, 0, g_Width, g_Height);
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    // GLFW handles viewport through framebuffer size callback
    int display_w, display_h;
    glfwGetFramebufferSize(ImPlatform_App_GetGLFWWindow(), &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
#elif defined(IM_CURRENT_PLATFORM) && ((IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2) || (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3))
    // SDL handles viewport through display size
    ImGuiIO& io = ImGui::GetIO();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
#endif

    glClearColor(vClearColor.x, vClearColor.y, vClearColor.z, vClearColor.w);
    glClear(GL_COLOR_BUFFER_BIT);
    return true;
}

// ImPlatform API - GfxAPIRender
IMPLATFORM_API bool ImPlatform_GfxAPIRender(ImVec4 const vClearColor)
{
    (void)vClearColor; // Not used for OpenGL
    ImDrawData* draw_data = ImGui::GetDrawData();
    ImGui_ImplOpenGL3_RenderDrawData(draw_data);
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

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
        // Restore the OpenGL rendering context to the main window DC
        wglMakeCurrent(g_MainWindow.hDC, g_hRC);
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
        // GLFW backend handles context switching automatically
        glfwMakeContextCurrent(ImPlatform_App_GetGLFWWindow());
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
        // Restore the SDL2 context
        SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
        SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
        SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
        // Restore the SDL3 context
        SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
        SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
        SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
#endif
    }
}

// ImPlatform API - GfxAPISwapBuffer
IMPLATFORM_API bool ImPlatform_GfxAPISwapBuffer(void)
{
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    ::SwapBuffers(g_MainWindow.hDC);
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    glfwSwapBuffers(ImPlatform_App_GetGLFWWindow());
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
    SDL_GL_SwapWindow(ImPlatform_App_GetSDL2Window());
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
    SDL_GL_SwapWindow(ImPlatform_App_GetSDL3Window());
#endif
    return true;
}

// ImPlatform API - ShutdownGfxAPI
IMPLATFORM_API void ImPlatform_ShutdownGfxAPI(void)
{
    // Nothing special needed for OpenGL
}

// ImPlatform API - ShutdownWindow
IMPLATFORM_API void ImPlatform_ShutdownWindow(void)
{
    ImGui_ImplOpenGL3_Shutdown();

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    HWND hWnd = ImPlatform_App_GetHWND();
    ImPlatform_Gfx_CleanupDevice_OpenGL3(hWnd, &g_GfxData);
    if (g_hRC)
    {
        wglDeleteContext(g_hRC);
        g_hRC = NULL;
    }
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    GLFWwindow* pWindow = ImPlatform_App_GetGLFWWindow();
    ImPlatform_Gfx_CleanupDevice_OpenGL3(pWindow, &g_GfxData);
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
    SDL_Window* pWindow = ImPlatform_App_GetSDL2Window();
    SDL_GLContext glContext = ImPlatform_App_GetData_SDL2()->glContext;
    ImPlatform_Gfx_CleanupDevice_OpenGL3(pWindow, glContext, &g_GfxData);
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
    SDL_Window* pWindow = ImPlatform_App_GetSDL3Window();
    SDL_GLContext glContext = ImPlatform_App_GetData_SDL3()->glContext;
    ImPlatform_Gfx_CleanupDevice_OpenGL3(pWindow, glContext, &g_GfxData);
#endif
}

// Platform-specific size tracking for Win32
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
void ImPlatform_Gfx_SetSize_OpenGL3(int width, int height)
{
    g_Width = width;
    g_Height = height;
}

// Win32+OpenGL viewport hooks
// Unlike most other backend combinations, we need specific hooks to combine Win32+OpenGL.
static void Hook_Renderer_CreateWindow(ImGuiViewport* viewport)
{
    assert(viewport->RendererUserData == NULL);

    WGL_WindowData* data = IM_NEW(WGL_WindowData);
    // Create OpenGL context for this viewport window
    // We need a temporary structure to get the HDC
    ImPlatform_GfxData_OpenGL3 temp_data = { 0 };
    ImPlatform_Gfx_CreateDevice_OpenGL3((HWND)viewport->PlatformHandle, &temp_data);
    data->hDC = temp_data.hDC;
    viewport->RendererUserData = data;
}

static void Hook_Renderer_DestroyWindow(ImGuiViewport* viewport)
{
    if (viewport->RendererUserData != NULL)
    {
        WGL_WindowData* data = (WGL_WindowData*)viewport->RendererUserData;
        // We need to pass the correct data structure for cleanup
        ImPlatform_GfxData_OpenGL3 temp_data = { 0 };
        temp_data.hDC = data->hDC;
        temp_data.hRC = g_hRC;  // Shared context
        ImPlatform_Gfx_CleanupDevice_OpenGL3((HWND)viewport->PlatformHandle, &temp_data);
        IM_DELETE(data);
        viewport->RendererUserData = NULL;
    }
}

static void Hook_Platform_RenderWindow(ImGuiViewport* viewport, void*)
{
    // Activate the platform window DC in the OpenGL rendering context
    if (WGL_WindowData* data = (WGL_WindowData*)viewport->RendererUserData)
        wglMakeCurrent(data->hDC, g_hRC);
}

static void Hook_Renderer_SwapBuffers(ImGuiViewport* viewport, void*)
{
    if (WGL_WindowData* data = (WGL_WindowData*)viewport->RendererUserData)
        ::SwapBuffers(data->hDC);
}
#endif

// ============================================================================
// Texture Creation API - OpenGL3 Implementation
// ============================================================================

// Define OpenGL 3.0+ constants if not available
#ifndef GL_R8
#define GL_R8 0x8229
#endif
#ifndef GL_RG8
#define GL_RG8 0x822B
#endif
#ifndef GL_R16
#define GL_R16 0x822A
#endif
#ifndef GL_RG16
#define GL_RG16 0x822C
#endif
#ifndef GL_R32F
#define GL_R32F 0x822E
#endif
#ifndef GL_RG32F
#define GL_RG32F 0x8230
#endif
#ifndef GL_RGBA32F
#define GL_RGBA32F 0x8814
#endif
#ifndef GL_RED
#define GL_RED 0x1903
#endif
#ifndef GL_RG
#define GL_RG 0x8227
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_MIRRORED_REPEAT
#define GL_MIRRORED_REPEAT 0x8370
#endif

// Define buffer-related constants if not available
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_ELEMENT_ARRAY_BUFFER
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif
#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW 0x88E8
#endif
#ifndef GL_STREAM_DRAW
#define GL_STREAM_DRAW 0x88E0
#endif

// Helper to get OpenGL format info from ImPlatform format
static void ImPlatform_GetOpenGLFormat(ImPlatform_PixelFormat format, GLint* out_internal_format, GLenum* out_format, GLenum* out_type, int* out_channels)
{
    switch (format)
    {
    case ImPlatform_PixelFormat_R8:
        *out_internal_format = GL_R8;
        *out_format = GL_RED;
        *out_type = GL_UNSIGNED_BYTE;
        *out_channels = 1;
        break;
    case ImPlatform_PixelFormat_RG8:
        *out_internal_format = GL_RG8;
        *out_format = GL_RG;
        *out_type = GL_UNSIGNED_BYTE;
        *out_channels = 2;
        break;
    case ImPlatform_PixelFormat_RGB8:
        *out_internal_format = GL_RGB8;
        *out_format = GL_RGB;
        *out_type = GL_UNSIGNED_BYTE;
        *out_channels = 3;
        break;
    case ImPlatform_PixelFormat_RGBA8:
        *out_internal_format = GL_RGBA8;
        *out_format = GL_RGBA;
        *out_type = GL_UNSIGNED_BYTE;
        *out_channels = 4;
        break;
    case ImPlatform_PixelFormat_R16:
        *out_internal_format = GL_R16;
        *out_format = GL_RED;
        *out_type = GL_UNSIGNED_SHORT;
        *out_channels = 1;
        break;
    case ImPlatform_PixelFormat_RG16:
        *out_internal_format = GL_RG16;
        *out_format = GL_RG;
        *out_type = GL_UNSIGNED_SHORT;
        *out_channels = 2;
        break;
    case ImPlatform_PixelFormat_RGBA16:
        *out_internal_format = GL_RGBA16;
        *out_format = GL_RGBA;
        *out_type = GL_UNSIGNED_SHORT;
        *out_channels = 4;
        break;
    case ImPlatform_PixelFormat_R32F:
        *out_internal_format = GL_R32F;
        *out_format = GL_RED;
        *out_type = GL_FLOAT;
        *out_channels = 1;
        break;
    case ImPlatform_PixelFormat_RG32F:
        *out_internal_format = GL_RG32F;
        *out_format = GL_RG;
        *out_type = GL_FLOAT;
        *out_channels = 2;
        break;
    case ImPlatform_PixelFormat_RGBA32F:
        *out_internal_format = GL_RGBA32F;
        *out_format = GL_RGBA;
        *out_type = GL_FLOAT;
        *out_channels = 4;
        break;
    default:
        *out_internal_format = GL_RGBA8;
        *out_format = GL_RGBA;
        *out_type = GL_UNSIGNED_BYTE;
        *out_channels = 4;
        break;
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
    if (!desc || !pixel_data)
        return 0;

    GLint internal_format;
    GLenum format;
    GLenum type;
    int channels;
    ImPlatform_GetOpenGLFormat(desc->format, &internal_format, &format, &type, &channels);

    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    // Set filtering
    GLint min_filter = (desc->min_filter == ImPlatform_TextureFilter_Nearest) ? GL_NEAREST : GL_LINEAR;
    GLint mag_filter = (desc->mag_filter == ImPlatform_TextureFilter_Nearest) ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);

    // Set wrapping
    GLint wrap_s = GL_CLAMP_TO_EDGE;
    GLint wrap_t = GL_CLAMP_TO_EDGE;

    if (desc->wrap_u == ImPlatform_TextureWrap_Repeat)
        wrap_s = GL_REPEAT;
    else if (desc->wrap_u == ImPlatform_TextureWrap_Mirror)
        wrap_s = GL_MIRRORED_REPEAT;

    if (desc->wrap_v == ImPlatform_TextureWrap_Repeat)
        wrap_t = GL_REPEAT;
    else if (desc->wrap_v == ImPlatform_TextureWrap_Mirror)
        wrap_t = GL_MIRRORED_REPEAT;

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_s);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_t);

#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif

    // Upload texture data
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, desc->width, desc->height, 0, format, type, pixel_data);

    return (ImTextureID)(intptr_t)texture_id;
}

IMPLATFORM_API bool ImPlatform_UpdateTexture(ImTextureID texture_id, const void* pixel_data,
                                              unsigned int x, unsigned int y,
                                              unsigned int width, unsigned int height)
{
    if (!texture_id || !pixel_data)
        return false;

    GLuint tex = (GLuint)(intptr_t)texture_id;
    glBindTexture(GL_TEXTURE_2D, tex);

    // Get texture format info
    GLint internal_format;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal_format);

    // Determine format and type from internal format
    GLenum format = GL_RGBA;
    GLenum type = GL_UNSIGNED_BYTE;

    // Map internal format to format/type (simplified)
    if (internal_format == GL_R8 || internal_format == GL_R16 || internal_format == GL_R32F)
        format = GL_RED;
    else if (internal_format == GL_RG8 || internal_format == GL_RG16 || internal_format == GL_RG32F)
        format = GL_RG;
    else if (internal_format == GL_RGB8)
        format = GL_RGB;
    else if (internal_format == GL_RGBA8 || internal_format == GL_RGBA16 || internal_format == GL_RGBA32F)
        format = GL_RGBA;

    if (internal_format == GL_R16 || internal_format == GL_RG16 || internal_format == GL_RGBA16)
        type = GL_UNSIGNED_SHORT;
    else if (internal_format == GL_R32F || internal_format == GL_RG32F || internal_format == GL_RGBA32F)
        type = GL_FLOAT;

#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif

    // Update texture sub-region
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, format, type, pixel_data);

    return true;
}

IMPLATFORM_API void ImPlatform_DestroyTexture(ImTextureID texture_id)
{
    if (!texture_id)
        return;

    GLuint tex = (GLuint)(intptr_t)texture_id;
    glDeleteTextures(1, &tex);
}

// ============================================================================
// Custom Vertex/Index Buffer Management API - OpenGL3 Implementation
// ============================================================================

// NOTE: Full implementation of vertex/index buffer API requires access to OpenGL 3+ functions
// (glGenVertexArrays, glGenBuffers, etc.) which need to be loaded dynamically on Windows.
// Since imgui_impl_opengl3.cpp already loads these, we would need to either:
// 1. Expose the loaded function pointers from imgui backend
// 2. Load them again here (duplicated effort)
// 3. Implement this properly when we add the Custom Shader System
//
// For now, these are stub implementations that will be completed alongside the shader system.

IMPLATFORM_API ImPlatform_VertexBuffer ImPlatform_CreateVertexBuffer(const void* vertex_data, const ImPlatform_VertexBufferDesc* desc)
{
    (void)vertex_data;
    (void)desc;
    // TODO: Implement when shader system is added
    return NULL;
}

IMPLATFORM_API bool ImPlatform_UpdateVertexBuffer(ImPlatform_VertexBuffer buffer, const void* vertex_data, unsigned int offset, unsigned int count)
{
    (void)buffer;
    (void)vertex_data;
    (void)offset;
    (void)count;
    // TODO: Implement when shader system is added
    return false;
}

IMPLATFORM_API void ImPlatform_DestroyVertexBuffer(ImPlatform_VertexBuffer buffer)
{
    (void)buffer;
    // TODO: Implement when shader system is added
}

IMPLATFORM_API ImPlatform_IndexBuffer ImPlatform_CreateIndexBuffer(const void* index_data, const ImPlatform_IndexBufferDesc* desc)
{
    (void)index_data;
    (void)desc;
    // TODO: Implement when shader system is added
    return NULL;
}

IMPLATFORM_API bool ImPlatform_UpdateIndexBuffer(ImPlatform_IndexBuffer buffer, const void* index_data, unsigned int offset, unsigned int count)
{
    (void)buffer;
    (void)index_data;
    (void)offset;
    (void)count;
    // TODO: Implement when shader system is added
    return false;
}

IMPLATFORM_API void ImPlatform_DestroyIndexBuffer(ImPlatform_IndexBuffer buffer)
{
    (void)buffer;
    // TODO: Implement when shader system is added
}

IMPLATFORM_API void ImPlatform_BindBuffers(ImPlatform_VertexBuffer vertex_buffer, ImPlatform_IndexBuffer index_buffer)
{
    (void)vertex_buffer;
    (void)index_buffer;
    // TODO: Implement when shader system is added
}

IMPLATFORM_API void ImPlatform_DrawIndexed(unsigned int primitive_type, unsigned int index_count, unsigned int start_index)
{
    (void)primitive_type;
    (void)index_count;
    (void)start_index;
    // TODO: Implement when shader system is added
}

// ============================================================================
// Custom Shader System API - OpenGL3 Implementation
// ============================================================================

// NOTE: Full shader system implementation is complex and requires:
// - Shader compilation (glCreateShader, glShaderSource, glCompileShader)
// - Program linking (glCreateProgram, glAttachShader, glLinkProgram)
// - Uniform management (glGetUniformLocation, glUniform*)
// - Proper error handling and reporting
//
// This is a substantial feature that should be implemented together with
// the vertex/index buffer system as they are tightly coupled.
// For now, these are stub implementations.

IMPLATFORM_API ImPlatform_Shader ImPlatform_CreateShader(const ImPlatform_ShaderDesc* desc)
{
    (void)desc;
    // TODO: Implement shader compilation
    return NULL;
}

IMPLATFORM_API void ImPlatform_DestroyShader(ImPlatform_Shader shader)
{
    (void)shader;
    // TODO: Implement shader destruction
}

IMPLATFORM_API ImPlatform_ShaderProgram ImPlatform_CreateShaderProgram(ImPlatform_Shader vertex_shader, ImPlatform_Shader fragment_shader)
{
    (void)vertex_shader;
    (void)fragment_shader;
    // TODO: Implement program linking
    return NULL;
}

IMPLATFORM_API void ImPlatform_DestroyShaderProgram(ImPlatform_ShaderProgram program)
{
    (void)program;
    // TODO: Implement program destruction
}

IMPLATFORM_API void ImPlatform_BindShaderProgram(ImPlatform_ShaderProgram program)
{
    (void)program;
    // TODO: Implement program binding
}

IMPLATFORM_API bool ImPlatform_SetShaderUniform(ImPlatform_ShaderProgram program, const char* name, const void* data, unsigned int size)
{
    (void)program;
    (void)name;
    (void)data;
    (void)size;
    // TODO: Implement uniform setting
    return false;
}

IMPLATFORM_API bool ImPlatform_SetShaderTexture(ImPlatform_ShaderProgram program, const char* name, unsigned int slot, ImTextureID texture)
{
    (void)program;
    (void)name;
    (void)slot;
    (void)texture;
    // TODO: Implement texture binding
    return false;
}

#endif // IM_GFX_OPENGL3
