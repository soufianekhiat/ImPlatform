#pragma once

#ifndef __IM_PLATFORM_BACKEND_H__
#define __IM_PLATFORM_BACKEND_H__

#include <imgui.h>

// Forward declarations
struct ImDrawShader;
struct ImVertexBuffer;
struct ImIndexBuffer;
struct ImImageDesc;

namespace ImPlatform
{
	// Graphics Backend API - Pure C-style functions
	// The linker will resolve these based on which backend .cpp is compiled:
	//   - implatform_impl_dx11.cpp    -> DX11 implementations
	//   - implatform_impl_dx12.cpp    -> DX12 implementations
	//   - implatform_impl_opengl3.cpp -> OpenGL3 implementations
	//   - implatform_impl_vulkan.cpp  -> Vulkan implementations
	//
	// NO VIRTUAL FUNCTIONS - Pure compile-time/link-time resolution!

	namespace Backend
	{
		// Initialization & Shutdown
		bool			InitGfxAPI();
		bool			InitGfx();
		void			ShutdownGfxAPI();
		void			ShutdownPostGfxAPI();

		// Frame operations
		void			GfxAPINewFrame();
		bool			GfxAPIClear( ImVec4 const vClearColor );
		bool			GfxAPIRender( ImVec4 const vClearColor );
		bool			GfxAPISwapBuffer();
		bool			GfxCheck();

		// Viewport operations
		void			GfxViewportPre();
		void			GfxViewportPost();

		// Texture management
		ImTextureID		CreateTexture2D( char* pData, ImU32 const uWidth, ImU32 const uHeight, ImImageDesc const& oImgDesc );
		void			ReleaseTexture2D( ImTextureID id );

		// Shader management
		ImDrawShader	CreateShader( char const* vs_source, char const* ps_source,
									  int sizeof_in_bytes_vs_constants, void* init_vs_data_constant,
									  int sizeof_in_bytes_ps_constants, void* init_ps_data_constant );
		void			ReleaseShader( ImDrawShader& shader );
		void			BeginCustomShader( ImDrawList* draw, ImDrawShader& shader );
		void			UpdateCustomPixelShaderConstants( ImDrawShader& shader, void* ptr_to_constants );
		void			UpdateCustomVertexShaderConstants( ImDrawShader& shader, void* ptr_to_constants );
		void			EndCustomShader( ImDrawList* draw );

		// Vertex/Index buffer management
		void			CreateVertexBuffer( ImVertexBuffer*& buffer, int sizeof_vertex_buffer, int vertices_count );
		void			CreateVertexBufferAndGrow( ImVertexBuffer** buffer, int sizeof_vertex_buffer, int vertices_count, int growth_size );
		void			UpdateVertexBuffer( ImVertexBuffer** buffer, int sizeof_vertex_buffer, int vertices_count, void* cpu_data );
		void			ReleaseVertexBuffer( ImVertexBuffer** buffer );
		void			CreateIndexBuffer( ImIndexBuffer*& buffer, int indices_count );
		void			CreateIndexBufferAndGrow( ImIndexBuffer** buffer, int indices_count, int growth_size );
		void			UpdateIndexBuffer( ImIndexBuffer** buffer, int indices_count, void* cpu_data );
		void			ReleaseIndexBuffer( ImIndexBuffer** buffer );

		// Resize handling
		bool			WindowResize();
	}

	// Platform API - Pure C-style functions
	// The linker will resolve these based on which platform .cpp is compiled:
	//   - implatform_impl_win32.cpp -> Win32 implementations
	//   - implatform_impl_glfw.cpp  -> GLFW implementations
	//
	// NO VIRTUAL FUNCTIONS - Pure compile-time/link-time resolution!

	namespace Platform
	{
		// Window creation & management
		bool			CreateWindow( char const* pWindowsName, ImVec2 const vPos, ImU32 const uWidth, ImU32 const uHeight );
		bool			ShowWindow();
		void			DestroyWindow();

		// Platform initialization & shutdown
		bool			InitPlatform();
		void			ShutdownPlatform();

		// Event handling
		bool			PlatformContinue();
		bool			PlatformEvents();  // Returns true if quit requested
		void			PlatformNewFrame();

		// Window state queries
		bool			IsMaximized();

		// Custom title bar support
		void			MinimizeApp();
		void			MaximizeApp();
		void			CloseApp();
	}
}

#endif // __IM_PLATFORM_BACKEND_H__
