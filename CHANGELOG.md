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
- DirectX 12: custom shader programs bind pixel shader textures t1..t8 (`ImPlatform_SetShaderTexture()`); static vertex
  and index buffers live in device memory.
- `ImPlatform_IsMaximized()` is public.
- Optional capabilities a backend does not implement are stubbed (`false` / no-op), selected by
  `IMPLATFORM_GFX_HAS_READBACK`, `_ASYNC_READBACK`, `_COMPUTE`, `_DEPTH_DRAW` and `_DEVICE_CONTROL`.
- Demo self-tests: `--compute-test` and `--render-pass-test` (exit code 0 pass, 1 fail, 2 unsupported).

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

### Fixed
- `ImPlatform_BindBuffers()` on DirectX 9, 10, 12, Vulkan and WebGPU (only DirectX 11 bound them).
- OpenGL 3 custom shaders: uniform buffer bindings.
- Vulkan: custom shader projections were upside down; `ImPlatform_DestroyTexture()` frees render textures.
- Render passes on the immediate backends (DirectX 9, 10, 11, OpenGL 3) are drawn one draw list at a time, so their
  vertex and index buffers no longer grow to a whole pass and stall every frame.
- DirectX 12: transient descriptor ring drains before wrapping; deferred release of compute pipelines still in flight;
  `R32_UINT` / `R32_SINT` texel size.
- Linking the Windows GLFW configurations against the prebuilt `glfw3.lib` (legacy stdio stubs).
