// dear imgui: Platform Abstraction - SDL2 Backend
// This handles SDL2 window creation, event handling, and the main loop

#include "ImPlatform_Internal.h"

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)

// Tell SDL we handle main() ourselves - prevents conflicts with SDL2main.lib
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif

#include "../imgui.h"
#include "../imgui/backends/imgui_impl_sdl2.h"
#include <SDL.h>
#include <SDL_syswm.h>

// Global state
static ImPlatform_AppData_SDL2 g_AppData = { 0 };

#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
// Hit test callback for borderless window dragging and resizing
static SDL_HitTestResult SDLCALL sdl2_hit_test_callback(SDL_Window* window, const SDL_Point* area, void* data)
{
    ImPlatform_AppData_SDL2* app = (ImPlatform_AppData_SDL2*)data;

    int w, h;
    SDL_GetWindowSize(window, &w, &h);

    // Check resize edges first (when not maximized and resize is allowed)
    if (g_BorderlessParams.allowResize && !(SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED))
    {
        const int border = g_BorderlessParams.resizeBorderSize;
        bool left = area->x < border;
        bool right = area->x >= w - border;
        bool top = area->y < border;
        bool bottom = area->y >= h - border;

        if (top && left) return SDL_HITTEST_RESIZE_TOPLEFT;
        if (top && right) return SDL_HITTEST_RESIZE_TOPRIGHT;
        if (bottom && left) return SDL_HITTEST_RESIZE_BOTTOMLEFT;
        if (bottom && right) return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
        if (left) return SDL_HITTEST_RESIZE_LEFT;
        if (right) return SDL_HITTEST_RESIZE_RIGHT;
        if (top) return SDL_HITTEST_RESIZE_TOP;
        if (bottom) return SDL_HITTEST_RESIZE_BOTTOM;
    }

    // Check titlebar drag
    if (g_BorderlessParams.allowMove && app->bTitleBarHovered)
        return SDL_HITTEST_DRAGGABLE;

    return SDL_HITTEST_NORMAL;
}
#endif

// Internal API - Get SDL2 window
SDL_Window* ImPlatform_App_GetSDL2Window(void)
{
    return g_AppData.pWindow;
}

// Internal API - Get SDL2 app data
ImPlatform_AppData_SDL2* ImPlatform_App_GetData_SDL2(void)
{
    return &g_AppData;
}

// Internal API - Get DPI scale
float ImPlatform_App_GetDpiScale_SDL2(void)
{
    return g_AppData.fDpiScale > 0.0f ? g_AppData.fDpiScale : 1.0f;
}

#ifdef _WIN32
// Internal API - Get native Windows HWND from SDL2 window
HWND ImPlatform_App_GetHWND(void)
{
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (SDL_GetWindowWMInfo(g_AppData.pWindow, &wmInfo))
    {
        return wmInfo.info.win.window;
    }
    return NULL;
}
#endif

// ImPlatform API - CreateWindow
IMPLATFORM_API bool ImPlatform_CreateWindow(char const* pWindowsName, ImVec2 const vPos, unsigned int uWidth, unsigned int uHeight)
{
#ifdef _WIN32
    ::SetProcessDPIAware();
#endif
    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        return false;
    }

    // From 2.0.18: Enable native IME
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

#if IM_CURRENT_GFX == IM_GFX_OPENGL3
    // Decide GL version (GLSL version is selected by the renderer backend)
#if defined(IMGUI_IMPL_OPENGL_ES2)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
#endif // IM_GFX_OPENGL3

    float main_scale = ImGui_ImplSDL2_GetContentScaleForDisplay(0);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(
#if IM_CURRENT_GFX == IM_GFX_OPENGL3
        SDL_WINDOW_OPENGL |
#elif IM_CURRENT_GFX == IM_GFX_VULKAN
        SDL_WINDOW_VULKAN |
#endif
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
    if (g_AppData.bCustomTitleBar)
        window_flags = (SDL_WindowFlags)(window_flags | SDL_WINDOW_BORDERLESS);
#endif

    int posX = (vPos.x == 0.0f) ? SDL_WINDOWPOS_CENTERED : (int)vPos.x;
    int posY = (vPos.y == 0.0f) ? SDL_WINDOWPOS_CENTERED : (int)vPos.y;

    g_AppData.pWindow = SDL_CreateWindow(
        pWindowsName,
        posX,
        posY,
        (int)(uWidth * main_scale),
        (int)(uHeight * main_scale),
        window_flags);

    if (g_AppData.pWindow == NULL)
    {
        SDL_Quit();
        return false;
    }

#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
    if (g_AppData.bCustomTitleBar)
    {
        SDL_SetWindowHitTest(g_AppData.pWindow, sdl2_hit_test_callback, &g_AppData);
        if (g_BorderlessParams.minWidth > 0 || g_BorderlessParams.minHeight > 0)
            SDL_SetWindowMinimumSize(g_AppData.pWindow, g_BorderlessParams.minWidth, g_BorderlessParams.minHeight);
    }
#endif

    g_AppData.fDpiScale = main_scale;

#if IM_CURRENT_GFX == IM_GFX_OPENGL3
    g_AppData.glContext = SDL_GL_CreateContext(g_AppData.pWindow);
    if (g_AppData.glContext == NULL)
    {
        SDL_DestroyWindow(g_AppData.pWindow);
        SDL_Quit();
        return false;
    }

    SDL_GL_MakeCurrent(g_AppData.pWindow, g_AppData.glContext);
    SDL_GL_SetSwapInterval(1); // Enable vsync
#endif

    return true;
}

// ImPlatform API - InitGfxAPI
// Note: This is called from the graphics backend for SDL

// ImPlatform API - ShowWindow
IMPLATFORM_API bool ImPlatform_ShowWindow(void)
{
    // SDL2 windows are shown by default
    return true;
}

// ImPlatform API - InitPlatform
IMPLATFORM_API bool ImPlatform_InitPlatform(void)
{
    // Init ImGui SDL2 backend
#if IM_CURRENT_GFX == IM_GFX_OPENGL3
    if (!ImGui_ImplSDL2_InitForOpenGL(g_AppData.pWindow, g_AppData.glContext))
        return false;
#elif IM_CURRENT_GFX == IM_GFX_VULKAN
    if (!ImGui_ImplSDL2_InitForVulkan(g_AppData.pWindow))
        return false;
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9) || (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || (IM_CURRENT_GFX == IM_GFX_DIRECTX11) || (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
    if (!ImGui_ImplSDL2_InitForD3D(g_AppData.pWindow))
        return false;
#elif IM_CURRENT_GFX == IM_GFX_METAL
    if (!ImGui_ImplSDL2_InitForMetal(g_AppData.pWindow))
        return false;
#else
    if (!ImGui_ImplSDL2_InitForOther(g_AppData.pWindow))
        return false;
#endif

    return true;
}

// ImPlatform API - PlatformContinue
IMPLATFORM_API bool ImPlatform_PlatformContinue(void)
{
    return !g_AppData.bDone;
}

// ImPlatform API - PlatformEvents
IMPLATFORM_API bool ImPlatform_PlatformEvents(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT)
        {
            g_AppData.bDone = true;
            return false;
        }
        if (event.type == SDL_WINDOWEVENT &&
            event.window.event == SDL_WINDOWEVENT_CLOSE &&
            event.window.windowID == SDL_GetWindowID(g_AppData.pWindow))
        {
            g_AppData.bDone = true;
            return false;
        }
#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
        // Notify gfx backend of resize (same as upstream example_sdl2_directx11)
        if (event.type == SDL_WINDOWEVENT &&
            event.window.event == SDL_WINDOWEVENT_RESIZED &&
            event.window.windowID == SDL_GetWindowID(g_AppData.pWindow))
        {
            ImPlatform_Gfx_OnResize_DX11(ImPlatform_Gfx_GetData_DX11(), (unsigned int)event.window.data1, (unsigned int)event.window.data2);
        }
#endif
#if IMPLATFORM_APP_SUPPORT_DROP_FILE
        if (event.type == SDL_DROPFILE && event.drop.file)
        {
            int mx = 0, my = 0;
            SDL_GetMouseState(&mx, &my);
            ImPlatform_NotifyFileDrop(event.drop.file, ImVec2((float)mx, (float)my));
            SDL_free(event.drop.file);
        }
#endif
    }

    // Skip rendering when minimized
    if (SDL_GetWindowFlags(g_AppData.pWindow) & SDL_WINDOW_MINIMIZED)
    {
        SDL_Delay(10);
        return false; // Skip frame
    }

    return true;
}

// ImPlatform API - PlatformNewFrame
IMPLATFORM_API void ImPlatform_PlatformNewFrame(void)
{
    ImGui_ImplSDL2_NewFrame();
}

// ImPlatform API - ShutdownPlatform
IMPLATFORM_API void ImPlatform_ShutdownPlatform(void)
{
    // Note: This is kept for backwards compatibility
    // New code should use ShutdownPostGfxAPI + DestroyWindow
}

// ImPlatform API - ShutdownPostGfxAPI
IMPLATFORM_API void ImPlatform_ShutdownPostGfxAPI(void)
{
    ImGui_ImplSDL2_Shutdown();
}

// ImPlatform API - DestroyWindow
IMPLATFORM_API void ImPlatform_DestroyWindow(void)
{
#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_OPENGL3)
    if (g_AppData.glContext)
    {
        SDL_GL_DeleteContext(g_AppData.glContext);
        g_AppData.glContext = NULL;
    }
#endif

    if (g_AppData.pWindow)
    {
        SDL_DestroyWindow(g_AppData.pWindow);
        g_AppData.pWindow = NULL;
    }

    SDL_Quit();
}

#if IMPLATFORM_APP_SUPPORT_DROP_FILE
void ImPlatform_App_OnDropFileCallbackChanged(bool has_callback)
{
    SDL_EventState(SDL_DROPFILE, has_callback ? SDL_ENABLE : SDL_DISABLE);
}
#endif // IMPLATFORM_APP_SUPPORT_DROP_FILE

#endif // IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2
