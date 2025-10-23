// dear imgui: Graphics API Abstraction - OpenGL 3 Backend
// This handles OpenGL context creation, rendering, and presentation

#include "ImPlatform_Internal.h"

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_OPENGL3)

#include <stdint.h>  // For uint16_t, uint32_t
#include <stdio.h>   // For fprintf, stderr
#include <string.h>  // For memset, memcpy
#include "../imgui/backends/imgui_impl_opengl3.h"

// Note: OpenGL loader is included in ImPlatform_Internal.h

// Define additional GL constants not in the stripped loader
#ifndef GL_RGB
#define GL_RGB                            0x1907
#endif
#ifndef GL_RGB8
#define GL_RGB8                           0x8051
#endif
#ifndef GL_RGBA8
#define GL_RGBA8                          0x8058
#endif
#ifndef GL_RGBA16
#define GL_RGBA16                         0x805B
#endif
#ifndef GL_LINES
#define GL_LINES                          0x0001
#endif
#ifndef GL_POINTS
#define GL_POINTS                         0x0000
#endif
#ifndef GL_TEXTURE_INTERNAL_FORMAT
#define GL_TEXTURE_INTERNAL_FORMAT        0x1003
#endif
#ifndef GL_RED
#define GL_RED                            0x1903
#endif
#ifndef GL_RG
#define GL_RG                             0x8227
#endif
#ifndef GL_R8
#define GL_R8                             0x8229
#endif
#ifndef GL_R16
#define GL_R16                            0x822A
#endif
#ifndef GL_RG8
#define GL_RG8                            0x822B
#endif
#ifndef GL_RG16
#define GL_RG16                           0x822C
#endif
#ifndef GL_R32F
#define GL_R32F                           0x822E
#endif
#ifndef GL_RG32F
#define GL_RG32F                          0x8230
#endif
#ifndef GL_RGBA32F
#define GL_RGBA32F                        0x8814
#endif
#ifndef GL_UNSIGNED_SHORT
#define GL_UNSIGNED_SHORT                 0x1403
#endif

// Load additional GL function pointers not in the stripped loader
typedef void (APIENTRYP PFNGLUNIFORM1FVPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP PFNGLUNIFORM2FVPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP PFNGLUNIFORM3FVPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP PFNGLUNIFORM4FVPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP PFNGLUNIFORM1IVPROC) (GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRYP PFNGLGETTEXLEVELPARAMETERIVPROC) (GLenum target, GLint level, GLenum pname, GLint *params);

static PFNGLUNIFORM1FVPROC glUniform1fv_Ptr = NULL;
static PFNGLUNIFORM2FVPROC glUniform2fv_Ptr = NULL;
static PFNGLUNIFORM3FVPROC glUniform3fv_Ptr = NULL;
static PFNGLUNIFORM4FVPROC glUniform4fv_Ptr = NULL;
static PFNGLUNIFORM1IVPROC glUniform1iv_Ptr = NULL;
static PFNGLGETTEXLEVELPARAMETERIVPROC glGetTexLevelParameteriv_Ptr = NULL;

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

    // Load additional GL function pointers not in the stripped loader
    glUniform1fv_Ptr = (PFNGLUNIFORM1FVPROC)imgl3wGetProcAddress("glUniform1fv");
    glUniform2fv_Ptr = (PFNGLUNIFORM2FVPROC)imgl3wGetProcAddress("glUniform2fv");
    glUniform3fv_Ptr = (PFNGLUNIFORM3FVPROC)imgl3wGetProcAddress("glUniform3fv");
    glUniform4fv_Ptr = (PFNGLUNIFORM4FVPROC)imgl3wGetProcAddress("glUniform4fv");
    glUniform1iv_Ptr = (PFNGLUNIFORM1IVPROC)imgl3wGetProcAddress("glUniform1iv");
    glGetTexLevelParameteriv_Ptr = (PFNGLGETTEXLEVELPARAMETERIVPROC)imgl3wGetProcAddress("glGetTexLevelParameteriv");

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
    GLint internal_format = GL_RGBA8; // Default
    if (glGetTexLevelParameteriv_Ptr)
        glGetTexLevelParameteriv_Ptr(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal_format);

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

// Internal buffer structure
struct ImPlatform_BufferData_GL
{
    GLuint vbo;                          // Vertex buffer object
    GLuint ibo;                          // Index buffer object
    GLuint vao;                          // Vertex array object
    ImPlatform_VertexBufferDesc vb_desc; // Vertex buffer descriptor (for updates)
    ImPlatform_IndexBufferDesc ib_desc;  // Index buffer descriptor (for updates)
    ImPlatform_VertexAttribute* attributes; // Cached attribute array
};

// Helper to convert buffer usage to GL usage hint
static GLenum ImPlatform_GetGLBufferUsage(ImPlatform_BufferUsage usage)
{
    switch (usage)
    {
    case ImPlatform_BufferUsage_Static:  return GL_STATIC_DRAW;
    case ImPlatform_BufferUsage_Dynamic: return GL_DYNAMIC_DRAW;
    case ImPlatform_BufferUsage_Stream:  return GL_STREAM_DRAW;
    default:                             return GL_STATIC_DRAW;
    }
}

// Helper to get GL type and size for vertex format
static void ImPlatform_GetGLVertexFormat(ImPlatform_VertexFormat format, GLenum* out_type, GLint* out_size, GLboolean* out_normalized)
{
    switch (format)
    {
    case ImPlatform_VertexFormat_Float:
        *out_type = GL_FLOAT;
        *out_size = 1;
        *out_normalized = GL_FALSE;
        break;
    case ImPlatform_VertexFormat_Float2:
        *out_type = GL_FLOAT;
        *out_size = 2;
        *out_normalized = GL_FALSE;
        break;
    case ImPlatform_VertexFormat_Float3:
        *out_type = GL_FLOAT;
        *out_size = 3;
        *out_normalized = GL_FALSE;
        break;
    case ImPlatform_VertexFormat_Float4:
        *out_type = GL_FLOAT;
        *out_size = 4;
        *out_normalized = GL_FALSE;
        break;
    case ImPlatform_VertexFormat_UByte4:
        *out_type = GL_UNSIGNED_BYTE;
        *out_size = 4;
        *out_normalized = GL_FALSE;
        break;
    case ImPlatform_VertexFormat_UByte4N:
        *out_type = GL_UNSIGNED_BYTE;
        *out_size = 4;
        *out_normalized = GL_TRUE;
        break;
    default:
        *out_type = GL_FLOAT;
        *out_size = 3;
        *out_normalized = GL_FALSE;
        break;
    }
}

IMPLATFORM_API ImPlatform_VertexBuffer ImPlatform_CreateVertexBuffer(const void* vertex_data, const ImPlatform_VertexBufferDesc* desc)
{
    if (!desc || !vertex_data)
        return NULL;

    ImPlatform_BufferData_GL* buffer = new ImPlatform_BufferData_GL();
    memset(buffer, 0, sizeof(ImPlatform_BufferData_GL));
    buffer->vb_desc = *desc;

    // Copy attribute array
    if (desc->attribute_count > 0 && desc->attributes)
    {
        buffer->attributes = new ImPlatform_VertexAttribute[desc->attribute_count];
        memcpy(buffer->attributes, desc->attributes, sizeof(ImPlatform_VertexAttribute) * desc->attribute_count);
        buffer->vb_desc.attributes = buffer->attributes;
    }

    // Generate VAO and VBO
    glGenVertexArrays(1, &buffer->vao);
    glGenBuffers(1, &buffer->vbo);

    glBindVertexArray(buffer->vao);
    glBindBuffer(GL_ARRAY_BUFFER, buffer->vbo);

    // Upload vertex data
    size_t size = desc->vertex_count * desc->vertex_stride;
    GLenum usage = ImPlatform_GetGLBufferUsage(desc->usage);
    glBufferData(GL_ARRAY_BUFFER, size, vertex_data, usage);

    // Setup vertex attributes
    for (unsigned int i = 0; i < desc->attribute_count; i++)
    {
        const ImPlatform_VertexAttribute* attr = &desc->attributes[i];

        GLenum type;
        GLint size_components;
        GLboolean normalized;
        ImPlatform_GetGLVertexFormat(attr->format, &type, &size_components, &normalized);

        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, size_components, type, normalized, desc->vertex_stride, (void*)(intptr_t)attr->offset);
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return (ImPlatform_VertexBuffer)buffer;
}

IMPLATFORM_API bool ImPlatform_UpdateVertexBuffer(ImPlatform_VertexBuffer buffer, const void* vertex_data, unsigned int offset, unsigned int count)
{
    if (!buffer || !vertex_data)
        return false;

    ImPlatform_BufferData_GL* buf = (ImPlatform_BufferData_GL*)buffer;

    glBindBuffer(GL_ARRAY_BUFFER, buf->vbo);
    size_t byte_offset = offset * buf->vb_desc.vertex_stride;
    size_t byte_size = count * buf->vb_desc.vertex_stride;
    glBufferSubData(GL_ARRAY_BUFFER, byte_offset, byte_size, vertex_data);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return true;
}

IMPLATFORM_API void ImPlatform_DestroyVertexBuffer(ImPlatform_VertexBuffer buffer)
{
    if (!buffer)
        return;

    ImPlatform_BufferData_GL* buf = (ImPlatform_BufferData_GL*)buffer;

    if (buf->vbo)
        glDeleteBuffers(1, &buf->vbo);
    if (buf->vao)
        glDeleteVertexArrays(1, &buf->vao);
    if (buf->attributes)
        delete[] buf->attributes;

    delete buf;
}

IMPLATFORM_API ImPlatform_IndexBuffer ImPlatform_CreateIndexBuffer(const void* index_data, const ImPlatform_IndexBufferDesc* desc)
{
    if (!desc || !index_data)
        return NULL;

    ImPlatform_BufferData_GL* buffer = new ImPlatform_BufferData_GL();
    memset(buffer, 0, sizeof(ImPlatform_BufferData_GL));
    buffer->ib_desc = *desc;

    // Generate IBO
    glGenBuffers(1, &buffer->ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->ibo);

    // Upload index data
    size_t size = desc->index_count * (desc->format == ImPlatform_IndexFormat_UInt16 ? sizeof(uint16_t) : sizeof(uint32_t));
    GLenum usage = ImPlatform_GetGLBufferUsage(desc->usage);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, index_data, usage);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    return (ImPlatform_IndexBuffer)buffer;
}

IMPLATFORM_API bool ImPlatform_UpdateIndexBuffer(ImPlatform_IndexBuffer buffer, const void* index_data, unsigned int offset, unsigned int count)
{
    if (!buffer || !index_data)
        return false;

    ImPlatform_BufferData_GL* buf = (ImPlatform_BufferData_GL*)buffer;

    unsigned int index_size = (buf->ib_desc.format == ImPlatform_IndexFormat_UInt16) ? sizeof(uint16_t) : sizeof(uint32_t);
    size_t byte_offset = offset * index_size;
    size_t byte_size = count * index_size;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf->ibo);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, byte_offset, byte_size, index_data);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    return true;
}

IMPLATFORM_API void ImPlatform_DestroyIndexBuffer(ImPlatform_IndexBuffer buffer)
{
    if (!buffer)
        return;

    ImPlatform_BufferData_GL* buf = (ImPlatform_BufferData_GL*)buffer;

    if (buf->ibo)
        glDeleteBuffers(1, &buf->ibo);

    delete buf;
}

// Global state for currently bound buffers
static ImPlatform_BufferData_GL* g_BoundVertexBuffer = NULL;
static ImPlatform_BufferData_GL* g_BoundIndexBuffer = NULL;

IMPLATFORM_API void ImPlatform_BindBuffers(ImPlatform_VertexBuffer vertex_buffer, ImPlatform_IndexBuffer index_buffer)
{
    g_BoundVertexBuffer = (ImPlatform_BufferData_GL*)vertex_buffer;
    g_BoundIndexBuffer = (ImPlatform_BufferData_GL*)index_buffer;

    if (g_BoundVertexBuffer)
    {
        glBindVertexArray(g_BoundVertexBuffer->vao);
        if (g_BoundIndexBuffer)
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_BoundIndexBuffer->ibo);
    }
    else
    {
        glBindVertexArray(0);
    }
}

IMPLATFORM_API void ImPlatform_DrawIndexed(unsigned int primitive_type, unsigned int index_count, unsigned int start_index)
{
    if (!g_BoundVertexBuffer || !g_BoundIndexBuffer)
        return;

    // Map primitive type
    GLenum mode = GL_TRIANGLES;
    if (primitive_type == 1)
        mode = GL_LINES;
    else if (primitive_type == 2)
        mode = GL_POINTS;

    // Determine index type
    GLenum index_type = (g_BoundIndexBuffer->ib_desc.format == ImPlatform_IndexFormat_UInt16) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    unsigned int index_size = (index_type == GL_UNSIGNED_SHORT) ? sizeof(uint16_t) : sizeof(uint32_t);

    // Use all indices if count is 0
    if (index_count == 0)
        index_count = g_BoundIndexBuffer->ib_desc.index_count;

    // Draw
    glDrawElements(mode, index_count, index_type, (void*)(intptr_t)(start_index * index_size));
}

// ============================================================================
// Custom Shader System API - OpenGL3 Implementation
// ============================================================================

// Internal shader structure
struct ImPlatform_ShaderData_GL
{
    GLuint shader_id;
    ImPlatform_ShaderStage stage;
};

// Internal shader program structure
// Uniform storage for deferred setting
struct ImPlatform_UniformData_GL
{
    char name[64];
    float data[16];  // Support up to mat4
    unsigned int size;
};

struct ImPlatform_ShaderProgramData_GL
{
    GLuint program_id;
    GLuint vertex_shader;
    GLuint fragment_shader;
    ImPlatform_UniformData_GL uniforms[8];  // Support up to 8 uniforms
    int uniform_count;
};

// Helper to compile a shader and check for errors
static GLuint ImPlatform_CompileShader_GL(GLenum shader_type, const char* source)
{
    GLuint shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    // Check for compilation errors
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char info_log[512];
        glGetShaderInfoLog(shader, 512, NULL, info_log);
        // Log error (in production, you'd want proper logging)
        fprintf(stderr, "Shader compilation failed: %s\n", info_log);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

IMPLATFORM_API ImPlatform_Shader ImPlatform_CreateShader(const ImPlatform_ShaderDesc* desc)
{
    if (!desc || !desc->source_code)
        return NULL;

    // Only support GLSL for OpenGL
    if (desc->format != ImPlatform_ShaderFormat_GLSL)
        return NULL;

    GLenum shader_type;
    switch (desc->stage)
    {
    case ImPlatform_ShaderStage_Vertex:
        shader_type = GL_VERTEX_SHADER;
        break;
    case ImPlatform_ShaderStage_Fragment:
        shader_type = GL_FRAGMENT_SHADER;
        break;
    case ImPlatform_ShaderStage_Compute:
#ifdef GL_COMPUTE_SHADER
        shader_type = GL_COMPUTE_SHADER;
#else
        return NULL; // Compute shaders not supported
#endif
        break;
    default:
        return NULL;
    }

    GLuint shader_id = ImPlatform_CompileShader_GL(shader_type, desc->source_code);
    if (shader_id == 0)
        return NULL;

    ImPlatform_ShaderData_GL* shader_data = new ImPlatform_ShaderData_GL();
    shader_data->shader_id = shader_id;
    shader_data->stage = desc->stage;

    return (ImPlatform_Shader)shader_data;
}

IMPLATFORM_API void ImPlatform_DestroyShader(ImPlatform_Shader shader)
{
    if (!shader)
        return;

    ImPlatform_ShaderData_GL* shader_data = (ImPlatform_ShaderData_GL*)shader;
    if (shader_data->shader_id)
        glDeleteShader(shader_data->shader_id);

    delete shader_data;
}

IMPLATFORM_API ImPlatform_ShaderProgram ImPlatform_CreateShaderProgram(ImPlatform_Shader vertex_shader, ImPlatform_Shader fragment_shader)
{
    if (!vertex_shader || !fragment_shader)
        return NULL;

    ImPlatform_ShaderData_GL* vs_data = (ImPlatform_ShaderData_GL*)vertex_shader;
    ImPlatform_ShaderData_GL* fs_data = (ImPlatform_ShaderData_GL*)fragment_shader;

    // Create program
    GLuint program = glCreateProgram();
    glAttachShader(program, vs_data->shader_id);
    glAttachShader(program, fs_data->shader_id);
    glLinkProgram(program);

    // Check for linking errors
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char info_log[512];
        glGetProgramInfoLog(program, 512, NULL, info_log);
        fprintf(stderr, "Shader program linking failed: %s\n", info_log);
        glDeleteProgram(program);
        return NULL;
    }

    ImPlatform_ShaderProgramData_GL* program_data = new ImPlatform_ShaderProgramData_GL();
    program_data->program_id = program;
    program_data->vertex_shader = vs_data->shader_id;
    program_data->fragment_shader = fs_data->shader_id;
    program_data->uniform_count = 0;

    return (ImPlatform_ShaderProgram)program_data;
}

IMPLATFORM_API void ImPlatform_DestroyShaderProgram(ImPlatform_ShaderProgram program)
{
    if (!program)
        return;

    ImPlatform_ShaderProgramData_GL* program_data = (ImPlatform_ShaderProgramData_GL*)program;

    if (program_data->program_id)
    {
        // Detach shaders before deleting program
        if (program_data->vertex_shader)
            glDetachShader(program_data->program_id, program_data->vertex_shader);
        if (program_data->fragment_shader)
            glDetachShader(program_data->program_id, program_data->fragment_shader);

        glDeleteProgram(program_data->program_id);
    }

    delete program_data;
}

IMPLATFORM_API void ImPlatform_BindShaderProgram(ImPlatform_ShaderProgram program)
{
    if (!program)
    {
        glUseProgram(0);
        return;
    }

    ImPlatform_ShaderProgramData_GL* program_data = (ImPlatform_ShaderProgramData_GL*)program;
    glUseProgram(program_data->program_id);
}

IMPLATFORM_API bool ImPlatform_SetShaderUniform(ImPlatform_ShaderProgram program, const char* name, const void* data, unsigned int size)
{
    if (!program || !name || !data || size > sizeof(float) * 16)
        return false;

    ImPlatform_ShaderProgramData_GL* program_data = (ImPlatform_ShaderProgramData_GL*)program;

    // Find existing uniform or add new one
    int uniform_index = -1;
    for (int i = 0; i < program_data->uniform_count; i++)
    {
        if (strcmp(program_data->uniforms[i].name, name) == 0)
        {
            uniform_index = i;
            break;
        }
    }

    if (uniform_index == -1)
    {
        // Add new uniform
        if (program_data->uniform_count >= 8)
            return false; // Too many uniforms

        uniform_index = program_data->uniform_count++;
        strncpy(program_data->uniforms[uniform_index].name, name, 63);
        program_data->uniforms[uniform_index].name[63] = '\0';
    }

    // Store uniform data
    program_data->uniforms[uniform_index].size = size;
    memcpy(program_data->uniforms[uniform_index].data, data, size);

    return true;
}

IMPLATFORM_API bool ImPlatform_SetShaderTexture(ImPlatform_ShaderProgram program, const char* name, unsigned int slot, ImTextureID texture)
{
    if (!program || !name)
        return false;

    ImPlatform_ShaderProgramData_GL* program_data = (ImPlatform_ShaderProgramData_GL*)program;
    GLint location = glGetUniformLocation(program_data->program_id, name);

    if (location == -1)
        return false; // Uniform not found

    // Activate texture unit and bind texture
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)texture);

    // Set the sampler uniform to the texture slot
    glUniform1i(location, slot);

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
        ImPlatform_ShaderProgramData_GL* program_data = (ImPlatform_ShaderProgramData_GL*)program;

        // Bind the shader program
        ImPlatform_BindShaderProgram(program);

        // Set the projection matrix uniform that ImGui uses
        // We need to use the viewport from the current draw data, not just io.DisplaySize
        ImDrawData* draw_data = ImGui::GetDrawData();
        if (!draw_data) return;

        float L = draw_data->DisplayPos.x;
        float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
        float T = draw_data->DisplayPos.y;
        float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

        const float ortho_projection[4][4] = {
            { 2.0f/(R-L),   0.0f,         0.0f,   0.0f },
            { 0.0f,         2.0f/(T-B),   0.0f,   0.0f },
            { 0.0f,         0.0f,        -1.0f,   0.0f },
            { (R+L)/(L-R),  (T+B)/(B-T),  0.0f,   1.0f },
        };

        // Apply projection matrix
        GLint proj_loc = glGetUniformLocation(program_data->program_id, "ProjMtx");
        if (proj_loc != -1)
            glUniformMatrix4fv(proj_loc, 1, GL_FALSE, &ortho_projection[0][0]);

        // Apply all stored uniforms
        for (int i = 0; i < program_data->uniform_count; i++)
        {
            ImPlatform_UniformData_GL* uniform = &program_data->uniforms[i];
            GLint location = glGetUniformLocation(program_data->program_id, uniform->name);

            if (location == -1)
                continue;

            unsigned int size = uniform->size;
            const float* data = uniform->data;

            if (size == sizeof(float) * 1)
                glUniform1fv_Ptr(location, 1, data);
            else if (size == sizeof(float) * 2)
                glUniform2fv_Ptr(location, 1, data);
            else if (size == sizeof(float) * 3)
                glUniform3fv_Ptr(location, 1, data);
            else if (size == sizeof(float) * 4)
                glUniform4fv_Ptr(location, 1, data);
            else if (size == sizeof(float) * 16)
                glUniformMatrix4fv(location, 1, GL_FALSE, data);
        }
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

#endif // IM_GFX_OPENGL3
