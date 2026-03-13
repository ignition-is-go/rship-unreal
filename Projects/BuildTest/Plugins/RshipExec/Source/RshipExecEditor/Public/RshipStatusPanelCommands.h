// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "RshipStatusPanelStyle.h"

class FRshipStatusPanelCommands : public TCommands<FRshipStatusPanelCommands>
{
public:
    FRshipStatusPanelCommands()
        : TCommands<FRshipStatusPanelCommands>(
            TEXT("RshipStatusPanel"),
            NSLOCTEXT("Contexts", "RshipStatusPanel", "Rocketship Status Panel"),
            NAME_None,
            FRshipStatusPanelStyle::GetStyleSetName())
    {
    }

    virtual void RegisterCommands() override;

public:
    TSharedPtr<FUICommandInfo> OpenStatusPanel;
};
