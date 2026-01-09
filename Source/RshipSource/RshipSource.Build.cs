// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RshipSource : ModuleRules
{
	public RshipSource(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"RshipSource",
			"RshipSource/Variant_Platforming",
			"RshipSource/Variant_Platforming/Animation",
			"RshipSource/Variant_Combat",
			"RshipSource/Variant_Combat/AI",
			"RshipSource/Variant_Combat/Animation",
			"RshipSource/Variant_Combat/Gameplay",
			"RshipSource/Variant_Combat/Interfaces",
			"RshipSource/Variant_Combat/UI",
			"RshipSource/Variant_SideScrolling",
			"RshipSource/Variant_SideScrolling/AI",
			"RshipSource/Variant_SideScrolling/Gameplay",
			"RshipSource/Variant_SideScrolling/Interfaces",
			"RshipSource/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
