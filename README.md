# ImPlatform
ImPlatform aim to simplify the multiplatform development with Dear ImGui

ImPlatform had 2 API, "Simple" and "Explicit". Please to not mix and do not call Internal::API

Add support of CustomTitleBar, still WIP, not working (yet) with Docking.
Work on Win32 & GLFW.
For GLFW that need a fork from @TheCherno, the dev branch of glfw3 of https://github.com/TheCherno/glfw/tree/dev.

Window with Custom Title Bar:
![ImPlatformDemo_U66gRkAD7S](https://github.com/soufianekhiat/ImPlatform/assets/4236325/78c72221-209a-4640-90c2-77abff0983c2)

// TODO:
- [ O ] WIN32_OPENGL3
- [ O ] WIN32_DIRECTX9
- [ O ] WIN32_DIRECTX10
- [ X ] WIN32_DIRECTX11 // Buggy resize windows
- [ X ] WIN32_DIRECTX12 // Buggy resize windows
- [ X ] GLFW_OPENGL2 // Produce clear_color frame
- [ O ] GLFW_OPENGL3 // Do not work well with high DPI
- [   ] GLFW_VULKAN
- [   ] GLFW_EMSCRIPTEM_OPENGL3
- [   ] SDL2_DIRECTX11
- [   ] SDL2_OPENGL2
- [   ] SDL2_OPENGL3
- [   ] SDL2_SDLRENDERER2
- [   ] SDL2_VULKAN

### Incentivise development:

[<img src="https://c5.patreon.com/external/logo/become_a_patron_button@2x.png" alt="Become a Patron" width="150"/>](https://www.patreon.com/SoufianeKHIAT)

https://www.patreon.com/SoufianeKHIAT

## Rewrite the Dear ImGui Main Loop

### Simple API
```cpp
#include <imgui.h>

// It will include ImPlatform.cpp internally
#define IM_PLATFORM_IMPLEMENTATION
// Define target
//#define IM_CURRENT_TARGET IM_TARGET_WIN32_DX11
// Or
//#define IM_CURRENT_TARGET (IM_PLATFORM_WIN32 | IM_GFX_OPENGL3)
// Or a permutation
//      sidenote: Not all permutations are valid for instance:
//          __DEAR_MAC__ + __DEAR_GFX_DX11__ // Which is really sad
#define __DEAR_WIN__
#define __DEAR_GFX_DX11__
#include <ImPlatform.h>

int main()
{
	ImPlatform::ImEnableCustomTitleBar();

	// Simple API
	bool bGood;

	bGood = ImPlatform::ImSimpleStart( "ImPlatform Simple Demo", 1024, 764 );
	if ( !bGood )
	{
		printf( "ImPlatform: Cannot Simple Start." );
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
		printf( "ImPlatform: Cannot Initialize." );
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
		}
		ImPlatform::ImEndCustomTitleBar();

		// ImGui Code
		bool show = true;
		ImGui::ShowDemoWindow( &show );

		ImPlatform::ImSimpleEnd( clear_color, io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable );
	}

	ImPlatform::ImSimpleFinish();

	return 0;
}
```

### Explicit API
```cpp
#include <imgui.h>

// It will include ImPlatform.cpp internally
#define IM_PLATFORM_IMPLEMENTATION
// Define target
//#define IM_CURRENT_TARGET IM_TARGET_WIN32_DX11
// Or
//#define IM_CURRENT_TARGET (IM_PLATFORM_WIN32 | IM_GFX_OPENGL3)
// Or a permutation
//      sidenote: Not all permutations are valid for instance:
//          __DEAR_MAC__ + __DEAR_GFX_DX11__ // Which is really sad
#define __DEAR_WIN__
#define __DEAR_GFX_DX11__
#include <ImPlatform.h>

int main()
{
	ImPlatform::ImEnableCustomTitleBar();

	// ImPlatform::ExplicitAPI
	bool bGood;

	bGood = ImPlatform::ImCreateWindow( "ImPlatform Demo", 1024, 764 );

	if ( !bGood )
	{
		printf( "ImPlatform: Cannot create window." );
		return false;
	}

	bGood = ImPlatform::ImInitGfxAPI();
	if ( !bGood )
	{
		printf( "ImPlatform: Cannot initialize the Graphics API." );
		return false;
	}

	bGood = ImPlatform::ImShowWindow();
	if ( !bGood )
	{
		printf( "ImPlatform: Cannot show the window." );
		return false;
	}

	IMGUI_CHECKVERSION();
	bGood = ImGui::CreateContext() != nullptr;
	if ( !bGood )
	{
		printf( "ImGui: Cannot create context." );
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
		printf( "ImPlatform: Cannot initialize platform." );
		return false;
	}
	bGood = ImPlatform::ImInitGfx();
	if ( !bGood )
	{
		printf( "ImPlatform: Cannot initialize graphics." );
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
		}
		ImPlatform::ImEndCustomTitleBar();

		// ImGui Code
		bool show = true;
		ImGui::ShowDemoWindow( &show );

		//ImPlatform::ImEndFrame( clear_color );

		ImPlatform::ImGfxAPIClear( clear_color );
		ImPlatform::ImGfxAPIRender( clear_color );

		// Update and Render additional Platform Windows
		if ( io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable )
		{
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
```

## Implementation consideration
* ImPlatform rely on the docking branch
* All Platform specific are available for the user with `PlatformData`.
    * For instance:
        * `PlatformData.oWinStruct` is the `WNDCLASSEX` on the `IM_PLATFORM_WIN32` target.
        * `PlatformData.pD3DDevice` is the `ID3D12Device*` on the `IM_GFX_DIRECTX12` target.
        * ...

## Defined Platforms
```cpp
// GPU Targets
IM_GFX_OPENGL2      // Not Implememted Yet
IM_GFX_OPENGL3      // Tested with Win32
IM_GFX_DIRECTX9     // Tested with Win32
IM_GFX_DIRECTX10    // Tested with Win32
IM_GFX_DIRECTX11    // Tested with Win32
IM_GFX_DIRECTX12    // Tested with Win32
IM_GFX_VULKAN       // Not Implememted Yet
IM_GFX_METAL        // Not Implememted Yet
IM_GFX_WGPU         // Not Implememted Yet

// Window Target
IM_PLATFORM_WIN32   // Tested with {Dx{9, 10, 11, 12}, OGL3}
IM_PLATFORM_GLFW    // Not Implememted Yet
IM_PLATFORM_APPLE   // Not Implememted Yet
```

For the future:
```cpp
// Possible Permutation
IM_TARGET_WIN32_DX9     // Tested
IM_TARGET_WIN32_DX10    // Tested
IM_TARGET_WIN32_DX11    // Tested
IM_TARGET_WIN32_DX12    // Tested
IM_TARGET_WIN32_OGL3    // Tested
IM_TARGET_APPLE_METAL   // Not Implememted Yet
IM_TARGET_APPLE_OPENGL2 // Not Implememted Yet
IM_TARGET_GLFW_OPENGL2  // Not Implememted Yet
IM_TARGET_GLFW_OPENGL3  // Not Implememted Yet
IM_TARGET_GLFW_VULKAN   // Not Implememted Yet
IM_TARGET_GLFW_METAL    // Not Implememted Yet
```

## Future
The author (me: @SoufianeKHIAT) don't have large number device. So I will only support the Windows specific target, the laptop I have. I'll add glfw support, etc. But for the other targets PRs are open to add for isntance MacOS specific code.
