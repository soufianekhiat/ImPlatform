
#ifdef _WIN32
extern "C" {
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>

// Platform and Graphics API are defined by the project configuration via:
// IM_CONFIG_PLATFORM and IM_CONFIG_GFX preprocessor defines
//
// Available configurations:
//   Win32_DX9        - Windows + DirectX 9
//   Win32_DX10       - Windows + DirectX 10
//   Win32_DX11       - Windows + DirectX 11
//   Win32_DX12       - Windows + DirectX 12
//   Win32_OpenGL3    - Windows + OpenGL 3
//   GLFW_OpenGL3     - GLFW + OpenGL 3
//   GLFW_Vulkan      - GLFW + Vulkan

#ifndef IM_CONFIG_PLATFORM
#error "IM_CONFIG_PLATFORM must be defined by project configuration"
#endif

#ifndef IM_CONFIG_GFX
#error "IM_CONFIG_GFX must be defined by project configuration"
#endif

// Define platform and graphics API from project configuration
#define IM_CURRENT_PLATFORM IM_CONFIG_PLATFORM
#define IM_CURRENT_GFX IM_CONFIG_GFX

// Define implementation to include the backend code
#define IMPLATFORM_IMPLEMENTATION

#include <ImPlatform.h>

#include <stdio.h>
#include <math.h>

#define IM_HAS_STBI 0

#if IM_HAS_STBI
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#endif

//////////////////////////////////////////////////////////////////////////
// Global Options
static bool g_bSimpleAPI		= false;
//////////////////////////////////////////////////////////////////////////

// Custom shader support not available in new API yet
#undef IM_SUPPORT_CUSTOM_SHADER

#ifdef IM_SUPPORT_CUSTOM_SHADER
struct param2
{
	ImVec4 col0;
	ImVec4 col1;
	ImVec2 uv_start;
	ImVec2 uv_end;
};

void CreateDearImGuiShaders( ImDrawShader* shader0,
							 ImDrawShader* shader1,
							 ImDrawShader* shader2,
							 param2* p2 )
{
	size_t data_size = 0;

#if IM_CURRENT_GFX == IM_GFX_DIRECTX11 || \
	IM_CURRENT_GFX == IM_GFX_DIRECTX12

#define IM_SHADER_ROOT "shaders/hlsl_src/"
#define IM_SHADER_EXT ".hlsl"

#elif IM_CURRENT_GFX == IM_GFX_OPENGL3

#define IM_SHADER_ROOT "shaders/glsl/"
#define IM_SHADER_VS_EXT "_vs.glsl"
#define IM_SHADER_PS_EXT "_ps.glsl"

#else
#error shader not supported
#endif

#if IM_CURRENT_GFX == IM_GFX_DIRECTX11 || IM_CURRENT_GFX == IM_GFX_DIRECTX12
	// DirectX: Load combined HLSL shaders
	// Load rotated_square shader (shader0)
	// Note: ImFileLoadToMemory adds padding=1 by default which adds a null terminator
	char* rotated_square_source = ( char * )ImFileLoadToMemory( IM_SHADER_ROOT "rotated_square" IM_SHADER_EXT, "rb", &data_size, 1 );
	if ( rotated_square_source )
	{
		*shader0 = ImPlatform::CreateShader( rotated_square_source, rotated_square_source, 0, NULL, 0, NULL );
		IM_FREE( rotated_square_source );
	}

	// Load arrow_shape shader (shader1)
	char* arrow_shape_source = ( char * )ImFileLoadToMemory( IM_SHADER_ROOT "arrow_shape" IM_SHADER_EXT, "rb", &data_size, 1 );
	if ( arrow_shape_source )
	{
		*shader1 = ImPlatform::CreateShader( arrow_shape_source, arrow_shape_source, 0, NULL, 0, NULL );
		IM_FREE( arrow_shape_source );
	}

	// Load linear_gradient shader (shader2) with constant buffer
	char* linear_gradient_source = ( char * )ImFileLoadToMemory( IM_SHADER_ROOT "linear_gradient" IM_SHADER_EXT, "rb", &data_size, 1 );
	if ( linear_gradient_source )
	{
		*shader2 = ImPlatform::CreateShader( linear_gradient_source, linear_gradient_source, 0, NULL, sizeof( param2 ), p2 );
		IM_FREE( linear_gradient_source );
	}
#elif IM_CURRENT_GFX == IM_GFX_OPENGL3
	// OpenGL: Load split vertex and fragment shader files
	// Load rotated_square shader (shader0)
	char* rotated_square_vs = ( char * )ImFileLoadToMemory( IM_SHADER_ROOT "rotated_square" IM_SHADER_VS_EXT, "rb", &data_size, 1 );
	char* rotated_square_ps = ( char * )ImFileLoadToMemory( IM_SHADER_ROOT "rotated_square" IM_SHADER_PS_EXT, "rb", &data_size, 1 );
	if ( rotated_square_vs && rotated_square_ps )
	{
		*shader0 = ImPlatform::CreateShader( rotated_square_vs, rotated_square_ps, 0, NULL, 0, NULL );
	}
	if ( rotated_square_vs ) IM_FREE( rotated_square_vs );
	if ( rotated_square_ps ) IM_FREE( rotated_square_ps );

	// Load arrow_shape shader (shader1)
	char* arrow_shape_vs = ( char * )ImFileLoadToMemory( IM_SHADER_ROOT "arrow_shape" IM_SHADER_VS_EXT, "rb", &data_size, 1 );
	char* arrow_shape_ps = ( char * )ImFileLoadToMemory( IM_SHADER_ROOT "arrow_shape" IM_SHADER_PS_EXT, "rb", &data_size, 1 );
	if ( arrow_shape_vs && arrow_shape_ps )
	{
		*shader1 = ImPlatform::CreateShader( arrow_shape_vs, arrow_shape_ps, 0, NULL, 0, NULL );
	}
	if ( arrow_shape_vs ) IM_FREE( arrow_shape_vs );
	if ( arrow_shape_ps ) IM_FREE( arrow_shape_ps );

	// Load linear_gradient shader (shader2) with constant buffer
	char* linear_gradient_vs = ( char * )ImFileLoadToMemory( IM_SHADER_ROOT "linear_gradient" IM_SHADER_VS_EXT, "rb", &data_size, 1 );
	char* linear_gradient_ps = ( char * )ImFileLoadToMemory( IM_SHADER_ROOT "linear_gradient" IM_SHADER_PS_EXT, "rb", &data_size, 1 );
	if ( linear_gradient_vs && linear_gradient_ps )
	{
		*shader2 = ImPlatform::CreateShader( linear_gradient_vs, linear_gradient_ps, 0, NULL, sizeof( param2 ), p2 );
	}
	if ( linear_gradient_vs ) IM_FREE( linear_gradient_vs );
	if ( linear_gradient_ps ) IM_FREE( linear_gradient_ps );
#endif
}

void DemoCustomShaders( ImDrawShader *shader0,
						ImDrawShader *shader1,
						ImDrawShader *shader2,
						param2 *p2,
						ImTextureID img, ImTextureID img_white,
						float t )
{
	ImVec2 cur;
	{
		ImGui::Begin( "Custom Shader 0" );
		ImDrawList *draw = ImGui::GetWindowDrawList();
		cur = ImGui::GetCursorScreenPos();
		ImPlatform::BeginCustomShader( draw, *shader0 );
		ImRect bb( cur, cur + ImGui::GetContentRegionAvail() );
		draw->AddImageQuad( img, bb.GetBL(), bb.GetBR(), bb.GetTR(), bb.GetTL(), ImVec2( 0, 0 ), ImVec2( 1, 0 ), ImVec2( 1, 1 ), ImVec2( 0, 1 ), IM_COL32_WHITE );
		ImPlatform::EndCustomShader( draw );
		ImGui::End();
	}
	{
		ImGui::Begin( "Custom Shader 1" );
		ImDrawList *draw = ImGui::GetWindowDrawList();
		cur = ImGui::GetCursorScreenPos();
		ImPlatform::BeginCustomShader( draw, *shader1 );
		ImRect bb( cur, cur + ImGui::GetContentRegionAvail() );
		draw->AddImageQuad( img_white, bb.GetBL(), bb.GetBR(), bb.GetTR(), bb.GetTL(), ImVec2( 0, 0 ), ImVec2( 1, 0 ), ImVec2( 1, 1 ), ImVec2( 0, 1 ), IM_COL32( 255, 0, 0, 255 ) );
		ImPlatform::EndCustomShader( draw );
		ImGui::End();
	}
	{
		ImGui::Begin( "Custom Shader 2" );
		ImDrawList *draw = ImGui::GetWindowDrawList();
		cur = ImGui::GetCursorScreenPos();
		float sin0 = ImSin( t );
		p2->uv_start = ImVec2( 0.0f, sin0 * sin0 );
		p2->uv_end = ImVec2( 0.0f, 1.0f );
		ImPlatform::UpdateCustomPixelShaderConstants( *shader2, p2 );
		ImPlatform::BeginCustomShader( draw, *shader2 );
		ImRect bb( cur, cur + ImGui::GetContentRegionAvail() );
		draw->AddImageQuad( img_white, bb.GetBL(), bb.GetBR(), bb.GetTR(), bb.GetTL(), ImVec2( 0, 0 ), ImVec2( 1, 0 ), ImVec2( 1, 1 ), ImVec2( 0, 1 ), IM_COL32( 255, 255, 255, 255 ) );
		ImPlatform::EndCustomShader( draw );
		ImGui::End();
	}
}
#endif

int main()
{
	// Setup for an image
	int width;
	int height;
	int channel;
#if IM_HAS_STBI
	stbi_uc* data = stbi_load( "MyImage01.jpg", &width, &height, &channel, STBI_rgb_alpha );
#else
	// If we don't have stb_image we create an image manually instead of loading the file.
	width = height = 256;
	channel = 4;
	unsigned char* data = ( unsigned char* )IM_ALLOC( width * height * channel );
	unsigned char* white_data = ( unsigned char* )IM_ALLOC( width * height * channel );
	for ( int j = 0; j < height; ++j )
	{
		for ( int i = 0; i < width; ++i )
		{
			float x = floorf( ( float )i / 16.0f );
			float y = floorf( ( float )j / 16.0f );
			float PatternMask = fmodf( x + fmodf( y, 2.0f ), 2.0f );
			unsigned char val = PatternMask ? 255 : 0;
			data[ channel * ( j * height + i ) + 0 ] = val;
			data[ channel * ( j * height + i ) + 1 ] = val;
			data[ channel * ( j * height + i ) + 2 ] = val;
			data[ channel * ( j * height + i ) + 3 ] = 0xFFu;
			white_data[ channel * ( j * height + i ) + 0 ] = 0xFFu;
			white_data[ channel * ( j * height + i ) + 1 ] = 0xFFu;
			white_data[ channel * ( j * height + i ) + 2 ] = 0xFFu;
			white_data[ channel * ( j * height + i ) + 3 ] = 0xFFu;
		}
	}
#endif

	float t = 0.0f;

	// Note: SimpleAPI is not available in the new C API yet
	// Using the explicit API instead
	if ( false ) // g_bSimpleAPI disabled - not implemented in new API
	{
		fprintf( stderr, "ImPlatform: Simple API not available in new C API yet.\n" );
		return 1;
	}
	else
	{
		// Using the new C API
		bool bGood;

		bGood = ImPlatform_CreateWindow( "ImPlatform Demo", ImVec2( 100, 100 ), 1024, 764 );
		if ( !bGood )
		{
			fprintf( stderr, "ImPlatform: Cannot create window.\n" );
			return 1;
		}

		bGood = ImPlatform_InitGfxAPI();
		if ( !bGood )
		{
			fprintf( stderr, "ImPlatform: Cannot initialize the Graphics API.\n" );
			return 1;
		}

		bGood = ImPlatform_ShowWindow();
		if ( !bGood )
		{
			fprintf( stderr, "ImPlatform: Cannot show the window.\n" );
			return 1;
		}

		IMGUI_CHECKVERSION();
		bGood = ImGui::CreateContext() != nullptr;
		if ( !bGood )
		{
			fprintf( stderr, "ImGui: Cannot create context.\n" );
			return 1;
		}

		// Setup Dear ImGui context
		ImGuiIO& io = ImGui::GetIO(); ( void )io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;	// Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;	// Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;		// Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;		// Enable Multi-Viewport / Platform Windows
		////io.ConfigViewportsNoAutoMerge = true;
		////io.ConfigViewportsNoTaskBarIcon = true;

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		//ImGui::StyleColorsClassic();

		//io.Fonts->AddFontFromFileTTF( "../extern/FiraCode/distr/ttf/FiraCode-Medium.ttf", 16.0f );

		// Setup DPI scaling (Win32 only)
		ImGuiStyle& style = ImGui::GetStyle();
#if defined(IM_CURRENT_PLATFORM) && (IM_CURRENT_PLATFORM == IM_PLATFORM_WIN32)
		float dpi_scale = ImPlatform_App_GetDpiScale_Win32();
		style.ScaleAllSizes(dpi_scale);
		style.FontScaleDpi = dpi_scale;
		io.ConfigDpiScaleFonts = true;
		io.ConfigDpiScaleViewports = true;
#endif

		// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
		if ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
		{
			style.WindowRounding = 0.0f;
			style.Colors[ ImGuiCol_WindowBg ].w = 1.0f;
		}

		bGood = ImPlatform_InitPlatform();
		if ( !bGood )
		{
			fprintf( stderr, "ImPlatform: Cannot initialize platform.\n" );
			return 1;
		}
		bGood = ImPlatform_InitGfx();
		if ( !bGood )
		{
			fprintf( stderr, "ImPlatform: Cannot initialize graphics.\n" );
			return 1;
		}

		// Note: Texture creation API not yet available in new C API
		// ImTextureID img = NULL;
		// ImTextureID img_white = NULL;

#if IM_HAS_STBI
		stbi_image_free( data );
#else
		IM_FREE( data );
#endif

		ImVec4 clear_color = ImVec4( 0.461f, 0.461f, 0.461f, 1.0f );
		while ( ImPlatform_PlatformContinue() )
		{
			ImPlatform_PlatformEvents();

			if ( !ImPlatform_GfxCheck() )
			{
				continue;
			}

			ImPlatform_GfxAPINewFrame();
			ImPlatform_PlatformNewFrame();

			ImGui::NewFrame();

			// ImGui Demo Code
			bool show = true;
			ImGui::ShowDemoWindow( &show );

			ImGui::Render();

			ImPlatform_GfxAPIClear( clear_color );
			ImPlatform_GfxAPIRender( clear_color );

			// Update and Render additional Platform Windows
			ImPlatform_GfxViewportPre();
			ImPlatform_GfxViewportPost();

			t += ImGui::GetIO().DeltaTime;

			ImPlatform_GfxAPISwapBuffer();
		}

		ImPlatform_ShutdownGfxAPI();
		ImPlatform_ShutdownWindow();

		ImGui::DestroyContext();

		ImPlatform_ShutdownPostGfxAPI();

		ImPlatform_DestroyWindow();
	}

	return 0;
}
