// dear imgui: Platform Abstraction - Win32 Application Backend
// This handles window creation, message loop, and input for Win32

#include "ImPlatform_Internal.h"

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)

#include "../imgui/backends/imgui_impl_win32.h"
#include <tchar.h>
#include <windowsx.h>  // For GET_X_LPARAM, GET_Y_LPARAM
#if IMPLATFORM_APP_SUPPORT_DROP_FILE
#include <shellapi.h>  // For DragQueryPoint, DragFinish
#endif

// Global state
static ImPlatform_AppData_Win32 g_AppData = { 0 };

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Forward declarations from internal header are already included

static DWORD g_FullscreenStyle = 0;     // The window's style before it went fullscreen; 0 while it is not

// Win32 message handler
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Pen, touch and trackpad pinch (ImPlatform_native.cpp, shared with the GLFW and SDL2 windows)
    if (ImPlatform_Native_WindowMessage(hWnd, msg, wParam, lParam))
        return 0;

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
#elif defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_CUSTOM)
            ImPlatform_Gfx_OnResize_Custom(uWidth, uHeight);
#else
            (void)uWidth; (void)uHeight;    // Vulkan and WebGPU read the client size each frame
#endif
        }
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;

    case WM_DPICHANGED:
        {
            UINT dpi = HIWORD(wParam);
            g_AppData.fDpiScale = (float)dpi / 96.0f;
            // The Win32 backend already applies the suggested rectangle when io.ConfigDpiScaleViewports is enabled
            bool backend_resizes = false;
#ifdef IMGUI_HAS_VIEWPORT
            backend_resizes = ImGui::GetCurrentContext() != NULL && ImGui::GetIO().ConfigDpiScaleViewports;
#endif
            if (!backend_resizes)
            {
                RECT* const prcNewWindow = (RECT*)lParam;
                ::SetWindowPos(hWnd, NULL,
                    prcNewWindow->left, prcNewWindow->top,
                    prcNewWindow->right - prcNewWindow->left,
                    prcNewWindow->bottom - prcNewWindow->top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
            }
            ImPlatform_NotifyDpiChange(g_AppData.fDpiScale);
        }
        return 0;

#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
    case WM_NCCALCSIZE:
        if (g_AppData.bCustomTitleBar && lParam)
        {
            // Borderless: the ENTIRE window is client area (no visible frame or
            // caption) in BOTH restored and maximized states. Returning 0 with
            // rgrc[0] unchanged removes the standard frame while keeping the
            // window functional. Resize is provided by WM_NCHITTEST returning
            // HTLEFT/HTRIGHT/HTTOP/HTBOTTOM near the edges (now aligned with the
            // window edges); maximize is clamped to the work area in
            // WM_GETMINMAXINFO. The old asymmetric left/right/bottom inset broke
            // edge resize and shifted the layout between restored and maximized.
            return 0;
        }
        break;

    case WM_NCHITTEST:
        if (g_AppData.bCustomTitleBar && !g_FullscreenStyle)    // Fullscreen: no border to resize by, no titlebar to move by
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

    case WM_NCLBUTTONDBLCLK:
        // Double-click the (custom) title bar toggles maximize/restore. The
        // borderless WS_POPUP window has no WS_MAXIMIZEBOX, so the default
        // double-click does nothing — do it explicitly. SC_MAXIMIZE goes through
        // the standard path (WM_GETMINMAXINFO work-area clamp), so no crop.
        if (g_AppData.bCustomTitleBar && wParam == HTCAPTION)
        {
            ::SendMessageW(hWnd, WM_SYSCOMMAND,
                ImPlatform_IsMaximized() ? SC_RESTORE : SC_MAXIMIZE, 0);
            return 0;
        }
        break;
#endif // IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR

#if IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR
    case WM_GETMINMAXINFO:
        if (g_AppData.bCustomTitleBar)
        {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;

            // Clamp the MAXIMIZED position + size to the monitor WORK AREA so a
            // borderless window fills the screen exactly instead of spilling its
            // (invisible) frame off the right/bottom/top edges. Position is
            // relative to the monitor origin; size is the work-area extent.
            // ptMaxTrackSize is left untouched so manual resize isn't capped.
            HMONITOR hMon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi; mi.cbSize = sizeof(mi);
            if (GetMonitorInfo(hMon, &mi))
            {
                mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
                mmi->ptMaxPosition.y = mi.rcWork.top  - mi.rcMonitor.top;
                mmi->ptMaxSize.x     = mi.rcWork.right  - mi.rcWork.left;
                mmi->ptMaxSize.y     = mi.rcWork.bottom - mi.rcWork.top;
            }

            if (g_BorderlessParams.minWidth > 0)
                mmi->ptMinTrackSize.x = g_BorderlessParams.minWidth;
            if (g_BorderlessParams.minHeight > 0)
                mmi->ptMinTrackSize.y = g_BorderlessParams.minHeight;
            return 0;
        }
        break;
#endif

#if IMPLATFORM_APP_SUPPORT_DROP_FILE
    case WM_DROPFILES:  // Only where the window could not be made an OLE drop target
        {
            HDROP hDrop = (HDROP)wParam;
            POINT pt = {};
            ::DragQueryPoint(hDrop, &pt);  // client-space coordinates
            const ImVec2 pos((float)pt.x, (float)pt.y);
            ImPlatform_NotifyDrop(ImPlatform_DropEvent_Begin, NULL, pos);
            ImPlatform_Native_NotifyDroppedFiles(hDrop, pos);
            ImPlatform_NotifyDrop(ImPlatform_DropEvent_End, NULL, pos);
            ::DragFinish(hDrop);
            return 0;
        }
#endif // IMPLATFORM_APP_SUPPORT_DROP_FILE

    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }

    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ImPlatform API - CreateWindow
IMPLATFORM_API bool ImPlatform_CreateWindow(char const* pWindowsName, ImVec2 const vPos, unsigned int uWidth, unsigned int uHeight)
{
    // Make process DPI aware and obtain the DPI scale for the monitor this
    // window will actually be created on. Was previously hardcoded to
    // MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY) -- i.e. always the
    // PRIMARY monitor's DPI, completely ignoring vPos -- so a window created
    // at a position on a secondary monitor with a different DPI than the
    // primary would size itself (uWidth/uHeight below) and set the initial
    // ImGui style scale using the WRONG monitor's DPI. MONITOR_DEFAULTTONEAREST
    // (rather than TOPRIMARY) as the fallback so an off-screen/edge vPos still
    // resolves to whichever real monitor is closest, instead of forcing primary.
    ImGui_ImplWin32_EnableDpiAwareness();
    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint({(LONG)vPos.x, (LONG)vPos.y}, MONITOR_DEFAULTTONEAREST));

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
    g_AppData.wc.lpszClassName = L"ImPlatform"; // Static string: must outlive the window for UnregisterClassW()
    g_AppData.wc.hIconSm = NULL;
    ::RegisterClassExW(&g_AppData.wc);

    // Apply DPI scaling to window size.
    // Custom titlebar: WS_POPUP (no caption) + WS_THICKFRAME (resize). WS_POPUP
    // maximizes cleanly to the work area once WM_GETMINMAXINFO clamps the size
    // (no right/bottom crop), and WM_NCCALCSIZE returns the FULL window as client
    // in BOTH states so restored and maximized layouts match. Double-click-to-
    // maximize is handled explicitly in WM_NCLBUTTONDBLCLK (so we don't need
    // WS_MAXIMIZEBOX/WS_SYSMENU, which would re-add a caption). WS_OVERLAPPEDWINDOW
    // was tried and re-introduced the crop + restored/maximized inconsistency.
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
#if defined(IM_CURRENT_GFX) && (IM_CURRENT_GFX == IM_GFX_OPENGL3)
    RECT rect;
    ::GetClientRect(g_AppData.hWnd, &rect);
    unsigned int client_width = (unsigned int)(rect.right - rect.left);
    unsigned int client_height = (unsigned int)(rect.bottom - rect.top);
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
#if IMPLATFORM_APP_SUPPORT_DROP_FILE
    if (g_AppData.bDropFileEnabled)
        ImPlatform_Native_AcceptDrops(true);
#endif
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
    ImPlatform_Native_Pump();
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
#if IMPLATFORM_APP_SUPPORT_WINDOW_STATE
static HICON g_WindowIcon = NULL;

IMPLATFORM_API void ImPlatform_SetFullscreen(bool fullscreen)
{
    static WINDOWPLACEMENT placement = { sizeof(WINDOWPLACEMENT) };
    HWND hWnd = g_AppData.hWnd;
    if (!hWnd || fullscreen == ImPlatform_IsFullscreen())
        return;
    if (fullscreen)
    {
        MONITORINFO mi = { sizeof(MONITORINFO) };
        ::GetWindowPlacement(hWnd, &placement);
        ::GetMonitorInfo(::MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), &mi);
        g_FullscreenStyle = (DWORD)::GetWindowLong(hWnd, GWL_STYLE);
        ::SetWindowLong(hWnd, GWL_STYLE, (LONG)((g_FullscreenStyle & ~(DWORD)WS_OVERLAPPEDWINDOW) | WS_POPUP));
        ::SetWindowPos(hWnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                       mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    else
    {
        ::SetWindowLong(hWnd, GWL_STYLE, (LONG)g_FullscreenStyle);
        g_FullscreenStyle = 0;
        ::SetWindowPlacement(hWnd, &placement);
        ::SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

IMPLATFORM_API bool ImPlatform_IsFullscreen(void)
{
    return g_FullscreenStyle != 0;
}

IMPLATFORM_API void ImPlatform_SetAlwaysOnTop(bool on_top)
{
    ::SetWindowPos(g_AppData.hWnd, on_top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

IMPLATFORM_API bool ImPlatform_SetWindowIcon(const unsigned char* rgba, int width, int height)
{
    if (!g_AppData.hWnd || !rgba || width <= 0 || height <= 0)
        return false;
    BITMAPV5HEADER bi;
    memset(&bi, 0, sizeof(bi));
    bi.bV5Size = sizeof(bi);
    bi.bV5Width = width;
    bi.bV5Height = -height;     // Rows top-down
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00ff0000;
    bi.bV5GreenMask = 0x0000ff00;
    bi.bV5BlueMask = 0x000000ff;
    bi.bV5AlphaMask = 0xff000000;
    unsigned char* bgra = NULL;
    HDC dc = ::GetDC(NULL);
    ICONINFO info = { TRUE, 0, 0, ::CreateBitmap(width, height, 1, 1, NULL),
                      ::CreateDIBSection(dc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, (void**)&bgra, NULL, 0) };
    ::ReleaseDC(NULL, dc);
    HICON icon = NULL;
    if (info.hbmMask && info.hbmColor && bgra)
    {
        for (int i = 0; i < width * height; ++i, rgba += 4, bgra += 4)
        {
            bgra[0] = rgba[2];
            bgra[1] = rgba[1];
            bgra[2] = rgba[0];
            bgra[3] = rgba[3];
        }
        icon = ::CreateIconIndirect(&info);     // It keeps copies of the two bitmaps
    }
    if (info.hbmMask)
        ::DeleteObject(info.hbmMask);
    if (info.hbmColor)
        ::DeleteObject(info.hbmColor);
    if (!icon)
        return false;
    ::SendMessageW(g_AppData.hWnd, WM_SETICON, ICON_BIG, (LPARAM)icon);
    ::SendMessageW(g_AppData.hWnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);
    if (g_WindowIcon)
        ::DestroyIcon(g_WindowIcon);
    g_WindowIcon = icon;
    return true;
}
#endif // IMPLATFORM_APP_SUPPORT_WINDOW_STATE

IMPLATFORM_API void ImPlatform_DestroyWindow(void)
{
    ImPlatform_Native_Shutdown();
    ::DestroyWindow(g_AppData.hWnd);
    ::UnregisterClassW(g_AppData.wc.lpszClassName, g_AppData.wc.hInstance);
#if IMPLATFORM_APP_SUPPORT_WINDOW_STATE
    if (g_WindowIcon)
        ::DestroyIcon(g_WindowIcon);
    g_WindowIcon = NULL;
#endif
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

#if IMPLATFORM_APP_SUPPORT_DROP_FILE
void ImPlatform_App_OnDropFileCallbackChanged(bool has_callback)
{
    g_AppData.bDropFileEnabled = has_callback;
    ImPlatform_Native_AcceptDrops(has_callback);
}
#endif // IMPLATFORM_APP_SUPPORT_DROP_FILE

#endif // IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32
