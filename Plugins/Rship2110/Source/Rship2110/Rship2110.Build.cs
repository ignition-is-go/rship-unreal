// Copyright Rocketship. All Rights Reserved.
// SMPTE 2110 / IPMX / PTP Integration Module
// Build version: 2024.12.12.5 - Graceful loading + SDK 1.8 support
//
// Rivermax SDK Setup:
// The SDK is bundled in ThirdParty/Rivermax. Users only need to:
// 1. Obtain a Rivermax license from NVIDIA
// 2. Place the license file (rivermax.lic) in ThirdParty/Rivermax/
//
// The plugin will automatically use the bundled SDK and copy the license
// to the output directory at build time.

using UnrealBuildTool;
using System;
using System.IO;

public class Rship2110 : ModuleRules
{
    // Rivermax SDK configuration
    private string RivermaxSDKPath = "";
    private string RivermaxLicensePath = "";
    private string RivermaxDLLPath = "";  // Path to DLLs (bin directory)
    private bool bRivermaxAvailable = false;  // SDK headers/libs found
    private bool bRivermaxDLLsFound = false;  // DLLs found (required for runtime)
    private bool bRivermaxLicenseFound = false;

    public Rship2110(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        PrecompileForTargets = PrecompileTargetsType.Any;

        // Enable C++ exceptions for third-party SDK integration
        bEnableExceptions = true;

        // Try to find Rivermax SDK (bundled first, then system)
        DetectRivermaxSDK();

        // On Windows, also check for DLLs - SDK without DLLs won't work
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            DetectRivermaxDLLs();
        }

        // Core dependencies
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "RenderCore",
                "RHI",
                "Renderer",
                "Json",
                "JsonUtilities",
                "HTTP",
                "Sockets",
                "Networking",
                "RshipExec",  // For integration with existing rship subsystem
                "RshipColorManagement",  // Color management for broadcast outputs
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore",
                "Settings",
                "Projects",  // For plugin version info
            }
        );

        // Editor-only dependencies for debug UI and license import
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "UnrealEd",
                    "PropertyEditor",
                    "EditorStyle",
                    "DesktopPlatform",  // For license file browser dialog
                }
            );
        }

        // Platform-specific configuration
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            ConfigureWindowsPlatform();
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            ConfigureLinuxPlatform();
        }
        else
        {
            // Other platforms - Rivermax not supported
            PublicDefinitions.Add("RSHIP_RIVERMAX_AVAILABLE=0");
            PublicDefinitions.Add("RSHIP_PTP_AVAILABLE=0");
            System.Console.WriteLine("Rship2110: Platform not supported for Rivermax/PTP");
        }

        // Define availability macros
        // We compile with Rivermax support if SDK is found, and use delay-loading
        // so the module can still load even if rivermax.dll isn't present at runtime
        bool bEnableRivermax = bRivermaxAvailable;
        if (Target.Platform == UnrealTargetPlatform.Win64 && bRivermaxAvailable && !bRivermaxDLLsFound)
        {
            System.Console.WriteLine("Rship2110: Rivermax SDK found but DLLs not detected at build time");
            System.Console.WriteLine("Rship2110: Module will use delay-loading - Rivermax features available if DLLs present at runtime");
        }

        if (bEnableRivermax)
        {
            PublicDefinitions.Add("RSHIP_RIVERMAX_AVAILABLE=1");
            PublicDefinitions.Add("RSHIP_PTP_AVAILABLE=1");

            if (bRivermaxLicenseFound)
            {
                System.Console.WriteLine("Rship2110: Rivermax SDK found with license - full 2110 support enabled");
            }
            else
            {
                System.Console.WriteLine("Rship2110: WARNING - Rivermax SDK found but NO LICENSE FILE detected!");
                System.Console.WriteLine("Rship2110: Place your rivermax.lic file in: " + Path.Combine(ModuleDirectory, "ThirdParty", "Rivermax"));
                System.Console.WriteLine("Rship2110: Streaming will fail at runtime without a valid license.");
            }
        }
        else
        {
            PublicDefinitions.Add("RSHIP_RIVERMAX_AVAILABLE=0");
            PublicDefinitions.Add("RSHIP_PTP_AVAILABLE=1");  // PTP can work without Rivermax
            if (!bRivermaxAvailable)
            {
                System.Console.WriteLine("Rship2110: Rivermax SDK not found, using stub implementations");
                System.Console.WriteLine("Rship2110: To enable full 2110 support, populate ThirdParty/Rivermax/ with SDK files");
            }
        }

        // IPMX/NMOS support (always available - uses HTTP REST)
        PublicDefinitions.Add("RSHIP_IPMX_AVAILABLE=1");
    }

    private void DetectRivermaxSDK()
    {
        // Priority 1: Bundled SDK in ThirdParty folder (preferred for distribution)
        string bundledPath = Path.Combine(ModuleDirectory, "ThirdParty", "Rivermax");
        if (CheckRivermaxPath(bundledPath))
        {
            RivermaxSDKPath = bundledPath;
            bRivermaxAvailable = true;
            System.Console.WriteLine("Rship2110: Using bundled Rivermax SDK from ThirdParty/Rivermax");
            DetectLicenseFile();
            return;
        }

        // Priority 2: Environment variable
        string envPath = Environment.GetEnvironmentVariable("RIVERMAX_SDK_PATH");
        if (!string.IsNullOrEmpty(envPath) && CheckRivermaxPath(envPath))
        {
            RivermaxSDKPath = envPath;
            bRivermaxAvailable = true;
            System.Console.WriteLine("Rship2110: Using Rivermax SDK from RIVERMAX_SDK_PATH: " + envPath);
            DetectLicenseFile();
            return;
        }

        // Priority 3: Common system installation paths
        string[] systemPaths = new string[]
        {
            "C:\\Program Files\\Mellanox\\Rivermax",
            "C:\\Program Files\\NVIDIA\\Rivermax",
            "C:\\Rivermax",
            "/opt/mellanox/rivermax",
            "/usr/local/rivermax",
        };

        foreach (string path in systemPaths)
        {
            if (CheckRivermaxPath(path))
            {
                RivermaxSDKPath = path;
                bRivermaxAvailable = true;
                System.Console.WriteLine("Rship2110: Using system Rivermax SDK from: " + path);
                DetectLicenseFile();
                return;
            }
        }
    }

    private bool CheckRivermaxPath(string basePath)
    {
        if (!Directory.Exists(basePath))
            return false;

        string includeDir = Path.Combine(basePath, "include");

        // Check for lib in either lib/ or lib/x64/
        string libDir = Path.Combine(basePath, "lib");
        string libX64Dir = Path.Combine(basePath, "lib", "x64");

        bool hasInclude = Directory.Exists(includeDir);
        bool hasLib = Directory.Exists(libDir) || Directory.Exists(libX64Dir);

        // Check for header file existence
        bool hasHeader = File.Exists(Path.Combine(includeDir, "rivermax_api.h")) ||
                         File.Exists(Path.Combine(includeDir, "rivermax.h"));

        if (!hasInclude || !hasLib || !hasHeader)
            return false;

        // Check SDK version - 1.3+ uses new API
        string deprecatedHeader = Path.Combine(includeDir, "rivermax_deprecated.h");
        if (File.Exists(deprecatedHeader))
        {
            System.Console.WriteLine("Rship2110: Detected Rivermax SDK 1.3+ (modern API)");
        }
        else
        {
            System.Console.WriteLine("Rship2110: Detected Rivermax SDK < 1.3 (legacy API)");
        }

        return true;
    }

    private void DetectRivermaxDLLs()
    {
        // Build list of paths to check for DLLs
        var dllSearchPaths = new System.Collections.Generic.List<string>();

        // 1. Check relative to SDK path first
        if (!string.IsNullOrEmpty(RivermaxSDKPath))
        {
            dllSearchPaths.Add(Path.Combine(RivermaxSDKPath, "bin", "x64"));
            dllSearchPaths.Add(Path.Combine(RivermaxSDKPath, "bin"));
            dllSearchPaths.Add(Path.Combine(RivermaxSDKPath, "lib", "x64"));
            dllSearchPaths.Add(Path.Combine(RivermaxSDKPath, "lib"));
        }

        // 2. Default NVIDIA Rivermax installation paths (Windows)
        // SDK 1.8+ uses different structure - check multiple variations
        dllSearchPaths.Add("C:\\Program Files\\NVIDIA\\Rivermax\\bin");
        dllSearchPaths.Add("C:\\Program Files\\NVIDIA\\Rivermax\\bin\\x64");
        dllSearchPaths.Add("C:\\Program Files\\NVIDIA\\Rivermax\\lib");
        dllSearchPaths.Add("C:\\Program Files\\NVIDIA\\Rivermax\\lib\\x64");
        // SDK 1.8 may install to versioned folders
        dllSearchPaths.Add("C:\\Program Files\\NVIDIA\\Rivermax SDK\\bin");
        dllSearchPaths.Add("C:\\Program Files\\NVIDIA\\Rivermax SDK\\lib");
        dllSearchPaths.Add("C:\\Program Files\\NVIDIA\\Rivermax SDK 1.8\\bin");
        dllSearchPaths.Add("C:\\Program Files\\NVIDIA\\Rivermax SDK 1.8\\lib");
        // Legacy Mellanox paths
        dllSearchPaths.Add("C:\\Program Files\\Mellanox\\Rivermax\\bin");
        dllSearchPaths.Add("C:\\Program Files\\Mellanox\\Rivermax\\bin\\x64");
        dllSearchPaths.Add("C:\\Program Files\\Mellanox\\Rivermax\\lib");
        dllSearchPaths.Add("C:\\Rivermax\\bin");
        dllSearchPaths.Add("C:\\Rivermax\\bin\\x64");
        dllSearchPaths.Add("C:\\Rivermax\\lib");

        // 3. Check NVIDIA driver installation (some versions bundle Rivermax)
        string programFiles = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles);
        dllSearchPaths.Add(Path.Combine(programFiles, "NVIDIA Corporation", "Rivermax", "bin"));
        dllSearchPaths.Add(Path.Combine(programFiles, "NVIDIA Corporation", "Rivermax", "bin", "x64"));

        // 4. Check NVIDIA GPU Computing Toolkit (CUDA-adjacent)
        string nvToolkit = Environment.GetEnvironmentVariable("CUDA_PATH");
        if (!string.IsNullOrEmpty(nvToolkit))
        {
            dllSearchPaths.Add(Path.Combine(Path.GetDirectoryName(nvToolkit), "Rivermax", "bin"));
        }

        foreach (string dllPath in dllSearchPaths)
        {
            if (string.IsNullOrEmpty(dllPath) || !Directory.Exists(dllPath))
                continue;

            string rivermaxDll = Path.Combine(dllPath, "rivermax.dll");
            if (File.Exists(rivermaxDll))
            {
                RivermaxDLLPath = dllPath;
                bRivermaxDLLsFound = true;
                System.Console.WriteLine("Rship2110: Found Rivermax DLLs in: " + dllPath);

                // List all DLLs found
                string[] expectedDlls = new string[] { "rivermax.dll", "dpcp.dll", "mlx5devx.dll" };
                foreach (string dll in expectedDlls)
                {
                    string dllFullPath = Path.Combine(dllPath, dll);
                    if (File.Exists(dllFullPath))
                    {
                        System.Console.WriteLine("  - " + dll + " (found)");
                    }
                    else
                    {
                        System.Console.WriteLine("  - " + dll + " (missing - may be optional)");
                    }
                }
                return;
            }
        }

        // DLLs not found in known locations - check system PATH
        // Note: DLLs in PATH will load at runtime but we can't copy them
        string pathEnv = Environment.GetEnvironmentVariable("PATH") ?? "";
        string[] pathDirs = pathEnv.Split(Path.PathSeparator);
        foreach (string dir in pathDirs)
        {
            if (string.IsNullOrEmpty(dir) || !Directory.Exists(dir))
                continue;

            string rivermaxDll = Path.Combine(dir, "rivermax.dll");
            if (File.Exists(rivermaxDll))
            {
                RivermaxDLLPath = dir;
                bRivermaxDLLsFound = true;
                System.Console.WriteLine("Rship2110: Found Rivermax DLLs in system PATH: " + dir);
                return;
            }
        }

        System.Console.WriteLine("Rship2110: WARNING - Rivermax SDK found but DLLs not found!");
        System.Console.WriteLine("Rship2110: Searched the following locations:");
        foreach (string path in dllSearchPaths)
        {
            if (!string.IsNullOrEmpty(path))
                System.Console.WriteLine("  - " + path);
        }
        System.Console.WriteLine("  - System PATH directories");
        System.Console.WriteLine("Rship2110: Please ensure rivermax.dll is accessible");
    }

    private void DetectLicenseFile()
    {
        // Look for license file in multiple locations
        string[] licenseNames = new string[]
        {
            "rivermax.lic",
            "license.lic",
            "RIVERMAX.lic",
            "LICENSE.lic",
        };

        string[] searchPaths = new string[]
        {
            RivermaxSDKPath,
            Path.Combine(ModuleDirectory, "ThirdParty", "Rivermax"),
            Environment.GetEnvironmentVariable("RIVERMAX_LICENSE_PATH") ?? "",
            Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData),
        };

        foreach (string searchPath in searchPaths)
        {
            if (string.IsNullOrEmpty(searchPath) || !Directory.Exists(searchPath))
                continue;

            foreach (string licenseName in licenseNames)
            {
                string licensePath = Path.Combine(searchPath, licenseName);
                if (File.Exists(licensePath))
                {
                    RivermaxLicensePath = licensePath;
                    bRivermaxLicenseFound = true;
                    System.Console.WriteLine("Rship2110: Found Rivermax license: " + licensePath);
                    return;
                }
            }
        }
    }

    private void ConfigureWindowsPlatform()
    {
        PublicDefinitions.Add("_WIN32");
        PublicDefinitions.Add("RSHIP_PLATFORM_WINDOWS=1");

        // Windows system libraries for networking and timing
        PublicSystemLibraries.AddRange(
            new string[]
            {
                "Ws2_32.lib",
                "Iphlpapi.lib",
                "Winmm.lib",  // For high-resolution timers
            }
        );

        // Configure Rivermax if SDK is available
        // Use delay-loading so module can load even without rivermax.dll present
        if (bRivermaxAvailable)
        {
            System.Console.WriteLine("Rship2110: Rivermax SDK path: " + RivermaxSDKPath);
            System.Console.WriteLine("Rship2110: Rivermax DLL path: " + RivermaxDLLPath);

            // Rivermax SDK includes
            PublicIncludePaths.Add(Path.Combine(RivermaxSDKPath, "include"));

            // Rivermax libraries - check both lib/ and lib/x64/
            string libPath = Path.Combine(RivermaxSDKPath, "lib", "x64");
            if (!Directory.Exists(libPath))
            {
                libPath = Path.Combine(RivermaxSDKPath, "lib");
            }

            string rivermaxLib = Path.Combine(libPath, "rivermax.lib");
            if (File.Exists(rivermaxLib))
            {
                PublicAdditionalLibraries.Add(rivermaxLib);
                System.Console.WriteLine("Rship2110: Linking against: " + rivermaxLib);

                // CRITICAL: Use delay-loading so module can load without rivermax.dll
                // The runtime code must check if DLL is available before calling any functions
                PublicDelayLoadDLLs.Add("rivermax.dll");
                System.Console.WriteLine("Rship2110: Using delay-load for rivermax.dll (graceful loading)");
            }

            // Copy DLLs to the plugin's Binaries folder for editor development (if found)
            // RuntimeDependencies only works for packaging - we need direct copy for editor
            if (bRivermaxDLLsFound && !string.IsNullOrEmpty(RivermaxDLLPath))
            {
                string pluginBinariesDir = Path.Combine(ModuleDirectory, "..", "..", "Binaries", "Win64");
                if (!Directory.Exists(pluginBinariesDir))
                {
                    Directory.CreateDirectory(pluginBinariesDir);
                    System.Console.WriteLine("Rship2110: Created Binaries directory: " + pluginBinariesDir);
                }

                string[] dllFiles = new string[] { "rivermax.dll", "dpcp.dll", "mlx5devx.dll" };
                foreach (string dllFile in dllFiles)
                {
                    string srcPath = Path.Combine(RivermaxDLLPath, dllFile);
                    string dstPath = Path.Combine(pluginBinariesDir, dllFile);

                    if (File.Exists(srcPath))
                    {
                        // Copy if destination doesn't exist or is older
                        bool needsCopy = !File.Exists(dstPath);
                        if (!needsCopy)
                        {
                            DateTime srcTime = File.GetLastWriteTime(srcPath);
                            DateTime dstTime = File.GetLastWriteTime(dstPath);
                            needsCopy = srcTime > dstTime;
                        }

                        if (needsCopy)
                        {
                            try
                            {
                                File.Copy(srcPath, dstPath, true);
                                System.Console.WriteLine("Rship2110: Copied " + dllFile + " to Binaries");
                            }
                            catch (Exception ex)
                            {
                                System.Console.WriteLine("Rship2110: WARNING - Failed to copy " + dllFile + ": " + ex.Message);
                            }
                        }
                        else
                        {
                            System.Console.WriteLine("Rship2110: " + dllFile + " already up to date");
                        }

                        // Also add to RuntimeDependencies for packaging
                        RuntimeDependencies.Add("$(BinaryOutputDir)/" + dllFile, srcPath);
                    }
                    else
                    {
                        // dpcp.dll and mlx5devx.dll may be optional driver components
                        if (dllFile == "rivermax.dll")
                        {
                            System.Console.WriteLine("Rship2110: WARNING - rivermax.dll not found at: " + srcPath);
                        }
                        else
                        {
                            System.Console.WriteLine("Rship2110: Optional DLL not found: " + dllFile);
                        }
                    }
                }
            }
            else
            {
                System.Console.WriteLine("Rship2110: DLLs not found at build time - will attempt to load from system PATH at runtime");
            }

            // Copy license file to output if found
            if (bRivermaxLicenseFound && !string.IsNullOrEmpty(RivermaxLicensePath))
            {
                string licenseFileName = Path.GetFileName(RivermaxLicensePath);
                RuntimeDependencies.Add("$(BinaryOutputDir)/" + licenseFileName, RivermaxLicensePath);
                System.Console.WriteLine("Rship2110: License file will be copied to output directory");
            }

            // GPU Direct dependencies
            PublicDefinitions.Add("RSHIP_GPUDIRECT_AVAILABLE=1");
        }
        else
        {
            PublicDefinitions.Add("RSHIP_GPUDIRECT_AVAILABLE=0");
        }
    }

    private void ConfigureLinuxPlatform()
    {
        PublicDefinitions.Add("__linux__");
        PublicDefinitions.Add("RSHIP_PLATFORM_LINUX=1");

        // Linux system libraries
        PublicSystemLibraries.AddRange(
            new string[]
            {
                "pthread",
                "rt",  // For clock_gettime
            }
        );

        if (bRivermaxAvailable)
        {
            // Rivermax SDK includes
            PublicIncludePaths.Add(Path.Combine(RivermaxSDKPath, "include"));

            // Rivermax libraries
            string libPath = Path.Combine(RivermaxSDKPath, "lib");
            string rivermaxSo = Path.Combine(libPath, "librivermax.so");
            if (File.Exists(rivermaxSo))
            {
                PublicAdditionalLibraries.Add(rivermaxSo);
            }

            // Copy .so files to output
            string[] soFiles = new string[] { "librivermax.so", "libdpcp.so" };
            foreach (string soFile in soFiles)
            {
                string soFullPath = Path.Combine(libPath, soFile);
                if (File.Exists(soFullPath))
                {
                    RuntimeDependencies.Add("$(BinaryOutputDir)/" + soFile, soFullPath);
                }
            }

            // Copy license file to output if found
            if (bRivermaxLicenseFound && !string.IsNullOrEmpty(RivermaxLicensePath))
            {
                string licenseFileName = Path.GetFileName(RivermaxLicensePath);
                RuntimeDependencies.Add("$(BinaryOutputDir)/" + licenseFileName, RivermaxLicensePath);
            }

            PublicDefinitions.Add("RSHIP_GPUDIRECT_AVAILABLE=1");
        }
        else
        {
            PublicDefinitions.Add("RSHIP_GPUDIRECT_AVAILABLE=0");
        }
    }
}
