#pragma once

#ifndef __IM_PLATFORM_H__
#define __IM_PLATFORM_H__

// For Dx12: #define ImTextureID ImU64

#include <imgui.h>

// To override the link of glfw3 do not use, other it ImPlatform will use the lib included on imgui
//#define IM_GLFW3_AUTO_LINK
// Use https://github.com/TheCherno/glfw/tree/dev to have CustomTitleBar Available
//#define IM_THE_CHERNO_GLFW3

// - [   ] WIN32_OPENGL3 // TBD
// - [ O ] WIN32_DIRECTX11
// - [   ] WIN32_DIRECTX12 // Issue with images
// - [   ] GLFW_OPENGL3 // Do not work well with high DPI
// - [ O ] GLFW_VULKAN
// - [   ] GLFW_EMSCRIPTEM_OPENGL3

// INTERNAL_MACRO
#define IM_GFX_OPENGL3		( 1u << 0u )
#define IM_GFX_DIRECTX11	( 1u << 1u )
#define IM_GFX_DIRECTX12	( 1u << 2u )
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

#ifdef __DEAR_GFX_DX11__
#define IM_CURRENT_GFX IM_GFX_DIRECTX11
#elif defined(__DEAR_GFX_DX12__)
#define IM_CURRENT_GFX IM_GFX_DIRECTX12
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
#define IM_TARGET_WIN32_DX11	( IM_PLATFORM_WIN32 | IM_GFX_DIRECTX11 )
#define IM_TARGET_WIN32_DX12	( IM_PLATFORM_WIN32 | IM_GFX_DIRECTX12 )
#define IM_TARGET_WIN32_OPENGL3	( IM_PLATFORM_WIN32 | IM_GFX_OPENGL3 )

#define IM_TARGET_APPLE_METAL	( IM_PLATFORM_APPLE | IM_GFX_METAL )

#define IM_TARGET_GLFW_OPENGL3	( IM_PLATFORM_GLFW | IM_GFX_OPENGL3 )

#define IM_TARGET_GLFW_VULKAN	( IM_PLATFORM_GLFW | IM_GFX_VULKAN )

#define IM_TARGET_GLFW_METAL	( IM_PLATFORM_GLFW | IM_GFX_METAL )

#if IM_CURRENT_TARGET != IM_TARGET_WIN32_DX11		&&	\
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

#if (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
#include <backends/imgui_impl_dx11.h>
#include <d3d11.h>
#define IM_SUPPORT_CUSTOM_SHADER
#define IM_GFX_HLSL
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
#include <backends/imgui_impl_dx12.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#define IM_SUPPORT_CUSTOM_SHADER
#define IM_GFX_HLSL
#elif (IM_CURRENT_GFX == IM_GFX_VULKAN)
#include <backends/imgui_impl_vulkan.h>
#define IM_GFX_GLSL
#elif (IM_CURRENT_GFX == IM_GFX_METAL)
#include <backends/imgui_impl_metal.h>
#define IM_GFX_MSL
#elif (IM_CURRENT_GFX == IM_GFX_OPENGL3)
// The backend cpp (imgui_impl_opengl3.cpp) is compiled separately
// and uses Dear ImGui's integrated OpenGL loader
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
#if (IM_CURRENT_GFX != IM_GFX_DIRECTX12) && (IM_CURRENT_GFX != IM_GFX_OPENGL3)
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

#if (IM_CURRENT_GFX == IM_GFX_OPENGL3)

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

	char const*				pGLSLVersion			= nullptr;  // Will be auto-detected if not specified
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

	static int const			NUM_SRV_DESCRIPTORS			= 128;
	UINT						uNextSRVDescriptor			= 1;
	ID3D12Resource*				pSRVResources[ NUM_SRV_DESCRIPTORS ] = {};
#elif (IM_CURRENT_GFX == IM_GFX_VULKAN)
	VkAllocationCallbacks*		pAllocator					= nullptr;
	VkInstance					pInstance					= VK_NULL_HANDLE;
	VkPhysicalDevice			pPhysicalDevice				= VK_NULL_HANDLE;
	VkDevice					pDevice						= VK_NULL_HANDLE;
	uint32_t					uQueueFamily				= (uint32_t)-1;
	VkQueue						pQueue						= VK_NULL_HANDLE;
	VkPipelineCache				pPipelineCache				= VK_NULL_HANDLE;
	VkDescriptorPool			pDescriptorPool				= VK_NULL_HANDLE;
	ImGui_ImplVulkanH_Window	oMainWindowData;
	uint32_t					uMinImageCount				= 2;
	bool						bSwapChainRebuild			= false;
	VkImageUsageFlags			uSwapChainImageUsage		= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
#ifdef _DEBUG
	VkDebugReportCallbackEXT	pDebugReport				= VK_NULL_HANDLE;
#endif
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
	bool		CustomTitleBarSupported();
	bool		CustomTitleBarEnabled();
	void		EnableCustomTitleBar();
	void		DisableCustomTitleBar();

	void		DrawCustomMenuBarDefault();

	void		MinimizeApp();
	void		MaximizeApp();
	void		CloseApp();

	bool		BeginCustomTitleBar( float fHeight );
	void		EndCustomTitleBar();

	// Configuration
	void SetFeatures( ImPlatformFeatures features );
	bool ValidateFeatures();

	// OpenGL3 specific configuration (call before SimpleStart or CreateWindow)
	void SetGLSLVersion( char const* glsl_version );

	// DX12 specific configuration (call before SimpleStart or CreateWindow)
	void SetDX12FramesInFlight( int frames_in_flight );
	void SetDX12RTVFormat( int rtv_format );  // DXGI_FORMAT
	void SetDX12DSVFormat( int dsv_format );  // DXGI_FORMAT

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

// Include common implementation code
#ifndef IM_PLATFORM_IMPLEMENTATION
#include <ImPlatform.h>
#endif

#include <string>
#include <fstream>
#include <imgui_internal.h>

// Platform-specific includes
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
#include <windowsx.h>
#define DIRECTINPUT_VERSION 0x0800
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
#ifdef IM_GLFW3_AUTO_LINK
#pragma comment( lib, "lib-vc2010-64/glfw3.lib" )
#pragma comment( lib, "legacy_stdio_definitions.lib" )
#endif
#elif (IM_CURRENT_PLATFORM) == IM_PLATFORM_APPLE)
#else
#error IM_CURRENT_TARGET not specified correctly
#endif

// Include ImGui platform backend implementations
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
#include <backends/imgui_impl_win32.cpp>
#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
#include <backends/imgui_impl_glfw.cpp>
#endif

// Include ImGui graphics backend implementations
#if (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
#include <backends/imgui_impl_dx11.cpp>
#pragma comment( lib, "d3d11.lib" )
#pragma comment( lib, "d3dcompiler.lib" )
#pragma comment( lib, "dxgi.lib" )
#include <comdef.h>
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
#include <backends/imgui_impl_dx12.cpp>
#pragma comment( lib, "d3d12.lib" )
#pragma comment( lib, "d3dcompiler.lib" )
#pragma comment( lib, "dxgi.lib" )
#elif (IM_CURRENT_GFX == IM_GFX_VULKAN)
#include <backends/imgui_impl_vulkan.cpp>
#elif (IM_CURRENT_GFX == IM_GFX_METAL)
#include <backends/imgui_impl_metal.cpp>
#elif (IM_CURRENT_GFX == IM_GFX_OPENGL3)
#include <backends/imgui_impl_opengl3.cpp>
#pragma comment( lib, "opengl32.lib" )
#elif (IM_CURRENT_GFX == IM_GFX_WGPU)
#include <backends/imgui_impl_wgpu.cpp>
#endif

// Include ImPlatform graphics backend implementations
#if (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
#include "implatform_impl_dx11.cpp"
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
#include "implatform_impl_dx12.cpp"
#elif (IM_CURRENT_GFX == IM_GFX_VULKAN)
#include "implatform_impl_vulkan.cpp"
#elif (IM_CURRENT_GFX == IM_GFX_OPENGL3)
#include "implatform_impl_opengl3.cpp"
#endif

// Include ImPlatform platform backend implementations
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
#include "implatform_impl_win32.cpp"
#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
#include "implatform_impl_glfw.cpp"
#endif

// Include common implementation code (non-backend-specific)
#include <ImPlatform.cpp>

#endif

#endif
