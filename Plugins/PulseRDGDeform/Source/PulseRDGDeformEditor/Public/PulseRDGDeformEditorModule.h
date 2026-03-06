#pragma once

#include "Modules/ModuleManager.h"

struct FToolMenuContext;

class FPulseRDGDeformEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();
    void ExecuteBakeFromLegacyCache(const FToolMenuContext& InContext) const;
    void ExecuteApplySelectedCacheToActors(const FToolMenuContext& InContext) const;
    void ExecuteAutoApplyGeneratedCaches(const FToolMenuContext& InContext) const;
};
