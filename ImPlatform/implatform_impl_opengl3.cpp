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

	// Forward declare ImSetCustomShader from ImPlatform.cpp
	void ImSetCustomShader( const ImDrawList* parent_list, const ImDrawCmd* cmd );

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

	// Shader constants
	#define GL_VERTEX_SHADER 0x8B31
	#define GL_FRAGMENT_SHADER 0x8B30
	#define GL_COMPILE_STATUS 0x8B81
	#define GL_LINK_STATUS 0x8B82
	#define GL_INFO_LOG_LENGTH 0x8B84
	#define GL_UNIFORM_BUFFER 0x8A11
	#define GL_DYNAMIC_DRAW 0x88E8
	#define GL_ARRAY_BUFFER 0x8892

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
			// First create a temporary legacy context to load wglCreateContextAttribsARB
			HGLRC tempContext = wglCreateContext( data->hDC );
			if ( !tempContext )
			{
				fprintf( stderr, "Failed to create temporary OpenGL context\n" );
				return false;
			}

			if ( !wglMakeCurrent( data->hDC, tempContext ) )
			{
				fprintf( stderr, "Failed to make temporary OpenGL context current\n" );
				wglDeleteContext( tempContext );
				return false;
			}

			// Load wglCreateContextAttribsARB to create a modern OpenGL 4.5 context
			typedef HGLRC (WINAPI * PFNWGLCREATECONTEXTATTRIBSARBPROC) (HDC hDC, HGLRC hShareContext, const int *attribList);
			PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB =
				(PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");

			if ( wglCreateContextAttribsARB )
			{
				// Request OpenGL 4.5 Core Profile for GLSL 450 support
				int attribs[] = {
					0x2091, 4,  // WGL_CONTEXT_MAJOR_VERSION_ARB
					0x2092, 5,  // WGL_CONTEXT_MINOR_VERSION_ARB
					0x9126, 0x00000001,  // WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB
					0  // End
				};

				PlatformData.pRC = wglCreateContextAttribsARB( data->hDC, nullptr, attribs );
				wglMakeCurrent( nullptr, nullptr );
				wglDeleteContext( tempContext );

				if ( !PlatformData.pRC )
				{
					fprintf( stderr, "Failed to create OpenGL 4.5 context, falling back to legacy context\n" );
					PlatformData.pRC = wglCreateContext( data->hDC );
				}
			}
			else
			{
				// wglCreateContextAttribsARB not available, use legacy context
				PlatformData.pRC = tempContext;
			}

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

			// Set GLSL version string for ImGui to use
			PlatformData.pGLSLVersion = "#version 450";

			// Print OpenGL version for debugging
			// Note: glGetString needs to be loaded via wglGetProcAddress or the GL loader
			typedef const unsigned char* (APIENTRY *PFNGLGETSTRINGPROC)(unsigned int name);
			PFNGLGETSTRINGPROC glGetString_func = (PFNGLGETSTRINGPROC)wglGetProcAddress("glGetString");
			if (!glGetString_func)
			{
				// Try getting it from opengl32.dll directly (it's part of OpenGL 1.1)
				HMODULE opengl32 = GetModuleHandleA("opengl32.dll");
				if (opengl32)
					glGetString_func = (PFNGLGETSTRINGPROC)GetProcAddress(opengl32, "glGetString");
			}

			if (glGetString_func)
			{
				const char* gl_version = (const char*)glGetString_func( 0x1F02 /* GL_VERSION */ );
				const char* glsl_version = (const char*)glGetString_func( 0x8B8C /* GL_SHADING_LANGUAGE_VERSION */ );
				fprintf( stderr, "OpenGL version: %s\n", gl_version ? gl_version : "unknown" );
				fprintf( stderr, "GLSL version: %s\n", glsl_version ? glsl_version : "unknown" );
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

		// Shader and buffer methods
		ImDrawShader CreateShader( char const* vs_source, char const* ps_source,
									int sizeof_in_bytes_vs_constants, void* init_vs_data_constant,
									int sizeof_in_bytes_ps_constants, void* init_ps_data_constant )
		{
			ImDrawShader shader = {};
			shader.sizeof_in_bytes_vs_constants = sizeof_in_bytes_vs_constants;
			shader.sizeof_in_bytes_ps_constants = sizeof_in_bytes_ps_constants;
			shader.is_cpu_vs_data_dirty = false;
			shader.is_cpu_ps_data_dirty = false;

			// Prepend required GLSL extensions for separate sampler/texture objects
			// These extensions are core in OpenGL 4.2+ but some drivers need explicit enable
			const char* extension_header =
				"#extension GL_ARB_separate_shader_objects : require\n"
				"#extension GL_ARB_shading_language_420pack : require\n";

			// Build vertex shader source with extensions
			size_t vs_header_len = strlen(extension_header);
			size_t vs_source_len = strlen(vs_source);
			char* vs_with_extensions = (char*)IM_ALLOC(vs_header_len + vs_source_len + 1);

			// Find the #version line and insert extensions after it
			const char* vs_version_end = strstr(vs_source, "\n");
			if (vs_version_end)
			{
				size_t version_len = vs_version_end - vs_source + 1;
				memcpy(vs_with_extensions, vs_source, version_len);
				memcpy(vs_with_extensions + version_len, extension_header, vs_header_len);
				memcpy(vs_with_extensions + version_len + vs_header_len, vs_version_end + 1, vs_source_len - version_len);
				vs_with_extensions[version_len + vs_header_len + vs_source_len - version_len] = '\0';
			}
			else
			{
				// No version line found, just concatenate
				memcpy(vs_with_extensions, extension_header, vs_header_len);
				memcpy(vs_with_extensions + vs_header_len, vs_source, vs_source_len + 1);
			}

			// Compile vertex shader
			GLuint vs = glCreateShader( GL_VERTEX_SHADER );
			glShaderSource( vs, 1, &vs_with_extensions, nullptr );
			glCompileShader( vs );
			IM_FREE(vs_with_extensions);

			GLint success = 0;
			glGetShaderiv( vs, GL_COMPILE_STATUS, &success );
			if ( !success )
			{
				char infoLog[512];
				glGetShaderInfoLog( vs, 512, nullptr, infoLog );
				fprintf( stderr, "Vertex shader compilation failed: %s\n", infoLog );
				glDeleteShader( vs );
				return shader;
			}

			// Build fragment shader source with extensions
			size_t ps_header_len = strlen(extension_header);
			size_t ps_source_len = strlen(ps_source);
			char* ps_with_extensions = (char*)IM_ALLOC(ps_header_len + ps_source_len + 1);

			const char* ps_version_end = strstr(ps_source, "\n");
			if (ps_version_end)
			{
				size_t version_len = ps_version_end - ps_source + 1;
				memcpy(ps_with_extensions, ps_source, version_len);
				memcpy(ps_with_extensions + version_len, extension_header, ps_header_len);
				memcpy(ps_with_extensions + version_len + ps_header_len, ps_version_end + 1, ps_source_len - version_len);
				ps_with_extensions[version_len + ps_header_len + ps_source_len - version_len] = '\0';
			}
			else
			{
				memcpy(ps_with_extensions, extension_header, ps_header_len);
				memcpy(ps_with_extensions + ps_header_len, ps_source, ps_source_len + 1);
			}

			// Compile fragment shader (pixel shader in OpenGL terms)
			GLuint fs = glCreateShader( GL_FRAGMENT_SHADER );
			glShaderSource( fs, 1, &ps_with_extensions, nullptr );
			glCompileShader( fs );
			IM_FREE(ps_with_extensions);

			glGetShaderiv( fs, GL_COMPILE_STATUS, &success );
			if ( !success )
			{
				char infoLog[512];
				glGetShaderInfoLog( fs, 512, nullptr, infoLog );
				fprintf( stderr, "Fragment shader compilation failed: %s\n", infoLog );
				glDeleteShader( vs );
				glDeleteShader( fs );
				return shader;
			}

			// Link shaders into a program
			GLuint program = glCreateProgram();
			glAttachShader( program, vs );
			glAttachShader( program, fs );
			glLinkProgram( program );

			glGetProgramiv( program, GL_LINK_STATUS, &success );
			if ( !success )
			{
				char infoLog[512];
				glGetProgramInfoLog( program, 512, nullptr, infoLog );
				fprintf( stderr, "Shader program linking failed: %s\n", infoLog );
				glDeleteShader( vs );
				glDeleteShader( fs );
				glDeleteProgram( program );
				return shader;
			}

			// Clean up individual shaders (they're now linked into the program)
			glDeleteShader( vs );
			glDeleteShader( fs );

			shader.vs = (void*)(intptr_t)program;  // Store program ID in vs field
			shader.ps = nullptr;  // Not used in OpenGL (program contains both shaders)

			// Create uniform buffers (UBOs) if constants are needed
			if ( sizeof_in_bytes_vs_constants > 0 )
			{
				GLuint ubo;
				glGenBuffers( 1, &ubo );
				glBindBuffer( GL_UNIFORM_BUFFER, ubo );
				glBufferData( GL_UNIFORM_BUFFER, sizeof_in_bytes_vs_constants, nullptr, GL_DYNAMIC_DRAW );
				glBindBuffer( GL_UNIFORM_BUFFER, 0 );

				shader.vs_cst = (void*)(intptr_t)ubo;
				shader.cpu_vs_data = IM_ALLOC( sizeof_in_bytes_vs_constants );
				if ( init_vs_data_constant )
				{
					memcpy( shader.cpu_vs_data, init_vs_data_constant, sizeof_in_bytes_vs_constants );
					shader.is_cpu_vs_data_dirty = true;
				}
			}

			if ( sizeof_in_bytes_ps_constants > 0 )
			{
				GLuint ubo;
				glGenBuffers( 1, &ubo );
				glBindBuffer( GL_UNIFORM_BUFFER, ubo );
				glBufferData( GL_UNIFORM_BUFFER, sizeof_in_bytes_ps_constants, nullptr, GL_DYNAMIC_DRAW );
				glBindBuffer( GL_UNIFORM_BUFFER, 0 );

				shader.ps_cst = (void*)(intptr_t)ubo;
				shader.cpu_ps_data = IM_ALLOC( sizeof_in_bytes_ps_constants );
				if ( init_ps_data_constant )
				{
					memcpy( shader.cpu_ps_data, init_ps_data_constant, sizeof_in_bytes_ps_constants );
					shader.is_cpu_ps_data_dirty = true;
				}
			}

			return shader;
		}

		void ReleaseShader( ImDrawShader& shader )
		{
			if ( shader.vs )
			{
				GLuint program = (GLuint)(intptr_t)shader.vs;
				glDeleteProgram( program );
				shader.vs = nullptr;
			}

			if ( shader.vs_cst )
			{
				GLuint ubo = (GLuint)(intptr_t)shader.vs_cst;
				glDeleteBuffers( 1, &ubo );
				shader.vs_cst = nullptr;
			}

			if ( shader.ps_cst )
			{
				GLuint ubo = (GLuint)(intptr_t)shader.ps_cst;
				glDeleteBuffers( 1, &ubo );
				shader.ps_cst = nullptr;
			}

			if ( shader.cpu_vs_data )
			{
				IM_FREE( shader.cpu_vs_data );
				shader.cpu_vs_data = nullptr;
			}

			if ( shader.cpu_ps_data )
			{
				IM_FREE( shader.cpu_ps_data );
				shader.cpu_ps_data = nullptr;
			}
		}

		void BeginCustomShader( ImDrawList* draw, ImDrawShader& shader )
		{
			draw->AddCallback( ImDrawCallback_ResetRenderState, nullptr );
			draw->AddCallback( ImSetCustomShader, &shader );
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
