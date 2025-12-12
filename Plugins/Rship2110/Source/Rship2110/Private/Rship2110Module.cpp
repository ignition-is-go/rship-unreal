// Copyright Rocketship. All Rights Reserved.

#include "Rship2110.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformProcess.h"

#if WITH_EDITOR
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#endif

#include "Rship2110Settings.h"

DEFINE_LOG_CATEGORY(LogRship2110);

// Runtime check if rivermax.dll is actually available (for delay-loaded builds)
static bool CheckRivermaxDLLAvailable()
{
#if PLATFORM_WINDOWS && RSHIP_RIVERMAX_AVAILABLE
    // Try to load the delay-loaded DLL explicitly to check availability
    void* DllHandle = FPlatformProcess::GetDllHandle(TEXT("rivermax.dll"));
    if (DllHandle)
    {
        // DLL is available - don't free it since we need it
        return true;
    }
    return false;
#else
    return false;
#endif
}

void FRship2110Module::StartupModule()
{
    UE_LOG(LogRship2110, Log, TEXT("Rship2110 module starting up"));

    // Check feature availability based on compile-time defines AND runtime DLL availability
#if RSHIP_RIVERMAX_AVAILABLE
    // Compiled with Rivermax support - now check if DLL is actually present
    if (CheckRivermaxDLLAvailable())
    {
        bRivermaxAvailable = true;
        UE_LOG(LogRship2110, Log, TEXT("Rivermax SDK: Available (DLL loaded)"));
    }
    else
    {
        bRivermaxAvailable = false;
        UE_LOG(LogRship2110, Warning, TEXT("Rivermax SDK: Compiled with support but rivermax.dll not found - features disabled"));
        UE_LOG(LogRship2110, Warning, TEXT("Ensure rivermax.dll is in plugin Binaries folder or system PATH"));
    }
#else
    bRivermaxAvailable = false;
    UE_LOG(LogRship2110, Log, TEXT("Rivermax SDK: Not available (stub mode)"));
#endif

#if RSHIP_PTP_AVAILABLE
    bPTPAvailable = true;
    UE_LOG(LogRship2110, Log, TEXT("PTP Support: Available"));
#else
    bPTPAvailable = false;
    UE_LOG(LogRship2110, Log, TEXT("PTP Support: Not available"));
#endif

#if RSHIP_IPMX_AVAILABLE
    bIPMXAvailable = true;
    UE_LOG(LogRship2110, Log, TEXT("IPMX Support: Available"));
#else
    bIPMXAvailable = false;
    UE_LOG(LogRship2110, Log, TEXT("IPMX Support: Not available"));
#endif

    // Register settings with editor
#if WITH_EDITOR
    if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
    {
        SettingsModule->RegisterSettings(
            "Project",
            "Plugins",
            "Rship2110",
            NSLOCTEXT("Rship2110", "SettingsName", "Rship 2110 Settings"),
            NSLOCTEXT("Rship2110", "SettingsDescription", "Configure SMPTE 2110, PTP, and IPMX settings"),
            GetMutableDefault<URship2110Settings>()
        );
    }
#endif

    UE_LOG(LogRship2110, Log, TEXT("Rship2110 module startup complete"));
}

void FRship2110Module::ShutdownModule()
{
    UE_LOG(LogRship2110, Log, TEXT("Rship2110 module shutting down"));

#if WITH_EDITOR
    if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
    {
        SettingsModule->UnregisterSettings("Project", "Plugins", "Rship2110");
    }
#endif

    UE_LOG(LogRship2110, Log, TEXT("Rship2110 module shutdown complete"));
}

FRship2110Module& FRship2110Module::Get()
{
    return FModuleManager::LoadModuleChecked<FRship2110Module>("Rship2110");
}

bool FRship2110Module::IsAvailable()
{
    return FModuleManager::Get().IsModuleLoaded("Rship2110");
}

IMPLEMENT_MODULE(FRship2110Module, Rship2110)
