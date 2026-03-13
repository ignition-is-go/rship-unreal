#include "PulseRDGDeformModule.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

void FPulseRDGDeformModule::StartupModule()
{
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("PulseRDGDeform"));
    if (!Plugin.IsValid())
    {
        return;
    }

    const FString ShaderDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"));
    AddShaderSourceDirectoryMapping(TEXT("/Plugin/PulseRDGDeform"), ShaderDir);
}

void FPulseRDGDeformModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FPulseRDGDeformModule, PulseRDGDeform)
