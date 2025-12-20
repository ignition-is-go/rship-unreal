// Copyright Lucid. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using System.Diagnostics;
using EpicGames.Core;

public class RshipNDI : ModuleRules
{
	// Auto-detected based on presence of built Rust library
	private bool bHasRustNDISender = false;

	// Set to true to automatically build Rust library if missing
	private bool bAutoBuildRust = true;

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
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			RustLibFile = Path.Combine(RustLibRelease, "rship_ndi_sender.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			RustLibFile = Path.Combine(RustLibRelease, "librship_ndi_sender.a");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			RustLibFile = Path.Combine(RustLibRelease, "librship_ndi_sender.a");
		}

		// Debug: Show the path we're checking
		Log.TraceInformation("RshipNDI: ModuleDirectory = " + ModuleDirectory);
		Log.TraceInformation("RshipNDI: Looking for library at: " + RustLibFile);

		// Auto-build Rust library if missing and enabled
		if (!File.Exists(RustLibFile) && bAutoBuildRust && Directory.Exists(RustLibPath))
		{
			Log.TraceInformation("RshipNDI: Rust library not found, attempting auto-build...");
			TryBuildRustLibrary(RustLibPath);
		}

		if (File.Exists(RustLibFile))
		{
			bHasRustNDISender = true;
			Log.TraceInformation("RshipNDI: Rust NDI sender library found at " + RustLibFile);
		}
		else
		{
			Log.TraceInformation("RshipNDI: ==================================================");
			Log.TraceInformation("RshipNDI: Rust NDI sender library NOT found.");
			Log.TraceInformation("RshipNDI: NDI streaming will be DISABLED.");
			Log.TraceInformation("RshipNDI: ");
			Log.TraceInformation("RshipNDI: To enable NDI streaming, build the Rust library:");
			Log.TraceInformation("RshipNDI:   cd " + RustLibPath);
			Log.TraceInformation("RshipNDI:   cargo build --release");
			Log.TraceInformation("RshipNDI: ");
			Log.TraceInformation("RshipNDI: NOTE: At runtime, NDI Tools must be installed:");
			Log.TraceInformation("RshipNDI:   https://ndi.video/tools/");
			Log.TraceInformation("RshipNDI: ==================================================");
		}

		// Check for bundled NDI runtime (optional - for redistribution)
		string NDIRuntimePath = Path.Combine(ModuleDirectory, "ThirdParty", "NDI", "Bin");
		if (Directory.Exists(NDIRuntimePath))
		{
			// Add runtime dependency path for deployment
			RuntimeDependencies.Add(Path.Combine(NDIRuntimePath, "*"));
			Log.TraceInformation("RshipNDI: Bundled NDI runtime found at " + NDIRuntimePath);
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
				Log.TraceInformation("RshipNDI: CineCameraSceneCapture found at: " + IncludePath);
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
			Log.TraceInformation("RshipNDI: ==================================================");
			Log.TraceInformation("RshipNDI: CineCameraSceneCapture plugin NOT found.");
			Log.TraceInformation("RshipNDI: Using standard USceneCaptureComponent2D instead.");
			Log.TraceInformation("RshipNDI: For exact CineCamera DOF/lens matching, install");
			Log.TraceInformation("RshipNDI: the CineCameraSceneCapture plugin.");
			Log.TraceInformation("RshipNDI: ==================================================");
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

	/// <summary>
	/// Attempt to build the Rust NDI sender library automatically.
	/// This runs during UE build if the library is missing.
	/// </summary>
	private void TryBuildRustLibrary(string RustLibPath)
	{
		try
		{
			// Find cargo executable - check common locations on Windows
			string cargoPath = FindCargoExecutable();
			if (string.IsNullOrEmpty(cargoPath))
			{
				Log.TraceInformation("RshipNDI: cargo not found, skipping auto-build");
				Log.TraceInformation("RshipNDI: Install Rust from https://rustup.rs or add cargo to PATH");
				return;
			}

			Log.TraceInformation("RshipNDI: Found cargo at: " + cargoPath);
			Log.TraceInformation("RshipNDI: Building Rust library (this may take a minute on first build)...");

			ProcessStartInfo buildInfo = new ProcessStartInfo
			{
				FileName = cargoPath,
				Arguments = "build --release",
				WorkingDirectory = RustLibPath,
				RedirectStandardOutput = true,
				RedirectStandardError = true,
				UseShellExecute = false,
				CreateNoWindow = true
			};

			using (Process buildProcess = Process.Start(buildInfo))
			{
				// Stream output
				buildProcess.OutputDataReceived += (sender, e) => {
					if (!string.IsNullOrEmpty(e.Data))
						Log.TraceInformation("RshipNDI: [cargo] " + e.Data);
				};
				buildProcess.ErrorDataReceived += (sender, e) => {
					if (!string.IsNullOrEmpty(e.Data))
						Log.TraceInformation("RshipNDI: [cargo] " + e.Data);
				};

				buildProcess.BeginOutputReadLine();
				buildProcess.BeginErrorReadLine();

				// Wait up to 5 minutes for build
				bool finished = buildProcess.WaitForExit(300000);

				if (!finished)
				{
					Log.TraceInformation("RshipNDI: Rust build timed out after 5 minutes");
					buildProcess.Kill();
				}
				else if (buildProcess.ExitCode == 0)
				{
					Log.TraceInformation("RshipNDI: Rust library built successfully!");
				}
				else
				{
					Log.TraceInformation("RshipNDI: Rust build failed with exit code " + buildProcess.ExitCode);
				}
			}
		}
		catch (System.Exception ex)
		{
			Log.TraceInformation("RshipNDI: Auto-build failed: " + ex.Message);
		}
	}

	/// <summary>
	/// Find the cargo executable, checking PATH and common installation locations.
	/// </summary>
	private string FindCargoExecutable()
	{
		// First try PATH
		string cargoName = System.Runtime.InteropServices.RuntimeInformation.IsOSPlatform(
			System.Runtime.InteropServices.OSPlatform.Windows) ? "cargo.exe" : "cargo";

		// Check if cargo is in PATH
		try
		{
			ProcessStartInfo psi = new ProcessStartInfo
			{
				FileName = cargoName,
				Arguments = "--version",
				RedirectStandardOutput = true,
				RedirectStandardError = true,
				UseShellExecute = false,
				CreateNoWindow = true
			};

			using (Process p = Process.Start(psi))
			{
				p.WaitForExit(5000);
				if (p.ExitCode == 0)
				{
					Log.TraceInformation("RshipNDI: Found cargo in PATH");
					return cargoName; // Found in PATH
				}
			}
		}
		catch (System.Exception ex)
		{
			Log.TraceInformation("RshipNDI: cargo not in PATH: " + ex.Message);
		}

		// Check common installation locations
		string userProfile = System.Environment.GetFolderPath(System.Environment.SpecialFolder.UserProfile);

		// Fallback to environment variables (more reliable in UE build context)
		if (string.IsNullOrEmpty(userProfile))
		{
			// Windows
			userProfile = System.Environment.GetEnvironmentVariable("USERPROFILE");
		}
		if (string.IsNullOrEmpty(userProfile))
		{
			// macOS/Linux
			userProfile = System.Environment.GetEnvironmentVariable("HOME");
		}

		Log.TraceInformation("RshipNDI: User profile path: " + (userProfile ?? "(null)"));

		// Build list of possible paths
		var possiblePathsList = new System.Collections.Generic.List<string>();

		if (!string.IsNullOrEmpty(userProfile))
		{
			// Windows rustup default
			possiblePathsList.Add(Path.Combine(userProfile, ".cargo", "bin", "cargo.exe"));
			// macOS/Linux rustup default
			possiblePathsList.Add(Path.Combine(userProfile, ".cargo", "bin", "cargo"));
		}

		// Windows alternative
		string localAppData = System.Environment.GetFolderPath(System.Environment.SpecialFolder.LocalApplicationData);
		if (!string.IsNullOrEmpty(localAppData))
		{
			possiblePathsList.Add(Path.Combine(localAppData, "cargo", "bin", "cargo.exe"));
		}

		// Homebrew on macOS
		possiblePathsList.Add("/opt/homebrew/bin/cargo");
		possiblePathsList.Add("/usr/local/bin/cargo");
		// Linux system install
		possiblePathsList.Add("/usr/bin/cargo");

		string[] possiblePaths = possiblePathsList.ToArray();

		foreach (string path in possiblePaths)
		{
			if (File.Exists(path))
			{
				Log.TraceInformation("RshipNDI: Found cargo at: " + path);
				return path;
			}
		}

		Log.TraceInformation("RshipNDI: cargo not found in any of these locations:");
		foreach (string path in possiblePaths)
		{
			Log.TraceInformation("RshipNDI:   - " + path);
		}

		return null;
	}
}
