#!/bin/sh
# Generates the ImPlatform projects with Sharpmake (extern/Sharpmake, built on first use).
# Output at the repository root: ImPlatform_make_linux.make, ImPlatform_xcode_mac.xcworkspace
# (plus the Visual Studio solution). Requires the .NET 8 SDK or later.
set -e

cd "$(dirname "$0")/.."

if [ ! -f extern/Sharpmake/Sharpmake.Application/Sharpmake.Application.csproj ]; then
    echo "Error: extern/Sharpmake is missing. Run: git submodule update --init extern/Sharpmake"
    exit 1
fi

dotnet run --project extern/Sharpmake/Sharpmake.Application -c Release -- "/sources('buildsystem/sharpmake/src/main.cs')"
