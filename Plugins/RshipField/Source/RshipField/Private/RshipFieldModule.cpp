#include "RshipFieldModule.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

void FRshipFieldModule::StartupModule()
{
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("RshipField"));
    if (!Plugin.IsValid())
    {
        return;
    }

    const FString ShaderDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"));
    AddShaderSourceDirectoryMapping(TEXT("/Plugin/RshipField"), ShaderDir);
}

void FRshipFieldModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FRshipFieldModule, RshipField)
