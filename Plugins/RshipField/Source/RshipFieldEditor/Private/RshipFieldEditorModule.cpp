#include "RshipFieldEditorModule.h"

#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "RshipFieldEditor"

namespace
{
const FName RshipFieldStudioTabName(TEXT("RshipFieldStudio"));
}

void FRshipFieldEditorModule::StartupModule()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        RshipFieldStudioTabName,
        FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
        {
            return SNew(SDockTab)
                .TabRole(ETabRole::NomadTab)
                [
                    SNew(SBorder)
                    .Padding(12.0f)
                    [
                        SNew(SScrollBox)
                        + SScrollBox::Slot()
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("FieldStudioTitle", "Field Studio"))
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
                            [ SNew(STextBlock).Text(LOCTEXT("FieldStudioGlobal", "Global")) ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
                            [ SNew(STextBlock).Text(LOCTEXT("FieldStudioLayers", "Layers")) ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
                            [ SNew(STextBlock).Text(LOCTEXT("FieldStudioEffectors", "Effectors")) ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
                            [ SNew(STextBlock).Text(LOCTEXT("FieldStudioSplines", "Splines")) ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
                            [ SNew(STextBlock).Text(LOCTEXT("FieldStudioTargets", "Targets")) ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
                            [ SNew(STextBlock).Text(LOCTEXT("FieldStudioDebug", "Debug")) ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
                            [ SNew(STextBlock).Text(LOCTEXT("FieldStudioPerf", "Perf")) ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("FieldStudioNote", "Runtime controls are available through RS_ actions on URshipFieldComponent."))
                            ]
                        ]
                    ]
                ];
        }))
        .SetDisplayName(LOCTEXT("FieldStudioTab", "Field Studio"))
        .SetMenuType(ETabSpawnerMenuType::Hidden);

    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FRshipFieldEditorModule::RegisterMenus));
}

void FRshipFieldEditorModule::ShutdownModule()
{
    UnregisterMenus();

    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(RshipFieldStudioTabName);
}

void FRshipFieldEditorModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
    FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");

    Section.AddMenuEntry(
        "RshipFieldOpenStudio",
        LOCTEXT("FieldStudioMenuLabel", "Field Studio"),
        LOCTEXT("FieldStudioMenuTooltip", "Open the Rship Field authoring and debug panel."),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateRaw(this, &FRshipFieldEditorModule::OpenFieldStudioTab)));
}

void FRshipFieldEditorModule::UnregisterMenus()
{
    if (UToolMenus::TryGet())
    {
        UToolMenus::UnRegisterStartupCallback(this);
        UToolMenus::UnregisterOwner(this);
    }
}

void FRshipFieldEditorModule::OpenFieldStudioTab()
{
    FGlobalTabmanager::Get()->TryInvokeTab(RshipFieldStudioTabName);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRshipFieldEditorModule, RshipFieldEditor)
