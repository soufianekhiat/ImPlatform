
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
#define __DEAR_GFX_DX9__
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
		}
	}
#endif

	if ( g_bCustomTitleBar )
		ImPlatform::ImEnableCustomTitleBar();

	// ImPlatform::SimpleAPI
	if ( g_bSimpleAPI )
	{
		bool bGood;

		bGood = ImPlatform::ImSimpleStart( "ImPlatform Simple Demo", ImVec2( 100, 100 ), 1024, 764 );
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

		bGood = ImPlatform::ImSimpleInitialize( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable );
		if ( !bGood )
		{
			fprintf( stderr, "ImPlatform : Cannot Initialize." );
			return false;
		}
		ImTextureID img = ImPlatform::ImCreateTexture2D( ( char* )data, width, height,
														 {
															ImPlatform::IM_RGBA,
															ImPlatform::IM_TYPE_UINT8,
															ImPlatform::IM_FILTERING_LINEAR,
															ImPlatform::IM_BOUNDARY_CLAMP,
															ImPlatform::IM_BOUNDARY_CLAMP
														 } );
#if IM_HAS_STBI
		stbi_image_free( data );
#else
		free( data );
#endif

		ImVec4 clear_color = ImVec4( 0.461f, 0.461f, 0.461f, 1.0f );
		while ( ImPlatform::ImPlatformContinue() )
		{
			bool quit = ImPlatform::ImPlatformEvents();
			if ( quit )
				break;

			if ( !ImPlatform::ImGfxCheck() )
			{
				continue;
			}

			ImPlatform::ImSimpleBegin();

			if ( ImPlatform::ImCustomTitleBarEnabled() )
			{
				if ( ImPlatform::ImBeginCustomTitleBar( 32.0f ) )
				{
					ImGui::Text( "ImPlatform with Custom Title Bar" );
					ImGui::SameLine();

					if ( ImGui::Button( "Minimize" ) )
						ImPlatform::ImMinimizeApp();
					ImGui::SameLine();

					if ( ImGui::Button( "Maximize" ) )
						ImPlatform::ImMaximizeApp();
					ImGui::SameLine();

					if ( ImGui::Button( "Close" ) )
						ImPlatform::ImCloseApp();
				}
				ImPlatform::ImEndCustomTitleBar();

				// ImGui Code
				bool show = true;
				ImGui::ShowDemoWindow( &show );

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

			ImPlatform::ImSimpleEnd( clear_color, io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable );
		}

		ImPlatform::ImSimpleFinish();
	}
	else
	{
		// ImPlatform::ExplicitAPI
		bool bGood;

		bGood = ImPlatform::ImCreateWindow( "ImPlatform Demo", ImVec2( 100, 100 ), 1024, 764 );
		if ( !bGood )
		{
			fprintf( stderr, "ImPlatform: Cannot create window." );
			return false;
		}

		bGood = ImPlatform::ImInitGfxAPI();
		if ( !bGood )
		{
			fprintf( stderr, "ImPlatform: Cannot initialize the Graphics API." );
			return false;
		}

		bGood = ImPlatform::ImShowWindow();
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

		bGood = ImPlatform::ImInitPlatform();
		if ( !bGood )
		{
			fprintf( stderr, "ImPlatform: Cannot initialize platform." );
			return false;
		}
		bGood = ImPlatform::ImInitGfx();
		if ( !bGood )
		{
			fprintf( stderr, "ImPlatform: Cannot initialize graphics." );
			return false;
		}
		ImTextureID img = ImPlatform::ImCreateTexture2D( ( char* )data, width, height,
														 {
															ImPlatform::IM_RGBA,
															ImPlatform::IM_TYPE_UINT8,
															ImPlatform::IM_FILTERING_LINEAR,
															ImPlatform::IM_BOUNDARY_CLAMP,
															ImPlatform::IM_BOUNDARY_CLAMP
														 } );
#if IM_HAS_STBI
		stbi_image_free( data );
#else
		free( data );
#endif

		ImVec4 clear_color = ImVec4( 0.461f, 0.461f, 0.461f, 1.0f );
		while ( ImPlatform::ImPlatformContinue() )
		{
			bool quit = ImPlatform::ImPlatformEvents();
			if ( quit )
				break;

			if ( !ImPlatform::ImGfxCheck() )
			{
				continue;
			}

			ImPlatform::ImGfxAPINewFrame();
			ImPlatform::ImPlatformNewFrame();

			ImGui::NewFrame();

			if ( ImPlatform::ImCustomTitleBarEnabled() )
			{
				if ( ImPlatform::ImBeginCustomTitleBar( 64.0f ) )
				{
					ImGui::Text( "ImPlatform with Custom Title Bar" );
					ImGui::SameLine();

					if ( ImGui::Button( "Minimize" ) )
						ImPlatform::ImMinimizeApp();
					ImGui::SameLine();

					if ( ImGui::Button( "Maximize" ) )
						ImPlatform::ImMaximizeApp();
					ImGui::SameLine();

					if ( ImGui::Button( "Close" ) )
						ImPlatform::ImCloseApp();
					ImGui::SameLine();
				}
				ImPlatform::ImEndCustomTitleBar();
			}

			// ImGui Code
			bool show = true;
			ImGui::ShowDemoWindow( &show );
			if ( ImGui::Begin( "Image" ) )
			{
				ImGui::Image( img, ImGui::GetContentRegionAvail() );
			}
			ImGui::End();

			ImPlatform::ImGfxAPIClear( clear_color );
			ImPlatform::ImGfxAPIRender( clear_color );

			// Update and Render additional Platform Windows
			if ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
			{
				ImPlatform::ImGfxViewportPre();

				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();

				ImPlatform::ImGfxViewportPost();
			}

			ImPlatform::ImGfxAPISwapBuffer();
		}

		ImPlatform::ImShutdownGfxAPI();
		ImPlatform::ImShutdownWindow();

		ImGui::DestroyContext();

		ImPlatform::ImShutdownPostGfxAPI();

		ImPlatform::ImDestroyWindow();
	}

	return 0;
}
