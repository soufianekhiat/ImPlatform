# Serve the ImPlatform web demo via local HTTP server
#
# WebGPU requires Chrome 113+, Edge 113+, or Firefox Nightly with WebGPU enabled.
# Open http://localhost:8080 in your browser after starting.
#
# Usage:
#   .\run_web.ps1            # default port 8080
#   .\run_web.ps1 -Port 3000 # custom port

param(
    [int]$Port = 8080
)

$WEB_DIR = "$PSScriptRoot/web"

if (-not (Test-Path "$WEB_DIR/index.html")) {
    Write-Host "Error: web/index.html not found. Run .\build_web.ps1 first." -ForegroundColor Red
    exit 1
}

Write-Host "Serving ImPlatform Demo at http://localhost:$Port" -ForegroundColor Cyan
Write-Host "Open in Chrome or Edge (WebGPU required)" -ForegroundColor Cyan
Write-Host "Press Ctrl+C to stop" -ForegroundColor DarkGray
Write-Host ""

python -m http.server $Port -d $WEB_DIR
