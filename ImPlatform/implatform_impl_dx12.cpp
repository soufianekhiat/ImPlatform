// ImPlatform DirectX 12 Backend Implementation - Pure C Style
// This file implements ImPlatform::Backend:: namespace functions for DX12
// The linker resolves these at link-time based on which .cpp is included

#include "ImPlatformBackend.h"
#include "ImPlatform.h"
#include <imgui.h>
#include <imgui_internal.h>

// DX12 includes
#include <backends/imgui_impl_dx12.h>
// Note: imgui_impl_dx12.cpp is included by main ImPlatform.cpp, don't include it again
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>

#pragma comment( lib, "d3d12.lib" )
#pragma comment( lib, "d3dcompiler.lib" )
#pragma comment( lib, "dxgi.lib" )

namespace ImPlatform
{
	extern PlatformDataImpl PlatformData;

	// Helper function for DX11/DX12 texture format conversion
	ImS32 ImPixelTypeChannelToDx11_12( DXGI_FORMAT* internalformat, ImPixelType const eType, ImPixelChannel const eChannel )
	{
		int sizeofChannel = -1;
		switch ( eChannel )
		{
		case ImPixelChannel_R:
			switch ( eType )
			{
			case ImPixelType_UInt8:
				sizeofChannel = 1;
				*internalformat = DXGI_FORMAT_R8_UINT;
				break;
			case ImPixelType_Int8:
				sizeofChannel = 1;
				*internalformat = DXGI_FORMAT_R8_SINT;
				break;
			case ImPixelType_UInt16:
				sizeofChannel = 2;
				*internalformat = DXGI_FORMAT_R16_UINT;
				break;
			case ImPixelType_Int16:
				sizeofChannel = 2;
				*internalformat = DXGI_FORMAT_R16_SINT;
				break;
			case ImPixelType_UInt32:
				sizeofChannel = 4;
				*internalformat = DXGI_FORMAT_R32_UINT;
				break;
			case ImPixelType_Int32:
				sizeofChannel = 4;
				*internalformat = DXGI_FORMAT_R32_SINT;
				break;
			case ImPixelType_Float16:
				sizeofChannel = 2;
				*internalformat = DXGI_FORMAT_R16_FLOAT;
				break;
			case ImPixelType_Float32:
				sizeofChannel = 4;
				*internalformat = DXGI_FORMAT_R32_FLOAT;
				break;
			}
			break;
		case ImPixelChannel_RG:
			switch ( eType )
			{
			case ImPixelType_UInt8:
				sizeofChannel = 1;
				*internalformat = DXGI_FORMAT_R8G8_UINT;
				break;
			case ImPixelType_Int8:
				sizeofChannel = 1;
				*internalformat = DXGI_FORMAT_R8G8_SINT;
				break;
			case ImPixelType_UInt16:
				sizeofChannel = 2;
				*internalformat = DXGI_FORMAT_R16G16_UINT;
				break;
			case ImPixelType_Int16:
				sizeofChannel = 2;
				*internalformat = DXGI_FORMAT_R16G16_SINT;
				break;
			case ImPixelType_UInt32:
				sizeofChannel = 4;
				*internalformat = DXGI_FORMAT_R32G32_UINT;
				break;
			case ImPixelType_Int32:
				sizeofChannel = 4;
				*internalformat = DXGI_FORMAT_R32G32_SINT;
				break;
			case ImPixelType_Float16:
				sizeofChannel = 2;
				*internalformat = DXGI_FORMAT_R16G16_FLOAT;
				break;
			case ImPixelType_Float32:
				sizeofChannel = 4;
				*internalformat = DXGI_FORMAT_R32G32_FLOAT;
				break;
			}
			break;
		case ImPixelChannel_RGB:
			switch ( eType )
			{
			case ImPixelType_UInt32:
				sizeofChannel = 4;
				*internalformat = DXGI_FORMAT_R32G32B32_UINT;
				break;
			case ImPixelType_Int32:
				sizeofChannel = 4;
				*internalformat = DXGI_FORMAT_R32G32B32_SINT;
				break;
			case ImPixelType_Float32:
				sizeofChannel = 4;
				*internalformat = DXGI_FORMAT_R32G32B32_FLOAT;
				break;
			}
			break;
		case ImPixelChannel_RGBA:
			switch ( eType )
			{
			case ImPixelType_UInt8:
				sizeofChannel = 1;
				*internalformat = DXGI_FORMAT_R8G8B8A8_UINT;
				break;
			case ImPixelType_Int8:
				sizeofChannel = 1;
				*internalformat = DXGI_FORMAT_R8G8B8A8_SINT;
				break;
			case ImPixelType_UInt16:
				sizeofChannel = 2;
				*internalformat = DXGI_FORMAT_R16G16B16A16_UINT;
				break;
			case ImPixelType_Int16:
				sizeofChannel = 2;
				*internalformat = DXGI_FORMAT_R16G16B16A16_SINT;
				break;
			case ImPixelType_UInt32:
				sizeofChannel = 4;
				*internalformat = DXGI_FORMAT_R32G32B32A32_UINT;
				break;
			case ImPixelType_Int32:
				sizeofChannel = 4;
				*internalformat = DXGI_FORMAT_R32G32B32A32_SINT;
				break;
			case ImPixelType_Float16:
				sizeofChannel = 2;
				*internalformat = DXGI_FORMAT_R16G16B16A16_FLOAT;
				break;
			case ImPixelType_Float32:
				sizeofChannel = 4;
				*internalformat = DXGI_FORMAT_R32G32B32A32_FLOAT;
				break;
			}
			break;
		}
		return sizeofChannel;
	}

	// Helper functions for frame management (internal to this file)
	namespace
	{
		void WaitForLastSubmittedFrame()
		{
			PlatformDataImpl::FrameContext* frameCtx = &PlatformData.pFrameContext[PlatformData.uFrameIndex % PlatformData.NUM_FRAMES_IN_FLIGHT];

			UINT64 fenceValue = frameCtx->FenceValue;
			if ( fenceValue == 0 )
				return; // No fence was signaled

			frameCtx->FenceValue = 0;
			if ( PlatformData.pFence->GetCompletedValue() >= fenceValue )
				return;

			PlatformData.pFence->SetEventOnCompletion( fenceValue, PlatformData.pFenceEvent );
			WaitForSingleObject( PlatformData.pFenceEvent, INFINITE );
		}

		PlatformDataImpl::FrameContext* WaitForNextFrameResources()
		{
			UINT nextFrameIndex = PlatformData.uFrameIndex + 1;
			PlatformData.uFrameIndex = nextFrameIndex;

			HANDLE waitableObjects[] = { PlatformData.pSwapChainWaitableObject, nullptr };
			DWORD numWaitableObjects = 1;

			PlatformDataImpl::FrameContext* frameCtx = &PlatformData.pFrameContext[nextFrameIndex % PlatformData.NUM_FRAMES_IN_FLIGHT];
			UINT64 fenceValue = frameCtx->FenceValue;
			if ( fenceValue != 0 ) // means fence was signaled
			{
				frameCtx->FenceValue = 0;
				PlatformData.pFence->SetEventOnCompletion( fenceValue, PlatformData.pFenceEvent );
				waitableObjects[1] = PlatformData.pFenceEvent;
				numWaitableObjects = 2;
			}

			WaitForMultipleObjects( numWaitableObjects, waitableObjects, TRUE, INFINITE );

			return frameCtx;
		}

		void CreateRenderTarget()
		{
			for ( UINT i = 0; i < PlatformData.NUM_BACK_BUFFERS; i++ )
			{
				ID3D12Resource* pBackBuffer = nullptr;
				PlatformData.pSwapChain->GetBuffer( i, IID_PPV_ARGS( &pBackBuffer ) );
				PlatformData.pD3DDevice->CreateRenderTargetView( pBackBuffer, nullptr, PlatformData.pMainRenderTargetDescriptor[i] );
				PlatformData.pMainRenderTargetResource[i] = pBackBuffer;
			}
		}

		void CleanupRenderTarget()
		{
			WaitForLastSubmittedFrame();

			for ( UINT i = 0; i < PlatformData.NUM_BACK_BUFFERS; i++ )
			{
				if ( PlatformData.pMainRenderTargetResource[i] )
				{
					PlatformData.pMainRenderTargetResource[i]->Release();
					PlatformData.pMainRenderTargetResource[i] = nullptr;
				}
			}
		}
	}

	// Implementation of Backend namespace for DX12
	namespace Backend
	{
		bool InitGfxAPI()
		{
			// Setup swap chain
			DXGI_SWAP_CHAIN_DESC1 sd;
			ZeroMemory( &sd, sizeof( sd ) );
			sd.BufferCount = PlatformData.NUM_BACK_BUFFERS;
			sd.Width = 0;
			sd.Height = 0;
			sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
			sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			sd.SampleDesc.Count = 1;
			sd.SampleDesc.Quality = 0;
			sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
			sd.Scaling = DXGI_SCALING_STRETCH;
			sd.Stereo = FALSE;

			// Enable debug layer
#ifdef _DEBUG
			ID3D12Debug* pdx12Debug = nullptr;
			if ( SUCCEEDED( D3D12GetDebugInterface( IID_PPV_ARGS( &pdx12Debug ) ) ) )
			{
				pdx12Debug->EnableDebugLayer();
				pdx12Debug->Release();
			}
#endif

			// Create device
			D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
			HRESULT hr = D3D12CreateDevice( nullptr, featureLevel, IID_PPV_ARGS( &PlatformData.pD3DDevice ) );
			if ( FAILED( hr ) )
				return false;

			// Create RTV descriptor heap
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
					PlatformData.pMainRenderTargetDescriptor[i] = rtvHandle;
					rtvHandle.ptr += rtvDescriptorSize;
				}
			}

			// Create SRV descriptor heap
			{
				D3D12_DESCRIPTOR_HEAP_DESC desc = {};
				desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
				desc.NumDescriptors = PlatformDataImpl::NUM_SRV_DESCRIPTORS;
				desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
				desc.NodeMask = 1;
				if ( PlatformData.pD3DDevice->CreateDescriptorHeap( &desc, IID_PPV_ARGS( &PlatformData.pD3DSRVDescHeap ) ) != S_OK )
					return false;
			}

			// Create command queue
			{
				D3D12_COMMAND_QUEUE_DESC desc = {};
				desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
				desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
				desc.NodeMask = 1;
				if ( PlatformData.pD3DDevice->CreateCommandQueue( &desc, IID_PPV_ARGS( &PlatformData.pD3DCommandQueue ) ) != S_OK )
					return false;
			}

			// Create command allocators
			for ( UINT i = 0; i < PlatformData.NUM_FRAMES_IN_FLIGHT; i++ )
			{
				if ( PlatformData.pD3DDevice->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( &PlatformData.pFrameContext[i].CommandAllocator ) ) != S_OK )
					return false;
			}

			// Create command list
			if ( PlatformData.pD3DDevice->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_DIRECT, PlatformData.pFrameContext[0].CommandAllocator, nullptr, IID_PPV_ARGS( &PlatformData.pD3DCommandList ) ) != S_OK ||
				 PlatformData.pD3DCommandList->Close() != S_OK )
				return false;

			// Create fence
			if ( PlatformData.pD3DDevice->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( &PlatformData.pFence ) ) != S_OK )
				return false;

			PlatformData.pFenceEvent = CreateEvent( nullptr, FALSE, FALSE, nullptr );
			if ( PlatformData.pFenceEvent == nullptr )
				return false;

			// Create swap chain
			{
				IDXGIFactory4* dxgiFactory = nullptr;
				IDXGISwapChain1* swapChain1 = nullptr;
				if ( CreateDXGIFactory1( IID_PPV_ARGS( &dxgiFactory ) ) != S_OK )
					return false;
				if ( dxgiFactory->CreateSwapChainForHwnd( PlatformData.pD3DCommandQueue, PlatformData.pHandle, &sd, nullptr, nullptr, &swapChain1 ) != S_OK )
				{
					dxgiFactory->Release();
					return false;
				}
				if ( swapChain1->QueryInterface( IID_PPV_ARGS( &PlatformData.pSwapChain ) ) != S_OK )
				{
					swapChain1->Release();
					dxgiFactory->Release();
					return false;
				}
				swapChain1->Release();
				dxgiFactory->Release();
				PlatformData.pSwapChain->SetMaximumFrameLatency( PlatformData.NUM_BACK_BUFFERS );
				PlatformData.pSwapChainWaitableObject = PlatformData.pSwapChain->GetFrameLatencyWaitableObject();
			}

			// Create render targets
			CreateRenderTarget();
			return true;
		}

		bool InitGfx()
		{
			return ImGui_ImplDX12_Init( PlatformData.pD3DDevice, PlatformData.NUM_FRAMES_IN_FLIGHT,
										DXGI_FORMAT_R8G8B8A8_UNORM, PlatformData.pD3DSRVDescHeap,
										PlatformData.pD3DSRVDescHeap->GetCPUDescriptorHandleForHeapStart(),
										PlatformData.pD3DSRVDescHeap->GetGPUDescriptorHandleForHeapStart() );
		}

		void ShutdownGfxAPI()
		{
			CleanupRenderTarget();

			if ( PlatformData.pSwapChain )
			{
				PlatformData.pSwapChain->SetFullscreenState( false, nullptr );
				PlatformData.pSwapChain->Release();
				PlatformData.pSwapChain = nullptr;
			}

			if ( PlatformData.pSwapChainWaitableObject != nullptr )
				CloseHandle( PlatformData.pSwapChainWaitableObject );

			for ( UINT i = 0; i < PlatformData.NUM_FRAMES_IN_FLIGHT; i++ )
			{
				if ( PlatformData.pFrameContext[i].CommandAllocator )
				{
					PlatformData.pFrameContext[i].CommandAllocator->Release();
					PlatformData.pFrameContext[i].CommandAllocator = nullptr;
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

			ImGui_ImplDX12_Shutdown();
		}

		void ShutdownPostGfxAPI()
		{
			// Nothing additional needed for DX12
		}

		void GfxAPINewFrame()
		{
			ImGui_ImplDX12_NewFrame();
		}

		bool GfxAPIClear( ImVec4 const vClearColor )
		{
			// DX12 doesn't do clear separately, it's done in GfxAPIRender
			return true;
		}

		bool GfxAPIRender( ImVec4 const vClearColor )
		{
			ImGui::Render();

			PlatformDataImpl::FrameContext* frameCtx = WaitForNextFrameResources();
			UINT backBufferIdx = PlatformData.pSwapChain->GetCurrentBackBufferIndex();
			frameCtx->CommandAllocator->Reset();

			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource = PlatformData.pMainRenderTargetResource[backBufferIdx];
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

			PlatformData.pD3DCommandList->Reset( frameCtx->CommandAllocator, nullptr );
			PlatformData.pD3DCommandList->ResourceBarrier( 1, &barrier );

			// Clear and render
			const float clear_color_with_alpha[4] = {
				vClearColor.x * vClearColor.w,
				vClearColor.y * vClearColor.w,
				vClearColor.z * vClearColor.w,
				vClearColor.w
			};
			PlatformData.pD3DCommandList->ClearRenderTargetView( PlatformData.pMainRenderTargetDescriptor[backBufferIdx], clear_color_with_alpha, 0, nullptr );
			PlatformData.pD3DCommandList->OMSetRenderTargets( 1, &PlatformData.pMainRenderTargetDescriptor[backBufferIdx], FALSE, nullptr );
			PlatformData.pD3DCommandList->SetDescriptorHeaps( 1, &PlatformData.pD3DSRVDescHeap );
			ImGui_ImplDX12_RenderDrawData( ImGui::GetDrawData(), PlatformData.pD3DCommandList );

			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			PlatformData.pD3DCommandList->ResourceBarrier( 1, &barrier );
			PlatformData.pD3DCommandList->Close();

			PlatformData.pD3DCommandQueue->ExecuteCommandLists( 1, ( ID3D12CommandList* const* )&PlatformData.pD3DCommandList );

			return true;
		}

		bool GfxAPISwapBuffer()
		{
			HRESULT hr = PlatformData.pSwapChain->Present( 1, 0 );
			PlatformData.bSwapChainOccluded = ( hr == DXGI_STATUS_OCCLUDED );

			UINT64 fenceValue = PlatformData.uFenceLastSignaledValue + 1;
			PlatformData.pD3DCommandQueue->Signal( PlatformData.pFence, fenceValue );
			PlatformData.uFenceLastSignaledValue = fenceValue;

			PlatformDataImpl::FrameContext* frameCtx = &PlatformData.pFrameContext[PlatformData.uFrameIndex % PlatformData.NUM_FRAMES_IN_FLIGHT];
			frameCtx->FenceValue = fenceValue;

			return true;
		}

		bool GfxCheck()
		{
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
			if ( PlatformData.bSwapChainOccluded && PlatformData.pSwapChain->Present( 0, DXGI_PRESENT_TEST ) == DXGI_STATUS_OCCLUDED )
			{
				::Sleep( 10 );
				return false;
			}
			PlatformData.bSwapChainOccluded = false;
#endif
			return true;
		}

		void GfxViewportPre() {}
		void GfxViewportPost() {}

		ImTextureID CreateTexture2D( char* pData, ImU32 const uWidth, ImU32 const uHeight, ImImageDesc const& oImgDesc )
		{
			DXGI_FORMAT internalformat;
			int iSizeOf = ImPixelTypeChannelToDx11_12( &internalformat, oImgDesc.eType, oImgDesc.eChannel );

			D3D12_HEAP_PROPERTIES props;
			memset( &props, 0, sizeof( D3D12_HEAP_PROPERTIES ) );
			props.Type = D3D12_HEAP_TYPE_DEFAULT;
			props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

			D3D12_RESOURCE_DESC desc;
			ZeroMemory( &desc, sizeof( desc ) );
			desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			desc.Alignment = 0;
			desc.Width = uWidth;
			desc.Height = uHeight;
			desc.DepthOrArraySize = 1;
			desc.MipLevels = 1;
			desc.Format = internalformat;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			desc.Flags = D3D12_RESOURCE_FLAG_NONE;

			ID3D12Resource* pTexture = nullptr;
			HRESULT hr = PlatformData.pD3DDevice->CreateCommittedResource(
				&props, D3D12_HEAP_FLAG_NONE, &desc,
				D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS( &pTexture ) );
			if ( FAILED( hr ) )
				return (ImTextureID)nullptr;

			// Create upload buffer
			UINT uploadPitch = ( uWidth * ((int)oImgDesc.eChannel) * iSizeOf + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u ) & ~( D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u );
			UINT uploadSize = uHeight * uploadPitch;
			desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			desc.Alignment = 0;
			desc.Width = uploadSize;
			desc.Height = 1;
			desc.DepthOrArraySize = 1;
			desc.MipLevels = 1;
			desc.Format = DXGI_FORMAT_UNKNOWN;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			desc.Flags = D3D12_RESOURCE_FLAG_NONE;

			props.Type = D3D12_HEAP_TYPE_UPLOAD;
			props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

			ID3D12Resource* uploadBuffer = nullptr;
			hr = PlatformData.pD3DDevice->CreateCommittedResource(
				&props, D3D12_HEAP_FLAG_NONE, &desc,
				D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS( &uploadBuffer ) );
			if ( FAILED( hr ) )
			{
				if ( pTexture ) pTexture->Release();
				return (ImTextureID)nullptr;
			}

			// Map and copy data
			void* mapped = nullptr;
			D3D12_RANGE range = { 0, uploadSize };
			hr = uploadBuffer->Map( 0, &range, &mapped );
			if ( FAILED( hr ) )
			{
				if ( pTexture ) pTexture->Release();
				if ( uploadBuffer ) uploadBuffer->Release();
				return (ImTextureID)nullptr;
			}

			for ( UINT y = 0; y < uHeight; y++ )
			{
				memcpy( ( void* )( ( uintptr_t )mapped + y * uploadPitch ),
						pData + y * uWidth * ((int)oImgDesc.eChannel) * iSizeOf,
						uWidth * ((int)oImgDesc.eChannel) * iSizeOf );
			}

			D3D12_RANGE writtenRange = { 0, uploadSize };
			uploadBuffer->Unmap( 0, &writtenRange );

			// Execute upload command (simplified - in production would use a proper command buffer)
			// TODO: This needs proper command buffer management for production
			uploadBuffer->Release();

			// Allocate descriptor from heap
			int descriptor_index = PlatformData.uNextSRVDescriptor++;
			if ( descriptor_index >= PlatformDataImpl::NUM_SRV_DESCRIPTORS )
			{
				pTexture->Release();
				return (ImTextureID)nullptr;
			}

			UINT handle_increment = PlatformData.pD3DDevice->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = PlatformData.pD3DSRVDescHeap->GetCPUDescriptorHandleForHeapStart();
			cpu_handle.ptr += ( handle_increment * descriptor_index );
			D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = PlatformData.pD3DSRVDescHeap->GetGPUDescriptorHandleForHeapStart();
			gpu_handle.ptr += ( handle_increment * descriptor_index );

			// Create SRV
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
			ZeroMemory( &srvDesc, sizeof( srvDesc ) );
			srvDesc.Format = internalformat;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MipLevels = 1;

			PlatformData.pD3DDevice->CreateShaderResourceView( pTexture, &srvDesc, cpu_handle );

			// Store resource pointer for cleanup
			PlatformData.pSRVResources[descriptor_index] = pTexture;

			return ( ImTextureID )gpu_handle.ptr;
		}

		void ReleaseTexture2D( ImTextureID id )
		{
			// Find and release the resource
			// Note: This is simplified - production code should track descriptors better
			if ( id )
			{
				// TODO: Proper resource tracking and release
			}
		}

		ImDrawShader CreateShader( char const* vs_source, char const* ps_source,
									int sizeof_in_bytes_vs_constants, void* init_vs_data_constant,
									int sizeof_in_bytes_ps_constants, void* init_ps_data_constant )
		{
			// DX12 shader creation is complex - would need root signature, PSO, etc.
			// TODO: Implement when needed
			ImDrawShader shader = {};
			return shader;
		}

		void ReleaseShader( ImDrawShader& shader ) {}
		void BeginCustomShader( ImDrawList* draw, ImDrawShader& shader ) {}
		void UpdateCustomPixelShaderConstants( ImDrawShader& shader, void* ptr_to_constants ) {}
		void UpdateCustomVertexShaderConstants( ImDrawShader& shader, void* ptr_to_constants ) {}
		void EndCustomShader( ImDrawList* draw ) {}

		void CreateVertexBuffer( ImVertexBuffer*& buffer, int sizeof_vertex_buffer, int vertices_count ) {}
		void CreateVertexBufferAndGrow( ImVertexBuffer** buffer, int sizeof_vertex_buffer, int vertices_count, int growth_size ) {}
		void UpdateVertexBuffer( ImVertexBuffer** buffer, int sizeof_vertex_buffer, int vertices_count, void* cpu_data ) {}
		void ReleaseVertexBuffer( ImVertexBuffer** buffer ) {}

		void CreateIndexBuffer( ImIndexBuffer*& buffer, int indices_count ) {}
		void CreateIndexBufferAndGrow( ImIndexBuffer** buffer, int indices_count, int growth_size ) {}
		void UpdateIndexBuffer( ImIndexBuffer** buffer, int indices_count, void* cpu_data ) {}
		void ReleaseIndexBuffer( ImIndexBuffer** buffer ) {}

		bool WindowResize()
		{
			CleanupRenderTarget();
			HRESULT result = PlatformData.pSwapChain->ResizeBuffers( 0, 0, 0, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT );
			assert( SUCCEEDED( result ) && "Failed to resize swapchain." );
			CreateRenderTarget();
			return true;
		}
	}
}
