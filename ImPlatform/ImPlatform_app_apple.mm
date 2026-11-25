// dear imgui: Platform Abstraction - macOS (Cocoa) Application Backend
// A Cocoa window made without GLFW or SDL: its events are polled once per frame, and Dear ImGui's imgui_impl_osx
// reads them (mouse, keyboard, text input, gamepads, the platform windows of multi-viewports).
//
// Objective-C++ with ARC: this file, and the file that includes it through ImPlatform.h with
// IMPLATFORM_IMPLEMENTATION, are compiled as such. Link the Cocoa and GameController frameworks (imgui_impl_osx).
// The window has the system's titlebar: no custom titlebar, file drop or window state calls on this platform.

#include "ImPlatform_Internal.h"

#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_APPLE)

#include <TargetConditionals.h>
#if !TARGET_OS_OSX
    #error "IM_PLATFORM_APPLE is the macOS platform (Cocoa)"
#endif

#include "../imgui/backends/imgui_impl_osx.h"
#import <Cocoa/Cocoa.h>

static bool g_Done = false;     // The close button or Quit: ImPlatform_PlatformContinue ends the application's loop

// The window's delegate, and the target of the Quit menu item
@interface ImPlatformWindowDelegate : NSObject <NSWindowDelegate>
- (void)quit:(id)sender;
@end

@implementation ImPlatformWindowDelegate
// The window stays until ImPlatform_DestroyWindow: the application shuts its graphics down first
- (BOOL)windowShouldClose:(NSWindow*)sender
{
    (void)sender;
    g_Done = true;
    return NO;
}

- (void)quit:(id)sender
{
    (void)sender;
    g_Done = true;
}
@end

static NSWindow* g_Window = nil;
static ImPlatformWindowDelegate* g_Delegate = nil;

// Internal API - the NSWindow* and its content view, the NSView* the graphics backend puts its layer on
void* ImPlatform_App_GetNSWindow(void)
{
    return (__bridge void*)g_Window;
}

void* ImPlatform_App_GetNSView(void)
{
    return (__bridge void*)g_Window.contentView;
}

// ImPlatform API - CreateWindow. vPos is the top left corner of the content area, from the top left of the main screen.
IMPLATFORM_API bool ImPlatform_CreateWindow(char const* pWindowsName, ImVec2 const vPos, unsigned int uWidth, unsigned int uHeight)
{
    @autoreleasepool {
        g_Done = false;
        g_Delegate = [ImPlatformWindowDelegate new];

        // An application with a Dock icon and a menu bar, though it has no bundle and no nib (a program started from
        // a terminal is a background process otherwise). The menu gives it Quit and its shortcut.
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        NSMenu* menu_bar = [NSMenu new];
        NSMenuItem* app_item = [menu_bar addItemWithTitle:@"" action:nil keyEquivalent:@""];
        NSMenu* app_menu = [NSMenu new];
        NSMenuItem* quit_item = [app_menu addItemWithTitle:@"Quit" action:@selector(quit:) keyEquivalent:@"q"];
        quit_item.target = g_Delegate;
        app_item.submenu = app_menu;
        NSApp.mainMenu = menu_bar;
        [NSApp finishLaunching];

        // Cocoa's origin is the bottom left corner of the first screen
        const CGFloat screen_height = NSScreen.screens.count > 0 ? NSScreen.screens[0].frame.size.height : (CGFloat)uHeight;
        const NSRect content = NSMakeRect(vPos.x, screen_height - vPos.y - (CGFloat)uHeight, (CGFloat)uWidth, (CGFloat)uHeight);
        g_Window = [[NSWindow alloc] initWithContentRect:content
                                               styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable
                                                 backing:NSBackingStoreBuffered
                                                   defer:NO];
        if (!g_Window)
            return false;
        g_Window.releasedWhenClosed = NO;           // ImPlatform_DestroyWindow closes it and drops the reference
        g_Window.acceptsMouseMovedEvents = YES;     // imgui_impl_osx reads the mouse position from them
        g_Window.delegate = g_Delegate;
        NSString* title = pWindowsName ? [NSString stringWithUTF8String:pWindowsName] : nil;
        if (title)
            g_Window.title = title;
        return true;
    }
}

// ImPlatform API - ShowWindow
IMPLATFORM_API bool ImPlatform_ShowWindow(void)
{
    @autoreleasepool {
        if (!g_Window)
            return false;
        [g_Window makeKeyAndOrderFront:nil];
        // Started from a terminal, the application is not the active one and its window would open behind. Same call as
        // Dear ImGui's example_apple_metal and GLFW: an activation without it waits for the active application to give way.
        [NSApp activateIgnoringOtherApps:YES];

        // An activation brings the key window in front of the application's other windows, and it ends when its event is
        // handled: left to the first frame, that would be after the platform windows of that frame are shown, which
        // then stay behind this one. The events are handled here until it has ended, half a second at most.
        NSDate* until = [NSDate dateWithTimeIntervalSinceNow:0.5];
        while (!NSApp.isActive)
        {
            NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:until inMode:NSDefaultRunLoopMode dequeue:YES];
            if (!event)
                break;
            [NSApp sendEvent:event];
        }
        return true;
    }
}

// ImPlatform API - InitPlatform
IMPLATFORM_API bool ImPlatform_InitPlatform(void)
{
    @autoreleasepool {
        return g_Window != nil && ImGui_ImplOSX_Init(g_Window.contentView);
    }
}

// ImPlatform API - PlatformContinue
IMPLATFORM_API bool ImPlatform_PlatformContinue(void)
{
    return !g_Done;
}

// ImPlatform API - PlatformEvents. The events waiting are handed to the application object, which runs the monitor of
// imgui_impl_osx and the window's own handling (close, resize, miniaturize).
IMPLATFORM_API bool ImPlatform_PlatformEvents(void)
{
    @autoreleasepool {
        for (;;)
        {
            NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES];
            if (!event)
                break;
            [NSApp sendEvent:event];
        }
        ImPlatform_Native_Pump();

        if (g_Window.miniaturized)
        {
            [NSThread sleepForTimeInterval:0.01];
            return false;   // Skip frame
        }
        return true;
    }
}

// ImPlatform API - PlatformNewFrame
IMPLATFORM_API void ImPlatform_PlatformNewFrame(void)
{
    @autoreleasepool {
        ImGui_ImplOSX_NewFrame(g_Window.contentView);
    }
}

// ImPlatform API - ShutdownPostGfxAPI
IMPLATFORM_API void ImPlatform_ShutdownPostGfxAPI(void)
{
    @autoreleasepool {
        ImGui_ImplOSX_Shutdown();
    }
}

// ImPlatform API - DestroyWindow
IMPLATFORM_API void ImPlatform_DestroyWindow(void)
{
    @autoreleasepool {
        ImPlatform_Native_Shutdown();
        g_Window.delegate = nil;
        [g_Window close];
        g_Window = nil;
        g_Delegate = nil;
    }
}

// Internal API - Get DPI scale: 1, as Dear ImGui's GLFW and SDL backends report on macOS. The window is sized in
// points, and io.DisplayFramebufferScale (the backing scale factor) does the scaling.
float ImPlatform_App_GetDpiScale_Apple(void)
{
    return 1.0f;
}

#endif // IM_CURRENT_PLATFORM == IM_PLATFORM_APPLE
