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

    // Create window with graphics context
    g_AppData.pWindow = glfwCreateWindow((int)uWidth, (int)uHeight, pWindowsName, NULL, NULL);
    if (g_AppData.pWindow == NULL)
        return false;

    // Set window position (GLFW doesn't support this in creation, so we set it after)
    glfwSetWindowPos(g_AppData.pWindow, (int)vPos.x, (int)vPos.y);

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
