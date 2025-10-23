// dear imgui: Graphics API Abstraction - OpenGL 3 Backend
// This handles OpenGL context creation, rendering, and presentation

#include "ImPlatform_Internal.h"

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_OPENGL3)

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

    // Store main window HDC for viewport system
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
    ImPlatform_Gfx_CreateDevice_OpenGL3((HWND)viewport->PlatformHandle, &g_GfxData);
    data->hDC = g_GfxData.hDC;
    viewport->RendererUserData = data;
}

static void Hook_Renderer_DestroyWindow(ImGuiViewport* viewport)
{
    if (viewport->RendererUserData != NULL)
    {
        WGL_WindowData* data = (WGL_WindowData*)viewport->RendererUserData;
        ImPlatform_Gfx_CleanupDevice_OpenGL3((HWND)viewport->PlatformHandle, &g_GfxData);
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

#endif // IM_GFX_OPENGL3
