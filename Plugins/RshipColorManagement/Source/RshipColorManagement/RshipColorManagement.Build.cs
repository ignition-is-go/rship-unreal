// Copyright Lucid. All Rights Reserved.

using UnrealBuildTool;

public class RshipColorManagement : ModuleRules
{
	public RshipColorManagement(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"CinematicCamera",  // For CineCameraActor/CineCameraComponent
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
			}
		);
	}
}
