# ImPlatform

ImPlatform is a platform abstraction layer that simplifies multiplatform development with Dear ImGui by providing a unified C API for window management, graphics initialization, and rendering across different platforms and graphics APIs.

## Features

- **Clean C API**: Simple, consistent function naming (`ImPlatform_*`)
- **Multiple Platform Support**: Win32, GLFW, SDL2, SDL3, Apple (macOS/iOS)
- **Multiple Graphics API Support**: OpenGL 3, DirectX 9/10/11/12, Vulkan, Metal, WebGPU
- **Header-Only Implementation**: Single header include with implementation macro
- **Multi-Viewport Support**: Built-in support for ImGui's multi-viewport feature
- **Flexible Configuration**: Define platform and graphics API at compile time

## Quick Start

### Basic Usage

```cpp
#include <imgui.h>

// Define platform and graphics API before including ImPlatform.h
#define IM_CURRENT_PLATFORM IM_PLATFORM_WIN32
#define IM_CURRENT_GFX IM_GFX_OPENGL3

// Define implementation to include the backend code
#define IMPLATFORM_IMPLEMENTATION

#include <ImPlatform.h>

int main()
{
    // Create window
    if (!ImPlatform_CreateWindow("My Application", ImVec2(100, 100), 1280, 720))
        return 1;

    // Initialize graphics API
    if (!ImPlatform_InitGfxAPI())
        return 1;

    // Show window
    if (!ImPlatform_ShowWindow())
        return 1;

    // Create ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Setup Dear ImGui
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();

    // Initialize platform and graphics backends
    if (!ImPlatform_InitPlatform())
        return 1;
    if (!ImPlatform_InitGfx())
        return 1;

    // Main loop
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    while (ImPlatform_PlatformContinue())
    {
        ImPlatform_PlatformEvents();

        if (!ImPlatform_GfxCheck())
            continue;

        ImPlatform_GfxAPINewFrame();
        ImPlatform_PlatformNewFrame();
        ImGui::NewFrame();

        // Your ImGui code here
        ImGui::ShowDemoWindow();

        ImGui::Render();
        ImPlatform_GfxAPIClear(clear_color);
        ImPlatform_GfxAPIRender(clear_color);

        // Multi-viewport support
        ImPlatform_GfxViewportPre();
        ImPlatform_GfxViewportPost();

        ImPlatform_GfxAPISwapBuffer();
    }

    // Cleanup
    ImPlatform_ShutdownGfxAPI();
    ImPlatform_ShutdownWindow();
    ImGui::DestroyContext();
    ImPlatform_ShutdownPostGfxAPI();
    ImPlatform_DestroyWindow();

    return 0;
}
```

## API Reference

### Initialization Functions

- `ImPlatform_CreateWindow(name, pos, width, height)` - Create application window
- `ImPlatform_InitGfxAPI()` - Initialize graphics device
- `ImPlatform_ShowWindow()` - Display the window
- `ImPlatform_InitPlatform()` - Initialize platform backend (input, etc.)
- `ImPlatform_InitGfx()` - Create graphics resources

### Main Loop Functions

- `ImPlatform_PlatformContinue()` - Check if application should continue running
- `ImPlatform_PlatformEvents()` - Process platform events
- `ImPlatform_GfxCheck()` - Check if graphics are ready to render
- `ImPlatform_GfxAPINewFrame()` - Begin new graphics frame
- `ImPlatform_PlatformNewFrame()` - Begin new platform frame
- `ImPlatform_GfxAPIClear(color)` - Clear framebuffer
- `ImPlatform_GfxAPIRender(color)` - Render ImGui draw data
- `ImPlatform_GfxViewportPre()` - Update platform windows (multi-viewport)
- `ImPlatform_GfxViewportPost()` - Render platform windows (multi-viewport)
- `ImPlatform_GfxAPISwapBuffer()` - Present frame

### Shutdown Functions

- `ImPlatform_ShutdownGfxAPI()` - Shutdown graphics backend
- `ImPlatform_ShutdownWindow()` - Shutdown ImGui backends
- `ImPlatform_ShutdownPostGfxAPI()` - Cleanup device and swapchain
- `ImPlatform_DestroyWindow()` - Destroy window

## Platform & Graphics API Defines

### Platform Targets

```cpp
IM_PLATFORM_WIN32   // Windows (Win32 API)
IM_PLATFORM_GLFW    // GLFW (cross-platform)
IM_PLATFORM_SDL2    // SDL2
IM_PLATFORM_SDL3    // SDL3
IM_PLATFORM_APPLE   // macOS/iOS
```

### Graphics API Targets

```cpp
IM_GFX_OPENGL3      // OpenGL 3+
IM_GFX_DIRECTX9     // DirectX 9
IM_GFX_DIRECTX10    // DirectX 10
IM_GFX_DIRECTX11    // DirectX 11
IM_GFX_DIRECTX12    // DirectX 12
IM_GFX_VULKAN       // Vulkan
IM_GFX_METAL        // Metal (macOS/iOS)
IM_GFX_WGPU         // WebGPU
```

### Predefined Combinations

For convenience, you can also use these predefined target combinations:

```cpp
IM_TARGET_WIN32_DX10
IM_TARGET_WIN32_DX11
IM_TARGET_WIN32_DX12
IM_TARGET_WIN32_OPENGL3

IM_TARGET_GLFW_OPENGL3
IM_TARGET_GLFW_VULKAN
IM_TARGET_GLFW_METAL

IM_TARGET_APPLE_METAL
```

Usage example with predefined target:
```cpp
#define IM_CURRENT_PLATFORM IM_PLATFORM_WIN32
#define IM_CURRENT_GFX IM_GFX_DIRECTX11
// or simply:
// #define IM_CURRENT_TARGET IM_TARGET_WIN32_DX11
```

## Implementation Status

| Platform/API | OpenGL 3 | DX9 | DX10 | DX11 | DX12 | Vulkan | Metal | WebGPU |
|--------------|----------|-----|------|------|------|--------|-------|--------|
| Win32        | ✅       | ✅  | ✅   | ✅   | ✅   | ⚠️     | ❌    | ❌     |
| GLFW         | ✅       | ❌  | ❌   | ❌   | ❌   | ✅     | ✅    | ❌     |
| SDL2         | ✅       | ❌  | ❌   | ❌   | ❌   | ✅     | ❌    | ❌     |
| SDL3         | ✅       | ❌  | ❌   | ❌   | ❌   | ✅     | ❌    | ❌     |
| Apple        | ❌       | ❌  | ❌   | ❌   | ❌   | ❌     | ✅    | ❌     |

Legend: ✅ Implemented | ⚠️ Partial | ❌ Not Implemented

## Building the Demo

The ImPlatformDemo project demonstrates basic usage and supports multiple platform/graphics API configurations.

### Using Visual Studio

1. Open `ImPlatformDemo.sln` in Visual Studio
2. Select your desired configuration from the dropdown (e.g., Win32_DX11, Win32_OpenGL3, etc.)
3. Build and run (F5)

Available configurations in the dropdown:
- **Win32_DX9** - Windows + DirectX 9
- **Win32_DX10** - Windows + DirectX 10
- **Win32_DX11** - Windows + DirectX 11
- **Win32_DX12** - Windows + DirectX 12
- **Win32_OpenGL3** - Windows + OpenGL 3
- **GLFW_OpenGL3** - GLFW + OpenGL 3
- **GLFW_Vulkan** - GLFW + Vulkan

### Command Line Build

```bash
cd ImPlatformDemo

# Build Win32 + OpenGL3
msbuild ImPlatformDemo.sln /p:Configuration=Win32_OpenGL3 /p:Platform=x64

# Build Win32 + DirectX 11
msbuild ImPlatformDemo.sln /p:Configuration=Win32_DX11 /p:Platform=x64

# Build Win32 + DirectX 12
msbuild ImPlatformDemo.sln /p:Configuration=Win32_DX12 /p:Platform=x64

# Build GLFW + OpenGL3
msbuild ImPlatformDemo.sln /p:Configuration=GLFW_OpenGL3 /p:Platform=x64
```

### Requirements
- Dear ImGui (docking branch recommended for multi-viewport support)
- Platform-specific dependencies:
  - **Win32**: Windows SDK (included with Visual Studio)
  - **GLFW**: GLFW library at `../imgui/examples/libs/glfw/lib-vc2010-64/glfw3.lib` (for GLFW configurations)
  - **Vulkan**: Vulkan SDK (for Vulkan configurations)
  - **SDL2/SDL3**: SDL library
  - **Apple**: Xcode

**Note**: The GLFW library should be available in the Dear ImGui examples folder. The project is configured to use the relative path from the Dear ImGui repository.

## Project Structure

```
ImPlatform/
├── ImPlatform/
│   ├── ImPlatform.h              # Main header with API declarations
│   ├── ImPlatform_Internal.h    # Internal data structures and helpers
│   ├── ImPlatform_app_*.cpp     # Platform backends (Win32, GLFW, SDL, Apple)
│   └── ImPlatform_gfx_*.cpp     # Graphics API backends (DX, OpenGL, Vulkan, etc.)
├── ImPlatformDemo/
│   └── main.cpp                  # Example application
└── README.md
```

## Migration from Old API

The library has been refactored from a C++ namespace API to a clean C API. Key changes:

| Old API (C++)                    | New API (C)                      |
|----------------------------------|----------------------------------|
| `ImPlatform::CreateWindow()`     | `ImPlatform_CreateWindow()`      |
| `ImPlatform::InitGfxAPI()`       | `ImPlatform_InitGfxAPI()`        |
| `ImPlatform::PlatformContinue()` | `ImPlatform_PlatformContinue()`  |
| `__DEAR_WIN__`                   | `IM_CURRENT_PLATFORM`            |
| `__DEAR_GFX_DX11__`              | `IM_CURRENT_GFX`                 |
| `IM_PLATFORM_IMPLEMENTATION`     | `IMPLATFORM_IMPLEMENTATION`      |

## Known Limitations

- Simple API is not yet implemented in the new C API
- Custom shader support is not yet available
- Texture creation API is not yet available
- Custom title bar feature is work in progress

## Contributing

Contributions are welcome! The author primarily develops for Windows, so contributions for other platforms (macOS, Linux, etc.) are especially appreciated.

## License

This project follows Dear ImGui's licensing model. Please refer to the Dear ImGui license for details.

## Credits

- Original author: @SoufianeKHIAT
- Built on top of [Dear ImGui](https://github.com/ocornut/imgui) by Omar Cornut

## Support Development

[<img src="https://c5.patreon.com/external/logo/become_a_patron_button@2x.png" alt="Become a Patron" width="150"/>](https://www.patreon.com/SoufianeKHIAT)

https://www.patreon.com/SoufianeKHIAT
