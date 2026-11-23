// dear imgui: Platform Abstraction - system services reached without the window library
// The user's light or dark mode, images on the clipboard, the pen, the trackpad's gestures, drags in and out: what
// GLFW, SDL2 or SDL3 do not expose is taken from the system itself.
//   Windows: Win32 and OLE. The pen, touch, pinch and drops of the Win32 backend's window are those of a GLFW or SDL2
//            window too.
//   macOS:   CoreFoundation, the pasteboard and ImageIO through their C interfaces, AppKit's events through the
//            Objective-C runtime, so this file compiles as C++ and as Objective-C++.
//   Linux:   the settings portal over D-Bus, libpng, the X11 clipboard and the X11 drag protocol (SDL3: SDL's own).
//            libdbus, libpng and libX11 are loaded at run time: where one is not installed, what needs it reports
//            that it is unsupported.

#include "ImPlatform_Internal.h"

#include <string.h>

#if defined(_WIN32) && !defined(_WINDOWS_)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #undef APIENTRY         // GLFW's header leaves its own definition, which windows.h would redefine
    #include <windows.h>    // The GLFW backends do not need it for anything else
#endif

#if defined(__APPLE__)
    #include <TargetConditionals.h>
#endif
#if defined(__APPLE__) && TARGET_OS_OSX
    #define IMPLATFORM_NATIVE_MACOS 1
    #include <ApplicationServices/ApplicationServices.h>    // CoreGraphics, ImageIO, the pasteboard
    #include <objc/message.h>
    #include <objc/runtime.h>
    #include <time.h>
#else
    #define IMPLATFORM_NATIVE_MACOS 0
#endif

#if defined(__linux__) && !defined(__ANDROID__)
    #define IMPLATFORM_NATIVE_LINUX 1
    #include <dlfcn.h>
    #include <stdio.h>
    #include <time.h>
    #include <unistd.h>
#else
    #define IMPLATFORM_NATIVE_LINUX 0
#endif

// Linux with GLFW or SDL2, which carry text on the clipboard and nothing else: the X11 clipboard is read and owned here.
#if IMPLATFORM_NATIVE_LINUX && defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW || IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
    #define IMPLATFORM_NATIVE_X11 1
#else
    #define IMPLATFORM_NATIVE_X11 0
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
    #define IMPLATFORM_NATIVE_SDL3 1
#else
    #define IMPLATFORM_NATIVE_SDL3 0
#endif

// ============================================================================
// Windows: pen, touch, pinch and drops of the window, whichever library made it
// ============================================================================

#ifdef _WIN32
#include <math.h>
#include <ole2.h>       // IDropTarget, RegisterDragDrop, DoDragDrop
#include <shellapi.h>   // DragQueryFileW
#include <shlobj.h>     // SHCreateDataObject, DROPFILES

// The backends whose window gets its pen, touch and drops here. SDL3 reads them itself.
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32 || IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW || IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2)
    #define IMPLATFORM_NATIVE_WINDOW 1
#else
    #define IMPLATFORM_NATIVE_WINDOW 0
#endif

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <GLFW/glfw3native.h>
#endif

void* ImPlatform_Native_MainWindow(void)
{
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
    GLFWwindow* window = ImPlatform_App_GetGLFWWindow();
    return window ? glfwGetWin32Window(window) : NULL;
#elif defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32 || IM_CURRENT_PLATFORM == IM_PLATFORM_SDL2 || IM_CURRENT_PLATFORM == IM_PLATFORM_SDL3)
    return ImPlatform_App_GetHWND();
#else
    return NULL;
#endif
}

#if IMPLATFORM_NATIVE_WINDOW
// Windows Ink pen samples: every hardware sample since the previous message (history is newest first).
static void ImPlatform_Native_PenMessage(HWND hWnd, WPARAM wParam)
{
    const UINT32 id = GET_POINTERID_WPARAM(wParam);
    POINTER_INPUT_TYPE type = PT_POINTER;
    POINTER_PEN_INFO pen = {};
    if (!::GetPointerType(id, &type) || type != PT_PEN || !::GetPointerPenInfo(id, &pen))
        return;
    POINTER_PEN_INFO history[64];
    UINT32 count = pen.pointerInfo.historyCount;
    if (count > 64)
        count = 64;     // Older samples beyond the buffer are skipped (rare: the queue is read every message).
    if (count <= 1 || !::GetPointerPenInfoHistory(id, &count, history))
    {
        history[0] = pen;
        count = 1;
    }
    LARGE_INTEGER freq;
    ::QueryPerformanceFrequency(&freq);
    for (UINT32 k = count; k-- > 0;)
    {
        const POINTER_PEN_INFO& h = history[k];
        POINT pt = h.pointerInfo.ptPixelLocation;
        ::ScreenToClient(hWnd, &pt);
        ImPlatform_PenSample s = {};
        s.x = (float)pt.x;
        s.y = (float)pt.y;
        s.pressure = (h.penMask & PEN_MASK_PRESSURE) ? (float)h.pressure / 1024.0f : -1.0f;
        s.has_tilt = (h.penMask & (PEN_MASK_TILT_X | PEN_MASK_TILT_Y)) != 0;
        s.tilt_x = (float)h.tiltX;
        s.tilt_y = (float)h.tiltY;
        s.time = (double)h.pointerInfo.PerformanceCount / (double)freq.QuadPart;
        s.in_contact = (h.pointerInfo.pointerFlags & POINTER_FLAG_INCONTACT) != 0;
        s.eraser = (h.penFlags & (PEN_FLAG_INVERTED | PEN_FLAG_ERASER)) != 0;
        s.has_rotation = (h.penMask & PEN_MASK_ROTATION) != 0;
        s.rotation = (float)h.rotation;
        ImPlatform_Pen_Push(&s);
    }
}

#if IM_CURRENT_PLATFORM != IM_PLATFORM_SDL2    // SDL2 reports the fingers itself, as its finger events
// A finger on the touch screen: in contact at a position, or lifted (a finger that hovers counts as lifted).
static void ImPlatform_Native_TouchMessage(HWND hWnd, UINT msg, WPARAM wParam)
{
    const UINT32 id = GET_POINTERID_WPARAM(wParam);
    POINTER_INPUT_TYPE type = PT_POINTER;
    POINTER_TOUCH_INFO touch = {};
    if (!::GetPointerType(id, &type) || type != PT_TOUCH || !::GetPointerTouchInfo(id, &touch))
        return;
    POINT pt = touch.pointerInfo.ptPixelLocation;
    ::ScreenToClient(hWnd, &pt);
    ImPlatform_Touch_Set(id, (float)pt.x, (float)pt.y, (touch.touchMask & TOUCH_MASK_PRESSURE) ? (float)touch.pressure / 1024.0f : -1.0f,
                         msg != WM_POINTERUP && (touch.pointerInfo.pointerFlags & POINTER_FLAG_INCONTACT) != 0);
}
#endif

// The pen, touch and pinch messages of the main window. Returns true when the message is used up (a pinch); the
// others are observed only and still reach Dear ImGui and the window's own procedure.
bool ImPlatform_Native_WindowMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM)
{
    // A pinch on a precision touchpad arrives as a wheel message marked Ctrl while the key is up. It is the gesture's
    // zoom, and no scroll for Dear ImGui.
    if (msg == WM_MOUSEWHEEL && (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) && ::GetKeyState(VK_CONTROL) >= 0)
    {
        ImPlatform_Touch_Zoom(powf(2.0f, (float)GET_WHEEL_DELTA_WPARAM(wParam) / (4.0f * (float)WHEEL_DELTA)));
        return true;
    }
    if (msg == WM_POINTERDOWN || msg == WM_POINTERUPDATE || msg == WM_POINTERUP)
    {
        ImPlatform_Native_PenMessage(hWnd, wParam);
#if IM_CURRENT_PLATFORM != IM_PLATFORM_SDL2
        ImPlatform_Native_TouchMessage(hWnd, msg, wParam);
#endif
    }
    else if (msg == WM_POINTERLEAVE || msg == WM_POINTERCAPTURECHANGED)
    {
        ImPlatform_Pen_Leave();
#if IM_CURRENT_PLATFORM != IM_PLATFORM_SDL2
        ImPlatform_Touch_Set(GET_POINTERID_WPARAM(wParam), 0.0f, 0.0f, 0.0f, false);   // Nothing to lift when it is the pen's
#endif
    }
    return false;
}

#if IM_CURRENT_PLATFORM != IM_PLATFORM_WIN32
// GLFW and SDL2 own the window procedure: this one is put before theirs.
static HWND    g_NativeHooked = NULL;
static WNDPROC g_NativeNextProc = NULL;

static LRESULT CALLBACK ImPlatform_Native_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImPlatform_Native_WindowMessage(hWnd, msg, wParam, lParam))
        return 0;
    return ::CallWindowProcW(g_NativeNextProc, hWnd, msg, wParam, lParam);
}
#endif
#endif // IMPLATFORM_NATIVE_WINDOW

// UTF-8 text as a global memory block of UTF-16, the form the clipboard and OLE carry text in.
static HGLOBAL ImPlatform_Native_WideMemory(const char* utf8)
{
    const int chars = ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, (size_t)chars * sizeof(wchar_t));
    if (wchar_t* wide = memory ? (wchar_t*)::GlobalLock(memory) : NULL)
    {
        ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, chars);
        ::GlobalUnlock(memory);
    }
    return memory;
}

#if IMPLATFORM_APP_SUPPORT_DROP_FILE && IMPLATFORM_NATIVE_WINDOW
// The files of a drop, one File event each.
void ImPlatform_Native_NotifyDroppedFiles(void* drop, ImVec2 pos)
{
    const UINT count = ::DragQueryFileW((HDROP)drop, 0xFFFFFFFF, NULL, 0);
    for (UINT i = 0; i < count; ++i)
    {
        wchar_t wpath[32767];
        ::DragQueryFileW((HDROP)drop, i, wpath, 32767);
        char utf8path[32767 * 3];
        ::WideCharToMultiByte(CP_UTF8, 0, wpath, -1, utf8path, sizeof(utf8path), NULL, NULL);
        ImPlatform_NotifyDrop(ImPlatform_DropEvent_File, utf8path, pos);
    }
}

// Drags from other applications over the application's windows (OLE): reported when they enter, as they move, and
// when they drop their files and text or leave. One object serves the main window and Dear ImGui's platform windows.
struct ImPlatform_DropTarget_Win32 : IDropTarget
{
    bool bAccepted;     // The drag over the window carries files or text

    // The main window's client area, whichever window the drag is over: ImPlatform_NotifyDrop takes it from there
    static ImVec2 ClientPos(POINTL pt)
    {
        POINT p = { pt.x, pt.y };
        ::ScreenToClient((HWND)ImPlatform_Native_MainWindow(), &p);
        return ImVec2((float)p.x, (float)p.y);
    }
    static FORMATETC Format(CLIPFORMAT format)
    {
        const FORMATETC f = { format, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        return f;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override
    {
        const bool mine = IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IDropTarget);
        *object = mine ? this : NULL;
        return mine ? S_OK : E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }     // Static storage: not counted
    ULONG STDMETHODCALLTYPE Release() override { return 1; }

    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* data, DWORD keys, POINTL pt, DWORD* effect) override
    {
        FORMATETC files = Format(CF_HDROP), text = Format(CF_UNICODETEXT);
        bAccepted = data->QueryGetData(&files) == S_OK || data->QueryGetData(&text) == S_OK;
        if (bAccepted)
            ImPlatform_NotifyDrop(ImPlatform_DropEvent_Begin, NULL, ClientPos(pt));
        return DragOver(keys, pt, effect);
    }
    HRESULT STDMETHODCALLTYPE DragOver(DWORD, POINTL pt, DWORD* effect) override
    {
        if (bAccepted)
            ImPlatform_NotifyDrop(ImPlatform_DropEvent_Position, NULL, ClientPos(pt));
        *effect = bAccepted ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragLeave() override
    {
        if (bAccepted)
            ImPlatform_NotifyDrop(ImPlatform_DropEvent_End, NULL, ImVec2(0.0f, 0.0f));
        bAccepted = false;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* data, DWORD, POINTL pt, DWORD* effect) override
    {
        *effect = bAccepted ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        if (!bAccepted)
            return S_OK;
        const ImVec2 pos = ClientPos(pt);
        FORMATETC files = Format(CF_HDROP), text = Format(CF_UNICODETEXT);
        STGMEDIUM medium;
        if (data->GetData(&files, &medium) == S_OK)
        {
            ImPlatform_Native_NotifyDroppedFiles(medium.hGlobal, pos);
            ::ReleaseStgMedium(&medium);
        }
        if (data->GetData(&text, &medium) == S_OK)
        {
            if (const wchar_t* wide = (const wchar_t*)::GlobalLock(medium.hGlobal))
            {
                ImVector<char> utf8;
                utf8.resize(::WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL));
                ::WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8.Data, utf8.Size, NULL, NULL);
                ::GlobalUnlock(medium.hGlobal);
                if (utf8.Size > 1)
                    ImPlatform_NotifyDrop(ImPlatform_DropEvent_Text, utf8.Data, pos);
            }
            ::ReleaseStgMedium(&medium);
        }
        bAccepted = false;
        ImPlatform_NotifyDrop(ImPlatform_DropEvent_End, NULL, pos);
        return S_OK;
    }
};
static ImPlatform_DropTarget_Win32 g_DropTarget;
static bool           g_DropWanted = false;         // The application set a drop callback
static bool           g_DropOle = false;            // OleInitialize succeeded, to be undone
static ImVector<HWND> g_DropWindows;                // The windows seen while drops are wanted

void* ImPlatform_Native_GetDropTarget(void)
{
    return static_cast<IDropTarget*>(&g_DropTarget);
}

// Makes the main window and each platform window of Dear ImGui a drop target while drops are wanted, and no longer
// one when they are not.
static void ImPlatform_Native_SyncDrops()
{
    HWND main = (HWND)ImPlatform_Native_MainWindow();
    for (int i = g_DropWindows.Size - 1; i >= 0; --i)
        if (!g_DropWanted || !::IsWindow(g_DropWindows[i]))
        {
            if (::IsWindow(g_DropWindows[i]))
                ::RevokeDragDrop(g_DropWindows[i]);
            g_DropWindows.erase(&g_DropWindows[i]);
        }
    if (!g_DropWanted || !main)
        return;
    if (!g_DropOle)
        g_DropOle = SUCCEEDED(::OleInitialize(NULL));
    ImVector<HWND> windows;
    windows.push_back(main);
    if (ImGui::GetCurrentContext() && (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable))
        for (ImGuiViewport* viewport : ImGui::GetPlatformIO().Viewports)
            if (viewport->PlatformHandleRaw && viewport->PlatformHandleRaw != (void*)main)
                windows.push_back((HWND)viewport->PlatformHandleRaw);
    for (HWND window : windows)
    {
        if (g_DropWindows.contains(window))
            continue;
        g_DropWindows.push_back(window);
        const bool registered = g_DropOle && SUCCEEDED(::RegisterDragDrop(window, &g_DropTarget));
#if IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32
        // A thread in the multithreaded apartment has no OLE: files, at the release (WM_DROPFILES). GLFW and SDL2
        // keep their own handling of that message for this case.
        if (window == main)
            ::DragAcceptFiles(main, registered ? FALSE : TRUE);
#else
        (void)registered;
#endif
    }
}

void ImPlatform_Native_AcceptDrops(bool accept)
{
    g_DropWanted = accept;
#if IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32
    if (!accept && ImPlatform_Native_MainWindow())
        ::DragAcceptFiles((HWND)ImPlatform_Native_MainWindow(), FALSE);
#endif
    ImPlatform_Native_SyncDrops();
}
#else
void* ImPlatform_Native_GetDropTarget(void) { return NULL; }
void ImPlatform_Native_AcceptDrops(bool) {}
#endif

// What is dragged out of the application lets go when no mouse button is held any more.
struct ImPlatform_DropSource_Win32 : IDropSource
{
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override
    {
        const bool mine = IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IDropSource);
        *object = mine ? this : NULL;
        return mine ? S_OK : E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }     // On the stack of ImPlatform_DragOut: not counted
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escape, DWORD keys) override
    {
        return escape ? DRAGDROP_S_CANCEL : (keys & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON)) ? S_OK : DRAGDROP_S_DROP;
    }
    HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) override { return DRAGDROP_S_USEDEFAULTCURSORS; }
};
#endif // _WIN32

IMPLATFORM_API bool ImPlatform_SupportsDragOut(void)
{
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

IMPLATFORM_API bool ImPlatform_DragOut(const char* const* file_paths, int file_count, const char* text)
{
#ifdef _WIN32
    if (FAILED(::OleInitialize(NULL)))      // Counted: undone below, also when OLE was initialized before
        return false;
    bool dropped = false;
    IDataObject* data = NULL;
    if (SUCCEEDED(::SHCreateDataObject(NULL, 0, NULL, NULL, IID_IDataObject, (void**)&data)))
    {
        FORMATETC format = { CF_UNICODETEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM medium;
        memset(&medium, 0, sizeof(medium));
        medium.tymed = TYMED_HGLOBAL;
        if (text && text[0] && (medium.hGlobal = ImPlatform_Native_WideMemory(text)) != NULL)
            if (FAILED(data->SetData(&format, &medium, TRUE)))  // TRUE: the object owns the memory
                ::GlobalFree(medium.hGlobal);
        // The files: a DROPFILES header, the paths one after the other, each ended by a zero, and a last zero
        size_t chars = 1;
        for (int i = 0; i < file_count; ++i)
            chars += (size_t)::MultiByteToWideChar(CP_UTF8, 0, file_paths[i], -1, NULL, 0);
        medium.hGlobal = file_count > 0 ? ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(DROPFILES) + chars * sizeof(wchar_t)) : NULL;
        if (DROPFILES* drop = medium.hGlobal ? (DROPFILES*)::GlobalLock(medium.hGlobal) : NULL)
        {
            drop->pFiles = sizeof(DROPFILES);
            drop->fWide = TRUE;
            wchar_t* at = (wchar_t*)(drop + 1);
            for (int i = 0; i < file_count; ++i)
                at += ::MultiByteToWideChar(CP_UTF8, 0, file_paths[i], -1, at, (int)chars);
            ::GlobalUnlock(medium.hGlobal);
            format.cfFormat = CF_HDROP;
            if (FAILED(data->SetData(&format, &medium, TRUE)))
                ::GlobalFree(medium.hGlobal);
        }
        ImPlatform_DropSource_Win32 source;
        DWORD effect = DROPEFFECT_NONE;
        dropped = ::DoDragDrop(data, &source, DROPEFFECT_COPY | DROPEFFECT_LINK, &effect) == DRAGDROP_S_DROP && effect != DROPEFFECT_NONE;
        data->Release();
        // The button was released inside the drag: Dear ImGui did not see it
        if (ImGui::GetCurrentContext())
            for (int button = 0; button < 3; ++button)
                ImGui::GetIO().AddMouseButtonEvent(button, false);
    }
    ::OleUninitialize();
    return dropped;
#else
    (void)file_paths; (void)file_count; (void)text;
    return false;
#endif
}

// ============================================================================
// System theme
// ============================================================================

#if IMPLATFORM_NATIVE_LINUX && !IMPLATFORM_NATIVE_SDL3
// The settings portal's org.freedesktop.appearance color-scheme (1: dark, otherwise light), asked over D-Bus.
static ImPlatform_SystemTheme ImPlatform_Linux_SystemTheme()
{
    typedef void* (*BusGet)(int, void*);
    typedef void (*SetExit)(void*, unsigned);
    typedef void* (*NewCall)(const char*, const char*, const char*, const char*);
    typedef unsigned (*AppendArgs)(void*, int, ...);
    typedef void* (*SendBlock)(void*, void*, int, void*);
    typedef void (*Unref)(void*);
    typedef unsigned (*IterInit)(void*, void*);
    typedef int (*IterType)(void*);
    typedef void (*IterRecurse)(void*, void*);
    typedef void (*IterBasic)(void*, void*);
    static void* lib = dlopen("libdbus-1.so.3", RTLD_LAZY);
    static void* bus = NULL;
    static int unanswered = 0;      // Calls the portal left without a reply: after three it is not asked again
    if (!lib || unanswered >= 3)
        return ImPlatform_SystemTheme_Unknown;
    const BusGet bus_get = (BusGet)dlsym(lib, "dbus_bus_get");
    const SetExit set_exit = (SetExit)dlsym(lib, "dbus_connection_set_exit_on_disconnect");
    const NewCall new_call = (NewCall)dlsym(lib, "dbus_message_new_method_call");
    const AppendArgs append_args = (AppendArgs)dlsym(lib, "dbus_message_append_args");
    const SendBlock send_block = (SendBlock)dlsym(lib, "dbus_connection_send_with_reply_and_block");
    const Unref unref = (Unref)dlsym(lib, "dbus_message_unref");
    const IterInit iter_init = (IterInit)dlsym(lib, "dbus_message_iter_init");
    const IterType iter_type = (IterType)dlsym(lib, "dbus_message_iter_get_arg_type");
    const IterRecurse iter_recurse = (IterRecurse)dlsym(lib, "dbus_message_iter_recurse");
    const IterBasic iter_basic = (IterBasic)dlsym(lib, "dbus_message_iter_get_basic");
    if (!bus_get || !set_exit || !new_call || !append_args || !send_block || !unref || !iter_init || !iter_type || !iter_recurse || !iter_basic)
        return ImPlatform_SystemTheme_Unknown;
    if (!bus && (bus = bus_get(0, NULL)) != NULL)   // 0: the session bus
        set_exit(bus, 0);                           // libdbus would end the process when the bus goes away
    if (!bus)
        return ImPlatform_SystemTheme_Unknown;
    void* call = new_call("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.Settings", "Read");
    if (!call)
        return ImPlatform_SystemTheme_Unknown;
    const char* group = "org.freedesktop.appearance";
    const char* key = "color-scheme";
    append_args(call, 's', &group, 's', &key, 0);
    void* reply = send_block(bus, call, 200, NULL);     // Milliseconds
    unref(call);
    if (!reply)
    {
        unanswered++;
        return ImPlatform_SystemTheme_Unknown;
    }
    ImPlatform_SystemTheme theme = ImPlatform_SystemTheme_Unknown;
    struct { void* opaque[16]; } iter[4];   // A DBusMessageIter is smaller. The value comes as a variant in a variant.
    int depth = 0;
    if (iter_init(reply, &iter[0]))
    {
        for (; depth < 3 && iter_type(&iter[depth]) == 'v'; ++depth)
            iter_recurse(&iter[depth], &iter[depth + 1]);
        if (iter_type(&iter[depth]) == 'u')
        {
            unsigned scheme = 0;
            iter_basic(&iter[depth], &scheme);
            theme = scheme == 1 ? ImPlatform_SystemTheme_Dark : ImPlatform_SystemTheme_Light;
        }
    }
    unref(reply);
    return theme;
}
#endif

IMPLATFORM_API ImPlatform_SystemTheme ImPlatform_GetSystemTheme(void)
{
#if IMPLATFORM_NATIVE_SDL3
    const SDL_SystemTheme theme = SDL_GetSystemTheme();
    return theme == SDL_SYSTEM_THEME_DARK ? ImPlatform_SystemTheme_Dark : theme == SDL_SYSTEM_THEME_LIGHT ? ImPlatform_SystemTheme_Light : ImPlatform_SystemTheme_Unknown;
#elif defined(_WIN32)
    DWORD light = 1, size = sizeof(light);
    if (::RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme",
                       RRF_RT_REG_DWORD, NULL, &light, &size) != ERROR_SUCCESS)
        return ImPlatform_SystemTheme_Unknown;      // Before Windows 10 1607
    return light ? ImPlatform_SystemTheme_Light : ImPlatform_SystemTheme_Dark;
#elif IMPLATFORM_NATIVE_MACOS || IMPLATFORM_NATIVE_LINUX
    // Asked once a second at most (a round trip to the preferences daemon, or to the portal), whatever the call rate.
    static ImPlatform_SystemTheme theme = ImPlatform_SystemTheme_Unknown;
    static time_t asked = 0;
    const time_t now = time(NULL);
    if (now == asked)
        return theme;
    asked = now;
#if IMPLATFORM_NATIVE_MACOS
    // AppleInterfaceStyle is "Dark" in dark mode and absent in light mode
    CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);
    CFPropertyListRef style = CFPreferencesCopyAppValue(CFSTR("AppleInterfaceStyle"), kCFPreferencesCurrentApplication);
    const bool dark = style && CFGetTypeID(style) == CFStringGetTypeID() && CFStringCompare((CFStringRef)style, CFSTR("Dark"), 0) == kCFCompareEqualTo;
    if (style)
        CFRelease(style);
    theme = dark ? ImPlatform_SystemTheme_Dark : ImPlatform_SystemTheme_Light;
#else
    theme = ImPlatform_Linux_SystemTheme();
#endif
    return theme;
#else
    return ImPlatform_SystemTheme_Unknown;
#endif
}

// ============================================================================
// macOS: trackpad gestures, clipboard image
// ============================================================================

#if IMPLATFORM_NATIVE_MACOS
// AppKit's magnify and rotate events, seen before the window library's view receives them.
static void ImPlatform_Mac_WatchGestures()
{
    static bool watching = false;
    if (watching)
        return;
    watching = true;
    enum { Rotate = 18, Magnify = 30 };     // NSEventTypeRotate, NSEventTypeMagnify
    unsigned long long mask = 1ULL << Rotate;
#if IMPLATFORM_NATIVE_SDL3
#if SDL_VERSION_ATLEAST(3, 4, 0)
    const bool magnify = false;     // SDL reports the magnify events itself, as its pinch events
#else
    const bool magnify = true;
#endif
#else
    const bool magnify = true;
#endif
    if (magnify)
        mask |= 1ULL << Magnify;
    typedef id (^Handler)(id);
    const Handler handler = ^id(id event)
    {
        const unsigned long type = ((unsigned long (*)(id, SEL))objc_msgSend)(event, sel_registerName("type"));
        if (type == Magnify)
            ImPlatform_Touch_Zoom(1.0f + (float)((double (*)(id, SEL))objc_msgSend)(event, sel_registerName("magnification")));
        else if (type == Rotate)
            ImPlatform_Touch_Rotate(-((float (*)(id, SEL))objc_msgSend)(event, sel_registerName("rotation")));   // AppKit turns counterclockwise
        return event;
    };
    ((id (*)(Class, SEL, unsigned long long, Handler))objc_msgSend)(objc_getClass("NSEvent"), sel_registerName("addLocalMonitorForEventsMatchingMask:handler:"), mask, handler);
}

// The first PNG or TIFF item of the clipboard, decoded by ImageIO and drawn into RGBA8.
static bool ImPlatform_Native_ReadClipboardImage(ImVector<unsigned char>* rgba, int* width, int* height)
{
    PasteboardRef pasteboard = NULL;
    if (PasteboardCreate(kPasteboardClipboard, &pasteboard) != noErr)
        return false;
    PasteboardSynchronize(pasteboard);
    const CFStringRef flavors[2] = { CFSTR("public.png"), CFSTR("public.tiff") };
    CFDataRef data = NULL;
    ItemCount count = 0;
    PasteboardGetItemCount(pasteboard, &count);
    for (ItemCount i = 1; i <= count && !data; ++i)
    {
        PasteboardItemID item = NULL;
        if (PasteboardGetItemIdentifier(pasteboard, i, &item) != noErr)
            continue;
        for (int f = 0; f < 2 && !data; ++f)
            if (PasteboardCopyItemFlavorData(pasteboard, item, flavors[f], &data) != noErr)
                data = NULL;
    }
    CFRelease(pasteboard);
    if (!data)
        return false;
    bool ok = false;
    CGImageSourceRef source = CGImageSourceCreateWithData(data, NULL);
    CGImageRef image = source ? CGImageSourceCreateImageAtIndex(source, 0, NULL) : NULL;
    if (image)
    {
        const size_t w = CGImageGetWidth(image), h = CGImageGetHeight(image);
        rgba->resize((int)(w * h * 4));
        memset(rgba->Data, 0, w * h * 4);
        CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        CGContextRef context = CGBitmapContextCreate(rgba->Data, w, h, 8, w * 4, space, (uint32_t)kCGImageAlphaPremultipliedLast | (uint32_t)kCGBitmapByteOrder32Big);
        if (context)
        {
            CGContextDrawImage(context, CGRectMake(0, 0, (CGFloat)w, (CGFloat)h), image);
            CGContextRelease(context);
            for (size_t i = 3; i < w * h * 4; i += 4)
                rgba->Data[i] = 255;
            *width = (int)w;
            *height = (int)h;
            ok = true;
        }
        CGColorSpaceRelease(space);
        CGImageRelease(image);
    }
    if (source)
        CFRelease(source);
    CFRelease(data);
    return ok;
}

// The image as a PNG, the one item of the clipboard.
static bool ImPlatform_Native_WriteClipboardImage(const unsigned char* rgba, int width, int height)
{
    const size_t w = (size_t)width, h = (size_t)height;
    CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, rgba, w * h * 4, NULL);
    CGImageRef image = CGImageCreate(w, h, 8, 32, w * 4, space, (uint32_t)kCGImageAlphaLast | (uint32_t)kCGBitmapByteOrder32Big, provider, NULL, false, kCGRenderingIntentDefault);
    CFMutableDataRef png = CFDataCreateMutable(NULL, 0);
    CGImageDestinationRef destination = CGImageDestinationCreateWithData(png, CFSTR("public.png"), 1, NULL);
    bool ok = false;
    if (image && destination)
    {
        CGImageDestinationAddImage(destination, image, NULL);
        ok = CGImageDestinationFinalize(destination);
    }
    PasteboardRef pasteboard = NULL;
    if (ok && PasteboardCreate(kPasteboardClipboard, &pasteboard) == noErr)
    {
        PasteboardClear(pasteboard);
        PasteboardSynchronize(pasteboard);
        ok = PasteboardPutItemFlavor(pasteboard, (PasteboardItemID)1, CFSTR("public.png"), png, 0) == noErr;
        CFRelease(pasteboard);
    }
    else
        ok = false;
    if (destination)
        CFRelease(destination);
    CFRelease(png);
    CGImageRelease(image);
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(space);
    return ok;
}
#endif // IMPLATFORM_NATIVE_MACOS

// ============================================================================
// Linux: PNG through libpng, the clipboard through SDL3 or X11
// ============================================================================

#if IMPLATFORM_NATIVE_LINUX
// libpng 1.6's png_image, for its simplified interface.
struct ImPlatform_PngImage
{
    void*    opaque;
    unsigned version, width, height, format, flags, colormap_entries, warning_or_error;
    char     message[64];
};

static void* ImPlatform_Png(const char* function)
{
    static void* lib = dlopen("libpng16.so.16", RTLD_LAZY);
    return lib ? dlsym(lib, function) : NULL;
}

static bool ImPlatform_PngDecode(const void* png, size_t size, ImVector<unsigned char>* rgba, int* width, int* height)
{
    typedef int (*BeginRead)(ImPlatform_PngImage*, const void*, size_t);
    typedef int (*FinishRead)(ImPlatform_PngImage*, const void*, void*, int, void*);
    typedef void (*Free)(ImPlatform_PngImage*);
    const BeginRead begin = (BeginRead)ImPlatform_Png("png_image_begin_read_from_memory");
    const FinishRead finish = (FinishRead)ImPlatform_Png("png_image_finish_read");
    const Free release = (Free)ImPlatform_Png("png_image_free");
    ImPlatform_PngImage image;
    memset(&image, 0, sizeof(image));
    image.version = 1;
    if (!begin || !finish || !release || !begin(&image, png, size))
        return false;
    image.format = 3;       // PNG_FORMAT_RGBA
    rgba->resize((int)(image.width * image.height * 4));
    *width = (int)image.width;
    *height = (int)image.height;
    if (finish(&image, NULL, rgba->Data, 0, NULL))
        return true;
    release(&image);
    return false;
}

static bool ImPlatform_PngEncode(const unsigned char* rgba, int width, int height, ImVector<unsigned char>* png)
{
    typedef int (*Write)(ImPlatform_PngImage*, void*, size_t*, int, const void*, int, const void*);
    const Write encode = (Write)ImPlatform_Png("png_image_write_to_memory");
    size_t bytes = 0;
    for (int pass = 0; encode && pass < 2; ++pass)  // The size, then the bytes
    {
        ImPlatform_PngImage image;
        memset(&image, 0, sizeof(image));
        image.version = 1;
        image.width = (unsigned)width;
        image.height = (unsigned)height;
        image.format = 3;   // PNG_FORMAT_RGBA
        png->resize((int)bytes);
        if (!encode(&image, pass ? png->Data : NULL, &bytes, 0, rgba, 0, NULL))
            return false;
    }
    png->resize((int)bytes);
    return encode != NULL;
}

// The PNG this application put on the clipboard: it is given to the applications that paste it for as long as this
// one runs and nothing else is copied.
static ImVector<unsigned char> g_ClipboardPng;
#endif // IMPLATFORM_NATIVE_LINUX

#if IMPLATFORM_NATIVE_X11
// Xlib, loaded at run time and declared here: its header defines names (None, Status, Bool) into every file after it.
typedef unsigned long ImPlatform_XID;   // Window, Atom, Time
union ImPlatform_XEvent
{
    int type;
    struct { int type; unsigned long serial; int send_event; void* display; ImPlatform_XID owner, requestor, selection, target, property, time; } request;  // XSelectionRequestEvent
    struct { int type; unsigned long serial; int send_event; void* display; ImPlatform_XID requestor, selection, target, property, time; } selection;       // XSelectionEvent
    struct { int type; unsigned long serial; int send_event; void* display; ImPlatform_XID window, atom, time; int state; } property;                       // XPropertyEvent
    struct { int type; unsigned long serial; int send_event; void* display; ImPlatform_XID window, message_type; int format; long l[5]; } client;           // XClientMessageEvent
    long pad[24];
};
enum { ImPlatform_XPropertyNotify = 28, ImPlatform_XSelectionClear = 29, ImPlatform_XSelectionRequest = 30, ImPlatform_XSelectionNotify = 31, ImPlatform_XClientMessage = 33 };

static struct
{
    void*          (*OpenDisplay)(const char*);
    ImPlatform_XID (*RootOf)(void*);        // XDefaultRootWindow: SDL2's headers bring Xlib's macro of the shorter name
    ImPlatform_XID (*CreateSimpleWindow)(void*, ImPlatform_XID, int, int, unsigned, unsigned, unsigned, unsigned long, unsigned long);
    int            (*SelectInput)(void*, ImPlatform_XID, long);
    ImPlatform_XID (*InternAtom)(void*, const char*, int);
    int            (*Pending)(void*);
    int            (*NextEvent)(void*, ImPlatform_XEvent*);
    int            (*ConvertSelection)(void*, ImPlatform_XID, ImPlatform_XID, ImPlatform_XID, ImPlatform_XID, ImPlatform_XID);
    int            (*GetWindowProperty)(void*, ImPlatform_XID, ImPlatform_XID, long, long, int, ImPlatform_XID, ImPlatform_XID*, int*, unsigned long*, unsigned long*, unsigned char**);
    int            (*Free)(void*);
    int            (*SetSelectionOwner)(void*, ImPlatform_XID, ImPlatform_XID, ImPlatform_XID);
    ImPlatform_XID (*GetSelectionOwner)(void*, ImPlatform_XID);
    int            (*ChangeProperty)(void*, ImPlatform_XID, ImPlatform_XID, ImPlatform_XID, int, int, const unsigned char*, int);
    int            (*SendEvent)(void*, ImPlatform_XID, int, long, ImPlatform_XEvent*);
    int            (*Flush)(void*);
    int            (*Sync)(void*, int);
    int            (*TranslateCoordinates)(void*, ImPlatform_XID, ImPlatform_XID, int, int, int*, int*, ImPlatform_XID*);
    int            (*DeleteProperty)(void*, ImPlatform_XID, ImPlatform_XID);
    int            (*(*SetErrorHandler)(int (*)(void*, void*)))(void*, void*);
    void*          display;
    ImPlatform_XID window, clipboard, png, targets, incr, property;
} g_X;

// A connection of this file's own, with a window that is never shown: the window library's event loop does not see
// its events, and answers nothing in its place.
static bool ImPlatform_X11_Open()
{
    static bool tried = false;
    if (tried)
        return g_X.display != NULL;
    tried = true;
    void* lib = dlopen("libX11.so.6", RTLD_LAZY);
    if (!lib)
        return false;
    bool ok = true;
#define IMPLATFORM_X_AS(name, symbol) { void* f = dlsym(lib, symbol); memcpy(&g_X.name, &f, sizeof(f)); ok = f && ok; }
#define IMPLATFORM_X(name) IMPLATFORM_X_AS(name, "X" #name)
    IMPLATFORM_X(OpenDisplay); IMPLATFORM_X_AS(RootOf, "XDefaultRootWindow"); IMPLATFORM_X(CreateSimpleWindow); IMPLATFORM_X(SelectInput);
    IMPLATFORM_X(InternAtom); IMPLATFORM_X(Pending); IMPLATFORM_X(NextEvent); IMPLATFORM_X(ConvertSelection);
    IMPLATFORM_X(GetWindowProperty); IMPLATFORM_X(Free); IMPLATFORM_X(SetSelectionOwner); IMPLATFORM_X(GetSelectionOwner);
    IMPLATFORM_X(ChangeProperty); IMPLATFORM_X(SendEvent); IMPLATFORM_X(Flush); IMPLATFORM_X(Sync);
    IMPLATFORM_X(SetErrorHandler); IMPLATFORM_X(TranslateCoordinates); IMPLATFORM_X(DeleteProperty);
#undef IMPLATFORM_X
#undef IMPLATFORM_X_AS
    void* display = ok ? g_X.OpenDisplay(NULL) : NULL;
    if (!display)
        return false;
    g_X.window = g_X.CreateSimpleWindow(display, g_X.RootOf(display), 0, 0, 1, 1, 0, 0, 0);
    g_X.SelectInput(display, g_X.window, 1L << 22);     // PropertyChangeMask: the chunks of an incremental transfer
    g_X.clipboard = g_X.InternAtom(display, "CLIPBOARD", 0);
    g_X.png = g_X.InternAtom(display, "image/png", 0);
    g_X.targets = g_X.InternAtom(display, "TARGETS", 0);
    g_X.incr = g_X.InternAtom(display, "INCR", 0);
    g_X.property = g_X.InternAtom(display, "IMPLATFORM_CLIPBOARD", 0);
    g_X.display = display;
    return true;
}

// Xlib ends the process on an error, and a window that asked for the clipboard can go away while it is answered:
// the errors are ignored while this connection is used. Its errors only arrive while it is read, which only happens
// here, so the handler of the window library is back in place for its own connection.
static int ImPlatform_X11_IgnoreError(void*, void*)
{
    return 0;
}
struct ImPlatform_X11_Errors
{
    int (*previous)(void*, void*);
    ImPlatform_X11_Errors() { previous = g_X.SetErrorHandler(ImPlatform_X11_IgnoreError); }
    ~ImPlatform_X11_Errors() { g_X.Sync(g_X.display, 0); g_X.SetErrorHandler(previous); }
};

// An image larger than this goes to the application that pastes it in increments (INCR), as the X11 conventions ask.
enum { ImPlatform_XIncrement = 256 * 1024 };
struct ImPlatform_XTransfer
{
    ImPlatform_XID requestor, property;
    int            sent;
};
static ImVector<ImPlatform_XTransfer> g_XTransfers;

#if IMPLATFORM_APP_SUPPORT_DROP_FILE
static void ImPlatform_X11_DropMessage(const ImPlatform_XEvent* event);
#endif

// Answers an application that asks what this one put on the clipboard, gives the next increment of an image it is
// reading, forgets the image when another application copied, or follows a drag over the window.
static void ImPlatform_X11_Serve(const ImPlatform_XEvent* event)
{
#if IMPLATFORM_APP_SUPPORT_DROP_FILE
    if (event->type == ImPlatform_XClientMessage)
        ImPlatform_X11_DropMessage(event);
#endif
    if (event->type == ImPlatform_XSelectionClear)
    {
        g_ClipboardPng.clear();
        g_XTransfers.clear();
    }
    if (event->type == ImPlatform_XPropertyNotify && event->property.state == 1)   // PropertyDelete: the increment was read
        for (int i = 0; i < g_XTransfers.Size; ++i)
        {
            ImPlatform_XTransfer& transfer = g_XTransfers[i];
            if (transfer.requestor != event->property.window || transfer.property != event->property.atom)
                continue;
            const int left = g_ClipboardPng.Size - transfer.sent;
            const int bytes = left < ImPlatform_XIncrement ? left : (int)ImPlatform_XIncrement;
            g_X.ChangeProperty(g_X.display, transfer.requestor, transfer.property, g_X.png, 8, 0, g_ClipboardPng.Data + transfer.sent, bytes);
            g_X.Flush(g_X.display);
            transfer.sent += bytes;
            if (bytes == 0)     // The empty increment ends the transfer
            {
                if (transfer.requestor != g_X.window)
                    g_X.SelectInput(g_X.display, transfer.requestor, 0);
                g_XTransfers.erase(&transfer);
            }
            break;
        }
    if (event->type != ImPlatform_XSelectionRequest)
        return;
    ImPlatform_XEvent reply;
    memset(&reply, 0, sizeof(reply));
    reply.selection.type = ImPlatform_XSelectionNotify;
    reply.selection.requestor = event->request.requestor;
    reply.selection.selection = event->request.selection;
    reply.selection.target = event->request.target;
    reply.selection.time = event->request.time;
    const ImPlatform_XID requestor = event->request.requestor;
    const ImPlatform_XID property = event->request.property ? event->request.property : event->request.target;
    if (event->request.target == g_X.targets)
    {
        const ImPlatform_XID targets[2] = { g_X.targets, g_X.png };
        g_X.ChangeProperty(g_X.display, requestor, property, 4, 32, 0, (const unsigned char*)targets, 2);   // XA_ATOM, PropModeReplace
        reply.selection.property = property;
    }
    else if (event->request.target == g_X.png && g_ClipboardPng.Size > 0)
    {
        if (g_ClipboardPng.Size <= ImPlatform_XIncrement)
            g_X.ChangeProperty(g_X.display, requestor, property, g_X.png, 8, 0, g_ClipboardPng.Data, g_ClipboardPng.Size);
        else
        {
            // The property announces the size; each time the requestor deletes it, the next increment replaces it
            const long size = g_ClipboardPng.Size;
            const ImPlatform_XTransfer transfer = { requestor, property, 0 };
            if (requestor != g_X.window)
                g_X.SelectInput(g_X.display, requestor, 1L << 22);     // PropertyChangeMask
            g_X.ChangeProperty(g_X.display, requestor, property, g_X.incr, 32, 0, (const unsigned char*)&size, 1);
            g_XTransfers.push_back(transfer);
        }
        reply.selection.property = property;
    }
    g_X.SendEvent(g_X.display, requestor, 0, 0, &reply);
    g_X.Flush(g_X.display);
}

// Waits for the answer to this file's own request (the selection notification, or a new value of its property),
// answering the other events. Returns false after a second without it.
static bool ImPlatform_X11_Wait(int type, ImPlatform_XEvent* event)
{
    for (int ms = 0; ms < 1000; ++ms)
    {
        while (g_X.Pending(g_X.display))
        {
            g_X.NextEvent(g_X.display, event);
            if (event->type == type && (type != ImPlatform_XPropertyNotify
                || (event->property.state == 0 && event->property.window == g_X.window && event->property.atom == g_X.property)))     // PropertyNewValue
                return true;
            ImPlatform_X11_Serve(event);
        }
        usleep(1000);
    }
    return false;
}

// The data of a selection (the clipboard's PNG, a drag's files), whole or in the increments large data comes in.
static bool ImPlatform_X11_ReadSelection(ImPlatform_XID selection, ImPlatform_XID target, ImPlatform_XID time, ImVector<unsigned char>* out)
{
    ImPlatform_XEvent event;
    g_X.ConvertSelection(g_X.display, selection, target, g_X.property, g_X.window, time);
    if (!ImPlatform_X11_Wait(ImPlatform_XSelectionNotify, &event) || !event.selection.property)
        return false;
    for (bool incremental = false;;)
    {
        ImPlatform_XID type = 0;
        int format = 0;
        unsigned long count = 0, left = 0;
        unsigned char* data = NULL;
        if (g_X.GetWindowProperty(g_X.display, g_X.window, g_X.property, 0, 0x1fffffff, 1, 0, &type, &format, &count, &left, &data) != 0)
            return false;
        const bool announce = type == g_X.incr && !incremental;     // The owner announces increments: the data follows
        if (!announce && format == 8 && count > 0)
        {
            const int at = out->Size;
            out->resize(at + (int)count);
            memcpy(out->Data + at, data, count);
        }
        if (data)
            g_X.Free(data);
        if (announce)
            incremental = true;
        else if (!incremental || count == 0)    // Whole in one property, or the empty increment that ends the transfer
            return out->Size > 0;
        if (!ImPlatform_X11_Wait(ImPlatform_XPropertyNotify, &event))
            return false;
    }
}

#if IMPLATFORM_APP_SUPPORT_DROP_FILE
// Drags over the main window (the XDND protocol). GLFW reports the dropped files only, SDL2 nothing before the
// release: the window's XdndProxy property sends the messages of a drag to this connection's window, which answers
// them in the window's place.
static struct
{
    ImPlatform_XID aware, proxy, enter, position, status, leave, drop, finished, selection, copy, type_list;
    ImPlatform_XID uri_list, utf8, text_utf8, text;
    ImPlatform_XID target;      // The window this connection answers for; 0 while it answers for none
    ImPlatform_XID source;      // The window the drag comes from
    ImPlatform_XID format;      // What the drag is asked for when it drops; 0 when it carries neither files nor text
    bool           wanted;      // The application set a drop callback
    ImVec2         pos;
} g_XDrop;

// Answers the source of the drag: XdndStatus while it moves, XdndFinished once it dropped.
static void ImPlatform_X11_DropReply(ImPlatform_XID message, bool accepted)
{
    ImPlatform_XEvent reply;
    memset(&reply, 0, sizeof(reply));
    reply.client.type = ImPlatform_XClientMessage;
    reply.client.window = g_XDrop.source;
    reply.client.message_type = message;
    reply.client.format = 32;
    reply.client.l[0] = (long)g_XDrop.target;
    reply.client.l[1] = accepted ? 1 : 0;
    reply.client.l[message == g_XDrop.status ? 4 : 2] = accepted ? (long)g_XDrop.copy : 0;     // The action taken
    g_X.SendEvent(g_X.display, g_XDrop.source, 0, 0, &reply);
    g_X.Flush(g_X.display);
}

// What a drag dropped: the lines of a URI list (a File event per file, a Text event per link), or a text.
static void ImPlatform_X11_NotifyDropped(char* data, bool uris)
{
    if (!uris)
    {
        if (data[0])
            ImPlatform_NotifyDrop(ImPlatform_DropEvent_Text, data, g_XDrop.pos);
        return;
    }
    for (char* line = data; *line;)
    {
        char* end = line;
        while (*end && *end != '\r' && *end != '\n')
            ++end;
        char* next = end;
        while (*next == '\r' || *next == '\n')
            ++next;
        *end = 0;
        char* path = strncmp(line, "file://", 7) == 0 ? strchr(line + 7, '/') : NULL;   // After the host name, empty as a rule
        if (path)
        {
            char* out = path;
            for (const char* in = path; *in; ++in)      // %XX is a byte
            {
                unsigned value = 0;
                if (in[0] == '%' && in[1] && in[2] && sscanf(in + 1, "%2x", &value) == 1)
                {
                    *out++ = (char)value;
                    in += 2;
                }
                else
                    *out++ = *in;
            }
            *out = 0;
            ImPlatform_NotifyDrop(ImPlatform_DropEvent_File, path, g_XDrop.pos);
        }
        else if (line[0] && line[0] != '#')
            ImPlatform_NotifyDrop(ImPlatform_DropEvent_Text, line, g_XDrop.pos);    // A link: its URL
        line = next;
    }
}

static void ImPlatform_X11_DropMessage(const ImPlatform_XEvent* event)
{
    const ImPlatform_XID message = event->client.message_type;
    const long* l = event->client.l;
    if (message == g_XDrop.enter)
    {
        // Up to three types come in the message; more are in a property of the source
        ImPlatform_XID types[64];
        int count = 0;
        g_XDrop.source = (ImPlatform_XID)l[0];
        if (l[1] & 1)
        {
            ImPlatform_XID type = 0;
            int format = 0;
            unsigned long listed = 0, left = 0;
            unsigned char* data = NULL;
            if (g_X.GetWindowProperty(g_X.display, g_XDrop.source, g_XDrop.type_list, 0, 64, 0, 4, &type, &format, &listed, &left, &data) == 0 && data)    // XA_ATOM
            {
                for (; format == 32 && count < (int)listed && count < 64; ++count)
                    types[count] = ((const ImPlatform_XID*)data)[count];
                g_X.Free(data);
            }
        }
        else
            for (int i = 2; i < 5; ++i)
                if (l[i])
                    types[count++] = (ImPlatform_XID)l[i];
        // Files and links first, then text in the best encoding the drag offers
        const ImPlatform_XID formats[4] = { g_XDrop.uri_list, g_XDrop.utf8, g_XDrop.text_utf8, g_XDrop.text };
        g_XDrop.format = 0;
        for (int f = 0; f < 4 && !g_XDrop.format; ++f)
            for (int i = 0; i < count; ++i)
                if (types[i] == formats[f])
                    g_XDrop.format = formats[f];
        if (g_XDrop.format)
            ImPlatform_NotifyDrop(ImPlatform_DropEvent_Begin, NULL, g_XDrop.pos);
    }
    else if (message == g_XDrop.position)
    {
        int x = 0, y = 0;
        ImPlatform_XID child = 0;
        g_XDrop.source = (ImPlatform_XID)l[0];
        g_X.TranslateCoordinates(g_X.display, g_X.RootOf(g_X.display), g_XDrop.target, (short)(l[2] >> 16), (short)(l[2] & 0xffff), &x, &y, &child);
        g_XDrop.pos = ImVec2((float)x, (float)y);
        if (g_XDrop.format)
            ImPlatform_NotifyDrop(ImPlatform_DropEvent_Position, NULL, g_XDrop.pos);
        ImPlatform_X11_DropReply(g_XDrop.status, g_XDrop.format != 0);
    }
    else if (message == g_XDrop.leave)
    {
        if (g_XDrop.format)
            ImPlatform_NotifyDrop(ImPlatform_DropEvent_End, NULL, g_XDrop.pos);
        g_XDrop.format = 0;
    }
    else if (message == g_XDrop.drop)
    {
        const ImPlatform_XID format = g_XDrop.format;
        g_XDrop.source = (ImPlatform_XID)l[0];
        g_XDrop.format = 0;     // Messages of another drag may come while this one is read
        ImVector<unsigned char> data;
        if (format && ImPlatform_X11_ReadSelection(g_XDrop.selection, format, (ImPlatform_XID)l[2], &data))
        {
            data.push_back(0);
            ImPlatform_X11_NotifyDropped((char*)data.Data, format == g_XDrop.uri_list);
        }
        if (format)
            ImPlatform_NotifyDrop(ImPlatform_DropEvent_End, NULL, g_XDrop.pos);
        ImPlatform_X11_DropReply(g_XDrop.finished, format != 0);
    }
}

// Stands for the main window in the drags over it while a drop callback is set, and no longer when none is.
static void ImPlatform_X11_WatchDrops()
{
    const ImPlatform_XID window = g_XDrop.wanted ? (ImPlatform_XID)ImPlatform_App_GetX11Window() : 0;
    if (window == g_XDrop.target || !ImPlatform_X11_Open())
        return;
    ImPlatform_X11_Errors ignored;
    if (!g_XDrop.enter)
    {
#define IMPLATFORM_ATOM(member, name) g_XDrop.member = g_X.InternAtom(g_X.display, name, 0)
        IMPLATFORM_ATOM(aware, "XdndAware"); IMPLATFORM_ATOM(proxy, "XdndProxy"); IMPLATFORM_ATOM(enter, "XdndEnter");
        IMPLATFORM_ATOM(position, "XdndPosition"); IMPLATFORM_ATOM(status, "XdndStatus"); IMPLATFORM_ATOM(leave, "XdndLeave");
        IMPLATFORM_ATOM(drop, "XdndDrop"); IMPLATFORM_ATOM(finished, "XdndFinished"); IMPLATFORM_ATOM(selection, "XdndSelection");
        IMPLATFORM_ATOM(copy, "XdndActionCopy"); IMPLATFORM_ATOM(type_list, "XdndTypeList"); IMPLATFORM_ATOM(uri_list, "text/uri-list");
        IMPLATFORM_ATOM(utf8, "UTF8_STRING"); IMPLATFORM_ATOM(text_utf8, "text/plain;charset=utf-8"); IMPLATFORM_ATOM(text, "text/plain");
#undef IMPLATFORM_ATOM
    }
    if (g_XDrop.target)
        g_X.DeleteProperty(g_X.display, g_XDrop.target, g_XDrop.proxy);
    g_XDrop.target = window;
    if (window)
    {
        // The window says where its drags go; the proxy names itself, which is how a source tells it is one
        const long version = 5;
        g_X.ChangeProperty(g_X.display, g_X.window, g_XDrop.aware, 4, 32, 0, (const unsigned char*)&version, 1);       // XA_ATOM
        g_X.ChangeProperty(g_X.display, g_X.window, g_XDrop.proxy, 33, 32, 0, (const unsigned char*)&g_X.window, 1);   // XA_WINDOW
        g_X.ChangeProperty(g_X.display, window, g_XDrop.proxy, 33, 32, 0, (const unsigned char*)&g_X.window, 1);
    }
    g_X.Flush(g_X.display);
}
#endif // IMPLATFORM_APP_SUPPORT_DROP_FILE
#endif // IMPLATFORM_NATIVE_X11

#ifndef _WIN32
void ImPlatform_Native_AcceptDrops(bool accept)
{
#if IMPLATFORM_NATIVE_X11 && IMPLATFORM_APP_SUPPORT_DROP_FILE
    g_XDrop.wanted = accept;    // Applied by ImPlatform_Native_Pump, once the window exists
#else
    (void)accept;
#endif
}
#endif

#if IMPLATFORM_NATIVE_LINUX
#if IMPLATFORM_NATIVE_SDL3
static const void* SDLCALL ImPlatform_SDL3_ClipboardPng(void*, const char*, size_t* size)
{
    *size = (size_t)g_ClipboardPng.Size;
    return g_ClipboardPng.Data;
}
#endif

static bool ImPlatform_Native_ReadClipboardImage(ImVector<unsigned char>* rgba, int* width, int* height)
{
#if IMPLATFORM_NATIVE_SDL3
    size_t size = 0;
    void* png = SDL_GetClipboardData("image/png", &size);
    const bool ok = png && size > 0 && ImPlatform_PngDecode(png, size, rgba, width, height);
    SDL_free(png);
    return ok;
#elif IMPLATFORM_NATIVE_X11
    ImVector<unsigned char> png;
    if (!ImPlatform_X11_Open())
        return false;
    ImPlatform_X11_Errors ignored;
    return ImPlatform_X11_ReadSelection(g_X.clipboard, g_X.png, 0, &png) && ImPlatform_PngDecode(png.Data, (size_t)png.Size, rgba, width, height);
#else
    (void)rgba; (void)width; (void)height;
    return false;
#endif
}

static bool ImPlatform_Native_WriteClipboardImage(const unsigned char* rgba, int width, int height)
{
#if IMPLATFORM_NATIVE_SDL3
    const char* types[1] = { "image/png" };
    return ImPlatform_PngEncode(rgba, width, height, &g_ClipboardPng) && SDL_SetClipboardData(ImPlatform_SDL3_ClipboardPng, NULL, NULL, types, 1);
#elif IMPLATFORM_NATIVE_X11
    if (!ImPlatform_X11_Open() || !ImPlatform_PngEncode(rgba, width, height, &g_ClipboardPng))
        return false;
    ImPlatform_X11_Errors ignored;
    g_XTransfers.clear();   // An application still reading the previous image gets no more of it
    g_X.SetSelectionOwner(g_X.display, g_X.clipboard, g_X.window, 0);   // CurrentTime
    g_X.Flush(g_X.display);
    return g_X.GetSelectionOwner(g_X.display, g_X.clipboard) == g_X.window;
#else
    (void)rgba; (void)width; (void)height;
    return false;
#endif
}
#endif // IMPLATFORM_NATIVE_LINUX

// What the system libraries need between two frames (ImPlatform_PlatformEvents).
void ImPlatform_Native_Pump(void)
{
#if defined(_WIN32)
#if IMPLATFORM_NATIVE_WINDOW
#if IM_CURRENT_PLATFORM != IM_PLATFORM_WIN32
    HWND main = (HWND)ImPlatform_Native_MainWindow();
    if (main && main != g_NativeHooked)
    {
        g_NativeNextProc = (WNDPROC)::SetWindowLongPtrW(main, GWLP_WNDPROC, (LONG_PTR)ImPlatform_Native_WndProc);
        g_NativeHooked = main;
    }
#endif
#if IMPLATFORM_APP_SUPPORT_DROP_FILE
    if (g_DropWanted || g_DropWindows.Size > 0)
        ImPlatform_Native_SyncDrops();      // Dear ImGui's platform windows come and go
#endif
#endif
#elif IMPLATFORM_NATIVE_MACOS
    ImPlatform_Mac_WatchGestures();
#elif IMPLATFORM_NATIVE_X11
#if IMPLATFORM_APP_SUPPORT_DROP_FILE
    ImPlatform_X11_WatchDrops();
#endif
    if (g_X.display && g_X.Pending(g_X.display))
    {
        ImPlatform_X11_Errors ignored;
        ImPlatform_XEvent event;
        while (g_X.Pending(g_X.display))
        {
            g_X.NextEvent(g_X.display, &event);
            ImPlatform_X11_Serve(&event);
        }
    }
#endif
}

// Before the main window is destroyed.
void ImPlatform_Native_Shutdown(void)
{
#if defined(_WIN32) && IMPLATFORM_NATIVE_WINDOW
#if IMPLATFORM_APP_SUPPORT_DROP_FILE
    g_DropWanted = false;
    ImPlatform_Native_SyncDrops();
    if (g_DropOle)
        ::OleUninitialize();
    g_DropOle = false;
#endif
#if IM_CURRENT_PLATFORM != IM_PLATFORM_WIN32
    // Dear ImGui's GLFW backend puts GLFW's own procedure back when it shuts down: this one is then already out
    if (g_NativeHooked && ::IsWindow(g_NativeHooked) && (WNDPROC)::GetWindowLongPtrW(g_NativeHooked, GWLP_WNDPROC) == ImPlatform_Native_WndProc)
        ::SetWindowLongPtrW(g_NativeHooked, GWLP_WNDPROC, (LONG_PTR)g_NativeNextProc);
    g_NativeHooked = NULL;
#endif
#endif
}

// ============================================================================
// Clipboard image
// ============================================================================

IMPLATFORM_API bool ImPlatform_SupportsClipboardImage(void)
{
#if defined(_WIN32) || IMPLATFORM_NATIVE_MACOS
    return true;
#elif IMPLATFORM_NATIVE_SDL3 && IMPLATFORM_NATIVE_LINUX
    return ImPlatform_Png("png_image_write_to_memory") != NULL;
#elif IMPLATFORM_NATIVE_X11
    return ImPlatform_Png("png_image_write_to_memory") != NULL && ImPlatform_X11_Open();
#else
    return false;
#endif
}

#ifdef _WIN32

IMPLATFORM_API bool ImPlatform_SetClipboardImage(const unsigned char* rgba, int width, int height)
{
    if (!rgba || width <= 0 || height <= 0)
        return false;
    const size_t bytes = (size_t)width * (size_t)height * 4;
    HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + bytes);
    BITMAPINFOHEADER* header = memory ? (BITMAPINFOHEADER*)::GlobalLock(memory) : NULL;
    if (!header)
        return false;
    memset(header, 0, sizeof(*header));
    header->biSize = sizeof(*header);
    header->biWidth = width;
    header->biHeight = height;      // Rows bottom-up, the form every application reads
    header->biPlanes = 1;
    header->biBitCount = 32;
    header->biCompression = BI_RGB;
    header->biSizeImage = (DWORD)bytes;
    unsigned char* bgra = (unsigned char*)(header + 1);
    for (int y = height - 1; y >= 0; --y)
    {
        const unsigned char* src = rgba + (size_t)y * (size_t)width * 4;
        for (int x = 0; x < width; ++x, src += 4, bgra += 4)
        {
            bgra[0] = src[2];
            bgra[1] = src[1];
            bgra[2] = src[0];
            bgra[3] = src[3];
        }
    }
    ::GlobalUnlock(memory);
    bool ok = false;
    if (::OpenClipboard(NULL))
    {
        ::EmptyClipboard();
        ok = ::SetClipboardData(CF_DIB, memory) != NULL;    // The clipboard owns the memory from here
        ::CloseClipboard();
    }
    if (!ok)
        ::GlobalFree(memory);
    return ok;
}

// The bitmap on the clipboard: its size (rgba NULL), or its pixels when it has the size given.
static bool ImPlatform_ClipboardBitmap(unsigned char* rgba, int* width, int* height)
{
    if (!width || !height || !::OpenClipboard(NULL))
        return false;
    bool ok = false;
    BITMAP info;
    HBITMAP bitmap = (HBITMAP)::GetClipboardData(CF_BITMAP);    // Windows makes it from whichever bitmap format is there
    if (bitmap && ::GetObjectW(bitmap, sizeof(info), &info))
    {
        if (!rgba)
        {
            *width = info.bmWidth;
            *height = info.bmHeight;
            ok = true;
        }
        else if (*width == info.bmWidth && *height == info.bmHeight)
        {
            BITMAPINFO bi;
            memset(&bi, 0, sizeof(bi));
            bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
            bi.bmiHeader.biWidth = info.bmWidth;
            bi.bmiHeader.biHeight = -info.bmHeight;     // Rows top-down
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            HDC dc = ::GetDC(NULL);
            ok = ::GetDIBits(dc, bitmap, 0, (UINT)info.bmHeight, rgba, &bi, DIB_RGB_COLORS) == info.bmHeight;
            ::ReleaseDC(NULL, dc);
            for (unsigned char* p = rgba, *end = rgba + (size_t)info.bmWidth * (size_t)info.bmHeight * 4; ok && p < end; p += 4)
            {
                const unsigned char blue = p[0];
                p[0] = p[2];
                p[2] = blue;
                p[3] = 255;
            }
        }
    }
    ::CloseClipboard();
    return ok;
}

IMPLATFORM_API bool ImPlatform_GetClipboardImageSize(int* width, int* height)
{
    return ImPlatform_ClipboardBitmap(NULL, width, height);
}

IMPLATFORM_API bool ImPlatform_GetClipboardImage(unsigned char* rgba, int width, int height)
{
    return rgba && ImPlatform_ClipboardBitmap(rgba, &width, &height);
}

#elif IMPLATFORM_NATIVE_MACOS || IMPLATFORM_NATIVE_LINUX

// The image ImPlatform_GetClipboardImageSize() read, kept for the ImPlatform_GetClipboardImage() that follows it:
// these clipboards hold an encoded image, decoded once.
static ImVector<unsigned char> g_ClipboardImage;
static int g_ClipboardImageSize[2] = { 0, 0 };

IMPLATFORM_API bool ImPlatform_SetClipboardImage(const unsigned char* rgba, int width, int height)
{
    return rgba && width > 0 && height > 0 && ImPlatform_Native_WriteClipboardImage(rgba, width, height);
}

IMPLATFORM_API bool ImPlatform_GetClipboardImageSize(int* width, int* height)
{
    g_ClipboardImage.clear();
    if (!width || !height || !ImPlatform_Native_ReadClipboardImage(&g_ClipboardImage, &g_ClipboardImageSize[0], &g_ClipboardImageSize[1]))
        return false;
    *width = g_ClipboardImageSize[0];
    *height = g_ClipboardImageSize[1];
    return true;
}

IMPLATFORM_API bool ImPlatform_GetClipboardImage(unsigned char* rgba, int width, int height)
{
    if (!rgba || g_ClipboardImage.Size == 0 || width != g_ClipboardImageSize[0] || height != g_ClipboardImageSize[1])
        return false;
    memcpy(rgba, g_ClipboardImage.Data, (size_t)g_ClipboardImage.Size);
    g_ClipboardImage.clear();
    return true;
}

#else

IMPLATFORM_API bool ImPlatform_SetClipboardImage(const unsigned char*, int, int) { return false; }
IMPLATFORM_API bool ImPlatform_GetClipboardImageSize(int*, int*) { return false; }
IMPLATFORM_API bool ImPlatform_GetClipboardImage(unsigned char*, int, int) { return false; }

#endif
