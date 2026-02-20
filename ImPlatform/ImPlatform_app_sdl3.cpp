// dear imgui: Platform Abstraction - SDL3 Backend
// This handles SDL3 window creation, event handling, and the main loop

#include "ImPlatform_Internal.h"

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)

// Tell SDL we handle main() ourselves - prevents conflicts with SDL3main
#define SDL_MAIN_HANDLED

#include "../imgui.h"
#include "../imgui/backends/imgui_impl_sdl3.h"
#include <SDL3/SDL.h>

// Global state
static ImPlatform_AppData_SDL3 g_AppData = { 0 };

#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
// Hit test callback for borderless window dragging and resizing
static SDL_HitTestResult SDLCALL sdl3_hit_test_callback(SDL_Window* window, const SDL_Point* area, void* data)
{
    ImPlatform_AppData_SDL3* app = (ImPlatform_AppData_SDL3*)data;

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

// Internal API - Get SDL3 window
SDL_Window* ImPlatform_App_GetSDL3Window(void)
{
    return g_AppData.pWindow;
}

// Internal API - Get SDL3 app data
ImPlatform_AppData_SDL3* ImPlatform_App_GetData_SDL3(void)
{
    return &g_AppData;
}

// Internal API - Get DPI scale
float ImPlatform_App_GetDpiScale_SDL3(void)
{
    return g_AppData.fDpiScale > 0.0f ? g_AppData.fDpiScale : 1.0f;
}

#ifdef _WIN32
// Internal API - Get native Windows HWND from SDL3 window
HWND ImPlatform_App_GetHWND(void)
{
    // SDL3 uses SDL_GetPointerProperty to get native window handles
    SDL_PropertiesID props = SDL_GetWindowProperties(g_AppData.pWindow);
    return (HWND)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
}
#endif

// ImPlatform API - CreateWindow
IMPLATFORM_API bool ImPlatform_CreateWindow(char const* pWindowsName, ImVec2 const vPos, unsigned int uWidth, unsigned int uHeight)
{
    // Setup SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        return false;
    }

#ifdef IM_GFX_OPENGL3
    // Decide GL+GLSL versions
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

    Uint32 window_flags =
#ifdef IM_GFX_OPENGL3
        SDL_WINDOW_OPENGL |
#endif
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;

#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
    if (g_AppData.bCustomTitleBar)
        window_flags |= SDL_WINDOW_BORDERLESS;
#endif

    g_AppData.pWindow = SDL_CreateWindow(
        pWindowsName,
        (int)uWidth,
        (int)uHeight,
        window_flags);

    if (g_AppData.pWindow == NULL)
    {
        SDL_Quit();
        return false;
    }

#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
    if (g_AppData.bCustomTitleBar)
    {
        SDL_SetWindowHitTest(g_AppData.pWindow, sdl3_hit_test_callback, &g_AppData);
        if (g_BorderlessParams.minWidth > 0 || g_BorderlessParams.minHeight > 0)
            SDL_SetWindowMinimumSize(g_AppData.pWindow, g_BorderlessParams.minWidth, g_BorderlessParams.minHeight);
    }
#endif

    // SDL3 requires setting position after window creation
    int posX = (vPos.x == 0.0f) ? SDL_WINDOWPOS_CENTERED : (int)vPos.x;
    int posY = (vPos.y == 0.0f) ? SDL_WINDOWPOS_CENTERED : (int)vPos.y;
    SDL_SetWindowPosition(g_AppData.pWindow, posX, posY);

    // Query DPI scale
    {
        float scale = SDL_GetWindowDisplayScale(g_AppData.pWindow);
        g_AppData.fDpiScale = (scale > 0.0f) ? scale : 1.0f;
    }

#ifdef IM_GFX_OPENGL3
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
    SDL_ShowWindow(g_AppData.pWindow);
    return true;
}

// ImPlatform API - InitPlatform
IMPLATFORM_API bool ImPlatform_InitPlatform(void)
{
    // Init ImGui SDL3 backend
#ifdef IM_GFX_OPENGL3
    if (!ImGui_ImplSDL3_InitForOpenGL(g_AppData.pWindow, g_AppData.glContext))
        return false;
#elif defined(IM_GFX_VULKAN)
    if (!ImGui_ImplSDL3_InitForVulkan(g_AppData.pWindow))
        return false;
#else
    if (!ImGui_ImplSDL3_InitForOther(g_AppData.pWindow))
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
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT)
        {
            g_AppData.bDone = true;
            return false;
        }
        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
            event.window.windowID == SDL_GetWindowID(g_AppData.pWindow))
        {
            g_AppData.bDone = true;
            return false;
        }
    }

    // Skip rendering when minimized
    if (SDL_GetWindowFlags(g_AppData.pWindow) & SDL_WINDOW_MINIMIZED)
    {
        SDL_Delay(10);
        return true; // Continue but don't render this frame
    }

    return true;
}

// ImPlatform API - PlatformNewFrame
IMPLATFORM_API void ImPlatform_PlatformNewFrame(void)
{
    ImGui_ImplSDL3_NewFrame();
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
    ImGui_ImplSDL3_Shutdown();
}

// ImPlatform API - DestroyWindow
IMPLATFORM_API void ImPlatform_DestroyWindow(void)
{
#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_OPENGL3)
    if (g_AppData.glContext)
    {
        SDL_GL_DestroyContext(g_AppData.glContext);
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

#endif // IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3
