using UnrealBuildTool;

public class PulseRDGDeformEditor : ModuleRules
{
    public PulseRDGDeformEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "PulseRDGDeform"
            });

        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "AssetRegistry",
                "ContentBrowser",
                "Slate",
                "SlateCore",
                "ToolMenus",
                "UnrealEd"
            });
    }
}
