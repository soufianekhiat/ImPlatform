//#include <ImPlatform.h>

#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
#include <backends/imgui_impl_win32.cpp>
#define DIRECTINPUT_VERSION 0x0800
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
#if (IM_CURRENT_GFX != IM_GFX_OPENGL2) && (IM_CURRENT_GFX != IM_GFX_OPENGL3)
#include <backends/imgui_impl_glfw.cpp>
#endif
#pragma comment( lib, "../GLFW/lib-vc2010-64/glfw3.lib" )
#elif (IM_CURRENT_PLATFORM) == IM_PLATFORM_APPLE)
#else
#error IM_CURRENT_TARGET not specified correctly
#endif

#if (IM_CURRENT_GFX == IM_GFX_DIRECTX9)
#include <backends/imgui_impl_dx9.cpp>
#include <d3d9.h>
#pragma comment( lib, "d3d9.lib" )
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)
#include <backends/imgui_impl_dx10.cpp>
#pragma comment( lib, "d3d10.lib" )
#pragma comment( lib, "d3dcompiler.lib" )
#pragma comment( lib, "dxgi.lib" )
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
#include <backends/imgui_impl_dx11.cpp>
#pragma comment( lib, "d3d11.lib" )
#pragma comment( lib, "d3dcompiler.lib" )
#pragma comment( lib, "dxgi.lib" )
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
#include <backends/imgui_impl_dx12.cpp>
#pragma comment( lib, "d3d12.lib" )
#pragma comment( lib, "d3dcompiler.lib" )
#pragma comment( lib, "dxgi.lib" )
#elif (IM_CURRENT_GFX == IM_GFX_VULKAN)
#include <backends/imgui_impl_vulkan.cpp>
#elif (IM_CURRENT_GFX == IM_GFX_METAL)
#include <backends/imgui_impl_metal.cpp>
#elif (IM_CURRENT_GFX == IM_GFX_OPENGL2)
#if (IM_CURRENT_PLATFORM != IM_PLATFORM_GLFW)
#include <backends/imgui_impl_opengl2.cpp>
#endif
#pragma comment( lib, "opengl32.lib" )
#elif (IM_CURRENT_GFX == IM_GFX_OPENGL3)
#if (IM_CURRENT_PLATFORM != IM_PLATFORM_GLFW)
#include <backends/imgui_impl_opengl3.cpp>
#endif
#pragma comment( lib, "opengl32.lib" )
#elif (IM_CURRENT_GFX == IM_GFX_WGPU)
#include <backends/imgui_impl_wgpu.cpp>
#endif

#if (IM_CURRENT_GFX == IM_GFX_OPENGL2 || IM_CURRENT_GFX == IM_GFX_OPENGL3) && (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
#if (IM_CURRENT_GFX == IM_GFX_OPENGL2)
#include <backends/imgui_impl_opengl2.cpp>
#elif (IM_CURRENT_GFX == IM_GFX_OPENGL3)
#include <backends/imgui_impl_opengl3.cpp>
#endif
#include <backends/imgui_impl_glfw.cpp>
#if _MSC_VER
#pragma comment(linker, "/NODEFAULTLIB:msvcrt.lib")
#endif
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif
#endif

PlatformDataImpl PlatformData;

namespace ImPlatform
{
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
	LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

#if (IM_CURRENT_GFX == IM_GFX_DIRECTX9) || (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || (IM_CURRENT_GFX == IM_GFX_DIRECTX11) || (IM_CURRENT_GFX == IM_GFX_DIRECTX12)

#if (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || (IM_CURRENT_GFX == IM_GFX_DIRECTX11) || (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
	void	ImCreateRenderTarget()
	{
#if (IM_CURRENT_GFX == IM_GFX_DIRECTX10)
		ID3D10Texture2D* pBackBuffer;
		PlatformData.pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
		PlatformData.pD3DDevice->CreateRenderTargetView(pBackBuffer, NULL, &PlatformData.pMainRenderTargetView);
		pBackBuffer->Release();
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
		ID3D11Texture2D* pBackBuffer;
		PlatformData.pSwapChain->GetBuffer( 0, IID_PPV_ARGS( &pBackBuffer ) );
		PlatformData.pD3DDevice->CreateRenderTargetView( pBackBuffer, NULL, &PlatformData.pMainRenderTargetView );
		pBackBuffer->Release();
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
		for ( UINT i = 0; i < PlatformData.NUM_BACK_BUFFERS; i++ )
		{
			ID3D12Resource* pBackBuffer = nullptr;
			PlatformData.pSwapChain->GetBuffer( i, IID_PPV_ARGS( &pBackBuffer ) );
			PlatformData.pD3DDevice->CreateRenderTargetView( pBackBuffer, nullptr, PlatformData.pMainRenderTargetDescriptor[ i ] );
			PlatformData.pMainRenderTargetResource[ i ] = pBackBuffer;
		}
#endif
	}
#endif

#if (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
	void ImWaitForLastSubmittedFrame()
	{
		PlatformDataImpl::FrameContext* frameCtx = &PlatformData.pFrameContext[ PlatformData.uFrameIndex % PlatformData.NUM_FRAMES_IN_FLIGHT ];

		UINT64 fenceValue = frameCtx->FenceValue;
		if ( fenceValue == 0 )
			return; // No fence was signaled

		frameCtx->FenceValue = 0;
		if ( PlatformData.pFence->GetCompletedValue() >= fenceValue )
			return;

		PlatformData.pFence->SetEventOnCompletion( fenceValue, PlatformData.pFenceEvent );
		WaitForSingleObject( PlatformData.pFenceEvent, INFINITE );
	}

	PlatformDataImpl::FrameContext* ImWaitForNextFrameResources()
	{
		UINT nextFrameIndex = PlatformData.uFrameIndex + 1;
		PlatformData.uFrameIndex = nextFrameIndex;

		HANDLE waitableObjects[] = { PlatformData.pSwapChainWaitableObject, nullptr };
		DWORD numWaitableObjects = 1;

		PlatformDataImpl::FrameContext* frameCtx = &PlatformData.pFrameContext[ nextFrameIndex % PlatformData.NUM_FRAMES_IN_FLIGHT ];
		UINT64 fenceValue = frameCtx->FenceValue;
		if ( fenceValue != 0 ) // means no fence was signaled
		{
			frameCtx->FenceValue = 0;
			PlatformData.pFence->SetEventOnCompletion( fenceValue, PlatformData.pFenceEvent );
			waitableObjects[ 1 ] = PlatformData.pFenceEvent;
			numWaitableObjects = 2;
		}

		WaitForMultipleObjects( numWaitableObjects, waitableObjects, TRUE, INFINITE );

		return frameCtx;
	}
#endif

	void	ImCleanupRenderTarget()
	{
#if (IM_CURRENT_GFX == IM_GFX_DIRECTX10)
		if (PlatformData.pMainRenderTargetView)
		{
			PlatformData.pMainRenderTargetView->Release();
			PlatformData.pMainRenderTargetView = nullptr;
		}
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
		ID3D11Texture2D* pBackBuffer;
		PlatformData.pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
		PlatformData.pD3DDevice->CreateRenderTargetView(pBackBuffer, nullptr, &PlatformData.pMainRenderTargetView);
		pBackBuffer->Release();
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)

		ImWaitForLastSubmittedFrame();

		for ( UINT i = 0; i < PlatformData.NUM_BACK_BUFFERS; i++ )
			if ( PlatformData.pMainRenderTargetResource[ i ] )
			{
				PlatformData.pMainRenderTargetResource[ i ]->Release();
				PlatformData.pMainRenderTargetResource[ i ] = nullptr;
			}

#ifdef DX12_ENABLE_DEBUG_LAYER
		IDXGIDebug1* pDebug = nullptr;
		if ( SUCCEEDED( DXGIGetDebugInterface1( 0, IID_PPV_ARGS( &pDebug ) ) ) )
		{
			pDebug->ReportLiveObjects( DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY );
			pDebug->Release();
		}
#endif
#endif
	}

	bool ImCreateDeviceD3D( HWND hWnd )
	{
#if (IM_CURRENT_GFX == IM_GFX_DIRECTX9)

		if ( ( PlatformData.pD3D = Direct3DCreate9( D3D_SDK_VERSION ) ) == nullptr )
			return false;

		// Create the D3DDevice
		ZeroMemory( &PlatformData.oD3Dpp, sizeof( PlatformData.oD3Dpp ) );
		PlatformData.oD3Dpp.Windowed				= TRUE;
		PlatformData.oD3Dpp.SwapEffect				= D3DSWAPEFFECT_DISCARD;
		PlatformData.oD3Dpp.BackBufferFormat		= D3DFMT_UNKNOWN; // Need to use an explicit format with alpha if needing per-pixel alpha composition.
		PlatformData.oD3Dpp.EnableAutoDepthStencil	= TRUE;
		PlatformData.oD3Dpp.AutoDepthStencilFormat	= D3DFMT_D16;
		//PlatformData.oD3Dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;	// Present with vsync
		PlatformData.oD3Dpp.PresentationInterval	= D3DPRESENT_INTERVAL_IMMEDIATE;		// Present without vsync, maximum unthrottled framerate
		if ( PlatformData.pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &PlatformData.oD3Dpp, &PlatformData.pD3DDevice ) < 0 )
			return false;

		return true;

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)

		// Setup swap chain
		DXGI_SWAP_CHAIN_DESC oSwapDesc;
		ZeroMemory( &oSwapDesc, sizeof( oSwapDesc ) );

		oSwapDesc.BufferCount							= 2;
		oSwapDesc.BufferDesc.Width						= 0;
		oSwapDesc.BufferDesc.Height						= 0;
		oSwapDesc.BufferDesc.Format						= DXGI_FORMAT_R8G8B8A8_UNORM;
		oSwapDesc.BufferDesc.RefreshRate.Numerator		= 60;
		oSwapDesc.BufferDesc.RefreshRate.Denominator	= 1;
		oSwapDesc.Flags									= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		oSwapDesc.BufferUsage							= DXGI_USAGE_RENDER_TARGET_OUTPUT;
		oSwapDesc.OutputWindow							= hWnd;
		oSwapDesc.SampleDesc.Count						= 1;
		oSwapDesc.SampleDesc.Quality					= 0;
		oSwapDesc.Windowed								= TRUE;
		oSwapDesc.SwapEffect							= DXGI_SWAP_EFFECT_DISCARD;

		UINT uCreateDeviceFlags = 0;
		//createDeviceFlags |= D3D10_CREATE_DEVICE_DEBUG;
		if ( D3D10CreateDeviceAndSwapChain( nullptr, D3D10_DRIVER_TYPE_HARDWARE, nullptr, uCreateDeviceFlags, D3D10_SDK_VERSION, &oSwapDesc, &PlatformData.pSwapChain, &PlatformData.pD3DDevice ) != S_OK )
			return false;

		ImCreateRenderTarget();

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)

		// Setup swap chain
		DXGI_SWAP_CHAIN_DESC oSwapDesc;
		ZeroMemory( &oSwapDesc, sizeof( oSwapDesc ) );
		oSwapDesc.BufferCount							= 2;
		oSwapDesc.BufferDesc.Width						= 0;
		oSwapDesc.BufferDesc.Height						= 0;
		oSwapDesc.BufferDesc.Format						= DXGI_FORMAT_R8G8B8A8_UNORM;
		oSwapDesc.BufferDesc.RefreshRate.Numerator		= 60;
		oSwapDesc.BufferDesc.RefreshRate.Denominator	= 1;
		oSwapDesc.Flags									= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		oSwapDesc.BufferUsage							= DXGI_USAGE_RENDER_TARGET_OUTPUT;
		oSwapDesc.OutputWindow							= hWnd;
		oSwapDesc.SampleDesc.Count						= 1;
		oSwapDesc.SampleDesc.Quality					= 0;
		oSwapDesc.Windowed								= TRUE;
		oSwapDesc.SwapEffect							= DXGI_SWAP_EFFECT_DISCARD;

		UINT uCreateDeviceFlags = 0;
		//createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
		D3D_FEATURE_LEVEL featureLevel;
		const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
		if ( D3D11CreateDeviceAndSwapChain( nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, uCreateDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &oSwapDesc, &PlatformData.pSwapChain, &PlatformData.pD3DDevice, &featureLevel, &PlatformData.pD3DDeviceContext ) != S_OK )
			return false;

		ImCreateRenderTarget();
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)

		// Setup swap chain
		DXGI_SWAP_CHAIN_DESC1 sd;
		{
			ZeroMemory( &sd, sizeof( sd ) );
			sd.BufferCount			= PlatformData.NUM_BACK_BUFFERS;
			sd.Width				= 0;
			sd.Height				= 0;
			sd.Format				= DXGI_FORMAT_R8G8B8A8_UNORM;
			sd.Flags				= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
			sd.BufferUsage			= DXGI_USAGE_RENDER_TARGET_OUTPUT;
			sd.SampleDesc.Count		= 1;
			sd.SampleDesc.Quality	= 0;
			sd.SwapEffect			= DXGI_SWAP_EFFECT_FLIP_DISCARD;
			sd.AlphaMode			= DXGI_ALPHA_MODE_UNSPECIFIED;
			sd.Scaling				= DXGI_SCALING_STRETCH;
			sd.Stereo				= FALSE;
		}

		// [DEBUG] Enable debug interface
#ifdef DX12_ENABLE_DEBUG_LAYER
		ID3D12Debug* pdx12Debug = nullptr;
		if ( SUCCEEDED( D3D12GetDebugInterface( IID_PPV_ARGS( &pdx12Debug ) ) ) )
			pdx12Debug->EnableDebugLayer();
#endif

		// Create device
		D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
		if ( D3D12CreateDevice( nullptr, featureLevel, IID_PPV_ARGS( &PlatformData.pD3DDevice ) ) != S_OK )
			return false;

		// [DEBUG] Setup debug interface to break on any warnings/errors
#ifdef DX12_ENABLE_DEBUG_LAYER
		if ( pdx12Debug != nullptr )
		{
			ID3D12InfoQueue* pInfoQueue = nullptr;
			PlatformData.pD3DDevice->QueryInterface( IID_PPV_ARGS( &pInfoQueue ) );
			pInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_ERROR, true );
			pInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_CORRUPTION, true );
			pInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_WARNING, true );
			pInfoQueue->Release();
			pdx12Debug->Release();
		}
#endif

		{
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			desc.NumDescriptors = PlatformData.NUM_BACK_BUFFERS;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			desc.NodeMask = 1;
			if ( PlatformData.pD3DDevice->CreateDescriptorHeap( &desc, IID_PPV_ARGS( &PlatformData.pD3DRTVDescHeap ) ) != S_OK )
				return false;

			SIZE_T rtvDescriptorSize = PlatformData.pD3DDevice->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );
			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = PlatformData.pD3DRTVDescHeap->GetCPUDescriptorHandleForHeapStart();
			for ( UINT i = 0; i < PlatformData.NUM_BACK_BUFFERS; i++ )
			{
				PlatformData.pMainRenderTargetDescriptor[ i ] = rtvHandle;
				rtvHandle.ptr += rtvDescriptorSize;
			}
		}

		{
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.NumDescriptors = 1;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			if ( PlatformData.pD3DDevice->CreateDescriptorHeap( &desc, IID_PPV_ARGS( &PlatformData.pD3DSRVDescHeap ) ) != S_OK )
				return false;
		}

		{
			D3D12_COMMAND_QUEUE_DESC desc = {};
			desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			desc.NodeMask = 1;
			if ( PlatformData.pD3DDevice->CreateCommandQueue( &desc, IID_PPV_ARGS( &PlatformData.pD3DCommandQueue ) ) != S_OK )
				return false;
		}

		for ( UINT i = 0; i < PlatformData.NUM_FRAMES_IN_FLIGHT; i++ )
			if ( PlatformData.pD3DDevice->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( &PlatformData.pFrameContext[ i ].CommandAllocator ) ) != S_OK )
				return false;

		if ( PlatformData.pD3DDevice->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_DIRECT, PlatformData.pFrameContext[ 0 ].CommandAllocator, nullptr, IID_PPV_ARGS( &PlatformData.pD3DCommandList ) ) != S_OK ||
			 PlatformData.pD3DCommandList->Close() != S_OK )
			return false;

		if ( PlatformData.pD3DDevice->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( &PlatformData.pFence ) ) != S_OK )
			return false;

		PlatformData.pFenceEvent = CreateEvent( nullptr, FALSE, FALSE, nullptr );
		if ( PlatformData.pFenceEvent == nullptr )
			return false;

		{
			IDXGIFactory4* dxgiFactory = nullptr;
			IDXGISwapChain1* swapChain1 = nullptr;
			if ( CreateDXGIFactory1( IID_PPV_ARGS( &dxgiFactory ) ) != S_OK )
				return false;
			if ( dxgiFactory->CreateSwapChainForHwnd( PlatformData.pD3DCommandQueue, hWnd, &sd, nullptr, nullptr, &swapChain1 ) != S_OK )
				return false;
			if ( swapChain1->QueryInterface( IID_PPV_ARGS( &PlatformData.pSwapChain ) ) != S_OK )
				return false;
			swapChain1->Release();
			dxgiFactory->Release();
			PlatformData.pSwapChain->SetMaximumFrameLatency( PlatformData.NUM_BACK_BUFFERS );
			PlatformData.pSwapChainWaitableObject = PlatformData.pSwapChain->GetFrameLatencyWaitableObject();
		}

		ImCreateRenderTarget();

#endif

		return true;
	}

	void ImCleanupDeviceD3D()
	{
#if (IM_CURRENT_GFX == IM_GFX_DIRECTX9)
		if ( PlatformData.pD3DDevice )
		{
			PlatformData.pD3DDevice->Release();
			PlatformData.pD3DDevice = nullptr;
		}
		if ( PlatformData.pD3D )
		{
			PlatformData.pD3D->Release();
			PlatformData.pD3D = nullptr;
		}
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)
		ImCleanupRenderTarget();
		if ( PlatformData.pSwapChain )
		{
			PlatformData.pSwapChain->Release();
			PlatformData.pSwapChain = nullptr;
		}
		if ( PlatformData.pD3DDevice )
		{
			PlatformData.pD3DDevice->Release();
			PlatformData.pD3DDevice = nullptr;
		}
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
		ImCleanupRenderTarget();
		if ( PlatformData.pSwapChain )
		{
			PlatformData.pSwapChain->Release();
			PlatformData.pSwapChain = nullptr;
		}
		if ( PlatformData.pD3DDeviceContext )
		{
			PlatformData.pD3DDeviceContext->Release();
			PlatformData.pD3DDeviceContext = nullptr;
		}
		if ( PlatformData.pD3DDevice )
		{
			PlatformData.pD3DDevice->Release();
			PlatformData.pD3DDevice = nullptr;
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
		ImCleanupRenderTarget();
		if ( PlatformData.pSwapChain )
		{
			PlatformData.pSwapChain->SetFullscreenState( false, nullptr ); PlatformData.pSwapChain->Release(); PlatformData.pSwapChain = nullptr;
		}
		if ( PlatformData.pSwapChainWaitableObject != nullptr )
		{
			CloseHandle( PlatformData.pSwapChainWaitableObject );
		}
		for ( UINT i = 0; i < PlatformData.NUM_FRAMES_IN_FLIGHT; i++ )
		{
			if ( PlatformData.pFrameContext[ i ].CommandAllocator )
			{
				PlatformData.pFrameContext[ i ].CommandAllocator->Release();
				PlatformData.pFrameContext[ i ].CommandAllocator = nullptr;
			}
		}
		if ( PlatformData.pD3DCommandQueue )
		{
			PlatformData.pD3DCommandQueue->Release();
			PlatformData.pD3DCommandQueue = nullptr;
		}
		if ( PlatformData.pD3DCommandList )
		{
			PlatformData.pD3DCommandList->Release();
			PlatformData.pD3DCommandList = nullptr;
		}
		if ( PlatformData.pD3DRTVDescHeap )
		{
			PlatformData.pD3DRTVDescHeap->Release();
			PlatformData.pD3DRTVDescHeap = nullptr;
		}
		if ( PlatformData.pD3DSRVDescHeap )
		{
			PlatformData.pD3DSRVDescHeap->Release();
			PlatformData.pD3DSRVDescHeap = nullptr;
		}
		if ( PlatformData.pFence )
		{
			PlatformData.pFence->Release();
			PlatformData.pFence = nullptr;
		}
		if ( PlatformData.pFenceEvent )
		{
			CloseHandle( PlatformData.pFenceEvent );
			PlatformData.pFenceEvent = nullptr;
		}
		if ( PlatformData.pD3DDevice )
		{
			PlatformData.pD3DDevice->Release();
			PlatformData.pD3DDevice = nullptr;
		}
#endif
	}

#if (IM_CURRENT_GFX == IM_GFX_DIRECTX9)
	void ImResetDevice()
	{
		ImGui_ImplDX9_InvalidateDeviceObjects();
		HRESULT hr = PlatformData.pD3DDevice->Reset( &PlatformData.oD3Dpp );
		if ( hr == D3DERR_INVALIDCALL )
			IM_ASSERT( 0 );

		ImGui_ImplDX9_CreateDeviceObjects();
	}
#endif
#elif IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32 && (IM_CURRENT_GFX == IM_GFX_OPENGL2 || IM_CURRENT_GFX == IM_GFX_OPENGL3)

bool ImCreateDeviceWGL( HWND hWnd, PlatformDataImpl::WGL_WindowData* data )
{
	HDC hDc = ::GetDC( hWnd );
	PIXELFORMATDESCRIPTOR pfd = { 0 };
	pfd.nSize		= sizeof( pfd );
	pfd.nVersion	= 1;
	pfd.dwFlags		= PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType	= PFD_TYPE_RGBA;
	pfd.cColorBits	= 32;

	const int pf = ::ChoosePixelFormat( hDc, &pfd );
	if ( pf == 0 )
		return false;
	if ( ::SetPixelFormat( hDc, pf, &pfd ) == FALSE )
		return false;
	::ReleaseDC( hWnd, hDc );

	data->hDC = ::GetDC( hWnd );
	if ( !PlatformData.pRC )
		PlatformData.pRC = wglCreateContext( data->hDC );

	return true;
}

void ImCleanupDeviceWGL( HWND hWnd, PlatformDataImpl::WGL_WindowData* data )
{
	wglMakeCurrent( nullptr, nullptr );
	::ReleaseDC( hWnd, data->hDC );
}

void ImResetDeviceWGL()
{

}

// Support function for multi-viewports
// Unlike most other backend combination, we need specific hooks to combine Win32+OpenGL.
// We could in theory decide to support Win32-specific code in OpenGL backend via e.g. an hypothetical ImGui_ImplOpenGL3_InitForRawWin32().
static void Im_Hook_Renderer_CreateWindow( ImGuiViewport* viewport )
{
	assert( viewport->RendererUserData == NULL );

	PlatformDataImpl::WGL_WindowData* data = IM_NEW( PlatformDataImpl::WGL_WindowData );
	ImCreateDeviceWGL( ( HWND )viewport->PlatformHandle, data );
	viewport->RendererUserData = data;
}

static void Im_Hook_Renderer_DestroyWindow( ImGuiViewport* viewport )
{
	if ( viewport->RendererUserData != NULL )
	{
		PlatformDataImpl::WGL_WindowData* data = ( PlatformDataImpl::WGL_WindowData* )viewport->RendererUserData;
		ImCleanupDeviceWGL( ( HWND )viewport->PlatformHandle, data );
		IM_DELETE( data );
		viewport->RendererUserData = NULL;
	}
}

static void Im_Hook_Platform_RenderWindow( ImGuiViewport* viewport, void* )
{
	// Activate the platform window DC in the OpenGL rendering context
	if ( PlatformDataImpl::WGL_WindowData* data = ( PlatformDataImpl::WGL_WindowData* )viewport->RendererUserData )
		wglMakeCurrent( data->hDC, PlatformData.pRC );
}

static void Im_Hook_Renderer_SwapBuffers( ImGuiViewport* viewport, void* )
{
	if ( PlatformDataImpl::WGL_WindowData* data = ( PlatformDataImpl::WGL_WindowData* )viewport->RendererUserData )
		::SwapBuffers( data->hDC );
}
#endif

#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
	// Win32 message handler
	LRESULT WINAPI WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
	{
		if ( ImGui_ImplWin32_WndProcHandler( hWnd, msg, wParam, lParam ) )
			return true;

		switch ( msg )
		{
		case WM_SIZE:
#if (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
			if ( PlatformData.pD3DDevice != nullptr && wParam != SIZE_MINIMIZED )
			{
				ImWaitForLastSubmittedFrame();
				ImCleanupRenderTarget();
				HRESULT result = PlatformData.pSwapChain->ResizeBuffers( 0, ( UINT )LOWORD( lParam ), ( UINT )HIWORD( lParam ), DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT );
				assert( SUCCEEDED( result ) && "Failed to resize swapchain." );
				ImCreateRenderTarget();
			}
#elif (IM_CURRENT_GFX == IM_GFX_OPENGL2) || (IM_CURRENT_GFX == IM_GFX_OPENGL3)
			if ( wParam != SIZE_MINIMIZED )
			{
				PlatformData.iWidth		= LOWORD( lParam );
				PlatformData.iHeight	= HIWORD( lParam );
			}
#else
			if ( wParam != SIZE_MINIMIZED )
			{
				if ( wParam == SIZE_MINIMIZED )
					return 0;
				PlatformData.uResizeWidth	= ( UINT )LOWORD( lParam ); // Queue resize
				PlatformData.uResizeHeight	= ( UINT )HIWORD( lParam );
			}
#endif
			return 0;
		case WM_SYSCOMMAND:
			if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
				return 0;
			break;
		case WM_DESTROY:
			::PostQuitMessage(0);
			return 0;
		case WM_DPICHANGED:
			if ( ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports )
			{
				//const int dpi = HIWORD(wParam);
				//printf("WM_DPICHANGED to %d (%.0f%%)\n", dpi, (float)dpi / 96.0f * 100.0f);
				const RECT* suggested_rect = ( RECT* )lParam;
				::SetWindowPos( hWnd, nullptr, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE );
			}
		}
		return ::DefWindowProc(hWnd, msg, wParam, lParam);
	}
#endif

#if IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW
	static void ImGLFWErrorCallback(int error, const char* description)
	{
		fprintf( stderr, "GLFW Error %d: %s\n", error, description );
	}
#endif

	// TODO: Add support for Windows Position, maximize/minimize/...
	bool ImCreateWindow( char const* pWindowsName, ImU32 const uWidth, ImU32 const uHeight )
	{
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)

#ifdef UNICODE
		const size_t WCHARBUF = 4096;
		wchar_t  wszDest[ WCHARBUF ];
		MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, pWindowsName, -1, wszDest, WCHARBUF );
#endif

		//ImGui_ImplWin32_EnableDpiAwareness();

		PlatformData.oWinStruct = { sizeof( PlatformData.oWinStruct ), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle( nullptr ), nullptr, nullptr, nullptr, nullptr, L"ImPlatform", nullptr };
		::RegisterClassExW( &PlatformData.oWinStruct );
		PlatformData.pHandle = ::CreateWindowW( PlatformData.oWinStruct.lpszClassName, wszDest, WS_OVERLAPPEDWINDOW, 100, 100, uWidth, uHeight, nullptr, nullptr, PlatformData.oWinStruct.hInstance, nullptr );

#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)

		glfwSetErrorCallback( ImGLFWErrorCallback );
		if ( !glfwInit() )
			return false;

		// Decide GL+GLSL versions
#if (IM_CURRENT_GFX == IM_GFX_OPENGL2)
		// GL ES 2.0 + GLSL 100
		PlatformData.pGLSLVersion = "#version 100";
		glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 2 );
		glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 0 );
		glfwWindowHint( GLFW_CLIENT_API, GLFW_OPENGL_ES_API );
#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_APPLE)
		// GL 3.2 + GLSL 150
		PlatformData.pGLSLVersion = "#version 150";
		glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
		glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 2 );
		glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );	// 3.2+ only
		glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE );				// Required on Mac
#elif (IM_CURRENT_GFX == IM_GFX_OPENGL3)
		// GL 3.0 + GLSL 130
		PlatformData.pGLSLVersion = "#version 130";
		glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
		glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 0 );
		//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);		// 3.2+ only
		//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);				// 3.0+ only
#endif

		PlatformData.pWindow = glfwCreateWindow( uWidth, uHeight, pWindowsName, nullptr, nullptr );
		if ( PlatformData.pWindow == nullptr )
			return false;

#elif (IM_CURRENT_PLATFORM) == IM_PLATFORM_APPLE)
#endif

		return true;
	}

	bool ImInitGfxAPI()
	{
#if (IM_CURRENT_GFX == IM_GFX_OPENGL2) || (IM_CURRENT_GFX == IM_GFX_OPENGL3)

#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
		if ( !ImCreateDeviceWGL( PlatformData.pHandle, &PlatformData.oMainWindow ) )
		{
			ImCleanupDeviceWGL( PlatformData.pHandle, &PlatformData.oMainWindow );
			::DestroyWindow( PlatformData.pHandle );
			::UnregisterClassW( PlatformData.oWinStruct.lpszClassName, PlatformData.oWinStruct.hInstance );
			return false;
		}
		wglMakeCurrent( PlatformData.oMainWindow.hDC, PlatformData.pRC );
#endif

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9) || (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || (IM_CURRENT_GFX == IM_GFX_DIRECTX11) || (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
		if ( !ImCreateDeviceD3D( PlatformData.pHandle ) )
		{
			ImCleanupDeviceD3D();
			::UnregisterClass( PlatformData.oWinStruct.lpszClassName, PlatformData.oWinStruct.hInstance );
			return false;
		}
#elif (IM_CURRENT_GFX == IM_GFX_VULKAN)
#elif (IM_CURRENT_GFX == IM_GFX_METAL)
#else
#error IM_CURRENT_TARGET not specified correctly
#endif

		return true;
	}

	bool ImShowWindow()
	{
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
		::ShowWindow( PlatformData.pHandle, SW_SHOWDEFAULT );
		::UpdateWindow( PlatformData.pHandle );
#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)

		glfwMakeContextCurrent( PlatformData.pWindow );
		glfwSwapInterval( 1 );

#elif (IM_CURRENT_PLATFORM) == IM_PLATFORM_APPLE)
#endif
		return true;
	}

	//void ImNewFrame()
	//{
	//	ImPlatform::ImGfxAPINewFrame();
	//	ImPlatform::ImPlatformNewFrame();
	//	ImGui::NewFrame();
	//}

	bool ImInitPlatform()
	{
		bool bGood;

#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)

#if (IM_CURRENT_GFX == IM_GFX_OPENGL2) || (IM_CURRENT_GFX == IM_GFX_OPENGL3)
		bGood = ImGui_ImplWin32_InitForOpenGL( PlatformData.pHandle );
		ImGuiIO& io = ImGui::GetIO();
		if ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
		{
			ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
			IM_ASSERT( platform_io.Renderer_CreateWindow == NULL );
			IM_ASSERT( platform_io.Renderer_DestroyWindow == NULL );
			IM_ASSERT( platform_io.Renderer_SwapBuffers == NULL );
			IM_ASSERT( platform_io.Platform_RenderWindow == NULL );
			platform_io.Renderer_CreateWindow	= Im_Hook_Renderer_CreateWindow;
			platform_io.Renderer_DestroyWindow	= Im_Hook_Renderer_DestroyWindow;
			platform_io.Renderer_SwapBuffers	= Im_Hook_Renderer_SwapBuffers;
			platform_io.Platform_RenderWindow	= Im_Hook_Platform_RenderWindow;
		}
#else
		bGood = ImGui_ImplWin32_Init( PlatformData.pHandle );
		ZeroMemory( &PlatformData.oMessage, sizeof( PlatformData.oMessage ) );
#endif
#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)

#if IM_CURRENT_GFX == IM_GFX_OPENGL2 || IM_CURRENT_GFX == IM_GFX_OPENGL3
		bGood = ImGui_ImplGlfw_InitForOpenGL( PlatformData.pWindow, true );
#ifdef __EMSCRIPTEN__
		ImGui_ImplGlfw_InstallEmscriptenCanvasResizeCallback( "#canvas" );
#endif
#endif

#endif
		return bGood;
	}

	bool ImInitGfx()
	{
#if (IM_CURRENT_GFX == IM_GFX_OPENGL2)
		return ImGui_ImplOpenGL2_Init();
#elif (IM_CURRENT_GFX == IM_GFX_OPENGL3)
		return ImGui_ImplOpenGL3_Init( PlatformData.pGLSLVersion );
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9)
		return ImGui_ImplDX9_Init( PlatformData.pD3DDevice );
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)
		return ImGui_ImplDX10_Init( PlatformData.pD3DDevice );
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
		return ImGui_ImplDX11_Init( PlatformData.pD3DDevice, PlatformData.pD3DDeviceContext );
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
		return ImGui_ImplDX12_Init( PlatformData.pD3DDevice, PlatformData.NUM_FRAMES_IN_FLIGHT,
							 DXGI_FORMAT_R8G8B8A8_UNORM, PlatformData.pD3DSRVDescHeap,
							 PlatformData.pD3DSRVDescHeap->GetCPUDescriptorHandleForHeapStart(),
							 PlatformData.pD3DSRVDescHeap->GetGPUDescriptorHandleForHeapStart() );
#elif (IM_CURRENT_GFX) == IM_GFX_VULKAN)
		return ImGui_ImplVulkan_Init(..);
#elif (IM_CURRENT_GFX) == IM_GFX_METAL)
		return ImGui_ImplMetal_Init(..);
#else
#error IM_CURRENT_TARGET not specified correctly
		return false;
#endif
	}

	void ImDestroyWindow()
	{
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
		::DestroyWindow( PlatformData.pHandle );
		::UnregisterClass( PlatformData.oWinStruct.lpszClassName, PlatformData.oWinStruct.hInstance );
#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
		glfwDestroyWindow( PlatformData.pWindow );
		glfwTerminate();
#elif (IM_CURRENT_PLATFORM) == IM_PLATFORM_APPLE)
#endif
	}

	bool ImGfxCheck()
	{
#if (IM_CURRENT_GFX == IM_GFX_DIRECTX9)
		// Handle lost D3D9 device
		if ( PlatformData.bDeviceLost )
		{
			HRESULT hr = PlatformData.pD3DDevice->TestCooperativeLevel();
			if ( hr == D3DERR_DEVICELOST )
			{
				::Sleep( 10 );
				return false;
			}
			if ( hr == D3DERR_DEVICENOTRESET )
				ImResetDevice();

			PlatformData.bDeviceLost = false;
		}
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || (IM_CURRENT_GFX == IM_GFX_DIRECTX11) || (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
		if ( PlatformData.bSwapChainOccluded && PlatformData.pSwapChain->Present( 0, DXGI_PRESENT_TEST ) == DXGI_STATUS_OCCLUDED )
		{
			::Sleep( 10 );
			return false;
		}
		PlatformData.bSwapChainOccluded = false;
#endif

#if (IM_CURRENT_GFX != IM_GFX_DIRECTX12) && (IM_CURRENT_GFX != IM_GFX_OPENGL2) && (IM_CURRENT_GFX != IM_GFX_OPENGL3)
		// Handle window resize (we don't resize directly in the WM_SIZE handler)
		if ( PlatformData.uResizeWidth != 0 && PlatformData.uResizeHeight != 0 )
		{
			Internal::ImWindowResize();
		}
#endif

		//ImPlatform::ImNewFrame();

		return true;
	}

	void ImGfxViewportPre()
	{
#if IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW
		PlatformData.pBackupContext = glfwGetCurrentContext();
#endif
	}

	void ImGfxViewportPost()
	{
#if IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32
#if IM_CURRENT_GFX == IM_GFX_OPENGL2 || IM_CURRENT_GFX == IM_GFX_OPENGL3
		// Restore the OpenGL rendering context to the main window DC, since platform windows might have changed it.
		wglMakeCurrent( PlatformData.oMainWindow.hDC, PlatformData.pRC );
#endif
#elif IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW
		glfwMakeContextCurrent( PlatformData.pBackupContext );
#endif
	}

	//////////////////////////////////////////////////////////////////////////

	bool ImPlatformContinue()
	{
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
		return PlatformData.oMessage.message != WM_QUIT;
#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
		return !glfwWindowShouldClose( PlatformData.pWindow );
#elif (IM_CURRENT_PLATFORM) == IM_PLATFORM_APPLE)
#endif
	}

	bool ImPlatformEvents()
	{
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
		while ( ::PeekMessage( &PlatformData.oMessage, nullptr, 0U, 0U, PM_REMOVE ) )
		{
			::TranslateMessage( &PlatformData.oMessage );
			::DispatchMessage( &PlatformData.oMessage );

			if ( PlatformData.oMessage.message == WM_QUIT )
				return true;
		}

		return false;
#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
		glfwPollEvents();
		return false;
#elif (IM_CURRENT_PLATFORM) == IM_PLATFORM_APPLE)
#endif
	}

	void ImGfxAPINewFrame()
	{
#if (IM_CURRENT_GFX == IM_GFX_OPENGL2)
		ImGui_ImplOpenGL2_NewFrame();
#elif (IM_CURRENT_GFX == IM_GFX_OPENGL3)
		ImGui_ImplOpenGL3_NewFrame();
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9)
		ImGui_ImplDX9_NewFrame();
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)
		ImGui_ImplDX10_NewFrame();
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
		ImGui_ImplDX11_NewFrame();
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
		ImGui_ImplDX12_NewFrame();
#elif (IM_CURRENT_GFX) == IM_GFX_VULKAN)
		ImGui_ImplVulkan_NewFrame();
#elif (IM_CURRENT_GFX) == IM_GFX_METAL)
		ImGui_ImplMetal_NewFrame();
#elif (IM_CURRENT_GFX) == IM_GFX_WGPU)
		ImGui_ImplWGPU_NewFrame();
#else
#error IM_CURRENT_TARGET not specified correctly
#endif
	}

	void ImPlatformNewFrame()
	{
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
		ImGui_ImplWin32_NewFrame();
#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
		ImGui_ImplGlfw_NewFrame();
#elif (IM_CURRENT_PLATFORM) == IM_PLATFORM_APPLE)
#endif
	}

	bool ImGfxAPIClear( ImVec4 const vClearColor )
	{
#if (IM_CURRENT_GFX != IM_GFX_DIRECTX12)
		ImGui::EndFrame();
#endif

#if IM_CURRENT_GFX == IM_GFX_OPENGL2 || IM_CURRENT_GFX == IM_GFX_OPENGL3

#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)

		// TODO
		glViewport( 0, 0, PlatformData.iWidth, PlatformData.iHeight );

#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)

		int iWidth, iHeight;
		glfwGetFramebufferSize( PlatformData.pWindow, &iWidth, &iHeight );
		glViewport( 0, 0, iWidth, iHeight );

#elif (IM_CURRENT_PLATFORM) == IM_PLATFORM_APPLE)
#endif

		glClearColor( vClearColor.x, vClearColor.y, vClearColor.z, vClearColor.w );
		glClear( GL_COLOR_BUFFER_BIT );

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9)

		PlatformData.pD3DDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
		PlatformData.pD3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		PlatformData.pD3DDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(vClearColor.x * vClearColor.w * 255.0f), (int)(vClearColor.y * vClearColor.w * 255.0f), (int)(vClearColor.z * vClearColor.w * 255.0f), (int)(vClearColor.w * 255.0f));
		PlatformData.pD3DDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)

		float const pClearColorWithAlpha[4] = { vClearColor.x * vClearColor.w, vClearColor.y * vClearColor.w, vClearColor.z * vClearColor.w, vClearColor.w };
		PlatformData.pD3DDevice->OMSetRenderTargets( 1, &PlatformData.pMainRenderTargetView, nullptr );
		PlatformData.pD3DDevice->ClearRenderTargetView( PlatformData.pMainRenderTargetView, pClearColorWithAlpha );

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)

		float const pClearColorWithAlpha[4] = { vClearColor.x * vClearColor.w, vClearColor.y * vClearColor.w, vClearColor.z * vClearColor.w, vClearColor.w };
		PlatformData.pD3DDeviceContext->OMSetRenderTargets( 1, &PlatformData.pMainRenderTargetView, nullptr );
		PlatformData.pD3DDeviceContext->ClearRenderTargetView( PlatformData.pMainRenderTargetView, pClearColorWithAlpha );

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)

#elif (IM_CURRENT_GFX == IM_GFX_VULKAN)



#elif (IM_CURRENT_GFX == IM_GFX_METAL)



#else

#error IM_CURRENT_TARGET not specified correctly

#endif

		return true;
	}

	bool ImGfxAPIRender( ImVec4 const vClearColor )
	{
#if (IM_CURRENT_GFX == IM_GFX_OPENGL2)
		ImGui::Render();
		ImGui_ImplOpenGL2_RenderDrawData( ImGui::GetDrawData() );
#elif (IM_CURRENT_GFX == IM_GFX_OPENGL3)
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9)
		if ( PlatformData.pD3DDevice->BeginScene() >= 0 )
		{
			ImGui::Render();
			ImGui_ImplDX9_RenderDrawData( ImGui::GetDrawData() );
			PlatformData.pD3DDevice->EndScene();
		}
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)
		ImGui::Render();
		ImGui_ImplDX10_RenderDrawData( ImGui::GetDrawData() );
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData() );
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
		ImGui::Render();

		PlatformDataImpl::FrameContext* frameCtx = ImWaitForNextFrameResources();
		UINT backBufferIdx = PlatformData.pSwapChain->GetCurrentBackBufferIndex();
		frameCtx->CommandAllocator->Reset();

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = PlatformData.pMainRenderTargetResource[ backBufferIdx ];
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		PlatformData.pD3DCommandList->Reset( frameCtx->CommandAllocator, nullptr );
		PlatformData.pD3DCommandList->ResourceBarrier( 1, &barrier );

		// Render Dear ImGui graphics
		const float clear_color_with_alpha[ 4 ] = { vClearColor.x * vClearColor.w, vClearColor.y * vClearColor.w, vClearColor.z * vClearColor.w, vClearColor.w };
		PlatformData.pD3DCommandList->ClearRenderTargetView( PlatformData.pMainRenderTargetDescriptor[ backBufferIdx ], clear_color_with_alpha, 0, nullptr );
		PlatformData.pD3DCommandList->OMSetRenderTargets( 1, &PlatformData.pMainRenderTargetDescriptor[ backBufferIdx ], FALSE, nullptr );
		PlatformData.pD3DCommandList->SetDescriptorHeaps( 1, &PlatformData.pD3DSRVDescHeap );
		ImGui_ImplDX12_RenderDrawData( ImGui::GetDrawData(), PlatformData.pD3DCommandList );
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		PlatformData.pD3DCommandList->ResourceBarrier( 1, &barrier );
		PlatformData.pD3DCommandList->Close();

		PlatformData.pD3DCommandQueue->ExecuteCommandLists( 1, ( ID3D12CommandList* const* )&PlatformData.pD3DCommandList );

#elif (IM_CURRENT_GFX) == IM_GFX_VULKAN)
		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData(), ... );
#elif (IM_CURRENT_GFX) == IM_GFX_METAL)
		ImGui::Render();
		ImGui_ImplMetal_RenderDrawData( ImGui::GetDrawData(), ... );
#elif (IM_CURRENT_GFX) == IM_GFX_WGPU)
		ImGui::Render();
		ImGui_ImplWGPU_RenderDrawData( ImGui::GetDrawData(), ... );
#else
#error IM_CURRENT_TARGET not specified correctly
#endif

		return true;
	}

	bool ImGfxAPISwapBuffer()
	{
#if (IM_CURRENT_GFX == IM_GFX_OPENGL2) || (IM_CURRENT_GFX == IM_GFX_OPENGL3)

#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
		::SwapBuffers( PlatformData.oMainWindow.hDC );

#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)

		glfwSwapBuffers( PlatformData.pWindow );
#elif (IM_CURRENT_PLATFORM) == IM_PLATFORM_APPLE)
#endif

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9)

		HRESULT uResult = PlatformData.pD3DDevice->Present( nullptr, nullptr, nullptr, nullptr );
		if ( uResult == D3DERR_DEVICELOST )
			PlatformData.bDeviceLost = true;

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || (IM_CURRENT_GFX == IM_GFX_DIRECTX11)

		HRESULT uResult = PlatformData.pSwapChain->Present( 0, 0 ); // Present without vsync
		PlatformData.bSwapChainOccluded = ( uResult == DXGI_STATUS_OCCLUDED );

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)

		// Present
		HRESULT hr = PlatformData.pSwapChain->Present( 1, 0 );   // Present with vsync
		//HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
		PlatformData.bSwapChainOccluded = ( hr == DXGI_STATUS_OCCLUDED );

		UINT64 fenceValue = PlatformData.uFenceLastSignaledValue + 1;
		PlatformData.pD3DCommandQueue->Signal( PlatformData.pFence, fenceValue );
		PlatformData.uFenceLastSignaledValue = fenceValue;

		PlatformDataImpl::FrameContext* frameCtx = &PlatformData.pFrameContext[ PlatformData.uFrameIndex % PlatformData.NUM_FRAMES_IN_FLIGHT ];
		frameCtx->FenceValue = fenceValue;

#elif (IM_CURRENT_GFX == IM_GFX_VULKAN)
#elif (IM_CURRENT_GFX == IM_GFX_METAL)
#elif (IM_CURRENT_GFX == IM_GFX_WGPU)
#else
#error IM_CURRENT_TARGET not specified correctly
#endif

		return true;
	}

	void ImShutdownGfxAPI()
	{
#if (IM_CURRENT_GFX == IM_GFX_OPENGL2)
		ImGui_ImplOpenGL2_Shutdown();
#elif (IM_CURRENT_GFX == IM_GFX_OPENGL3)
		ImGui_ImplOpenGL3_Shutdown();
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9)
		ImGui_ImplDX9_Shutdown();
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)
		ImGui_ImplDX10_Shutdown();
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
		ImGui_ImplDX11_Shutdown();
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
		ImGui_ImplDX12_Shutdown();
#elif (IM_CURRENT_GFX) == IM_GFX_VULKAN)
		ImGui_ImplVulkan_Shutdown();
#elif (IM_CURRENT_GFX) == IM_GFX_METAL)
		ImGui_ImplMetal_Shutdown();
#else
#error IM_CURRENT_TARGET not specified correctly
#endif
	}

	void ImShutdownPostGfxAPI()
	{
#if (IM_CURRENT_GFX == IM_GFX_OPENGL2) || (IM_CURRENT_GFX == IM_GFX_OPENGL3)

#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
		ImCleanupDeviceWGL( PlatformData.pHandle, &PlatformData.oMainWindow );
		wglDeleteContext( PlatformData.pRC );
#endif

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9) || (IM_CURRENT_GFX == IM_GFX_DIRECTX10)
		ImCleanupDeviceD3D();
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
#elif (IM_CURRENT_GFX == IM_GFX_VULKAN)
#elif (IM_CURRENT_GFX == IM_GFX_METAL)
#else
#error IM_CURRENT_TARGET not specified correctly
#endif
	}

	void ImShutdownWindow()
	{
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
		ImGui_ImplWin32_Shutdown();
#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
		ImGui_ImplGlfw_Shutdown();
#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_APPLE)
#else
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	bool ImSimpleStart( char const* pWindowsName, ImU32 const uWidth, ImU32 const uHeight )
	{
		bool bGood = true;

		bGood &= ImPlatform::ImCreateWindow( pWindowsName, uWidth, uHeight );
		bGood &= ImPlatform::ImInitGfxAPI();
		bGood &= ImPlatform::ImShowWindow();
		IMGUI_CHECKVERSION();
		bGood &= ImGui::CreateContext() != nullptr;

		return bGood;
	}

	bool ImSimpleInitialize( bool hasViewport )
	{
		bool bGood = true;

		if ( hasViewport )
		{
			ImGuiStyle& style = ImGui::GetStyle();
			style.WindowRounding = 0.0f;
			style.Colors[ ImGuiCol_WindowBg ].w = 1.0f;
		}

		bGood &= ImPlatform::ImInitPlatform();
		bGood &= ImPlatform::ImInitGfx();

		return bGood;
	}

	void ImSimpleFinish()
	{
		ImPlatform::ImShutdownGfxAPI();
		ImPlatform::ImShutdownWindow();

		ImGui::DestroyContext();

		ImPlatform::ImShutdownPostGfxAPI();

		ImPlatform::ImDestroyWindow();
	}

	void ImSimpleBegin()
	{
		ImPlatform::ImGfxAPINewFrame();
		ImPlatform::ImPlatformNewFrame();

		ImGui::NewFrame();
	}

	void ImSimpleEnd( ImVec4 const vClearColor, bool hasViewport )
	{
		ImPlatform::ImGfxAPIClear( vClearColor );
		ImPlatform::ImGfxAPIRender( vClearColor );

		if ( hasViewport )
		{
			ImPlatform::ImGfxViewportPre();

			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();

			ImPlatform::ImGfxViewportPost();
		}

		ImPlatform::ImGfxAPISwapBuffer();
	}

	namespace Internal
	{
		bool ImWindowResize()
		{
#if (IM_CURRENT_GFX == IM_GFX_DIRECTX9)
			PlatformData.oD3Dpp.BackBufferWidth = PlatformData.uResizeWidth;
			PlatformData.oD3Dpp.BackBufferHeight = PlatformData.uResizeHeight;
			PlatformData.uResizeWidth = PlatformData.uResizeHeight = 0;
			ImResetDevice();

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
			ImCleanupRenderTarget();
			PlatformData.pSwapChain->ResizeBuffers( 0, PlatformData.uResizeWidth, PlatformData.uResizeHeight, DXGI_FORMAT_UNKNOWN, 0 );
			PlatformData.uResizeWidth = PlatformData.uResizeHeight = 0;
			ImCreateRenderTarget();
#endif

			return true;
		}
	}
}
