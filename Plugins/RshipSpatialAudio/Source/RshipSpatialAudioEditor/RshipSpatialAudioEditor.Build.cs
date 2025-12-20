// Copyright Rocketship. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class RshipSpatialAudioEditor : ModuleRules
{
	public RshipSpatialAudioEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Check for optional RshipExec plugin
		string RshipExecPluginPath = Path.Combine(ModuleDirectory, "..", "..", "..", "..", "RshipExec");
		bool bHasRshipExec = Directory.Exists(RshipExecPluginPath);
		// Note: RSHIP_SPATIAL_AUDIO_HAS_EXEC is already defined by Runtime module

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

		// Optional RshipExec integration
		if (bHasRshipExec)
		{
			PrivateDependencyModuleNames.Add("RshipExec");
		}
	}
}
