// dear imgui: Platform Abstraction - SDL2 Backend
// This handles SDL2 window creation, event handling, and the main loop

#include "ImPlatform_Internal.h"

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)

// Tell SDL we handle main() ourselves - prevents conflicts with SDL2main.lib
#define SDL_MAIN_HANDLED

#include "../imgui.h"
#include "../imgui/backends/imgui_impl_sdl2.h"
#include <SDL.h>
#include <SDL_syswm.h>

// Global state
static ImPlatform_AppData_SDL2 g_AppData = { 0 };

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
    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
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

    // From 2.0.18: Enable native IME
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
#endif // IM_GFX_OPENGL3

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(
#ifdef IM_GFX_OPENGL3
        SDL_WINDOW_OPENGL |
#endif
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    int posX = (vPos.x == 0.0f) ? SDL_WINDOWPOS_CENTERED : (int)vPos.x;
    int posY = (vPos.y == 0.0f) ? SDL_WINDOWPOS_CENTERED : (int)vPos.y;

    g_AppData.pWindow = SDL_CreateWindow(
        pWindowsName,
        posX,
        posY,
        (int)uWidth,
        (int)uHeight,
        window_flags);

    if (g_AppData.pWindow == NULL)
    {
        SDL_Quit();
        return false;
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
    // SDL2 windows are shown by default
    return true;
}

// ImPlatform API - InitPlatform
IMPLATFORM_API bool ImPlatform_InitPlatform(void)
{
    // Init ImGui SDL2 backend
#ifdef IM_GFX_OPENGL3
    if (!ImGui_ImplSDL2_InitForOpenGL(g_AppData.pWindow, g_AppData.glContext))
        return false;
#elif defined(IM_GFX_VULKAN)
    if (!ImGui_ImplSDL2_InitForVulkan(g_AppData.pWindow))
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
    return true; // SDL2 doesn't track a "done" state in the platform layer
}

// ImPlatform API - PlatformEvents
IMPLATFORM_API bool ImPlatform_PlatformEvents(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT)
            return false;
        if (event.type == SDL_WINDOWEVENT &&
            event.window.event == SDL_WINDOWEVENT_CLOSE &&
            event.window.windowID == SDL_GetWindowID(g_AppData.pWindow))
            return false;
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

#endif // IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2
