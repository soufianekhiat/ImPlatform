# ImPlatform

ImPlatform is a platform abstraction layer that simplifies multiplatform development with Dear ImGui by providing a unified C API for window management, graphics initialization, and rendering across different platforms and graphics APIs.

## Features

- **Clean C API**: Simple, consistent function naming (`ImPlatform_*`)
- **Multiple Platform Support**: Win32, GLFW, SDL2, SDL3, Apple (macOS/iOS)
- **Multiple Graphics API Support**: OpenGL 3, DirectX 9/10/11/12, Vulkan, Metal, WebGPU
- **Header-Only Implementation**: Single header include with implementation macro
- **Multi-Viewport Support**: Built-in support for ImGui's multi-viewport and docking features
- **Custom Shader Support**: Render custom shaders with ImGui's vertex/index buffers
- **Texture Creation API**: Platform-agnostic texture loading and management
- **Render Targets and Render Passes**: Nestable render-target stack, and Dear ImGui draw data rendered into a texture before the frame on every backend, with premultiplied alpha
- **Direct Draws from Draw Callbacks**: switch to a render texture, copy the current target, draw a quad or your own vertex buffers with a custom shader and pushed constants (DirectX 9/11/12, OpenGL 3, Vulkan; DirectX 10 and WebGPU without target switches or copies)
- **Readback and Compute**: Texture readback (synchronous, and asynchronous on DirectX 11/12) and compute dispatch on DirectX 11/12, Vulkan and OpenGL 4.3+
- **Pen Input**: Pressure, tilt, barrel rotation, eraser and coalesced samples (Win32)
- **Custom Graphics Backend**: `IM_GFX_CUSTOM` keeps the platform layer and lets the application provide the renderer
- **Custom Title Bar**: Optional borderless window with custom titlebar rendering (Win32, GLFW*)
- **Flexible Configuration**: Define platform and graphics API at compile time with feature flags

## Quick Start

### Version Checking

You can check the ImPlatform version at both compile-time and runtime:

```cpp
// Compile-time version checking
#include <ImPlatform.h>

#if IMPLATFORM_VERSION_NUM >= 20000
    // Features available in version 2.0.0+
#endif

// Runtime version checking
const char* version = ImPlatform_GetVersion();  // Returns "2.0.0"
int version_num = ImPlatform_GetVersionNum();   // Returns 20000

printf("ImPlatform version: %s (numeric: %d)\n", version, version_num);
```

Version encoding: `XYYZZ` where `X` = major, `YY` = minor, `ZZ` = patch
- Version 0.5.0 = 500
- Version 2.0.0 = 20000

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
        if (!ImPlatform_PlatformEvents()) // false: quit requested or window minimized
            continue;

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
- `ImPlatform_PlatformEvents()` - Process platform events, returns false when the frame should be skipped (quit requested, window minimized)
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

### Advanced Features

#### Custom Shaders (when `IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER` is enabled)

```cpp
// One stage from source (HLSL on Direct3D, GLSL on OpenGL, MSL on Metal) or bytecode (SPIR-V on Vulkan)
ImPlatform_ShaderDesc vs_desc = {};
vs_desc.stage = ImPlatform_ShaderStage_Vertex;
vs_desc.format = ImPlatform_ShaderFormat_HLSL;
vs_desc.source_code = vertex_shader_source;
vs_desc.entry_point = "vs_main";
ImPlatform_Shader vs = ImPlatform_CreateShader(&vs_desc);

ImPlatform_ShaderProgram program = ImPlatform_CreateShaderProgram(vs, ps);

// Parameters (register b1), then draw list commands drawn with the program
ImPlatform_SetShaderUniform(program, "params", &params, sizeof(params));
ImPlatform_BeginCustomShader(draw_list, program);
draw_list->AddImage(...);
ImPlatform_EndCustomShader(draw_list);
```

Plain HLSL files can be compiled offline for every backend with Slang (see `ImPlatformDemo/shaders/README.md`).

#### Shader Parts (when `IMPLATFORM_GFX_SUPPORT_SHADER_PARTS` is enabled)

A custom shader written once and completed at run time for the backend: HLSL on Direct3D 9 to 12, GLSL 330 on
OpenGL, MSL on Metal. The application gives its parameter block, extra varyings, textures, helper functions and the
vertex / pixel bodies; ImPlatform writes the entry points, inputs, outputs and declarations. Bodies are HLSL plus a few
macros (`IMP_SAMPLE`, `IMP_LOAD`, `IMP_MUL`, `IMP_CAST`, `IMP_UNROLL`); the rules are in `ImPlatform.h`.

```cpp
ImPlatform_ShaderParts parts = {};
parts.params    = "float4 ColorStart; float4 ColorEnd; float4 Offset;";   // b1, same layout as the C struct
parts.varyings  = "float2 world;";
parts.textures[1] = "tint";                                                // t1, ImPlatform_SetShaderTexture(..., 1, ...)
parts.functions = "float4 Gradient(float t) { return lerp(ColorStart, ColorEnd, saturate(t)); }";
parts.vs_body   = "position = mul(ProjMtx, float4(in_pos + Offset.xy, 0.0, 1.0)); v.world = in_pos;";   // vertex animation
parts.ps_body   = "col_out = Gradient(v.world.y / 64.0) * IMP_SAMPLE(tint, v.uv);";
ImPlatform_Shader vs = ImPlatform_CreateShaderFromParts(&parts, ImPlatform_ShaderStage_Vertex);
ImPlatform_Shader ps = ImPlatform_CreateShaderFromParts(&parts, ImPlatform_ShaderStage_Fragment);
ImPlatform_ShaderProgram program = ImPlatform_CreateShaderProgram(vs, ps);
```

`ImPlatform_GenerateShaderSource()` returns the generated text of any language, whatever the backend.
#### Texture Creation (when `IMGUI_HAS_TEXTURES` is defined)

```cpp
// Create texture
ImPlatform_TextureDesc tex_desc = ImPlatform_TextureDesc_Default(width, height);
ImTextureID texture = ImPlatform_CreateTexture(pixel_data, &tex_desc);

// Use in ImGui
ImGui::Image(texture, ImVec2(256, 256));

// Cleanup
ImPlatform_DestroyTexture(texture);
```

#### Readback and Compute (optional capabilities)

```cpp
if (ImPlatform_SupportsCompute())
{
    ImTextureID out = ImPlatform_CreateStorageTexture(NULL, &tex_desc);       // zeroed, drawable by ImGui
    ImPlatform_DispatchCompute(cs, &in, 1, &out, 1, &params, sizeof(params), w / 8, h / 8, 1);
}
if (ImPlatform_SupportsReadback())
    ImPlatform_ReadTexture(out, 0, 0, w, h, pixels, sizeof(pixels));        // synchronous
```

| Backend | Readback | Compute |
|---|---|---|
| DirectX 11, DirectX 12, Vulkan | yes | yes |
| OpenGL 3 | yes | OpenGL 4.3+ (checked at runtime) |
| DirectX 10 | yes | no (cs_4_0 cannot write textures) |
| DirectX 9 | yes | no |
| Metal, WebGPU | no | no |

Shader bindings per backend are listed above `ImPlatform_DispatchCompute` in `ImPlatform.h`.
`ImPlatformDemo --compute-test` runs the same dispatch + readback check on any backend (exit code 0 pass, 1 fail, 2 unsupported).

#### Render passes

Dear ImGui draw data rendered into a render texture before the frame, by the backend's own ImGui renderer (draw
callbacks included): to cache vector drawings as images, for instance.

```cpp
ImPlatform_TextureDesc desc = ImPlatform_TextureDesc_Default(w, h);
desc.format = ImPlatform_GetRenderPassFormat();              // The ImGui pipeline's format (BGRA8 on some backends)
ImTextureID target = ImPlatform_CreateRenderTexture(&desc);

// Each frame that needs it (one pass per frame): draw_data maps DisplayPos / DisplaySize to the whole target.
ImPlatform_QueueRenderPass(target, &draw_data, true);       // true: cleared to transparent first

// Rendered over transparent, its colors are premultiplied: draw it with premultiplied alpha.
ImPlatform_PushPremultipliedAlpha(draw_list);
draw_list->AddImage(target, p_min, p_max);
ImPlatform_PopPremultipliedAlpha(draw_list);
```

The pass runs at the start of `ImPlatform_GfxAPIRender()` (in `ImPlatform_GfxAPIClear()` on Vulkan, outside the
frame's render pass); the draw data must stay valid until then. Every backend: DirectX 9, 10, 11, 12, OpenGL 3, Vulkan,
WebGPU and Metal (premultiplied alpha through blend states, or a copy of ImGui's pipeline where blending is part of it).
`ImPlatformDemo --render-pass-test` checks a pass target and a premultiplied composite by readback (exit code 0 pass,
1 fail, 2 unsupported).

#### Custom Title Bar (when `IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR` is enabled)

```cpp
// Enable before creating window
ImPlatform_EnableCustomTitleBar();
ImPlatform_CreateWindow(...);

// In main loop, before other ImGui windows:
if (ImPlatform_BeginCustomTitleBar(32.0f))
{
    // Option 1: Use default titlebar
    ImPlatform_DrawCustomMenuBarDefault();

    // Option 2: Custom titlebar
    ImGui::Text("My App");
    if (ImGui::Button("_")) ImPlatform_MinimizeApp();
    if (ImGui::Button("[]")) ImPlatform_MaximizeApp();
    if (ImGui::Button("X")) ImPlatform_CloseApp();
}
ImPlatform_EndCustomTitleBar();
```

**Note**: Custom titlebar on GLFW requires [TheCherno's GLFW fork](https://github.com/TheCherno/glfw/tree/dev) and defining `IMPLATFORM_APP_SUPPORT_CUSTOM_TITLEBAR=1` and `IM_THE_CHERNO_GLFW3`.

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
IM_GFX_CUSTOM       // No built-in backend: the application provides its own
```

### Backend Capabilities

Some API groups exist only on the backends that implement them. `ImPlatform.h` sets an `IMPLATFORM_GFX_SUPPORT_*`
macro per group for the selected backend and declares the group's functions only when it is 1, so a call that the
backend cannot honor fails to compile instead of returning `false` at run time. Guard such calls in code shared
between backends:

```cpp
#if IMPLATFORM_GFX_SUPPORT_RENDER_TARGETS
    ImPlatform_PushRenderTarget(target, true);
    ...
#endif
```

| `IMPLATFORM_GFX_SUPPORT_` | Functions | DX9 | DX10 | DX11 | DX12 | OpenGL 3 | Vulkan | Metal | WebGPU |
|---|---|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
| `SHADER_TEXTURES` | `SetShaderTexture` (t0..t8) | x | x | x | x | x | x | x | x |
| `SHADER_PARTS` | `CreateShaderFromParts` (shaders written once, compiled at run time) | x | x | x | x | x | | x | |
| `VERTEX_BUFFERS` | vertex / index buffers, `DrawIndexed` | x | x | x | x | x | x | | x |
| `DIRECT_DRAW` | `Push/PopShaderConstants`, `GetCurrentTargetSize` | x | | x | x | x | x | | x |
| `RENDER_TARGETS` | `Push/PopRenderTarget`, `CopyCurrentTarget`, `DrawQuadNow` | x | | x | x | x | x | | |
| `READBACK` | `ReadTexture` | x | x | x | x | x | x | x | |
| `ASYNC_READBACK` | `BeginReadTexture`, `Poll/Map/ReleaseReadback` | | | x | x | | | | |
| `COMPUTE` | `CreateStorageTexture`, `DispatchCompute` | | | x | x | 4.3+ | x | | |
| `DEPTH_DRAW` | `CreateRenderTextureWithDepth`, `DrawMesh` | | | x | x | | | | |
| `DEVICE_CONTROL` | `GfxDeviceLost`, `GfxRecreateDevice`, `GfxAdapterName`, testing aids | | | x | x | | | | |

`ImPlatform_SupportsReadback()`, `SupportsCompute()` and `SupportsDepthDraw()` are declared on every backend and
answer at run time (OpenGL compute needs a 4.3 context). Custom shaders (`IMPLATFORM_GFX_SUPPORT_CUSTOM_SHADER`) are on every
built-in backend. So are 3D textures: `ImPlatform_CreateTexture3D()`, bound with `SetShaderTexture()` to a slot the
shader declares `Texture3D` (a `<slot> <name> 3d` line in shader parts); `ImPlatform_SupportsTexture3D()` answers at
run time (DirectX 9: the device's volume texture capability).

### Custom (or no) Graphics Backend

`IM_GFX_CUSTOM` keeps ImPlatform's platform layer (window, input, DPI, title bar) and compiles
none of its graphics backends. The application links its own implementation of the graphics
entry points (`ImPlatform_InitGfxAPI`, `ImPlatform_GfxAPINewFrame`, `ImPlatform_GfxAPIRender`,
`ImPlatform_GfxAPISwapBuffer`, textures, shaders, the render passes `ImPlatform_GetRenderPassFormat` /
`QueueRenderPass` / `PushPremultipliedAlpha` / `PopPremultipliedAlpha`, the `Supports*` queries, and the functions of
every optional group above that it keeps) plus:

```cpp
// Called by the platform layer (Win32, GLFW, SDL2, SDL3) when the window's framebuffer is resized.
void ImPlatform_Gfx_OnResize_Custom(unsigned int uWidth, unsigned int uHeight);
```

The `IMPLATFORM_GFX_SUPPORT_*` feature flags and API groups default to 1 and can each be predefined to 0
to describe what the application's backend supports (a group set to 0 is neither declared nor needed). With GLFW the window is created with
`GLFW_NO_API`.

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
Clone with submodules (`git clone --recursive`, or `git submodule update --init --recursive`).

### Sharpmake (default)

The projects are generated by [Sharpmake](https://github.com/ubisoft/Sharpmake) (the `extern/Sharpmake` submodule,
built on first use; requires the .NET 8 SDK or later). Each OS generates its own projects at the repository root:

```bash
buildsystem\generate_projects.bat      # Windows: ImPlatform_vs2022_Win64.sln
./buildsystem/generate_projects.sh     # Linux: ImPlatform_make_Linux.make, macOS: ImPlatform_xcode_macOS.xcworkspace
```

Executables are written to `bin/<platform>/ImPlatformDemo_<Platform>_<Gfx>`. One configuration per platform/graphics pair:

| OS | Configurations |
|----|----------------|
| Windows | `Win32_DX9`, `Win32_DX10`, `Win32_DX11`, `Win32_DX12`, `Win32_OpenGL3`, `GLFW_OpenGL3`, `GLFW_Vulkan`, `SDL2_DX11`, `SDL2_OpenGL3`, `SDL2_Vulkan`, `SDL3_DX11`, `SDL3_OpenGL3`, `SDL3_Vulkan` |
| Linux | `GLFW_OpenGL3`, `GLFW_Vulkan`, `SDL2_OpenGL3`, `SDL2_Vulkan`, `SDL3_OpenGL3`, `SDL3_Vulkan` |
| macOS | `GLFW_OpenGL3`, `GLFW_Metal`, `SDL2_OpenGL3`, `SDL2_Metal`, `SDL3_OpenGL3`, `SDL3_Metal` |

```bash
# Windows (or open the solution in Visual Studio 2022)
msbuild ImPlatform_vs2022_Win64.sln /p:Configuration=Win32_DX11 /p:Platform=x64

# Linux (configuration names in lower case)
make -f ImPlatform_make_Linux.make config=glfw_opengl3

# macOS
xcodebuild -workspace ImPlatform_xcode_macOS.xcworkspace -scheme ImPlatformDemo_xcode_macOS_GLFW_Metal -configuration GLFW_Metal build
```

Edit the scripts in `buildsystem/sharpmake/src/` and regenerate; the generated files are not versioned.

### CMake

`CMakeLists.txt` mirrors the Sharpmake configurations (one `ImPlatformDemo_<Platform>_<Gfx>` target per pair whose
dependencies are found, plus WebGPU when a Dawn or wgpu-native target is provided):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DIMPLATFORM_FETCH_DEPENDENCIES=ON
cmake --build build --config Release --parallel
```

### Requirements
- Dear ImGui: the `imgui` submodule (docking branch with stack layout)
- Platform-specific dependencies:
  - **Win32**: Windows SDK (included with Visual Studio)
  - **GLFW**: on Windows the prebuilt `imgui/examples/libs/glfw/lib-vc2010-64/glfw3.lib`; elsewhere the system package (`libglfw3-dev`, `brew install glfw`)
  - **Vulkan**: Vulkan SDK (`VULKAN_SDK` environment variable on Windows)
  - **SDL2/SDL3**: SDL development package; on Windows set `SDL2_INCLUDE` / `SDL2_LIB` / `SDL3_INCLUDE` / `SDL3_LIB` (environment variables or MSBuild `/p:` properties) and copy the DLL next to the executable
  - **Apple**: Xcode

### Self-tests

The demo checks the readback/compute, render pass and custom shader paths of whichever backend it was built with:

```bash
ImPlatformDemo_Win32_DX12 --compute-test       # dispatch + readback
ImPlatformDemo_Win32_DX12 --render-pass-test   # render pass target and premultiplied composite
ImPlatformDemo_Win32_DX12 --shader-test        # shaders/gradient.hlsl drawn into a render pass, read back
ImPlatformDemo_Win32_DX12 --direct-draw-test   # from a draw callback: push a render target, push constants, DrawQuadNow
ImPlatformDemo_Win32_DX12 --shader-parts-test  # a shader written as parts: parameters in both stages, varying, texture t1
ImPlatformDemo_Win32_DX12 --texture3d-test     # a 2x2x2 texture at t1, sampled by shaders/volume_test.hlsl and by shader parts
```

Each prints PASS or FAIL and exits with 0 (pass), 1 (fail) or 2 (unsupported by the backend).

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
├── buildsystem/
│   ├── generate_projects.*       # Runs Sharpmake
│   └── sharpmake/src/            # Project definitions (main.cs, common.cs, ImPlatformDemo.cs)
├── extern/Sharpmake/             # Sharpmake (submodule)
├── imgui/                        # Dear ImGui (submodule)
├── CMakeLists.txt                # CMake mirror of the Sharpmake configurations
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

## Tested Versions

The library has been tested with the following configurations:

### Platforms Tested
- **Windows 10/11** (Win32 API) - Tested
- **Windows 10/11** (GLFW) - Tested
- **Windows 10/11** (SDL2) - Tested (self-tests)
- **Windows 10/11** (SDL3) - Tested (self-tests)
- **macOS** - Not tested (contributions welcome!)
- **Linux** (GLFW, SDL2, SDL3) - Tested (self-tests, Ubuntu 24.04 with Mesa)

### Graphics APIs Tested
- **DirectX 9** - Tested (Win32 only)
- **DirectX 10** - Tested (Win32 only)
- **DirectX 11** - Tested (Win32 only)
- **DirectX 12** - Tested (Win32 only)
- **OpenGL 3** - Tested (Win32, GLFW)
- **Vulkan** - Basic testing (GLFW, SDL2, SDL3)
- **Metal** - Not tested (macOS/iOS only, contributions welcome!)
- **WebGPU** - Basic testing (Emscripten + GLFW in Chrome)

### Dear ImGui Version
- **Tested with**: Dear ImGui v1.92.x (docking branch)
- **Minimum required**: Dear ImGui v1.92.x
- **Recommended**: Use the latest docking branch for full multi-viewport support

## Known Limitations

- Apple (macOS/iOS) platform implementation needs testing
- OpenGL3 multi-viewport with custom shaders may have minor offset issues in some edge cases
- Custom titlebar on GLFW requires TheCherno's fork

## Contributing

Contributions are welcome! The author primarily develops and tests on **Windows with Visual Studio**, so contributions and testing reports for other platforms are **especially appreciated**.

### We Need Your Help!

**Platform Testers Wanted:**
- macOS testers (Metal, OpenGL3)
- Linux testers (OpenGL3, Vulkan)
- SDL2/SDL3 comprehensive testing on all platforms
- iOS/mobile platform testing

**Feature Contributors:**
- Apple Metal backend improvements
- SDL platform feature parity (custom titlebar, etc.)
- Platform-specific bug fixes and optimizations

**How to Contribute:**
1. Test ImPlatform on your platform/configuration
2. Report bugs or issues via GitHub Issues
3. Submit pull requests with fixes or new features
4. Share your experience and feedback

If you've successfully built and tested ImPlatform on a platform/configuration not listed above, please let us know! Your feedback helps improve the library for everyone.

## License

This project is licensed under the **MIT License**, the same as Dear ImGui.

## Credits

- Original author: @SoufianeKHIAT
- Built on top of [Dear ImGui](https://github.com/ocornut/imgui) by Omar Cornut

## Support Development

[<img src="https://c5.patreon.com/external/logo/become_a_patron_button@2x.png" alt="Become a Patron" width="150"/>](https://www.patreon.com/SoufianeKHIAT)

https://www.patreon.com/SoufianeKHIAT
