# Build ImPlatform Demo for web browser (Emscripten + GLFW + WebGPU)
#
# Prerequisites:
#   1. Install Emscripten SDK: https://emscripten.org/docs/getting_started/downloads.html
#   2. Activate emsdk in this shell:
#        cd <emsdk-dir>
#        .\emsdk activate latest
#        .\emsdk_env.ps1        # PowerShell
#      Or for cmd.exe:
#        emsdk_env.bat
#
# Usage:
#   .\build_web.ps1
#   .\run_web.ps1

$ErrorActionPreference = "Stop"

$ROOT = $PSScriptRoot
$IMGUI = "$ROOT/imgui"
$WEB_DIR = "$ROOT/web"

# Check for em++ compiler
$EMPP = Get-Command "em++" -ErrorAction SilentlyContinue
if (-not $EMPP) {
    Write-Host "Error: em++ not found. Please activate Emscripten SDK first:" -ForegroundColor Red
    Write-Host "  cd <emsdk-dir>" -ForegroundColor Yellow
    Write-Host "  .\emsdk activate latest" -ForegroundColor Yellow
    Write-Host "  .\emsdk_env.ps1" -ForegroundColor Yellow
    exit 1
}

# Create output directory
if (-not (Test-Path $WEB_DIR)) {
    New-Item -ItemType Directory -Path $WEB_DIR | Out-Null
}

# Source files
$SOURCES = @(
    "$ROOT/ImPlatformDemo/main.cpp",
    "$IMGUI/imgui.cpp",
    "$IMGUI/imgui_demo.cpp",
    "$IMGUI/imgui_draw.cpp",
    "$IMGUI/imgui_tables.cpp",
    "$IMGUI/imgui_widgets.cpp",
    "$IMGUI/backends/imgui_impl_glfw.cpp",
    "$IMGUI/backends/imgui_impl_wgpu.cpp"
)

$OUTPUT = "$WEB_DIR/index.html"

# Build arguments
$ARGS = @(
    # Source files
    $SOURCES

    # Output
    "-o", $OUTPUT

    # Include paths
    "-I$ROOT/ImPlatform"
    "-I$IMGUI"
    "-I$IMGUI/backends"

    # Defines
    "-DIM_CONFIG_PLATFORM=IM_PLATFORM_GLFW"
    "-DIM_CONFIG_GFX=IM_GFX_WGPU"
    "-DIMGUI_IMPL_WEBGPU_BACKEND_DAWN"
    "-DIMGUI_DISABLE_FILE_FUNCTIONS"

    # Compiler flags
    "-Wall"
    "-Wformat"
    "-Os"
    "-std=c++17"

    # Emscripten flags (both compile and link)
    "--use-port=emdawnwebgpu"
    "-sDISABLE_EXCEPTION_CATCHING=1"
    "--use-port=contrib.glfw3"

    # Linker flags
    "-sWASM=1"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sNO_EXIT_RUNTIME=0"
    "-sASSERTIONS=1"
    "-sNO_FILESYSTEM=1"
    "-sASYNCIFY"
)

Write-Host "Building ImPlatform Demo for Emscripten (GLFW + WebGPU)..." -ForegroundColor Cyan
Write-Host "em++ $($ARGS -join ' ')" -ForegroundColor DarkGray

& em++ @ARGS

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "Build succeeded!" -ForegroundColor Green
    Write-Host "Output: $WEB_DIR/" -ForegroundColor Green
    Write-Host "Run .\run_web.ps1 to start the local server." -ForegroundColor Green
} else {
    Write-Host "Build failed with exit code: $LASTEXITCODE" -ForegroundColor Red
    exit 1
}
