// ImPlatform Win32 Platform Implementation - Pure C Style
// This file implements ImPlatform::Platform:: namespace functions for Win32
// The linker resolves these at link-time based on which .cpp is included

#include "ImPlatformPlatform.h"
#include "ImPlatform.h"
#include <imgui.h>
#include <imgui_internal.h>

// Win32 includes
#include <backends/imgui_impl_win32.h>
// Note: imgui_impl_win32.cpp is included by main ImPlatform.cpp, don't include it again
#include <windowsx.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <tchar.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

// Win32 window procedure (internal to this file, outside namespace to avoid conflicts)
static LRESULT WINAPI ImPlatformWin32_WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	using namespace ImPlatform;
	if ( ImGui_ImplWin32_WndProcHandler( hWnd, msg, wParam, lParam ) )
		return true;

	switch ( msg )
	{
	case WM_SIZE:
		if ( wParam != SIZE_MINIMIZED )
		{
#if (IM_CURRENT_GFX != IM_GFX_DIRECTX12) && (IM_CURRENT_GFX != IM_GFX_OPENGL3)
			PlatformData.uResizeWidth = ( UINT )LOWORD( lParam );
			PlatformData.uResizeHeight = ( UINT )HIWORD( lParam );
#endif
		}
		return 0;
	case WM_SYSCOMMAND:
		if ( ( wParam & 0xfff0 ) == SC_KEYMENU )
			return 0;
		break;
	case WM_DESTROY:
		::PostQuitMessage( 0 );
		return 0;
	case WM_DPICHANGED:
		if ( ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports )
		{
			const RECT* suggested_rect = ( RECT* )lParam;
			::SetWindowPos( hWnd, nullptr, suggested_rect->left, suggested_rect->top,
				suggested_rect->right - suggested_rect->left,
				suggested_rect->bottom - suggested_rect->top,
				SWP_NOZORDER | SWP_NOACTIVATE );
		}
		break;
	}
	return ::DefWindowProcW( hWnd, msg, wParam, lParam );
}

namespace ImPlatform
{
	extern PlatformDataImpl PlatformData;

	// Implementation of Platform namespace for Win32
	namespace Platform
	{
		bool CreateWindow( char const* pWindowsName, ImVec2 const vPos, ImU32 const uWidth, ImU32 const uHeight )
		{
			// Create window class
			WNDCLASSEXW wc = {
				sizeof( wc ),
				CS_CLASSDC,
				ImPlatformWin32_WndProc,
				0L,
				0L,
				GetModuleHandle( nullptr ),
				nullptr,
				nullptr,
				nullptr,
				nullptr,
				L"ImPlatform",
				nullptr
			};
			::RegisterClassExW( &wc );

			// Create window
			DWORD dwStyle = WS_OVERLAPPEDWINDOW;
			DWORD dwExStyle = 0;

			if ( PlatformData.bCustomTitleBar )
			{
				dwStyle = WS_POPUP | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
			}

			RECT rect = { (LONG)vPos.x, (LONG)vPos.y, (LONG)(vPos.x + uWidth), (LONG)(vPos.y + uHeight) };
			::AdjustWindowRectEx( &rect, dwStyle, FALSE, dwExStyle );

			PlatformData.pHandle = ::CreateWindowExW(
				dwExStyle,
				wc.lpszClassName,
				L"Dear ImGui Platform Window",
				dwStyle,
				rect.left, rect.top,
				rect.right - rect.left, rect.bottom - rect.top,
				nullptr, nullptr, wc.hInstance, nullptr );

			if ( !PlatformData.pHandle )
				return false;

#ifdef UNICODE
			PlatformData.oWinStruct = wc;
#else
			// Would need ANSI version
#endif

			return true;
		}

		bool ShowWindow()
		{
			::ShowWindow( PlatformData.pHandle, SW_SHOWDEFAULT );
			::UpdateWindow( PlatformData.pHandle );
			return true;
		}

		void DestroyWindow()
		{
			if ( PlatformData.pHandle )
			{
				::DestroyWindow( PlatformData.pHandle );
				PlatformData.pHandle = nullptr;
			}
			::UnregisterClassW( PlatformData.oWinStruct.lpszClassName, PlatformData.oWinStruct.hInstance );
		}

		bool InitPlatform()
		{
			return ImGui_ImplWin32_Init( PlatformData.pHandle );
		}

		void ShutdownPlatform()
		{
			ImGui_ImplWin32_Shutdown();
		}

		bool PlatformContinue()
		{
			return PlatformData.pHandle != nullptr;
		}

		bool PlatformEvents()
		{
			MSG msg;
			while ( ::PeekMessageW( &msg, nullptr, 0U, 0U, PM_REMOVE ) )
			{
				::TranslateMessage( &msg );
				::DispatchMessageW( &msg );
				if ( msg.message == WM_QUIT )
					return true; // Quit requested
			}
			return false; // Continue
		}

		void PlatformNewFrame()
		{
			ImGui_ImplWin32_NewFrame();
		}

		bool IsMaximized()
		{
			WINDOWPLACEMENT placement = {};
			placement.length = sizeof( WINDOWPLACEMENT );
			if ( GetWindowPlacement( PlatformData.pHandle, &placement ) )
			{
				return placement.showCmd == SW_SHOWMAXIMIZED;
			}
			return false;
		}

		bool CustomTitleBarSupported()
		{
			return true;
		}

		void MinimizeApp()
		{
			if ( PlatformData.pHandle )
				::ShowWindow( PlatformData.pHandle, SW_MINIMIZE );
		}

		void MaximizeApp()
		{
			if ( PlatformData.pHandle )
			{
				if ( IsMaximized() )
					::ShowWindow( PlatformData.pHandle, SW_RESTORE );
				else
					::ShowWindow( PlatformData.pHandle, SW_MAXIMIZE );
			}
		}

		void CloseApp()
		{
			if ( PlatformData.pHandle )
				::PostMessage( PlatformData.pHandle, WM_CLOSE, 0, 0 );
		}
	}

} // namespace ImPlatform
