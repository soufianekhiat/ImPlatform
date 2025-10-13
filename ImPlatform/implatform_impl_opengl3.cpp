// ImPlatform OpenGL3 Backend Implementation - Pure C Style
// This file implements ImPlatform::Backend:: namespace functions for OpenGL3
// The linker resolves these at link-time based on which .cpp is included

#include "ImPlatformBackend.h"
#include "ImPlatform.h"
#include <imgui.h>
#include <imgui_internal.h>

// OpenGL includes
#include <backends/imgui_impl_opengl3.h>
#pragma comment( lib, "opengl32.lib" )

namespace ImPlatform
{
	extern PlatformDataImpl PlatformData;

	// Forward declare GL types and constants
	typedef unsigned int GLenum;
	typedef int GLint;
	typedef unsigned int GLuint;
	typedef int GLsizei;
	typedef float GLfloat;
	typedef unsigned char GLubyte;

	// GL constants
	#define GL_TEXTURE_2D 0x0DE1
	#define GL_RGBA 0x1908
	#define GL_UNSIGNED_BYTE 0x1401
	#define GL_UNSIGNED_SHORT 0x1403
	#define GL_UNSIGNED_INT 0x1405
	#define GL_FLOAT 0x1406
	#define GL_NEAREST 0x2600
	#define GL_LINEAR 0x2601
	#define GL_CLAMP_TO_EDGE 0x812F
	#define GL_REPEAT 0x2901
	#define GL_TEXTURE_MIN_FILTER 0x2801
	#define GL_TEXTURE_MAG_FILTER 0x2800
	#define GL_TEXTURE_WRAP_S 0x2802
	#define GL_TEXTURE_WRAP_T 0x2803
	#define GL_UNPACK_ALIGNMENT 0x0CF5
	#define GL_COLOR_BUFFER_BIT 0x00004000

	// Note: OpenGL functions are loaded by imgui_impl_opengl3.cpp
	// We don't need to declare them here - use the ones from ImGui's loader

	// WGL helper functions for Win32 OpenGL context creation
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
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
		{
			PlatformData.pRC = wglCreateContext( data->hDC );
			if ( !PlatformData.pRC )
			{
				fprintf( stderr, "Failed to create OpenGL context\n" );
				return false;
			}

			if ( !wglMakeCurrent( data->hDC, PlatformData.pRC ) )
			{
				fprintf( stderr, "Failed to make OpenGL context current\n" );
				wglDeleteContext( PlatformData.pRC );
				PlatformData.pRC = nullptr;
				return false;
			}
		}

		return true;
	}

	void ImCleanupDeviceWGL( HWND hWnd, PlatformDataImpl::WGL_WindowData* data )
	{
		wglMakeCurrent( nullptr, nullptr );
		::ReleaseDC( hWnd, data->hDC );
	}
#endif

	// Helper functions (internal to this file)
	namespace
	{
		void ImPixelTypeChannelToOGL( GLint* internalFormat, GLenum* format, ImPixelType const eType, ImPixelChannel const eChannel )
		{
			switch ( eChannel )
			{
			case ImPixelChannel_RGBA:
				*internalFormat = *format = GL_RGBA;
				break;
			default:
				fprintf( stderr, "ImChannelToOGL eChannel unsupported on OpenGL3 {IM_RGBA}\n" );
				break;
			}
		}

		void ImPixelTypeToOGL( GLenum* type, ImPixelType const eType )
		{
			switch ( eType )
			{
			case ImPixelType_UInt8:
				*type = GL_UNSIGNED_BYTE;
				break;
			case ImPixelType_UInt16:
				*type = GL_UNSIGNED_SHORT;
				break;
			case ImPixelType_UInt32:
				*type = GL_UNSIGNED_INT;
				break;
			case ImPixelType_Float32:
				*type = GL_FLOAT;
				break;
			default:
				fprintf( stderr, "ImCreateImage ImType unsupported on OpenGL3 {IM_UINT8, IM_UINT16, IM_UINT32, IM_FLOAT32}\n" );
				break;
			}
		}
	}

	// Implementation of Backend namespace for OpenGL3
	namespace Backend
	{
		bool InitGfxAPI()
		{
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
			if ( !ImCreateDeviceWGL( PlatformData.pHandle, &PlatformData.oMainWindow ) )
			{
				ImCleanupDeviceWGL( PlatformData.pHandle, &PlatformData.oMainWindow );
				::DestroyWindow( PlatformData.pHandle );

#ifdef UNICODE
				::UnregisterClassW( PlatformData.oWinStruct.lpszClassName, PlatformData.oWinStruct.hInstance );
#else
				::UnregisterClassA( PlatformData.oWinStruct.lpszClassName, PlatformData.oWinStruct.hInstance );
#endif
				return false;
			}
			wglMakeCurrent( PlatformData.oMainWindow.hDC, PlatformData.pRC );
#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
			glfwMakeContextCurrent( PlatformData.pWindow );
#endif
			return true;
		}

		bool InitGfx()
		{
			const char* glsl_version = PlatformData.pGLSLVersion ? PlatformData.pGLSLVersion : "#version 130";
			return ImGui_ImplOpenGL3_Init( glsl_version );
		}

		void ShutdownGfxAPI()
		{
			ImGui_ImplOpenGL3_Shutdown();
		}

		void ShutdownPostGfxAPI()
		{
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
			ImCleanupDeviceWGL( PlatformData.pHandle, &PlatformData.oMainWindow );
			wglDeleteContext( PlatformData.pRC );
#endif
		}

		void GfxAPINewFrame()
		{
			ImGui_ImplOpenGL3_NewFrame();
		}

		bool GfxAPIClear( ImVec4 const vClearColor )
		{
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
			glViewport( 0, 0, PlatformData.iWidth, PlatformData.iHeight );
#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
			int iWidth, iHeight;
			glfwGetFramebufferSize( PlatformData.pWindow, &iWidth, &iHeight );
			glViewport( 0, 0, iWidth, iHeight );
#endif
			glClearColor( vClearColor.x * vClearColor.w, vClearColor.y * vClearColor.w, vClearColor.z * vClearColor.w, vClearColor.w );
			glClear( GL_COLOR_BUFFER_BIT );
			return true;
		}

		bool GfxAPIRender( ImVec4 const vClearColor )
		{
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );
			return true;
		}

		bool GfxAPISwapBuffer()
		{
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
			::SwapBuffers( PlatformData.oMainWindow.hDC );
#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
			glfwSwapBuffers( PlatformData.pWindow );
#endif
			return true;
		}

		bool GfxCheck()
		{
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
			if ( ::IsIconic( PlatformData.pHandle ) )
			{
				::Sleep( 10 );
				return false;
			}
#endif
			return true;
		}

		void GfxViewportPre()
		{
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
			PlatformData.pBackupContext = glfwGetCurrentContext();
#endif
		}

		void GfxViewportPost()
		{
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
			// Restore the OpenGL rendering context to the main window DC, since platform windows might have changed it.
			wglMakeCurrent( PlatformData.oMainWindow.hDC, PlatformData.pRC );
#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
			glfwMakeContextCurrent( PlatformData.pBackupContext );
#endif
		}

		ImTextureID CreateTexture2D( char* pData, ImU32 const uWidth, ImU32 const uHeight, ImImageDesc const& oImgDesc )
		{
			GLint internalformat;
			GLenum format;
			ImPixelTypeChannelToOGL( &internalformat, &format, oImgDesc.eType, oImgDesc.eChannel );

			GLenum type;
			ImPixelTypeToOGL( &type, oImgDesc.eType );

			GLuint image_texture;
			glGenTextures( 1, &image_texture );
			glBindTexture( GL_TEXTURE_2D, image_texture );

			GLint minMag, magMag;
			switch ( oImgDesc.eFiltering )
			{
			case ImTextureFiltering_Nearest:
				minMag = magMag = GL_NEAREST;
				break;
			case ImTextureFiltering_Linear:
				minMag = magMag = GL_LINEAR;
				break;
			default:
				fprintf( stderr, "ImCreateImage eSampler unsupported on OpenGL {IM_NEAREST, IM_LINEAR}\n" );
				minMag = magMag = GL_LINEAR;
				break;
			}

			GLint wrapS = GL_CLAMP_TO_EDGE;
			GLint wrapT = GL_CLAMP_TO_EDGE;
			switch ( oImgDesc.eBoundaryU )
			{
			case ImTextureBoundary_Clamp:
				wrapS = GL_CLAMP_TO_EDGE;
				break;
			case ImTextureBoundary_Repeat:
				wrapS = GL_REPEAT;
				break;
			}
			switch ( oImgDesc.eBoundaryV )
			{
			case ImTextureBoundary_Clamp:
				wrapT = GL_CLAMP_TO_EDGE;
				break;
			case ImTextureBoundary_Repeat:
				wrapT = GL_REPEAT;
				break;
			}

			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minMag );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magMag );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT );

			glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
			glTexImage2D( GL_TEXTURE_2D, 0, internalformat, ( GLsizei )uWidth, ( GLsizei )uHeight, 0, format, type, pData );

			GLenum gl_error = glGetError();
			if ( gl_error != 0 )
			{
				fprintf( stderr, "OpenGL error 0x%x during texture creation\n", gl_error );
				glDeleteTextures( 1, &image_texture );
				return (ImTextureID)nullptr;
			}

			return ( ImTextureID )( intptr_t )image_texture;
		}

		void ReleaseTexture2D( ImTextureID id )
		{
			if ( id )
			{
				GLuint texture = ( GLuint )( intptr_t )id;
				glDeleteTextures( 1, &texture );
			}
		}

		// Shader and buffer methods - TODO: extract from ImPlatform.cpp
		ImDrawShader CreateShader( char const* vs_source, char const* ps_source,
									int sizeof_in_bytes_vs_constants, void* init_vs_data_constant,
									int sizeof_in_bytes_ps_constants, void* init_ps_data_constant )
		{
			// TODO: OpenGL shader creation
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
			return true;
		}
	}

} // namespace ImPlatform
