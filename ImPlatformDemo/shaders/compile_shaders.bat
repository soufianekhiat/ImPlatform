@echo off
REM Batch script to compile GLSL shaders to SPIR-V
REM Calls the Python script to do the actual work

python compile_shaders.py
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Shader compilation failed
    pause
    exit /b 1
)

pause
exit /b 0
