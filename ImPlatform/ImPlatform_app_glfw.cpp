// dear imgui: Platform Abstraction - GLFW Application Backend
// This handles window creation, message loop, and input for GLFW

#include "ImPlatform_Internal.h"

#include <stdio.h>

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
#elif IM_CURRENT_GFX == IM_GFX_VULKAN
    // Vulkan doesn't use OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#endif

#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
    // Custom titlebar support
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

    // Create window with graphics context
    g_AppData.pWindow = glfwCreateWindow((int)uWidth, (int)uHeight, pWindowsName, NULL, NULL);
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

    // Set window position (GLFW doesn't support this in creation, so we set it after)
    glfwSetWindowPos(g_AppData.pWindow, (int)vPos.x, (int)vPos.y);

    // Query DPI scale and register callback for runtime changes (GLFW 3.3+)
#if GLFW_VERSION_MAJOR > 3 || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 3)
    {
        float xscale = 1.0f, yscale = 1.0f;
        glfwGetWindowContentScale(g_AppData.pWindow, &xscale, &yscale);
        g_AppData.fDpiScale = xscale;
        glfwSetWindowContentScaleCallback(g_AppData.pWindow, glfw_content_scale_callback);
    }
#else
    g_AppData.fDpiScale = 1.0f;
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
#ifdef IM_GFX_OPENGL3
    return ImGui_ImplGlfw_InitForOpenGL(g_AppData.pWindow, true);
#elif defined(IM_GFX_VULKAN)
    return ImGui_ImplGlfw_InitForVulkan(g_AppData.pWindow, true);
#else
    return ImGui_ImplGlfw_InitForOther(g_AppData.pWindow, true);
#endif
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

    // Handle minimize
    if (glfwGetWindowAttrib(g_AppData.pWindow, GLFW_ICONIFIED) != 0)
    {
        ImGui_ImplGlfw_Sleep(10);
        return false; // Skip frame
    }

#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR && !defined(IM_THE_CHERNO_GLFW3)
    // Software drag for borderless window (when TheCherno's fork is not available)
    if (g_AppData.bCustomTitleBar && g_BorderlessParams.allowMove)
    {
        int mouse_state = glfwGetMouseButton(g_AppData.pWindow, GLFW_MOUSE_BUTTON_LEFT);
        if (mouse_state == GLFW_PRESS && g_AppData.bTitleBarHovered && !g_AppData.bDragging)
        {
            g_AppData.bDragging = true;
            glfwGetCursorPos(g_AppData.pWindow, &g_AppData.fDragStartX, &g_AppData.fDragStartY);
            glfwGetWindowPos(g_AppData.pWindow, &g_AppData.iWinStartX, &g_AppData.iWinStartY);
        }
        else if (mouse_state == GLFW_PRESS && g_AppData.bDragging)
        {
            double curX, curY;
            glfwGetCursorPos(g_AppData.pWindow, &curX, &curY);
            int newX = g_AppData.iWinStartX + (int)(curX - g_AppData.fDragStartX);
            int newY = g_AppData.iWinStartY + (int)(curY - g_AppData.fDragStartY);
            glfwSetWindowPos(g_AppData.pWindow, newX, newY);
        }
        else
        {
            g_AppData.bDragging = false;
        }
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
    glfwDestroyWindow(g_AppData.pWindow);
    glfwTerminate();
}

// ImPlatform API - DestroyWindow
IMPLATFORM_API void ImPlatform_DestroyWindow(void)
{
    // Already handled in ShutdownPostGfxAPI
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

#endif // IM_PLATFORM_GLFW
