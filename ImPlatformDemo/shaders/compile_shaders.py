#!/usr/bin/env python3
"""
Compile GLSL shaders to SPIR-V and generate C++ header files with bytecode arrays.
Requires glslangValidator to be in PATH or in Vulkan SDK.
"""

import subprocess
import sys
import os
from pathlib import Path

# Shader files to compile
SHADERS = [
    ("arrow_sdf.vert", "arrow_sdf_vert_spv"),
    ("arrow_sdf.frag", "arrow_sdf_frag_spv"),
    ("gradient.vert", "gradient_vert_spv"),
    ("gradient.frag", "gradient_frag_spv"),
]

def check_glslang():
    """Check if glslangValidator is available."""
    try:
        subprocess.run(["glslangValidator", "--version"],
                      capture_output=True, check=True)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("ERROR: glslangValidator not found in PATH")
        print("Please install Vulkan SDK or add glslangValidator to your PATH")
        print("Vulkan SDK: https://vulkan.lunarg.com/")
        return False

def compile_shader(shader_file):
    """Compile a GLSL shader to SPIR-V."""
    output_file = shader_file + ".spv"
    print(f"Compiling {shader_file}...")

    try:
        result = subprocess.run(
            ["glslangValidator", "-V", shader_file, "-o", output_file],
            capture_output=True,
            text=True,
            check=True
        )
        print(f"  -> {output_file}")
        return output_file
    except subprocess.CalledProcessError as e:
        print(f"ERROR: Failed to compile {shader_file}")
        print(e.stderr)
        return None

def generate_header(spv_file, array_name):
    """Generate C++ header file from SPIR-V binary."""
    header_file = spv_file.replace(".spv", ".h")

    print(f"Generating {header_file}...")

    # Read SPIR-V binary
    with open(spv_file, "rb") as f:
        data = f.read()

    # Convert to uint32_t array
    # SPIR-V is always 4-byte aligned
    if len(data) % 4 != 0:
        print(f"WARNING: {spv_file} size is not 4-byte aligned!")
        # Pad with zeros
        data += b'\x00' * (4 - len(data) % 4)

    # Convert bytes to uint32_t values (little-endian)
    uint32_values = []
    for i in range(0, len(data), 4):
        value = int.from_bytes(data[i:i+4], byteorder='little')
        uint32_values.append(value)

    # Generate header file
    with open(header_file, "w") as f:
        f.write(f"// Auto-generated from {spv_file}\n")
        f.write(f"// Size: {len(data)} bytes ({len(uint32_values)} uint32_t values)\n")
        f.write("\n")
        f.write("#pragma once\n")
        f.write("\n")
        f.write(f"static const unsigned int {array_name}[] = {{\n")

        # Write array values, 8 per line
        for i in range(0, len(uint32_values), 8):
            line_values = uint32_values[i:i+8]
            line = "    " + ", ".join(f"0x{v:08x}" for v in line_values)
            if i + 8 < len(uint32_values):
                line += ","
            f.write(line + "\n")

        f.write("};\n")
        f.write("\n")
        f.write(f"static const unsigned int {array_name}_size = sizeof({array_name});\n")

    print(f"  -> {header_file}")
    return header_file

def main():
    """Main function."""
    print("Compiling Vulkan shaders to SPIR-V...")
    print()

    # Check for glslangValidator
    if not check_glslang():
        return 1

    print()

    # Compile all shaders
    compiled_files = []
    for shader_file, array_name in SHADERS:
        if not Path(shader_file).exists():
            print(f"ERROR: Shader file not found: {shader_file}")
            return 1

        spv_file = compile_shader(shader_file)
        if spv_file is None:
            return 1

        compiled_files.append((spv_file, array_name))

    print()
    print("Generating C++ header files...")
    print()

    # Generate headers
    header_files = []
    for spv_file, array_name in compiled_files:
        header_file = generate_header(spv_file, array_name)
        header_files.append(header_file)

    print()
    print("=" * 60)
    print("Shader compilation completed successfully!")
    print("Generated files:")
    for spv_file, _ in compiled_files:
        print(f"  - {spv_file}")
    for header_file in header_files:
        print(f"  - {header_file}")
    print("=" * 60)

    return 0

if __name__ == "__main__":
    sys.exit(main())
