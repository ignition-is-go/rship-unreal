// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SCheckBox.h"

class URshipSubsystem;
class SVerticalBox;

/**
 * Main Rocketship Status Panel widget.
 * Shows connection status, server address, and diagnostics.
 */
class SRshipStatusPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SRshipStatusPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual ~SRshipStatusPanel();

    // SWidget interface
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
    // UI update helpers
    void UpdateConnectionStatus();
    void UpdateDiagnostics();

    // Get the subsystem
    URshipSubsystem* GetSubsystem() const;

    // Button callbacks
    FReply OnReconnectClicked();
    FReply OnSettingsClicked();

    // Server address editing
    void OnServerAddressCommitted(const FText& NewText, ETextCommit::Type CommitType);
    void OnServerPortCommitted(const FText& NewText, ETextCommit::Type CommitType);

    // Global remote communication toggle
    ECheckBoxState GetRemoteToggleState() const;
    void OnRemoteToggleChanged(ECheckBoxState NewState);
    EVisibility GetRemoteOffBannerVisibility() const;
    bool IsRemoteControlsEnabled() const;

    // Build UI sections
    TSharedRef<SWidget> BuildConnectionSection();
    TSharedRef<SWidget> BuildDiagnosticsSection();

    // Cached UI elements for updates
    TSharedPtr<STextBlock> ConnectionStatusText;
    TSharedPtr<SImage> StatusIndicator;
    TSharedPtr<SEditableTextBox> ServerAddressBox;
    TSharedPtr<SEditableTextBox> ServerPortBox;
    TSharedPtr<class SCheckBox> RemoteToggleCheckBox;

    // Diagnostics text blocks
    TSharedPtr<STextBlock> QueueLengthText;
    TSharedPtr<STextBlock> MessageRateText;
    TSharedPtr<STextBlock> ByteRateText;
    TSharedPtr<STextBlock> DroppedText;
    TSharedPtr<STextBlock> BackoffText;
    TSharedPtr<STextBlock> SyncStatusText;
    TSharedPtr<STextBlock> TargetSyncText;
    TSharedPtr<STextBlock> ActionSyncText;
    TSharedPtr<STextBlock> EmitterSyncText;
    TSharedPtr<STextBlock> StatusSyncText;

    // Refresh timer
    float RefreshTimer = 0.0f;
    static constexpr float RefreshInterval = 0.5f;  // Update every 0.5 seconds
};
