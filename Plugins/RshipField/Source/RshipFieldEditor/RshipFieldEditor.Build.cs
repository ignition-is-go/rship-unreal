using UnrealBuildTool;

public class RshipFieldEditor : ModuleRules
{
    public RshipFieldEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "RshipField"
            });

        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "Slate",
                "SlateCore",
                "ToolMenus",
                "UnrealEd",
                "LevelEditor"
            });
    }
}
