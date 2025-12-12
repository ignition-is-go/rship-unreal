// Copyright Lucid. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using System.Diagnostics;

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

		// Auto-build Rust library if missing and enabled
		if (!File.Exists(RustLibFile) && bAutoBuildRust && Directory.Exists(RustLibPath))
		{
			System.Console.WriteLine("RshipNDI: Rust library not found, attempting auto-build...");
			TryBuildRustLibrary(RustLibPath);
		}

		if (File.Exists(RustLibFile))
		{
			bHasRustNDISender = true;
			System.Console.WriteLine("RshipNDI: Rust NDI sender library found at " + RustLibFile);
		}
		else
		{
			System.Console.WriteLine("RshipNDI: ==================================================");
			System.Console.WriteLine("RshipNDI: Rust NDI sender library NOT found.");
			System.Console.WriteLine("RshipNDI: NDI streaming will be DISABLED.");
			System.Console.WriteLine("RshipNDI: ");
			System.Console.WriteLine("RshipNDI: To enable NDI streaming, build the Rust library:");
			System.Console.WriteLine("RshipNDI:   cd " + RustLibPath);
			System.Console.WriteLine("RshipNDI:   cargo build --release");
			System.Console.WriteLine("RshipNDI: ");
			System.Console.WriteLine("RshipNDI: NOTE: At runtime, NDI Tools must be installed:");
			System.Console.WriteLine("RshipNDI:   https://ndi.video/tools/");
			System.Console.WriteLine("RshipNDI: ==================================================");
		}

		// Check for bundled NDI runtime (optional - for redistribution)
		string NDIRuntimePath = Path.Combine(ModuleDirectory, "ThirdParty", "NDI", "Bin");
		if (Directory.Exists(NDIRuntimePath))
		{
			// Add runtime dependency path for deployment
			RuntimeDependencies.Add(Path.Combine(NDIRuntimePath, "*"));
			System.Console.WriteLine("RshipNDI: Bundled NDI runtime found at " + NDIRuntimePath);
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

		// CineCameraSceneCapture plugin for UCineCaptureComponent2D
		// NOTE: This plugin must be enabled in the project
		PrivateDependencyModuleNames.Add("CineCameraSceneCapture");

		// Add explicit include path for CineCameraSceneCapture module
		// The module doesn't expose its headers via the standard mechanism
		// EngineDirectory points to Engine/Source, so we go up to Engine root first
		string EngineRoot = Path.GetFullPath(Path.Combine(EngineDirectory, ".."));
		System.Console.WriteLine("RshipNDI: Engine root detected at: " + EngineRoot);

		// Try multiple potential plugin locations
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
			// Alternative - Compositing category (sometimes VirtualProduction plugins are here)
			Path.Combine(EngineRoot, "Plugins", "Compositing", "CineCameraSceneCapture", "Source", "CineCameraSceneCapture", "Public"),
		};

		bool bFoundIncludePath = false;
		foreach (string IncludePath in PotentialPluginPaths)
		{
			if (Directory.Exists(IncludePath))
			{
				PublicIncludePaths.Add(IncludePath);
				System.Console.WriteLine("RshipNDI: Added CineCameraSceneCapture include path: " + IncludePath);
				bFoundIncludePath = true;
				break;
			}
		}

		if (!bFoundIncludePath)
		{
			System.Console.WriteLine("RshipNDI: WARNING - CineCameraSceneCapture include path not found");
			System.Console.WriteLine("RshipNDI:   The CineCameraSceneCapture plugin may not expose public headers.");
			System.Console.WriteLine("RshipNDI:   Build may fail with 'CineCaptureComponent2D.h' not found.");
			System.Console.WriteLine("RshipNDI:   Searched paths:");
			foreach (string IncludePath in PotentialPluginPaths)
			{
				System.Console.WriteLine("RshipNDI:     - " + IncludePath);
			}
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
				System.Console.WriteLine("RshipNDI: cargo not found, skipping auto-build");
				System.Console.WriteLine("RshipNDI: Install Rust from https://rustup.rs or add cargo to PATH");
				return;
			}

			System.Console.WriteLine("RshipNDI: Found cargo at: " + cargoPath);
			System.Console.WriteLine("RshipNDI: Building Rust library (this may take a minute on first build)...");

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
						System.Console.WriteLine("RshipNDI: [cargo] " + e.Data);
				};
				buildProcess.ErrorDataReceived += (sender, e) => {
					if (!string.IsNullOrEmpty(e.Data))
						System.Console.WriteLine("RshipNDI: [cargo] " + e.Data);
				};

				buildProcess.BeginOutputReadLine();
				buildProcess.BeginErrorReadLine();

				// Wait up to 5 minutes for build
				bool finished = buildProcess.WaitForExit(300000);

				if (!finished)
				{
					System.Console.WriteLine("RshipNDI: Rust build timed out after 5 minutes");
					buildProcess.Kill();
				}
				else if (buildProcess.ExitCode == 0)
				{
					System.Console.WriteLine("RshipNDI: Rust library built successfully!");
				}
				else
				{
					System.Console.WriteLine("RshipNDI: Rust build failed with exit code " + buildProcess.ExitCode);
				}
			}
		}
		catch (System.Exception ex)
		{
			System.Console.WriteLine("RshipNDI: Auto-build failed: " + ex.Message);
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
					return cargoName; // Found in PATH
				}
			}
		}
		catch { }

		// Check common installation locations
		string userProfile = System.Environment.GetFolderPath(System.Environment.SpecialFolder.UserProfile);
		string[] possiblePaths = new string[]
		{
			// Windows rustup default
			Path.Combine(userProfile, ".cargo", "bin", "cargo.exe"),
			// Windows alternative
			Path.Combine(System.Environment.GetFolderPath(System.Environment.SpecialFolder.LocalApplicationData), "cargo", "bin", "cargo.exe"),
			// macOS/Linux rustup default
			Path.Combine(userProfile, ".cargo", "bin", "cargo"),
			// Homebrew on macOS
			"/opt/homebrew/bin/cargo",
			"/usr/local/bin/cargo",
			// Linux system install
			"/usr/bin/cargo",
		};

		foreach (string path in possiblePaths)
		{
			if (File.Exists(path))
			{
				return path;
			}
		}

		return null;
	}
}
