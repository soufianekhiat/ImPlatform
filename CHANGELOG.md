# Changelog

## 2.0.0

### Added
- **Render passes** on every backend (DirectX 9, 10, 11, 12, OpenGL 3, Vulkan, WebGPU, Metal):
  `ImPlatform_QueueRenderPass()` renders Dear ImGui draw data into a render texture before the frame, with the backend's
  own ImGui renderer (draw callbacks included). `ImPlatform_GetRenderPassFormat()` gives the target format.
- **Premultiplied alpha**: `ImPlatform_PushPremultipliedAlpha()` / `ImPlatform_PopPremultipliedAlpha()` around draw-list
  commands that composite premultiplied images (render pass results); the vertex color is premultiplied too.
- **Render-target stack**: `ImPlatform_PushRenderTarget()` / `ImPlatform_PopRenderTarget()` (nestable, optional clear),
  `ImPlatform_CopyCurrentTarget()`, `ImPlatform_GetCurrentTargetSize()`, `ImPlatform_DrawQuadNow()`.
- **Readback**: `ImPlatform_SupportsReadback()`, `ImPlatform_ReadTexture()` (DirectX 9, 10, 11, 12, OpenGL 3, Vulkan);
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
- Optional capabilities a backend does not implement are stubbed (`false` / no-op), selected by
  `IMPLATFORM_GFX_HAS_READBACK`, `_ASYNC_READBACK`, `_COMPUTE`, `_DEPTH_DRAW` and `_DEVICE_CONTROL`.
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
- DirectX 9 custom shaders: the same HLSL, compiled for `vs_3_0` / `ps_3_0`; cbuffer members are uploaded to the
  constant registers shader model 3 assigns them, and the draw's texture `t0` is sampler 0.

### Changed
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
- `ImPlatform_BindBuffers()` on DirectX 9, 10, 12, Vulkan and WebGPU (only DirectX 11 bound them).
- `ImPlatform_SetShaderTexture()` on Metal returns `false` (it bound nothing and reported success); on OpenGL it finds
  the sampler SPIRV-Cross generates for the named HLSL texture.
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
