
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
#define __DEAR_GLFW__
//#define __DEAR_WIN__
#define __DEAR_GFX_OGL2__
//#define IM_CURRENT_TARGET IM_TARGET_GLFW_OPENGL3
#include <ImPlatform.h>

#include <stdio.h>

#define SIMPLE_API 0

int main()
{
	// ImPlatform::SimpleAPI
#if SIMPLE_API
	bool bGood;

	bGood = ImPlatform::ImSimpleStart( "ImPlatform Simple Demo", 1024, 764 );
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
		fprintf( stderr, "ImPlatform: Cannot Initialize." );
		return false;
	}

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

		// ImGui Code
		bool show = true;
		ImGui::ShowDemoWindow( &show );

		ImPlatform::ImSimpleEnd( clear_color, io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable );
	}

	ImPlatform::ImSimpleFinish();
#else
	// ImPlatform::ExplicitAPI
	bool bGood;

	bGood = ImPlatform::ImCreateWindow( "ImPlatform Demo", 1024, 764 );
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

		// ImGui Code
		bool show = true;
		ImGui::ShowDemoWindow( &show );

		//ImPlatform::ImEndFrame( clear_color );

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
#endif

	return 0;
}
