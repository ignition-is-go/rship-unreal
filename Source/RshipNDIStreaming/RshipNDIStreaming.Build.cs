// Copyright Lucid. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using System.Diagnostics;

public class RshipNDIStreaming : ModuleRules
{
	// Auto-detected based on presence of built Rust library
	private bool bHasRustNDISender = false;

	// Set to true to automatically build Rust library if missing
	private bool bAutoBuildRust = true;

	public RshipNDIStreaming(ReadOnlyTargetRules Target) : base(Target)
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
			System.Console.WriteLine("RshipNDIStreaming: Rust library not found, attempting auto-build...");
			TryBuildRustLibrary(RustLibPath);
		}

		if (File.Exists(RustLibFile))
		{
			bHasRustNDISender = true;
			System.Console.WriteLine("RshipNDIStreaming: Rust NDI sender library found at " + RustLibFile);
		}
		else
		{
			System.Console.WriteLine("RshipNDIStreaming: Rust NDI sender library NOT found.");
			System.Console.WriteLine("RshipNDIStreaming: To enable NDI streaming, build with:");
			System.Console.WriteLine("RshipNDIStreaming:   cd " + RustLibPath);
			System.Console.WriteLine("RshipNDIStreaming:   cargo build --release");
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
				System.Console.WriteLine("RshipNDIStreaming: cargo not found, skipping auto-build");
				System.Console.WriteLine("RshipNDIStreaming: Install Rust from https://rustup.rs or add cargo to PATH");
				return;
			}

			System.Console.WriteLine("RshipNDIStreaming: Found cargo at: " + cargoPath);
			System.Console.WriteLine("RshipNDIStreaming: Building Rust library (this may take a minute on first build)...");

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
						System.Console.WriteLine("RshipNDIStreaming: [cargo] " + e.Data);
				};
				buildProcess.ErrorDataReceived += (sender, e) => {
					if (!string.IsNullOrEmpty(e.Data))
						System.Console.WriteLine("RshipNDIStreaming: [cargo] " + e.Data);
				};

				buildProcess.BeginOutputReadLine();
				buildProcess.BeginErrorReadLine();

				// Wait up to 5 minutes for build
				bool finished = buildProcess.WaitForExit(300000);

				if (!finished)
				{
					System.Console.WriteLine("RshipNDIStreaming: Rust build timed out after 5 minutes");
					buildProcess.Kill();
				}
				else if (buildProcess.ExitCode == 0)
				{
					System.Console.WriteLine("RshipNDIStreaming: Rust library built successfully!");
				}
				else
				{
					System.Console.WriteLine("RshipNDIStreaming: Rust build failed with exit code " + buildProcess.ExitCode);
				}
			}
		}
		catch (System.Exception ex)
		{
			System.Console.WriteLine("RshipNDIStreaming: Auto-build failed: " + ex.Message);
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
