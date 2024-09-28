
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
// Available target, not all tested.
// IM_TARGET_WIN32_DX9
// IM_TARGET_WIN32_DX10
// IM_TARGET_WIN32_DX11
// IM_TARGET_WIN32_DX12
// IM_TARGET_WIN32_OPENGL3

// Available Permutations:
//	OS
//		__DEAR_WIN__
//		__DEAR_GLFW__
//		__DEAR_LINUX__
//		__DEAR_MAC__
//	Gfx API:
//		__DEAR_GFX_DX9__
//		__DEAR_GFX_DX10__
//		__DEAR_GFX_DX11__
//		__DEAR_GFX_DX12__
//		__DEAR_GFX_OGL2__
//		__DEAR_GFX_OGL3__
//		__DEAR_GFX_VULKAN__
//		__DEAR_GFX_METAL__

#define IM_PLATFORM_IMPLEMENTATION // It will include ImPlatform.cpp internally
// // Define target
//#define IM_CURRENT_TARGET IM_TARGET_WIN32_DX11
// Or
//#define IM_CURRENT_TARGET (IM_PLATFORM_WIN32 | IM_GFX_OPENGL3)
// Or a permutation, Not all permutation are valid for instance __DEAR_MAC__ + __DEAR_GFX_DX11__
//#define __DEAR_GLFW__
#define __DEAR_WIN__
//#define IM_GLFW3_AUTO_LINK
//#define __DEAR_GLFW__
//#define __DEAR_GFX_DX9__
//#define __DEAR_GFX_DX10__
#define __DEAR_GFX_DX11__
//#define __DEAR_GFX_DX12__
//#define __DEAR_GFX_OGL3__
//#define IM_CURRENT_TARGET IM_TARGET_GLFW_OPENGL3
//#define IM_THE_CHERNO_GLFW3
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
static bool g_bSimpleAPI		= true;
static bool g_bCustomTitleBar	= true;
//////////////////////////////////////////////////////////////////////////

struct param3
{
	ImVec4 col0;
	ImVec4 col1;
	ImVec2 uv_start;
	ImVec2 uv_end;
};

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
	unsigned char* data = ( unsigned char* )malloc( width * height * channel );
	unsigned char* white_data = ( unsigned char* )malloc( width * height * channel );
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

	if ( g_bCustomTitleBar )
		ImPlatform::EnableCustomTitleBar();

#ifdef IM_SUPPORT_CUSTOM_SHADER
	std::string source =
		"float2 P = uv - 0.5f;\n\
		P.y = -P.y;\n\
		float size = 1.0f;\n\
		float x = sqrt(2.0f) / 2.0f * ( P.x - P.y );\n\
		float y = sqrt(2.0f) / 2.0f * ( P.x + P.y );\n\
		float r1 = max( abs( x ), abs( y ) ) - size / 3.5f;\n\
		float r2 = length( P - sqrt( 2.0f ) / 2.0f * float2( +1.0f, -1.0f ) * size / 3.5f ) - size / 3.5f;\n\
		float r3 = length( P - sqrt( 2.0f ) / 2.0f * float2( -1.0f, -1.0f ) * size / 3.5f ) - size / 3.5f;\n\
		col_out.rgb = ( min( min( r1, r2 ), r3 ) < 0.0f ).xxx;\n";
	std::string source2 =
		"float2 P = uv - 0.5f;\n\
		P.y = -P.y;\n\
		float size = 1.0f;\n\
		float x = P.x;\n\
		float y = P.y;\n\
		float r1 = abs( x ) + abs( y ) - size / 2.0f;\n\
		float r2 = max( abs( x + size / 2.0f ), abs( y ) ) - size / 2;\n\
		float r3 = max( abs( x - size / 6.0f ) - size / 4.0f, abs( y ) - size / 4.0f );\n\
		col_out.rgb = ( min( r3, max( .75f * r1, r2 ) ) < 0.0f ).xxx;\n";
	std::string params3 =
		"float4 col0;\n\
		float4 col1;\n\
		float2 uv_start;\n\
		float2 uv_end;\n";
	std::string source3 =
		"float2 delta = uv_end - uv_start;\n\
		float2 d = normalize( delta );\n\
		float l = rcp( length( delta ) );\n\
		float2 c = uv - uv_start;\n\
		float t = saturate( dot( d, c ) * l );\n\
		col_out = lerp( col0, col1, t );\n";
	param3 p3;
	p3.col0 = ImVec4( 1.0f, 1.0f, 1.0f, 1.0f );
	p3.col1 = ImVec4( 0.0f, 1.0f, 1.0f, 0.0f );
	p3.uv_start = ImVec2(0.4f, 0.4f);
	p3.uv_end = ImVec2(0.45f, 0.45f);
#endif

	// ImPlatform::SimpleAPI
	if ( g_bSimpleAPI )
	{
		bool bGood;

		bGood = ImPlatform::SimpleStart( "ImPlatform Simple Demo", ImVec2( 100, 100 ), 1024, 764 );
		if ( !bGood )
		{
			fprintf( stderr, "ImPlatform: Cannot Simple Start." );
			return false;
		}

		// Setup Dear ImGui context
		ImGuiIO& io = ImGui::GetIO(); ( void )io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;	// Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;	// Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;		// Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;		// Enable Multi-Viewport / Platform Windows

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		//io.Fonts->AddFontFromFileTTF( "../extern/FiraCode/distr/ttf/FiraCode-Medium.ttf", 16.0f );

		// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
		ImGuiStyle& style = ImGui::GetStyle();

		bGood = ImPlatform::SimpleInitialize( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable );
		if ( !bGood )
		{
			fprintf( stderr, "ImPlatform : Cannot Initialize." );
			return false;
		}
		ImTextureID img = ImPlatform::CreateTexture2D( ( char* )data, width, height,
														 {
															ImPixelChannel_RGBA,
															ImPixelType_UInt8,
															ImTextureFiltering_Linear,
															ImTextureBoundary_Clamp,
															ImTextureBoundary_Clamp
														 } );
		ImTextureID img_white = ImPlatform::CreateTexture2D( ( char* )white_data, width, height,
														 {
															ImPixelChannel_RGBA,
															ImPixelType_UInt8,
															ImTextureFiltering_Linear,
															ImTextureBoundary_Clamp,
															ImTextureBoundary_Clamp
														 } );
#if IM_HAS_STBI
		stbi_image_free( data );
#else
		free( data );
#endif

#ifdef IM_SUPPORT_CUSTOM_SHADER
		ImDrawShader shader;
		shader = ImPlatform::CreateShader( source.c_str(), "", "", 0 );
		ImDrawShader shader2;
		shader2 = ImPlatform::CreateShader( source2.c_str(), "", "", 0);
		ImDrawShader shader3;
		shader3 = ImPlatform::CreateShader( source3.c_str(), params3.c_str(), "", sizeof( param3 ), &p3 );
#endif

		float t = 0.0f;
		ImVec4 clear_color = ImVec4( 0.461f, 0.461f, 0.461f, 1.0f );
		while ( ImPlatform::PlatformContinue() )
		{
			bool quit = ImPlatform::PlatformEvents();
			if ( quit )
				break;

			if ( !ImPlatform::GfxCheck() )
			{
				continue;
			}

			ImPlatform::SimpleBegin();

			if ( ImPlatform::CustomTitleBarEnabled() )
			{
				if ( ImPlatform::BeginCustomTitleBar( 32.0f ) )
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
				}
				ImPlatform::EndCustomTitleBar();

				// ImGui Code
				bool show = true;
				ImGui::ShowDemoWindow( &show );

#ifdef IM_SUPPORT_CUSTOM_SHADER
				{
					ImGui::Begin( "Custom Shader" );
						ImDrawList* draw = ImGui::GetWindowDrawList();
						ImVec2 cur = ImGui::GetCursorScreenPos();
						ImPlatform::BeginCustomShader( draw, shader );
						ImRect bb( cur, cur + ImGui::GetContentRegionAvail() );
						draw->AddImageQuad( img, bb.GetBL(), bb.GetBR(), bb.GetTR(), bb.GetTL(), ImVec2( 0, 0 ), ImVec2( 1, 0 ), ImVec2( 1, 1 ), ImVec2( 0, 1 ), IM_COL32_WHITE );
						ImPlatform::EndCustomShader( draw );
					ImGui::End();
				}
				{
					ImGui::Begin( "Custom Shader 2" );
						ImDrawList* draw = ImGui::GetWindowDrawList();
						ImVec2 cur = ImGui::GetCursorScreenPos();
						ImPlatform::BeginCustomShader( draw, shader2 );
						ImRect bb( cur, cur + ImGui::GetContentRegionAvail() );
						draw->AddImageQuad( img_white, bb.GetBL(), bb.GetBR(), bb.GetTR(), bb.GetTL(), ImVec2( 0, 0 ), ImVec2( 1, 0 ), ImVec2( 1, 1 ), ImVec2( 0, 1 ), IM_COL32( 255, 0, 0, 255 ) );
						ImPlatform::EndCustomShader( draw );
					ImGui::End();
				}
				{
					ImGui::Begin( "Custom Shader 3" );
						ImDrawList* draw = ImGui::GetWindowDrawList();
						ImVec2 cur = ImGui::GetCursorScreenPos();
						float sin0 = ImSin( t );
						p3.uv_start = ImVec2( 0.0f, sin0 * sin0 );
						p3.uv_end = ImVec2( 0.0f, 1.0f );
						ImPlatform::UpdateCustomShaderConstant( shader3, &p3 );
						ImPlatform::BeginCustomShader( draw, shader3 );
						ImRect bb( cur, cur + ImGui::GetContentRegionAvail() );
						draw->AddImageQuad( img_white, bb.GetBL(), bb.GetBR(), bb.GetTR(), bb.GetTL(), ImVec2( 0, 0 ), ImVec2( 1, 0 ), ImVec2( 1, 1 ), ImVec2( 0, 1 ), IM_COL32( 255, 255, 255, 255 ) );
						ImPlatform::EndCustomShader( draw );
					ImGui::End();
				}
#endif

				if ( ImGui::Begin( "Image" ) )
				{
					ImGui::Image( img, ImGui::GetContentRegionAvail() );
				}
				ImGui::End();
			}
			else
			{
				// ImGui Code
				bool show = true;
				ImGui::ShowDemoWindow( &show );

				if ( ImGui::Begin( "Image" ) )
				{
					ImGui::Image( img, ImGui::GetContentRegionAvail() );
				}
				ImGui::End();
			}

			ImPlatform::SimpleEnd( clear_color, io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable );

			t += ImGui::GetIO().DeltaTime;
		}

		ImPlatform::SimpleFinish();
	}
	else
	{
		// ImPlatform::ExplicitAPI
		bool bGood;

		bGood = ImPlatform::CreateWindow( "ImPlatform Demo", ImVec2( 100, 100 ), 1024, 764 );
		if ( !bGood )
		{
			fprintf( stderr, "ImPlatform: Cannot create window." );
			return false;
		}

		bGood = ImPlatform::InitGfxAPI();
		if ( !bGood )
		{
			fprintf( stderr, "ImPlatform: Cannot initialize the Graphics API." );
			return false;
		}

#ifdef IM_SUPPORT_CUSTOM_SHADER
		ImDrawShader shader;
		shader = ImPlatform::CreateShader( source.c_str(), "", "", 0 );
		ImDrawShader shader2;
		shader2 = ImPlatform::CreateShader( source2.c_str(), "", "", 0 );
#endif

		bGood = ImPlatform::ShowWindow();
		if ( !bGood )
		{
			fprintf( stderr, "ImPlatform: Cannot show the window." );
			return false;
		}

		IMGUI_CHECKVERSION();
		bGood = ImGui::CreateContext() != nullptr;
		if ( !bGood )
		{
			fprintf( stderr, "ImGui: Cannot create context." );
			return false;
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

		// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
		ImGuiStyle& style = ImGui::GetStyle();
		if ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
		{
			style.WindowRounding = 0.0f;
			style.Colors[ ImGuiCol_WindowBg ].w = 1.0f;
		}

		bGood = ImPlatform::InitPlatform();
		if ( !bGood )
		{
			fprintf( stderr, "ImPlatform: Cannot initialize platform." );
			return false;
		}
		bGood = ImPlatform::InitGfx();
		if ( !bGood )
		{
			fprintf( stderr, "ImPlatform: Cannot initialize graphics." );
			return false;
		}
		ImTextureID img = ImPlatform::CreateTexture2D( ( char* )data, width, height,
														 {
															ImPixelChannel_RGBA,
															ImPixelType_UInt8,
															ImTextureFiltering_Linear,
															ImTextureBoundary_Clamp,
															ImTextureBoundary_Clamp
														 } );
		ImTextureID img_white = ImPlatform::CreateTexture2D( ( char* )white_data, width, height,
															   {
																  ImPixelChannel_RGBA,
																  ImPixelType_UInt8,
																  ImTextureFiltering_Linear,
																  ImTextureBoundary_Clamp,
																  ImTextureBoundary_Clamp
															   } );
#if IM_HAS_STBI
		stbi_image_free( data );
#else
		free( data );
#endif

		ImVec4 clear_color = ImVec4( 0.461f, 0.461f, 0.461f, 1.0f );
		while ( ImPlatform::PlatformContinue() )
		{
			bool quit = ImPlatform::PlatformEvents();
			if ( quit )
				break;

			if ( !ImPlatform::GfxCheck() )
			{
				continue;
			}

			ImPlatform::GfxAPINewFrame();
			ImPlatform::PlatformNewFrame();

			ImGui::NewFrame();

			if ( ImPlatform::CustomTitleBarEnabled() )
			{
				if ( ImPlatform::BeginCustomTitleBar( 64.0f ) )
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
				ImPlatform::EndCustomTitleBar();
			}

			// ImGui Code
			bool show = true;
			ImGui::ShowDemoWindow( &show );

#ifdef IM_SUPPORT_CUSTOM_SHADER
			{
				ImGui::Begin( "Custom Shader" );
					ImDrawList* draw = ImGui::GetWindowDrawList();
					ImVec2 cur = ImGui::GetCursorScreenPos();
					ImPlatform::BeginCustomShader( draw, shader );
					ImRect bb( cur, cur + ImGui::GetContentRegionAvail() );
					draw->AddImageQuad( img, bb.GetBL(), bb.GetBR(), bb.GetTR(), bb.GetTL(), ImVec2( 0, 0 ), ImVec2( 1, 0 ), ImVec2( 1, 1 ), ImVec2( 0, 1 ), IM_COL32_WHITE );
					ImPlatform::EndCustomShader( draw );
				ImGui::End();
			}
			{
				ImGui::Begin( "Custom Shader 2" );
					ImDrawList* draw = ImGui::GetWindowDrawList();
					ImVec2 cur = ImGui::GetCursorScreenPos();
					ImPlatform::BeginCustomShader( draw, shader2 );
					ImRect bb( cur, cur + ImGui::GetContentRegionAvail() );
					draw->AddImageQuad( img_white, bb.GetBL(), bb.GetBR(), bb.GetTR(), bb.GetTL(), ImVec2( 0, 0 ), ImVec2( 1, 0 ), ImVec2( 1, 1 ), ImVec2( 0, 1 ), IM_COL32( 255, 0, 0, 255 ) );
					ImPlatform::EndCustomShader( draw );
				ImGui::End();
			}
#endif

			if ( ImGui::Begin( "Image" ) )
			{
				ImGui::Image( img, ImGui::GetContentRegionAvail() );
			}
			ImGui::End();

			ImPlatform::GfxAPIClear( clear_color );
			ImPlatform::GfxAPIRender( clear_color );

			// Update and Render additional Platform Windows
			if ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
			{
				ImPlatform::GfxViewportPre();

				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();

				ImPlatform::GfxViewportPost();
			}

			ImPlatform::GfxAPISwapBuffer();
		}

		ImPlatform::ShutdownGfxAPI();
		ImPlatform::ShutdownWindow();

		ImGui::DestroyContext();

		ImPlatform::ShutdownPostGfxAPI();

		ImPlatform::DestroyWindow();
	}

	return 0;
}
