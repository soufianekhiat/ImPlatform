// ImPlatform DirectX 11 Backend Implementation - Pure C Style
// This file implements ImPlatform::Backend:: namespace functions for DX11
// The linker resolves these at link-time based on which .cpp is included

#include "ImPlatformBackend.h"
#include "ImPlatform.h"
#include <imgui.h>
#include <imgui_internal.h>

// DX11 includes
#include <backends/imgui_impl_dx11.h>
// Note: imgui_impl_dx11.cpp is included by main ImPlatform.cpp, don't include it again
#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment( lib, "d3d11.lib" )
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
			case ImPixelType_Float16:
				sizeofChannel = 2;
				*internalformat = DXGI_FORMAT_R16_FLOAT;
				break;
			case ImPixelType_Float32:
				sizeofChannel = 4;
				*internalformat = DXGI_FORMAT_R32_FLOAT;
				break;
			default:
				fprintf( stderr, "ImPixelTypeChannelToDx11_12 ImType unsupported for Single channel image.\n" );
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
			default:
				fprintf( stderr, "ImPixelTypeChannelToDx11_12 ImType unsupported for RG channel image.\n" );
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
			default:
				fprintf( stderr, "ImPixelTypeChannelToDx11_12 ImType unsupported for RGB channel image.\n" );
				break;
			}
			break;
		case ImPixelChannel_RGBA:
			switch ( eType )
			{
			case ImPixelType_UInt8:
				sizeofChannel = 1;
				*internalformat = DXGI_FORMAT_R8G8B8A8_UNORM;
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
			default:
				fprintf( stderr, "ImPixelTypeChannelToDx11_12 ImType unsupported for RGBA channel image.\n" );
				break;
			}
			break;
		default:
			fprintf( stderr, "ImPixelTypeChannelToDx11_12 eChannel unsupported {IM_R, IM_RG, IM_RGB, IM_RGBA}\n" );
			break;
		}

		return sizeofChannel;
	}

	// Implementation of Backend namespace for DX11
	namespace Backend
	{
		bool InitGfxAPI()
		{
			// Create D3D11 device and swap chain
			DXGI_SWAP_CHAIN_DESC sd;
			ZeroMemory( &sd, sizeof( sd ) );
			sd.BufferCount = 2;
			sd.BufferDesc.Width = 0;
			sd.BufferDesc.Height = 0;
			sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			sd.BufferDesc.RefreshRate.Numerator = 60;
			sd.BufferDesc.RefreshRate.Denominator = 1;
			sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
			sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			sd.OutputWindow = PlatformData.pHandle;
			sd.SampleDesc.Count = 1;
			sd.SampleDesc.Quality = 0;
			sd.Windowed = TRUE;
			sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

			UINT createDeviceFlags = 0;
#ifdef _DEBUG
			createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

			D3D_FEATURE_LEVEL featureLevel;
			const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
			HRESULT res = D3D11CreateDeviceAndSwapChain(
				nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
				createDeviceFlags, featureLevelArray, 2,
				D3D11_SDK_VERSION, &sd,
				&PlatformData.pSwapChain,
				&PlatformData.pD3DDevice,
				&featureLevel,
				&PlatformData.pD3DDeviceContext );

			if ( res == DXGI_ERROR_UNSUPPORTED )
			{
				res = D3D11CreateDeviceAndSwapChain(
					nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
					createDeviceFlags, featureLevelArray, 2,
					D3D11_SDK_VERSION, &sd,
					&PlatformData.pSwapChain,
					&PlatformData.pD3DDevice,
					&featureLevel,
					&PlatformData.pD3DDeviceContext );
			}

			if ( res != S_OK )
				return false;

			// Create render target
			ID3D11Texture2D* pBackBuffer;
			PlatformData.pSwapChain->GetBuffer( 0, IID_PPV_ARGS( &pBackBuffer ) );
			PlatformData.pD3DDevice->CreateRenderTargetView( pBackBuffer, nullptr, &PlatformData.pMainRenderTargetView );
			pBackBuffer->Release();

			return true;
		}

		bool InitGfx()
		{
			return ImGui_ImplDX11_Init( PlatformData.pD3DDevice, PlatformData.pD3DDeviceContext );
		}

		void ShutdownGfxAPI()
		{
			if ( PlatformData.pMainRenderTargetView )
			{
				PlatformData.pMainRenderTargetView->Release();
				PlatformData.pMainRenderTargetView = nullptr;
			}
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
			}

			ImGui_ImplDX11_Shutdown();
		}

		void ShutdownPostGfxAPI()
		{
			// Nothing additional for DX11
		}

		void GfxAPINewFrame()
		{
			ImGui_ImplDX11_NewFrame();
		}

		bool GfxAPIClear( ImVec4 const vClearColor )
		{
			const float clear_color_with_alpha[4] = {
				vClearColor.x * vClearColor.w,
				vClearColor.y * vClearColor.w,
				vClearColor.z * vClearColor.w,
				vClearColor.w
			};
			PlatformData.pD3DDeviceContext->OMSetRenderTargets( 1, &PlatformData.pMainRenderTargetView, nullptr );
			PlatformData.pD3DDeviceContext->ClearRenderTargetView( PlatformData.pMainRenderTargetView, clear_color_with_alpha );
			return true;
		}

		bool GfxAPIRender( ImVec4 const vClearColor )
		{
			ImGui::Render();
			ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData() );
			return true;
		}

		bool GfxAPISwapBuffer()
		{
			HRESULT hr = PlatformData.pSwapChain->Present( 1, 0 );
			PlatformData.bSwapChainOccluded = ( hr == DXGI_STATUS_OCCLUDED );
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
			ID3D11ShaderResourceView* out_srv = NULL;

			DXGI_FORMAT internalformat;
			int iSizeOf = ImPixelTypeChannelToDx11_12( &internalformat, oImgDesc.eType, oImgDesc.eChannel );

			// Create texture
			D3D11_TEXTURE2D_DESC desc;
			ZeroMemory( &desc, sizeof( desc ) );
			desc.Width = uWidth;
			desc.Height = uHeight;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.Format = internalformat;
			desc.SampleDesc.Count = 1;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			desc.CPUAccessFlags = 0;

			ID3D11Texture2D* pTexture = NULL;
			D3D11_SUBRESOURCE_DATA subResource;
			subResource.pSysMem = pData;
			subResource.SysMemPitch = desc.Width * int( oImgDesc.eChannel ) * iSizeOf;
			subResource.SysMemSlicePitch = 0;
			HRESULT hr = PlatformData.pD3DDevice->CreateTexture2D( &desc, &subResource, &pTexture );
			if ( FAILED( hr ) )
				return NULL;

			// Create texture view
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
			ZeroMemory( &srvDesc, sizeof( srvDesc ) );
			srvDesc.Format = internalformat;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = desc.MipLevels;
			srvDesc.Texture2D.MostDetailedMip = 0;
			hr = PlatformData.pD3DDevice->CreateShaderResourceView( pTexture, &srvDesc, &out_srv );
			pTexture->Release();

			if ( FAILED( hr ) )
				return NULL;

			return ( ImTextureID )( out_srv );
		}

		void ReleaseTexture2D( ImTextureID id )
		{
			if ( id )
			{
				ID3D11ShaderResourceView* srv = ( ID3D11ShaderResourceView* )id;
				srv->Release();
			}
		}

		ImDrawShader CreateShader( char const* vs_source, char const* ps_source,
									int sizeof_in_bytes_vs_constants, void* init_vs_data_constant,
									int sizeof_in_bytes_ps_constants, void* init_ps_data_constant )
		{
			ImDrawShader shader = {};
			shader.sizeof_in_bytes_vs_constants = sizeof_in_bytes_vs_constants;
			shader.sizeof_in_bytes_ps_constants = sizeof_in_bytes_ps_constants;
			shader.is_cpu_vs_data_dirty = false;
			shader.is_cpu_ps_data_dirty = false;

			// Compile vertex shader
			ID3DBlob* vsBlob = nullptr;
			ID3DBlob* errorBlob = nullptr;
			HRESULT hr = D3DCompile( vs_source, strlen( vs_source ), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, &vsBlob, &errorBlob );
			if ( FAILED( hr ) )
			{
				if ( errorBlob )
				{
					fprintf( stderr, "Vertex shader compilation failed: %s\n", ( char* )errorBlob->GetBufferPointer() );
					errorBlob->Release();
				}
				return shader;
			}

			hr = PlatformData.pD3DDevice->CreateVertexShader( vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, ( ID3D11VertexShader** )&shader.vs );
			vsBlob->Release();
			if ( FAILED( hr ) )
				return shader;

			// Compile pixel shader
			hr = D3DCompile( ps_source, strlen( ps_source ), nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, &vsBlob, &errorBlob );
			if ( FAILED( hr ) )
			{
				if ( errorBlob )
				{
					fprintf( stderr, "Pixel shader compilation failed: %s\n", ( char* )errorBlob->GetBufferPointer() );
					errorBlob->Release();
				}
				if ( shader.vs ) ( ( ID3D11VertexShader* )shader.vs )->Release();
				shader.vs = nullptr;
				return shader;
			}

			hr = PlatformData.pD3DDevice->CreatePixelShader( vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, ( ID3D11PixelShader** )&shader.ps );
			vsBlob->Release();
			if ( FAILED( hr ) )
			{
				if ( shader.vs ) ( ( ID3D11VertexShader* )shader.vs )->Release();
				shader.vs = nullptr;
				return shader;
			}

			// Create constant buffers
			if ( sizeof_in_bytes_vs_constants > 0 )
			{
				D3D11_BUFFER_DESC cbd = {};
				cbd.ByteWidth = sizeof_in_bytes_vs_constants;
				cbd.Usage = D3D11_USAGE_DYNAMIC;
				cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
				cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
				hr = PlatformData.pD3DDevice->CreateBuffer( &cbd, nullptr, ( ID3D11Buffer** )&shader.vs_cst );
				if ( FAILED( hr ) )
				{
					if ( shader.vs ) ( ( ID3D11VertexShader* )shader.vs )->Release();
					if ( shader.ps ) ( ( ID3D11PixelShader* )shader.ps )->Release();
					shader.vs = shader.ps = nullptr;
					return shader;
				}

				shader.cpu_vs_data = IM_ALLOC( sizeof_in_bytes_vs_constants );
				if ( init_vs_data_constant )
					memcpy( shader.cpu_vs_data, init_vs_data_constant, sizeof_in_bytes_vs_constants );
			}

			if ( sizeof_in_bytes_ps_constants > 0 )
			{
				D3D11_BUFFER_DESC cbd = {};
				cbd.ByteWidth = sizeof_in_bytes_ps_constants;
				cbd.Usage = D3D11_USAGE_DYNAMIC;
				cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
				cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
				hr = PlatformData.pD3DDevice->CreateBuffer( &cbd, nullptr, ( ID3D11Buffer** )&shader.ps_cst );
				if ( FAILED( hr ) )
				{
					if ( shader.vs ) ( ( ID3D11VertexShader* )shader.vs )->Release();
					if ( shader.ps ) ( ( ID3D11PixelShader* )shader.ps )->Release();
					if ( shader.vs_cst ) ( ( ID3D11Buffer* )shader.vs_cst )->Release();
					shader.vs = shader.ps = shader.vs_cst = nullptr;
					if ( shader.cpu_vs_data ) IM_FREE( shader.cpu_vs_data );
					return shader;
				}

				shader.cpu_ps_data = IM_ALLOC( sizeof_in_bytes_ps_constants );
				if ( init_ps_data_constant )
					memcpy( shader.cpu_ps_data, init_ps_data_constant, sizeof_in_bytes_ps_constants );
			}

			return shader;
		}

		void ReleaseShader( ImDrawShader& shader )
		{
			if ( shader.vs ) ( ( ID3D11VertexShader* )shader.vs )->Release();
			if ( shader.ps ) ( ( ID3D11PixelShader* )shader.ps )->Release();
			if ( shader.vs_cst ) ( ( ID3D11Buffer* )shader.vs_cst )->Release();
			if ( shader.ps_cst ) ( ( ID3D11Buffer* )shader.ps_cst )->Release();
			if ( shader.cpu_vs_data ) IM_FREE( shader.cpu_vs_data );
			if ( shader.cpu_ps_data ) IM_FREE( shader.cpu_ps_data );
			memset( &shader, 0, sizeof( shader ) );
		}

		void BeginCustomShader( ImDrawList* draw, ImDrawShader& shader )
		{
			draw->AddCallback( ImDrawCallback_ResetRenderState, nullptr );
			draw->AddCallback( []( const ImDrawList* parent_list, const ImDrawCmd* cmd ) {
				// Custom shader callback implementation would go here
			}, &shader );
		}

		void UpdateCustomPixelShaderConstants( ImDrawShader& shader, void* ptr_to_constants )
		{
			if ( shader.sizeof_in_bytes_ps_constants > 0 && ptr_to_constants )
			{
				memcpy( shader.cpu_ps_data, ptr_to_constants, shader.sizeof_in_bytes_ps_constants );
				shader.is_cpu_ps_data_dirty = true;
			}
		}

		void UpdateCustomVertexShaderConstants( ImDrawShader& shader, void* ptr_to_constants )
		{
			if ( shader.sizeof_in_bytes_vs_constants > 0 && ptr_to_constants )
			{
				memcpy( shader.cpu_vs_data, ptr_to_constants, shader.sizeof_in_bytes_vs_constants );
				shader.is_cpu_vs_data_dirty = true;
			}
		}

		void EndCustomShader( ImDrawList* draw )
		{
			draw->AddCallback( ImDrawCallback_ResetRenderState, nullptr );
		}

		void CreateVertexBuffer( ImVertexBuffer*& buffer, int sizeof_vertex_buffer, int vertices_count )
		{
			buffer = new ImVertexBuffer();
			buffer->vertices_count = vertices_count;
			buffer->sizeof_each_struct_in_bytes = sizeof_vertex_buffer;

			D3D11_BUFFER_DESC desc = {};
			desc.ByteWidth = vertices_count * sizeof_vertex_buffer;
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			HRESULT hr = PlatformData.pD3DDevice->CreateBuffer( &desc, nullptr, ( ID3D11Buffer** )&buffer->gpu_buffer );
			if ( FAILED( hr ) )
			{
				delete buffer;
				buffer = nullptr;
			}
		}

		void CreateVertexBufferAndGrow( ImVertexBuffer** buffer, int sizeof_vertex_buffer, int vertices_count, int growth_size )
		{
			if ( *buffer == nullptr || ( *buffer )->vertices_count < vertices_count )
			{
				if ( *buffer ) ReleaseVertexBuffer( buffer );
				int new_count = vertices_count + growth_size;
				CreateVertexBuffer( *buffer, sizeof_vertex_buffer, new_count );
			}
		}

		void UpdateVertexBuffer( ImVertexBuffer** buffer, int sizeof_vertex_buffer, int vertices_count, void* cpu_data )
		{
			CreateVertexBufferAndGrow( buffer, sizeof_vertex_buffer, vertices_count, 1000 );
			if ( *buffer && ( *buffer )->gpu_buffer )
			{
				D3D11_MAPPED_SUBRESOURCE mapped;
				HRESULT hr = PlatformData.pD3DDeviceContext->Map( ( ID3D11Buffer* )( *buffer )->gpu_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped );
				if ( SUCCEEDED( hr ) )
				{
					memcpy( mapped.pData, cpu_data, vertices_count * sizeof_vertex_buffer );
					PlatformData.pD3DDeviceContext->Unmap( ( ID3D11Buffer* )( *buffer )->gpu_buffer, 0 );
				}
			}
		}

		void ReleaseVertexBuffer( ImVertexBuffer** buffer )
		{
			if ( *buffer )
			{
				if ( ( *buffer )->gpu_buffer )
					( ( ID3D11Buffer* )( *buffer )->gpu_buffer )->Release();
				delete *buffer;
				*buffer = nullptr;
			}
		}

		void CreateIndexBuffer( ImIndexBuffer*& buffer, int indices_count )
		{
			buffer = new ImIndexBuffer();
			buffer->indices_count = indices_count;
			buffer->sizeof_each_index = sizeof( ImDrawIdx );

			D3D11_BUFFER_DESC desc = {};
			desc.ByteWidth = indices_count * sizeof( ImDrawIdx );
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			HRESULT hr = PlatformData.pD3DDevice->CreateBuffer( &desc, nullptr, ( ID3D11Buffer** )&buffer->gpu_buffer );
			if ( FAILED( hr ) )
			{
				delete buffer;
				buffer = nullptr;
			}
		}

		void CreateIndexBufferAndGrow( ImIndexBuffer** buffer, int indices_count, int growth_size )
		{
			if ( *buffer == nullptr || ( *buffer )->indices_count < indices_count )
			{
				if ( *buffer ) ReleaseIndexBuffer( buffer );
				int new_count = indices_count + growth_size;
				CreateIndexBuffer( *buffer, new_count );
			}
		}

		void UpdateIndexBuffer( ImIndexBuffer** buffer, int indices_count, void* cpu_data )
		{
			CreateIndexBufferAndGrow( buffer, indices_count, 1000 );
			if ( *buffer && ( *buffer )->gpu_buffer )
			{
				D3D11_MAPPED_SUBRESOURCE mapped;
				HRESULT hr = PlatformData.pD3DDeviceContext->Map( ( ID3D11Buffer* )( *buffer )->gpu_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped );
				if ( SUCCEEDED( hr ) )
				{
					memcpy( mapped.pData, cpu_data, indices_count * sizeof( ImDrawIdx ) );
					PlatformData.pD3DDeviceContext->Unmap( ( ID3D11Buffer* )( *buffer )->gpu_buffer, 0 );
				}
			}
		}

		void ReleaseIndexBuffer( ImIndexBuffer** buffer )
		{
			if ( *buffer )
			{
				if ( ( *buffer )->gpu_buffer )
					( ( ID3D11Buffer* )( *buffer )->gpu_buffer )->Release();
				delete *buffer;
				*buffer = nullptr;
			}
		}

		bool WindowResize()
		{
			if ( PlatformData.uResizeWidth != 0 && PlatformData.uResizeHeight != 0 )
			{
				if ( PlatformData.pMainRenderTargetView )
				{
					PlatformData.pMainRenderTargetView->Release();
					PlatformData.pMainRenderTargetView = nullptr;
				}

				PlatformData.pSwapChain->ResizeBuffers( 0, PlatformData.uResizeWidth, PlatformData.uResizeHeight, DXGI_FORMAT_UNKNOWN, 0 );

				ID3D11Texture2D* pBackBuffer;
				PlatformData.pSwapChain->GetBuffer( 0, IID_PPV_ARGS( &pBackBuffer ) );
				PlatformData.pD3DDevice->CreateRenderTargetView( pBackBuffer, nullptr, &PlatformData.pMainRenderTargetView );
				pBackBuffer->Release();

				PlatformData.uResizeWidth = PlatformData.uResizeHeight = 0;
			}
			return true;
		}
	}
}
