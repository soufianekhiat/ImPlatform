@echo off
REM Generates the ImPlatform projects with Sharpmake (extern/Sharpmake, built on first use).
REM Output: ImPlatform_vs2022_win64.sln (plus the Linux makefiles and macOS Xcode workspace) at the repository root.
REM Requires the .NET 8 SDK or later.

pushd "%~dp0.."

if not exist "extern\Sharpmake\Sharpmake.Application\Sharpmake.Application.csproj" (
    echo Error: extern\Sharpmake is missing. Run: git submodule update --init extern/Sharpmake
    popd
    exit /b 1
)

dotnet run --project extern\Sharpmake\Sharpmake.Application -c Release -- /sources('buildsystem/sharpmake/src/main.cs')
set RESULT=%ERRORLEVEL%

popd
exit /b %RESULT%
