// Copyright Rocketship. All Rights Reserved.

#include "RshipStatusPanelStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/FileManager.h"

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BOX_BRUSH(RelativePath, ...) FSlateBoxBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BORDER_BRUSH(RelativePath, ...) FSlateBorderBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)

TSharedPtr<FSlateStyleSet> FRshipStatusPanelStyle::StyleInstance = nullptr;

void FRshipStatusPanelStyle::Initialize()
{
    if (!StyleInstance.IsValid())
    {
        StyleInstance = Create();
        FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
    }
}

void FRshipStatusPanelStyle::Shutdown()
{
    FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
    ensure(StyleInstance.IsUnique());
    StyleInstance.Reset();
}

void FRshipStatusPanelStyle::ReloadTextures()
{
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
    }
}

const ISlateStyle& FRshipStatusPanelStyle::Get()
{
    return *StyleInstance;
}

FName FRshipStatusPanelStyle::GetStyleSetName()
{
    static FName StyleSetName(TEXT("RshipStatusPanelStyle"));
    return StyleSetName;
}

TSharedRef<FSlateStyleSet> FRshipStatusPanelStyle::Create()
{
    TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet("RshipStatusPanelStyle"));
    Style->SetContentRoot(IPluginManager::Get().FindPlugin("RshipExec")->GetBaseDir() / TEXT("Resources"));

    const FVector2D Icon16x16(16.0f, 16.0f);
    const FVector2D Icon40x40(40.0f, 40.0f);

    const FString IconPath = Style->RootToContentDir(TEXT("RshipIcon"), TEXT(".png"));
    const FString ToolbarIconPath = Style->RootToContentDir(TEXT("RshipToolbarIcon"), TEXT(".png"));
    const FString ToolbarIconGreenPath = Style->RootToContentDir(TEXT("RshipToolbarIconGreen"), TEXT(".png"));
    const FString ToolbarIconRedPath = Style->RootToContentDir(TEXT("RshipToolbarIconRed"), TEXT(".png"));
    if (IFileManager::Get().FileExists(*IconPath))
    {
        Style->Set("Rship.StatusPanel.TabIcon", new IMAGE_BRUSH(TEXT("RshipIcon"), Icon16x16));

        if (IFileManager::Get().FileExists(*ToolbarIconGreenPath))
        {
            Style->Set("Rship.StatusPanel.ToolbarIcon.Connected", new IMAGE_BRUSH(TEXT("RshipToolbarIconGreen"), Icon40x40));
        }
        else
        {
            Style->Set("Rship.StatusPanel.ToolbarIcon.Connected", new IMAGE_BRUSH(TEXT("RshipIcon"), Icon40x40));
        }

        if (IFileManager::Get().FileExists(*ToolbarIconRedPath))
        {
            Style->Set("Rship.StatusPanel.ToolbarIcon.Disconnected", new IMAGE_BRUSH(TEXT("RshipToolbarIconRed"), Icon40x40));
        }
        else if (IFileManager::Get().FileExists(*ToolbarIconPath))
        {
            Style->Set("Rship.StatusPanel.ToolbarIcon.Disconnected", new IMAGE_BRUSH(TEXT("RshipToolbarIcon"), Icon40x40));
        }
        else
        {
            Style->Set("Rship.StatusPanel.ToolbarIcon.Disconnected", new IMAGE_BRUSH(TEXT("RshipIcon"), Icon40x40));
        }

        Style->Set("Rship.StatusPanel.ToolbarIcon", new IMAGE_BRUSH(TEXT("RshipIcon"), Icon40x40));
    }
    else
    {
        // Fallback while icon file is missing.
        Style->Set("Rship.StatusPanel.TabIcon", new FSlateRoundedBoxBrush(FLinearColor(0.1f, 0.6f, 0.9f, 1.0f), 4.0f, FLinearColor::Transparent, 0.0f, Icon16x16));
        Style->Set("Rship.StatusPanel.ToolbarIcon", new FSlateRoundedBoxBrush(FLinearColor(0.1f, 0.6f, 0.9f, 1.0f), 4.0f, FLinearColor::Transparent, 0.0f, Icon40x40));
        Style->Set("Rship.StatusPanel.ToolbarIcon.Connected", new FSlateRoundedBoxBrush(FLinearColor(0.1f, 0.6f, 0.9f, 1.0f), 4.0f, FLinearColor::Transparent, 0.0f, Icon40x40));
        Style->Set("Rship.StatusPanel.ToolbarIcon.Disconnected", new FSlateRoundedBoxBrush(FLinearColor(0.1f, 0.6f, 0.9f, 1.0f), 4.0f, FLinearColor::Transparent, 0.0f, Icon40x40));
    }

    // Status indicator brushes
    Style->Set("Rship.Status.Connected", new FSlateRoundedBoxBrush(GetConnectedColor(), 6.0f, FLinearColor::Transparent, 0.0f, FVector2D(12.0f, 12.0f)));
    Style->Set("Rship.Status.Disconnected", new FSlateRoundedBoxBrush(GetDisconnectedColor(), 6.0f, FLinearColor::Transparent, 0.0f, FVector2D(12.0f, 12.0f)));
    Style->Set("Rship.Status.Connecting", new FSlateRoundedBoxBrush(GetConnectingColor(), 6.0f, FLinearColor::Transparent, 0.0f, FVector2D(12.0f, 12.0f)));
    Style->Set("Rship.Status.BackingOff", new FSlateRoundedBoxBrush(GetBackingOffColor(), 6.0f, FLinearColor::Transparent, 0.0f, FVector2D(12.0f, 12.0f)));

    return Style;
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
