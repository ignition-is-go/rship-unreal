// Copyright Rocketship. All Rights Reserved.
// SMPTE 2110 / IPMX / PTP Integration Module

using UnrealBuildTool;
using System;
using System.IO;

public class Rship2110 : ModuleRules
{
    // Rivermax SDK paths - update these to match your installation
    private string RivermaxSDKPath = "";
    private bool bRivermaxAvailable = false;

    public Rship2110(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        PrecompileForTargets = PrecompileTargetsType.Any;

        // Enable C++ exceptions for third-party SDK integration
        bEnableExceptions = true;

        // Try to find Rivermax SDK
        DetectRivermaxSDK();

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

        // Editor-only dependencies for debug UI
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "UnrealEd",
                    "PropertyEditor",
                    "EditorStyle",
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
        if (bRivermaxAvailable)
        {
            PublicDefinitions.Add("RSHIP_RIVERMAX_AVAILABLE=1");
            PublicDefinitions.Add("RSHIP_PTP_AVAILABLE=1");
            System.Console.WriteLine("Rship2110: Rivermax SDK found, enabling full 2110 support");
        }
        else
        {
            PublicDefinitions.Add("RSHIP_RIVERMAX_AVAILABLE=0");
            PublicDefinitions.Add("RSHIP_PTP_AVAILABLE=1");  // PTP can work without Rivermax
            System.Console.WriteLine("Rship2110: Rivermax SDK not found, using stub implementations");
        }

        // IPMX/NMOS support (always available - uses HTTP REST)
        PublicDefinitions.Add("RSHIP_IPMX_AVAILABLE=1");
    }

    private void DetectRivermaxSDK()
    {
        // Check environment variable first
        string envPath = Environment.GetEnvironmentVariable("RIVERMAX_SDK_PATH");
        if (!string.IsNullOrEmpty(envPath) && Directory.Exists(envPath))
        {
            RivermaxSDKPath = envPath;
            bRivermaxAvailable = true;
            return;
        }

        // Check common installation paths on Windows
        string[] commonPaths = new string[]
        {
            "C:\\Program Files\\Mellanox\\Rivermax",
            "C:\\Program Files\\NVIDIA\\Rivermax",
            Path.Combine(ModuleDirectory, "ThirdParty", "Rivermax"),
        };

        foreach (string path in commonPaths)
        {
            if (Directory.Exists(path))
            {
                string includeDir = Path.Combine(path, "include");
                string libDir = Path.Combine(path, "lib");
                if (Directory.Exists(includeDir) && Directory.Exists(libDir))
                {
                    RivermaxSDKPath = path;
                    bRivermaxAvailable = true;
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

        if (bRivermaxAvailable)
        {
            // Rivermax SDK includes
            PublicIncludePaths.Add(Path.Combine(RivermaxSDKPath, "include"));

            // Rivermax libraries
            string libPath = Path.Combine(RivermaxSDKPath, "lib", "x64");
            PublicAdditionalLibraries.Add(Path.Combine(libPath, "rivermax.lib"));

            // Runtime DLLs (copy to output)
            string dllPath = Path.Combine(RivermaxSDKPath, "bin", "x64");
            if (Directory.Exists(dllPath))
            {
                RuntimeDependencies.Add(Path.Combine(dllPath, "rivermax.dll"));
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
            PublicAdditionalLibraries.Add(Path.Combine(libPath, "librivermax.so"));

            PublicDefinitions.Add("RSHIP_GPUDIRECT_AVAILABLE=1");
        }
        else
        {
            PublicDefinitions.Add("RSHIP_GPUDIRECT_AVAILABLE=0");
        }
    }
}
