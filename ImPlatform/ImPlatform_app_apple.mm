// dear imgui: Platform Abstraction - Apple (macOS/iOS) Backend
// This handles Cocoa/UIKit window creation, event handling, and the main loop
//
// NOTE: Apple platform support requires Objective-C++
// This file must be compiled as .mm (Objective-C++)

#include "ImPlatform_Internal.h"

#ifdef IM_PLATFORM_APPLE

#include "../imgui.h"

#if TARGET_OS_OSX
    #include "../backends/imgui_impl_osx.h"
    #import <Cocoa/Cocoa.h>
#elif TARGET_OS_IOS
    #include "../backends/imgui_impl_ios.h"
    #import <UIKit/UIKit.h>
#endif

// Global state
static struct ImPlatform_AppData_Apple {
#if TARGET_OS_OSX
    NSWindow* pWindow;
    NSView* pView;
#elif TARGET_OS_IOS
    UIWindow* pWindow;
    UIView* pView;
    UIViewController* pViewController;
#endif
    float fDpiScale;
} g_AppData = { 0 };

#if TARGET_OS_OSX
// Internal API - Get NSWindow
void* ImPlatform_App_GetNSWindow(void)
{
    return (__bridge void*)g_AppData.pWindow;
}

// Internal API - Get NSView
void* ImPlatform_App_GetNSView(void)
{
    return (__bridge void*)g_AppData.pView;
}
#elif TARGET_OS_IOS
// Internal API - Get UIWindow
void* ImPlatform_App_GetUIWindow(void)
{
    return (__bridge void*)g_AppData.pWindow;
}

// Internal API - Get UIView
void* ImPlatform_App_GetUIView(void)
{
    return (__bridge void*)g_AppData.pView;
}
#endif

// ImPlatform API - CreateWindow
IMPLATFORM_API bool ImPlatform_CreateWindow(char const* pWindowsName, ImVec2 const vPos, unsigned int uWidth, unsigned int uHeight)
{
    @autoreleasepool {
#if TARGET_OS_OSX
        // Create macOS window
        NSRect frame = NSMakeRect(vPos.x, vPos.y, uWidth, uHeight);

        NSWindowStyleMask styleMask = NSWindowStyleMaskTitled |
                                      NSWindowStyleMaskClosable |
                                      NSWindowStyleMaskResizable |
                                      NSWindowStyleMaskMiniaturizable;

        g_AppData.pWindow = [[NSWindow alloc]
            initWithContentRect:frame
            styleMask:styleMask
            backing:NSBackingStoreBuffered
            defer:NO];

        if (!g_AppData.pWindow)
            return false;

        [g_AppData.pWindow setTitle:[NSString stringWithUTF8String:pWindowsName]];
        [g_AppData.pWindow makeKeyAndOrderFront:nil];

        // Create content view
        g_AppData.pView = g_AppData.pWindow.contentView;

        // Query backing scale factor for DPI
        g_AppData.fDpiScale = (float)[[g_AppData.pWindow screen] backingScaleFactor];

        return true;

#elif TARGET_OS_IOS
        // Create iOS window
        CGRect bounds = [[UIScreen mainScreen] bounds];
        g_AppData.pWindow = [[UIWindow alloc] initWithFrame:bounds];

        if (!g_AppData.pWindow)
            return false;

        // Create view controller
        g_AppData.pViewController = [[UIViewController alloc] init];
        g_AppData.pWindow.rootViewController = g_AppData.pViewController;

        // Create view
        g_AppData.pView = [[UIView alloc] initWithFrame:bounds];
        g_AppData.pViewController.view = g_AppData.pView;

        [g_AppData.pWindow makeKeyAndVisible];

        // Query screen scale for DPI (2.0 for Retina, 3.0 for Plus models)
        g_AppData.fDpiScale = (float)[[UIScreen mainScreen] scale];

        return true;
#else
        return false;
#endif
    }
}

// ImPlatform API - InitGfxAPI
// Note: This is called from the graphics backend for Apple

// ImPlatform API - ShowWindow
IMPLATFORM_API bool ImPlatform_ShowWindow(void)
{
    @autoreleasepool {
#if TARGET_OS_OSX
        [g_AppData.pWindow makeKeyAndOrderFront:nil];
#elif TARGET_OS_IOS
        [g_AppData.pWindow makeKeyAndVisible];
#endif
        return true;
    }
}

// ImPlatform API - InitPlatform
IMPLATFORM_API bool ImPlatform_InitPlatform(void)
{
    @autoreleasepool {
#if TARGET_OS_OSX
        if (!ImGui_ImplOSX_Init(g_AppData.pView))
            return false;
#elif TARGET_OS_IOS
        if (!ImGui_ImplIOS_Init(g_AppData.pView))
            return false;
#endif
        return true;
    }
}

// ImPlatform API - PlatformContinue
IMPLATFORM_API bool ImPlatform_PlatformContinue(void)
{
    // Apple apps typically use run loop
    return true;
}

// ImPlatform API - PlatformEvents
IMPLATFORM_API bool ImPlatform_PlatformEvents(void)
{
    @autoreleasepool {
#if TARGET_OS_OSX
        // Process Cocoa events
        NSEvent* event;
        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                           untilDate:[NSDate distantPast]
                                              inMode:NSDefaultRunLoopMode
                                             dequeue:YES]))
        {
            [NSApp sendEvent:event];
        }
#elif TARGET_OS_IOS
        // iOS uses UIApplication run loop - events are handled automatically
        // Just process pending events
        [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                                 beforeDate:[NSDate distantPast]];
#endif

        return true;
    }
}

// ImPlatform API - PlatformNewFrame
IMPLATFORM_API void ImPlatform_PlatformNewFrame(void)
{
    @autoreleasepool {
#if TARGET_OS_OSX
        ImGui_ImplOSX_NewFrame(g_AppData.pView);
#elif TARGET_OS_IOS
        ImGui_ImplIOS_NewFrame();
#endif
    }
}

// ImPlatform API - ShutdownPlatform
IMPLATFORM_API void ImPlatform_ShutdownPlatform(void)
{
    @autoreleasepool {
#if TARGET_OS_OSX
        ImGui_ImplOSX_Shutdown();

        if (g_AppData.pWindow)
        {
            [g_AppData.pWindow close];
            g_AppData.pWindow = nil;
        }
#elif TARGET_OS_IOS
        ImGui_ImplIOS_Shutdown();

        if (g_AppData.pWindow)
        {
            g_AppData.pWindow = nil;
        }
#endif
    }
}

// Internal API - Get DPI scale
float ImPlatform_App_GetDpiScale_Apple(void)
{
    return g_AppData.fDpiScale > 0.0f ? g_AppData.fDpiScale : 1.0f;
}

#endif // IM_PLATFORM_APPLE
