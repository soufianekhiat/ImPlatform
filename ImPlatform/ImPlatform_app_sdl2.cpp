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

    if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN)
        return SDL_HITTEST_NORMAL;      // No border to resize by, no titlebar to move by

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

#if defined(__linux__)
unsigned long ImPlatform_App_GetX11Window(void)
{
#if defined(SDL_VIDEO_DRIVER_X11)
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (g_AppData.pWindow && SDL_GetWindowWMInfo(g_AppData.pWindow, &info) && info.subsystem == SDL_SYSWM_X11)
        return info.info.x11.window;
#endif
    return 0;
}
#endif

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
    ImPlatform_Native_Pump();
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_FINGERDOWN || event.type == SDL_FINGERUP || event.type == SDL_FINGERMOTION)
        {
            // Fingers on the main window; SDL gives their position as a fraction of it. One that goes to another
            // window counts as lifted. Observed only: SDL also sends the first finger as a mouse.
            int w = 0, h = 0;
            SDL_GetWindowSize(g_AppData.pWindow, &w, &h);
            ImPlatform_Touch_Set((unsigned long long)event.tfinger.fingerId, event.tfinger.x * (float)w, event.tfinger.y * (float)h, event.tfinger.pressure,
                                 event.type != SDL_FINGERUP && event.tfinger.windowID == SDL_GetWindowID(g_AppData.pWindow));
        }
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
#if SDL_VERSION_ATLEAST(2, 0, 18)
        // The window moved to a display of another scale
        if (event.type == SDL_WINDOWEVENT &&
            event.window.event == SDL_WINDOWEVENT_DISPLAY_CHANGED &&
            event.window.windowID == SDL_GetWindowID(g_AppData.pWindow))
        {
            const float scale = ImGui_ImplSDL2_GetContentScaleForWindow(g_AppData.pWindow);
            if (scale > 0.0f && scale != g_AppData.fDpiScale)
            {
                g_AppData.fDpiScale = scale;
                ImPlatform_NotifyDpiChange(scale);
            }
        }
#endif
#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
        // Notify gfx backend of resize. SIZE_CHANGED comes for every change of size; RESIZED, which upstream's
        // example_sdl2_directx11 reads, only for those made by the user or the system, not for SDL_SetWindowSize.
        if (event.type == SDL_WINDOWEVENT &&
            event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED &&
            event.window.windowID == SDL_GetWindowID(g_AppData.pWindow))
        {
            ImPlatform_Gfx_OnResize_DX11(ImPlatform_Gfx_GetData_DX11(), (unsigned int)event.window.data1, (unsigned int)event.window.data2);
        }
#elif defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_CUSTOM)
        // The application's backend owns its swap chain: tell it the window's new size.
        if (event.type == SDL_WINDOWEVENT &&
            event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED &&
            event.window.windowID == SDL_GetWindowID(g_AppData.pWindow))
        {
            ImPlatform_Gfx_OnResize_Custom((unsigned int)event.window.data1, (unsigned int)event.window.data2);
        }
#endif
#if IMPLATFORM_APP_SUPPORT_DROP_FILE
        if (event.type >= SDL_DROPFILE && event.type <= SDL_DROPCOMPLETE)
        {
            // SDL2 reports a drop at the release, without a position: the mouse's
            int mx = 0, my = 0;
            SDL_GetMouseState(&mx, &my);
            const ImPlatform_DropEventType type = event.type == SDL_DROPBEGIN ? ImPlatform_DropEvent_Begin
                : event.type == SDL_DROPFILE ? ImPlatform_DropEvent_File
                : event.type == SDL_DROPTEXT ? ImPlatform_DropEvent_Text : ImPlatform_DropEvent_End;
            const bool item = type == ImPlatform_DropEvent_File || type == ImPlatform_DropEvent_Text;
            if (!item || event.drop.file)
                ImPlatform_NotifyDrop(type, item ? event.drop.file : NULL, ImVec2((float)mx, (float)my));
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

    ImPlatform_Native_Shutdown();
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
    for (Uint32 type = SDL_DROPFILE; type <= SDL_DROPCOMPLETE; ++type)
        SDL_EventState(type, has_callback ? SDL_ENABLE : SDL_DISABLE);
    // Windows and X11: the system's own drag protocol is followed for the window (ImPlatform_native.cpp), which
    // reports the drag while it hovers. SDL2's events, at the release, stay for where that is not possible.
    ImPlatform_Native_AcceptDrops(has_callback);
}
#endif // IMPLATFORM_APP_SUPPORT_DROP_FILE

#if IMPLATFORM_APP_SUPPORT_WINDOW_STATE
IMPLATFORM_API void ImPlatform_SetFullscreen(bool fullscreen)
{
    SDL_SetWindowFullscreen(g_AppData.pWindow, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

IMPLATFORM_API bool ImPlatform_IsFullscreen(void)
{
    return (SDL_GetWindowFlags(g_AppData.pWindow) & SDL_WINDOW_FULLSCREEN) != 0;
}

IMPLATFORM_API void ImPlatform_SetAlwaysOnTop(bool on_top)
{
    SDL_SetWindowAlwaysOnTop(g_AppData.pWindow, on_top ? SDL_TRUE : SDL_FALSE);
}

IMPLATFORM_API bool ImPlatform_SetWindowIcon(const unsigned char* rgba, int width, int height)
{
    SDL_Surface* icon = rgba ? SDL_CreateRGBSurfaceWithFormatFrom(const_cast<unsigned char*>(rgba), width, height, 32, width * 4, SDL_PIXELFORMAT_RGBA32) : NULL;
    if (icon)
        SDL_SetWindowIcon(g_AppData.pWindow, icon);
    SDL_FreeSurface(icon);
    return icon != NULL;
}
#endif // IMPLATFORM_APP_SUPPORT_WINDOW_STATE

#endif // IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2
