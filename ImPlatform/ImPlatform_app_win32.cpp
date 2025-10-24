// dear imgui: Platform Abstraction - Win32 Application Backend
// This handles window creation, message loop, and input for Win32

#include "ImPlatform_Internal.h"

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)

#include "../imgui/backends/imgui_impl_win32.h"
#include <tchar.h>
#include <windowsx.h>  // For GET_X_LPARAM, GET_Y_LPARAM

// Global state
static ImPlatform_AppData_Win32 g_AppData = { 0 };

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Forward declarations from internal header are already included

#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
// Forward declaration - defined in ImPlatform_titlebar.cpp
bool ImPlatform_IsMaximized(void);
#endif

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
#elif defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_OPENGL3)
            ImPlatform_Gfx_SetSize_OpenGL3(uWidth, uHeight);
#endif
        }
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;

#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
    case WM_NCCALCSIZE:
        if (g_AppData.bCustomTitleBar && lParam)
        {
            // Custom titlebar: remove standard window frame but keep resize borders
            RECT border_thickness = {0, 0, 0, 0};
            AdjustWindowRectEx(&border_thickness, GetWindowLong(hWnd, GWL_STYLE) & ~WS_CAPTION, FALSE, NULL);
            border_thickness.left *= -1;
            border_thickness.top *= -1;
            border_thickness.right *= -1;
            border_thickness.bottom *= -1;

            NCCALCSIZE_PARAMS* sz = (NCCALCSIZE_PARAMS*)lParam;
            sz->rgrc[0].left += border_thickness.left;
            sz->rgrc[0].right -= border_thickness.right;
            sz->rgrc[0].bottom -= border_thickness.bottom;
            return 0;
        }
        break;

    case WM_NCHITTEST:
        if (g_AppData.bCustomTitleBar)
        {
            // Allow resizing from borders when not maximized
            if (!ImPlatform_IsMaximized())
            {
                RECT winRc;
                GetClientRect(hWnd, &winRc);
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                ScreenToClient(hWnd, &pt);

                RECT border_thickness = {0, 0, 0, 0};
                AdjustWindowRectEx(&border_thickness, GetWindowLong(hWnd, GWL_STYLE) & ~WS_CAPTION, FALSE, NULL);
                border_thickness.left *= -1;
                border_thickness.top *= -1;
                border_thickness.right *= -1;
                border_thickness.bottom *= -1;

                const int verticalBorderSize = GetSystemMetrics(SM_CYFRAME);

                if (PtInRect(&winRc, pt))
                {
                    enum { left = 1, top = 2, right = 4, bottom = 8 };
                    int hit = 0;
                    if (pt.x <= border_thickness.left)
                        hit |= left;
                    if (pt.x >= winRc.right - border_thickness.right)
                        hit |= right;
                    if (pt.y <= border_thickness.top || pt.y < verticalBorderSize)
                        hit |= top;
                    if (pt.y >= winRc.bottom - border_thickness.bottom)
                        hit |= bottom;

                    if (hit & top && hit & left)        return HTTOPLEFT;
                    if (hit & top && hit & right)       return HTTOPRIGHT;
                    if (hit & bottom && hit & left)     return HTBOTTOMLEFT;
                    if (hit & bottom && hit & right)    return HTBOTTOMRIGHT;
                    if (hit & left)                     return HTLEFT;
                    if (hit & top)                      return HTTOP;
                    if (hit & right)                    return HTRIGHT;
                    if (hit & bottom)                   return HTBOTTOM;
                }
            }

            // Check if hovering titlebar for dragging
            if (g_AppData.bTitleBarHovered)
            {
                return HTCAPTION;
            }
            else
            {
                return HTCLIENT;
            }
        }
        break;
#endif // IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR

    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }

    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ImPlatform API - CreateWindow
IMPLATFORM_API bool ImPlatform_CreateWindow(char const* pWindowsName, ImVec2 const vPos, unsigned int uWidth, unsigned int uHeight)
{
    // Make process DPI aware and obtain main monitor scale
    // NOTE: DPI awareness is disabled for OpenGL3 - the official ImGui example has it commented out
    //       with "FIXME: This somehow doesn't work in the Win32+OpenGL example. Why?"
#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_OPENGL3)
    //ImGui_ImplWin32_EnableDpiAwareness(); // Disabled for OpenGL3
#else
    ImGui_ImplWin32_EnableDpiAwareness();
#endif
    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY));

    // Store scale for later use in ImPlatform_InitPlatform
    g_AppData.fDpiScale = main_scale;

    // Convert name to wide string
    int wchars_num = ::MultiByteToWideChar(CP_UTF8, 0, pWindowsName, -1, NULL, 0);
    wchar_t* wName = new wchar_t[wchars_num];
    ::MultiByteToWideChar(CP_UTF8, 0, pWindowsName, -1, wName, wchars_num);

    // Create application window
    g_AppData.wc.cbSize = sizeof(WNDCLASSEXW);
#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_OPENGL3)
    g_AppData.wc.style = CS_OWNDC; // OpenGL needs CS_OWNDC
#else
    g_AppData.wc.style = CS_CLASSDC;
#endif
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

    // Apply DPI scaling to window size
    // Use custom window style for custom titlebar (borderless with thick frame for resizing)
#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
    DWORD dwStyle = g_AppData.bCustomTitleBar ? (WS_POPUP | WS_THICKFRAME) : WS_OVERLAPPEDWINDOW;
#else
    DWORD dwStyle = WS_OVERLAPPEDWINDOW;
#endif

    g_AppData.hWnd = ::CreateWindowW(
        g_AppData.wc.lpszClassName,
        wName,
        dwStyle,
        (int)vPos.x, (int)vPos.y,
        (int)(uWidth * main_scale), (int)(uHeight * main_scale),
        NULL, NULL,
        g_AppData.wc.hInstance,
        NULL
    );

    delete[] wName;

    if (!g_AppData.hWnd)
        return false;

    // Get actual client area size after window creation and update graphics backend
    RECT rect;
    ::GetClientRect(g_AppData.hWnd, &rect);
    unsigned int client_width = (unsigned int)(rect.right - rect.left);
    unsigned int client_height = (unsigned int)(rect.bottom - rect.top);

#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_OPENGL3)
    // OpenGL needs viewport size set immediately
    ImPlatform_Gfx_SetSize_OpenGL3(client_width, client_height);
#endif
    // DirectX backends handle size through OnResize during swapchain creation, not here

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
#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_OPENGL3)
    return ImGui_ImplWin32_InitForOpenGL(g_AppData.hWnd);
#else
    return ImGui_ImplWin32_Init(g_AppData.hWnd);
#endif
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

// Internal API - Get DPI scale
float ImPlatform_App_GetDpiScale_Win32(void)
{
    return g_AppData.fDpiScale;
}

// Internal API - Get Win32 app data
ImPlatform_AppData_Win32* ImPlatform_App_GetData_Win32(void)
{
    return &g_AppData;
}

#endif // IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32
