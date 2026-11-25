# Changelog

## 3.0.0

Changes since 2.0.0 (the `v2` branch), in progress on the `v3` branch.

### Added
- **Blending of custom shader programs** on every backend: by name with `ImPlatform_SetShaderProgramBlendPreset()`
  (alpha, premultiplied, opaque, additive, subtractive, multiplicative, screen, darken, lighten), or
  `ImPlatform_SetShaderProgramBlend()` with an `ImPlatform_BlendDesc` (factors and operation for color and for alpha;
  `ImPlatform_BlendDesc_Preset()` gives a preset's). It applies to the draw list commands of the program,
  `ImPlatform_DrawIndexed()` and `ImPlatform_DrawQuadNow()`. Demo `--blend-test`.
- **Shader compile errors for the application**: `ImPlatform_GetShaderLog()` returns the compiler's or linker's messages
  of the last create call that failed; for shader parts the generated source follows with line numbers.
- **Shader permutations**: `ImPlatform_ShaderDesc::defines` ("CLIP;MODE=2") for `#ifdef` / `#if` in HLSL, GLSL and MSL
  source, with the compiler's line numbers kept and one bytecode cache entry per combination.
- **Asynchronous program compilation**: `ImPlatform_CreateShaderProgramAsync()` /
  `ImPlatform_CreateShaderProgramFromPartsAsync()` return a job at once, `ImPlatform_GetShaderJobStatus()` reports
  compiling, ready or failed and gives the program, `ImPlatform_GetShaderJobLog()` the messages. DirectX 10, 11 and 12
  compile the HLSL on a worker thread; the other backends compile inside the create call. Demo `--shader-async-test`.
- **Asynchronous compute** on every backend with compute (DirectX 11, 12, OpenGL 4.3+, Vulkan):
  `ImPlatform_CreateComputeShaderAsync()` compiles a compute shader as a job (`ImPlatform_GetShaderJobShader()` gives it
  once ready), and `ImPlatform_DispatchComputeAsync()` returns a handle that `ImPlatform_PollComputeDispatch()` reports
  completed, without ever waiting for the GPU (new group `IMPLATFORM_GFX_SUPPORT_ASYNC_COMPUTE`). Demo
  `--compute-async-test`.
- **Screenshots**: `ImPlatform_CaptureViewport()` returns a render texture that holds the image of a viewport once the
  frame is rendered, `ImPlatform_ReadCapture()` its pixels as RGBA8 (DirectX 9, 11, 12, OpenGL, Vulkan; main viewport
  only on DirectX 12 and Vulkan). Demo `--screenshot-test`, `--screenshot-viewport-test`.
- **Pen input on SDL3** (Windows, Linux, macOS) from its pen events: pressure, tilt, rotation, eraser, contact. Demo
  `--pen-test` (synthetic events).
- **macOS without a window library** (`IM_PLATFORM_APPLE` with `IM_GFX_METAL`, configuration `Apple_Metal`): a Cocoa
  window whose events are polled once per frame, with Dear ImGui's `imgui_impl_osx` for the input and the platform
  windows. It has the system's titlebar: the custom titlebar, file drop and window state calls are not declared
  there. The file is Objective-C++ with ARC and needs the GameController framework. `ImPlatform_app_apple.mm` was
  an unfinished file that no configuration built.
- **Samplers on any draw list**: `ImPlatform_PushSamplerEx()` / `ImPlatform_PopSamplerEx()` take the draw list, so a
  render pass's list can change its sampler (`ImPlatform_PushSampler()` reaches the current window's only). Demo
  `--sampler-test`: nearest and linear rows read back, on every backend with readback.
- Demo `--viewports`: multi-viewports with every Dear ImGui window in a platform window of its own, the custom
  shaders included. The CI screenshots of Linux and macOS show every configuration, as it starts and in that mode.
- **Touch input** (Win32 pointer messages, SDL2 and SDL3 finger events; main window): `ImPlatform_SupportsTouch()`,
  `ImPlatform_GetTouchPoints()` gives the fingers in contact, `ImPlatform_ConsumeTouchGesture()` what they did since
  the previous call: pan, pinch zoom and rotation, computed from the finger positions the same way on every platform.
  A trackpad's pinch comes through the same gesture as its zoom (Win32 precision touchpads, macOS with every window
  library, where the two-finger turn is its rotation, SDL 3.4 and later).
  Demo `--touch-test` (synthetic SDL finger events; the recognizer alone on Win32), and a "Gestures" window whose
  square follows the gesture, to try a real trackpad or touch screen.
- **Drag and drop from other applications**: `ImPlatform_SetDropCallback()` receives every event of a drag over the
  main window: begin, position, one event per dropped file and per dropped text (a URL for a link or an image dragged
  from a browser), end. Win32 (an OLE drop target) and SDL3 report the drag while it hovers; SDL2 and GLFW at the
  release. `ImPlatform_SetDropFileCallback()` still receives each file. Demo `--drop-test`.
- **Window state** (`IMPLATFORM_APP_SUPPORT_WINDOW_STATE`: Win32, GLFW, SDL2, SDL3): `ImPlatform_SetFullscreen()` /
  `ImPlatform_IsFullscreen()` (borderless, on the monitor the window is on), `ImPlatform_SetAlwaysOnTop()`,
  `ImPlatform_SetWindowIcon()` from RGBA pixels. Demo `--window-test`.
- **System theme**: `ImPlatform_GetSystemTheme()` gives the user's light or dark mode on Windows, macOS and Linux
  (the settings portal).
- **Clipboard images**: `ImPlatform_SetClipboardImage()`, `ImPlatform_GetClipboardImageSize()`,
  `ImPlatform_GetClipboardImage()` as RGBA pixels on Windows, macOS and Linux; `ImPlatform_SupportsClipboardImage()`
  tells whether the system has what it takes. Demo `--clipboard-test`.
- **`ImPlatform_native.cpp`**: the theme, the clipboard image and the macOS trackpad gestures are taken from the
  system itself, so they do not depend on what GLFW, SDL2 or SDL3 expose. macOS: CoreFoundation, the pasteboard,
  ImageIO and the Objective-C runtime, from C++. Linux: libdbus, libpng and libX11, loaded at run time (no new build
  dependency). macOS builds link the ApplicationServices framework.
- **Windows, whichever library made the window**: the Windows Ink pen, the touch points, the trackpad pinch and the
  OLE drop target of the Win32 backend are those of a GLFW or SDL2 window too (`ImPlatform_native.cpp`; SDL3 reads them
  itself). GLFW gains pen and touch there, SDL2 the pen, and both report a drag while it hovers and take dropped text.
- **Drops on Dear ImGui's platform windows** (multi-viewports) on Windows and with SDL3.
- **Dragging out of the application**: `ImPlatform_DragOut()` gives files and text to the application the user drops
  them on (Windows; `ImPlatform_SupportsDragOut()`). Demo `--drop-test` drags out and drops on its own window.
- **Linux on X11, whichever library made the window**: a GLFW or SDL2 window follows the drag protocol itself
  (XDND, through the window's proxy property), so it reports a drag while it hovers and takes files, links and text.
  libX11 is loaded at run time. Demo `--drop-wait-test` prints the events of a drag made by another application.
  `ImPlatform_DragOut()` works there too, as the source of the same protocol: the pointer is followed while a button
  is held and the window under it takes the files or the text at the release. Demo `--drag-out-test`.
- **Win32 + Vulkan** build configuration (Sharpmake `Win32_Vulkan`, CMake `ImPlatformDemo_Win32_Vulkan`, CI) and
  `IM_TARGET_WIN32_VULKAN`.
- **Borderless window on stock GLFW** is resized by its edges (`ImPlatform_BorderlessParams::resizeBorderSize`,
  `allowResize`), with the matching mouse cursors. Demo `--drag-rect-test`.

### Changed
- **Stream vertex / index buffers** (`ImPlatform_BufferUsage_Stream`) hold their content for one frame: the frame they
  were created in, or last updated as a whole in. DirectX 12 takes their memory from the per-frame upload arena (no
  committed resource per buffer, no deferred release, no wait), and a Stream buffer drawn in a later frame without an
  update draws nothing there. Vulkan retires destroyed buffers and takes them again once the frames that could draw
  them have completed: `ImPlatform_DestroyVertexBuffer()` / `ImPlatform_DestroyIndexBuffer()` no longer wait for the
  device. DirectX 9, 10, 11, Metal and WebGPU keep destroyed Stream buffers for the next one of a close size.
- **`ImPlatform_UpdateVertexBuffer()` / `ImPlatform_UpdateIndexBuffer()` may be called every frame**, and between two
  draws of a frame: draws already recorded keep the content they were recorded with. DirectX 12, Vulkan and Metal wrote
  into memory the GPU could still be reading; they now write another version of the buffer and take it back once the
  frames that read it have completed. WebGPU applied an update to the draws already recorded in the frame; it writes
  another buffer too. On DirectX 10 and 11 Dynamic buffers are updated by `UpdateSubresource`, on
  DirectX 9 they are not dynamic buffers: a partial update is ordered with the draws as a whole one is. Demo
  `--buffer-update-test`, `--mesh-stream-test`.
- **Dear ImGui's docking branch after 1.93.0** (1.93.1 WIP, on the way to 1.94) is the version of the `imgui`
  submodule. ImPlatform needs 1.92.8 or later: it uses `ImGui::GetPlatformIO().DrawCallback_ResetRenderState` and
  `IM_COUNTOF`.
- DirectX 9 with Dear ImGui 1.93.1 and later, whose backend sends pre-transformed vertices that Direct3D gives to no
  vertex shader: a program of `ImPlatform_BeginCustomShader()` declares them as plain positions, in the pixels of the
  target, and `ImPlatform_EndCustomShader()` puts Dear ImGui's format back. A program bound with
  `ImPlatform_BeginCustomShader_Render()` or `ImPlatform_UseShaderProgram()` is for the application's own draws there:
  Dear ImGui's draws would not run its vertex shader. The vertex format of both versions is handled.
- DirectX 12: the waits on the frame's fence and on the swap chain end when the device is lost, as in Dear ImGui's
  example, instead of blocking forever. `ImPlatform_GfxCheck()` returns false on a lost device: the loop skips its
  frames and keeps handling the window's messages until `ImPlatform_GfxRecreateDevice()`.
- DirectX 12: `ImPlatform_GfxRecreateDevice()` works again. It stopped on a Dear ImGui assert (the main viewport
  kept its platform data while the renderer backend shut down), and it found no device once a premultiplied alpha scope
  had been drawn: the backend's own program stayed on the removed device, and Direct3D creates no device on an adapter
  whose removed one is still alive. `ImPlatform_ShutdownWindow()` after a recreation that failed no longer asserts.
  Demo `--device-lost-test`: the device removed on purpose, the frames skipped, then the device recreated and a frame
  of it read back.
- **`ImPlatform_PushSampler()` / `ImPlatform_PopSampler()` are implemented once**, on
  `ImPlatform_PushSamplerEx()` / `ImPlatform_PopSamplerEx()`: an `IM_GFX_CUSTOM` application provides those two
  instead (the same functions with the draw list as first argument, NULL for the current window's).
- **DirectX 9 creates a Direct3D 9Ex device**: a reset (every resize of the window) keeps the textures, render textures
  and buffers. Direct3D 9Ex has no managed pool: ImPlatform's textures and buffers are in the default pool, and what an
  application creates itself on `ImPlatform_GfxData_DX9::pDevice` must be too.
- DirectX 12: the sampler `s0` of a custom shader program clamps and reads every mip level, as on the other backends
  (it wrapped and read the first level only).
- WebGPU: the surface format is chosen in the order of Dear ImGui's example (BGRA8, RGBA8, then their sRGB variants).
- Demo: frames and grabs are rounded as in Dear ImGui's examples.
- `ImPlatform_GfxData_Vulkan` (`ImPlatform_Internal.h`) no longer has the members that were never filled (swapchain,
  render pass, command buffers, semaphores, fences, pipeline cache): they belong to Dear ImGui's
  `ImGui_ImplVulkanH_Window`.
- DirectX 10, 11 and 12 share one HLSL compile function (`ImPlatform_D3DCompileShader` in `ImPlatform_Internal.h`):
  DirectX 10 and 12 print the compile time of a shader as DirectX 11 did.
- An `IM_GFX_CUSTOM` application also provides `ImPlatform_GetShaderLog()` and `ImPlatform_SetShaderProgramBlend()`.
- New source file `ImPlatform/ImPlatform_shader_common.cpp` (included by `ImPlatform.h`; listed in `CMakeLists.txt`).

### Fixed
- Metal: `ImPlatform_InitGfxAPI()` returned false on every configuration, so no Metal application started. It now
  creates the device and its layer on GLFW's Cocoa window or on SDL's Metal view, and a frame takes one drawable and
  one command buffer, presented by `ImPlatform_GfxAPISwapBuffer()`, as Dear ImGui's examples do. `ImPlatform_PopSampler()`
  restores the linear clamped sampler instead of none. The Metal self-tests now run in CI: they were skipped as
  "no GPU" while the device was never created.
- DirectX 9: resizing the window while a render texture existed failed the device reset (an assertion in debug
  builds). A render texture larger than the window can be drawn into (the window's depth buffer is unbound meanwhile).
  `ImPlatform_DrawQuadNow()` left the vertex stream unbound, which dropped the rest of the frame's interface; it also
  puts back the vertex format, which Dear ImGui sets once per draw data.
- DirectX 12: `ImPlatform_ShutdownWindow()` forgets the viewport render callback it wrapped, so the device can be
  initialized again.
- The DPI callback on GLFW reports the scale Dear ImGui's backend uses (1 on macOS and Wayland, where the framebuffer
  scale does the scaling) instead of GLFW's raw content scale, which scaled the interface twice there. On SDL2 and
  SDL3 it is now called when the window moves to a display of another scale.
- Custom titlebar: the main viewport's work area is shortened by the titlebar's height; it only moved down, so a
  window fitted to the work area ran past the bottom of the window.
- `ImPlatform_EndCustomShader()` no longer ends the sampler and the premultiplied alpha scopes it is inside of: its
  reset of the render state went back to the linear sampler and to straight alpha blending for the draws that followed,
  on every backend. The scopes open on a draw list are recorded as it is built, and the end of a custom shader sets
  the innermost of each again.
- Vulkan: the pipelines of a custom shader program are kept per target format instead of per render pass. Every
  resize of the window and every render texture has a render pass of its own, so the pipelines added up, and one could
  be matched to a new render pass that reused the handle of a destroyed one.
- Vulkan: creating a texture or a render texture returns 0 once 968 are alive, instead of aborting the application
  when the descriptor pool shared with Dear ImGui's backend ran out.
- OpenGL: `ImPlatform_DrawQuadNow()`, `ImPlatform_DrawIndexed()`, `ImPlatform_CopyCurrentTarget()` and
  `ImPlatform_ReadTexture()` create their vertex array or framebuffer object for the call. They kept one made in the
  main window's context, which is not valid in the contexts GLFW and SDL give the other platform windows.
  `ImPlatform_DrawIndexed()` puts back the vertex array object and array buffer bound before it, and creating a vertex
  or index buffer leaves the bindings as found (inside a draw callback they are Dear ImGui's).
- DirectX 12: `ImPlatform_PushSampler()` / `ImPlatform_PopSampler()` between `ImPlatform_BeginCustomShader()` and
  `ImPlatform_EndCustomShader()` bound Dear ImGui's pipeline in place of the program's. The program keeps its sampler,
  and the pushed one applies from the end of the scope.
- DirectX 9: `ImPlatform_GfxAPIRender()` draws nothing when the frame's `BeginScene` failed; it read draw data that
  was not rendered.
- `ImPlatform.h` says what the filter and wrap members of `ImPlatform_TextureDesc` apply to: not to `ImGui::Image()`,
  which samples with Dear ImGui's sampler (see `ImPlatform_PushSampler()`).
- The position of a drop is in the space of `ImGui::GetMousePos()`, as documented: with multi-viewports enabled it was
  still relative to the main window.
- `ImPlatform_UpdateVertexBuffer()` / `ImPlatform_UpdateIndexBuffer()` took their last two arguments as (count, offset)
  on DirectX 9, 10, 11, 12, Vulkan and WebGPU, against the header's (offset, count) that OpenGL and Metal followed: a
  call written for the header updated nothing there. Every backend now follows the header; a call that passed
  (count, offset) to suit those backends must swap them.
- A partial update discarded the rest of the buffer on DirectX 9, 10 and 11; it now keeps it. Updates are checked
  against the buffer's size there.
- OpenGL: an update left no buffer bound, which broke Dear ImGui's draws when called from a draw callback.
- The titlebar drag of the borderless window on stock GLFW compared cursor positions relative to the window it was
  moving. It now works in screen coordinates and starts only on a press.
- The Emscripten build of the demo referenced `--dump-msl`, which is not compiled there.
- README: the custom titlebar does not require TheCherno's GLFW fork; SDL2 and SDL3 have it too.

## 2.0.0

Changes since 0.5.0 (the `main` branch).

### Added
- **Render passes** on every backend (DirectX 9, 10, 11, 12, OpenGL 3, Vulkan, WebGPU, Metal):
  `ImPlatform_QueueRenderPass()` renders Dear ImGui draw data into a render texture before the frame, with the backend's
  own ImGui renderer (draw callbacks included). `ImPlatform_GetRenderPassFormat()` gives the target format.
- **Premultiplied alpha**: `ImPlatform_PushPremultipliedAlpha()` / `ImPlatform_PopPremultipliedAlpha()` around draw-list
  commands that composite premultiplied images (render pass results); the vertex color is premultiplied too.
- **Render-target stack**: `ImPlatform_PushRenderTarget()` / `ImPlatform_PopRenderTarget()` (nestable, optional clear),
  `ImPlatform_CopyCurrentTarget()`, `ImPlatform_GetCurrentTargetSize()`, `ImPlatform_DrawQuadNow()`.
- **Readback**: `ImPlatform_SupportsReadback()`, `ImPlatform_ReadTexture()` (DirectX 9, 10, 11, 12, OpenGL 3, Vulkan, Metal);
  asynchronous `ImPlatform_BeginReadTexture()` / `PollReadback()` / `MapReadback()` / `ReleaseReadback()` on DirectX 11
  and 12, with pooled, persistently mapped readback buffers.
- **Compute**: `ImPlatform_SupportsCompute()`, `ImPlatform_CreateStorageTexture()`, `ImPlatform_DispatchCompute()`
  (DirectX 11, 12, Vulkan, OpenGL 4.3+ checked at run time).
- **Depth-tested drawing** (DirectX 11, 12): `ImPlatform_SupportsDepthDraw()`,
  `ImPlatform_CreateRenderTextureWithDepth()`, `ImPlatform_DrawMesh()` with depth, blend and cull states.
- **Device control** (DirectX 11, 12): `ImPlatform_GfxDeviceLost()`, `ImPlatform_GfxDeviceReady()`,
  `ImPlatform_GfxRecreateDevice()`, `ImPlatform_GfxAdapterName()`, and testing aids
  (`ImPlatform_GfxDebugRemoveDevice()`, `GfxDebugSetMemoryLimit()`, `GfxDebugFailTextureStep()`,
  `GfxDebugLiveCounts()`, `GfxDebugGpuTimer()` / `GfxDebugGpuTimings()`). The device is created on the
  high-performance adapter.
- **Pen input** (Win32, Windows Ink): `ImPlatform_SupportsPen()`, `ImPlatform_GetPenState()`,
  `ImPlatform_ConsumePenSamples()`, `ImPlatform_GetPenClock()`: pressure, tilt, barrel rotation, contact, eraser and
  coalesced samples. Other platforms report no support.
- **`IM_GFX_CUSTOM`**: keeps the platform layer and compiles no graphics backend; the application links its own
  graphics entry points and `ImPlatform_Gfx_OnResize_Custom()`.
- DirectX 12, Vulkan and WebGPU: custom shader programs bind textures t1..t8 (`ImPlatform_SetShaderTexture()`, any
  ImPlatform texture, kept by the program); DirectX 12 static vertex and index buffers live in device memory.
- `ImPlatform_IsMaximized()` is public.
- Demo self-tests: `--compute-test`, `--render-pass-test`, `--shader-test` (custom shader drawn into a render pass
  and read back) and `--direct-draw-test` (from a draw callback: a pushed render target, pushed constants and
  `ImPlatform_DrawQuadNow`, read back); exit code 0 pass, 1 fail, 2 unsupported.
- Direct draws from draw callbacks on DirectX 12 and Vulkan, as on DirectX 11 and OpenGL: `ImPlatform_PushRenderTarget()`
  / `PopRenderTarget()`, `CopyCurrentTarget()`, `GetCurrentTargetSize()`, `DrawQuadNow()`, `PushShaderConstants()` and
  `BindBuffers()` + `DrawIndexed()` with the vertex buffer's own layout (a pipeline per layout, kept by the program).
  WebGPU gets `PushShaderConstants()`, `DrawIndexed()` with custom layouts and `GetCurrentTargetSize()`; its render pass
  encoder belongs to the ImGui backend, so it has no target switch or copy. Vulkan vertex and index buffers, and vertex
  shaders with inputs other than `ImDrawVert`'s (drawn with `DrawIndexed()` only), on Vulkan and WebGPU.
- `ImPlatform_SetShaderTexture()` slot 0 (DirectX 12, Vulkan, WebGPU): the texture of the direct draws.
- Custom shaders written once in plain HLSL: `ImPlatformDemo/shaders/compile_shaders.*` compiles each `.hlsl` with
  [Slang](https://github.com/shader-slang/slang) to SPIR-V (Vulkan), WGSL (WebGPU) and, through SPIRV-Cross, to GLSL 330
  (OpenGL, macOS included); Direct3D compiles the HLSL itself. `buildsystem/fetch_slang.*` downloads the pinned Slang release.
  A compute shader (entry point `cs_main`) gets SPIR-V and GLSL 430 with `ImPlatform_DispatchCompute()`'s bindings; the
  demo's `--compute-test` shader is one HLSL file.
- **Shader parts**: `ImPlatform_CreateShaderFromParts()` builds a custom shader written once (parameter block, varyings,
  textures, helper functions, vertex and pixel bodies in HLSL plus `IMP_*` macros) for the backend at run time: HLSL
  on DirectX 9 to 12, GLSL 330 on OpenGL, MSL on Metal (`IMPLATFORM_GFX_SUPPORT_SHADER_PARTS`).
  `ImPlatform_GenerateShaderSource()` gives the text of any language. Demo: `--shader-parts-test`.
  `asuint`, `asint`, `asfloat` and `f16tof32` map to GLSL and MSL (not DirectX 9: no bit operations in shader model 3);
  `//` comments in the parameter block may hold `;`.
- **3D textures on every backend**: `ImPlatform_CreateTexture3D()` was DirectX 11 only; DirectX 9 (volume textures, when
  the device reports them), 10, 12, OpenGL 3, Vulkan, Metal and WebGPU now create them too, bound with
  `ImPlatform_SetShaderTexture()` to a slot the shader declares `Texture3D`. Shader parts declare one with a
  `<slot> <name> 3d` texture line (`ImPlatform_ShaderParts::textures_3d`). Demo: `--texture3d-test`.
- **Shader parts with their own vertex inputs** (`vertex_inputs`, `//@inputs`: `float4 pos : POSITION; ...` in the
  vertex buffer's attribute order, `in_<name>` in the vertex body) for `ImPlatform_DrawIndexed`, and flat varyings
  (`nointerpolation int4 id;`: `flat` in GLSL, `[[flat]]` in MSL; not on DirectX 9). Demo: `--vertex-inputs-test`; `--dump-msl` prints the MSL generated from the tests' parts, compiled
  in CI by the offline Metal compiler.
- Metal: vertex and index buffers (`IMPLATFORM_GFX_SUPPORT_VERTEX_BUFFERS`): shared storage buffers drawn by
  `ImPlatform_DrawIndexed()` from a draw callback, with a pipeline per vertex buffer layout (attribute i at location i);
  a program whose inputs are not Dear ImGui's vertex draws vertex buffers only.
- CI: a failing self-test's output becomes an error annotation of the run.
- `ImPlatform_CreateVertexInputLayout()` is in the `VERTEX_BUFFERS` group and implemented by every backend in it: a
  vertex declaration on DirectX 9 and an input layout on DirectX 10 (custom vertex buffers drew without one there),
  a check of its arguments where the layout is built at draw time. DirectX 11 maps `UByte4N` vertex attributes.
- Shader parts on DirectX 9 define `IMP_SM3` (shader model 3: 224 pixel constant registers, no integer or bit
  operations, no texel loads, no flat varyings), so a shader can take another path there; `nointerpolation` varyings
  are interpolated there.
- Shader parts: `ddx` / `ddy` in a helper function compile in the vertex stage too (a zero there; GLSL and MSL have
  derivatives in fragment shaders only).
- Metal: `ImPlatform_SetShaderTexture()` slots 1..8 (both stages).
- The custom shader parameters (`b1`) are bound to the vertex stage too on DirectX 10, 11 and 12 (vertex animation).
- DirectX 9 custom shaders: the same HLSL, compiled for `vs_3_0` / `ps_3_0`; cbuffer members are uploaded to the
  constant registers shader model 3 assigns them, and texture `tN` is sampler N (`ImPlatform_SetShaderTexture()` slots
  0..8). DirectX 9 also gets the render-target stack, `ImPlatform_CopyCurrentTarget()` (StretchRect),
  `ImPlatform_DrawQuadNow()` and `ImPlatform_PushShaderConstants()`.
- Vulkan: render textures take copies (`ImPlatform_CopyCurrentTarget()` into them) and start in their shader-read layout.
- The demo's `--direct-draw-test` also checks `ImPlatform_CopyCurrentTarget()` from the pushed target.
- Metal custom shaders from the same HLSL: `compile_shaders` emits MSL (Slang) with buffer indices shifted by one, the
  program's vertex layout comes from its attributes, and the parameters are bound at buffer 2. Metal buffer indices of
  hand-written MSL are now the HLSL registers plus one: projection at 1, parameters at 2 (they were at 1).

### Changed
- Optional API groups are declared only for the backends that implement them: `IMPLATFORM_GFX_SUPPORT_SHADER_TEXTURES`,
  `_VERTEX_BUFFERS`, `_DIRECT_DRAW`, `_RENDER_TARGETS`, `_READBACK`, `_ASYNC_READBACK`, `_COMPUTE`, `_DEPTH_DRAW` and
  `_DEVICE_CONTROL` (see the capability table in the README). Calling a function the selected backend does not
  implement is now a compile error instead of a stub returning `false`; guard such calls with the macro in code built
  for several backends. `ImPlatform_SupportsReadback()`, `SupportsCompute()` and `SupportsDepthDraw()` stay on every
  backend. The backend-internal `IMPLATFORM_GFX_HAS_*` macros are gone.
- `ImPlatform_PlatformEvents()` returns `false` when the frame should be skipped (quit requested or window
  minimized): main loops should `continue` on `false`.
- The `imgui` submodule follows the `LayoutDocking` fork (docking + stack layout); `imgui_stacklayout.cpp` is part of
  the build.
- Build: [Sharpmake](https://github.com/ubisoft/Sharpmake) (`extern/Sharpmake` submodule) generates the projects for
  Windows (Visual Studio 2022), Linux (make) and macOS (Xcode) with `buildsystem/generate_projects.*`. The checked-in
  `ImPlatformDemo.sln` is removed; generated files are not versioned. `CMakeLists.txt` mirrors the Sharpmake
  configurations.
- Demo executables are named `ImPlatformDemo_<Platform>_<Gfx>` and written to `bin/<platform>/`.
- Vulkan custom shaders follow the Direct3D register convention (`b0` projection, `b1` parameters, `t0`/`s0` the
  draw's texture and Dear ImGui's sampler), compiled with Slang's binding shifts (see `ImPlatformDemo/shaders/README.md`);
  SPIR-V written for the previous push-constant layout must be recompiled. The instance requests Vulkan 1.1 (SPIR-V 1.3).
- WebGPU custom shaders take the same registers in bind group 0 (`b0` binding 0, `b1` binding 1, `t0` binding 16 = the
  draw's texture, `s0` binding 32), as Slang emits them; vertex inputs are found by type. WGSL written for the previous
  layout (one uniform buffer at binding 0, sampler 1, texture 2) must be updated.
- OpenGL custom shaders: uniform block binding qualifiers are applied by name after linking (GLSL below 4.20, macOS),
  and the vertex attributes are found by name or type instead of relying on Dear ImGui's locations.

### Fixed
- DirectX 12: each bind of a custom shader takes its own copy of the program's parameters (frame upload memory), so a
  program drawn several times in a frame keeps each draw's values; before, every draw read the last ones set. The demo's
  `--shader-test` draws one program twice with different parameters set from draw callbacks.
- OpenGL: `ImPlatform_ReadTexture()` reports its own GL errors only (an error left pending by an earlier call failed it).
- Vulkan: texture uploads map and flush the whole staging allocation (a size that was not a multiple of
  `nonCoherentAtomSize` broke a validation rule).
- DirectX 9: the regular draws after a callback that called `ImPlatform_DrawIndexed()` used its vertex and index buffers
  (Dear ImGui's DX9 backend binds its own once per draw data). `DrawIndexed` now binds the buffers for its draw only.
- Vulkan: the render-complete semaphore is chosen per swapchain image, not per frame (the validation layer reported a semaphore
  signaled again while a present could still wait on it, VUID-vkQueueSubmit-pSignalSemaphores-00067).
- Draw callbacks after `ImPlatform_BeginCustomShader()` that draw by themselves (`ImPlatform_DrawIndexed()`) ran with the
  previous list's scissor, or none at the start of a render pass, when no regular draw preceded them: the backends set
  the scissor for regular draws only. `BeginCustomShader` now adds a zero-area transparent triangle so they do.
- DirectX 9: RGBA8 textures had red and blue swapped (the bytes were copied into `D3DFMT_A8R8G8B8`); uploads and
  readbacks now swizzle them.
- OpenGL render passes: shaders reading vertex positions saw them mirrored (render textures are stored bottom-up);
  shader parts undo the mirror through the projection block.
- DirectX 10 and 11: a custom shader's parameters were bound only when they had changed, so a program could draw
  with another program's parameters.
- `ImPlatform_BindBuffers()` on DirectX 9, 10, 12, Vulkan and WebGPU (only DirectX 11 bound them).
- `ImPlatform_SetShaderTexture()` on OpenGL finds the sampler SPIRV-Cross generates for the named HLSL texture (on Metal,
  where it bound nothing and reported success, it is no longer declared).
- DirectX 12 custom shaders: the parameters (`b1`) are bound on every activation, not only after they changed; textures
  set before `ImPlatform_BeginCustomShader_Render()` are bound again by it.
- OpenGL: `ImPlatform_DrawQuadNow()` feeds the program's own vertex input locations (shaders declaring the color before
  the uv drew with the wrong attributes).
- Building with `IMGUI_DISABLE_OBSOLETE_FUNCTIONS` (`ImDrawData::CmdListsCount`).
- OpenGL 3 custom shaders: uniform buffer bindings.
- Vulkan: custom shader projections were upside down; `ImPlatform_DestroyTexture()` frees render textures.
- Render passes on the immediate backends (DirectX 9, 10, 11, OpenGL 3) are drawn one draw list at a time, so their
  vertex and index buffers no longer grow to a whole pass and stall every frame.
- DirectX 12: transient descriptor ring drains before wrapping; deferred release of compute pipelines still in flight;
  `R32_UINT` / `R32_SINT` texel size.
- Linking the Windows GLFW configurations against the prebuilt `glfw3.lib` (legacy stdio stubs).
- Vulkan: debug builds no longer abort without the validation layer (Vulkan SDK); `ImPlatform_DestroyTexture()` frees
  the image, view and memory of every texture; custom shaders draw in secondary viewports and render passes, with
  pipelines compatible with Dear ImGui's layout (no validation errors).
