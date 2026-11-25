# ImPlatform 3.0.0

ImPlatform 3.0 is about custom shaders in a live application: programs that blend the way they need to, compile
without freezing the frame, and report their errors to the application. The full list is in
[CHANGELOG.md](CHANGELOG.md); this page covers the highlights and what to change when upgrading from 2.0.0.

## Highlights

- **Blending per program** on every backend: `ImPlatform_SetShaderProgramBlendPreset()` by name (alpha, premultiplied,
  opaque, additive, subtractive, multiplicative, screen, darken, lighten), or any factors and operations with
  `ImPlatform_SetShaderProgramBlend()`.
- **Compile errors for the application**: `ImPlatform_GetShaderLog()` returns the compiler's messages of a failed
  shader or program; shader parts add the generated source with line numbers.
- **Permutations**: `ImPlatform_ShaderDesc::defines` ("CLIP;MODE=2") selects `#ifdef` / `#if` paths of one HLSL, GLSL or
  MSL source, with the compiler's line numbers kept.
- **Asynchronous compilation**: `ImPlatform_CreateShaderProgramAsync()` and `ImPlatform_CreateComputeShaderAsync()`
  return a job to poll (`ImPlatform_GetShaderJobStatus()`), compiled on a worker thread on DirectX 10, 11 and 12.
- **Asynchronous compute**: `ImPlatform_DispatchComputeAsync()` never waits for the GPU and returns a handle to poll.
- **Screenshots**: `ImPlatform_CaptureViewport()` gives the image of a viewport as a texture,
  `ImPlatform_ReadCapture()` its RGBA pixels (DirectX 9, 11, 12, OpenGL, Vulkan; a secondary viewport everywhere but
  DirectX 12 and Vulkan).
- **Buffers for geometry that changes every frame**: Stream buffers take their memory from the per-frame upload arena
  on DirectX 12 (no resource created per draw), and `ImPlatform_UpdateVertexBuffer()` / `ImPlatform_UpdateIndexBuffer()`
  may be called every frame on Dynamic buffers: draws already recorded keep their content.
- **Pen input** on Windows with every window library (Win32, GLFW, SDL2), and on SDL3 on Windows, Linux and macOS.
- **Touch input** on Win32, SDL2 and SDL3: the fingers in contact (`ImPlatform_GetTouchPoints()`) and their gesture,
  pan, pinch zoom and rotation (`ImPlatform_ConsumeTouchGesture()`), which also carries a trackpad's pinch.
- **Drag and drop from other applications**: `ImPlatform_SetDropCallback()` reports a drag as it hovers (Win32, SDL3)
  and what it drops: files, and text or URLs.
  On Windows this holds for GLFW and SDL2 windows too, and `ImPlatform_DragOut()` drags files and text out of the
  application. On Linux (X11) GLFW and SDL2 windows report the hover and take text as well.
- **Window state**: borderless fullscreen, always on top, window icon. **System theme**: the user's light or dark
  mode. **Clipboard images** as RGBA pixels. The last two are taken from Windows, macOS and Linux themselves,
  whatever the window library.
- **Borderless window on stock GLFW**: moved and resized by its edges without TheCherno's fork.
- **Win32 + Vulkan** is a built and tested configuration.

## Upgrading from 2.0.0

- **Separately compiled sources**: add `ImPlatform/ImPlatform_shader_common.cpp` and
  `ImPlatform/ImPlatform_native.cpp` (applications that only define `IMPLATFORM_IMPLEMENTATION` get them through
  `ImPlatform.h`). The first uses `std::thread` on DirectX 10, 11 and 12.
- **macOS**: link the ApplicationServices framework.
- **Dear ImGui**: 1.92.8 or later (the submodule is the docking branch after 1.93.0).
- **DirectX 9** is a Direct3D 9Ex device: resources an application creates itself on the device go in
  `D3DPOOL_DEFAULT` (there is no managed pool), and they stay valid across a reset.
- **DirectX 12**: the sampler `s0` of a custom shader program clamps; a shader that relied on it wrapping needs its
  own texture coordinates wrapped (`frac`).
- **`IM_GFX_CUSTOM` backends** provide two more functions, `ImPlatform_GetShaderLog()` and
  `ImPlatform_SetShaderProgramBlend()`, and the group `IMPLATFORM_GFX_SUPPORT_ASYNC_COMPUTE` (three functions) unless
  they define it to 0. Their `ImPlatform_PushSampler()` / `ImPlatform_PopSampler()` become
  `ImPlatform_PushSamplerEx()` / `ImPlatform_PopSamplerEx()`, with the draw list as first argument (NULL: the
  current window's).
- **`ImPlatform_ShaderDesc`** has a new last member, `defines`: descriptors that are zero-initialized need no change.
- **`ImPlatform_UpdateVertexBuffer()` / `ImPlatform_UpdateIndexBuffer()`** take (offset, count) on every backend, as
  the header declares. DirectX 9, 10, 11, 12, Vulkan and WebGPU read them as (count, offset): swap the two arguments in
  calls that were written to suit those backends.
- **Stream buffers** hold their content for one frame: update the whole buffer, or create it again, each frame it is
  drawn in.
- **Borderless GLFW windows** can now be resized by their edges on stock GLFW; set
  `ImPlatform_BorderlessParams::allowResize` to false to keep the previous behavior.
- Nothing else changes for code written against 2.0.0.

## Verification

- The demo's self-tests, in addition to those of 2.0.0: `--blend-test`, `--shader-async-test`, `--compute-async-test`,
  `--screenshot-test`, `--screenshot-viewport-test`, `--buffer-update-test`, `--mesh-stream-test`, `--pen-test`
  (synthetic SDL3 pen events), `--touch-test` (synthetic SDL finger events), `--sampler-test`, `--device-lost-test` (DirectX 12), `--drop-test`, `--window-test`, `--clipboard-test` and `--drag-rect-test`. They pass on Windows (Win32 with
  DirectX 9, 10, 11, 12, OpenGL and Vulkan; GLFW with OpenGL and Vulkan) and on Linux with Mesa (GLFW, SDL2 and SDL3
  with OpenGL and Vulkan), where the backend has the feature.
- CI builds every Windows, Linux and macOS configuration with both Sharpmake and CMake and runs the self-tests on
  Linux and macOS.

## Known limitations

- Metal runs its self-tests in CI on the hosted macOS runner (a virtual machine), with GLFW and with the Cocoa
  platform. It has not been tried by hand on a Mac; macOS OpenGL does not run in CI, and the SDL configurations are
  only built there.
- Shaders compile on a worker thread on DirectX 10, 11 and 12 only: elsewhere a job is ready or failed as soon as it
  is created, and Vulkan, DirectX 12 and OpenGL still create pipelines at the first draw or dispatch.
- An asynchronous dispatch runs on the queue of the frames: a long one still delays the frames submitted after it.
- Viewport capture is not available on DirectX 10, Metal and WebGPU, nor for secondary viewports on Vulkan.
- The borderless window on stock GLFW has no system window snapping.
- The limitations of 2.0.0 below still apply.

# ImPlatform 2.0.0

ImPlatform 2.0 turns the window and device wrapper into a small rendering layer over Dear ImGui's backends: offscreen
passes, custom shaders written once, readback and compute, on every graphics API it supports. The full list is in
[CHANGELOG.md](CHANGELOG.md); this page covers the highlights and what to change when upgrading from 0.5.0.

## Highlights

- **Render passes** on every backend: `ImPlatform_QueueRenderPass()` renders Dear ImGui draw data into a texture
  before the frame, composited with premultiplied alpha (`ImPlatform_PushPremultipliedAlpha()`).
- **Custom shaders written once.** Plain HLSL compiled offline to every backend (Slang: SPIR-V, WGSL, GLSL, MSL), or
  **shader parts** (`ImPlatform_CreateShaderFromParts()`) generated and compiled at run time as HLSL, GLSL or MSL.
  Shader parts support their own vertex inputs, flat varyings, 3D textures and an `IMP_SM3` path for DirectX 9.
- **Direct draws from draw callbacks**: render-target stack, `ImPlatform_CopyCurrentTarget()`,
  `ImPlatform_DrawQuadNow()`, pushed constants, and vertex / index buffers drawn with `ImPlatform_DrawIndexed()`.
- **3D textures** on every backend (`ImPlatform_CreateTexture3D()`), textures t1..t8 bound per program.
- **Readback** (synchronous everywhere but WebGPU, asynchronous on DirectX 11 and 12) and **compute** (DirectX 11,
  12, Vulkan, OpenGL 4.3+).
- **Depth-tested meshes and device control** (lost device, recreation, adapter name, testing aids) on DirectX 11 and 12.
- **Pen input** (Windows Ink): pressure, tilt, rotation, eraser, coalesced samples.
- **`IM_GFX_CUSTOM`**: keep ImPlatform's platform layer and bring your own graphics backend.
- **Sharpmake build** (Visual Studio 2022, make, Xcode) with a matching CMake build.

## Upgrading from 0.5.0

- **Main loop**: `ImPlatform_PlatformEvents()` returns `false` when the frame should be skipped (quit requested or
  window minimized); `continue` the loop on `false`.
- **Optional functions are compile-time.** Functions a backend does not implement are no longer declared. Guard calls
  in code built for several backends with `IMPLATFORM_GFX_SUPPORT_*` (see the capability table in the README).
- **Shader bindings follow the Direct3D registers** (`b0` projection, `b1` parameters, `t0`/`s0` the draw's texture):
  - Vulkan SPIR-V written for the old push-constant layout must be recompiled. The instance now requests Vulkan 1.1.
  - WGSL written for the old layout (uniform 0, sampler 1, texture 2) must be updated.
  - Hand-written MSL buffer indices are the HLSL registers plus one: projection at 1, parameters at 2 (they were at 1).
- **Build**: the checked-in `ImPlatformDemo.sln` / `.vcxproj` are gone. Run `buildsystem/generate_projects.*` (needs
  the .NET 8 SDK) or use CMake. Clone with `--recursive`: `imgui` now follows the docking + stack layout fork and
  `extern/Sharpmake` is a submodule.
- Demo executables are named `ImPlatformDemo_<Platform>_<Gfx>` and written to `bin/<platform>/`.

## Verification

- CI builds every Windows, Linux and macOS configuration with both Sharpmake and CMake.
- The demo's self-tests check results by readback: `--compute-test`, `--render-pass-test`, `--shader-test`,
  `--direct-draw-test`, `--shader-parts-test`, `--texture3d-test` and `--vertex-inputs-test`. They pass on DirectX 9,
  10, 11, 12, OpenGL and Vulkan, on Windows and on Linux (Mesa).
- The MSL generated by shader parts is compiled in CI by the offline Metal compiler.

## Known limitations

- Metal and macOS build and compile in CI but have not run on a Mac with a GPU.
- WebGPU has no render-target switch or copy (its render pass encoder belongs to Dear ImGui's backend), and no readback.
- Shader model 3 (DirectX 9): no integer or bit operations, no texel loads, `nointerpolation` varyings are interpolated.
