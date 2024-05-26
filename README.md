# ImPlatform
ImPlatform aim to simplify the multiplatform development with Dear ImGui

// TODO:
- [x] WIN32_OPENGL3
- [x] WIN32_DIRECTX9
- [x] WIN32_DIRECTX10
- [x] WIN32_DIRECTX11
- [x] WIN32_DIRECTX12
- [ ] GLFW_OPENGL2
- [ ] GLFW_OPENGL3
- [ ] GLFW_VULKAN
- [ ] SDL2_DIRECTX11
- [ ] SDL2_OPENGL2
- [ ] SDL2_OPENGL3
- [ ] SDL2_SDLRENDERER2
- [ ] SDL2_VULKAN

### Incentivise development:

[<img src="https://c5.patreon.com/external/logo/become_a_patron_button@2x.png" alt="Become a Patron" width="150"/>](https://www.patreon.com/SoufianeKHIAT)

https://www.patreon.com/SoufianeKHIAT

## Rewrite the Dear ImGui Main Loop

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
	if ( !ImPlatform::ImInit( "ImPlatform Demo", 1024, 764 ) )
		return 1;

	// Setup Dear ImGui context
	ImGuiIO& io = ImGui::GetIO(); ( void )io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

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
