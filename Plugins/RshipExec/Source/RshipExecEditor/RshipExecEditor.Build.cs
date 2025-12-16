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
				"EditorStyle",
				"InputCore",
				"ToolMenus",
				"WorkspaceMenuStructure",
				"Projects",
				"EditorWidgets",
				"Sockets",           // For network interface enumeration
				"LiveLinkInterface", // For ILiveLinkClient in SRshipLiveLinkPanel
				"LiveLink",          // For LiveLinkClient and LiveLink roles
				"DesktopPlatform",   // For DesktopPlatformModule in SRshipAssetSyncPanel
				"Json",              // For FJsonObject in SRshipTimecodePanel
			}
		);

		// Rship2110 plugin for SMPTE 2110 status display (optional)
		// Check if the plugin exists to avoid circular dependency issues
		string Rship2110PluginPath = Path.Combine(ModuleDirectory, "..", "..", "..", "..", "Rship2110");
		bool bHasRship2110 = Directory.Exists(Rship2110PluginPath);

		if (bHasRship2110)
		{
			PrivateDependencyModuleNames.Add("Rship2110");
			PublicDefinitions.Add("RSHIP_EDITOR_HAS_2110=1");
			System.Console.WriteLine("RshipExecEditor: Rship2110 plugin found, 2110 status display enabled");
		}
		else
		{
			PublicDefinitions.Add("RSHIP_EDITOR_HAS_2110=0");
			System.Console.WriteLine("RshipExecEditor: Rship2110 plugin not found, 2110 status display disabled");
		}
	}
}
