// dear imgui: Platform Abstraction - Win32 Application Backend
// This handles window creation, message loop, and input for Win32

#include "ImPlatform_Internal.h"

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)

#include "../imgui/backends/imgui_impl_win32.h"
#include <tchar.h>

// Global state
static ImPlatform_AppData_Win32 g_AppData = { 0 };

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Forward declarations from internal header are already included

// Win32 message handler
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;

        // Notify gfx backend of resize
        {
            unsigned int uWidth = (unsigned int)LOWORD(lParam);
            unsigned int uHeight = (unsigned int)HIWORD(lParam);
#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_DIRECTX9)
            ImPlatform_Gfx_OnResize_DX9(ImPlatform_Gfx_GetData_DX9(), uWidth, uHeight);
#elif defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_DIRECTX10)
            ImPlatform_Gfx_OnResize_DX10(ImPlatform_Gfx_GetData_DX10(), uWidth, uHeight);
#elif defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
            ImPlatform_Gfx_OnResize_DX11(ImPlatform_Gfx_GetData_DX11(), uWidth, uHeight);
#elif defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
            ImPlatform_Gfx_OnResize_DX12(ImPlatform_Gfx_GetData_DX12(), uWidth, uHeight);
#endif
        }
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;

    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }

    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ImPlatform API - CreateWindow
IMPLATFORM_API bool ImPlatform_CreateWindow(char const* pWindowsName, ImVec2 const vPos, unsigned int uWidth, unsigned int uHeight)
{
    // Convert name to wide string
    int wchars_num = ::MultiByteToWideChar(CP_UTF8, 0, pWindowsName, -1, NULL, 0);
    wchar_t* wName = new wchar_t[wchars_num];
    ::MultiByteToWideChar(CP_UTF8, 0, pWindowsName, -1, wName, wchars_num);

    // Create application window
    g_AppData.wc.cbSize = sizeof(WNDCLASSEXW);
    g_AppData.wc.style = CS_CLASSDC;
    g_AppData.wc.lpfnWndProc = WndProc;
    g_AppData.wc.cbClsExtra = 0L;
    g_AppData.wc.cbWndExtra = 0L;
    g_AppData.wc.hInstance = ::GetModuleHandle(NULL);
    g_AppData.wc.hIcon = NULL;
    g_AppData.wc.hCursor = NULL;
    g_AppData.wc.hbrBackground = NULL;
    g_AppData.wc.lpszMenuName = NULL;
    g_AppData.wc.lpszClassName = wName;
    g_AppData.wc.hIconSm = NULL;
    ::RegisterClassExW(&g_AppData.wc);

    g_AppData.hWnd = ::CreateWindowW(
        g_AppData.wc.lpszClassName,
        wName,
        WS_OVERLAPPEDWINDOW,
        (int)vPos.x, (int)vPos.y,
        (int)uWidth, (int)uHeight,
        NULL, NULL,
        g_AppData.wc.hInstance,
        NULL
    );

    delete[] wName;

    if (!g_AppData.hWnd)
        return false;

    g_AppData.bDone = false;
    return true;
}

// ImPlatform API - ShowWindow
IMPLATFORM_API bool ImPlatform_ShowWindow(void)
{
    ::ShowWindow(g_AppData.hWnd, SW_SHOWDEFAULT);
    ::UpdateWindow(g_AppData.hWnd);
    return true;
}

// ImPlatform API - InitPlatform (platform-specific part)
IMPLATFORM_API bool ImPlatform_InitPlatform(void)
{
    return ImGui_ImplWin32_Init(g_AppData.hWnd);
}

// ImPlatform API - PlatformContinue
IMPLATFORM_API bool ImPlatform_PlatformContinue(void)
{
    return !g_AppData.bDone;
}

// ImPlatform API - PlatformEvents
IMPLATFORM_API bool ImPlatform_PlatformEvents(void)
{
    MSG msg;
    while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
        if (msg.message == WM_QUIT)
            g_AppData.bDone = true;
    }

    if (g_AppData.bDone)
        return false;

    return true;
}

// ImPlatform API - PlatformNewFrame
IMPLATFORM_API void ImPlatform_PlatformNewFrame(void)
{
    ImGui_ImplWin32_NewFrame();
}

// ImPlatform API - ShutdownPostGfxAPI (platform-specific part)
IMPLATFORM_API void ImPlatform_ShutdownPostGfxAPI(void)
{
    ImGui_ImplWin32_Shutdown();
}

// ImPlatform API - DestroyWindow
IMPLATFORM_API void ImPlatform_DestroyWindow(void)
{
    ::DestroyWindow(g_AppData.hWnd);
    ::UnregisterClassW(g_AppData.wc.lpszClassName, g_AppData.wc.hInstance);
}

// Internal API - Get HWND
HWND ImPlatform_App_GetHWND(void)
{
    return g_AppData.hWnd;
}

// Internal API - Get Win32 app data
ImPlatform_AppData_Win32* ImPlatform_App_GetData_Win32(void)
{
    return &g_AppData;
}

#endif // IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32
