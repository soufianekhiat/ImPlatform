#ifndef IM_PLATFORM_IMPLEMENTATION
#include <ImPlatform.h>
#endif

//#include <string>
//#include <fstream>

#include <imgui_internal.h>

#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
#include <backends/imgui_impl_win32.cpp>
#define DIRECTINPUT_VERSION 0x0800
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
#if (IM_CURRENT_GFX != IM_GFX_OPENGL2) && (IM_CURRENT_GFX != IM_GFX_OPENGL3)
#include <backends/imgui_impl_glfw.cpp>
#endif
#ifdef IM_GLFW3_AUTO_LINK
#pragma comment( lib, "lib-vc2010-64/glfw3.lib" )
#endif
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

namespace ImPlatform
{
	PlatformDataImpl PlatformData;

#if (IM_CURRENT_GFX == IM_GFX_OPENGL2) || (IM_CURRENT_GFX == IM_GFX_OPENGL3)

	void ImPixelTypeChannelToOGL( GLint* internalFormat, GLenum* format, ImPixelType const eType, ImPixelChannel const eChannel )
	{
#if (IM_CURRENT_GFX == IM_GFX_OPENGL2)
		switch ( eChannel )
		{
		case ImPixelChannel_R:
			*internalformat = *format = GL_LUMINANCE;
			break;
		case ImPixelChannel_RG:
			*internalformat = *format = GL_LUMINANCE_ALPHA;
			break;
		case ImPixelChannel_RGB:
			*internalformat = *format = GL_RGB;
			break;
		case ImPixelChannel_RGBA:
			*internalformat = *format = GL_RGBA;
			break;
		default:
			fprintf( stderr, "ImChannelToOGL eChannel unsupported on OpenGL2 {IM_R, IM_RG, IM_RGB, IM_RGBA}\n" );
			break;
		}
#else
#if 1
		switch ( eChannel )
		{
		case ImPixelChannel_RGBA:
			*internalFormat = *format = GL_RGBA;
			break;
		default:
			fprintf( stderr, "ImChannelToOGL eChannel unsupported on OpenGL3 {IM_RGBA}\n" );
			break;
		}
#else
		switch ( eChannel )
		{
		case ImPixelChannel_R:
			*format = GL_RED;
			switch ( eType )
			{
			case ImPixelType_UInt8:
				*internalformat = GL_R8UI;
				break;
			case ImPixelType_UInt16:
				*internalformat = GL_R16UI;
				break;
			case ImPixelType_UInt32:
				*internalformat = GL_R32UI;
				break;
			case ImPixelType_Int8:
				*internalformat = GL_R8I;
				break;
			case ImPixelType_Int16:
				*internalformat = GL_R16I;
				break;
			case ImPixelType_Int32:
				*internalformat = GL_R32I;
				break;
			case ImPixelType_Float16:
				*internalformat = GL_R16F;
				break;
			case ImPixelType_Float32:
				*internalformat = GL_R32F;
				break;
			default:
				fprintf( stderr, "ImCreateImage ImType unsupported on OpenGL2 {IM_UINT8, IM_UINT16, IM_UINT32, IM_INT8, IM_INT16, IM_INT32, IM_FLOAT16, IM_FLOAT32}\n" );
				break;
			}
			break;
		case ImPixelChannel_RG:
			*format = GL_RG;
			switch ( eType )
			{
			case ImPixelType_UInt8:
				*internalformat = GL_RG8UI;
				break;
			case ImPixelType_UInt16:
				*internalformat = GL_RG16UI;
				break;
			case ImPixelType_UInt32:
				*internalformat = GL_RG32UI;
				break;
			case ImPixelType_Int8:
				*internalformat = GL_RG8I;
				break;
			case ImPixelType_Int16:
				*internalformat = GL_RG16I;
				break;
			case ImPixelType_Int32:
				*internalformat = GL_RG32I;
				break;
			case ImPixelType_Float16:
				*internalformat = GL_RG16F;
				break;
			case ImPixelType_Float32:
				*internalformat = GL_RG32F;
				break;
			default:
				fprintf( stderr, "ImCreateImage ImType unsupported on OpenGL2 {IM_UINT8, IM_UINT16, IM_UINT32, IM_INT8, IM_INT16, IM_INT32, IM_FLOAT16, IM_FLOAT32}\n" );
				break;
			}
			break;
		case ImPixelChannel_RGB:
			*format = GL_RGB;
			switch ( eType )
			{
			case ImPixelType_UInt8:
				*internalformat = GL_RGB8UI;
				break;
			case ImPixelType_UInt16:
				*internalformat = GL_RGB16UI;
				break;
			case ImPixelType_UInt32:
				*internalformat = GL_RGB32UI;
				break;
			case ImPixelType_Int8:
				*internalformat = GL_RGB8I;
				break;
			case ImPixelType_Int16:
				*internalformat = GL_RGB16I;
				break;
			case ImPixelType_Int32:
				*internalformat = GL_RGB32I;
				break;
			case ImPixelType_Float16:
				*internalformat = GL_RGB16F;
				break;
			case ImPixelType_Float32:
				*internalformat = GL_RGB32F;
				break;
			default:
				fprintf( stderr, "ImCreateImage ImType unsupported on OpenGL2 {IM_UINT8, IM_UINT16, IM_UINT32, IM_INT8, IM_INT16, IM_INT32, IM_FLOAT16, IM_FLOAT32}\n" );
				break;
			}
			break;
		case ImPixelChannel_RGBA:
			*format = GL_RGBA;
			switch ( eType )
			{
			case ImPixelType_UInt8:
				*internalformat = GL_RGBA8UI;
				break;
			case ImPixelType_UInt16:
				*internalformat = GL_RGBA16UI;
				break;
			case ImPixelType_UInt32:
				*internalformat = GL_RGBA32UI;
				break;
			case ImPixelType_Int8:
				*internalformat = GL_RGBA8I;
				break;
			case ImPixelType_Int16:
				*internalformat = GL_RGBA16I;
				break;
			case ImPixelType_Int32:
				*internalformat = GL_RGBA32I;
				break;
			case ImPixelType_Float16:
				*internalformat = GL_RGBA16F;
				break;
			case ImPixelType_Float32:
				*internalformat = GL_RGBA32F;
				break;
			default:
				fprintf( stderr, "ImCreateImage ImType unsupported on OpenGL2 {IM_UINT8, IM_UINT16, IM_UINT32, IM_INT8, IM_INT16, IM_INT32, IM_FLOAT16, IM_FLOAT32}\n" );
				break;
			}
			break;
		default:
			fprintf( stderr, "ImCreateImage eChannel unsupported on OpenGL3 {IM_R, IM_RG, IM_RGB, IM_RGBA}\n" );
			break;
		}
#endif
#endif
	}

	void ImPixelTypeToOGL( GLenum* type, ImPixelType const eType )
	{
#if (IM_CURRENT_GFX == IM_GFX_OPENGL2)
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
			fprintf( stderr, "ImCreateImage ImType unsupported on OpenGL2 {IM_UINT8, IM_UINT16, IM_UINT32, IM_FLOAT32}\n" );
			break;
		}
#else
#if 1
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
#else
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
		case ImPixelType_Int8:
			*type = GL_BYTE;
			break;
		case ImPixelType_Int16:
			*type = GL_SHORT;
			break;
		case ImPixelType_Int32:
			*type = GL_INT;
			break;
		case ImPixelType_Float16:
			*type = GL_HALF_FLOAT;
			break;
		case ImPixelType_Float32:
			*type = GL_FLOAT;
			break;
		default:
			fprintf( stderr, "ImCreateImage ImType unsupported on OpenGL2 {IM_UINT8, IM_UINT16, IM_UINT32, IM_INT8, IM_INT16, IM_INT32, IM_FLOAT32}\n" );
			break;
		}
#endif
#endif
	}

	//void ImBoundaryToOGL( GLint* wrap, ImTextureBoundary const eBoundary )
	//{
	//	switch ( eBoundary )
	//	{
	//	case IM_BOUNDARY_CLAMP:
	//		*wrap = GL_CLAMP_TO_EDGE;
	//		break;
	//	case IM_BOUNDARY_REPEAT:
	//		*wrap = GL_REPEAT;
	//		break;
	//	case IM_BOUNDARY_MIRROR:
	//		*wrapS = GL_MIRRORED_REPEAT;
	//	default:
	//		fprintf( stderr, "ImCreateImage eBoundaryU unsupported on OpenGL {IM_CLAMP, IM_REPEAT, IM_MIRROR_REPEAT}\n" );
	//		break;
	//	}
	//}

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9)
	ImS32 ImPixelTypeChannelToDx9( D3DFORMAT* internalformat, ImPixelType const eType, ImPixelChannel const eChannel )
	{
		int sizeofChannel = -1;
		switch ( eChannel )
		{
		case ImPixelChannel_R:
			switch ( eType )
			{
			case ImPixelType_UInt8:
			case ImPixelType_Int8:
				sizeofChannel = 1;
				*internalformat = D3DFMT_L8;
				break;
			case ImPixelType_UInt16:
			case ImPixelType_Int16:
				sizeofChannel = 2;
				*internalformat = D3DFMT_L16;
				break;
			case ImPixelType_Float16:
				sizeofChannel = 2;
				*internalformat = D3DFMT_R16F;
				break;
			case ImPixelType_Float32:
				sizeofChannel = 4;
				*internalformat = D3DFMT_R32F;
				break;
			default:
				fprintf( stderr, "ImPixelTypeChannelToDx9 ImType unsupported on Dx9 for Single channel image.\n" );
				break;
			}
			break;
		case ImPixelChannel_RG:
			switch ( eType )
			{
			case ImPixelType_UInt8:
			case ImPixelType_Int8:
				sizeofChannel = 1;
				*internalformat = D3DFMT_A8L8;
				break;
			case ImPixelType_UInt16:
			case ImPixelType_Int16:
				sizeofChannel = 2;
				*internalformat = D3DFMT_G16R16;
				break;
			case ImPixelType_UInt32:
			case ImPixelType_Int32:
				sizeofChannel = 4;
				*internalformat = D3DFMT_R3G3B2;
				break;
			case ImPixelType_Float16:
				sizeofChannel = 2;
				*internalformat = D3DFMT_G16R16F;
				break;
			case ImPixelType_Float32:
				sizeofChannel = 4;
				*internalformat = D3DFMT_G32R32F;
				break;
			default:
				fprintf( stderr, "ImPixelTypeChannelToDx9 ImType unsupported on Dx9 for RG channel image.\n" );
				break;
			}
			break;
		case ImPixelChannel_RGB:
			switch ( eType )
			{
			case ImPixelType_UInt8:
			case ImPixelType_Int8:
				sizeofChannel = 1;
				*internalformat = D3DFMT_R8G8B8;
				break;
			default:
				fprintf( stderr, "ImPixelTypeChannelToDx9 ImType unsupported on Dx9 for RGB channel image.\n" );
				break;
			}
			break;
		case ImPixelChannel_RGBA:
			switch ( eType )
			{
			case ImPixelType_UInt8:
			case ImPixelType_Int8:
				sizeofChannel = 1;
				*internalformat = D3DFMT_A8B8G8R8;
				break;
			case ImPixelType_UInt16:
			case ImPixelType_Int16:
				sizeofChannel = 2;
				*internalformat = D3DFMT_A16B16G16R16;
				break;
			case ImPixelType_Float16:
				sizeofChannel = 2;
				*internalformat = D3DFMT_A16B16G16R16F;
				break;
			case ImPixelType_Float32:
				sizeofChannel = 4;
				*internalformat = D3DFMT_A32B32G32R32F;
				break;
			default:
				fprintf( stderr, "ImPixelTypeChannelToDx9 ImType unsupported on Dx9 for RGBA channel image.\n" );
				break;
			}
			break;
		default:
			fprintf( stderr, "ImPixelTypeChannelToDx9 eChannel unsupported on Dx9 {IM_R, IM_RG, IM_RGB, IM_RGBA}\n" );
			break;
		}

		return sizeofChannel;
	}

	D3DTEXTUREADDRESS ImTextureBoundaryToDX9( ImTextureBoundary const eBoundary )
	{
		switch ( eBoundary )
		{
		case ImTextureBoundary_Clamp:
			return D3DTADDRESS_CLAMP;
		case ImTextureBoundary_Repeat:
			return D3DTADDRESS_WRAP;
		case ImTextureBoundary_Mirror:
			return D3DTADDRESS_MIRROR;
		default:
			fprintf( stderr, "ImTextureBoundaryToDX9 eBoundary unsupported on Dx9 {IM_CLAMP, IM_REPEAT, IM_MIRROR_REPEAT}\n" );
			return D3DTADDRESS_FORCE_DWORD;
		}
	}

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || (IM_CURRENT_GFX == IM_GFX_DIRECTX11) || (IM_CURRENT_GFX == IM_GFX_DIRECTX12)
	ImS32 ImPixelTypeChannelToDx10_11_12( DXGI_FORMAT* internalformat, ImPixelType const eType, ImPixelChannel const eChannel )
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
				fprintf( stderr, "ImPixelTypeChannelToDx10_11 ImType unsupported on Dx for Single channel image.\n" );
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
				fprintf( stderr, "ImPixelTypeChannelToDx10_11 ImType unsupported on Dx for RG channel image.\n" );
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
				fprintf( stderr, "ImPixelTypeChannelToDx10_11 ImType unsupported on Dx for RGB channel image.\n" );
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
				fprintf( stderr, "ImPixelTypeChannelToDx10_11 ImType unsupported on Dx for RGBA channel image.\n" );
				break;
			}
			break;
		default:
			fprintf( stderr, "ImPixelTypeChannelToDx9 eChannel unsupported on Dx9 {IM_R, IM_RG, IM_RGB, IM_RGBA}\n" );
			break;
			}

		return sizeofChannel;
	}
#endif

	ImTextureID	CreateTexture2D( char* pData, ImU32 const uWidth, ImU32 const uHeight, ImImageDesc const& oImgDesc )
	{
#if (IM_CURRENT_GFX == IM_GFX_OPENGL2) || (IM_CURRENT_GFX == IM_GFX_OPENGL3)

		GLint internalformat;
		GLenum format;
		ImPixelTypeChannelToOGL( &internalformat, &format, oImgDesc.eType, oImgDesc.eChannel );

		GLenum type;
		ImPixelTypeToOGL( &type, oImgDesc.eType );

		GLuint image_texture;
		glGenTextures( 1, &image_texture );
		glBindTexture( GL_TEXTURE_2D, image_texture );

		GLint minMag;
		GLint magMag;
		switch ( oImgDesc.eFiltering )
		{
		//case IM_FILTERING_POINT:
		//	minMag = magMag = GL_NEAREST;
		//	break;
		case ImTextureFiltering_Linear:
			minMag = magMag = GL_LINEAR;
			break;
		default:
			fprintf( stderr, "ImCreateImage eSampler unsupported on OpenGL {IM_LINEAR}\n" );
			//fprintf( stderr, "ImCreateImage eSampler unsupported on OpenGL {IM_NEAREST, IM_LINEAR}\n" );
			break;
		}

		//GLint wrapS;
		//GLint wrapT;
		//ImBoundaryToOGL( &wrapS, oImgDesc.eBoundaryU );
		//ImBoundaryToOGL( &wrapT, oImgDesc.eBoundaryV );

		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minMag );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magMag );
		//glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS );
		//glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT );

#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
		glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
#endif
		glTexImage2D( GL_TEXTURE_2D, 0, internalformat, ( GLsizei )uWidth, ( GLsizei )uHeight, 0, format, type, pData );

		return ( void* )( intptr_t )image_texture;

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9)

		D3DFORMAT internalformat;
		int iSizeOf = ImPixelTypeChannelToDx9( &internalformat, oImgDesc.eType, oImgDesc.eChannel );

		LPDIRECT3DTEXTURE9 pTexture;
		PlatformData.pD3DDevice->CreateTexture( uWidth, uHeight, 1, 0,
												internalformat, D3DPOOL_MANAGED, &pTexture, 0 );
		if ( !pTexture )
		{
			fprintf( stderr, "ImCreateImage wasn't able to create the texture on Dx9\n" );
		}

		D3DLOCKED_RECT rect;
		pTexture->LockRect( 0, &rect, 0, D3DLOCK_DISCARD );
		unsigned char* dest = static_cast< unsigned char* >( rect.pBits );
		memcpy( dest, &pData[ 0 ], iSizeOf * uWidth * uHeight * int( oImgDesc.eChannel ) );
		pTexture->UnlockRect( 0 );

		HRESULT hr = PlatformData.pD3DDevice->SetTexture( 0, pTexture );
		if ( hr != D3D_OK )
		{
			//handle error
		}

		D3DTEXTUREFILTERTYPE eFiltering;
		switch ( oImgDesc.eFiltering )
		{
		case ImTextureFiltering_Nearest:
			eFiltering = D3DTEXF_POINT;
			break;
		case ImTextureFiltering_Linear:
			eFiltering = D3DTEXF_LINEAR;
			break;
		default:
			fprintf( stderr, "ImCreateImage eSampler unsupported on Dx9 {IM_NEAREST, IM_LINEAR}\n" );
			break;
		}

		D3DTEXTUREADDRESS eBoundaryU = ImTextureBoundaryToDX9( oImgDesc.eBoundaryU );
		D3DTEXTUREADDRESS eBoundaryV = ImTextureBoundaryToDX9( oImgDesc.eBoundaryV );
		PlatformData.pD3DDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, eBoundaryU );
		PlatformData.pD3DDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, eBoundaryV );
		PlatformData.pD3DDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, eFiltering );
		PlatformData.pD3DDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, eFiltering );

		return pTexture;

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)

		ID3D10ShaderResourceView* out_srv = NULL;

		DXGI_FORMAT internalformat;
		int iSizeOf = ImPixelTypeChannelToDx10_11_12( &internalformat, oImgDesc.eType, oImgDesc.eChannel );

		// Create texture
		D3D10_TEXTURE2D_DESC desc;
		ZeroMemory( &desc, sizeof( desc ) );
		desc.Width = uWidth;
		desc.Height = uHeight;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = internalformat;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D10_USAGE_DEFAULT;
		desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;

		ID3D10Texture2D* pTexture = NULL;
		D3D10_SUBRESOURCE_DATA subResource;
		subResource.pSysMem = pData;
		subResource.SysMemPitch = desc.Width * int( oImgDesc.eChannel ) * iSizeOf;
		subResource.SysMemSlicePitch = 0;
		PlatformData.pD3DDevice->CreateTexture2D( &desc, &subResource, &pTexture );

		// Create texture view
		D3D10_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory( &srvDesc, sizeof( srvDesc ) );
		srvDesc.Format = internalformat;
		srvDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = desc.MipLevels;
		srvDesc.Texture2D.MostDetailedMip = 0;
		PlatformData.pD3DDevice->CreateShaderResourceView( pTexture, &srvDesc, &out_srv );
		pTexture->Release();

		// TODO add sampler
		//D3D10_SAMPLER_DESC desc;
		//ZeroMemory( &desc, sizeof( desc ) );
		//desc.Filter = D3D10_FILTER_MIN_MAG_MIP_LINEAR;
		//desc.AddressU = D3D10_TEXTURE_ADDRESS_WRAP;
		//desc.AddressV = D3D10_TEXTURE_ADDRESS_WRAP;
		//desc.AddressW = D3D10_TEXTURE_ADDRESS_WRAP;
		//desc.MipLODBias = 0.f;
		//desc.ComparisonFunc = D3D10_COMPARISON_ALWAYS;
		//desc.MinLOD = 0.f;
		//desc.MaxLOD = 0.f;
		//PlatformData.pD3DDevice->CreateSamplerState( &desc, &Sampler );

		return out_srv;

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)

		ID3D11ShaderResourceView* out_srv = NULL;

		DXGI_FORMAT internalformat;
		int iSizeOf = ImPixelTypeChannelToDx10_11_12( &internalformat, oImgDesc.eType, oImgDesc.eChannel );

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
		PlatformData.pD3DDevice->CreateTexture2D( &desc, &subResource, &pTexture );

		// Create texture view
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory( &srvDesc, sizeof( srvDesc ) );
		srvDesc.Format = internalformat;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = desc.MipLevels;
		srvDesc.Texture2D.MostDetailedMip = 0;
		PlatformData.pD3DDevice->CreateShaderResourceView( pTexture, &srvDesc, &out_srv );
		pTexture->Release();

		// TODO Sampler
		//D3D11_SAMPLER_DESC desc;
		//ZeroMemory( &desc, sizeof( desc ) );
		//desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		//desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		//desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		//desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		//desc.MipLODBias = 0.f;
		//desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		//desc.MinLOD = 0.f;
		//desc.MaxLOD = 0.f;
		//PlatformData.pD3DDevice->CreateSamplerState( &desc, &Sampler );

		return ( ImTextureID )( out_srv );

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)

		DXGI_FORMAT internalformat;
		int iSizeOf = ImPixelTypeChannelToDx10_11_12( &internalformat, oImgDesc.eType, oImgDesc.eChannel );

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
		PlatformData.pD3DDevice->CreateCommittedResource( &props, D3D12_HEAP_FLAG_NONE, &desc,
												 D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS( &pTexture ) );

		UINT uploadPitch = ( uWidth * ((int)oImgDesc.eChannel ) + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u ) & ~( D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u );
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
		HRESULT hr = PlatformData.pD3DDevice->CreateCommittedResource( &props, D3D12_HEAP_FLAG_NONE, &desc,
															  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS( &uploadBuffer ) );
		IM_ASSERT( SUCCEEDED( hr ) );

		void* mapped = nullptr;
		D3D12_RANGE range = { 0, uploadSize };
		hr = uploadBuffer->Map( 0, &range, &mapped );
		IM_ASSERT( SUCCEEDED( hr ) );
		for ( int y = 0; y < (int)uHeight; y++ )
			memcpy( ( void* )( ( uintptr_t )mapped + y * uploadPitch ), pData + y * uHeight * ((int)oImgDesc.eFiltering ), uWidth * ((int)oImgDesc.eChannel));
		uploadBuffer->Unmap( 0, &range );

		D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
		srcLocation.pResource = uploadBuffer;
		srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		srcLocation.PlacedFootprint.Footprint.Format = internalformat;
		srcLocation.PlacedFootprint.Footprint.Width = uWidth;
		srcLocation.PlacedFootprint.Footprint.Height = uHeight;
		srcLocation.PlacedFootprint.Footprint.Depth = 1;
		srcLocation.PlacedFootprint.Footprint.RowPitch = uploadPitch;

		D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
		dstLocation.pResource = pTexture;
		dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dstLocation.SubresourceIndex = 0;

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = pTexture;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

		ID3D12Fence* fence = nullptr;
		hr = PlatformData.pD3DDevice->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( &fence ) );
		IM_ASSERT( SUCCEEDED( hr ) );

		HANDLE event = CreateEvent( 0, 0, 0, 0 );
		IM_ASSERT( event != nullptr );

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.NodeMask = 1;

		ID3D12CommandQueue* cmdQueue = nullptr;
		hr = PlatformData.pD3DDevice->CreateCommandQueue( &queueDesc, IID_PPV_ARGS( &cmdQueue ) );
		IM_ASSERT( SUCCEEDED( hr ) );

		ID3D12CommandAllocator* cmdAlloc = nullptr;
		hr = PlatformData.pD3DDevice->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( &cmdAlloc ) );
		IM_ASSERT( SUCCEEDED( hr ) );

		ID3D12GraphicsCommandList* cmdList = nullptr;
		hr = PlatformData.pD3DDevice->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc, nullptr, IID_PPV_ARGS( &cmdList ) );
		IM_ASSERT( SUCCEEDED( hr ) );

		cmdList->CopyTextureRegion( &dstLocation, 0, 0, 0, &srcLocation, nullptr );
		cmdList->ResourceBarrier( 1, &barrier );

		hr = cmdList->Close();
		IM_ASSERT( SUCCEEDED( hr ) );

		cmdQueue->ExecuteCommandLists( 1, ( ID3D12CommandList* const* )&cmdList );
		hr = cmdQueue->Signal( fence, 1 );
		IM_ASSERT( SUCCEEDED( hr ) );

		fence->SetEventOnCompletion( 1, event );
		WaitForSingleObject( event, INFINITE );

		cmdList->Release();
		cmdAlloc->Release();
		cmdQueue->Release();
		CloseHandle( event );
		fence->Release();
		uploadBuffer->Release();

		// Create texture view
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory( &srvDesc, sizeof( srvDesc ) );
		srvDesc.Format = internalformat;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = desc.MipLevels;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		UINT handle_increment = PlatformData.pD3DDevice->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
		int descriptor_index = 1; // The descriptor table index to use (not normally a hard-coded constant, but in this case we'll assume we have slot 1 reserved for us)
		D3D12_CPU_DESCRIPTOR_HANDLE my_texture_srv_cpu_handle = PlatformData.pD3DSRVDescHeap->GetCPUDescriptorHandleForHeapStart();
		my_texture_srv_cpu_handle.ptr += ( handle_increment * descriptor_index );
		D3D12_GPU_DESCRIPTOR_HANDLE my_texture_srv_gpu_handle = PlatformData.pD3DSRVDescHeap->GetGPUDescriptorHandleForHeapStart();
		my_texture_srv_gpu_handle.ptr += ( handle_increment * descriptor_index );

		PlatformData.pD3DDevice->CreateShaderResourceView( pTexture, &srvDesc, my_texture_srv_cpu_handle );

		return reinterpret_cast< ImTextureID >( pTexture );
#endif
	}

	void		ReleaseTexture2D( ImTextureID id )
	{
#if (IM_CURRENT_GFX == IM_GFX_OPENGL2) || (IM_CURRENT_GFX == IM_GFX_OPENGL3)

		GLuint tex = ( GLuint )( intptr_t )id;
		glDeleteTextures( 1, &tex );

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9)

		( ( LPDIRECT3DTEXTURE9 )id )->Release();

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)

		( ( ID3D10ShaderResourceView* )id )->Release();

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)

		( ( ID3D11ShaderResourceView* )id )->Release();

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)

		( *reinterpret_cast< ID3D12Resource** >( &id ) )->Release();
		( *reinterpret_cast< ID3D12Resource** >( &id ) ) = NULL;

#endif
	}

	void*	InternalCreateDynamicConstantBuffer( int sizeof_in_bytes_constants,
										  void* init_data_constant )
	{
#if (IM_CURRENT_GFX == IM_GFX_OPENGL2)
#elif (IM_CURRENT_GFX == IM_GFX_OPENGL3)
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9)
#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)
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
#endif
	}

#pragma optimize( "", off )
	char* ReplaceFirst_return_allocated_buffer_free_after_use( char const* src, char const* src_end, char const* token, char const* token_end, char const* new_str, char const* new_str_end )
	{
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
PS_INPUT main(VS_INPUT input)\n\
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
float4 main(PS_INPUT input) : SV_Target\n\
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

	void	CreateDefaultPixelShaderSource
	( char** out_vs_source,
	  char** out_ps_source,
	  char const* ps_pre_functions,
	  char const* ps_params,
	  char const* ps_source,
	  bool multiply_with_texture )
	{
		CreateShaderSource( out_vs_source,
							out_ps_source,
							"",
							"",
"output.pos = mul( ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\n\
output.col = input.col;\n\
output.uv  = input.uv;\n",
							ps_pre_functions,
							ps_params,
							ps_source,
"float2 pos : POSITION;\n\
float4 col : COLOR0;\n\
float2 uv  : TEXCOORD0;\n",
"float4 pos : SV_POSITION;\n\
float4 col : COLOR0;\n\
float2 uv  : TEXCOORD0;\n",
							true );
	}

	ImDrawShader	CreateShader( char const* vs_source,
								  char const* ps_source,
								  int sizeof_in_bytes_vs_constants,
								  void* init_vs_data_constant,
								  int sizeof_in_bytes_ps_constants,
								  void* init_ps_data_constant )
	{
		void* vs_const = NULL;
		void* ps_const = NULL;

#if (IM_CURRENT_GFX == IM_GFX_OPENGL2)
#elif (IM_CURRENT_GFX == IM_GFX_OPENGL3)

		std::string glsl_version = std::string( "#version 130" );

		sVS = glsl_version + "\n" + "#define IM_SHADER_GLSL\n" + sVS;
		sPS = glsl_version + "\n" + "#define IM_SHADER_GLSL\n" + sPS;

#if 0
		{
			std::ofstream oFile( "shader_vs.hlsl" );
			oFile << sVS << std::endl;
			oFile.close();
		}
		{
			std::ofstream oFile( "shader_ps.hlsl" );
			oFile << sPS << std::endl;
			oFile.close();
		}
#endif

		ImGui_ImplOpenGL3_Data* bd = ImGui_ImplOpenGL3_GetBackendData();

		const GLchar* vertex_shader_with_version[ 2 ] = { bd->GlslVersionString, sVS.c_str() };
		GLuint vert_handle = glCreateShader( GL_VERTEX_SHADER );
		glShaderSource( vert_handle, 2, vertex_shader_with_version, nullptr );
		glCompileShader( vert_handle );
		CheckShader( vert_handle, "vertex shader" );

		const GLchar* fragment_shader_with_version[ 2 ] = { bd->GlslVersionString, sPS.c_str() };
		GLuint frag_handle = glCreateShader( GL_FRAGMENT_SHADER );
		glShaderSource( frag_handle, 2, fragment_shader_with_version, nullptr );
		glCompileShader( frag_handle );
		CheckShader( frag_handle, "fragment shader" );

		bd->ShaderHandle = glCreateProgram();
		glAttachShader( bd->ShaderHandle, vert_handle );
		glAttachShader( bd->ShaderHandle, frag_handle );
		glLinkProgram( bd->ShaderHandle );
		CheckProgram( bd->ShaderHandle, "shader program" );

		glDetachShader( bd->ShaderHandle, vert_handle );
		glDetachShader( bd->ShaderHandle, frag_handle );
		glDeleteShader( vert_handle );
		glDeleteShader( frag_handle );

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)

		ImGui_ImplDX10_Data* bd = ImGui_ImplDX10_GetBackendData();

		ID3D10VertexShader* pVertexShader = NULL;
		ID3D10PixelShader* pPixelShader = NULL;

		ID3D10Buffer* constant_buffer = NULL;
		if ( sizeof_in_bytes_constants > 0 )
		{
			D3D10_BUFFER_DESC desc;
			desc.ByteWidth = sizeof_in_bytes_constants;
			desc.Usage = D3D10_USAGE_DYNAMIC;
			desc.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
			desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
			desc.MiscFlags = 0;
			if ( init_data_constant != NULL )
			{
				D3D10_SUBRESOURCE_DATA init = D3D10_SUBRESOURCE_DATA{ 0 };
				init.pSysMem = init_data_constant;
				bd->pd3dDevice->CreateBuffer( &desc, &init, &constant_buffer );
			}
			else
			{
				bd->pd3dDevice->CreateBuffer( &desc, NULL, &constant_buffer );
			}
		}

		{
			std::ofstream oFile( "shader_vs.hlsl" );
			oFile << sVS << std::endl;
			oFile.close();
		}
		{
			std::ofstream oFile( "shader_ps.hlsl" );
			oFile << sPS << std::endl;
			oFile.close();
		}

		D3D_SHADER_MACRO macros[] = { "IM_SHADER_HLSL", "", NULL, NULL };

		ID3DBlob* vertexShaderBlob;
		if ( FAILED( D3DCompile( sVS.c_str(), sVS.size(), nullptr, nullptr, nullptr, "main", "vs_4_0", 0, 0, &vertexShaderBlob, nullptr ) ) )
			return { NULL, NULL, NULL, 0 };
		if ( bd->pd3dDevice->CreateVertexShader( vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(), &pVertexShader ) != S_OK )
		{
			vertexShaderBlob->Release();
			return { NULL, NULL, NULL, 0 };
		}

		ID3DBlob* pixelShaderBlob;
		if ( FAILED( D3DCompile( sPS.c_str(), sPS.size(), nullptr, nullptr, nullptr, "main", "ps_4_0", 0, 0, &pixelShaderBlob, nullptr ) ) )
			return { NULL, NULL, NULL, 0 };
		if ( bd->pd3dDevice->CreatePixelShader( pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize(), &pPixelShader ) != S_OK )
		{
			pixelShaderBlob->Release();
			return { NULL, NULL, NULL, 0 };
		}
		pixelShaderBlob->Release();

		void* cpu_data = NULL;
		if ( init_data_constant != NULL )
		{
			cpu_data = IM_ALLOC( sizeof_in_bytes_constants );
			memcpy( cpu_data, init_data_constant, sizeof_in_bytes_constants );
		}

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)

		ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();

		ID3D11VertexShader* pVertexShader = NULL;
		ID3D11PixelShader* pPixelShader = NULL;

		//{
		//	std::ofstream oFile( "shader_vs.hlsl" );
		//	oFile << sVS << std::endl;
		//	oFile.close();
		//}
		//{
		//	std::ofstream oFile( "shader_ps.hlsl" );
		//	oFile << sPS << std::endl;
		//	oFile.close();
		//}

		D3D_SHADER_MACRO macros[] = { "IM_SHADER_HLSL", "", NULL, NULL };

		ID3DBlob* vertexShaderBlob;

		ID3DBlob* pErrorBlob;
		if ( FAILED( D3DCompile( vs_source, strlen( vs_source ), nullptr, &macros[0], nullptr, "main", "vs_5_0", 0, 0, &vertexShaderBlob, &pErrorBlob)) )
		{
			int error_count = int( pErrorBlob->GetBufferSize() );
			printf( "%*s\n", error_count, ( char* )pErrorBlob->GetBufferPointer() );
			fflush( stdout );
			vertexShaderBlob->Release();
			return { NULL, NULL, NULL, 0 };
		}
		if ( bd->pd3dDevice->CreateVertexShader( vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(), nullptr, &pVertexShader ) != S_OK )
		{
			vertexShaderBlob->Release();
			return { NULL, NULL, NULL, 0 };
		}
		vertexShaderBlob->Release();

		ID3DBlob* pixelShaderBlob;
		if ( FAILED( D3DCompile( ps_source, strlen( ps_source ), nullptr, &macros[ 0 ], nullptr, "main", "ps_5_0", 0, 0, &pixelShaderBlob, &pErrorBlob ) ) )
		{
			int error_count = int( pErrorBlob->GetBufferSize() );
			printf( "%*s\n", error_count, ( char* )pErrorBlob->GetBufferPointer() );
			fflush( stdout );
			pixelShaderBlob->Release();
			return { NULL, NULL, NULL };
		}
		if ( bd->pd3dDevice->CreatePixelShader( pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize(), nullptr, &pPixelShader ) != S_OK )
		{
			vertexShaderBlob->Release();
			return { NULL, NULL, NULL, 0 };
		}
		pixelShaderBlob->Release();

		vs_const = InternalCreateDynamicConstantBuffer( sizeof_in_bytes_vs_constants, init_vs_data_constant );
		ps_const = InternalCreateDynamicConstantBuffer( sizeof_in_bytes_ps_constants, init_ps_data_constant );

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)

#endif

		void* cpu_vs_data = NULL;
		void* cpu_ps_data = NULL;
		if ( init_vs_data_constant != NULL )
		{
			cpu_vs_data = IM_ALLOC( sizeof_in_bytes_vs_constants );
			memcpy( cpu_vs_data, init_vs_data_constant, sizeof_in_bytes_vs_constants );
		}
		if ( init_ps_data_constant != NULL )
		{
			cpu_ps_data = IM_ALLOC( sizeof_in_bytes_ps_constants );
			memcpy( cpu_ps_data, init_ps_data_constant, sizeof_in_bytes_ps_constants );
		}

		return
		{
			pVertexShader, pPixelShader,
			vs_const, ps_const,
			cpu_vs_data, cpu_ps_data,
			sizeof_in_bytes_vs_constants, sizeof_in_bytes_ps_constants,
			false, false
		};
	}

	void		ReleaseShader( ImDrawShader& shader )
	{
#if (IM_CURRENT_GFX == IM_GFX_OPENGL2) || (IM_CURRENT_GFX == IM_GFX_OPENGL3)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)

		ID3D10VertexShader* pVertexShader = ( ID3D10VertexShader* )shader.vs;
		ID3D10PixelShader* pPixelShader = ( ID3D10PixelShader* )shader.ps;

		if ( pVertexShader )
			pVertexShader->Release();
		if ( pPixelShader )
			pPixelShader->Release();

		pVertexShader->Release();
		pPixelShader->Release();

		ID3D10Buffer* vs_buffer = ( ID3D10Buffer* )shader.vs_cst;
		ID3D10Buffer* ps_buffer = ( ID3D10Buffer* )shader.ps_cst;
		if ( vs_buffer )
			vs_buffer->Release();
		if ( ps_buffer )
			ps_buffer->Release();

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)

		ID3D11VertexShader* pVertexShader = ( ID3D11VertexShader* )shader.vs;
		ID3D11PixelShader* pPixelShader = ( ID3D11PixelShader* )shader.ps;

		if ( pVertexShader )
			pVertexShader->Release();
		if ( pPixelShader )
			pPixelShader->Release();

		ID3D11Buffer* vs_buffer = (ID3D11Buffer*)shader.vs_cst;
		ID3D11Buffer* ps_buffer = (ID3D11Buffer*)shader.ps_cst;
		if ( vs_buffer )
			vs_buffer->Release();
		if ( ps_buffer )
			ps_buffer->Release();

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)

#endif

		if ( shader.cpu_vs_data )
		{
			IM_FREE( shader.cpu_vs_data );
			shader.cpu_vs_data = NULL;
		}
		if ( shader.cpu_ps_data )
		{
			IM_FREE( shader.cpu_ps_data );
			shader.cpu_ps_data = NULL;
		}

	}

	void	CreateVertexBuffer( ImVertexBuffer*& vb, int sizeof_vertex_buffer, int vertices_count )
	{
#if (IM_CURRENT_GFX == IM_GFX_OPENGL2) || (IM_CURRENT_GFX == IM_GFX_OPENGL3)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)

		ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();

		//ImVertexBuffer* vb = buffer;

		if ( vb == NULL )
			vb = ( ImVertexBuffer* )IM_ALLOC( sizeof( ImVertexBuffer ) );

		vb->vertices_count = vertices_count;
		vb->sizeof_each_struct_in_bytes = sizeof_vertex_buffer;

		D3D11_BUFFER_DESC desc;
		memset( &desc, 0, sizeof( D3D11_BUFFER_DESC ) );
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.ByteWidth = vertices_count * sizeof_vertex_buffer;
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;
		//ID3D11Buffer* d3d11Buffer = ( ID3D11Buffer* )vb->gpu_buffer;
		HRESULT hr = bd->pd3dDevice->CreateBuffer( &desc, nullptr, ( ID3D11Buffer** )&vb->gpu_buffer );
		if ( hr != S_OK )
		{
			_com_error err( hr );
			LPCTSTR errMsg = err.ErrorMessage();
			OutputDebugString( errMsg );
		}

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)

#endif
	}

	void	CreateVertexBufferAndGrow( ImVertexBuffer** buffer, int sizeof_vertex_buffer, int vertices_count, int growth_size )
	{
#if (IM_CURRENT_GFX == IM_GFX_OPENGL2) || (IM_CURRENT_GFX == IM_GFX_OPENGL3)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)

		ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)

#endif

		ImVertexBuffer* vb = *buffer;

		if ( vb == NULL )
		{
			CreateVertexBuffer( *buffer, sizeof_vertex_buffer, vertices_count );
		}
		if ( vb->vertices_count < vertices_count )
		{
			if ( buffer )
			{
#if (IM_CURRENT_GFX == IM_GFX_OPENGL2) || (IM_CURRENT_GFX == IM_GFX_OPENGL3)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)

				ID3D11Buffer* d3d11Buffer = ( ID3D11Buffer* )vb->gpu_buffer;
				d3d11Buffer->Release();
				d3d11Buffer = nullptr;

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)

#endif
			}
			CreateVertexBuffer( *buffer, sizeof_vertex_buffer, vertices_count + growth_size );
		}
	}

	void	UpdateVertexBuffer( ImVertexBuffer** buffer, int sizeof_vertex_buffer, int vertices_count, void* cpu_data )
	{
		ImVertexBuffer* vb = *buffer;

#if (IM_CURRENT_GFX == IM_GFX_OPENGL2) || (IM_CURRENT_GFX == IM_GFX_OPENGL3)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)

		ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
		ID3D11DeviceContext* ctx = bd->pd3dDeviceContext;
		ID3D11Buffer* d3d11Buffer = ( ID3D11Buffer* )vb->gpu_buffer;

		D3D11_MAPPED_SUBRESOURCE vtx_resource;
		if ( ctx->Map( d3d11Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &vtx_resource ) != S_OK )
			return;

		//ImDrawVert* vtx_dst = ( ImDrawVert* )vtx_resource.pData;

		memcpy( vtx_resource.pData, cpu_data, vertices_count * sizeof_vertex_buffer );

		ctx->Unmap( d3d11Buffer, 0 );

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)

#endif
	}

	void	ReleaseVertexBuffer( ImVertexBuffer** buffer )
	{
		if ( buffer && *buffer )
		{
			ImVertexBuffer* vb = *buffer;
			ID3D11Buffer* d3d11Buffer = ( ID3D11Buffer* )vb->gpu_buffer;
			d3d11Buffer->Release();
			d3d11Buffer = nullptr;
			IM_FREE( vb );
			vb = NULL;
		}
	}

	void	CreateIndexBuffer( ImIndexBuffer*& ib, int indices_count )
	{
#if (IM_CURRENT_GFX == IM_GFX_OPENGL2) || (IM_CURRENT_GFX == IM_GFX_OPENGL3)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)

		ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();

		if ( ib == NULL )
			ib = ( ImIndexBuffer* )IM_ALLOC( sizeof( ImIndexBuffer ) );

		ib->indices_count = indices_count;
		if ( indices_count < ( 1 << 15 ) )
			ib->sizeof_each_index = sizeof( ImU16 );
		else
			ib->sizeof_each_index = sizeof( ImU32 );

		D3D11_BUFFER_DESC desc;
		memset( &desc, 0, sizeof( D3D11_BUFFER_DESC ) );
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.ByteWidth = indices_count * ib->sizeof_each_index;
		desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;
		//ID3D11Buffer* d3d11Buffer = ( ID3D11Buffer* )ib->gpu_buffer;
		HRESULT hr = bd->pd3dDevice->CreateBuffer( &desc, nullptr, ( ID3D11Buffer** )&ib->gpu_buffer );
		if ( hr != S_OK )
		{
			_com_error err( hr );
			LPCTSTR errMsg = err.ErrorMessage();
			OutputDebugString( errMsg );
		}

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)

#endif
	}

	void	CreateIndexBufferAndGrow( ImIndexBuffer** buffer, int indices_count, int growth_size )
	{
#if (IM_CURRENT_GFX == IM_GFX_OPENGL2) || (IM_CURRENT_GFX == IM_GFX_OPENGL3)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)

		ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)

#endif

		ImIndexBuffer* ib = *buffer;

		if ( ib == NULL )
		{
			CreateIndexBuffer( *buffer, indices_count );
		}
		if ( ib->indices_count < indices_count )
		{
			if ( buffer )
			{
#if (IM_CURRENT_GFX == IM_GFX_OPENGL2) || (IM_CURRENT_GFX == IM_GFX_OPENGL3)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)

				ID3D11Buffer* d3d11Buffer = ( ID3D11Buffer* )ib->gpu_buffer;
				d3d11Buffer->Release();
				d3d11Buffer = nullptr;

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)

#endif
			}
			CreateIndexBuffer( *buffer, indices_count + growth_size );
		}
	}

	void	UpdateIndexBuffer( ImIndexBuffer** buffer, int indices_count, void* cpu_data )
	{
		ImIndexBuffer* ib = *buffer;

#if (IM_CURRENT_GFX == IM_GFX_OPENGL2) || (IM_CURRENT_GFX == IM_GFX_OPENGL3)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX11)

		ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
		ID3D11DeviceContext* ctx = bd->pd3dDeviceContext;
		ID3D11Buffer* d3d11Buffer = ( ID3D11Buffer* )ib->gpu_buffer;

		D3D11_MAPPED_SUBRESOURCE idx_resource;
		if ( ctx->Map( d3d11Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &idx_resource ) != S_OK )
			return;

		//ImDrawVert* vtx_dst = ( ImDrawVert* )idx_resource.pData;

		memcpy( idx_resource.pData, cpu_data, indices_count * ib->sizeof_each_index );

		ctx->Unmap( d3d11Buffer, 0 );

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX12)

#endif
	}

	void	ReleaseIndexBuffer( ImIndexBuffer** buffer )
	{
		if ( buffer && *buffer )
		{
			ImIndexBuffer* ib = *buffer;
			ID3D11Buffer* d3d11Buffer = ( ID3D11Buffer* )ib->gpu_buffer;
			d3d11Buffer->Release();
			d3d11Buffer = nullptr;
			IM_FREE( ib );
			ib = NULL;
		}
	}

	void ImSetCustomShader( const ImDrawList* parent_list, const ImDrawCmd* cmd )
	{
#if (IM_CURRENT_GFX == IM_GFX_OPENGL2) || (IM_CURRENT_GFX == IM_GFX_OPENGL3)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX9)

#elif (IM_CURRENT_GFX == IM_GFX_DIRECTX10)

		ImDrawShader* shader = ( ImDrawShader* )cmd->UserCallbackData;
		ImGui_ImplDX10_Data* bd = ImGui_ImplDX10_GetBackendData();
		ID3D10Device* ctx = bd->pd3dDevice;
		ctx->VSSetShader( ( ID3D10VertexShader* )( shader->vs ) );
		ctx->PSSetShader( ( ID3D10PixelShader* )( shader->ps ) );
		if ( shader->ps_cst != NULL &&
			 shader->sizeof_in_bytes_ps_constants > 0 &&
			 shader->cpu_ps_data != NULL )
		{
			void* mapped_resource;
			if ( ( ( ID3D10Buffer* )shader.ps_cst )->Map( D3D10_MAP_WRITE_DISCARD, 0, &mapped_resource ) != S_OK )
				return;
			memcpy( mapped_resource, ptr_to_constants, shader.sizeof_in_bytes_ps_constants );
			( ( ID3D10Buffer* )shader.ps_cst )->Unmap();

			//D3D11_MAPPED_SUBRESOURCE mapped_resource;
			//if ( ctx->Map( ( ID3D11Buffer* )shader->cst, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_resource ) == S_OK )
			//{
			//	IM_ASSERT( mapped_resource.RowPitch == shader.sizeof_in_bytes_constants );
			//	memcpy( mapped_resource.pData, shader->cpu_data, shader->sizeof_in_bytes_constants );
			//	ctx->Unmap( ( ID3D11Buffer* )shader->cst, 0 );
			//}
		}
		ctx->PSSetConstantBuffers( 0, 1, ( ID3D10Buffer* const* )( &( shader->ps_cst ) ) );

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

#endif
	}

	void		InternalUpdateCustomPixelShaderConstants( int sizeof_in_bytes_constants, void* ptr_to_constants, void* cpu_data, bool& is_cpu_data_dirty )
	{
		if ( sizeof_in_bytes_constants <= 0 ||
			 ptr_to_constants == NULL )
			return;

		if ( cpu_data == NULL )
		{
			cpu_data = IM_ALLOC( sizeof_in_bytes_constants );
		}
		if ( ptr_to_constants != NULL )
		{
			memcpy( cpu_data, ptr_to_constants, sizeof_in_bytes_constants );
			is_cpu_data_dirty = true;
		}
	}
	void		UpdateCustomPixelShaderConstants( ImDrawShader& shader, void* ptr_to_constants )
	{
		InternalUpdateCustomPixelShaderConstants( shader.sizeof_in_bytes_ps_constants,
												  ptr_to_constants,
												  shader.cpu_ps_data,
												  shader.is_cpu_ps_data_dirty );
	}
	void		UpdateCustomVertexShaderConstants( ImDrawShader& shader, void* ptr_to_constants )
	{
		InternalUpdateCustomPixelShaderConstants( shader.sizeof_in_bytes_vs_constants,
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
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)

		return ( ( ( ( DWORD )GetWindowLong( PlatformData.pHandle, GWL_STYLE ) ) & ( WS_MAXIMIZE ) ) != 0L );

#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)

		return ( bool )glfwGetWindowAttrib( PlatformData.pWindow, GLFW_MAXIMIZED );

#elif (IM_CURRENT_PLATFORM) == IM_PLATFORM_APPLE)

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
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)

		ShowWindow( PlatformData.pHandle, SW_MINIMIZE );

#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)

		glfwIconifyWindow( PlatformData.pWindow );

#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_APPLE)
#error Not yet implemented supported
#else
#error Not yet implemented supported
#endif
	}

	void		MaximizeApp()
	{
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)

		if ( ImIsMaximized() )
			ShowWindow( PlatformData.pHandle, SW_RESTORE );
		else
			ShowWindow( PlatformData.pHandle, SW_SHOWMAXIMIZED );

#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)

		if ( ImIsMaximized() )
			glfwRestoreWindow( PlatformData.pWindow );
		else
			glfwMaximizeWindow( PlatformData.pWindow );

#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_APPLE)
#error Not yet implemented supported
#else
#error Not yet implemented supported
#endif
	}

	void		CloseApp()
	{
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)

		PostQuitMessage( WM_CLOSE );

#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)

		glfwSetWindowShouldClose( PlatformData.pWindow, true );

#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_APPLE)
#error Not yet implemented supported
#else
#error Not yet implemented supported
#endif
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
#if (IM_CURRENT_GFX == IM_GFX_DIRECTX10) || (IM_CURRENT_GFX == IM_GFX_DIRECTX11)
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
		/*if ( D3D11CreateDeviceAndSwapChain( nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, uCreateDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &oSwapDesc, &PlatformData.pSwapChain, &PlatformData.pD3DDevice, &featureLevel, &PlatformData.pD3DDeviceContext ) != S_OK )
			return false;*/
		HRESULT res = D3D11CreateDeviceAndSwapChain( nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, uCreateDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &oSwapDesc, &PlatformData.pSwapChain, &PlatformData.pD3DDevice, &featureLevel, &PlatformData.pD3DDeviceContext );
		if ( res == DXGI_ERROR_UNSUPPORTED ) // Try high-performance WARP software driver if hardware is not available.
			res = D3D11CreateDeviceAndSwapChain( nullptr, D3D_DRIVER_TYPE_WARP, nullptr, uCreateDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &oSwapDesc, &PlatformData.pSwapChain, &PlatformData.pD3DDevice, &featureLevel, &PlatformData.pD3DDeviceContext );
		if ( res != S_OK )
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
			//desc.NumDescriptors = 1;
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
#elif ((IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32) && (IM_CURRENT_GFX == IM_GFX_OPENGL2 || IM_CURRENT_GFX == IM_GFX_OPENGL3))

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

#if (IM_CURRENT_GFX == IM_GFX_OPENGL2) || (IM_CURRENT_GFX == IM_GFX_OPENGL3)

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

	bool InitGfx()
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
			Internal::WindowResize();
		}
#endif

		return true;
	}

	void GfxViewportPre()
	{
#if IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW
		PlatformData.pBackupContext = glfwGetCurrentContext();
#endif
	}

	void GfxViewportPost()
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

	bool PlatformContinue()
	{
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
		return PlatformData.oMessage.message != WM_QUIT;
#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
		return !glfwWindowShouldClose( PlatformData.pWindow );
#elif (IM_CURRENT_PLATFORM) == IM_PLATFORM_APPLE)
#endif
	}

	bool PlatformEvents()
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

	void GfxAPINewFrame()
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

	void PlatformNewFrame()
	{
#if (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
		ImGui_ImplWin32_NewFrame();
#elif (IM_CURRENT_PLATFORM == IM_PLATFORM_GLFW)
		ImGui_ImplGlfw_NewFrame();
#elif (IM_CURRENT_PLATFORM) == IM_PLATFORM_APPLE)
#endif
	}

	bool GfxAPIClear( ImVec4 const vClearColor )
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

	bool GfxAPIRender( ImVec4 const vClearColor )
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

	bool GfxAPISwapBuffer()
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

		HRESULT uResult = PlatformData.pSwapChain->Present( 1, 0 ); // Present without vsync
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

	void ShutdownGfxAPI()
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

	void ShutdownPostGfxAPI()
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

	void ShutdownWindow()
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
	void SetFeatures( ImPlatformFeatures features )
	{
		PlatformData.features = features;
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
