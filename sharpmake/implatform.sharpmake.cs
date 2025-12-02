using System.IO;
using Sharpmake;

// =============================================================================
// ImPlatformDemo Project
// =============================================================================
[Generate]
public class ImPlatformDemoProject : Project
{
    public ImPlatformDemoProject()
    {
        Name = "ImPlatformDemo";
        SourceRootPath = Path.Combine(Globals.RootDirectory, "ImPlatformDemo");

        AddTargets(ImPlatformSolution.GetTargets());
    }

    [Configure]
    public void ConfigureAll(Configuration conf, ImPlatformTarget target)
    {
        conf.ProjectFileName = $"ImPlatformDemo_{target.PlatformConfig}_{target.GfxConfig}";
        conf.ProjectPath = Path.Combine(Globals.RootDirectory, "generated", target.Platform.ToString());

        string configName = $"{target.PlatformConfig}_{target.GfxConfig}";
        conf.Name = $"{configName}_{target.Optimization}";

        conf.Output = Configuration.OutputType.Exe;

        // Include paths
        conf.IncludePaths.Add(Path.Combine(Globals.RootDirectory, "ImPlatform"));
        conf.IncludePaths.Add(Path.Combine(Globals.RootDirectory, "imgui"));
        conf.IncludePaths.Add(Path.Combine(Globals.RootDirectory, "imgui", "backends"));

        // Dear ImGui core sources
        conf.SourceFilesBuildExcludeRegex.Add(@".*");  // Exclude all by default in SourceRootPath

        // Explicitly add sources
        var imguiPath = Path.Combine(Globals.RootDirectory, "imgui");
        var implatformPath = Path.Combine(Globals.RootDirectory, "ImPlatform");
        var demoPath = Path.Combine(Globals.RootDirectory, "ImPlatformDemo");

        // ImGui core
        conf.SourceFiles.Add(Path.Combine(imguiPath, "imgui.cpp"));
        conf.SourceFiles.Add(Path.Combine(imguiPath, "imgui_demo.cpp"));
        conf.SourceFiles.Add(Path.Combine(imguiPath, "imgui_draw.cpp"));
        conf.SourceFiles.Add(Path.Combine(imguiPath, "imgui_tables.cpp"));
        conf.SourceFiles.Add(Path.Combine(imguiPath, "imgui_widgets.cpp"));

        // Demo main
        conf.SourceFiles.Add(Path.Combine(demoPath, "main.cpp"));

        // ImPlatform titlebar
        conf.SourceFiles.Add(Path.Combine(implatformPath, "ImPlatform_titlebar.cpp"));

        // Platform-specific defines and sources
        ConfigurePlatform(conf, target, imguiPath, implatformPath);

        // Graphics-specific defines and sources
        ConfigureGraphics(conf, target, imguiPath, implatformPath);

        // Compiler options
        conf.Options.Add(Options.Makefile.Compiler.CppLanguageStandard.Cpp17);
        conf.Options.Add(Options.Makefile.Compiler.Warnings.NormalWarnings);

        // Debug/Release
        if (target.Optimization == Optimization.Debug)
        {
            conf.Defines.Add("_DEBUG");
            conf.Options.Add(Options.Makefile.Compiler.OptimizationLevel.Disable);
        }
        else
        {
            conf.Defines.Add("NDEBUG");
            conf.Options.Add(Options.Makefile.Compiler.OptimizationLevel.FullOptimization);
        }

        // Platform-specific libraries
        ConfigurePlatformLibraries(conf, target);
    }

    private void ConfigurePlatform(Configuration conf, ImPlatformTarget target, string imguiPath, string implatformPath)
    {
        // On macOS with Metal, use the Apple platform backend
        bool useApplePlatform = (target.Platform == Platform.mac && target.GfxConfig == ImGfxConfig.Metal);

        if (useApplePlatform)
        {
            // Use dedicated Apple/macOS platform backend for Metal
            conf.Defines.Add("IM_CONFIG_PLATFORM=IM_PLATFORM_APPLE");
            conf.SourceFiles.Add(Path.Combine(implatformPath, "ImPlatform_app_apple.mm"));
            // imgui macOS backend is handled via GLFW/SDL, Metal handles its own platform integration
        }

        switch (target.PlatformConfig)
        {
            case ImPlatformConfig.GLFW:
                if (!useApplePlatform)
                {
                    conf.Defines.Add("IM_CONFIG_PLATFORM=IM_PLATFORM_GLFW");
                    conf.SourceFiles.Add(Path.Combine(implatformPath, "ImPlatform_app_glfw.cpp"));
                }
                conf.SourceFiles.Add(Path.Combine(imguiPath, "backends", "imgui_impl_glfw.cpp"));

                if (target.Platform == Platform.linux)
                {
                    conf.LibraryPaths.Add("/usr/lib/x86_64-linux-gnu");
                    conf.LibraryFiles.Add("glfw");
                }
                else if (target.Platform == Platform.mac)
                {
                    conf.LibraryPaths.Add("/usr/local/lib");
                    conf.LibraryPaths.Add("/opt/homebrew/lib");
                    conf.IncludePaths.Add("/usr/local/include");
                    conf.IncludePaths.Add("/opt/homebrew/include");
                    conf.LibraryFiles.Add("glfw");
                }
                break;

            case ImPlatformConfig.SDL2:
                if (!useApplePlatform)
                {
                    conf.Defines.Add("IM_CONFIG_PLATFORM=IM_PLATFORM_SDL2");
                    conf.SourceFiles.Add(Path.Combine(implatformPath, "ImPlatform_app_sdl2.cpp"));
                }
                conf.SourceFiles.Add(Path.Combine(imguiPath, "backends", "imgui_impl_sdl2.cpp"));

                if (target.Platform == Platform.linux)
                {
                    conf.AdditionalCompilerOptions.Add("`sdl2-config --cflags`");
                    conf.AdditionalLinkerOptions.Add("`sdl2-config --libs`");
                }
                else if (target.Platform == Platform.mac)
                {
                    conf.AdditionalCompilerOptions.Add("`sdl2-config --cflags`");
                    conf.AdditionalLinkerOptions.Add("`sdl2-config --libs`");
                }
                break;

            case ImPlatformConfig.SDL3:
                if (!useApplePlatform)
                {
                    conf.Defines.Add("IM_CONFIG_PLATFORM=IM_PLATFORM_SDL3");
                    conf.SourceFiles.Add(Path.Combine(implatformPath, "ImPlatform_app_sdl3.cpp"));
                }
                conf.SourceFiles.Add(Path.Combine(imguiPath, "backends", "imgui_impl_sdl3.cpp"));

                if (target.Platform == Platform.linux)
                {
                    conf.AdditionalCompilerOptions.Add("`pkg-config --cflags sdl3`");
                    conf.AdditionalLinkerOptions.Add("`pkg-config --libs sdl3`");
                }
                else if (target.Platform == Platform.mac)
                {
                    conf.AdditionalCompilerOptions.Add("`pkg-config --cflags sdl3`");
                    conf.AdditionalLinkerOptions.Add("`pkg-config --libs sdl3`");
                }
                break;
        }
    }

    private void ConfigureGraphics(Configuration conf, ImPlatformTarget target, string imguiPath, string implatformPath)
    {
        switch (target.GfxConfig)
        {
            case ImGfxConfig.OpenGL3:
                conf.Defines.Add("IM_CONFIG_GFX=IM_GFX_OPENGL3");
                conf.Defines.Add("IMGUI_IMPL_OPENGL_LOADER_CUSTOM");
                conf.SourceFiles.Add(Path.Combine(imguiPath, "backends", "imgui_impl_opengl3.cpp"));
                conf.SourceFiles.Add(Path.Combine(implatformPath, "ImPlatform_gfx_opengl3.cpp"));
                break;

            case ImGfxConfig.Vulkan:
                conf.Defines.Add("IM_CONFIG_GFX=IM_GFX_VULKAN");
                conf.SourceFiles.Add(Path.Combine(imguiPath, "backends", "imgui_impl_vulkan.cpp"));
                conf.SourceFiles.Add(Path.Combine(implatformPath, "ImPlatform_gfx_vulkan.cpp"));
                break;

            case ImGfxConfig.Metal:
                conf.Defines.Add("IM_CONFIG_GFX=IM_GFX_METAL");
                // Metal backend uses .mm files (Objective-C++)
                conf.SourceFiles.Add(Path.Combine(imguiPath, "backends", "imgui_impl_metal.mm"));
                conf.SourceFiles.Add(Path.Combine(implatformPath, "ImPlatform_gfx_metal.mm"));
                break;
        }
    }

    private void ConfigurePlatformLibraries(Configuration conf, ImPlatformTarget target)
    {
        if (target.Platform == Platform.linux)
        {
            // Common Linux libraries
            conf.LibraryFiles.Add("dl");
            conf.LibraryFiles.Add("pthread");

            switch (target.GfxConfig)
            {
                case ImGfxConfig.OpenGL3:
                    conf.LibraryFiles.Add("GL");
                    break;
                case ImGfxConfig.Vulkan:
                    conf.LibraryFiles.Add("vulkan");
                    break;
            }
        }
        else if (target.Platform == Platform.mac)
        {
            // Common macOS frameworks
            conf.AdditionalLinkerOptions.Add("-framework Cocoa");
            conf.AdditionalLinkerOptions.Add("-framework IOKit");
            conf.AdditionalLinkerOptions.Add("-framework CoreVideo");

            switch (target.GfxConfig)
            {
                case ImGfxConfig.OpenGL3:
                    conf.AdditionalLinkerOptions.Add("-framework OpenGL");
                    break;
                case ImGfxConfig.Vulkan:
                    // MoltenVK or Vulkan SDK
                    conf.LibraryFiles.Add("vulkan");
                    conf.IncludePaths.Add("$(VULKAN_SDK)/include");
                    conf.LibraryPaths.Add("$(VULKAN_SDK)/lib");
                    break;
                case ImGfxConfig.Metal:
                    conf.AdditionalLinkerOptions.Add("-framework Metal");
                    conf.AdditionalLinkerOptions.Add("-framework MetalKit");
                    conf.AdditionalLinkerOptions.Add("-framework QuartzCore");
                    break;
            }
        }
    }
}
