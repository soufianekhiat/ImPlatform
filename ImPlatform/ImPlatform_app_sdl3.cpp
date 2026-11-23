// dear imgui: Platform Abstraction - SDL3 Backend
// This handles SDL3 window creation, event handling, and the main loop

#include "ImPlatform_Internal.h"

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)

// Tell SDL we handle main() ourselves - prevents conflicts with SDL3main
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif

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

    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    if (main_scale <= 0.0f)
        main_scale = 1.0f;
    SDL_WindowFlags window_flags =
#if IM_CURRENT_GFX == IM_GFX_OPENGL3
        SDL_WINDOW_OPENGL |
#elif IM_CURRENT_GFX == IM_GFX_VULKAN
        SDL_WINDOW_VULKAN |
#elif IM_CURRENT_GFX == IM_GFX_METAL
        SDL_WINDOW_METAL |
#endif
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;

#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
    if (g_AppData.bCustomTitleBar)
        window_flags |= SDL_WINDOW_BORDERLESS;
#endif

    g_AppData.pWindow = SDL_CreateWindow(
        pWindowsName,
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
        SDL_SetWindowHitTest(g_AppData.pWindow, sdl3_hit_test_callback, &g_AppData);
        if (g_BorderlessParams.minWidth > 0 || g_BorderlessParams.minHeight > 0)
            SDL_SetWindowMinimumSize(g_AppData.pWindow, g_BorderlessParams.minWidth, g_BorderlessParams.minHeight);
    }
#endif

    // SDL3 requires setting position after window creation
    int posX = (vPos.x == 0.0f) ? SDL_WINDOWPOS_CENTERED : (int)vPos.x;
    int posY = (vPos.y == 0.0f) ? SDL_WINDOWPOS_CENTERED : (int)vPos.y;
    SDL_SetWindowPosition(g_AppData.pWindow, posX, posY);

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
    SDL_ShowWindow(g_AppData.pWindow);
    return true;
}

// ImPlatform API - InitPlatform
IMPLATFORM_API bool ImPlatform_InitPlatform(void)
{
    // Init ImGui SDL3 backend
#if IM_CURRENT_GFX == IM_GFX_OPENGL3
    if (!ImGui_ImplSDL3_InitForOpenGL(g_AppData.pWindow, g_AppData.glContext))
        return false;
#elif IM_CURRENT_GFX == IM_GFX_VULKAN
    if (!ImGui_ImplSDL3_InitForVulkan(g_AppData.pWindow))
        return false;
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9) || (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || (IM_CURRENT_GFX == IM_GFX_DIRECTX11) || (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
    if (!ImGui_ImplSDL3_InitForD3D(g_AppData.pWindow))
        return false;
#elif IM_CURRENT_GFX == IM_GFX_METAL
    if (!ImGui_ImplSDL3_InitForMetal(g_AppData.pWindow))
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

// Pen events of the main window, one sample per event: SDL reports the position (motion, touch) and each axis in its
// own event, so the sample carries the last value of the others. Observed only: SDL also sends the pen as a mouse.
static void ImPlatform_SDL3_PenEvent(const SDL_Event& event)
{
    static ImPlatform_PenSample pen = { 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0, false, false, false, false, 0.0f };
    SDL_PenInputFlags state = 0;
    switch (event.type)
    {
    case SDL_EVENT_PEN_PROXIMITY_IN:
        pen.pressure = -1.0f;       // Until the pen reports each axis
        pen.has_tilt = pen.has_rotation = false;
        return;
    case SDL_EVENT_PEN_PROXIMITY_OUT:
        ImPlatform_Pen_Leave();
        return;
    case SDL_EVENT_PEN_MOTION:
        if (event.pmotion.windowID != SDL_GetWindowID(g_AppData.pWindow))
            return;
        pen.x = event.pmotion.x;
        pen.y = event.pmotion.y;
        state = event.pmotion.pen_state;
        break;
    case SDL_EVENT_PEN_DOWN:
    case SDL_EVENT_PEN_UP:
        if (event.ptouch.windowID != SDL_GetWindowID(g_AppData.pWindow))
            return;
        pen.x = event.ptouch.x;
        pen.y = event.ptouch.y;
        state = event.ptouch.pen_state;
        break;
    case SDL_EVENT_PEN_AXIS:
        if (event.paxis.windowID != SDL_GetWindowID(g_AppData.pWindow))
            return;
        pen.x = event.paxis.x;
        pen.y = event.paxis.y;
        state = event.paxis.pen_state;
        switch (event.paxis.axis)
        {
        case SDL_PEN_AXIS_PRESSURE: pen.pressure = event.paxis.value; break;
        case SDL_PEN_AXIS_XTILT:    pen.tilt_x = event.paxis.value; pen.has_tilt = true; break;
        case SDL_PEN_AXIS_YTILT:    pen.tilt_y = event.paxis.value; pen.has_tilt = true; break;
        case SDL_PEN_AXIS_ROTATION: // SDL: [-180, 180) clockwise from up
            pen.rotation = event.paxis.value < 0.0f ? event.paxis.value + 360.0f : event.paxis.value;
            pen.has_rotation = true;
            break;
        default: return;            // Distance, slider, barrel pressure: not part of a sample
        }
        break;
    default:
        return;
    }
    pen.in_contact = (state & SDL_PEN_INPUT_DOWN) != 0;
    pen.eraser = (state & SDL_PEN_INPUT_ERASER_TIP) != 0;
    pen.time = (double)event.common.timestamp * 1e-9;   // SDL_GetTicksNS, the clock of ImPlatform_GetPenClock
    ImPlatform_Pen_Push(&pen);
}

// ImPlatform API - PlatformEvents
IMPLATFORM_API bool ImPlatform_PlatformEvents(void)
{
    SDL_Event event;
    ImPlatform_Native_Pump();
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type >= SDL_EVENT_PEN_PROXIMITY_IN && event.type <= SDL_EVENT_PEN_AXIS)
            ImPlatform_SDL3_PenEvent(event);
        if (event.type >= SDL_EVENT_FINGER_DOWN && event.type <= SDL_EVENT_FINGER_CANCELED)
        {
            // Fingers on the main window; SDL gives their position as a fraction of it. One that goes to another
            // window counts as lifted. Observed only: SDL also sends the first finger as a mouse.
            int w = 0, h = 0;
            SDL_GetWindowSize(g_AppData.pWindow, &w, &h);
            ImPlatform_Touch_Set((unsigned long long)event.tfinger.fingerID, event.tfinger.x * (float)w, event.tfinger.y * (float)h, event.tfinger.pressure,
                                 (event.type == SDL_EVENT_FINGER_DOWN || event.type == SDL_EVENT_FINGER_MOTION) &&
                                 event.tfinger.windowID == SDL_GetWindowID(g_AppData.pWindow));
        }
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
#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
        // Notify gfx backend of resize (same as upstream example_sdl3_directx11)
        if (event.type == SDL_EVENT_WINDOW_RESIZED &&
            event.window.windowID == SDL_GetWindowID(g_AppData.pWindow))
        {
            int fb_w = 0, fb_h = 0;
            SDL_GetWindowSizeInPixels(g_AppData.pWindow, &fb_w, &fb_h);
            ImPlatform_Gfx_OnResize_DX11(ImPlatform_Gfx_GetData_DX11(), (unsigned int)fb_w, (unsigned int)fb_h);
        }
#elif defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_CUSTOM)
        // The application's backend owns its swap chain: tell it the window's new size in pixels.
        if (event.type == SDL_EVENT_WINDOW_RESIZED &&
            event.window.windowID == SDL_GetWindowID(g_AppData.pWindow))
        {
            int fb_w = 0, fb_h = 0;
            SDL_GetWindowSizeInPixels(g_AppData.pWindow, &fb_w, &fb_h);
            ImPlatform_Gfx_OnResize_Custom((unsigned int)fb_w, (unsigned int)fb_h);
        }
#endif
#if SDL_VERSION_ATLEAST(3, 4, 0)
        if (event.type == SDL_EVENT_PINCH_UPDATE)
            ImPlatform_Touch_Zoom(event.pinch.scale);       // A trackpad's pinch
#endif
#if IMPLATFORM_APP_SUPPORT_DROP_FILE
        if (event.type >= SDL_EVENT_DROP_FILE && event.type <= SDL_EVENT_DROP_POSITION)
        {
            // SDL gives the position in the window the drag is over (not with the begin event)
            const ImPlatform_DropEventType type = event.type == SDL_EVENT_DROP_BEGIN ? ImPlatform_DropEvent_Begin
                : event.type == SDL_EVENT_DROP_POSITION ? ImPlatform_DropEvent_Position
                : event.type == SDL_EVENT_DROP_FILE ? ImPlatform_DropEvent_File
                : event.type == SDL_EVENT_DROP_TEXT ? ImPlatform_DropEvent_Text : ImPlatform_DropEvent_End;
            const bool item = type == ImPlatform_DropEvent_File || type == ImPlatform_DropEvent_Text;
            if (!item || event.drop.data)
                ImPlatform_NotifyDrop(type, item ? event.drop.data : NULL, ImVec2(event.drop.x, event.drop.y));
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

#if IMPLATFORM_APP_SUPPORT_DROP_FILE
void ImPlatform_App_OnDropFileCallbackChanged(bool has_callback)
{
    for (Uint32 type = SDL_EVENT_DROP_FILE; type <= SDL_EVENT_DROP_POSITION; ++type)
        SDL_SetEventEnabled(type, has_callback);
}
#endif // IMPLATFORM_APP_SUPPORT_DROP_FILE

#if IMPLATFORM_APP_SUPPORT_WINDOW_STATE
IMPLATFORM_API void ImPlatform_SetFullscreen(bool fullscreen)
{
    SDL_SetWindowFullscreen(g_AppData.pWindow, fullscreen);     // No mode was set: the desktop's, borderless
}

IMPLATFORM_API bool ImPlatform_IsFullscreen(void)
{
    return (SDL_GetWindowFlags(g_AppData.pWindow) & SDL_WINDOW_FULLSCREEN) != 0;
}

IMPLATFORM_API void ImPlatform_SetAlwaysOnTop(bool on_top)
{
    SDL_SetWindowAlwaysOnTop(g_AppData.pWindow, on_top);
}

IMPLATFORM_API bool ImPlatform_SetWindowIcon(const unsigned char* rgba, int width, int height)
{
    SDL_Surface* icon = rgba ? SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, const_cast<unsigned char*>(rgba), width * 4) : NULL;
    const bool ok = icon && SDL_SetWindowIcon(g_AppData.pWindow, icon);
    SDL_DestroySurface(icon);
    return ok;
}
#endif // IMPLATFORM_APP_SUPPORT_WINDOW_STATE

#endif // IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3
