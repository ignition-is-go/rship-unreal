// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class RshipExec : ModuleRules
{
	// Auto-detected based on presence of IXWebSocket in ThirdParty folder
	private bool bUseIXWebSocket = false;

	public RshipExec(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PrecompileForTargets = PrecompileTargetsType.Any;

		// Memory optimization: Use unity builds to reduce parallel compile memory pressure
		// Higher grouping = fewer parallel compiles = less memory needed
		bUseUnity = true;
		MinSourceFilesForUnityBuildOverride = 16; // Group more files to reduce memory (RshipExec has many files)
		NumIncludedBytesPerUnityCPPOverride = 1048576; // 1MB per unity file

		// Check if IXWebSocket is available (bundled as submodule)
		string IXWebSocketPath = Path.Combine(ModuleDirectory, "ThirdParty", "IXWebSocket", "ixwebsocket");
		if (Directory.Exists(IXWebSocketPath))
		{
			bUseIXWebSocket = true;
			System.Console.WriteLine("RshipExec: IXWebSocket found, enabling high-performance WebSocket");
		}
		else
		{
			System.Console.WriteLine("RshipExec: IXWebSocket not found, using UE WebSocket with send thread optimization");
		}

		// Define whether to use IXWebSocket - IXWebSocketWrapper.cpp checks this
		if (bUseIXWebSocket)
		{
			PublicDefinitions.Add("RSHIP_USE_IXWEBSOCKET=1");
		}
		else
		{
			PublicDefinitions.Add("RSHIP_USE_IXWEBSOCKET=0");
		}

		// Add msgpack-c include path (header-only library)
		string MsgPackIncludePath = Path.Combine(ModuleDirectory, "ThirdParty", "msgpack-c", "include");
		PublicIncludePaths.Add(MsgPackIncludePath);
		System.Console.WriteLine("RshipExec: msgpack-c headers added from " + MsgPackIncludePath);

		// Add IXWebSocket includes if available
		if (bUseIXWebSocket)
		{
			string IXWebSocketRoot = Path.Combine(ModuleDirectory, "ThirdParty", "IXWebSocket");

			// Add include paths for IXWebSocket headers
			PublicIncludePaths.Add(IXWebSocketRoot);
			PrivateIncludePaths.Add(IXWebSocketPath);

			// IXWebSocket build configuration
			// NOTE: Do NOT define IXWEBSOCKET_USE_TLS or IXWEBSOCKET_USE_ZLIB
			// Their code uses #ifdef so defining them to 0 still enables them!
			// By leaving them undefined, zlib and TLS are disabled.

			// Disable shadowing warnings for third-party code (IXWebSocket has variable shadowing)
			CppCompileWarningSettings.ShadowVariableWarningLevel = WarningLevel.Off;

			// Platform-specific defines
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				PublicDefinitions.Add("_WIN32");
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				PublicDefinitions.Add("__APPLE__");
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				PublicDefinitions.Add("__linux__");
			}

			// Enable exceptions for IXWebSocket (it uses them internally)
			bEnableExceptions = true;

			// IXWebSocket sources are compiled via Private/IXWebSocketWrapper.cpp (unity build)
		}

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"WebSockets",  // Still needed for fallback mode
				"Json",
				"JsonUtilities",
				"Sockets",     // For socket options
				"Networking",  // For network utilities
				"HTTP",        // For IES profile fetching
				"RenderCore",  // For texture generation
				"RHI",         // For GRHIGlobals (feedback reporter)
				"InputCore",   // For FKey, EKeys (editor widget, camera presets)
				"Niagara",     // For RshipNiagaraBinding
				"NiagaraCore", // For Niagara types
				"ControlRig",  // For RshipControlRigBinding
				"RigVM",       // For ControlRig
				"LiveLinkInterface", // For RshipLiveLinkSource
				"LevelSequence",     // For RshipSequencerSync
				"MovieScene",        // For sequencer playback
				"ProceduralMeshComponent", // For RshipFixtureVisualizer
				"CinematicCamera",         // For CineCameraActor (NDI panel, camera managers)
			}
		);

		// PCG is optional - check if it exists
		bool bHasPCG = false;
		try
		{
			// Check if PCG module exists by looking for its header
			string PCGModulePath = Path.Combine(Target.ProjectFile?.Directory?.FullName ?? "", "..", "Engine", "Plugins", "PCG");
			if (Directory.Exists(PCGModulePath))
			{
				bHasPCG = true;
			}
		}
		catch { }

		// For now, disable PCG by default - users can enable by uncommenting below
		bHasPCG = false;
		// To enable PCG: set bHasPCG = true above and add "PCG" plugin to your .uproject

		// Always include PCG directory paths - base types don't require PCG plugin
		// Only RshipPCGSpawnActorSettings requires PCG and is guarded with #if RSHIP_HAS_PCG
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public", "PCG"));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private", "PCG"));

		if (bHasPCG)
		{
			PublicDependencyModuleNames.Add("PCG");
			PublicDefinitions.Add("RSHIP_HAS_PCG=1");
			System.Console.WriteLine("RshipExec: PCG plugin enabled, PCG spawn actor node available");
		}
		else
		{
			PublicDefinitions.Add("RSHIP_HAS_PCG=0");
			System.Console.WriteLine("RshipExec: PCG plugin not enabled, using manual component attachment");
		}

		// RshipNDI plugin for NDI streaming (optional)
		// Check if the RshipNDI plugin exists in sibling directory
		string RshipNDIPluginPath = Path.Combine(ModuleDirectory, "..", "..", "..", "RshipNDI");
		bool bHasRshipNDI = Directory.Exists(RshipNDIPluginPath);

		if (bHasRshipNDI)
		{
			PrivateDependencyModuleNames.Add("RshipNDI");
			PublicDefinitions.Add("RSHIP_HAS_NDI=1");
			System.Console.WriteLine("RshipExec: RshipNDI plugin found, NDI dashboard enabled");
		}
		else
		{
			PublicDefinitions.Add("RSHIP_HAS_NDI=0");
			System.Console.WriteLine("RshipExec: RshipNDI plugin not found, NDI dashboard disabled");
		}

		// RshipColorManagement plugin for broadcast color control
		string RshipColorPluginPath = Path.Combine(ModuleDirectory, "..", "..", "..", "RshipColorManagement");
		bool bHasRshipColor = Directory.Exists(RshipColorPluginPath);

		if (bHasRshipColor)
		{
			PrivateDependencyModuleNames.Add("RshipColorManagement");
			PublicDefinitions.Add("RSHIP_HAS_COLOR_MANAGEMENT=1");
			System.Console.WriteLine("RshipExec: RshipColorManagement plugin found, color targets enabled");
		}
		else
		{
			PublicDefinitions.Add("RSHIP_HAS_COLOR_MANAGEMENT=0");
			System.Console.WriteLine("RshipExec: RshipColorManagement plugin not found, color targets disabled");
		}

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"Settings",
			}
		);

		// Editor-only dependencies for viewport selection sync
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"LevelEditor",
				}
			);
		}

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
		);

		// Platform-specific libraries
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.Add("Ws2_32.lib");
		}
	}
}
