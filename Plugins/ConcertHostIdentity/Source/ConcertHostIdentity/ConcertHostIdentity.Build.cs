using UnrealBuildTool;

public class ConcertHostIdentity : ModuleRules
{
	public ConcertHostIdentity(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("ConcertClient");
		}
	}
}
