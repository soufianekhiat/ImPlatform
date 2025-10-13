// ImPlatform GLFW Platform Implementation - Pure C Style
// This file implements ImPlatform::Platform:: namespace functions for GLFW
// The linker resolves these at link-time based on which .cpp is included

#include "ImPlatformPlatform.h"
#include "ImPlatform.h"
#include <imgui.h>
#include <imgui_internal.h>

// GLFW includes
#include <backends/imgui_impl_glfw.h>

// Include GLFW headers
#ifdef IM_THE_CHERNO_GLFW3
// Using TheCherno's GLFW fork with custom title bar support
#include <GLFW/glfw3.h>
#else
// Standard GLFW3
#include <GLFW/glfw3.h>
#endif

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

// Include GLFW implementation
#include <backends/imgui_impl_glfw.cpp>

namespace ImPlatform
{
	extern PlatformDataImpl PlatformData;

	// GLFW error callback (internal to this file)
	static void glfw_error_callback( int error, const char* description )
	{
		fprintf( stderr, "GLFW Error %d: %s\n", error, description );
	}

	// Implementation of Platform namespace for GLFW
	namespace Platform
	{
		bool CreateWindow( char const* pWindowsName, ImVec2 const vPos, ImU32 const uWidth, ImU32 const uHeight )
		{
			glfwSetErrorCallback( glfw_error_callback );

			if ( !glfwInit() )
				return false;

			// Setup window hints based on graphics API
#if (IM_CURRENT_GFX == IM_GFX_OPENGL3)
			// OpenGL hints
			const char* glsl_version = "#version 130";
			glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
			glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 0 );
			//glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );
#elif (IM_CURRENT_GFX == IM_GFX_VULKAN)
			// Vulkan hints
			glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
#else
			glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
#endif

			// Custom title bar support (if using TheCherno's fork)
			if ( PlatformData.bCustomTitleBar )
			{
#ifdef IM_THE_CHERNO_GLFW3
				glfwWindowHint( GLFW_TITLEBAR, false );
#endif
			}

			// Create window
			PlatformData.pWindow = glfwCreateWindow(
				uWidth, uHeight, pWindowsName, nullptr, nullptr );

			if ( !PlatformData.pWindow )
			{
				glfwTerminate();
				return false;
			}

			// Set window position
			glfwSetWindowPos( PlatformData.pWindow, (int)vPos.x, (int)vPos.y );

#if (IM_CURRENT_GFX == IM_GFX_OPENGL3)
			glfwMakeContextCurrent( PlatformData.pWindow );
			glfwSwapInterval( 1 ); // Enable vsync
#endif

			return true;
		}

		bool ShowWindow()
		{
			// GLFW windows are visible by default after creation
			glfwShowWindow( PlatformData.pWindow );
			return true;
		}

		void DestroyWindow()
		{
			if ( PlatformData.pWindow )
			{
				glfwDestroyWindow( PlatformData.pWindow );
				PlatformData.pWindow = nullptr;
			}
			glfwTerminate();
		}

		bool InitPlatform()
		{
			return ImGui_ImplGlfw_InitForOther( PlatformData.pWindow, true );
		}

		void ShutdownPlatform()
		{
			ImGui_ImplGlfw_Shutdown();
		}

		bool PlatformContinue()
		{
			return PlatformData.pWindow != nullptr && !glfwWindowShouldClose( PlatformData.pWindow );
		}

		bool PlatformEvents()
		{
			glfwPollEvents();
			return glfwWindowShouldClose( PlatformData.pWindow );
		}

		void PlatformNewFrame()
		{
			ImGui_ImplGlfw_NewFrame();
		}

		bool IsMaximized()
		{
			if ( !PlatformData.pWindow )
				return false;
			return glfwGetWindowAttrib( PlatformData.pWindow, GLFW_MAXIMIZED ) != 0;
		}

		bool CustomTitleBarSupported()
		{
#ifdef IM_THE_CHERNO_GLFW3
			return true;
#else
			return false;
#endif
		}

		void MinimizeApp()
		{
			if ( PlatformData.pWindow )
				glfwIconifyWindow( PlatformData.pWindow );
		}

		void MaximizeApp()
		{
			if ( !PlatformData.pWindow )
				return;

			if ( IsMaximized() )
				glfwRestoreWindow( PlatformData.pWindow );
			else
				glfwMaximizeWindow( PlatformData.pWindow );
		}

		void CloseApp()
		{
			if ( PlatformData.pWindow )
				glfwSetWindowShouldClose( PlatformData.pWindow, GLFW_TRUE );
		}
	}

} // namespace ImPlatform
