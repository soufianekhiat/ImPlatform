# Demo shaders

Each custom shader is written once, in plain HLSL (`<name>.hlsl`, entry points `vs_main` and `ps_main`), and compiled
for every graphics backend into headers that `main.cpp` includes:

| Header | Backend | Produced by |
|---|---|---|
| `<name>.hlsl.h` | Direct3D 9, 10, 11, 12 (compiled at run time) | the source itself |
| `<name>.spv.h` | Vulkan | Slang (`slangc`), SPIR-V 1.3 |
| `<name>.glsl.h` | OpenGL 3.3+ (macOS included) | Slang's SPIR-V converted to GLSL 330 by SPIRV-Cross |
| `<name>.wgsl.h` | WebGPU | Slang (`slangc -target wgsl`) |
| `<name>.metal.h` | Metal | Slang (`slangc -target metal`), buffer indices shifted by one |

A compute shader (entry point `cs_main`, such as `compute_test.hlsl`) gets `<name>.hlsl.h`, `<name>.spv.h` and
`<name>.glsl.h` (GLSL 430) with the bindings of `ImPlatform_DispatchCompute`: on Vulkan constants 0, inputs 1..,
outputs 9.. (`-fvk-b-shift 0 0 -fvk-t-shift 1 0 -fvk-u-shift 9 0`); on OpenGL each class from 0.

The headers are generated and committed. Rerun the script after editing a `.hlsl`:

```
compile_shaders.bat          # Windows
./compile_shaders.sh         # Linux, macOS
```

Both scripts produce the same bytes. They fetch the pinned Slang release into `extern/slang`
(`buildsystem/fetch_slang.*`) and need `spirv-cross` (Vulkan SDK, or the `spirv-cross` package on Linux / Homebrew).

## Shader convention

The same registers as ImPlatform's Direct3D 11 backend:

| Register | Content |
|---|---|
| `b0` | `float4x4 ProjMtx`, vertex stage (`mul(ProjMtx, float4(pos, 0, 1))`) |
| `b1` | parameters, from `ImPlatform_BeginUniformBlock` / `SetUniform` / `EndUniformBlock` (D3D packing, back to back) |
| `t0`, `s0` | the texture of the draw command and Dear ImGui's sampler |
| `t1`..`t8` | textures from `ImPlatform_SetShaderTexture(program, name, slot, texture)`, slot 1..8 |

`ImPlatform_SetShaderTexture` takes any ImPlatform texture. The program keeps it for its later draws; called inside a
draw callback while the program is bound, it also applies at once. Unset slots are a white texture on Vulkan, the
font atlas on WebGPU (a slot declared `Texture3D` must be set there and on Vulkan), and undefined on the other
backends. Slot 0 is the texture of the direct draws (`ImPlatform_DrawIndexed`, `ImPlatform_DrawQuadNow`); Dear ImGui's
draws bind their own. On OpenGL, `name` is the HLSL texture name (SPIRV-Cross renames it `SPIRV_Cross_Combined<name><sampler>`, which is also found).

Vertex inputs are `ImDrawVert`: `float2 POSITION`, `float2 TEXCOORD0`, `float4 COLOR0`, in any order (Vulkan, OpenGL
and WebGPU tell them apart by type: the 4-component input is the color, the first 2-component input the position).

OpenGL and WebGPU take the registers as bindings `b0` -> 0, `b1` -> 1, `t0` -> 16, `s0` -> 32
(`-fvk-b-shift 0 0 -fvk-t-shift 16 0 -fvk-s-shift 32 0`); on WebGPU all of them are in bind group 0, with `t1`..`t8`
(`ImPlatform_SetShaderTexture`, slot 1..8) at bindings 17..24; other registers (`s1`+, `b2`+) are not supported.

Direct3D 9 compiles the same HLSL for `vs_3_0` / `ps_3_0` (Slang has no shader model 3 target). Shader model 3 has
no constant buffers: each used cbuffer member gets its own float4 registers, and the backend fills them from `b0` and
`b1` at the member offsets a shader model 4 compile of the source reflects. The draw's texture `t0` is renumbered to
sampler 0 and `t1`..`t8` to samplers 1..8.

Metal takes Slang's indices (`bN` -> `buffer(N)`, `tN` -> `texture(N)`, `sN` -> `sampler(N)`) with the buffers shifted by one,
because Dear ImGui's vertices are at buffer 0: `b0` (the projection) at 1, where Dear ImGui's backend also puts it,
`b1` at 2. The vertex attributes are told apart by type, as on Vulkan.

Parameters belong to the program on every backend: the values set last before the frame is rendered apply to all of
that program's draws in the frame.

On Vulkan, the scripts compile with each register class in its own binding range
(`-fvk-t-shift 0 0 -fvk-s-shift 16 0 -fvk-b-shift 32 0 -fvk-u-shift 48 0`) and `ImPlatform_CreateShader` moves them to
Dear ImGui's descriptor sets (texture set 0, sampler set 1, `b0`/`b1` set 2) and `t1`..`t8` to set 3. Other
registers (`t9`+, `s1`+, `b2`+) are not supported there.
