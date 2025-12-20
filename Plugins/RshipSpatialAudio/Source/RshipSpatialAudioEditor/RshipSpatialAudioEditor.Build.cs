// Copyright Rocketship. All Rights Reserved.

using UnrealBuildTool;

public class RshipSpatialAudioEditor : ModuleRules
{
	public RshipSpatialAudioEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"RshipSpatialAudioRuntime",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"RshipExec",
				"Slate",
				"SlateCore",
				"InputCore",
				"UnrealEd",
				"LevelEditor",
				"EditorStyle",
				"EditorSubsystem",
				"EditorFramework",
				"ToolMenus",
				"PropertyEditor",
				"DetailCustomizations",
				"WorkspaceMenuStructure",
				"Projects",
				"Json",
				"JsonUtilities",
				"DesktopPlatform",      // File dialogs for import
				"MainFrame",
				"StatusBar",
				"ApplicationCore",      // Clipboard support
			}
		);
	}
}
