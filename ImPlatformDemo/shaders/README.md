# Vulkan Shaders - SPIR-V Compilation

This directory contains GLSL shaders for the Vulkan backend and scripts to compile them to SPIR-V bytecode.

## Files

### GLSL Source Files
- `arrow_sdf.vert` - Arrow SDF vertex shader
- `arrow_sdf.frag` - Arrow SDF fragment shader
- `gradient.vert` - Gradient vertex shader
- `gradient.frag` - Gradient fragment shader

### Compiled SPIR-V Bytecode
- `*.spv` - Binary SPIR-V files (generated)
- `*.h` - C++ header files with bytecode arrays (generated)

### Compilation Scripts
- `compile_shaders.bat` - Windows batch script to compile shaders
- `compile_shaders.py` - Python script for cross-platform compilation
- `spv_to_header.sh` - Bash script to convert SPIR-V to C++ headers

## Requirements

- **Vulkan SDK**: Required for `glslangValidator`
  - Download from: https://vulkan.lunarg.com/
  - Make sure `glslangValidator` is in your PATH

For Python script:
- **Python 3.x**

For Bash scripts (Git Bash on Windows):
- **xxd** (included with Git Bash)

## Usage

### Windows (Batch File)
```
compile_shaders.bat
```

### Python (Cross-platform)
```
python compile_shaders.py
```

### Manual Compilation
```bash
# Compile GLSL to SPIR-V
glslangValidator -V arrow_sdf.vert -o arrow_sdf.vert.spv
glslangValidator -V arrow_sdf.frag -o arrow_sdf.frag.spv
glslangValidator -V gradient.vert -o gradient.vert.spv
glslangValidator -V gradient.frag -o gradient.frag.spv

# Convert to C++ headers (using bash script)
bash spv_to_header.sh arrow_sdf.vert.spv arrow_sdf_vert_spv
bash spv_to_header.sh arrow_sdf.frag.spv arrow_sdf_frag_spv
bash spv_to_header.sh gradient.vert.spv gradient_vert_spv
bash spv_to_header.sh gradient.frag.spv gradient_frag_spv
```

## Generated Headers

The generated `.h` files contain:
- `const unsigned int array_name[]` - SPIR-V bytecode as uint32_t array
- `const unsigned int array_name_size` - Size of the array in bytes

These headers are included in `main.cpp` when compiling for Vulkan:
```cpp
#if (IM_CURRENT_GFX == IM_GFX_VULKAN)
    #include "shaders/arrow_sdf.vert.h"
    #include "shaders/arrow_sdf.frag.h"
    #include "shaders/gradient.vert.h"
    #include "shaders/gradient.frag.h"
#endif
```

## Shader Structure

All shaders follow ImGui's vertex format:
```glsl
layout(location = 0) in vec2 aPos;    // Position
layout(location = 1) in vec2 aUV;     // Texture coordinates
layout(location = 2) in vec4 aColor;  // Vertex color
```

Vertex shaders use push constants for the projection matrix:
```glsl
layout(push_constant) uniform PushConstants {
    mat4 ProjectionMatrix;
} pc;
```

Fragment shaders use:
- `layout(binding = 0)` - Texture sampler (from ImGui)
- `layout(binding = 1)` - Custom uniform buffer

## Notes

- The SPIR-V bytecode is stored as little-endian uint32_t arrays
- Each uint32_t value represents 4 bytes of the SPIR-V binary
- Shaders must be recompiled after any changes to the GLSL source
- The generated headers should be committed to version control for convenience
