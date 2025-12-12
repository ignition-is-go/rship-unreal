// Copyright Rocketship. All Rights Reserved.

using UnrealBuildTool;

public class RshipExecEditor : ModuleRules
{
	public RshipExecEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"RshipExec",
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
			}
		);
	}
}
