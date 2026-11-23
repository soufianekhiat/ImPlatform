using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using Sharpmake;

namespace ImPlatform
{
    // Platform layer compiled into a configuration (IM_CONFIG_PLATFORM).
    [Fragment, Flags]
    public enum App
    {
        Win32 = 1 << 0,
        GLFW  = 1 << 1,
        SDL2  = 1 << 2,
        SDL3  = 1 << 3,
    }

    // Graphics backend compiled into a configuration (IM_CONFIG_GFX).
    [Fragment, Flags]
    public enum Gfx
    {
        DX9     = 1 << 0,
        DX10    = 1 << 1,
        DX11    = 1 << 2,
        DX12    = 1 << 3,
        OpenGL3 = 1 << 4,
        Vulkan  = 1 << 5,
        Metal   = 1 << 6,
    }

    [Generate]
    public class ImPlatformTarget : Target
    {
        public App App = App.Win32;
        public Gfx Gfx = Gfx.DX11;

        public ImPlatformTarget()
        {
        }

        public ImPlatformTarget(Platform platform, DevEnv devEnv, App app, Gfx gfx)
            : base(platform, devEnv, Optimization.Debug)
        {
            App = app;
            Gfx = gfx;
        }

        // Configuration name, also the solution configuration: Win32_DX11, GLFW_Vulkan, ...
        public string ConfigName { get { return App + "_" + Gfx; } }

        // The Xcode and makefile generators name configurations after the target, not after conf.Name.
        public override string Name { get { return ConfigName; } }

        // The App x Gfx pairs of the OS running Sharpmake (Windows: Visual Studio, Linux: make, macOS: Xcode).
        public static ImPlatformTarget[] All()
        {
            Platform host = Util.GetExecutingPlatform();
            return AllPlatforms().Where(t => t.Platform == host).ToArray();
        }

        // Only the App x Gfx pairs that exist on each OS; CMakeLists.txt mirrors this list.
        public static ImPlatformTarget[] AllPlatforms()
        {
            var targets = new List<ImPlatformTarget>();

            foreach (Gfx gfx in new[] { Gfx.DX9, Gfx.DX10, Gfx.DX11, Gfx.DX12, Gfx.OpenGL3, Gfx.Vulkan })
                targets.Add(new ImPlatformTarget(Platform.win64, DevEnv.vs2022, App.Win32, gfx));
            foreach (Gfx gfx in new[] { Gfx.OpenGL3, Gfx.Vulkan })
                targets.Add(new ImPlatformTarget(Platform.win64, DevEnv.vs2022, App.GLFW, gfx));
            foreach (App app in new[] { App.SDL2, App.SDL3 })
                foreach (Gfx gfx in new[] { Gfx.DX11, Gfx.OpenGL3, Gfx.Vulkan })
                    targets.Add(new ImPlatformTarget(Platform.win64, DevEnv.vs2022, app, gfx));

            foreach (App app in new[] { App.GLFW, App.SDL2, App.SDL3 })
            {
                foreach (Gfx gfx in new[] { Gfx.OpenGL3, Gfx.Vulkan })
                    targets.Add(new ImPlatformTarget(Platform.linux, DevEnv.make, app, gfx));
                foreach (Gfx gfx in new[] { Gfx.OpenGL3, Gfx.Metal })
                    targets.Add(new ImPlatformTarget(Platform.mac, DevEnv.xcode, app, gfx));
            }

            return targets.ToArray();
        }
    }

    public static class Paths
    {
        private static string ThisFile([CallerFilePath] string p = null) { return p; }

        // Repository root: buildsystem/sharpmake/src -> ../../..
        public static readonly string Root = Path.GetFullPath(Path.Combine(Path.GetDirectoryName(ThisFile()), "..", "..", ".."));
        public static readonly string ImGui = Path.Combine(Root, "imgui");
        public static readonly string ImPlatform = Path.Combine(Root, "ImPlatform");
        public static readonly string Demo = Path.Combine(Root, "ImPlatformDemo");
    }
}
