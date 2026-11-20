@echo off
REM Compiles the demo's HLSL shaders for every graphics backend (see compile_shaders.ps1).
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0compile_shaders.ps1"
exit /b %ERRORLEVEL%
