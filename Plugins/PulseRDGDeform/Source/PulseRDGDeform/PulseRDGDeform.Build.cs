using UnrealBuildTool;

public class PulseRDGDeform : ModuleRules
{
    public PulseRDGDeform(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine"
            });

        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "Projects",
                "RHI",
                "RenderCore",
                "Renderer"
            });
    }
}
