using UnrealBuildTool;

public class RshipMapping : ModuleRules
{
	public RshipMapping(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PrecompileForTargets = PrecompileTargetsType.Any;

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
				"Json",
				"JsonUtilities",
				"RenderCore",
				"RHI",
				"InputCore",
				"Niagara",
				"NiagaraCore",
				"ControlRig",
				"RigVM",
				"LiveLinkInterface",
				"LevelSequence",
				"MovieScene",
				"ProceduralMeshComponent",
				"CinematicCamera",
				"HTTP",
				"Sockets",
				"Networking",
				"WebSockets",
				"Slate",
				"SlateCore",
				"Settings",
				"ImageWrapper",
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"LevelEditor",
					"AssetRegistry",
				}
			);
		}

	}
}
