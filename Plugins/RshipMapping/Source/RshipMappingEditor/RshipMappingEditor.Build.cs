using UnrealBuildTool;

public class RshipMappingEditor : ModuleRules
{
	public RshipMappingEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		bUseUnity = true;
		MinSourceFilesForUnityBuildOverride = 1;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"RshipExec",
				"RshipMapping",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"UnrealEd",
				"EditorStyle",
				"InputCore",
				"ToolMenus",
				"WorkspaceMenuStructure",
				"Projects",
				"EditorWidgets",
				"Json",
				"ApplicationCore",
			}
		);
	}
}
