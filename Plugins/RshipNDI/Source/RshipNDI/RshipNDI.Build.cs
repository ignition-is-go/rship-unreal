// Copyright Lucid. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class RshipNDI : ModuleRules
{
	// Auto-detected based on presence of built Rust library
	private bool bHasRustNDISender = false;

	public RshipNDI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PrecompileForTargets = PrecompileTargetsType.Any;

		// Memory optimization: Use unity builds to reduce parallel compile memory pressure
		bUseUnity = true;
		MinSourceFilesForUnityBuildOverride = 1; // Combine all files into fewer unity files

		// Check for Rust NDI sender library
		string RustLibPath = Path.Combine(ModuleDirectory, "ThirdParty", "rship-ndi-sender");
		string RustLibRelease = Path.Combine(RustLibPath, "target", "release");
		string RustInclude = Path.Combine(RustLibPath, "include");

		// Platform-specific library detection
		string RustLibFile = "";
		string BuildScript = "";
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			RustLibFile = Path.Combine(RustLibRelease, "rship_ndi_sender.lib");
			BuildScript = Path.Combine(RustLibPath, "build.bat");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			RustLibFile = Path.Combine(RustLibRelease, "librship_ndi_sender.a");
			BuildScript = Path.Combine(RustLibPath, "build.sh");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			RustLibFile = Path.Combine(RustLibRelease, "librship_ndi_sender.a");
			BuildScript = Path.Combine(RustLibPath, "build.sh");
		}

		if (File.Exists(RustLibFile))
		{
			bHasRustNDISender = true;
		}
		else
		{
			// Clear warning that NDI is disabled and how to fix
			System.Console.WriteLine("");
			System.Console.WriteLine("================================================================================");
			System.Console.WriteLine("  RshipNDI: Rust NDI sender library NOT found - NDI streaming DISABLED");
			System.Console.WriteLine("");
			System.Console.WriteLine("  To enable NDI streaming, run the build script:");
			System.Console.WriteLine("    " + BuildScript);
			System.Console.WriteLine("");
			System.Console.WriteLine("  Or manually:");
			System.Console.WriteLine("    cd " + RustLibPath);
			System.Console.WriteLine("    cargo build --release");
			System.Console.WriteLine("");
			System.Console.WriteLine("  Requires Rust: https://rustup.rs");
			System.Console.WriteLine("  Requires NDI Tools at runtime: https://ndi.video/tools/");
			System.Console.WriteLine("================================================================================");
			System.Console.WriteLine("");
		}

		// Check for bundled NDI runtime (optional - for redistribution)
		string NDIRuntimePath = Path.Combine(ModuleDirectory, "ThirdParty", "NDI", "Bin");
		if (Directory.Exists(NDIRuntimePath))
		{
			// Add runtime dependency path for deployment
			RuntimeDependencies.Add(Path.Combine(NDIRuntimePath, "*"));
		}

		// Define whether Rust NDI sender is available
		if (bHasRustNDISender)
		{
			PublicDefinitions.Add("RSHIP_HAS_NDI_SENDER=1");

			// Add Rust include path for generated headers
			PublicIncludePaths.Add(RustInclude);

			// Link Rust static library
			PublicAdditionalLibraries.Add(RustLibFile);

			// Platform-specific system libraries required by Rust
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				PublicSystemLibraries.AddRange(new string[] {
					"Ws2_32.lib",    // Windows sockets
					"Userenv.lib",   // User environment
					"Bcrypt.lib",    // Cryptography
					"ntdll.lib",     // NT layer
					"Advapi32.lib",  // Advanced API
				});
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				PublicFrameworks.AddRange(new string[] {
					"Security",
					"SystemConfiguration",
					"CoreFoundation",
				});
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				PublicSystemLibraries.AddRange(new string[] {
					"pthread",
					"dl",
					"m",
				});
			}
		}
		else
		{
			PublicDefinitions.Add("RSHIP_HAS_NDI_SENDER=0");
		}

		// Enable exceptions for Rust FFI panic handling
		bEnableExceptions = true;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"RenderCore",
				"RHI",
				"CinematicCamera",         // For ACineCameraActor, UCineCameraComponent
				"RshipColorManagement",    // For color config and subsystem
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"Projects",
			}
		);

		// CineCameraSceneCapture plugin for UCineCaptureComponent2D (optional)
		// This plugin provides UCineCaptureComponent2D for exact CineCamera matching
		// If not available, we fall back to standard USceneCaptureComponent2D

		// EngineDirectory points to Engine/Source, so we go up to Engine root first
		string EngineRoot = Path.GetFullPath(Path.Combine(EngineDirectory, ".."));

		// Try multiple potential plugin locations for CineCameraSceneCapture
		string[] PotentialPluginPaths = new string[]
		{
			// Standard engine installation - VirtualProduction category
			Path.Combine(EngineRoot, "Plugins", "VirtualProduction", "CineCameraSceneCapture", "Source", "CineCameraSceneCapture", "Public"),
			Path.Combine(EngineRoot, "Plugins", "VirtualProduction", "CineCameraSceneCapture", "Source", "CineCameraSceneCapture"),
			// Alternative - root Plugins folder
			Path.Combine(EngineRoot, "Plugins", "CineCameraSceneCapture", "Source", "CineCameraSceneCapture", "Public"),
			Path.Combine(EngineRoot, "Plugins", "CineCameraSceneCapture", "Source", "CineCameraSceneCapture"),
			// Alternative - Media category
			Path.Combine(EngineRoot, "Plugins", "Media", "CineCameraSceneCapture", "Source", "CineCameraSceneCapture", "Public"),
			// Alternative - Runtime category
			Path.Combine(EngineRoot, "Plugins", "Runtime", "CineCameraSceneCapture", "Source", "CineCameraSceneCapture", "Public"),
			// Alternative - Compositing category
			Path.Combine(EngineRoot, "Plugins", "Compositing", "CineCameraSceneCapture", "Source", "CineCameraSceneCapture", "Public"),
		};

		bool bFoundCineCapture = false;
		foreach (string IncludePath in PotentialPluginPaths)
		{
			if (Directory.Exists(IncludePath))
			{
				PublicIncludePaths.Add(IncludePath);
				PrivateDependencyModuleNames.Add("CineCameraSceneCapture");
				bFoundCineCapture = true;
				break;
			}
		}

		// Define whether CineCameraSceneCapture is available
		if (bFoundCineCapture)
		{
			PublicDefinitions.Add("RSHIP_HAS_CINE_CAPTURE=1");
		}
		else
		{
			PublicDefinitions.Add("RSHIP_HAS_CINE_CAPTURE=0");
		}

		// Editor-only dependencies
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
				}
			);
		}
	}
}
