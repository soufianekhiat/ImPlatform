// Note: This file is included by ImPlatform.h when IM_PLATFORM_IMPLEMENTATION is defined
// All backend-specific includes are handled in ImPlatform.h
// This file contains only common implementation code that is not backend-specific

namespace ImPlatform
{
	PlatformDataImpl PlatformData;

	ImTextureID	CreateTexture2D( char* pData, ImU32 const uWidth, ImU32 const uHeight, ImImageDesc const& oImgDesc )
	{
		return Backend::CreateTexture2D( pData, uWidth, uHeight, oImgDesc );
	}

	void		ReleaseTexture2D( ImTextureID id )
	{
		Backend::ReleaseTexture2D( id );
	}

#ifdef IM_SUPPORT_CUSTOM_SHADER
	void*	InternalCreateDynamicConstantBuffer( int sizeof_in_bytes_constants,
										  void* init_data_constant )
	{
#if (IM_CURRENT_GFX == IM_GFX_OPENGL3)

		return NULL;

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)

		ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();

		ID3D11Buffer* constant_buffer = NULL;
		if ( sizeof_in_bytes_constants > 0 )
		{
			D3D11_BUFFER_DESC desc;
			desc.ByteWidth = sizeof_in_bytes_constants;
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			desc.MiscFlags = 0;
			if ( init_data_constant != NULL )
			{
				D3D11_SUBRESOURCE_DATA init = D3D11_SUBRESOURCE_DATA{ 0 };
				init.pSysMem = init_data_constant;
				HRESULT hr = bd->pd3dDevice->CreateBuffer( &desc, &init, &constant_buffer );

				IM_UNUSED( hr );

#if 0
				if ( hr != S_OK )
				{
					_com_error err( hr );
					LPCTSTR errMsg = err.ErrorMessage();
					OutputDebugStringA( errMsg );
				}
#endif
			}
			else
			{
				bd->pd3dDevice->CreateBuffer( &desc, NULL, &constant_buffer );
			}
		}

		return ( void* )constant_buffer;

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)

		ID3D12Resource* constant_buffer = NULL;
		if ( sizeof_in_bytes_constants > 0 )
		{
			// Align to 256 bytes (D3D12 constant buffer requirement)
			UINT aligned_size = ( sizeof_in_bytes_constants + 255 ) & ~255;

			D3D12_HEAP_PROPERTIES heap_props = {};
			heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;
			heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

			D3D12_RESOURCE_DESC resource_desc = {};
			resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resource_desc.Alignment = 0;
			resource_desc.Width = aligned_size;
			resource_desc.Height = 1;
			resource_desc.DepthOrArraySize = 1;
			resource_desc.MipLevels = 1;
			resource_desc.Format = DXGI_FORMAT_UNKNOWN;
			resource_desc.SampleDesc.Count = 1;
			resource_desc.SampleDesc.Quality = 0;
			resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

			if ( FAILED( PlatformData.pD3DDevice->CreateCommittedResource(
				&heap_props,
				D3D12_HEAP_FLAG_NONE,
				&resource_desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS( &constant_buffer ) ) ) )
			{
				return NULL;
			}

			// Initialize with data if provided
			if ( init_data_constant != NULL )
			{
				void* mapped_data = NULL;
				D3D12_RANGE read_range = { 0, 0 };
				if ( SUCCEEDED( constant_buffer->Map( 0, &read_range, &mapped_data ) ) )
				{
					memcpy( mapped_data, init_data_constant, sizeof_in_bytes_constants );
					constant_buffer->Unmap( 0, nullptr );
				}
			}
		}

		return ( void* )constant_buffer;

#endif
	}

	char* ReplaceFirst_return_allocated_buffer_free_after_use( char const* src, char const* src_end, char const* token, char const* token_end, char const* new_str, char const* new_str_end )
	{
		IM_UNUSED( new_str_end );
		char const* pos = ImStristr( src, src_end, token, token_end );

		int dx0 = pos - src;
		int dx1 = strlen(new_str);
		int dx2 = strlen(pos);
		int dxtk = strlen(token);
		char* full_buffer = ( char* )IM_ALLOC(dx0 + dx1 + dx2);
		memcpy( full_buffer, src, dx0 );
		memcpy( &full_buffer[ dx0 ], new_str, dx1 );
		memcpy( &full_buffer[ dx0 + dx1 ], src + dx0 + dxtk, dx2 );

		return full_buffer;
	}

	// 
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
								  bool multiply_with_texture )
	{
		void* vs_const = NULL;
		void* ps_const = NULL;

		static const char* pShaderBase =
"#ifndef __IM_SHADER_H__\n\
#define __IM_SHADER_H__\n\
\n\
#if defined(IM_SHADER_HLSL)\n\
#define IMS_IN        in\n\
#define IMS_OUT       out\n\
#define IMS_INOUT     inout\n\
#define IMS_UNIFORM   uniform\n\
#define IMS_CBUFFER   cbuffer\n\
#elif defined(IM_SHADER_GLSL)\n\
#define IMS_IN        in\n\
#define IMS_OUT       out\n\
#define IMS_INOUT     inout\n\
#define IMS_UNIFORM   const\n\
#define IMS_CBUFFER   uniform\n\
#endif\n\
\n\
#if defined(IM_SHADER_HLSL)\n\
\n\
#define Mat44f matrix<float, 4, 4>\n\
#define Mat33f matrix<float, 3, 3>\n\
\n\
#endif\n\
\n\
#if defined(IM_SHADER_GLSL)\n\
\n\
#define Mat44f   mat4\n\
#define Mat33f   mat3\n\
\n\
#define float2   vec2\n\
#define float3   vec3\n\
#define float4   vec4\n\
#define uint2    uvec2\n\
#define uint3    uvec3\n\
#define uint4    uvec4\n\
#define int2     ivec2\n\
#define int3     ivec3\n\
#define int4     ivec4\n\
\n\
#define float4x4 mat4\n\
#define float3x3 mat3\n\
#define float2x2 mat2\n\
\n\
#endif\n\
#endif\n";

		static const char* pVertexShaderTemplate =
"%IM_PLATFORM_SHADER_BASE%\n\
\n\
IMS_CBUFFER vertexBuffer\n\
{\n\
	float4x4 ProjectionMatrix;\n\
};\n\
struct VS_INPUT\n\
{\n\
	//float2 pos : POSITION;\n\
	//float4 col : COLOR0;\n\
	//float2 uv  : TEXCOORD0;\n\
%VB_DESC%\n\
};\n\
\n\
struct PS_INPUT\n\
{\n\
	//float4 pos : SV_POSITION;\n\
	//float4 col : COLOR0;\n\
	//float2 uv  : TEXCOORD0;\n\
%VS_TO_PS%\n\
};\n\
\n\
IMS_CBUFFER VS_CONSTANT_BUFFER\n\
{\n\
%VS_PARAMS%\n\
};\n\
\n\
%VS_PRE_FUNCS%\n\
\n\
PS_INPUT main_vs(VS_INPUT input)\n\
{\n\
	PS_INPUT output;\n\
	//output.pos = mul( ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\n\
	//output.col = input.col;\n\
	//output.uv  = input.uv;\n\
%VS_SOURCE%\n\
	return output;\n\
}\n";

		static const char* pPixelShaderTemplate =
"%IM_PLATFORM_SHADER_BASE%\n\
\n\
struct PS_INPUT\n\
{\n\
	//float4 pos : SV_POSITION;\n\
	//float4 col : COLOR0;\n\
	//float2 uv  : TEXCOORD0;\n\
%VS_TO_PS%\n\
};\n\
\n\
IMS_CBUFFER PS_CONSTANT_BUFFER\n\
{\n\
%PS_PARAMS%\n\
};\n\
\n\
%PS_PRE_FUNCS%\n\
\n\
sampler sampler0;\n\
Texture2D texture0;\n\
\n\
float4 main_ps(PS_INPUT input) : SV_Target\n\
{\n\
	float2 uv = input.uv.xy;\n\
	float4 col_in = input.col;\n\
	float4 col_out = float4(1.0f, 1.0f, 1.0f, 1.0f);\n\
	%IM_PLATFORM_WRITE_OUT_COL%\n\
	%IM_MUL_WITH_TEX%\n\
	return col_out;\n\
}\n";

		char* replace0;
		char* replace1;
		char* replace2;
		char* replace3;
		char* replace4;

		replace0 = ReplaceFirst_return_allocated_buffer_free_after_use(
			pVertexShaderTemplate, NULL, "%IM_PLATFORM_SHADER_BASE%", NULL, pShaderBase, NULL );
		replace1 = ReplaceFirst_return_allocated_buffer_free_after_use(
			replace0, NULL, "%VB_DESC%", NULL, vb_desc, NULL );
		replace2 = ReplaceFirst_return_allocated_buffer_free_after_use(
			replace1, NULL, "%VS_TO_PS%", NULL, vs_to_ps_desc, NULL );
		replace3 = ReplaceFirst_return_allocated_buffer_free_after_use(
			replace2, NULL, "%VS_PRE_FUNCS%", NULL, vs_pre_functions, NULL );
		replace4 = ReplaceFirst_return_allocated_buffer_free_after_use(
			replace3, NULL, "%VS_PARAMS%", NULL, vs_params, NULL );
		*out_vs_source = ReplaceFirst_return_allocated_buffer_free_after_use(
			replace4, NULL, "%VS_SOURCE%", NULL, vs_source, NULL );

		IM_FREE( replace0 );
		IM_FREE( replace1 );
		IM_FREE( replace2 );
		IM_FREE( replace3 );
		IM_FREE( replace4 );

		replace0 = ReplaceFirst_return_allocated_buffer_free_after_use(
			pPixelShaderTemplate, NULL, "%IM_PLATFORM_SHADER_BASE%", NULL, pShaderBase, NULL );
		replace1 = ReplaceFirst_return_allocated_buffer_free_after_use(
			replace0, NULL, "%IM_PLATFORM_WRITE_OUT_COL%", NULL, ps_source, NULL );
		replace2 = ReplaceFirst_return_allocated_buffer_free_after_use(
			replace1, NULL, "%PS_PARAMS%", NULL, ps_params, NULL );
		replace3 = ReplaceFirst_return_allocated_buffer_free_after_use(
			replace2, NULL, "%PS_PRE_FUNCS%", NULL, ps_pre_functions, NULL );
		replace4 = ReplaceFirst_return_allocated_buffer_free_after_use(
			replace3, NULL, "%VS_TO_PS%", NULL, vs_to_ps_desc, NULL );

		if ( multiply_with_texture )
		{
			*out_ps_source = ReplaceFirst_return_allocated_buffer_free_after_use(
				replace4, NULL, "%IM_MUL_WITH_TEX%", NULL, "col_out *= col_in * texture0.Sample( sampler0, input.uv );", NULL );
		}
		else
		{
			*out_ps_source = ReplaceFirst_return_allocated_buffer_free_after_use(
				replace4, NULL, "%IM_MUL_WITH_TEX%", NULL, "", NULL );
		}
		IM_FREE( replace0 );
		IM_FREE( replace1 );
		IM_FREE( replace2 );
		IM_FREE( replace3 );
	}

	int get_file_size( ImFileHandle file ) // path to file
	{
		fseek( file, 0, SEEK_END );
		int sz = ftell( file );
		fseek( file, 0, SEEK_SET );

		return sz;
	}

	void	CreateDefaultPixelShaderSource
	( char** out_vs_source,
	  char** out_ps_source,
	  char const* ps_pre_functions,
	  char const* ps_params,
	  char const* ps_source,
	  bool multiply_with_texture )
	{
		size_t data_size = 0;
		*out_vs_source = ( char * )ImFileLoadToMemory( "shaders/hlsl_src/default.hlsl", "rb", &data_size, 0 );
		IM_ASSERT( *out_vs_source );
		IM_ASSERT( data_size > 0 );
		*out_ps_source = ( char * )ImFileLoadToMemory( "shaders/hlsl_src/default.hlsl", "rb", &data_size, 0 );
		IM_ASSERT( *out_ps_source );
		IM_ASSERT( data_size > 0 );

		ImFileHandle file_vs = ImFileOpen( "shaders/hlsl_src/default.hlsl", "rb" );
		ImU64 size_vs = ImFileGetSize( file_vs );
		*out_vs_source = ( char * )IM_ALLOC( size_vs + 1 );
		ImFileRead( *out_vs_source, 1, size_vs, file_vs );
		( *out_vs_source )[ size_vs ] = '\0';
		ImFileClose( file_vs );


		ImFileHandle file_ps = ImFileOpen( "shaders/hlsl_src/default.hlsl", "rb" );
		ImU64 size_ps = ImFileGetSize( file_ps );
		*out_ps_source = ( char * )IM_ALLOC( size_ps + 1 );
		ImFileRead( *out_ps_source, 1, size_ps, file_ps );
		(
			*out_ps_source )[ size_ps ] = '\0';
		ImFileClose( file_ps );


//		CreateShaderSource( out_vs_source,
//							out_ps_source,
//							"",
//							"",
//"output.pos = mul( ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\n\
//output.col = input.col;\n\
//output.uv  = input.uv;\n",
//							ps_pre_functions,
//							ps_params,
//							ps_source,
//"float2 pos : POSITION;\n\
//float4 col : COLOR0;\n\
//float2 uv  : TEXCOORD0;\n",
//"float4 pos : SV_POSITION;\n\
//float4 col : COLOR0;\n\
//float2 uv  : TEXCOORD0;\n",
//							true );
	}

	ImDrawShader	CreateShader( char const* vs_source,
								  char const* ps_source,
								  int sizeof_in_bytes_vs_constants,
								  void* init_vs_data_constant,
								  int sizeof_in_bytes_ps_constants,
								  void* init_ps_data_constant )
	{
		return Backend::CreateShader( vs_source, ps_source,
									  sizeof_in_bytes_vs_constants, init_vs_data_constant,
									  sizeof_in_bytes_ps_constants, init_ps_data_constant );
	}

	void		ReleaseShader( ImDrawShader& shader )
	{
		Backend::ReleaseShader( shader );
	}
#endif 

	void	CreateVertexBuffer( ImVertexBuffer*& vb, int sizeof_vertex_buffer, int vertices_count )
	{
		Backend::CreateVertexBuffer( vb, sizeof_vertex_buffer, vertices_count );
	}

	void	CreateVertexBufferAndGrow( ImVertexBuffer** buffer, int sizeof_vertex_buffer, int vertices_count, int growth_size )
	{
		Backend::CreateVertexBufferAndGrow( buffer, sizeof_vertex_buffer, vertices_count, growth_size );
	}

	void	UpdateVertexBuffer( ImVertexBuffer** buffer, int sizeof_vertex_buffer, int vertices_count, void* cpu_data )
	{
		Backend::UpdateVertexBuffer( buffer, sizeof_vertex_buffer, vertices_count, cpu_data );
	}

	void	ReleaseVertexBuffer( ImVertexBuffer** buffer )
	{
		Backend::ReleaseVertexBuffer( buffer );
	}

	void	CreateIndexBuffer( ImIndexBuffer*& ib, int indices_count )
	{
		Backend::CreateIndexBuffer( ib, indices_count );
	}

	void	CreateIndexBufferAndGrow( ImIndexBuffer** buffer, int indices_count, int growth_size )
	{
		Backend::CreateIndexBufferAndGrow( buffer, indices_count, growth_size );
	}

	void	UpdateIndexBuffer( ImIndexBuffer** buffer, int indices_count, void* cpu_data )
	{
		Backend::UpdateIndexBuffer( buffer, indices_count, cpu_data );
	}

	void	ReleaseIndexBuffer( ImIndexBuffer** buffer )
	{
		Backend::ReleaseIndexBuffer( buffer );
	}

	void ImSetCustomShader( const ImDrawList* parent_list, const ImDrawCmd* cmd )
	{
#if (IM_CURRENT_GFX == IM_GFX_OPENGL3)
		// OpenGL custom shader setting - not implemented yet
		// Shaders in OpenGL are managed differently than DirectX
		fprintf(stderr, "ImSetCustomShader not implemented for OpenGL3\n");

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)

		ImDrawShader* shader = ( ImDrawShader* )cmd->UserCallbackData;
		ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
		ID3D11DeviceContext* ctx = bd->pd3dDeviceContext;
		ctx->VSSetShader( ( ID3D11VertexShader* )( shader->vs ), nullptr, 0 );
		ctx->PSSetShader( ( ID3D11PixelShader* )( shader->ps ), nullptr, 0 );
		if ( shader->vs_cst != NULL &&
			 shader->sizeof_in_bytes_vs_constants > 0 &&
			 shader->cpu_vs_data != NULL &&
			 shader->is_cpu_vs_data_dirty )
		{
			D3D11_MAPPED_SUBRESOURCE mapped_resource;
			if ( ctx->Map( ( ID3D11Buffer* )shader->vs_cst, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource ) == S_OK )
			{
				IM_ASSERT( mapped_resource.RowPitch == shader->sizeof_in_bytes_vs_constants );
				memcpy( mapped_resource.pData, shader->cpu_vs_data, shader->sizeof_in_bytes_vs_constants );
				ctx->Unmap( ( ID3D11Buffer* )shader->vs_cst, 0 );
				shader->is_cpu_vs_data_dirty = false;
			}
		}
		if ( shader->vs_cst )
			ctx->VSSetConstantBuffers( 0, 1, ( ID3D11Buffer* const* )( &( shader->vs_cst ) ) );
		if ( shader->ps_cst != NULL &&
			 shader->sizeof_in_bytes_ps_constants > 0 &&
			 shader->cpu_ps_data != NULL &&
			 shader->is_cpu_ps_data_dirty )
		{
			D3D11_MAPPED_SUBRESOURCE mapped_resource;
			if ( ctx->Map( ( ID3D11Buffer* )shader->ps_cst, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource ) == S_OK )
			{
				IM_ASSERT( mapped_resource.RowPitch == shader->sizeof_in_bytes_ps_constants );
				memcpy( mapped_resource.pData, shader->cpu_ps_data, shader->sizeof_in_bytes_ps_constants );
				ctx->Unmap( ( ID3D11Buffer* )shader->ps_cst, 0 );
				shader->is_cpu_ps_data_dirty = false;
			}
		}
		if ( shader->ps_cst )
			ctx->PSSetConstantBuffers( 0, 1, ( ID3D11Buffer* const* )( &( shader->ps_cst ) ) );

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)

		// DX12 custom shaders require pipeline state objects which cannot be easily changed per draw call
		// For now, we'll just update constant buffers if they exist
		// Full implementation would require creating PSOs ahead of time
		ImDrawShader* shader = ( ImDrawShader* )cmd->UserCallbackData;

		if ( shader->vs_cst != NULL &&
			 shader->sizeof_in_bytes_vs_constants > 0 &&
			 shader->cpu_vs_data != NULL &&
			 shader->is_cpu_vs_data_dirty )
		{
			ID3D12Resource* vs_buffer = ( ID3D12Resource* )shader->vs_cst;
			void* mapped_data = NULL;
			D3D12_RANGE read_range = { 0, 0 };
			if ( SUCCEEDED( vs_buffer->Map( 0, &read_range, &mapped_data ) ) )
			{
				memcpy( mapped_data, shader->cpu_vs_data, shader->sizeof_in_bytes_vs_constants );
				vs_buffer->Unmap( 0, nullptr );
				shader->is_cpu_vs_data_dirty = false;
			}
		}

		if ( shader->ps_cst != NULL &&
			 shader->sizeof_in_bytes_ps_constants > 0 &&
			 shader->cpu_ps_data != NULL &&
			 shader->is_cpu_ps_data_dirty )
		{
			ID3D12Resource* ps_buffer = ( ID3D12Resource* )shader->ps_cst;
			void* mapped_data = NULL;
			D3D12_RANGE read_range = { 0, 0 };
			if ( SUCCEEDED( ps_buffer->Map( 0, &read_range, &mapped_data ) ) )
			{
				memcpy( mapped_data, shader->cpu_ps_data, shader->sizeof_in_bytes_ps_constants );
				ps_buffer->Unmap( 0, nullptr );
				shader->is_cpu_ps_data_dirty = false;
			}
		}

		// Note: Actually applying shaders in DX12 requires PSO management
		// which is beyond the scope of this simple implementation
		// The shaders are compiled but not actively used in rendering

#endif
	}

	void		InternalUpdateCustomCPUShaderConstants( int sizeof_in_bytes_constants, void* ptr_to_constants, void*& cpu_data, bool& is_cpu_data_dirty )
	{
		if ( sizeof_in_bytes_constants <= 0 ||
			 ptr_to_constants == NULL )
			return;

		if ( cpu_data == NULL )
		{
			cpu_data = IM_ALLOC( sizeof_in_bytes_constants );
		}
		// Copy the new constants into the persistent CPU-side buffer and mark dirty
		memcpy( cpu_data, ptr_to_constants, sizeof_in_bytes_constants );
		is_cpu_data_dirty = true;
	}
	void		UpdateCustomPixelShaderConstants( ImDrawShader &shader, void *ptr_to_constants )
	{
		InternalUpdateCustomCPUShaderConstants( shader.sizeof_in_bytes_ps_constants,
												ptr_to_constants,
												shader.cpu_ps_data,
												shader.is_cpu_ps_data_dirty );
	}
	void		UpdateCustomVertexShaderConstants( ImDrawShader &shader, void *ptr_to_constants )
	{
		InternalUpdateCustomCPUShaderConstants( shader.sizeof_in_bytes_vs_constants,
												ptr_to_constants,
												shader.cpu_vs_data,
												shader.is_cpu_vs_data_dirty );
	}
	void		BeginCustomShader( ImDrawList* draw, ImDrawShader& shader )
	{
		draw->AddCallback( &ImSetCustomShader, &shader );
	}
	void		EndCustomShader( ImDrawList* draw )
	{
		draw->AddCallback( ImDrawCallback_ResetRenderState, NULL );
	}

	bool		ImIsMaximized()
	{
		return Platform::IsMaximized();
	}

	bool		CustomTitleBarSupported()
	{
		#if (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
		return false;
		#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
		return true;
		#else
		return false;
		#endif
	}

	bool		CustomTitleBarEnabled()
	{
		return PlatformData.bCustomTitleBar;
	}

	void		EnableCustomTitleBar()
	{
		PlatformData.bCustomTitleBar = true;
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
#ifndef IM_THE_CHERNO_GLFW3
		fprintf( stderr, "To have the support of Custom Title Bar on GLFW3, need CHERNO dev version of GLFW3 (https://github.com/TheCherno/glfw/tree/dev).\n" );
#endif
#endif
	}

	void		DisableCustomTitleBar()
	{
		PlatformData.bCustomTitleBar = false;
	}

	void		DrawCustomMenuBarDefault()
	{
		ImGui::Text( "ImPlatform with Custom Title Bar" );
		ImGui::SameLine();

		if ( ImGui::Button( "Minimize" ) )
			ImPlatform::MinimizeApp();
		ImGui::SameLine();

		if ( ImGui::Button( "Maximize" ) )
			ImPlatform::MaximizeApp();
		ImGui::SameLine();

		if ( ImGui::Button( "Close" ) )
			ImPlatform::CloseApp();
		ImGui::SameLine();
	}

	void		MinimizeApp()
	{
		Platform::MinimizeApp();
	}

	void		MaximizeApp()
	{
		Platform::MaximizeApp();
	}

	void		CloseApp()
	{
		Platform::CloseApp();
	}

	bool		BeginCustomTitleBar( float fHeight )
	{
		IM_ASSERT( ImPlatform::CustomTitleBarEnabled() );

		ImGuiViewport* pViewport = ImGui::GetMainViewport();
		ImVec2 vDragZoneSize = ImVec2( pViewport->Size.x, fHeight );

		float titlebarVerticalOffset = ImIsMaximized() ? 6.0f : 0.0f;

		ImGui::SetNextWindowPos( ImVec2( pViewport->Pos.x, pViewport->Pos.y + titlebarVerticalOffset ), ImGuiCond_Always );
		ImGui::SetNextWindowSize( vDragZoneSize );
#ifdef IMGUI_HAS_VIEWPORT
		ImGui::SetNextWindowViewport( pViewport->ID );
#endif

		bool bRet = ImGui::Begin( "##ImPlatformCustomTitleBar", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking );

		ImVec2 vPos = ImGui::GetCursorPos();
		ImGui::SetNextItemAllowOverlap();
		ImGui::InvisibleButton( "##ImPlatformCustomTitleBarDragZone", vDragZoneSize );
		PlatformData.bTitleBarHovered		= ImGui::IsItemHovered();
		PlatformData.vEndCustomToolBar		= ImGui::GetCursorPos();
		PlatformData.fCustomTitleBarHeight	= fHeight;
		ImGui::SetCursorPos( vPos );

		return bRet;
	}

	void		EndCustomTitleBar()
	{
		IM_ASSERT( ImPlatform::CustomTitleBarEnabled() );

		ImGui::SetCursorPos( PlatformData.vEndCustomToolBar );
		ImGui::End();
		ImGuiViewport* pViewport = ImGui::GetMainViewport();
		pViewport->WorkPos.y += PlatformData.fCustomTitleBarHeight;
	}

#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
	LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

#if (IM_CURRENT_GFX == IM_GFX_DIRECTX11) || (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
	void	ImCreateRenderTarget()
	{
#if (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
		ID3D11Texture2D* pBackBuffer;
		HRESULT hr = PlatformData.pSwapChain->GetBuffer( 0, IID_PPV_ARGS( &pBackBuffer ) );
		if ( SUCCEEDED( hr ) )
		{
			hr = PlatformData.pD3DDevice->CreateRenderTargetView( pBackBuffer, NULL, &PlatformData.pMainRenderTargetView );
			pBackBuffer->Release();
		}
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
#if (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
		if (PlatformData.pMainRenderTargetView)
		{
			PlatformData.pMainRenderTargetView->Release();
			PlatformData.pMainRenderTargetView = nullptr;
		}
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

#if (IM_CURRENT_GFX == IM_GFX_VULKAN)
	static void check_vk_result( VkResult err )
	{
		if ( err == VK_SUCCESS )
			return;
		fprintf( stderr, "[vulkan] Error: VkResult = %d\n", err );
		if ( err < 0 )
			abort();
	}

#ifdef _DEBUG
	static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report( VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData )
	{
		(void)flags; (void)object; (void)location; (void)messageCode; (void)pUserData; (void)pLayerPrefix;
		fprintf( stderr, "[vulkan] Debug report from ObjectType: %i\nMessage: %s\n\n", objectType, pMessage );
		return VK_FALSE;
	}
#endif

	static bool IsExtensionAvailable( const ImVector<VkExtensionProperties>& properties, const char* extension )
	{
		for ( const VkExtensionProperties& p : properties )
			if ( strcmp( p.extensionName, extension ) == 0 )
				return true;
		return false;
	}

	bool ImSetupVulkan( ImVector<const char*> instance_extensions )
	{
		VkResult err;

		// Create Vulkan Instance
		{
			VkInstanceCreateInfo create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

			// Enumerate available extensions
			uint32_t properties_count;
			ImVector<VkExtensionProperties> properties;
			vkEnumerateInstanceExtensionProperties( nullptr, &properties_count, nullptr );
			properties.resize( properties_count );
			err = vkEnumerateInstanceExtensionProperties( nullptr, &properties_count, properties.Data );
			check_vk_result( err );

			// Enable required extensions
			if ( IsExtensionAvailable( properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME ) )
				instance_extensions.push_back( VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME );
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
			if ( IsExtensionAvailable( properties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME ) )
			{
				instance_extensions.push_back( VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME );
				create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
			}
#endif

			// Enabling validation layers
#ifdef _DEBUG
			const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
			create_info.enabledLayerCount = 1;
			create_info.ppEnabledLayerNames = layers;
			instance_extensions.push_back( "VK_EXT_debug_report" );
#endif

			// Create Vulkan Instance
			create_info.enabledExtensionCount = (uint32_t)instance_extensions.Size;
			create_info.ppEnabledExtensionNames = instance_extensions.Data;
			err = vkCreateInstance( &create_info, PlatformData.pAllocator, &PlatformData.pInstance );
			check_vk_result( err );

			// Setup the debug report callback
#ifdef _DEBUG
			auto f_vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr( PlatformData.pInstance, "vkCreateDebugReportCallbackEXT" );
			IM_ASSERT( f_vkCreateDebugReportCallbackEXT != nullptr );
			VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
			debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
			debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
			debug_report_ci.pfnCallback = debug_report;
			debug_report_ci.pUserData = nullptr;
			err = f_vkCreateDebugReportCallbackEXT( PlatformData.pInstance, &debug_report_ci, PlatformData.pAllocator, &PlatformData.pDebugReport );
			check_vk_result( err );
#endif
		}

		// Select Physical Device (GPU)
		PlatformData.pPhysicalDevice = ImGui_ImplVulkanH_SelectPhysicalDevice( PlatformData.pInstance );
		IM_ASSERT( PlatformData.pPhysicalDevice != VK_NULL_HANDLE );

		// Select graphics queue family
		PlatformData.uQueueFamily = ImGui_ImplVulkanH_SelectQueueFamilyIndex( PlatformData.pPhysicalDevice );
		IM_ASSERT( PlatformData.uQueueFamily != (uint32_t)-1 );

		// Create Logical Device (with 1 queue)
		{
			ImVector<const char*> device_extensions;
			device_extensions.push_back( "VK_KHR_swapchain" );

			// Enumerate physical device extension
			uint32_t properties_count;
			ImVector<VkExtensionProperties> properties;
			vkEnumerateDeviceExtensionProperties( PlatformData.pPhysicalDevice, nullptr, &properties_count, nullptr );
			properties.resize( properties_count );
			vkEnumerateDeviceExtensionProperties( PlatformData.pPhysicalDevice, nullptr, &properties_count, properties.Data );
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
			if ( IsExtensionAvailable( properties, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME ) )
				device_extensions.push_back( VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME );
#endif

			const float queue_priority[] = { 1.0f };
			VkDeviceQueueCreateInfo queue_info[1] = {};
			queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue_info[0].queueFamilyIndex = PlatformData.uQueueFamily;
			queue_info[0].queueCount = 1;
			queue_info[0].pQueuePriorities = queue_priority;
			VkDeviceCreateInfo create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			create_info.queueCreateInfoCount = sizeof( queue_info ) / sizeof( queue_info[0] );
			create_info.pQueueCreateInfos = queue_info;
			create_info.enabledExtensionCount = (uint32_t)device_extensions.Size;
			create_info.ppEnabledExtensionNames = device_extensions.Data;
			err = vkCreateDevice( PlatformData.pPhysicalDevice, &create_info, PlatformData.pAllocator, &PlatformData.pDevice );
			check_vk_result( err );
			vkGetDeviceQueue( PlatformData.pDevice, PlatformData.uQueueFamily, 0, &PlatformData.pQueue );
		}

		// Create Descriptor Pool
		{
			VkDescriptorPoolSize pool_sizes[] =
			{
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE + 100 }, // +100 for user textures
			};
			VkDescriptorPoolCreateInfo pool_info = {};
			pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
			pool_info.maxSets = 0;
			for ( VkDescriptorPoolSize& pool_size : pool_sizes )
				pool_info.maxSets += pool_size.descriptorCount;
			pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE( pool_sizes );
			pool_info.pPoolSizes = pool_sizes;
			err = vkCreateDescriptorPool( PlatformData.pDevice, &pool_info, PlatformData.pAllocator, &PlatformData.pDescriptorPool );
			check_vk_result( err );
		}

		return true;
	}

	bool ImSetupVulkanWindow( VkSurfaceKHR surface, int width, int height )
	{
		ImGui_ImplVulkanH_Window* wd = &PlatformData.oMainWindowData;
		wd->Surface = surface;

		// Check for WSI support
		VkBool32 res;
		vkGetPhysicalDeviceSurfaceSupportKHR( PlatformData.pPhysicalDevice, PlatformData.uQueueFamily, wd->Surface, &res );
		if ( res != VK_TRUE )
		{
			fprintf( stderr, "Error no WSI support on physical device 0\n" );
			return false;
		}

		// Select Surface Format
		const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
		const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
		wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat( PlatformData.pPhysicalDevice, wd->Surface, requestSurfaceImageFormat, (size_t)IM_ARRAYSIZE( requestSurfaceImageFormat ), requestSurfaceColorSpace );

		// Select Present Mode
		VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
		wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode( PlatformData.pPhysicalDevice, wd->Surface, &present_modes[0], IM_ARRAYSIZE( present_modes ) );

		// Create SwapChain, RenderPass, Framebuffer, etc.
		IM_ASSERT( PlatformData.uMinImageCount >= 2 );
		ImGui_ImplVulkanH_CreateOrResizeWindow( PlatformData.pInstance, PlatformData.pPhysicalDevice, PlatformData.pDevice, wd, PlatformData.uQueueFamily, PlatformData.pAllocator, width, height, PlatformData.uMinImageCount, PlatformData.uSwapChainImageUsage );

		return true;
	}

	void ImCleanupVulkan()
	{
		vkDestroyDescriptorPool( PlatformData.pDevice, PlatformData.pDescriptorPool, PlatformData.pAllocator );

#ifdef _DEBUG
		// Remove the debug report callback
		auto f_vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr( PlatformData.pInstance, "vkDestroyDebugReportCallbackEXT" );
		f_vkDestroyDebugReportCallbackEXT( PlatformData.pInstance, PlatformData.pDebugReport, PlatformData.pAllocator );
#endif

		vkDestroyDevice( PlatformData.pDevice, PlatformData.pAllocator );
		vkDestroyInstance( PlatformData.pInstance, PlatformData.pAllocator );
	}

	void ImCleanupVulkanWindow()
	{
		ImGui_ImplVulkanH_DestroyWindow( PlatformData.pInstance, PlatformData.pDevice, &PlatformData.oMainWindowData, PlatformData.pAllocator );
	}
#endif

	bool ImCreateDeviceD3D( HWND hWnd )
	{
#if (IM_CURRENT_GFX == IM_GFX_DIRECTX11)

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
#ifdef _DEBUG
		// Enable D3D11 debug layer in debug builds
		uCreateDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
		D3D_FEATURE_LEVEL featureLevel;
		const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };

		HRESULT res = D3D11CreateDeviceAndSwapChain( nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, uCreateDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &oSwapDesc, &PlatformData.pSwapChain, &PlatformData.pD3DDevice, &featureLevel, &PlatformData.pD3DDeviceContext );
		if ( res == DXGI_ERROR_UNSUPPORTED ) // Try high-performance WARP software driver if hardware is not available.
			res = D3D11CreateDeviceAndSwapChain( nullptr, D3D_DRIVER_TYPE_WARP, nullptr, uCreateDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &oSwapDesc, &PlatformData.pSwapChain, &PlatformData.pD3DDevice, &featureLevel, &PlatformData.pD3DDeviceContext );
		if ( FAILED( res ) )
			return false;

		ImCreateRenderTarget();
		return true;
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
#ifdef _DEBUG
		ID3D12Debug* pdx12Debug = nullptr;
		if ( SUCCEEDED( D3D12GetDebugInterface( IID_PPV_ARGS( &pdx12Debug ) ) ) )
		{
			pdx12Debug->EnableDebugLayer();
#ifdef DX12_ENABLE_DEBUG_LAYER
			// Additional debug features if explicitly requested
			ID3D12Debug1* pdx12Debug1 = nullptr;
			if ( SUCCEEDED( pdx12Debug->QueryInterface( IID_PPV_ARGS( &pdx12Debug1 ) ) ) )
			{
				pdx12Debug1->SetEnableGPUBasedValidation( TRUE );
				pdx12Debug1->Release();
			}
#endif
		}
#endif

		// Create device
		D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
		HRESULT hr = D3D12CreateDevice( nullptr, featureLevel, IID_PPV_ARGS( &PlatformData.pD3DDevice ) );
		if ( FAILED( hr ) )
		{
			return false;
		}

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
			desc.NumDescriptors = PlatformDataImpl::NUM_SRV_DESCRIPTORS;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			desc.NodeMask = 1;
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
		return true;

#else
		// Unsupported graphics API
		return false;
#endif
	}

	void ImCleanupDeviceD3D()
	{
#if (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
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
		}
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
#else
		// Unsupported graphics API - no cleanup needed
#endif
	}

	void ImResetDevice()
	{
		// DirectX 11/12 don't need device reset - this function is now unused
	}

#if ((IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32) && (IM_CURRENT_GFX == IM_GFX_OPENGL3))

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

		static RECT border_thickness = { 4, 4, 4, 4 };
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
#elif (IM_CURRENT_GFX == IM_GFX_OPENGL3)
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
			::PostQuitMessage( 0 );
			return 0;
		case WM_NCCALCSIZE:
			if ( CustomTitleBarEnabled() && lParam )
			{
				NCCALCSIZE_PARAMS* sz = ( NCCALCSIZE_PARAMS* )lParam;
				sz->rgrc[ 0 ].left += border_thickness.left;
				sz->rgrc[ 0 ].right -= border_thickness.right;
				sz->rgrc[ 0 ].bottom -= border_thickness.bottom;
				return 0;
			}
			break;
		case WM_NCHITTEST:
		{
			if ( !CustomTitleBarEnabled() )
				break;

			if ( !ImIsMaximized() )
			{
				RECT winRc;
				GetClientRect( hWnd, &winRc );
				// Hit test for custom frames
				POINT pt = { GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) };
				ScreenToClient( hWnd, &pt );
				const int verticalBorderSize = GetSystemMetrics( SM_CYFRAME );

				if ( PtInRect( &winRc, pt ) )
				{
					enum
					{
						left = 1, top = 2, right = 4, bottom = 8
					};
					int hit = 0;
					if ( pt.x <= border_thickness.left )
						hit |= left;
					if ( pt.x >= winRc.right - border_thickness.right )
						hit |= right;
					if ( pt.y <= border_thickness.top || pt.y < verticalBorderSize )
						hit |= top;
					if ( pt.y >= winRc.bottom - border_thickness.bottom )
						hit |= bottom;
					if ( hit )
						fprintf( stderr, "\n" );

					if ( hit & top && hit & left )		return HTTOPLEFT;
					if ( hit & top && hit & right )		return HTTOPRIGHT;
					if ( hit & bottom && hit & left )	return HTBOTTOMLEFT;
					if ( hit & bottom && hit & right )	return HTBOTTOMRIGHT;
					if ( hit & left )					return HTLEFT;
					if ( hit & top )					return HTTOP;
					if ( hit & right )					return HTRIGHT;
					if ( hit & bottom )					return HTBOTTOM;
				}
			}
			if ( CustomTitleBarEnabled() && PlatformData.bTitleBarHovered )
			{
				return HTCAPTION;
			}
			else
			{
				return HTCLIENT;
			}
		}
		case WM_DPICHANGED:
			if ( ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports )
			{
				//const int dpi = HIWORD(wParam);
				//printf("WM_DPICHANGED to %d (%.0f%%)\n", dpi, (float)dpi / 96.0f * 100.0f);
				const RECT* suggested_rect = ( RECT* )lParam;
				::SetWindowPos( hWnd, nullptr, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE );
			}
		}
		return ::DefWindowProc( hWnd, msg, wParam, lParam );
	}
#endif

#if IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW
	static void ImGLFWErrorCallback(int error, const char* description)
	{
		fprintf( stderr, "GLFW Error %d: %s\n", error, description );
	}
#endif

	// TODO: Add support for maximize/minimize/...
#ifdef CreateWindow
#undef CreateWindow // Windows API :(
#endif
	bool CreateWindow( char const* pWindowsName, ImVec2 const vPos, ImU32 const uWidth, ImU32 const uHeight )
	{
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)

		//ImGui_ImplWin32_EnableDpiAwareness();

#ifdef UNICODE
		const size_t WCHARBUF = 4096;
		wchar_t  wszDest[ WCHARBUF ];
		MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, pWindowsName, -1, wszDest, WCHARBUF );
		PlatformData.oWinStruct = { sizeof( PlatformData.oWinStruct ), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle( nullptr ), nullptr, nullptr, nullptr, nullptr, L"ImPlatform", nullptr };
		::RegisterClassExW( &PlatformData.oWinStruct );
		PlatformData.pHandle = ::CreateWindowW(
			PlatformData.oWinStruct.lpszClassName,
			wszDest,
			PlatformData.bCustomTitleBar ? ( WS_POPUPWINDOW | WS_THICKFRAME ) : WS_OVERLAPPEDWINDOW,
			( int )vPos.x, ( int )vPos.y,
			uWidth, uHeight,
			nullptr,
			nullptr,
			PlatformData.oWinStruct.hInstance,
			nullptr );
#else
		PlatformData.oWinStruct = { sizeof( PlatformData.oWinStruct ), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle( nullptr ), nullptr, nullptr, nullptr, nullptr, "ImPlatform", nullptr };
		::RegisterClassExA( &PlatformData.oWinStruct );
		PlatformData.pHandle = ::CreateWindowA(
			PlatformData.oWinStruct.lpszClassName,
			pWindowsName,
			PlatformData.bCustomTitleBar ? ( WS_POPUPWINDOW | WS_THICKFRAME ) : WS_OVERLAPPEDWINDOW,
			( int )vPos.x, ( int )vPos.y,
			uWidth, uHeight,
			nullptr,
			nullptr,
			PlatformData.oWinStruct.hInstance,
			nullptr );
#endif

#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)

		glfwSetErrorCallback( ImGLFWErrorCallback );
		if ( !glfwInit() )
			return false;

		// Decide GL+GLSL versions (following imgui backend approach)
#if (IM_CURRENT_GFX == IM_GFX_VULKAN)
		// Vulkan requires no OpenGL context
		glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
#elif defined(__APPLE__)
		// GL 3.2 + GLSL 150 (Required on Mac)
		if ( PlatformData.pGLSLVersion == nullptr )
			PlatformData.pGLSLVersion = "#version 150";
		glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
		glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 2 );
		glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );	// 3.2+ only
		glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE );				// Required on Mac
#elif (IM_CURRENT_GFX == IM_GFX_OPENGL3)
		// GL 3.0 + GLSL 130 (Default for desktop OpenGL3)
		if ( PlatformData.pGLSLVersion == nullptr )
			PlatformData.pGLSLVersion = "#version 130";
		glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
		glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 0 );
		//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);		// 3.2+ only
		//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);				// 3.0+ only
#endif

		if ( PlatformData.bCustomTitleBar )
		{
			//	glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
#ifdef IM_THE_CHERNO_GLFW3
			glfwWindowHint( GLFW_TITLEBAR, false );
#endif

			// NOTE(Yan): Undecorated windows are probably
			//            also desired, so make this an option
			//glfwWindowHint( GLFW_DECORATED, false );
			glfwWindowHint( GLFW_VISIBLE, GLFW_FALSE );
		}

		PlatformData.pWindow = glfwCreateWindow( uWidth, uHeight, pWindowsName, nullptr, nullptr );
		if ( PlatformData.pWindow == nullptr )
			return false;

		if ( PlatformData.bCustomTitleBar )
		{
			glfwSetWindowUserPointer( PlatformData.pWindow, &PlatformData );
#ifdef IM_THE_CHERNO_GLFW3
			glfwSetTitlebarHitTestCallback( PlatformData.pWindow,
											[]( GLFWwindow* window, int x, int y, int* hit ){
												PlatformDataImpl* app = ( PlatformDataImpl* )glfwGetWindowUserPointer( window );
												*hit = app->bTitleBarHovered;
											} );
#endif
		}

#elif (IM_CURRENT_PLATFORM) == IM_PLATFORM_APPLE)
#endif

		return true;
	}

	bool InitGfxAPI()
	{
		IM_ASSERT( ValidateFeatures() );
		return Backend::InitGfxAPI();
	}

	bool ShowWindow()
	{
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
		::ShowWindow( PlatformData.pHandle, SW_SHOWDEFAULT );
		::UpdateWindow( PlatformData.pHandle );
#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)

		glfwMakeContextCurrent( PlatformData.pWindow );
		glfwSwapInterval( 1 );

		glfwShowWindow( PlatformData.pWindow );

#elif (IM_CURRENT_PLATFORM) == IM_PLATFORM_APPLE)
#endif
		return true;
	}

	bool InitPlatform()
	{
		bool bGood = true;

#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)

#if (IM_CURRENT_GFX == IM_GFX_OPENGL3)
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

#if IM_CURRENT_GFX == IM_GFX_OPENGL3
		bGood = ImGui_ImplGlfw_InitForOpenGL( PlatformData.pWindow, true );
#ifdef __EMSCRIPTEN__
		ImGui_ImplGlfw_InstallEmscriptenCanvasResizeCallback( "#canvas" );
#endif
#elif IM_CURRENT_GFX == IM_GFX_VULKAN
		bGood = ImGui_ImplGlfw_InitForVulkan( PlatformData.pWindow, true );
#endif

#endif

		return bGood;
	}

	bool InitGfx()
	{
#if (IM_CURRENT_GFX == IM_GFX_OPENGL3)
		return ImGui_ImplOpenGL3_Init( PlatformData.pGLSLVersion );
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
		return ImGui_ImplDX11_Init( PlatformData.pD3DDevice, PlatformData.pD3DDeviceContext );
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
		// Use the new ImGui_ImplDX12_InitInfo structure for better API compatibility
		ImGui_ImplDX12_InitInfo init_info = {};
		init_info.Device = PlatformData.pD3DDevice;
		init_info.CommandQueue = PlatformData.pD3DCommandQueue;
		init_info.NumFramesInFlight = PlatformData.NUM_FRAMES_IN_FLIGHT;
		init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		init_info.DSVFormat = DXGI_FORMAT_D32_FLOAT;  // Standard depth format
		init_info.SrvDescriptorHeap = PlatformData.pD3DSRVDescHeap;

		// Use legacy single descriptor for backwards compatibility
		init_info.LegacySingleSrvCpuDescriptor = PlatformData.pD3DSRVDescHeap->GetCPUDescriptorHandleForHeapStart();
		init_info.LegacySingleSrvGpuDescriptor = PlatformData.pD3DSRVDescHeap->GetGPUDescriptorHandleForHeapStart();

		return ImGui_ImplDX12_Init( &init_info );
#elif (IM_CURRENT_GFX) == IM_GFX_VULKAN)
		// Initialize ImGui Vulkan backend
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = PlatformData.pInstance;
		init_info.PhysicalDevice = PlatformData.pPhysicalDevice;
		init_info.Device = PlatformData.pDevice;
		init_info.QueueFamily = PlatformData.uQueueFamily;
		init_info.Queue = PlatformData.pQueue;
		init_info.PipelineCache = PlatformData.pPipelineCache;
		init_info.DescriptorPool = PlatformData.pDescriptorPool;
		init_info.MinImageCount = PlatformData.uMinImageCount;
		init_info.ImageCount = PlatformData.oMainWindowData.ImageCount;
		init_info.Allocator = PlatformData.pAllocator;
		init_info.PipelineInfoMain.RenderPass = PlatformData.oMainWindowData.RenderPass;
		init_info.PipelineInfoMain.Subpass = 0;
		init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		init_info.CheckVkResultFn = check_vk_result;
		return ImGui_ImplVulkan_Init( &init_info );
#elif (IM_CURRENT_GFX) == IM_GFX_METAL)
		return ImGui_ImplMetal_Init(..);
#else
#error IM_CURRENT_TARGET not specified correctly
		return false;
#endif
	}

	void DestroyWindow()
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

	bool GfxCheck()
	{
		return Backend::GfxCheck();
	}

	void GfxViewportPre()
	{
		Backend::GfxViewportPre();
	}

	void GfxViewportPost()
	{
		Backend::GfxViewportPost();
	}

	//////////////////////////////////////////////////////////////////////////

	bool PlatformContinue()
	{
		return Platform::PlatformContinue();
	}

	bool PlatformEvents()
	{
		return Platform::PlatformEvents();
	}

	void GfxAPINewFrame()
	{
		Backend::GfxAPINewFrame();
	}

	void PlatformNewFrame()
	{
		Platform::PlatformNewFrame();
	}

	bool GfxAPIClear( ImVec4 const vClearColor )
	{
#if (IM_CURRENT_GFX != IM_GFX_DIRECTX12) && (IM_CURRENT_GFX != IM_GFX_VULKAN)
		ImGui::EndFrame();
#endif
		return Backend::GfxAPIClear( vClearColor );
	}

	bool GfxAPIRender( ImVec4 const vClearColor )
	{
		return Backend::GfxAPIRender( vClearColor );
	}

	bool GfxAPISwapBuffer()
	{
		return Backend::GfxAPISwapBuffer();
	}

	void ShutdownGfxAPI()
	{
		Backend::ShutdownGfxAPI();
	}

	void ShutdownPostGfxAPI()
	{
		Backend::ShutdownPostGfxAPI();
	}

	void ShutdownWindow()
	{
		Platform::ShutdownPlatform();
	}

	//////////////////////////////////////////////////////////////////////////
	void SetFeatures( ImPlatformFeatures features )
	{
		PlatformData.features = features;
	}

	void SetGLSLVersion( char const* glsl_version )
	{
#if (IM_CURRENT_GFX == IM_GFX_OPENGL3)
		PlatformData.pGLSLVersion = glsl_version;
#else
		(void)glsl_version; // Suppress unused parameter warning for non-OpenGL backends
#endif
	}

	void SetDX12FramesInFlight( int frames_in_flight )
	{
#if (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
		// Validate the range (typical values are 2-3)
		if ( frames_in_flight >= 2 && frames_in_flight <= 8 )
		{
			// Note: This would require modifying NUM_FRAMES_IN_FLIGHT to be configurable
			// For now, we keep it as a compile-time constant but this function is ready for future enhancement
		}
#else
		(void)frames_in_flight; // Suppress unused parameter warning for non-DX12 backends
#endif
	}

	void SetDX12RTVFormat( int rtv_format )
	{
#if (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
		// This would be used in the InitInfo structure during initialization
		// Implementation would store this for use in ImGui_ImplDX12_InitInfo
		(void)rtv_format; // Placeholder for now
#else
		(void)rtv_format; // Suppress unused parameter warning for non-DX12 backends
#endif
	}

	void SetDX12DSVFormat( int dsv_format )
	{
#if (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
		// This would be used in the InitInfo structure during initialization
		// Implementation would store this for use in ImGui_ImplDX12_InitInfo
		(void)dsv_format; // Placeholder for now
#else
		(void)dsv_format; // Suppress unused parameter warning for non-DX12 backends
#endif
	}

	bool ValidateFeatures()
	{
#ifdef IM_SUPPORT_CUSTOM_SHADER
		return true;
#else
		return ( PlatformData.features & ImPlatformFeatures_CustomShader ) == 0;
#endif
	}

	//////////////////////////////////////////////////////////////////////////
	bool SimpleStart( char const* pWindowsName, ImVec2 const vPos, ImU32 const uWidth, ImU32 const uHeight )
	{
		IM_ASSERT( ImPlatform::ValidateFeatures() );

		bool bGood = true;

		bGood &= ImPlatform::CreateWindow( pWindowsName, vPos, uWidth, uHeight );
		bGood &= ImPlatform::InitGfxAPI();
		bGood &= ImPlatform::ShowWindow();
		IMGUI_CHECKVERSION();
		bGood &= ImGui::CreateContext() != nullptr;

		return bGood;
	}

	bool SimpleInitialize( bool hasViewport )
	{
		bool bGood = true;

		if ( hasViewport )
		{
			ImGuiStyle& style = ImGui::GetStyle();
			style.WindowRounding = 0.0f;
			style.Colors[ ImGuiCol_WindowBg ].w = 1.0f;
		}

		bGood &= ImPlatform::InitPlatform();
		bGood &= ImPlatform::InitGfx();

		return bGood;
	}

	void SimpleFinish()
	{
		ImPlatform::ShutdownGfxAPI();
		ImPlatform::ShutdownWindow();

		ImGui::DestroyContext();

		ImPlatform::ShutdownPostGfxAPI();

		ImPlatform::DestroyWindow();
	}

	void SimpleBegin()
	{
		ImPlatform::GfxAPINewFrame();
		ImPlatform::PlatformNewFrame();

		ImGui::NewFrame();
	}

	void SimpleEnd( ImVec4 const vClearColor, bool hasViewport )
	{
		ImPlatform::GfxAPIClear( vClearColor );
		ImPlatform::GfxAPIRender( vClearColor );

		if ( hasViewport )
		{
			ImPlatform::GfxViewportPre();

#ifdef IMGUI_HAS_VIEWPORT
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
#endif

			ImPlatform::GfxViewportPost();
		}

		ImPlatform::GfxAPISwapBuffer();
	}

	namespace Internal
	{
		bool WindowResize()
		{
#if (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
			ImCleanupRenderTarget();
			PlatformData.pSwapChain->ResizeBuffers( 0, PlatformData.uResizeWidth, PlatformData.uResizeHeight, DXGI_FORMAT_UNKNOWN, 0 );
			PlatformData.uResizeWidth = PlatformData.uResizeHeight = 0;
			ImCreateRenderTarget();
#endif

			return true;
		}
	}
}
