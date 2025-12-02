using System.IO;
using Sharpmake;

// =============================================================================
// ImPlatform Sharpmake Configuration
// Generates Makefiles for Linux and macOS builds
// =============================================================================

[module: Sharpmake.Include("implatform.sharpmake.cs")]

public static class Globals
{
    public static string RootDirectory = @"[project.SharpmakeCsPath]\..\";
}

// =============================================================================
// Custom Targets
// =============================================================================
public class ImPlatformTarget : ITarget
{
    public Platform Platform;
    public DevEnv DevEnv;
    public Optimization Optimization;
    public ImPlatformConfig PlatformConfig;
    public ImGfxConfig GfxConfig;
}

public enum ImPlatformConfig
{
    GLFW,
    SDL2,
    SDL3
}

public enum ImGfxConfig
{
    OpenGL3,
    Vulkan,
    Metal
}

// =============================================================================
// Main Solution
// =============================================================================
[Generate]
public class ImPlatformSolution : Solution
{
    public ImPlatformSolution()
    {
        Name = "ImPlatformDemo";

        AddTargets(GetTargets());
    }

    public static ITarget[] GetTargets()
    {
        var targets = new List<ImPlatformTarget>();

        // Linux targets
        targets.AddRange(new[]
        {
            new ImPlatformTarget { Platform = Platform.linux, DevEnv = DevEnv.make, Optimization = Optimization.Debug, PlatformConfig = ImPlatformConfig.GLFW, GfxConfig = ImGfxConfig.OpenGL3 },
            new ImPlatformTarget { Platform = Platform.linux, DevEnv = DevEnv.make, Optimization = Optimization.Debug, PlatformConfig = ImPlatformConfig.GLFW, GfxConfig = ImGfxConfig.Vulkan },
            new ImPlatformTarget { Platform = Platform.linux, DevEnv = DevEnv.make, Optimization = Optimization.Debug, PlatformConfig = ImPlatformConfig.SDL2, GfxConfig = ImGfxConfig.OpenGL3 },
            new ImPlatformTarget { Platform = Platform.linux, DevEnv = DevEnv.make, Optimization = Optimization.Debug, PlatformConfig = ImPlatformConfig.SDL2, GfxConfig = ImGfxConfig.Vulkan },
            new ImPlatformTarget { Platform = Platform.linux, DevEnv = DevEnv.make, Optimization = Optimization.Debug, PlatformConfig = ImPlatformConfig.SDL3, GfxConfig = ImGfxConfig.OpenGL3 },
            new ImPlatformTarget { Platform = Platform.linux, DevEnv = DevEnv.make, Optimization = Optimization.Debug, PlatformConfig = ImPlatformConfig.SDL3, GfxConfig = ImGfxConfig.Vulkan },
            new ImPlatformTarget { Platform = Platform.linux, DevEnv = DevEnv.make, Optimization = Optimization.Release, PlatformConfig = ImPlatformConfig.GLFW, GfxConfig = ImGfxConfig.OpenGL3 },
            new ImPlatformTarget { Platform = Platform.linux, DevEnv = DevEnv.make, Optimization = Optimization.Release, PlatformConfig = ImPlatformConfig.GLFW, GfxConfig = ImGfxConfig.Vulkan },
            new ImPlatformTarget { Platform = Platform.linux, DevEnv = DevEnv.make, Optimization = Optimization.Release, PlatformConfig = ImPlatformConfig.SDL2, GfxConfig = ImGfxConfig.OpenGL3 },
            new ImPlatformTarget { Platform = Platform.linux, DevEnv = DevEnv.make, Optimization = Optimization.Release, PlatformConfig = ImPlatformConfig.SDL2, GfxConfig = ImGfxConfig.Vulkan },
            new ImPlatformTarget { Platform = Platform.linux, DevEnv = DevEnv.make, Optimization = Optimization.Release, PlatformConfig = ImPlatformConfig.SDL3, GfxConfig = ImGfxConfig.OpenGL3 },
            new ImPlatformTarget { Platform = Platform.linux, DevEnv = DevEnv.make, Optimization = Optimization.Release, PlatformConfig = ImPlatformConfig.SDL3, GfxConfig = ImGfxConfig.Vulkan },
        });

        // macOS targets
        targets.AddRange(new[]
        {
            new ImPlatformTarget { Platform = Platform.mac, DevEnv = DevEnv.make, Optimization = Optimization.Debug, PlatformConfig = ImPlatformConfig.GLFW, GfxConfig = ImGfxConfig.OpenGL3 },
            new ImPlatformTarget { Platform = Platform.mac, DevEnv = DevEnv.make, Optimization = Optimization.Debug, PlatformConfig = ImPlatformConfig.GLFW, GfxConfig = ImGfxConfig.Metal },
            new ImPlatformTarget { Platform = Platform.mac, DevEnv = DevEnv.make, Optimization = Optimization.Debug, PlatformConfig = ImPlatformConfig.SDL2, GfxConfig = ImGfxConfig.OpenGL3 },
            new ImPlatformTarget { Platform = Platform.mac, DevEnv = DevEnv.make, Optimization = Optimization.Debug, PlatformConfig = ImPlatformConfig.SDL2, GfxConfig = ImGfxConfig.Metal },
            new ImPlatformTarget { Platform = Platform.mac, DevEnv = DevEnv.make, Optimization = Optimization.Debug, PlatformConfig = ImPlatformConfig.SDL3, GfxConfig = ImGfxConfig.OpenGL3 },
            new ImPlatformTarget { Platform = Platform.mac, DevEnv = DevEnv.make, Optimization = Optimization.Debug, PlatformConfig = ImPlatformConfig.SDL3, GfxConfig = ImGfxConfig.Metal },
            new ImPlatformTarget { Platform = Platform.mac, DevEnv = DevEnv.make, Optimization = Optimization.Release, PlatformConfig = ImPlatformConfig.GLFW, GfxConfig = ImGfxConfig.OpenGL3 },
            new ImPlatformTarget { Platform = Platform.mac, DevEnv = DevEnv.make, Optimization = Optimization.Release, PlatformConfig = ImPlatformConfig.GLFW, GfxConfig = ImGfxConfig.Metal },
            new ImPlatformTarget { Platform = Platform.mac, DevEnv = DevEnv.make, Optimization = Optimization.Release, PlatformConfig = ImPlatformConfig.SDL2, GfxConfig = ImGfxConfig.OpenGL3 },
            new ImPlatformTarget { Platform = Platform.mac, DevEnv = DevEnv.make, Optimization = Optimization.Release, PlatformConfig = ImPlatformConfig.SDL2, GfxConfig = ImGfxConfig.Metal },
            new ImPlatformTarget { Platform = Platform.mac, DevEnv = DevEnv.make, Optimization = Optimization.Release, PlatformConfig = ImPlatformConfig.SDL3, GfxConfig = ImGfxConfig.OpenGL3 },
            new ImPlatformTarget { Platform = Platform.mac, DevEnv = DevEnv.make, Optimization = Optimization.Release, PlatformConfig = ImPlatformConfig.SDL3, GfxConfig = ImGfxConfig.Metal },
        });

        return targets.ToArray();
    }

    [Configure]
    public void ConfigureAll(Configuration conf, ImPlatformTarget target)
    {
        conf.SolutionFileName = $"ImPlatformDemo_{target.Platform}";
        conf.SolutionPath = Path.Combine(Globals.RootDirectory, "generated", target.Platform.ToString());

        conf.AddProject<ImPlatformDemoProject>(target);
    }
}

// =============================================================================
// Main Entry Point
// =============================================================================
public static class Main
{
    [Sharpmake.Main]
    public static void SharpmakeMain(Sharpmake.Arguments arguments)
    {
        arguments.Generate<ImPlatformSolution>();
    }
}
