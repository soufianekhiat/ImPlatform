using System.Collections.Generic;
using System.IO;
using Sharpmake;

namespace ImPlatform
{
    // The demo: main.cpp defines IMPLATFORM_IMPLEMENTATION, so ImPlatform.h pulls the selected
    // ImPlatform backends into it. Dear ImGui and its backends for the selected App/Gfx pair
    // are compiled next to it; every other backend file is excluded from the configuration.
    [Generate]
    public class ImPlatformDemoProject : Project
    {
        private static readonly Dictionary<App, string> s_appBackend = new Dictionary<App, string>
        {
            { App.Win32, "imgui_impl_win32.cpp" },
            { App.GLFW,  "imgui_impl_glfw.cpp" },
            { App.SDL2,  "imgui_impl_sdl2.cpp" },
            { App.SDL3,  "imgui_impl_sdl3.cpp" },
        };

        private static readonly Dictionary<Gfx, string> s_gfxBackend = new Dictionary<Gfx, string>
        {
            { Gfx.DX9,     "imgui_impl_dx9.cpp" },
            { Gfx.DX10,    "imgui_impl_dx10.cpp" },
            { Gfx.DX11,    "imgui_impl_dx11.cpp" },
            { Gfx.DX12,    "imgui_impl_dx12.cpp" },
            { Gfx.OpenGL3, "imgui_impl_opengl3.cpp" },
            { Gfx.Vulkan,  "imgui_impl_vulkan.cpp" },
            { Gfx.Metal,   "imgui_impl_metal.mm" },
        };

        private static readonly Dictionary<App, string> s_appDefine = new Dictionary<App, string>
        {
            { App.Win32, "IM_PLATFORM_WIN32" },
            { App.GLFW,  "IM_PLATFORM_GLFW" },
            { App.SDL2,  "IM_PLATFORM_SDL2" },
            { App.SDL3,  "IM_PLATFORM_SDL3" },
        };

        private static readonly Dictionary<Gfx, string> s_gfxDefine = new Dictionary<Gfx, string>
        {
            { Gfx.DX9,     "IM_GFX_DIRECTX9" },
            { Gfx.DX10,    "IM_GFX_DIRECTX10" },
            { Gfx.DX11,    "IM_GFX_DIRECTX11" },
            { Gfx.DX12,    "IM_GFX_DIRECTX12" },
            { Gfx.OpenGL3, "IM_GFX_OPENGL3" },
            { Gfx.Vulkan,  "IM_GFX_VULKAN" },
            { Gfx.Metal,   "IM_GFX_METAL" },
        };

        public ImPlatformDemoProject() : base(typeof(ImPlatformTarget))
        {
            Name = "ImPlatformDemo";
            IsFileNameToLower = false;
            IsTargetFileNameToLower = false;
            SourceFilesExtensions.Add(".mm");

            SourceRootPath = Paths.Demo;
            foreach (string file in new[] { "imgui.cpp", "imgui_demo.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp", "imgui_stacklayout.cpp" })
                SourceFiles.Add(Path.Combine(Paths.ImGui, file));
            foreach (string file in s_appBackend.Values)
                SourceFiles.Add(Path.Combine(Paths.ImGui, "backends", file));
            foreach (string file in s_gfxBackend.Values)
                SourceFiles.Add(Path.Combine(Paths.ImGui, "backends", file));
            SourceFiles.Add(Path.Combine(Paths.ImPlatform, "ImPlatform.h"));
            SourceFiles.Add(Path.Combine(Paths.ImPlatform, "ImPlatform_Internal.h"));

            AddTargets(ImPlatformTarget.All());
        }

        [Configure]
        public void ConfigureAll(Configuration conf, ImPlatformTarget target)
        {
            conf.Name = target.ConfigName;
            conf.ProjectFileName = "[project.Name]_[target.DevEnv]_[target.Platform]";
            conf.ProjectPath = Path.Combine(Paths.Root, "projects", "ImPlatformDemo");
            conf.IntermediatePath = Path.Combine(Paths.Root, "tmp", "[target.Platform]", target.ConfigName);
            conf.TargetPath = Path.Combine(Paths.Root, "bin", "[target.Platform]");
            conf.TargetFileName = "ImPlatformDemo_" + target.ConfigName;
            conf.Output = Configuration.OutputType.Exe;

            conf.IncludePaths.Add(Paths.ImPlatform);
            conf.IncludePaths.Add(Paths.ImGui);
            conf.IncludePaths.Add(Path.Combine(Paths.ImGui, "backends"));

            conf.Defines.Add("IM_CONFIG_PLATFORM=" + s_appDefine[target.App]);
            conf.Defines.Add("IM_CONFIG_GFX=" + s_gfxDefine[target.Gfx]);
            // imconfig overrides of DearWidgets, which builds on ImPlatform: kept in the build so imconfig.h stays upstream
            conf.Defines.Add("ImDrawIdx=unsigned int");
            conf.Defines.Add("IMGUI_USE_WCHAR32");
            if (target.App == App.SDL2 || target.App == App.SDL3)
                conf.Defines.Add("SDL_MAIN_HANDLED"); // the demo provides its own main()

            foreach (var backend in s_appBackend)
                if (backend.Key != target.App)
                    conf.SourceFilesBuildExclude.Add(Path.Combine(Paths.ImGui, "backends", backend.Value));
            foreach (var backend in s_gfxBackend)
                if (backend.Key != target.Gfx)
                    conf.SourceFilesBuildExclude.Add(Path.Combine(Paths.ImGui, "backends", backend.Value));
            // Metal compiles the demo as Objective-C++ through main_metal.mm (see there), the other configurations main.cpp
            conf.SourceFilesBuildExclude.Add(Path.Combine(Paths.Demo, target.Gfx == Gfx.Metal ? "main.cpp" : "main_metal.mm"));
        }

        [Configure(Platform.win64)]
        public void ConfigureWindows(Configuration conf, ImPlatformTarget target)
        {
            conf.Options.Add(Options.Vc.General.WindowsTargetPlatformVersion.Latest);
            conf.Options.Add(Options.Vc.General.CharacterSet.Unicode);
            conf.Options.Add(Options.Vc.General.WarningLevel.Level4);
            conf.Options.Add(Options.Vc.Compiler.ConformanceMode.Enable);
            conf.Options.Add(Options.Vc.Compiler.CppLanguageStandard.CPP17);
            // MSVC defaults, which Sharpmake does not keep: DLL runtime (the prebuilt GLFW and SDL import
            // libraries use it), /EHsc, /GR, /fp:precise.
            conf.Options.Add(Options.Vc.Compiler.RuntimeLibrary.MultiThreadedDebugDLL);
            conf.Options.Add(Options.Vc.Compiler.Exceptions.Enable);
            conf.Options.Add(Options.Vc.Compiler.RTTI.Enable);
            conf.Options.Add(Options.Vc.Compiler.FloatingPointModel.Precise);
            conf.Options.Add(Options.Vc.Linker.SubSystem.Console);
            conf.AdditionalCompilerOptions.Add("/sdl");
            conf.Defines.Add("_CRT_SECURE_NO_WARNINGS");
            conf.Defines.Add("_DEBUG");
            conf.Defines.Add("_CONSOLE");
            conf.CustomProperties.Add("VcpkgEnabled", "false");

            conf.VcxprojUserFile = new Configuration.VcxprojUserFileSettings();
            conf.VcxprojUserFile.LocalDebuggerWorkingDirectory = "$(TargetDir)";

            // SDL and the Vulkan SDK come from the environment (or /p: properties on the MSBuild command line);
            // GLFW from the prebuilt library shipped in Dear ImGui's examples.
            switch (target.App)
            {
                case App.GLFW:
                    conf.IncludePaths.Add(Path.Combine(Paths.ImGui, "examples", "libs", "glfw", "include"));
                    conf.LibraryPaths.Add(Path.Combine(Paths.ImGui, "examples", "libs", "glfw", "lib-vc2010-64"));
                    // glfw3.lib is a VS2010 release build: its stdio imports (vsnprintf, sscanf) need the legacy
                    // stubs, and its release CRT reference gives way to the debug one.
                    conf.LibraryFiles.Add("glfw3.lib", "legacy_stdio_definitions.lib");
                    conf.AdditionalLinkerOptions.Add("/NODEFAULTLIB:MSVCRT");
                    break;
                case App.SDL2:
                    conf.IncludePaths.Add("$(SDL2_INCLUDE)");
                    conf.LibraryPaths.Add("$(SDL2_LIB)");
                    conf.LibraryFiles.Add("SDL2.lib");
                    break;
                case App.SDL3:
                    conf.IncludePaths.Add("$(SDL3_INCLUDE)");
                    conf.LibraryPaths.Add("$(SDL3_LIB)");
                    conf.LibraryFiles.Add("SDL3.lib");
                    break;
            }

            switch (target.Gfx)
            {
                case Gfx.DX9:
                    conf.LibraryFiles.Add("d3d9.lib");
                    break;
                case Gfx.DX10:
                    conf.LibraryFiles.Add("d3d10.lib", "dxgi.lib", "d3dcompiler.lib");
                    break;
                case Gfx.DX11:
                    conf.LibraryFiles.Add("d3d11.lib", "dxgi.lib", "d3dcompiler.lib");
                    break;
                case Gfx.DX12:
                    conf.LibraryFiles.Add("d3d12.lib", "dxgi.lib", "d3dcompiler.lib");
                    break;
                case Gfx.OpenGL3:
                    conf.LibraryFiles.Add("opengl32.lib");
                    break;
                case Gfx.Vulkan:
                    conf.IncludePaths.Add(@"$(VULKAN_SDK)\Include");
                    conf.LibraryPaths.Add(@"$(VULKAN_SDK)\Lib");
                    conf.LibraryFiles.Add("vulkan-1.lib");
                    break;
            }
        }

        // System packages (apt, or SDL3 built from source into /usr/local). Passed as raw flags so the
        // absolute paths survive generation on any host.
        [Configure(Platform.linux)]
        public void ConfigureLinux(Configuration conf, ImPlatformTarget target)
        {
            conf.Options.Add(Options.Makefile.Compiler.CppLanguageStandard.Cpp17);
            conf.Options.Add(Options.Makefile.Compiler.Exceptions.Enable);
            conf.Options.Add(Options.Makefile.Compiler.Rtti.Enable);
            conf.Defines.Add("_DEBUG");

            switch (target.App)
            {
                case App.GLFW:
                    conf.AdditionalLinkerOptions.Add("-lglfw");
                    break;
                case App.SDL2:
                    conf.AdditionalCompilerOptions.Add("-I/usr/include/SDL2");
                    conf.AdditionalLinkerOptions.Add("-lSDL2");
                    break;
                case App.SDL3:
                    conf.AdditionalLinkerOptions.Add("-L/usr/local/lib", "-Wl,-rpath,/usr/local/lib", "-lSDL3");
                    break;
            }

            conf.AdditionalLinkerOptions.Add(target.Gfx == Gfx.Vulkan ? "-lvulkan" : "-lGL");
            conf.AdditionalLinkerOptions.Add("-ldl", "-lpthread");
        }

        // Homebrew packages (/opt/homebrew on Apple silicon, /usr/local on Intel and for SDL3 built from source).
        [Configure(Platform.mac)]
        public void ConfigureMac(Configuration conf, ImPlatformTarget target)
        {
            conf.Options.Add(Options.XCode.Compiler.CppLanguageStandard.CPP17);
            conf.Options.Add(Options.XCode.Compiler.CppExceptions.Enable);
            conf.Options.Add(Options.XCode.Compiler.RTTI.Enable);
            conf.Options.Add(Options.XCode.Compiler.OnlyActiveArch.Enable);
            conf.Defines.Add("_DEBUG");
            conf.AdditionalCompilerOptions.Add("-I/opt/homebrew/include", "-I/usr/local/include");
            conf.AdditionalLinkerOptions.Add("-L/opt/homebrew/lib", "-L/usr/local/lib", "-Wl,-rpath,/opt/homebrew/lib", "-Wl,-rpath,/usr/local/lib");
            conf.XcodeSystemFrameworks.Add("Cocoa", "IOKit", "CoreVideo");

            switch (target.App)
            {
                case App.GLFW:
                    conf.AdditionalLinkerOptions.Add("-lglfw");
                    break;
                case App.SDL2:
                    conf.AdditionalCompilerOptions.Add("-I/opt/homebrew/include/SDL2", "-I/usr/local/include/SDL2");
                    conf.AdditionalLinkerOptions.Add("-lSDL2");
                    break;
                case App.SDL3:
                    conf.AdditionalLinkerOptions.Add("-lSDL3");
                    break;
            }

            switch (target.Gfx)
            {
                case Gfx.OpenGL3:
                    conf.XcodeSystemFrameworks.Add("OpenGL");
                    break;
                case Gfx.Metal:
                    conf.XcodeSystemFrameworks.Add("Metal", "MetalKit", "QuartzCore", "Foundation");
                    break;
            }
        }
    }
}
