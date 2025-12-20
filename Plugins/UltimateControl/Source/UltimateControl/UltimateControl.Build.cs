// Copyright Rocketship. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UltimateControl : ModuleRules
{
	public UltimateControl(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Required for async and HTTP functionality
		bEnableExceptions = true;

		// Check for optional RshipColorManagement plugin
		string RshipColorMgmtPluginPath = Path.Combine(ModuleDirectory, "..", "..", "..", "..", "RshipColorManagement");
		bool bHasRshipColorManagement = Directory.Exists(RshipColorMgmtPluginPath);
		if (bHasRshipColorManagement)
		{
			PublicDefinitions.Add("ULTIMATE_CONTROL_HAS_COLOR_MANAGEMENT=1");
			System.Console.WriteLine("UltimateControl: RshipColorManagement plugin found - enabling color management");
		}
		else
		{
			PublicDefinitions.Add("ULTIMATE_CONTROL_HAS_COLOR_MANAGEMENT=0");
			System.Console.WriteLine("UltimateControl: RshipColorManagement plugin not found - color endpoints disabled");
		}

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
				"Json",
				"JsonUtilities",
				"HTTP",
				"Sockets",
				"Networking",
			}
		);

		// Optional RshipColorManagement integration
		if (bHasRshipColorManagement)
		{
			PublicDependencyModuleNames.Add("RshipColorManagement");
		}

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"InputCore",
				"Settings",
				"HTTPServer",
				"WebSockets",
				"Projects",
				"DeveloperSettings",
			}
		);

		// Editor-only modules
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"LevelEditor",
					"AssetRegistry",
					"AssetTools",
					"ContentBrowser",
					"BlueprintGraph",
					"Kismet",
					"KismetCompiler",
					"GraphEditor",
					"PropertyEditor",
					"EditorStyle",
					"EditorSubsystem",
					"SourceControl",
					"Blutility",
					"UMGEditor",
					"EditorFramework",
					"ToolMenus",
					"SubobjectDataInterface",
					"PhysicsCore",
					"Landscape",
					"Foliage",
					"DesktopPlatform",
					"MainFrame",
					"WorkspaceMenuStructure",
					"StatusBar",
					"OutputLog",
					"MessageLog",
					"AutomationController",
					"AIModule",
					"NavigationSystem",
					"RHI",
					"RenderCore",
					"Niagara",
					"NiagaraCore",
					"NiagaraEditor",
					// Sequencer support
					"LevelSequence",
					"MovieScene",
					"MovieSceneTracks",
					"Sequencer",
					// Asset library
					"EditorScriptingUtilities",
					// Material editor
					"MaterialEditor",
					// Settings
					"SettingsEditor",
				}
			);

			// Add definitions for editor-only code
			PublicDefinitions.Add("WITH_ULTIMATE_CONTROL_EDITOR=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_ULTIMATE_CONTROL_EDITOR=0");
		}
	}
}
