
#include <imgui.h>
// Available target, not all tested.
// IM_TARGET_WIN32_DX9
// IM_TARGET_WIN32_DX10
// IM_TARGET_WIN32_DX11
// IM_TARGET_WIN32_DX12
// IM_TARGET_APPLE_METAL
// IM_TARGET_APPLE_OPENGL2
// IM_TARGET_GLFW_OPENGL2
// IM_TARGET_GLFW_OPENGL3
// IM_TARGET_GLFW_VULKAN
// IM_TARGET_GLFW_METAL

// Available Permutations:
//	OS
//		__DEAR_WIN__
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
//		__DEAR_METAL__

#define IM_PLATFORM_IMPLEMENTATION // It will include ImPlatform.cpp internally
// // Define target
//#define IM_CURRENT_TARGET IM_TARGET_WIN32_DX11
// Or
//#define IM_CURRENT_TARGET (IM_PLATFORM_WIN32 | IM_GFX_OPENGL3)
// Or a permutation, Not all permutation are valid for instance __DEAR_MAC__ + __DEAR_GFX_DX11__
#define __DEAR_WIN__
//#define __DEAR_GFX_DX12__
#define __DEAR_GFX_OGL3__
#include <ImPlatform.h>

int main()
{
	if ( !ImPlatform::ImInit( "ImPlatform Demo", 1024, 764 ) )
		return 1;

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

	ImPlatform::ImBegin();

	ImVec4 clear_color = ImVec4( 0.0f, 0.0f, 0.0f, 1.0f );
	while ( ImPlatform::ImPlatformContinue() )
	{
		bool quit = ImPlatform::ImPlatformEvents();
		if ( quit )
			break;

		if ( !ImPlatform::ImBeginFrame() )
		{
			continue;
		}

		// ImGui Code
		bool show = true;
		ImGui::ShowDemoWindow( &show );

		ImPlatform::ImEndFrame( clear_color );

		// Update and Render additional Platform Windows
		if ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();

			ImPlatform::ImGfxViewportPost();
		}

		ImPlatform::ImSwapGfx();
	}

	ImPlatform::ImEnd();

	return 0;
}