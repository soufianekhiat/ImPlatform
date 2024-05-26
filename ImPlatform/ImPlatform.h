#pragma once

#include <imgui.h>

// - [ x ] WIN32_OPENGL3
// - [ x ] WIN32_DIRECTX9
// - [ x ] WIN32_DIRECTX10
// - [ x ] WIN32_DIRECTX11
// - [ x ] WIN32_DIRECTX12
// - [   ] GLFW_OPENGL2
// - [   ] GLFW_OPENGL3
// - [   ] GLFW_VULKAN
// - [   ] SDL2_DIRECTX11
// - [   ] SDL2_OPENGL2
// - [   ] SDL2_OPENGL3
// - [   ] SDL2_SDLRENDERER2
// - [   ] SDL2_VULKAN

// INTERNAL_MACRO
#define IM_GFX_OPENGL2		( 1u << 0u )
#define IM_GFX_OPENGL3		( 1u << 1u )
#define IM_GFX_DIRECTX9		( 1u << 2u )
#define IM_GFX_DIRECTX10	( 1u << 3u )
#define IM_GFX_DIRECTX11	( 1u << 4u )
#define IM_GFX_DIRECTX12	( 1u << 5u )
#define IM_GFX_VULKAN		( 1u << 6u )
#define IM_GFX_METAL		( 1u << 7u )
#define IM_GFX_WGPU			( 1u << 8u )

#define IM_GFX_MASK			0x0000FFFFu

#define IM_PLATFORM_WIN32	( ( 1u << 0u ) << 16u )
#define IM_PLATFORM_GLFW	( ( 1u << 1u ) << 16u )
#define IM_PLATFORM_APPLE	( ( 1u << 2u ) << 16u )

#define IM_PLATFORM_MASK	0xFFFF0000u

#ifndef IM_CURRENT_TARGET
#ifdef __DEAR_WIN__
#define IM_CURRENT_PLATFORM IM_PLATFORM_WIN32
#elif defined(__DEAR_LINUX__)
#define IM_CURRENT_PLATFORM IM_PLATFORM_GLFW
#elif defined(__DEAR_MAC__)
#define IM_CURRENT_PLATFORM IM_PLATFORM_APPLE
#else
#endif

#ifdef __DEAR_GFX_DX9__
#define IM_CURRENT_GFX IM_GFX_DIRECTX9
#elif defined(__DEAR_GFX_DX10__)
#define IM_CURRENT_GFX IM_GFX_DIRECTX10
#elif defined(__DEAR_GFX_DX11__)
#define IM_CURRENT_GFX IM_GFX_DIRECTX11
#elif defined(__DEAR_GFX_DX12__)
#define IM_CURRENT_GFX IM_GFX_DIRECTX12
#elif defined(__DEAR_GFX_OGL2__)
#define IM_CURRENT_GFX IM_GFX_OPENGL2
#elif defined(__DEAR_GFX_OGL3__)
#define IM_CURRENT_GFX IM_GFX_OPENGL3
#elif defined(__DEAR_GFX_VULKAN__)
#define IM_CURRENT_GFX IM_GFX_VULKAN
#elif defined(__DEAR_METAL__)
#define IM_CURRENT_GFX IM_GFX_METAL
#else
#endif

#define IM_CURRENT_TARGET	(IM_CURRENT_PLATFORM | IM_CURRENT_GFX)

#else

#define IM_CURRENT_PLATFORM	( IM_CURRENT_TARGET & IM_PLATFORM_MASK )
#define IM_CURRENT_GFX		( IM_CURRENT_TARGET & IM_GFX_MASK )

#endif

// Possible Permutation
#define IM_TARGET_WIN32_DX9		( IM_PLATFORM_WIN32 | IM_GFX_DIRECTX9 )
#define IM_TARGET_WIN32_DX10	( IM_PLATFORM_WIN32 | IM_GFX_DIRECTX10 )
#define IM_TARGET_WIN32_DX11	( IM_PLATFORM_WIN32 | IM_GFX_DIRECTX11 )
#define IM_TARGET_WIN32_DX12	( IM_PLATFORM_WIN32 | IM_GFX_DIRECTX12 )
#define IM_TARGET_WIN32_OGL3	( IM_PLATFORM_WIN32 | IM_GFX_OPENGL3 )

#define IM_TARGET_APPLE_METAL	( IM_PLATFORM_APPLE | IM_GFX_METAL )
#define IM_TARGET_APPLE_OPENGL2	( IM_PLATFORM_APPLE | IM_GFX_OPENGL2 )

#define IM_TARGET_GLFW_OPENGL2	( IM_PLATFORM_GLFW | IM_GFX_OPENGL2 )
#define IM_TARGET_GLFW_OPENGL3	( IM_PLATFORM_GLFW | IM_GFX_OPENGL3 )

#define IM_TARGET_GLFW_VULKAN	( IM_PLATFORM_GLFW | IM_GFX_VULKAN )

#define IM_TARGET_GLFW_METAL	( IM_PLATFORM_GLFW | IM_GFX_METAL )

#if IM_CURRENT_TARGET != IM_TARGET_WIN32_DX9 && IM_CURRENT_TARGET != IM_TARGET_WIN32_DX10 && IM_CURRENT_TARGET != IM_TARGET_WIN32_DX11 && IM_CURRENT_TARGET != IM_TARGET_WIN32_OGL3

//#warning ImPlatform unsupported permutation
#pragma message( "ImPlatform unsupported permutation" )

#endif

#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
#include <backends/imgui_impl_win32.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <tchar.h>
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0 // From Windows SDK 8.1+ headers
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
#include <backends/imgui_impl_glfw.h>
#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_APPLE)
#include <backends/imgui_impl_osx.h>
#else
#error IM_CURRENT_TARGET not specified correctly
#endif

#if (IM_CURRENT_GFX == IM_GFX_DIRECTX9)
#include <backends/imgui_impl_dx9.h>
#include <d3d9.h>
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)
#include <backends/imgui_impl_dx10.h>
#include <d3d10_1.h>
#include <d3d10.h>
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
#include <backends/imgui_impl_dx11.h>
#include <d3d11.h>
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
#include <backends/imgui_impl_dx12.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#elif (IM_CURRENT_GFX == IM_GFX_VULKAN)
#include <backends/imgui_impl_vulkan.h>
#elif (IM_CURRENT_GFX == IM_GFX_METAL)
#include <backends/imgui_impl_metal.h>
#elif (IM_CURRENT_GFX == IM_GFX_OPENGL2)
#include <backends/imgui_impl_opengl2.h>
#elif (IM_CURRENT_GFX == IM_GFX_OPENGL3)
#include <backends/imgui_impl_opengl3.h>
#elif (IM_CURRENT_GFX == IM_GFX_WGPU)
#include <backends/imgui_impl_wgpu.h>
#endif

struct PlatformDataImpl
{
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
	HWND		pHandle		= nullptr;
	WNDCLASSEX	oWinStruct;
	MSG			oMessage;

	// Dx12 Backend uses WM_SIZE to resize buffers
#if (IM_CURRENT_GFX != IM_GFX_DIRECTX12) && (IM_CURRENT_GFX != IM_GFX_OPENGL3) && (IM_CURRENT_GFX != IM_GFX_OPENGL2s)
	UINT		uResizeWidth	= 0;
	UINT		uResizeHeight	= 0;
#endif

#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
	GLFWwindow*	pWindow		=	nullptr;
#elif (IM_CURRENT_PLATFORM) == IM_PLATFORM_APPLE)
#else
#error IM_CURRENT_TARGET not specified correctly
#endif

#if (IM_CURRENT_GFX == IM_GFX_OPENGL2)
#elif (IM_CURRENT_GFX == IM_GFX_OPENGL3)

#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
	struct WGL_WindowData
	{
		HDC hDC;
	};
	HGLRC				pRC;
	WGL_WindowData		oMainWindow;
	int					iWidth;
	int					iHeight;
#endif

	char const*				pGLSLVersion			= "#version 130";
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9)
	LPDIRECT3D9				pD3D					= nullptr;
	LPDIRECT3DDEVICE9		pD3DDevice				= nullptr;
	D3DPRESENT_PARAMETERS	oD3Dpp					= {};
	bool					bDeviceLost				= false;
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)
	ID3D10Device*			pD3DDevice				= nullptr;
	IDXGISwapChain*			pSwapChain				= nullptr;
	ID3D10RenderTargetView* pMainRenderTargetView	= nullptr;
	bool					bSwapChainOccluded		= false;
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
	ID3D11Device*			pD3DDevice				= nullptr;
	ID3D11DeviceContext*	pD3DDeviceContext		= nullptr;
	IDXGISwapChain*			pSwapChain				= nullptr;
	ID3D11RenderTargetView*	pMainRenderTargetView	= nullptr;
	bool					bSwapChainOccluded		= false;
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
	struct FrameContext
	{
		ID3D12CommandAllocator*	CommandAllocator;
		UINT64					FenceValue;
	};

	static int const			NUM_FRAMES_IN_FLIGHT		= 3;
	FrameContext				pFrameContext[ NUM_FRAMES_IN_FLIGHT ]
															= {};
	UINT						uFrameIndex					= 0;

	static int const			NUM_BACK_BUFFERS			= 3;
	ID3D12Device*				pD3DDevice					= nullptr;
	ID3D12DescriptorHeap*		pD3DRTVDescHeap				= nullptr;
	ID3D12DescriptorHeap*		pD3DSRVDescHeap				= nullptr;
	ID3D12CommandQueue*			pD3DCommandQueue			= nullptr;
	ID3D12GraphicsCommandList*	pD3DCommandList				= nullptr;
	ID3D12Fence*				pFence						= nullptr;
	HANDLE						pFenceEvent					= nullptr;
	UINT64						uFenceLastSignaledValue		= 0;
	IDXGISwapChain3*			pSwapChain					= nullptr;
	HANDLE						pSwapChainWaitableObject	= nullptr;
	ID3D12Resource*				pMainRenderTargetResource[ NUM_BACK_BUFFERS ]
															= {};
	D3D12_CPU_DESCRIPTOR_HANDLE	pMainRenderTargetDescriptor[ NUM_BACK_BUFFERS ]
															= {};
	bool							bSwapChainOccluded		= false;
#endif
};

extern PlatformDataImpl PlatformData;

namespace ImPlatform
{
	// Functions as FrontEnd;
	bool ImInit( char const* pWindowsName, ImU32 const uWidth, ImU32 const uHeight );
	void ImBegin();
	void ImEnd();
	bool ImBeginFrame();
	void ImEndFrame( ImVec4 const vClearColor );
	void ImSwapGfx();

	void ImGfxViewportPost();

	bool ImPlatformContinue();
	bool ImPlatformEvents();

	// Internal-Use Only
	void ImNewFrame();

	bool ImInitWindow( char const* pWindowsName, ImU32 const uWidth, ImU32 const uHeight );
	bool ImInitGfxAPI();
	bool ImShowWindow();
	bool ImWindowResize( ImU32 const uWidth, ImU32 const uHeight );

	bool ImGfxAPINewFrame();
	bool ImPlatformNewFrame();

	bool ImGfxAPIClear( ImVec4 const vClearColor );
	// Need to pass the clear color to support dx12
	bool ImGfxAPIRender( ImVec4 const vClearColor );
	bool ImGfxAPISwapBuffer();

	void ImShutdownGfxAPI();
	void ImShutdownPostGfxAPI();
	void ImShutdownWindow();
}

#ifdef IM_PLATFORM_IMPLEMENTATION
#include <ImPlatform.cpp>
#endif