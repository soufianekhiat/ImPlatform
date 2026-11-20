using Sharpmake;

[module: Sharpmake.Include("common.cs")]
[module: Sharpmake.Include("ImPlatformDemo.cs")]

namespace ImPlatform
{
    // One solution per OS at the repository root: ImPlatform_vs2022_win64.sln,
    // ImPlatform_make_linux.make, ImPlatform_xcode_mac.xcworkspace.
    [Generate]
    public class ImPlatformSolution : Solution
    {
        public ImPlatformSolution() : base(typeof(ImPlatformTarget))
        {
            Name = "ImPlatform";
            IsFileNameToLower = false;
            AddTargets(ImPlatformTarget.All());
        }

        [Configure]
        public void ConfigureAll(Configuration conf, ImPlatformTarget target)
        {
            conf.Name = target.ConfigName;
            conf.SolutionFileName = "[solution.Name]_[target.DevEnv]_[target.Platform]";
            conf.SolutionPath = Paths.Root;
            conf.AddProject<ImPlatformDemoProject>(target);
        }

        [Main]
        public static void SharpmakeMain(Arguments args)
        {
            // Makefiles link through g++ (Sharpmake's default calls ld directly).
            var linux = (Linux.LinuxPlatform)PlatformRegistry.Get<IPlatformDescriptor>(Platform.linux);
            linux.IsLinkerInvokedViaCompiler = true;

            args.Generate<ImPlatformSolution>();
        }
    }
}
