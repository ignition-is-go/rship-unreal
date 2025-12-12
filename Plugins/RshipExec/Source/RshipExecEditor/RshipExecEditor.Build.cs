// Copyright Rocketship. All Rights Reserved.

using UnrealBuildTool;

public class RshipExecEditor : ModuleRules
{
	public RshipExecEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Memory optimization: Use unity builds to reduce parallel compile memory pressure
		// Unity builds combine multiple .cpp files, reducing total memory needed
		bUseUnity = true;
		MinSourceFilesForUnityBuildOverride = 1;

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
				"Rship2110",  // For SMPTE 2110 status display
				"Sockets",    // For network interface enumeration
			}
		);
	}
}
