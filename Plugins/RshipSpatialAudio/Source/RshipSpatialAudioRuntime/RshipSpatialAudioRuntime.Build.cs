// Copyright Rocketship. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class RshipSpatialAudioRuntime : ModuleRules
{
	public RshipSpatialAudioRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Enable exceptions for audio processing edge cases
		bEnableExceptions = true;

		// Check for optional RshipExec plugin
		string RshipExecPluginPath = Path.Combine(ModuleDirectory, "..", "..", "..", "..", "RshipExec");
		bool bHasRshipExec = Directory.Exists(RshipExecPluginPath);
		if (bHasRshipExec)
		{
			PublicDefinitions.Add("RSHIP_SPATIAL_AUDIO_HAS_EXEC=1");
			System.Console.WriteLine("RshipSpatialAudioRuntime: RshipExec plugin found - enabling rShip/Myko integration");
		}
		else
		{
			PublicDefinitions.Add("RSHIP_SPATIAL_AUDIO_HAS_EXEC=0");
			System.Console.WriteLine("RshipSpatialAudioRuntime: RshipExec plugin not found - operating standalone");
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
				"AudioMixer",          // UE audio system integration
				"SignalProcessing",    // DSP utilities
				"AudioExtensions",     // Spatialization interfaces
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Json",
				"JsonUtilities",
				"Projects",
				"Sockets",             // UDP socket support
				"Networking",          // Network utilities (FIPv4Address, etc.)
			}
		);

		// Optional RshipExec integration
		if (bHasRshipExec)
		{
			PrivateDependencyModuleNames.Add("RshipExec");
		}

		// Editor-only dependencies for visualization
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
				}
			);
		}

		// Platform-specific audio IO
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// ASIO support will be added here
			PublicDefinitions.Add("SPATIAL_AUDIO_ASIO_SUPPORT=1");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			// CoreAudio support
			PublicDefinitions.Add("SPATIAL_AUDIO_COREAUDIO_SUPPORT=1");
		}
		else
		{
			PublicDefinitions.Add("SPATIAL_AUDIO_ASIO_SUPPORT=0");
			PublicDefinitions.Add("SPATIAL_AUDIO_COREAUDIO_SUPPORT=0");
		}

		// Maximum supported configuration
		PublicDefinitions.Add("SPATIAL_AUDIO_MAX_SPEAKERS=512");
		PublicDefinitions.Add("SPATIAL_AUDIO_MAX_SPEAKERS_PER_OBJECT=64");
		PublicDefinitions.Add("SPATIAL_AUDIO_MAX_OBJECTS=512");
		PublicDefinitions.Add("SPATIAL_AUDIO_MAX_OUTPUTS=256");
		PublicDefinitions.Add("SPATIAL_AUDIO_MAX_EQ_BANDS=16");
		PublicDefinitions.Add("SPATIAL_AUDIO_MAX_DELAY_MS=1000");
	}
}
