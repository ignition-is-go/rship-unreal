// Copyright Rocketship. All Rights Reserved.

#include "Rship2110Settings.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Interfaces/IPluginManager.h"

#if WITH_EDITOR
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#endif

// Helper to get the plugin's ThirdParty/Rivermax directory
static FString GetRivermaxThirdPartyPath()
{
    // Get the module directory for Rship2110
    FString ModulePath = FPaths::Combine(
        FPaths::ProjectPluginsDir(),
        TEXT("RshipExec/Source/Rship2110/ThirdParty/Rivermax")
    );

    // If not found in project plugins, check engine plugins
    if (!FPaths::DirectoryExists(ModulePath))
    {
        ModulePath = FPaths::Combine(
            FPaths::EnginePluginsDir(),
            TEXT("RshipExec/Source/Rship2110/ThirdParty/Rivermax")
        );
    }

    // Fallback to relative path from module
    if (!FPaths::DirectoryExists(ModulePath))
    {
        ModulePath = FPaths::Combine(
            FPaths::ProjectDir(),
            TEXT("Plugins/RshipExec/Source/Rship2110/ThirdParty/Rivermax")
        );
    }

    return ModulePath;
}

URship2110Settings::URship2110Settings()
{
    // Default video format: 1080p60 YCbCr 4:2:2 10-bit
    DefaultVideoFormat.Width = 1920;
    DefaultVideoFormat.Height = 1080;
    DefaultVideoFormat.FrameRateNumerator = 60;
    DefaultVideoFormat.FrameRateDenominator = 1;
    DefaultVideoFormat.ColorFormat = ERship2110ColorFormat::YCbCr_422;
    DefaultVideoFormat.BitDepth = ERship2110BitDepth::Bits_10;
    DefaultVideoFormat.bInterlaced = false;

    // Default transport: multicast on 239.0.0.1:5004
    DefaultTransportParams.DestinationIP = TEXT("239.0.0.1");
    DefaultTransportParams.DestinationPort = 5004;
    DefaultTransportParams.SourcePort = 5004;
    DefaultTransportParams.PayloadType = 96;
    DefaultTransportParams.DSCP = 46;  // EF (Expedited Forwarding)
    DefaultTransportParams.TTL = 64;

    // Initialize license status
    RefreshLicenseStatus();
}

URship2110Settings* URship2110Settings::Get()
{
    return GetMutableDefault<URship2110Settings>();
}

void URship2110Settings::ImportLicenseFile()
{
#if WITH_EDITOR
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (!DesktopPlatform)
    {
        LicenseStatus = TEXT("Error: Desktop platform not available");
        return;
    }

    // Get parent window for dialog
    void* ParentWindowHandle = nullptr;
    if (FSlateApplication::IsInitialized())
    {
        TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
        if (ParentWindow.IsValid())
        {
            ParentWindowHandle = ParentWindow->GetNativeWindow()->GetOSWindowHandle();
        }
    }

    // Open file dialog
    TArray<FString> OutFiles;
    const bool bOpened = DesktopPlatform->OpenFileDialog(
        ParentWindowHandle,
        TEXT("Select Rivermax License File"),
        FPaths::GetProjectFilePath(),
        TEXT(""),
        TEXT("License Files (*.lic)|*.lic|All Files (*.*)|*.*"),
        EFileDialogFlags::None,
        OutFiles
    );

    if (!bOpened || OutFiles.Num() == 0)
    {
        // User cancelled
        return;
    }

    const FString& SourceFile = OutFiles[0];

    // Validate it looks like a license file
    if (!FPaths::FileExists(SourceFile))
    {
        LicenseStatus = TEXT("Error: Selected file does not exist");
        return;
    }

    // Get destination path
    FString DestDir = GetRivermaxThirdPartyPath();

    // Create directory if it doesn't exist
    if (!FPaths::DirectoryExists(DestDir))
    {
        IFileManager::Get().MakeDirectory(*DestDir, true);
    }

    // Copy as rivermax.lic
    FString DestFile = FPaths::Combine(DestDir, TEXT("rivermax.lic"));

    // Check if destination already exists
    if (FPaths::FileExists(DestFile))
    {
        // Backup existing file
        FString BackupFile = FPaths::Combine(DestDir, TEXT("rivermax.lic.backup"));
        IFileManager::Get().Move(*BackupFile, *DestFile, true);
    }

    // Copy the license file
    if (IFileManager::Get().Copy(*DestFile, *SourceFile) == COPY_OK)
    {
        RivermaxLicensePath = DestFile;
        LicenseStatus = TEXT("License imported successfully. Rebuild to apply.");
        UE_LOG(LogTemp, Log, TEXT("Rship2110: License file imported to %s"), *DestFile);
    }
    else
    {
        LicenseStatus = FString::Printf(TEXT("Error: Failed to copy license file to %s"), *DestDir);
        UE_LOG(LogTemp, Error, TEXT("Rship2110: Failed to copy license file to %s"), *DestFile);
    }
#else
    LicenseStatus = TEXT("License import only available in editor");
#endif
}

void URship2110Settings::RefreshLicenseStatus()
{
    // License file names to search for
    TArray<FString> LicenseNames = {
        TEXT("rivermax.lic"),
        TEXT("license.lic"),
        TEXT("RIVERMAX.lic"),
        TEXT("LICENSE.lic")
    };

    // Paths to search
    TArray<FString> SearchPaths;

    // 1. ThirdParty/Rivermax in plugin
    SearchPaths.Add(GetRivermaxThirdPartyPath());

    // 2. Environment variable path
    FString EnvPath = FPlatformMisc::GetEnvironmentVariable(TEXT("RIVERMAX_LICENSE_PATH"));
    if (!EnvPath.IsEmpty())
    {
        SearchPaths.Add(EnvPath);
    }

    // 3. Common system paths
#if PLATFORM_WINDOWS
    SearchPaths.Add(TEXT("C:/Program Files/Mellanox/Rivermax"));
    SearchPaths.Add(TEXT("C:/Program Files/NVIDIA/Rivermax"));
    SearchPaths.Add(TEXT("C:/Rivermax"));
#else
    SearchPaths.Add(TEXT("/opt/mellanox/rivermax"));
    SearchPaths.Add(TEXT("/usr/local/rivermax"));
#endif

    // Search for license
    RivermaxLicensePath.Empty();

    for (const FString& SearchPath : SearchPaths)
    {
        if (SearchPath.IsEmpty() || !FPaths::DirectoryExists(SearchPath))
        {
            continue;
        }

        for (const FString& LicenseName : LicenseNames)
        {
            FString LicensePath = FPaths::Combine(SearchPath, LicenseName);
            if (FPaths::FileExists(LicensePath))
            {
                RivermaxLicensePath = LicensePath;
                break;
            }
        }

        if (!RivermaxLicensePath.IsEmpty())
        {
            break;
        }
    }

    // Update status message
#if RSHIP_RIVERMAX_AVAILABLE
    if (!RivermaxLicensePath.IsEmpty())
    {
        LicenseStatus = TEXT("Valid license found - Full 2110 support enabled");
    }
    else
    {
        LicenseStatus = TEXT("WARNING: No license file found - Streaming will fail at runtime");
    }
#else
    if (!RivermaxLicensePath.IsEmpty())
    {
        LicenseStatus = TEXT("License found - Rebuild with SDK to enable 2110 support");
    }
    else
    {
        LicenseStatus = TEXT("Rivermax SDK not available - Running in stub mode");
    }
#endif
}
