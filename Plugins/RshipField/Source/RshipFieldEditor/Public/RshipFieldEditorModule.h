#pragma once

#include "Modules/ModuleManager.h"

class FRshipFieldEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();
    void UnregisterMenus();
    void OpenFieldStudioTab();
};
