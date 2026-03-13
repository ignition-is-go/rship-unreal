// Copyright Rocketship. All Rights Reserved.

#include "RshipStatusPanelCommands.h"

#define LOCTEXT_NAMESPACE "FRshipStatusPanelCommands"

void FRshipStatusPanelCommands::RegisterCommands()
{
    UI_COMMAND(OpenStatusPanel, "Open Rocketship Panel", "Opens the Rocketship Status Panel", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
