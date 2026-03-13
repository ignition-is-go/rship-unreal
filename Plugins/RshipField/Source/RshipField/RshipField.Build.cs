using UnrealBuildTool;

public class RshipField : ModuleRules
{
    public RshipField(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Json",
                "JsonUtilities"
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
