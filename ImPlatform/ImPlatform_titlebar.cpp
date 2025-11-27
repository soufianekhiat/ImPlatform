// dear imgui: Platform Abstraction - Custom Title Bar Implementation
// This handles custom titlebar rendering and window controls

#include "ImPlatform_Internal.h"

#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR

// ============================================================================
// Helper Functions
// ============================================================================

// Helper function for checking if window is maximized (used by both titlebar and app backends)
bool ImPlatform_IsMaximized(void)
{
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    ImPlatform_AppData_Win32* pData = ImPlatform_App_GetData_Win32();
    return (((( DWORD )GetWindowLong(pData->hWnd, GWL_STYLE)) & (WS_MAXIMIZE)) != 0L);

#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    ImPlatform_AppData_GLFW* pData = ImPlatform_App_GetData_GLFW();
    return (bool)glfwGetWindowAttrib(pData->pWindow, GLFW_MAXIMIZED);

#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
    ImPlatform_AppData_SDL2* pData = ImPlatform_App_GetData_SDL2();
    return (SDL_GetWindowFlags(pData->pWindow) & SDL_WINDOW_MAXIMIZED) != 0;

#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
    ImPlatform_AppData_SDL3* pData = ImPlatform_App_GetData_SDL3();
    return (SDL_GetWindowFlags(pData->pWindow) & SDL_WINDOW_MAXIMIZED) != 0;

#else
    return false;
#endif
}

// ============================================================================
// Public API Implementation
// ============================================================================

IMPLATFORM_API bool ImPlatform_CustomTitleBarEnabled(void)
{
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    ImPlatform_AppData_Win32* pData = ImPlatform_App_GetData_Win32();
    return pData->bCustomTitleBar;

#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    ImPlatform_AppData_GLFW* pData = ImPlatform_App_GetData_GLFW();
    return pData->bCustomTitleBar;

#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
    return false;  // Not yet supported

#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
    return false;  // Not yet supported

#else
    return false;
#endif
}

IMPLATFORM_API void ImPlatform_EnableCustomTitleBar(void)
{
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    ImPlatform_AppData_Win32* pData = ImPlatform_App_GetData_Win32();
    pData->bCustomTitleBar = true;

#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    ImPlatform_AppData_GLFW* pData = ImPlatform_App_GetData_GLFW();
    pData->bCustomTitleBar = true;
#ifndef IM_THE_CHERNO_GLFW3
    fprintf(stderr, "To have the support of Custom Title Bar on GLFW3, need CHERNO dev version of GLFW3 (https://github.com/TheCherno/glfw/tree/dev).\n");
#endif

#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
    fprintf(stderr, "Custom Title Bar not yet supported on SDL2.\n");

#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
    fprintf(stderr, "Custom Title Bar not yet supported on SDL3.\n");

#endif
}

IMPLATFORM_API void ImPlatform_DisableCustomTitleBar(void)
{
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    ImPlatform_AppData_Win32* pData = ImPlatform_App_GetData_Win32();
    pData->bCustomTitleBar = false;

#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    ImPlatform_AppData_GLFW* pData = ImPlatform_App_GetData_GLFW();
    pData->bCustomTitleBar = false;

#endif
}

IMPLATFORM_API void ImPlatform_DrawCustomMenuBarDefault(void)
{
    ImGui::Text("ImPlatform with Custom Title Bar");
    ImGui::SameLine();

    if (ImGui::Button("Minimize"))
        ImPlatform_MinimizeApp();
    ImGui::SameLine();

    if (ImGui::Button("Maximize"))
        ImPlatform_MaximizeApp();
    ImGui::SameLine();

    if (ImGui::Button("Close"))
        ImPlatform_CloseApp();
    ImGui::SameLine();
}

IMPLATFORM_API void ImPlatform_MinimizeApp(void)
{
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    ImPlatform_AppData_Win32* pData = ImPlatform_App_GetData_Win32();
    ShowWindow(pData->hWnd, SW_MINIMIZE);

#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    ImPlatform_AppData_GLFW* pData = ImPlatform_App_GetData_GLFW();
    glfwIconifyWindow(pData->pWindow);

#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
    ImPlatform_AppData_SDL2* pData = ImPlatform_App_GetData_SDL2();
    SDL_MinimizeWindow(pData->pWindow);

#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
    ImPlatform_AppData_SDL3* pData = ImPlatform_App_GetData_SDL3();
    SDL_MinimizeWindow(pData->pWindow);

#endif
}

IMPLATFORM_API void ImPlatform_MaximizeApp(void)
{
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    ImPlatform_AppData_Win32* pData = ImPlatform_App_GetData_Win32();
    if (ImPlatform_IsMaximized())
        ShowWindow(pData->hWnd, SW_RESTORE);
    else
        ShowWindow(pData->hWnd, SW_SHOWMAXIMIZED);

#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    ImPlatform_AppData_GLFW* pData = ImPlatform_App_GetData_GLFW();
    if (ImPlatform_IsMaximized())
        glfwRestoreWindow(pData->pWindow);
    else
        glfwMaximizeWindow(pData->pWindow);

#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
    ImPlatform_AppData_SDL2* pData = ImPlatform_App_GetData_SDL2();
    if (ImPlatform_IsMaximized())
        SDL_RestoreWindow(pData->pWindow);
    else
        SDL_MaximizeWindow(pData->pWindow);

#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
    ImPlatform_AppData_SDL3* pData = ImPlatform_App_GetData_SDL3();
    if (ImPlatform_IsMaximized())
        SDL_RestoreWindow(pData->pWindow);
    else
        SDL_MaximizeWindow(pData->pWindow);

#endif
}

IMPLATFORM_API void ImPlatform_CloseApp(void)
{
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    PostQuitMessage(WM_CLOSE);

#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    ImPlatform_AppData_GLFW* pData = ImPlatform_App_GetData_GLFW();
    glfwSetWindowShouldClose(pData->pWindow, true);

#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
    ImPlatform_AppData_SDL2* pData = ImPlatform_App_GetData_SDL2();
    pData->bDone = true;

#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
    ImPlatform_AppData_SDL3* pData = ImPlatform_App_GetData_SDL3();
    pData->bDone = true;

#endif
}

IMPLATFORM_API bool ImPlatform_BeginCustomTitleBar(float fHeight)
{
    if (!ImPlatform_CustomTitleBarEnabled())
        return false;

    ImGuiViewport* pViewport = ImGui::GetMainViewport();
    ImVec2 vDragZoneSize = ImVec2(pViewport->Size.x, fHeight);

    float titlebarVerticalOffset = ImPlatform_IsMaximized() ? 6.0f : 0.0f;

    ImGui::SetNextWindowPos(ImVec2(pViewport->Pos.x, pViewport->Pos.y + titlebarVerticalOffset), ImGuiCond_Always);
    ImGui::SetNextWindowSize(vDragZoneSize);
#ifdef IMGUI_HAS_VIEWPORT
    ImGui::SetNextWindowViewport(pViewport->ID);
#endif

    bool bRet = ImGui::Begin("##ImPlatformCustomTitleBar", 0, ImGuiWindowFlags_NoDecoration);

    ImVec2 vPos = ImGui::GetCursorPos();
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("##ImPlatformCustomTitleBarDragZone", vDragZoneSize);

    // Update hovered state
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    ImPlatform_AppData_Win32* pData = ImPlatform_App_GetData_Win32();
    pData->bTitleBarHovered = ImGui::IsItemHovered();
    pData->vEndCustomToolBar = ImGui::GetCursorPos();
    pData->fCustomTitleBarHeight = fHeight;

#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    ImPlatform_AppData_GLFW* pData = ImPlatform_App_GetData_GLFW();
    pData->bTitleBarHovered = ImGui::IsItemHovered();
    pData->vEndCustomToolBar = ImGui::GetCursorPos();
    pData->fCustomTitleBarHeight = fHeight;

#endif

    ImGui::SetCursorPos(vPos);

    return bRet;
}

IMPLATFORM_API void ImPlatform_EndCustomTitleBar(void)
{
    if (!ImPlatform_CustomTitleBarEnabled())
        return;

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
    ImPlatform_AppData_Win32* pData = ImPlatform_App_GetData_Win32();
    ImGui::SetCursorPos(pData->vEndCustomToolBar);
    ImGui::End();
    ImGuiViewport* pViewport = ImGui::GetMainViewport();
    pViewport->WorkPos.y += pData->fCustomTitleBarHeight;

#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    ImPlatform_AppData_GLFW* pData = ImPlatform_App_GetData_GLFW();
    ImGui::SetCursorPos(pData->vEndCustomToolBar);
    ImGui::End();
    ImGuiViewport* pViewport = ImGui::GetMainViewport();
    pViewport->WorkPos.y += pData->fCustomTitleBarHeight;

#endif
}

#endif // IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
