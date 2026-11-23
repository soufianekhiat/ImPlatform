// dear imgui: Platform Abstraction - GLFW Application Backend
// This handles window creation, message loop, and input for GLFW

#include "ImPlatform_Internal.h"

#include <stdio.h>
#include <string.h>

#ifdef IM_PLATFORM_GLFW

#include "../imgui/backends/imgui_impl_glfw.h"
#include <GLFW/glfw3.h>

// Global state
static ImPlatform_AppData_GLFW g_AppData = { 0 };

// Error callback
static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// DPI change callback (GLFW 3.3+)
#if GLFW_VERSION_MAJOR > 3 || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 3)
static void glfw_content_scale_callback(GLFWwindow* window, float xscale, float yscale)
{
    (void)window;
    (void)yscale;
    g_AppData.fDpiScale = xscale;
    ImPlatform_NotifyDpiChange(xscale);
}
#endif

#if IMPLATFORM_APP_SUPPORT_DROP_FILE
// File drop callback: paths are already UTF-8, position taken from current cursor
static void glfw_drop_callback(GLFWwindow* window, int count, const char** paths)
{
    double cx = 0.0, cy = 0.0;
    glfwGetCursorPos(window, &cx, &cy);
    ImVec2 pos((float)cx, (float)cy);
    for (int i = 0; i < count; ++i)
        ImPlatform_NotifyFileDrop(paths[i], pos);
}
#endif

// ImPlatform API - CreateWindow
IMPLATFORM_API bool ImPlatform_CreateWindow(char const* pWindowsName, ImVec2 const vPos, unsigned int uWidth, unsigned int uHeight)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return false;

    // Decide GL+GLSL versions (only for OpenGL backends)
#if IM_CURRENT_GFX == IM_GFX_OPENGL3
    #if defined(IMGUI_IMPL_OPENGL_ES2)
        // GL ES 2.0 + GLSL 100 (WebGL 1.0)
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    #elif defined(IMGUI_IMPL_OPENGL_ES3)
        // GL ES 3.0 + GLSL 300 es (WebGL 2.0)
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    #elif defined(__APPLE__)
        // GL 3.2 + GLSL 150
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
    #else
        // GL 3.0 + GLSL 130
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    #endif
#elif (IM_CURRENT_GFX == IM_GFX_VULKAN) || (IM_CURRENT_GFX == IM_GFX_WGPU) || (IM_CURRENT_GFX == IM_GFX_METAL) || (IM_CURRENT_GFX == IM_GFX_CUSTOM)
    // Vulkan/WebGPU/Metal/the application's own backend don't use an OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#endif

#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR && !defined(__EMSCRIPTEN__)
    // Custom titlebar support (not applicable on web)
    if (g_AppData.bCustomTitleBar)
    {
#ifdef IM_THE_CHERNO_GLFW3
        // TheCherno's GLFW fork: native titlebar removal with hit-test support
        glfwWindowHint(GLFW_TITLEBAR, false);
#else
        // Standard GLFW: remove window decorations for borderless
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
#endif
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    }
#endif

    // Query DPI scale (same helper as upstream examples: consistent with the monitor DpiScale reported by the backend,
    // handles legacy GLFW < 3.3 on Windows and returns 1.0f on Apple platforms which use FramebufferScale instead)
    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    g_AppData.fDpiScale = main_scale;

    // Create window with graphics context
    g_AppData.pWindow = glfwCreateWindow((int)(uWidth * main_scale), (int)(uHeight * main_scale), pWindowsName, NULL, NULL);
    if (g_AppData.pWindow == NULL)
        return false;

#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
    // Setup custom titlebar hit-test callback
    if (g_AppData.bCustomTitleBar)
    {
        glfwSetWindowUserPointer(g_AppData.pWindow, &g_AppData);
#ifdef IM_THE_CHERNO_GLFW3
        glfwSetTitlebarHitTestCallback(g_AppData.pWindow,
            [](GLFWwindow* window, int x, int y, int* hit) {
                ImPlatform_AppData_GLFW* app = (ImPlatform_AppData_GLFW*)glfwGetWindowUserPointer(window);
                *hit = app->bTitleBarHovered;
            });
#endif
    }
#endif

#ifndef __EMSCRIPTEN__
    // Set window position (GLFW doesn't support this in creation, so we set it after)
    glfwSetWindowPos(g_AppData.pWindow, (int)vPos.x, (int)vPos.y);
#endif

#if IMPLATFORM_APP_SUPPORT_DROP_FILE
    if (g_AppData.bDropFileEnabled)
        glfwSetDropCallback(g_AppData.pWindow, glfw_drop_callback);
#endif

    // Register callback for runtime changes (GLFW 3.3+)
#if GLFW_VERSION_MAJOR > 3 || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 3)
    glfwSetWindowContentScaleCallback(g_AppData.pWindow, glfw_content_scale_callback);
#endif

#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
    if (g_AppData.bCustomTitleBar && (g_BorderlessParams.minWidth > 0 || g_BorderlessParams.minHeight > 0))
    {
        int minW = g_BorderlessParams.minWidth > 0 ? g_BorderlessParams.minWidth : GLFW_DONT_CARE;
        int minH = g_BorderlessParams.minHeight > 0 ? g_BorderlessParams.minHeight : GLFW_DONT_CARE;
        glfwSetWindowSizeLimits(g_AppData.pWindow, minW, minH, GLFW_DONT_CARE, GLFW_DONT_CARE);
    }
#endif

#if IM_CURRENT_GFX == IM_GFX_OPENGL3
    glfwMakeContextCurrent(g_AppData.pWindow);
    glfwSwapInterval(1); // Enable vsync
#elif IM_CURRENT_GFX == IM_GFX_CUSTOM
    // The application's backend owns its swap chain: tell it the framebuffer's new size.
    // (imgui_impl_glfw installs no framebuffer-size callback, so this chains nothing.)
    glfwSetFramebufferSizeCallback(g_AppData.pWindow, [](GLFWwindow*, int w, int h)
    {
        ImPlatform_Gfx_OnResize_Custom((unsigned int)w, (unsigned int)h);
    });
#endif

    return true;
}

// ImPlatform API - ShowWindow
IMPLATFORM_API bool ImPlatform_ShowWindow(void)
{
    // GLFW windows are visible by default
    glfwShowWindow(g_AppData.pWindow);
    return true;
}

// ImPlatform API - InitPlatform
IMPLATFORM_API bool ImPlatform_InitPlatform(void)
{
    bool result;
#if IM_CURRENT_GFX == IM_GFX_OPENGL3
    result = ImGui_ImplGlfw_InitForOpenGL(g_AppData.pWindow, true);
#elif IM_CURRENT_GFX == IM_GFX_VULKAN
    result = ImGui_ImplGlfw_InitForVulkan(g_AppData.pWindow, true);
#else
    result = ImGui_ImplGlfw_InitForOther(g_AppData.pWindow, true);
#endif

#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(g_AppData.pWindow, "#canvas");
#endif

    return result;
}

// ImPlatform API - PlatformContinue
IMPLATFORM_API bool ImPlatform_PlatformContinue(void)
{
    return !glfwWindowShouldClose(g_AppData.pWindow);
}

// ImPlatform API - PlatformEvents
IMPLATFORM_API bool ImPlatform_PlatformEvents(void)
{
    glfwPollEvents();

#ifndef __EMSCRIPTEN__
    // Handle minimize
    if (glfwGetWindowAttrib(g_AppData.pWindow, GLFW_ICONIFIED) != 0)
    {
        ImGui_ImplGlfw_Sleep(10);
        return false; // Skip frame
    }
#endif

#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR && !defined(IM_THE_CHERNO_GLFW3)
    // Software move and resize of the borderless window (stock GLFW has no hit-testing; TheCherno's fork does): a press
    // on a resize border or on the titlebar starts a drag that lasts until the button is released.
    if (g_AppData.bCustomTitleBar)
    {
        GLFWwindow* window = g_AppData.pWindow;
        const bool down = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        int win[4];
        double cx, cy;
        glfwGetWindowPos(window, &win[0], &win[1]);
        glfwGetWindowSize(window, &win[2], &win[3]);
        glfwGetCursorPos(window, &cx, &cy);        // Relative to the window
        const int border = g_BorderlessParams.resizeBorderSize;
        int edges = 0;
        if (g_BorderlessParams.allowResize && !glfwGetWindowAttrib(window, GLFW_MAXIMIZED)
            && cx >= 0 && cy >= 0 && cx < win[2] && cy < win[3])
            edges = (cx < border ? 1 : 0) | (cx >= win[2] - border ? 2 : 0) | (cy < border ? 4 : 0) | (cy >= win[3] - border ? 8 : 0);
        // Screen coordinates: they do not change when the window moves under a still cursor.
        const double sx = win[0] + cx, sy = win[1] + cy;
        if (!down)
            g_AppData.iDragEdges = 0;
        else if (!g_AppData.bMouseWasDown)
        {
            g_AppData.iDragEdges = edges ? edges : (g_AppData.bTitleBarHovered && g_BorderlessParams.allowMove) ? 16 : 0;
            g_AppData.fDragStartX = sx;
            g_AppData.fDragStartY = sy;
            memcpy(g_AppData.iWinStart, win, sizeof(win));
        }
        else if (g_AppData.iDragEdges)
        {
            int rect[4];
            ImPlatform_DragRect(g_AppData.iDragEdges, g_AppData.iWinStart, (int)(sx - g_AppData.fDragStartX), (int)(sy - g_AppData.fDragStartY),
                                g_BorderlessParams.minWidth > 1 ? g_BorderlessParams.minWidth : 1,
                                g_BorderlessParams.minHeight > 1 ? g_BorderlessParams.minHeight : 1, rect);
            if (rect[0] != win[0] || rect[1] != win[1])
                glfwSetWindowPos(window, rect[0], rect[1]);
            if (rect[2] != win[2] || rect[3] != win[3])
                glfwSetWindowSize(window, rect[2], rect[3]);
        }
        g_AppData.bMouseWasDown = down;
        g_AppData.iHoverEdges = g_AppData.iDragEdges ? (g_AppData.iDragEdges & 15) : edges;
    }
#endif

    return true;
}

// ImPlatform API - PlatformNewFrame
IMPLATFORM_API void ImPlatform_PlatformNewFrame(void)
{
    ImGui_ImplGlfw_NewFrame();
}

// ImPlatform API - ShutdownPostGfxAPI
IMPLATFORM_API void ImPlatform_ShutdownPostGfxAPI(void)
{
    ImGui_ImplGlfw_Shutdown();
}

// ImPlatform API - DestroyWindow
IMPLATFORM_API void ImPlatform_DestroyWindow(void)
{
    glfwDestroyWindow(g_AppData.pWindow);
    g_AppData.pWindow = NULL;
    glfwTerminate();
}

// Internal API - Get DPI scale
float ImPlatform_App_GetDpiScale_GLFW(void)
{
    return g_AppData.fDpiScale > 0.0f ? g_AppData.fDpiScale : 1.0f;
}

// Internal API - Get GLFW window
GLFWwindow* ImPlatform_App_GetGLFWWindow(void)
{
    return g_AppData.pWindow;
}

// Internal API - Get GLFW app data
ImPlatform_AppData_GLFW* ImPlatform_App_GetData_GLFW(void)
{
    return &g_AppData;
}

#if IMPLATFORM_APP_SUPPORT_DROP_FILE
void ImPlatform_App_OnDropFileCallbackChanged(bool has_callback)
{
    g_AppData.bDropFileEnabled = has_callback;
    if (g_AppData.pWindow)
        glfwSetDropCallback(g_AppData.pWindow, has_callback ? glfw_drop_callback : NULL);
}
#endif // IMPLATFORM_APP_SUPPORT_DROP_FILE

#endif // IM_PLATFORM_GLFW
