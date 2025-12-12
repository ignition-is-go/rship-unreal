// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

class FRshipStatusPanelStyle
{
public:
    static void Initialize();
    static void Shutdown();
    static void ReloadTextures();
    static const ISlateStyle& Get();
    static FName GetStyleSetName();

    // Status colors
    static FLinearColor GetConnectedColor() { return FLinearColor(0.0f, 0.8f, 0.2f, 1.0f); }      // Green
    static FLinearColor GetDisconnectedColor() { return FLinearColor(0.8f, 0.1f, 0.1f, 1.0f); }   // Red
    static FLinearColor GetConnectingColor() { return FLinearColor(0.9f, 0.7f, 0.0f, 1.0f); }     // Yellow/Orange
    static FLinearColor GetBackingOffColor() { return FLinearColor(0.9f, 0.5f, 0.0f, 1.0f); }     // Orange

private:
    static TSharedRef<class FSlateStyleSet> Create();
    static TSharedPtr<class FSlateStyleSet> StyleInstance;
};
