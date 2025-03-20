#pragma once

#ifndef __IM_PLATFORM_H__
#define __IM_PLATFORM_H__

// For Dx12: #define ImTextureID ImU64

#include <imgui.h>

// To override the link of glfw3 do not use, other it ImPlatform will use the lib included on imgui
//#define IM_GLFW3_AUTO_LINK
// Use https://github.com/TheCherno/glfw/tree/dev to have CustomTitleBar Available
//#define IM_THE_CHERNO_GLFW3

// - [ X ] WIN32_OPENGL3 // TBD
// - [ O ] WIN32_DIRECTX9
// - [ O ] WIN32_DIRECTX10
// - [ O ] WIN32_DIRECTX11
// - [ O ] WIN32_DIRECTX12 // Issue with images
// - [ X ] GLFW_OPENGL2 // Produce clear_color frame
// - [ O ] GLFW_OPENGL3 // Do not work well with high DPI
// - [   ] GLFW_VULKAN
// - [   ] GLFW_EMSCRIPTEM_OPENGL3
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
#define IM_GFX_EMSCRIPTEM	( 1u << 9u )

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
#elif defined(__DEAR_GLFW__)
#define IM_CURRENT_PLATFORM IM_PLATFORM_GLFW
#elif defined(__DEAR_MAC__)
#define IM_CURRENT_PLATFORM IM_PLATFORM_APPLE
#else
#error __DEAR_{X}__ not specified correctly
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
#error __DEAR_GFX_{X}__ not specified correctly
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
#define IM_TARGET_WIN32_OPENGL3	( IM_PLATFORM_WIN32 | IM_GFX_OPENGL3 )

#define IM_TARGET_APPLE_METAL	( IM_PLATFORM_APPLE | IM_GFX_METAL )
#define IM_TARGET_APPLE_OPENGL2	( IM_PLATFORM_APPLE | IM_GFX_OPENGL2 )

#define IM_TARGET_GLFW_OPENGL2	( IM_PLATFORM_GLFW | IM_GFX_OPENGL2 )
#define IM_TARGET_GLFW_OPENGL3	( IM_PLATFORM_GLFW | IM_GFX_OPENGL3 )

#define IM_TARGET_GLFW_VULKAN	( IM_PLATFORM_GLFW | IM_GFX_VULKAN )

#define IM_TARGET_GLFW_METAL	( IM_PLATFORM_GLFW | IM_GFX_METAL )

#if IM_CURRENT_TARGET != IM_TARGET_WIN32_DX9		&&	\
	IM_CURRENT_TARGET != IM_TARGET_WIN32_DX10		&&	\
	IM_CURRENT_TARGET != IM_TARGET_WIN32_DX11		&&	\
	IM_CURRENT_TARGET != IM_TARGET_WIN32_DX12		&&	\
	IM_CURRENT_TARGET != IM_TARGET_WIN32_OPENGL3	&&	\
	IM_CURRENT_TARGET != IM_TARGET_GLFW_OPENGL3			

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
#define IM_GFX_HLSL
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)
#include <backends/imgui_impl_dx10.h>
#include <d3d10_1.h>
#include <d3d10.h>
#define IM_SUPPORT_CUSTOM_SHADER
#define IM_GFX_HLSL
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
#include <backends/imgui_impl_dx11.h>
#include <d3d11.h>
#define IM_SUPPORT_CUSTOM_SHADER
#define IM_GFX_HLSL
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
#include <backends/imgui_impl_dx12.h>
#include <d3d12.h>
#include <dxgi1_4.h>
//#define IM_SUPPORT_CUSTOM_SHADER
#define IM_GFX_HLSL
#elif (IM_CURRENT_GFX == IM_GFX_VULKAN)
#include <backends/imgui_impl_vulkan.h>
#define IM_GFX_GLSL
#elif (IM_CURRENT_GFX == IM_GFX_METAL)
#include <backends/imgui_impl_metal.h>
#define IM_GFX_MSL
#elif (IM_CURRENT_GFX == IM_GFX_OPENGL2)
#include <backends/imgui_impl_opengl2.h>
#define IM_GFX_GLSL
#elif (IM_CURRENT_GFX == IM_GFX_OPENGL3)
#include <backends/imgui_impl_opengl3.h>
#define IM_SUPPORT_CUSTOM_SHADER
#define IM_GFX_GLSL
#elif (IM_CURRENT_GFX == IM_GFX_WGPU)
#include <backends/imgui_impl_wgpu.h>
#define IM_GFX_WGPU
#endif

#include <stdio.h>

enum ImPlatformFeatures_
{
	ImPlatformFeatures_None,
	ImPlatformFeatures_CustomShader,

	ImPlatformFeatures_COUNT
};
typedef int ImPlatformFeatures;

struct PlatformDataImpl
{
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
	HWND		pHandle		= nullptr;
#ifdef UNICODE
	WNDCLASSEXW	oWinStruct;
#else
	WNDCLASSEXA	oWinStruct;
#endif
	MSG			oMessage;

	// Dx12 Backend uses WM_SIZE to resize buffers
#if (IM_CURRENT_GFX != IM_GFX_DIRECTX12) && (IM_CURRENT_GFX != IM_GFX_OPENGL3) && (IM_CURRENT_GFX != IM_GFX_OPENGL2)
	UINT		uResizeWidth	= 0;
	UINT		uResizeHeight	= 0;
#endif

#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
	GLFWwindow*	pWindow			= nullptr;
	GLFWwindow* pBackupContext	= nullptr;
#elif (IM_CURRENT_PLATFORM) == IM_PLATFORM_APPLE)
#else
#error IM_CURRENT_TARGET not specified correctly
#endif

#if (IM_CURRENT_GFX == IM_GFX_OPENGL2)
	char const* pGLSLVersion						= "#version 100";
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

	ImVec2	vEndCustomToolBar		= ImVec2( 0.0f, 0.0f );
	float	fCustomTitleBarHeight	= 32.0f;
	ImPlatformFeatures	features	= ImPlatformFeatures_None;
	bool	bCustomTitleBar			= false;
	bool	bTitleBarHovered		= false;
};

enum ImPixelType
{
	ImPixelType_UInt8,
	ImPixelType_UInt16,
	ImPixelType_UInt32,
	ImPixelType_UInt64,
	ImPixelType_Int8,
	ImPixelType_Int16,
	ImPixelType_Int32,
	ImPixelType_Int64,
	ImPixelType_Float16,
	ImPixelType_Float32,
	ImPixelType_Float64
};

enum ImPixelChannel
{
	ImPixelChannel_R = 1,
	ImPixelChannel_RG,
	ImPixelChannel_RGB,
	ImPixelChannel_RGBA
};

enum ImTextureFiltering
{
	ImTextureFiltering_Nearest,
	ImTextureFiltering_Linear
};

enum ImTextureBoundary
{
	ImTextureBoundary_Clamp,
	ImTextureBoundary_Repeat,
	ImTextureBoundary_Mirror
};

struct ImImageDesc
{
	ImPixelChannel		eChannel;
	ImPixelType			eType;
	ImTextureFiltering	eFiltering;
	ImTextureBoundary	eBoundaryU;
	ImTextureBoundary	eBoundaryV;
};

typedef void* ImShaderID;
typedef void* ImConstantID;
struct ImDrawShader
{
	ImShaderID vs;
	ImShaderID ps;
	ImConstantID vs_cst;
	ImConstantID ps_cst;
	void* cpu_vs_data;
	void* cpu_ps_data;
	int sizeof_in_bytes_vs_constants;
	int sizeof_in_bytes_ps_constants;
	bool is_cpu_vs_data_dirty;
	bool is_cpu_ps_data_dirty;
};

// Store a vertex buffer for a given and a *fixed* struct of B
typedef void* ImVertexBufferID;
typedef void* ImIndexBufferID;
struct ImVertexBuffer
{
	ImVertexBufferID gpu_buffer;
	int vertices_count;
	int sizeof_each_struct_in_bytes;
};
struct ImIndexBuffer
{
	ImIndexBufferID gpu_buffer;
	int indices_count;
	ImU8 sizeof_each_index;
};

namespace ImPlatform
{
	extern PlatformDataImpl PlatformData;

	// Generic
	ImTextureID	CreateTexture2D	( char* pData, ImU32 const uWidth, ImU32 const uHeight, ImImageDesc const& oImgDesc );
	void		ReleaseTexture2D( ImTextureID id );

	// [Deprecated]
	// Default Vertex Buffer used by Dear ImGui { float2 pos; float4 col; float2 uv; } => { float4 pos; float4 cocl; float2 uv; }
	void	CreateDefaultPixelShaderSource
								( char** out_vs_source,
								  char** out_ps_source,
								  char const* ps_pre_functions,
								  char const* ps_params,
								  char const* ps_source,
								  bool multiply_with_texture = true );

	// [Deprecated]
	void	CreateShaderSource	( char** out_vs_source,
								  char** out_ps_source,
								  char const* vs_pre_functions,
								  char const* vs_params,
								  char const* vs_source,
								  char const* ps_pre_functions,
								  char const* ps_params,
								  char const* ps_source,
								  char const* vb_desc,
								  char const* vs_to_ps_desc,
								  bool multiply_with_texture = true );

	ImDrawShader	CreateShader( char const* vs_source,
								  char const* ps_source,
								  int sizeof_in_bytes_vs_constants,
								  void* init_vs_data_constant,
								  int sizeof_in_bytes_ps_constants,
								  void* init_ps_data_constant );

	void			ReleaseShader( ImDrawShader& shader );

	void	CreateVertexBuffer( ImVertexBuffer*& buffer, int sizeof_vertex_buffer, int vertices_count );
	void	CreateVertexBufferAndGrow( ImVertexBuffer** buffer, int sizeof_vertex_buffer, int vertices_count, int growth_size );
	void	UpdateVertexBuffer( ImVertexBuffer** buffer, int sizeof_vertex_buffer, int vertices_count, void* cpu_data );
	void	ReleaseVertexBuffer( ImVertexBuffer** buffer );
	void	CreateIndexBuffer( ImIndexBuffer*& buffer, int indices_count );
	void	CreateIndexBufferAndGrow( ImIndexBuffer** buffer, int indices_count, int growth_size );
	void	UpdateIndexBuffer( ImIndexBuffer** buffer, int indices_count, void* cpu_data );
	void	ReleaseIndexBuffer( ImIndexBuffer** buffer );

	void		BeginCustomShader( ImDrawList* draw, ImDrawShader& shader );
	void		UpdateCustomPixelShaderConstants( ImDrawShader& shader, void* ptr_to_constants );
	void		UpdateCustomVertexShaderConstants( ImDrawShader& shader, void* ptr_to_constants );
	void		EndCustomShader( ImDrawList* draw );

	bool		ImIsMaximized();

	// Custom Title Bar
	bool		CustomTitleBarEnabled();
	void		EnableCustomTitleBar();
	void		DisableCustomTitleBar();

	void		DrawCustomMenuBarDefault();

	void		MinimizeApp();
	void		MaximizeApp();
	void		CloseApp();

	bool		BeginCustomTitleBar( float fHeight );
	void		EndCustomTitleBar();

	// Inits
	void SetFeatures( ImPlatformFeatures features );
	bool ValidateFeatures();

	// SimpleAPI:
	bool SimpleStart( char const* pWindowsName, ImVec2 const vPos, ImU32 const uWidth, ImU32 const uHeight );
	bool SimpleInitialize( bool hasViewport );
	void SimpleFinish();

	void SimpleBegin();
	void SimpleEnd( ImVec4 const vClearColor, bool hasViewport );

	// ExplicitAPI:
#ifdef CreateWindow
#undef CreateWindow // Windows API :(
#endif
	bool CreateWindow( char const* pWindowsName, ImVec2 const vPos, ImU32 const uWidth, ImU32 const uHeight );
	bool InitGfxAPI();
	bool ShowWindow();
	bool InitPlatform();
	bool InitGfx();
	bool PlatformContinue();
	bool PlatformEvents();
	bool GfxCheck();
	void GfxAPINewFrame();
	void PlatformNewFrame();
	bool GfxAPIClear( ImVec4 const vClearColor );
	// Need to pass the clear color to support dx12
	bool GfxAPIRender( ImVec4 const vClearColor );
	void GfxViewportPre();
	void GfxViewportPost();
	bool GfxAPISwapBuffer();
	void ShutdownGfxAPI();
	void ShutdownWindow();
	void ShutdownPostGfxAPI();
	void DestroyWindow();

	// Internal API:
	namespace Internal
	{
		// Resize is not handle in the same way depending on the GfxAPI, do not call by yourself
		bool WindowResize();
	}
}

#ifdef IM_PLATFORM_IMPLEMENTATION
#include <ImPlatform.cpp>
#endif

#endif
