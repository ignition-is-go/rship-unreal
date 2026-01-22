using UnrealBuildTool;

public class CoverageHeatmap : ModuleRules
{
    public CoverageHeatmap(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "CinematicCamera",
            "RHI",
            "RenderCore"
        });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
            PrivateDependencyModuleNames.Add("GLTFExporter");
        }
    }
}
