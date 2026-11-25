# ImPlatform

ImPlatform is a platform abstraction layer that simplifies multiplatform development with Dear ImGui by providing a unified C API for window management, graphics initialization, and rendering across different platforms and graphics APIs.

## Features

- **Clean C API**: Simple, consistent function naming (`ImPlatform_*`)
- **Multiple Platform Support**: Win32, GLFW, SDL2, SDL3, Apple (macOS/iOS)
- **Multiple Graphics API Support**: OpenGL 3, DirectX 9/10/11/12, Vulkan, Metal, WebGPU
- **Header-Only Implementation**: Single header include with implementation macro
- **Multi-Viewport Support**: Built-in support for ImGui's multi-viewport and docking features
- **Custom Shader Support**: Render custom shaders with ImGui's vertex/index buffers, written once in HLSL and compiled offline for every backend
- **Shader Parts**: Shaders written once and generated at run time as HLSL, GLSL or MSL (Direct3D 9 to 12, OpenGL 3, Metal), with their own vertex inputs, varyings and textures
- **Shader Blending, Permutations and Errors**: per-program blend state on every backend, `#define` permutations of one source, compiler messages returned to the application, and programs compiled without blocking the frame (on a worker thread on DirectX 10/11/12)
- **Texture Creation API**: Platform-agnostic texture loading and management, 3D textures on every backend
- **Render Targets and Render Passes**: Nestable render-target stack, and Dear ImGui draw data rendered into a texture before the frame on every backend, with premultiplied alpha
- **Direct Draws from Draw Callbacks**: switch to a render texture, copy the current target, draw a quad or your own vertex buffers with a custom shader and pushed constants (DirectX 9/11/12, OpenGL 3, Vulkan; DirectX 10 and WebGPU without target switches or copies; Metal draws vertex buffers)
- **Readback and Compute**: Texture readback (synchronous on every backend but WebGPU, asynchronous on DirectX 11/12) and compute dispatch on DirectX 11/12, Vulkan and OpenGL 4.3+
- **Depth-Tested Meshes and Device Control**: Depth render textures, meshes with depth / blend / cull states, device loss and recreation (DirectX 11/12)
- **Pen Input**: Pressure, tilt, barrel rotation and eraser (Windows with Win32, GLFW or SDL2, with coalesced samples; SDL3 on Windows, Linux and macOS)
- **Touch Input**: the fingers on a touch screen and their gesture, pan, pinch zoom and rotation (Windows, SDL2, SDL3); a trackpad's pinch (Windows, macOS, SDL 3.4+)
- **Drag and Drop with Other Applications**: files and text or URLs dropped on the application, with the drag reported while it hovers (Windows, Linux on X11, SDL3) or at the release (macOS and Wayland with SDL2 or GLFW); files and text dragged out of it (Windows, Linux on X11)
- **Window State**: borderless fullscreen, always on top, window icon (Win32, GLFW, SDL2, SDL3)
- **System Theme and Clipboard Images**: the user's light or dark mode, and RGBA images to and from the clipboard, on Windows, macOS and Linux whatever the window library
- **Screenshots**: a viewport captured as a texture and read back as RGBA pixels (DirectX 9/11/12, OpenGL 3, Vulkan)
- **Custom Graphics Backend**: `IM_GFX_CUSTOM` keeps the platform layer and lets the application provide the renderer
- **Custom Title Bar**: Optional borderless window with custom titlebar rendering (Win32, GLFW, SDL2, SDL3)
- **Flexible Configuration**: Define platform and graphics API at compile time with feature flags

## Quick Start

### Version Checking

You can check the ImPlatform version at both compile-time and runtime:

```cpp
// Compile-time version checking
#include <ImPlatform.h>

#if IMPLATFORM_VERSION_NUM >= 30000
    // Features available in version 3.0.0+
#endif

// Runtime version checking
const char* version = ImPlatform_GetVersion();  // Returns "3.0.0"
int version_num = ImPlatform_GetVersionNum();   // Returns 30000

printf("ImPlatform version: %s (numeric: %d)\n", version, version_num);
```

Version encoding: `XYYZZ` where `X` = major, `YY` = minor, `ZZ` = patch
- Version 0.5.0 = 500
- Version 2.0.0 = 20000
- Version 3.0.0 = 30000

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
// One stage from source (HLSL on Direct3D, GLSL on OpenGL, MSL on Metal, WGSL on WebGPU) or bytecode (SPIR-V on Vulkan)
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

**Compile errors.** When `ImPlatform_CreateShader()`, `ImPlatform_CreateShaderFromParts()` or
`ImPlatform_CreateShaderProgram()` returns NULL, `ImPlatform_GetShaderLog()` returns the compiler's or linker's
messages (also printed to stderr), to show in the application. For shader parts the generated source follows, with line
numbers. Vulkan and WebGPU take compiled or validated-later code (SPIR-V, WGSL): their log holds ImPlatform's own
messages only, and WebGPU reports WGSL errors through the device's error callback (stderr).

```cpp
ImPlatform_Shader ps = ImPlatform_CreateShader(&ps_desc);
if (!ps)
    ImGui::TextUnformatted(ImPlatform_GetShaderLog());
```

**Permutations.** `ImPlatform_ShaderDesc::defines` puts preprocessor definitions before the source (HLSL, GLSL, MSL),
for `#ifdef` and `#if NAME == value`: one shader per combination. The bytecode cache keeps one entry per combination
and the compiler's line numbers stay those of the source. Shader parts have the same field
(`ImPlatform_ShaderParts::defines`, or a `//@defines` section).

```cpp
ps_desc.defines = "CLIP;MODE=2";     // #define CLIP  and  #define MODE 2
```

**Blending.** A program draws with Dear ImGui's blending (straight alpha) unless it is given its own:

```cpp
// By name: Alpha, Premultiplied, Opaque, Additive, Subtractive, Multiplicative, Screen, Darken, Lighten
ImPlatform_SetShaderProgramBlendPreset(program, ImPlatform_BlendPreset_Additive);

// Or any factors and operations, starting from a preset or from scratch
ImPlatform_BlendDesc blend = ImPlatform_BlendDesc_Preset(ImPlatform_BlendPreset_Additive);
blend.op_alpha = ImPlatform_BlendOp_Max;
ImPlatform_SetShaderProgramBlend(program, &blend);                                          // NULL: Dear ImGui's again
```

It applies to the draw list commands between `ImPlatform_BeginCustomShader()` and `ImPlatform_EndCustomShader()`, to
`ImPlatform_DrawIndexed()` and to `ImPlatform_DrawQuadNow()`, on every backend (a device state on DirectX 9 to 11 and
OpenGL, part of the program's pipelines elsewhere). It is read when the program is bound: set it once after creating the
program, or from a draw callback before the draw.

**Asynchronous compilation.** `ImPlatform_CreateShaderProgramAsync()` returns at once and the program is compiled
without blocking the frame; `ImPlatform_GetShaderJobStatus()` tells when it is ready.

```cpp
static ImPlatform_ShaderJob job = ImPlatform_CreateShaderProgramAsync(&vs_desc, &ps_desc);
ImPlatform_ShaderProgram program = NULL;
switch (ImPlatform_GetShaderJobStatus(job, &program))
{
case ImPlatform_ShaderStatus_Compiling: ImGui::TextUnformatted("Shader compilation in progress"); break;
case ImPlatform_ShaderStatus_Failed:    ImGui::TextUnformatted(ImPlatform_GetShaderJobLog(job)); break;
case ImPlatform_ShaderStatus_Ready:     ImPlatform_DrawCustomShaderQuad(program); break;
}
...
ImPlatform_DestroyShaderJob(job);    // With its program
```

`ImPlatform_CreateShaderProgramFromPartsAsync()` does the same from shader parts. DirectX 10, 11 and 12 compile the HLSL
on a worker thread (one for all jobs); the device objects are created by the first `ImPlatform_GetShaderJobStatus()`
call after that, on the graphics thread. The other backends compile inside the create call, so the job is ready or
failed at once there: DirectX 9, OpenGL (it compiles on the thread of its context) and Metal; Vulkan and WebGPU take
SPIR-V / WGSL and build their pipelines at the first draw.

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

The same parts can come from one text with `//@params`, `//@inputs`, `//@varyings`, `//@textures`, `//@defines`,
`//@functions`, `//@vs` and `//@ps` sections (`ImPlatform_ParseShaderParts()`). `vertex_inputs` gives a shader its own
vertex buffer layout for `ImPlatform_DrawIndexed()`; a `<slot> <name> 3d` texture line declares a 3D texture. DirectX 9
defines `IMP_SM3` (shader model 3 limits) so a shader can take another path there.

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
| DirectX 9, Metal | yes | no |
| WebGPU | no | no |

Shader bindings per backend are listed above `ImPlatform_DispatchCompute` in `ImPlatform.h`.
`ImPlatformDemo --compute-test` runs the same dispatch + readback check on any backend (exit code 0 pass, 1 fail, 2 unsupported).

**Asynchronous compute** (every backend with compute). A compute shader can be compiled without blocking, like a
program, and a dispatch can be polled for completion instead of waited for:

```cpp
static ImPlatform_ShaderJob job = ImPlatform_CreateComputeShaderAsync(&cs_desc);   // worker thread on DirectX 11 / 12
static ImPlatform_ComputeDispatch dispatch = NULL;
if (!dispatch)
    if (ImPlatform_Shader cs = ImPlatform_GetShaderJobShader(job))                 // NULL until the job is ready
        dispatch = ImPlatform_DispatchComputeAsync(cs, &in, 1, &out, 1, &params, sizeof(params), w / 8, h / 8, 1);

if (ImPlatform_PollComputeDispatch(dispatch) == 1)   // 1 completed, 0 not yet, -1 error (or no dispatch yet)
    ImGui::Image(out, size);
else
    ImGui::TextUnformatted("Computing...");
...
ImPlatform_ReleaseComputeDispatch(dispatch);
ImPlatform_DestroyShaderJob(job);                    // With its compute shader
```

`ImPlatform_DispatchComputeAsync()` never waits for the GPU (on Vulkan, `ImPlatform_DispatchCompute()` does). GPU work
completes in order, so for a chain of dispatches only the last one needs a handle. The dispatch runs on the same GPU
queue as the frames: a long one still delays the frames submitted after it. `ImPlatformDemo --compute-async-test`
checks both.

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

#### Screenshots

The image of a viewport as rendered this frame, as a texture and as pixels (DirectX 9, 11, 12, OpenGL, Vulkan: the
backends with `IMPLATFORM_GFX_SUPPORT_RENDER_TARGETS` and `_READBACK`):

```cpp
// During the frame, after the windows were submitted and before ImGui::Render()
unsigned int w, h;
ImTextureID capture = ImPlatform_CaptureViewport(NULL, &w, &h);   // NULL: the main viewport

// After the frame was rendered
std::vector<unsigned char> rgba(w * h * 4);
ImPlatform_ReadCapture(capture, w, h, rgba.data(), rgba.size()); // RGBA8, rows top-down, alpha 255
ImPlatform_DestroyTexture(capture);
```

The capture is a render texture, so it can also be drawn (`ImGui::Image`) as a thumbnail. A secondary platform window
is captured by passing its viewport, except on Vulkan. Writing an image file from the pixels is left to the
application (`stb_image_write`, for instance). `ImPlatformDemo --screenshot-test` checks a capture of the main
viewport.

#### Pen and Touch Input

Dear ImGui sees a pen or a finger as the mouse. These calls give what the mouse does not carry, the same way on every
platform that has it (`ImPlatform_SupportsPen()`: Windows with Win32, GLFW or SDL2, and SDL3; `ImPlatform_SupportsTouch()`: Windows, SDL2, SDL3):

```cpp
// Pen: every sample since the previous call (pressure, tilt, rotation, eraser, contact)
ImPlatform_PenSample samples[64];
int count = ImPlatform_ConsumePenSamples(samples, 64);

// Touch: what the fingers did since the previous call, once a frame
ImPlatform_TouchGesture gesture;
if (ImPlatform_ConsumeTouchGesture(&gesture) && gesture.finger_count >= 2)
{
    view_offset += ImVec2(gesture.pan_x, gesture.pan_y);
    view_scale *= gesture.zoom;         // around (gesture.x, gesture.y), the centroid of the fingers
    view_angle += gesture.rotation;     // degrees, clockwise
}

// The fingers themselves
ImPlatform_TouchPoint fingers[10];
int finger_count = ImPlatform_GetTouchPoints(fingers, 10);
```

Positions are client-area coordinates of the main window. One finger pans too (`finger_count == 1`); ImGui receives
it as a mouse drag, so applications usually act on two fingers or more. Taps, long presses and swipes are not
recognized: a tap arrives as a mouse click.

A pinch on a trackpad comes through the same call as `gesture.zoom` with `finger_count == 0` (Win32: precision
touchpads; macOS: with GLFW, SDL2 and SDL3, where the two-finger turn also comes as `gesture.rotation`; SDL 3.4 and
later: its pinch events). The trackpad's two-finger pan is the mouse wheel ImGui receives.

#### Drag and Drop from Other Applications (when `IMPLATFORM_APP_SUPPORT_DROP_FILE` is enabled)

```cpp
// Every event of a drag over the window
static void OnDrop(const ImPlatform_DropEvent* event, void* user_data)
{
    switch (event->type)
    {
    case ImPlatform_DropEvent_Begin:    /* a drag entered the window: show the drop zones */    break;
    case ImPlatform_DropEvent_Position: /* event->pos: highlight the zone under it */           break;
    case ImPlatform_DropEvent_File:     /* event->text: the UTF-8 path of a dropped file */     break;
    case ImPlatform_DropEvent_Text:     /* event->text: dropped text, a URL from a browser */   break;
    case ImPlatform_DropEvent_End:      /* dropped, or the drag left: hide the drop zones */    break;
    }
}
ImPlatform_SetDropCallback(OnDrop, NULL);

// Or the files only, one call each
ImPlatform_SetDropFileCallback(OnDropFile, NULL);
```

A drop is `Begin`, one `File` or `Text` event per item, then `End`. ImPlatform passes on what the drag carries and
leaves the rest to the application: an image dragged from a web page arrives as `Text` holding its URL, and fetching
it is the application's work.

| Platform | Drag reported while it hovers | Files | Text / URLs |
|---|---|---|---|
| Windows (Win32, GLFW, SDL2) | yes | yes | yes |
| SDL3 (Windows, Linux, macOS) | yes | yes | yes |
| Linux on X11 (GLFW, SDL2) | yes | yes | yes |
| SDL2 on macOS and Wayland | no (at the release) | yes | where SDL2 reports it |
| GLFW on macOS and Wayland | no (at the release) | yes | no |

On Windows and with SDL3, Dear ImGui's platform windows (multi-viewports) take drops too; elsewhere the main window does. `event->pos` is in the space of `ImGui::GetMousePos()`: client-area coordinates, or desktop coordinates once multi-viewports are enabled.

Dragging out of the application (Windows with every window library, Linux on X11 with GLFW or SDL2; `ImPlatform_SupportsDragOut()` is false elsewhere):

```cpp
// While a mouse button is held, when a drag that started in the application leaves its window
if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
{
    const char* paths[] = { "C:/exports/picture.png" };
    bool taken = ImPlatform_DragOut(paths, 1, NULL);    // files and / or text; returns when the drag ends
}
```

#### Window State, System Theme, Clipboard Images

```cpp
// When IMPLATFORM_APP_SUPPORT_WINDOW_STATE is enabled (Win32, GLFW, SDL2, SDL3; not the web build)
ImPlatform_SetFullscreen(!ImPlatform_IsFullscreen());   // borderless, on the monitor the window is on
ImPlatform_SetAlwaysOnTop(true);
ImPlatform_SetWindowIcon(rgba, 32, 32);                 // RGBA8, rows top-down

// The user's light or dark mode: Windows, macOS, Linux; Unknown where the system does not tell
if (ImPlatform_GetSystemTheme() == ImPlatform_SystemTheme_Dark)
    ImGui::StyleColorsDark();

// An image to and from the clipboard as RGBA8 pixels: Windows, macOS, Linux
ImPlatform_SetClipboardImage(rgba, width, height);
int w, h;
if (ImPlatform_GetClipboardImageSize(&w, &h))
{
    std::vector<unsigned char> pixels((size_t)w * h * 4);
    ImPlatform_GetClipboardImage(pixels.data(), w, h);
}
```

`ImPlatform_GetSystemTheme()` is meant to be called once a frame: compare it with the previous value to follow a
change. Clipboard text stays with `ImGui::SetClipboardText()` / `ImGui::GetClipboardText()`.

The theme and the clipboard image do not go through the window library (`ImPlatform_native.cpp`), so they are the
same with GLFW, SDL2 and SDL3:

| System | Theme | Clipboard image |
|---|---|---|
| Windows | the registry's app mode | bitmap formats of the clipboard |
| macOS | the user's interface style | PNG or TIFF of the pasteboard, decoded by ImageIO |
| Linux | the settings portal over D-Bus | PNG of the X11 clipboard (SDL3: of SDL's clipboard), decoded by libpng |

On Linux, libdbus, libpng and libX11 are loaded at run time and are no build dependency: without one of them the
theme is `Unknown`, or `ImPlatform_SupportsClipboardImage()` is false. An image copied there stays on the clipboard
while the application runs, as any X11 selection does.

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

**Note**: On GLFW the custom titlebar works with stock GLFW: the window is created undecorated and ImPlatform moves it while the titlebar is dragged and resizes it by its edges (`ImPlatform_BorderlessParams::resizeBorderSize`), without the system's window snapping. [TheCherno's GLFW fork](https://github.com/TheCherno/glfw/tree/dev) with `IM_THE_CHERNO_GLFW3` defined gives native titlebar hit-testing instead. Win32, SDL2 and SDL3 hit-test natively (move and resize borders).

## Platform & Graphics API Defines

### Platform Targets

```cpp
IM_PLATFORM_WIN32   // Windows (Win32 API)
IM_PLATFORM_GLFW    // GLFW (cross-platform)
IM_PLATFORM_SDL2    // SDL2
IM_PLATFORM_SDL3    // SDL3
IM_PLATFORM_APPLE   // macOS: a Cocoa window, without GLFW or SDL (with IM_GFX_METAL)
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
| `VERTEX_BUFFERS` | vertex / index buffers, `DrawIndexed` | x | x | x | x | x | x | x | x |
| `DIRECT_DRAW` | `Push/PopShaderConstants`, `GetCurrentTargetSize` | x | | x | x | x | x | | x |
| `RENDER_TARGETS` | `Push/PopRenderTarget`, `CopyCurrentTarget`, `DrawQuadNow` | x | | x | x | x | x | | |
| `READBACK` | `ReadTexture` | x | x | x | x | x | x | x | |
| `ASYNC_READBACK` | `BeginReadTexture`, `Poll/Map/ReleaseReadback` | | | x | x | | | | |
| `ASYNC_COMPUTE` | `DispatchComputeAsync`, `Poll/ReleaseComputeDispatch` | | | x | x | 4.3+ | x | | |
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
`ImPlatform_GfxAPISwapBuffer`, textures, shaders with `ImPlatform_GetShaderLog` and `ImPlatform_SetShaderProgramBlend`,
the render passes `ImPlatform_GetRenderPassFormat` / `QueueRenderPass` / `PushPremultipliedAlpha` /
`PopPremultipliedAlpha`, the samplers `ImPlatform_PushSamplerEx` / `PopSamplerEx`, the `Supports*` queries, and the functions of every optional group above that it keeps) plus:

```cpp
// Called by the platform layer (Win32, GLFW, SDL2, SDL3) when the window's framebuffer is resized.
void ImPlatform_Gfx_OnResize_Custom(unsigned int uWidth, unsigned int uHeight);
```

The `IMPLATFORM_GFX_SUPPORT_*` feature flags and API groups default to 1 and can each be predefined to 0
to describe what the application's backend supports (a group set to 0 is neither declared nor needed). With GLFW the window is created with
`GLFW_NO_API`. The asynchronous shader jobs, `ImPlatform_BlendDesc_Preset` and `ImPlatform_SetShaderProgramBlendPreset` stay ImPlatform's: a job calls the
application's `ImPlatform_CreateShader` and `ImPlatform_CreateShaderProgram` when it is created.

### Predefined Combinations

For convenience, you can also use these predefined target combinations:

```cpp
IM_TARGET_WIN32_DX10
IM_TARGET_WIN32_DX11
IM_TARGET_WIN32_DX12
IM_TARGET_WIN32_OPENGL3
IM_TARGET_WIN32_VULKAN

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
| Win32        | ✅       | ✅  | ✅   | ✅   | ✅   | ✅     | ❌    | ❌     |
| GLFW         | ✅       | ❌  | ❌   | ❌   | ❌   | ✅     | ✅    | ✅     |
| SDL2         | ✅       | ❌  | ❌   | ✅   | ❌   | ✅     | ✅    | ❌     |
| SDL3         | ✅       | ❌  | ❌   | ✅   | ❌   | ✅     | ✅    | ❌     |
| Apple        | ❌       | ❌  | ❌   | ❌   | ❌   | ❌     | ✅    | ❌     |

Legend: ✅ Implemented | ❌ Not Implemented. GLFW + WebGPU is the Emscripten build.

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
| Windows | `Win32_DX9`, `Win32_DX10`, `Win32_DX11`, `Win32_DX12`, `Win32_OpenGL3`, `Win32_Vulkan`, `GLFW_OpenGL3`, `GLFW_Vulkan`, `SDL2_DX11`, `SDL2_OpenGL3`, `SDL2_Vulkan`, `SDL3_DX11`, `SDL3_OpenGL3`, `SDL3_Vulkan` |
| Linux | `GLFW_OpenGL3`, `GLFW_Vulkan`, `SDL2_OpenGL3`, `SDL2_Vulkan`, `SDL3_OpenGL3`, `SDL3_Vulkan` |
| macOS | `GLFW_OpenGL3`, `GLFW_Metal`, `SDL2_OpenGL3`, `SDL2_Metal`, `SDL3_OpenGL3`, `SDL3_Metal`, `Apple_Metal` |

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
ImPlatformDemo_Win32_DX12 --compute-async-test # the same with the shader compiled asynchronously and the dispatch polled
ImPlatformDemo_Win32_DX12 --render-pass-test   # render pass target and premultiplied composite
ImPlatformDemo_Win32_DX12 --shader-test        # shaders/gradient.hlsl drawn into a render pass, read back
ImPlatformDemo_Win32_DX12 --sampler-test       # nearest and linear samplers on a render pass list; scopes kept across a custom shader
ImPlatformDemo_Win32_DX12 --device-lost-test   # the device removed on purpose: frames skipped without stalling, then recreated and a frame read back
ImPlatformDemo_Win32_DX12 --direct-draw-test   # from a draw callback: push a render target, push constants, DrawQuadNow
ImPlatformDemo_Win32_DX12 --shader-parts-test  # a shader written as parts: parameters in both stages, varying, texture t1
ImPlatformDemo_Win32_DX12 --texture3d-test     # a 2x2x2 texture at t1, sampled by shaders/volume_test.hlsl and by shader parts
ImPlatformDemo_Win32_DX12 --vertex-inputs-test  # shader parts with their own vertex inputs and a flat int varying, DrawIndexed
ImPlatformDemo_Win32_DX12 --buffer-update-test # Dynamic and Stream vertex buffers written and drawn twice a frame: each draw keeps its content
ImPlatformDemo_Win32_DX12 --mesh-stream-test   # the same through ImPlatform_DrawMesh, the Stream buffer created and destroyed each frame
ImPlatformDemo_Win32_DX12 --blend-test         # one program drawn with six blend presets (additive to lighten), set by name from draw callbacks
ImPlatformDemo_Win32_DX12 --shader-async-test  # programs compiled asynchronously, a defines permutation, the log of a shader that fails
ImPlatformDemo_Win32_DX12 --screenshot-test    # the main viewport captured and read back
ImPlatformDemo_SDL3_OpenGL3 --pen-test         # SDL3: synthetic pen events come out as pen samples (exit 2 elsewhere)
ImPlatformDemo_SDL3_OpenGL3 --touch-test       # two fingers touch, spread, turn and lift, then a trackpad pinch: touch points and gesture (exit 2 on GLFW)
ImPlatformDemo_Win32_DX11 --drop-test          # a drag with a file and a URL enters, moves, drops, leaves: the drop events (exit 2 on GLFW off Windows)
ImPlatformDemo_Win32_DX11 --drag-out-test      # a file and a text dragged out and dropped back on the window (on Windows it moves the mouse cursor there and back)
ImPlatformDemo_GLFW_OpenGL3 --drop-wait-test  # waits ten seconds for a drag from another application and prints its events
ImPlatformDemo_Win32_DX11 --window-test        # fullscreen and back, always on top, icon; prints the system theme
ImPlatformDemo_Win32_DX11 --clipboard-test     # an image to the clipboard and back (it replaces the clipboard's content); prints the system theme
ImPlatformDemo_GLFW_OpenGL3 --drag-rect-test   # the move / resize arithmetic of the borderless GLFW window, without a window
```

Each prints PASS or FAIL and exits with 0 (pass), 1 (fail) or 2 (unsupported by the backend).

`--dump-msl <shader_parts|texture3d|vertex_inputs> <vs|ps>` prints the MSL generated from those tests' shader parts,
without a window or device, in any configuration. CI compiles it with the offline Metal compiler (`xcrun metal -c`),
next to the Metal self-tests it runs.

## Project Structure

```
ImPlatform/
├── ImPlatform/
│   ├── ImPlatform.h              # Main header with API declarations
│   ├── ImPlatform_Internal.h    # Internal data structures and helpers
│   ├── ImPlatform_app_*.cpp     # Platform backends (Win32, GLFW, SDL, Apple)
│   ├── ImPlatform_gfx_*.cpp     # Graphics API backends (DX, OpenGL, Vulkan, etc.)
│   ├── ImPlatform_shader_parts.cpp  # HLSL / GLSL / MSL generation from shader parts
│   ├── ImPlatform_shader_common.cpp # Shader compile log, blend presets, asynchronous program compilation
│   ├── ImPlatform_native.cpp    # System theme, clipboard images, macOS gestures: taken from the system itself
│   └── ImPlatform_titlebar.cpp  # Custom titlebar drawing
├── ImPlatformDemo/
│   ├── main.cpp                  # Example application and self-tests
│   └── shaders/                  # Demo HLSL shaders, compiled headers and compile_shaders.*
├── buildsystem/
│   ├── generate_projects.*       # Runs Sharpmake
│   └── sharpmake/src/            # Project definitions (main.cs, common.cs, ImPlatformDemo.cs)
├── extern/Sharpmake/             # Sharpmake (submodule)
├── imgui/                        # Dear ImGui (submodule)
├── CMakeLists.txt                # CMake mirror of the Sharpmake configurations
├── CHANGELOG.md / RELEASE_NOTES.md
└── README.md
```

## Migration from Old API

Upgrading from 0.5.0 to 2.0.0: see [RELEASE_NOTES.md](RELEASE_NOTES.md) (main loop, compile-time optional functions,
shader register convention, build).

Before 0.5.0, the library was refactored from a C++ namespace API to a clean C API. Key changes:

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
- **Windows 10/11** (Win32 API) - Tested (self-tests)
- **Windows 10/11** (GLFW) - Tested (self-tests)
- **Windows 10/11** (SDL2) - Tested (self-tests)
- **Windows 10/11** (SDL3) - Tested (self-tests)
- **macOS** (GLFW, SDL2, SDL3) - Builds in CI; not run on a Mac with a GPU yet (contributions welcome!)
- **Linux** (GLFW, SDL2, SDL3) - Tested (self-tests, Ubuntu 24.04 with Mesa)

### Graphics APIs Tested
- **DirectX 9** - Tested (self-tests, Win32)
- **DirectX 10** - Tested (self-tests, Win32)
- **DirectX 11** - Tested (self-tests, Win32, SDL2, SDL3)
- **DirectX 12** - Tested (self-tests, Win32)
- **OpenGL 3** - Tested (self-tests, Windows and Linux)
- **Vulkan** - Tested (self-tests, Windows and Linux, Win32, GLFW, SDL2, SDL3)
- **Metal** - Tested in CI (self-tests of `GLFW_Metal` and `Apple_Metal` on the hosted macOS runner, a virtual machine); not tried by hand on a Mac
- **WebGPU** - Basic testing (Emscripten + GLFW in Chrome)

### Dear ImGui Version
- **Tested with**: Dear ImGui docking branch after v1.93.0 (1.93.1 WIP)
- **Minimum required**: Dear ImGui v1.92.8
- **Recommended**: Use the latest docking branch for full multi-viewport support

## Known Limitations

- macOS: the Metal self-tests run in CI on a virtual machine, with GLFW and with the Cocoa platform. OpenGL gets no pixel format there, the SDL configurations are only built, and nothing has been tried by hand on a Mac
- WebGPU: no readback, and no render-target switch or copy (its render pass encoder belongs to Dear ImGui's backend)
- Vulkan: at most 968 textures and render textures alive at once (the descriptor pool shared with Dear ImGui's backend); creating one more returns 0
- DirectX 9 with Dear ImGui 1.93.1 and later: a custom program draws Dear ImGui's vertices when bound with `ImPlatform_BeginCustomShader` (a draw list scope); one bound with `ImPlatform_BeginCustomShader_Render` is for the application's own draws
- DirectX 9 shaders are shader model 3: no integer or bit operations, no texel loads, `nointerpolation` varyings are interpolated
- OpenGL3 multi-viewport with custom shaders may have minor offset issues in some edge cases
- Custom titlebar on stock GLFW: the window is moved and resized by ImPlatform, without the system's window snapping (TheCherno's fork with `IM_THE_CHERNO_GLFW3` gives native hit-testing)
- Off Windows, pen input needs SDL3 and touch input SDL2 or SDL3 (GLFW has neither there, SDL2 no pen)
- On macOS and Wayland, GLFW and SDL2 report a drop at the release (GLFW: files only), and nothing can be dragged out of the application; nor with SDL3 on Linux
- Clipboard images on Linux need libpng 1.6 and, with GLFW or SDL2, libX11 at run time
- The macOS clipboard image test passes in CI; the trackpad gestures compile there but have not been tried on a trackpad
- A trackpad's pinch is recognized on Windows as the Ctrl-marked wheel messages a precision touchpad sends; it has not been tried on a touchpad. Linux reports one through SDL 3.4+ only
- `ImPlatform_CopyBackbuffer()` is DirectX 11 only; `ImPlatform_CaptureViewport()` covers DirectX 9, 11, 12, OpenGL and Vulkan (main viewport only on DirectX 12 and Vulkan)

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
