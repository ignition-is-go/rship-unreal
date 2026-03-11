// Copyright Rocketship. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

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
				"LevelEditor",
				"SceneOutliner",
				"EditorStyle",
				"InputCore",
				"ToolMenus",
				"WorkspaceMenuStructure",
				"Projects",
				"EditorWidgets",
				"LiveLinkInterface", // For ILiveLinkClient in SRshipLiveLinkPanel
				"LiveLink",          // For LiveLinkClient and LiveLink roles
				"DesktopPlatform",   // For DesktopPlatformModule in SRshipAssetSyncPanel
				"Json",              // For FJsonObject in SRshipTimecodePanel
				"PropertyEditor",    // Native Details panel widgets
				"StructUtils",       // FInstancedPropertyBag
				"StructUtilsEditor", // FInstancePropertyBagStructureDataProvider
				"ClassViewer",       // Component class picker dialog
			}
			);
	}
}


