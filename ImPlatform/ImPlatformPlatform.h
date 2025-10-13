#pragma once

#ifndef __IM_PLATFORM_PLATFORM_H__
#define __IM_PLATFORM_PLATFORM_H__

#include <imgui.h>

// Forward declarations
struct PlatformDataImpl;

namespace ImPlatform
{
	// Platform (windowing) namespace - Pure C style
	// Each platform .cpp file (win32, glfw, etc.) implements these functions
	// The linker resolves which implementation to use at link-time
	namespace Platform
	{
		// Window creation & management
		bool CreateWindow( char const* pWindowsName, ImVec2 const vPos, ImU32 const uWidth, ImU32 const uHeight );
		bool ShowWindow();
		void DestroyWindow();

		// Platform initialization & shutdown
		bool InitPlatform();
		void ShutdownPlatform();

		// Event handling
		bool PlatformContinue();
		bool PlatformEvents();  // Returns true if quit requested
		void PlatformNewFrame();

		// Window state queries
		bool IsMaximized();

		// Custom title bar support
		bool CustomTitleBarSupported();
		void MinimizeApp();
		void MaximizeApp();
		void CloseApp();
	}
}

#endif // __IM_PLATFORM_PLATFORM_H__
