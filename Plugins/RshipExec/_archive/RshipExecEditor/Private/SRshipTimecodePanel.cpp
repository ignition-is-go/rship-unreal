// Copyright Rocketship. All Rights Reserved.

#include "SRshipTimecodePanel.h"
#include "RshipStatusPanelStyle.h"
#include "RshipSubsystem.h"
#include "RshipTimecodeSync.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Views/SHeaderRow.h"
#include "EditorStyleSet.h"
#include "Engine/Engine.h"

#define LOCTEXT_NAMESPACE "SRshipTimecodePanel"

void SRshipTimecodePanel::Construct(const FArguments& InArgs)
{
    // Initialize source options
    SourceOptions.Add(MakeShareable(new FString(TEXT("Internal (UE Clock)"))));
    SourceOptions.Add(MakeShareable(new FString(TEXT("Rship Server"))));
    SourceOptions.Add(MakeShareable(new FString(TEXT("LTC Audio Input"))));
    SourceOptions.Add(MakeShareable(new FString(TEXT("MIDI Timecode"))));
    SourceOptions.Add(MakeShareable(new FString(TEXT("Art-Net Timecode"))));
    SourceOptions.Add(MakeShareable(new FString(TEXT("PTP/IEEE 1588"))));
    SourceOptions.Add(MakeShareable(new FString(TEXT("NTP Network Time"))));
    SourceOptions.Add(MakeShareable(new FString(TEXT("Manual/Triggered"))));

    // Initialize mode options
    ModeOptions.Add(MakeShareable(new FString(TEXT("Receive (Follow rship)"))));
    ModeOptions.Add(MakeShareable(new FString(TEXT("Publish (UE is master)"))));
    ModeOptions.Add(MakeShareable(new FString(TEXT("Bidirectional"))));

    ChildSlot
    [
        SNew(SScrollBox)
        + SScrollBox::Slot()
        .Padding(8.0f)
        [
            SNew(SVerticalBox)

            // Timecode Display Section
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
            [
                BuildTimecodeDisplaySection()
            ]

            // Separator
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 4.0f)
            [
                SNew(SSeparator)
            ]

            // Playback Controls
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 8.0f, 0.0f, 8.0f)
            [
                BuildPlaybackControlSection()
            ]

            // Separator
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 4.0f)
            [
                SNew(SSeparator)
            ]

            // Source Selection
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 8.0f, 0.0f, 8.0f)
            [
                BuildSourceSection()
            ]

            // Separator
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 4.0f)
            [
                SNew(SSeparator)
            ]

            // Cue Points
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .Padding(0.0f, 8.0f, 0.0f, 8.0f)
            [
                BuildCuePointsSection()
            ]

            // Separator
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 4.0f)
            [
                SNew(SSeparator)
            ]

            // Test Section
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 8.0f, 0.0f, 0.0f)
            [
                BuildTestSection()
            ]
        ]
    ];

    // Initial data load
    UpdateTimecodeDisplay();
    UpdateSourceStatus();
    RefreshCuePointList();
}

SRshipTimecodePanel::~SRshipTimecodePanel()
{
}

void SRshipTimecodePanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

    RefreshTimer += InDeltaTime;
    if (RefreshTimer >= RefreshInterval)
    {
        RefreshTimer = 0.0f;
        UpdateTimecodeDisplay();
        UpdateSourceStatus();
        // Cue points don't need high-frequency updates
        static int32 CueRefreshCounter = 0;
        if (++CueRefreshCounter >= 15) // Every ~0.5 seconds
        {
            CueRefreshCounter = 0;
            RefreshCuePointList();
        }
    }
}

URshipSubsystem* SRshipTimecodePanel::GetSubsystem() const
{
    if (GEngine)
    {
        return GEngine->GetEngineSubsystem<URshipSubsystem>();
    }
    return nullptr;
}

URshipTimecodeSync* SRshipTimecodePanel::GetTimecodeSync() const
{
    URshipSubsystem* Subsystem = GetSubsystem();
    if (Subsystem)
    {
        return Subsystem->GetTimecodeSync();
    }
    return nullptr;
}

TSharedRef<SWidget> SRshipTimecodePanel::BuildTimecodeDisplaySection()
{
    return SNew(SVerticalBox)

        // Header
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 8.0f)
        [
            SNew(SHorizontalBox)

            // Sync indicator
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SAssignNew(SyncIndicator, SImage)
                .Image(FRshipStatusPanelStyle::Get().GetBrush("Rship.Status.Disconnected"))
            ]

            // Title
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("TimecodeTitle", "Timecode"))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNullWidget::NullWidget
            ]

            // State text
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SAssignNew(StateText, STextBlock)
                .Text(LOCTEXT("StateStopped", "Stopped"))
            ]
        ]

        // Large timecode display
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Center)
        .Padding(0.0f, 8.0f)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
            .Padding(16.0f)
            [
                SAssignNew(TimecodeText, STextBlock)
                .Text(LOCTEXT("TimecodeDefault", "00:00:00:00"))
                .Font(FCoreStyle::GetDefaultFontStyle("Mono", 36))
                .ColorAndOpacity(FLinearColor::White)
            ]
        ]

        // Frame and time info row
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 8.0f)
        [
            SNew(SHorizontalBox)

            // Frame number
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("FrameLabel", "Frame"))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SAssignNew(FrameNumberText, STextBlock)
                    .Text(LOCTEXT("FrameDefault", "0"))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                ]
            ]

            // Elapsed time
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ElapsedLabel", "Elapsed"))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SAssignNew(ElapsedTimeText, STextBlock)
                    .Text(LOCTEXT("ElapsedDefault", "0.000s"))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                ]
            ]

            // Frame rate
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("FrameRateLabel", "Frame Rate"))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SAssignNew(FrameRateText, STextBlock)
                    .Text(LOCTEXT("FrameRateDefault", "30 fps"))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                ]
            ]

            // Sync offset
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("SyncOffsetLabel", "Sync Offset"))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SAssignNew(SyncOffsetText, STextBlock)
                    .Text(LOCTEXT("SyncOffsetDefault", "0.0 ms"))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                ]
            ]
        ];
}

TSharedRef<SWidget> SRshipTimecodePanel::BuildPlaybackControlSection()
{
    return SNew(SVerticalBox)

        // Header
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 8.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("PlaybackTitle", "Playback Control"))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
        ]

        // Transport controls
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Center)
        [
            SNew(SHorizontalBox)

            // Step backward
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("StepBackward", "|<"))
                .ToolTipText(LOCTEXT("StepBackwardTooltip", "Step backward one frame"))
                .OnClicked(this, &SRshipTimecodePanel::OnStepBackwardClicked)
            ]

            // Previous cue
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("PrevCue", "<<"))
                .ToolTipText(LOCTEXT("PrevCueTooltip", "Jump to previous cue point"))
                .OnClicked(this, &SRshipTimecodePanel::OnJumpToPrevCueClicked)
            ]

            // Stop
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("Stop", "Stop"))
                .ToolTipText(LOCTEXT("StopTooltip", "Stop and reset to start"))
                .OnClicked(this, &SRshipTimecodePanel::OnStopClicked)
            ]

            // Play
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("Play", "Play"))
                .ToolTipText(LOCTEXT("PlayTooltip", "Start playback"))
                .OnClicked(this, &SRshipTimecodePanel::OnPlayClicked)
            ]

            // Pause
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("Pause", "Pause"))
                .ToolTipText(LOCTEXT("PauseTooltip", "Pause playback"))
                .OnClicked(this, &SRshipTimecodePanel::OnPauseClicked)
            ]

            // Next cue
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("NextCue", ">>"))
                .ToolTipText(LOCTEXT("NextCueTooltip", "Jump to next cue point"))
                .OnClicked(this, &SRshipTimecodePanel::OnJumpToNextCueClicked)
            ]

            // Step forward
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("StepForward", ">|"))
                .ToolTipText(LOCTEXT("StepForwardTooltip", "Step forward one frame"))
                .OnClicked(this, &SRshipTimecodePanel::OnStepForwardClicked)
            ]
        ];
}

TSharedRef<SWidget> SRshipTimecodePanel::BuildSourceSection()
{
    return SNew(SVerticalBox)

        // Header
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 8.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("SourceTitle", "Timecode Source & Mode"))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
        ]

        // Source selector
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("SourceLabel", "Source:"))
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SAssignNew(SourceComboBox, SComboBox<TSharedPtr<FString>>)
                .OptionsSource(&SourceOptions)
                .OnSelectionChanged(this, &SRshipTimecodePanel::OnSourceChanged)
                .OnGenerateWidget(this, &SRshipTimecodePanel::GenerateSourceComboItem)
                .InitiallySelectedItem(SourceOptions[0])
                [
                    SNew(STextBlock)
                    .Text(this, &SRshipTimecodePanel::GetCurrentSourceText)
                ]
            ]
        ]

        // Mode selector (bidirectional)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 8.0f, 0.0f, 0.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ModeLabel", "Mode:"))
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SAssignNew(ModeComboBox, SComboBox<TSharedPtr<FString>>)
                .OptionsSource(&ModeOptions)
                .OnSelectionChanged(this, &SRshipTimecodePanel::OnModeChanged)
                .OnGenerateWidget(this, &SRshipTimecodePanel::GenerateModeComboItem)
                .InitiallySelectedItem(ModeOptions[0])
                [
                    SNew(STextBlock)
                    .Text(this, &SRshipTimecodePanel::GetCurrentModeText)
                ]
            ]
        ]

        // Mode status/info
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 4.0f, 0.0f, 0.0f)
        [
            SAssignNew(ModeStatusText, STextBlock)
            .Text(LOCTEXT("ModeStatusReceive", "UE follows timecode from rship server"))
            .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        ]

        // Sync status
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 8.0f, 0.0f, 0.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("SyncStatusLabel", "Sync Status:"))
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SAssignNew(SyncStatusText, STextBlock)
                .Text(LOCTEXT("SyncStatusDefault", "Not synchronized"))
            ]
        ];
}

TSharedRef<SWidget> SRshipTimecodePanel::BuildCuePointsSection()
{
    return SNew(SVerticalBox)

        // Header with buttons
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 8.0f)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("CuePointsTitle", "Cue Points"))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNullWidget::NullWidget
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(4.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("AddCue", "+"))
                .ToolTipText(LOCTEXT("AddCueTooltip", "Add cue point at current timecode"))
                .OnClicked(this, &SRshipTimecodePanel::OnAddCuePointClicked)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(4.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("RemoveCue", "-"))
                .ToolTipText(LOCTEXT("RemoveCueTooltip", "Remove selected cue point"))
                .OnClicked(this, &SRshipTimecodePanel::OnRemoveCuePointClicked)
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(4.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("ClearCues", "Clear"))
                .ToolTipText(LOCTEXT("ClearCuesTooltip", "Remove all cue points"))
                .OnClicked(this, &SRshipTimecodePanel::OnClearCuePointsClicked)
            ]
        ]

        // Cue point list
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .Padding(2.0f)
            [
                SAssignNew(CuePointListView, SListView<TSharedPtr<FRshipCuePointListItem>>)
                .ListItemsSource(&CuePointItems)
                .OnGenerateRow(this, &SRshipTimecodePanel::GenerateCuePointRow)
                .OnSelectionChanged(this, &SRshipTimecodePanel::OnCuePointSelectionChanged)
                .SelectionMode(ESelectionMode::Single)
                .HeaderRow
                (
                    SNew(SHeaderRow)
                    + SHeaderRow::Column("Name")
                    .DefaultLabel(LOCTEXT("CueNameHeader", "Name"))
                    .FillWidth(0.4f)
                    + SHeaderRow::Column("Timecode")
                    .DefaultLabel(LOCTEXT("CueTimecodeHeader", "Timecode"))
                    .FillWidth(0.3f)
                    + SHeaderRow::Column("Status")
                    .DefaultLabel(LOCTEXT("CueStatusHeader", "Status"))
                    .FillWidth(0.3f)
                )
            ]
        ];
}

TSharedRef<SWidget> SRshipTimecodePanel::BuildTestSection()
{
    return SNew(SVerticalBox)

        // Header
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 8.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("TestTitle", "Testing"))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
        ]

        // Test buttons
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(SButton)
                .Text(LOCTEXT("InjectTimecode", "Inject Test Timecode"))
                .ToolTipText(LOCTEXT("InjectTimecodeTooltip", "Simulate receiving a timecode pulse from rship (for testing without server)"))
                .OnClicked(this, &SRshipTimecodePanel::OnInjectTestTimecodeClicked)
            ]
        ];
}

// ============================================================================
// UPDATE METHODS
// ============================================================================

void SRshipTimecodePanel::UpdateTimecodeDisplay()
{
    URshipTimecodeSync* TimecodeSync = GetTimecodeSync();
    if (!TimecodeSync)
    {
        return;
    }

    FRshipTimecodeStatus Status = TimecodeSync->GetStatus();

    // Update timecode text
    if (TimecodeText.IsValid())
    {
        FString TCString = FString::Printf(TEXT("%02d:%02d:%02d:%02d"),
            Status.Timecode.Hours,
            Status.Timecode.Minutes,
            Status.Timecode.Seconds,
            Status.Timecode.Frames);
        TimecodeText->SetText(FText::FromString(TCString));
    }

    // Update frame number
    if (FrameNumberText.IsValid())
    {
        FrameNumberText->SetText(FText::FromString(FString::Printf(TEXT("%lld"), Status.TotalFrames)));
    }

    // Update elapsed time
    if (ElapsedTimeText.IsValid())
    {
        ElapsedTimeText->SetText(FText::FromString(FString::Printf(TEXT("%.3fs"), Status.ElapsedSeconds)));
    }

    // Update frame rate
    if (FrameRateText.IsValid())
    {
        float FPS = (float)Status.FrameRate.Numerator / (float)Status.FrameRate.Denominator;
        FrameRateText->SetText(FText::FromString(FString::Printf(TEXT("%.2f fps"), FPS)));
    }

    // Update sync offset
    if (SyncOffsetText.IsValid())
    {
        SyncOffsetText->SetText(FText::FromString(FString::Printf(TEXT("%.1f ms"), Status.SyncOffsetMs)));
    }

    // Update state text
    if (StateText.IsValid())
    {
        FText StateStr;
        switch (Status.State)
        {
        case ERshipTimecodeState::Stopped: StateStr = LOCTEXT("StateStopped", "Stopped"); break;
        case ERshipTimecodeState::Playing: StateStr = LOCTEXT("StatePlaying", "Playing"); break;
        case ERshipTimecodeState::Paused: StateStr = LOCTEXT("StatePaused", "Paused"); break;
        case ERshipTimecodeState::Seeking: StateStr = LOCTEXT("StateSeeking", "Seeking"); break;
        case ERshipTimecodeState::Syncing: StateStr = LOCTEXT("StateSyncing", "Syncing"); break;
        case ERshipTimecodeState::Lost: StateStr = LOCTEXT("StateLost", "Lost"); break;
        default: StateStr = LOCTEXT("StateUnknown", "Unknown"); break;
        }
        StateText->SetText(StateStr);
    }

    // Update sync indicator
    if (SyncIndicator.IsValid())
    {
        FName BrushName = Status.bIsSynchronized
            ? "Rship.Status.Connected"
            : "Rship.Status.Disconnected";
        SyncIndicator->SetImage(FRshipStatusPanelStyle::Get().GetBrush(BrushName));
    }
}

void SRshipTimecodePanel::UpdateSourceStatus()
{
    URshipTimecodeSync* TimecodeSync = GetTimecodeSync();
    if (!TimecodeSync)
    {
        return;
    }

    FRshipTimecodeStatus Status = TimecodeSync->GetStatus();
    CurrentSource = Status.Source;
    CurrentMode = Status.Mode;

    // Update sync status text
    if (SyncStatusText.IsValid())
    {
        if (Status.bIsSynchronized)
        {
            SyncStatusText->SetText(FText::Format(
                LOCTEXT("SyncStatusSynced", "Synchronized (offset: {0} ms)"),
                FText::AsNumber(Status.SyncOffsetMs, &FNumberFormattingOptions::DefaultWithGrouping())));
        }
        else
        {
            SyncStatusText->SetText(LOCTEXT("SyncStatusNotSynced", "Not synchronized"));
        }
    }

    // Update mode status text based on current mode
    if (ModeStatusText.IsValid())
    {
        switch (CurrentMode)
        {
        case ERshipTimecodeMode::Receive:
            ModeStatusText->SetText(LOCTEXT("ModeStatusReceive", "UE follows timecode from rship server"));
            break;
        case ERshipTimecodeMode::Publish:
            ModeStatusText->SetText(LOCTEXT("ModeStatusPublish", "UE publishes timecode as emitter (UE_Timecode/timecode)"));
            break;
        case ERshipTimecodeMode::Bidirectional:
            ModeStatusText->SetText(LOCTEXT("ModeStatusBidirectional", "UE follows rship AND publishes for monitoring"));
            break;
        }
    }
}

void SRshipTimecodePanel::RefreshCuePointList()
{
    URshipTimecodeSync* TimecodeSync = GetTimecodeSync();
    if (!TimecodeSync)
    {
        return;
    }

    TArray<FRshipCuePoint> CuePoints = TimecodeSync->GetCuePoints();

    CuePointItems.Empty();
    for (const FRshipCuePoint& CuePoint : CuePoints)
    {
        CuePointItems.Add(MakeShareable(new FRshipCuePointListItem(CuePoint)));
    }

    if (CuePointListView.IsValid())
    {
        CuePointListView->RequestListRefresh();
    }
}

// ============================================================================
// CALLBACKS
// ============================================================================

FReply SRshipTimecodePanel::OnPlayClicked()
{
    if (URshipTimecodeSync* TimecodeSync = GetTimecodeSync())
    {
        TimecodeSync->Play();
    }
    return FReply::Handled();
}

FReply SRshipTimecodePanel::OnPauseClicked()
{
    if (URshipTimecodeSync* TimecodeSync = GetTimecodeSync())
    {
        TimecodeSync->Pause();
    }
    return FReply::Handled();
}

FReply SRshipTimecodePanel::OnStopClicked()
{
    if (URshipTimecodeSync* TimecodeSync = GetTimecodeSync())
    {
        TimecodeSync->Stop();
    }
    return FReply::Handled();
}

FReply SRshipTimecodePanel::OnStepForwardClicked()
{
    if (URshipTimecodeSync* TimecodeSync = GetTimecodeSync())
    {
        TimecodeSync->StepForward(1);
    }
    return FReply::Handled();
}

FReply SRshipTimecodePanel::OnStepBackwardClicked()
{
    if (URshipTimecodeSync* TimecodeSync = GetTimecodeSync())
    {
        TimecodeSync->StepBackward(1);
    }
    return FReply::Handled();
}

FReply SRshipTimecodePanel::OnJumpToNextCueClicked()
{
    if (URshipTimecodeSync* TimecodeSync = GetTimecodeSync())
    {
        TimecodeSync->JumpToNextCue();
    }
    return FReply::Handled();
}

FReply SRshipTimecodePanel::OnJumpToPrevCueClicked()
{
    if (URshipTimecodeSync* TimecodeSync = GetTimecodeSync())
    {
        TimecodeSync->JumpToPreviousCue();
    }
    return FReply::Handled();
}

void SRshipTimecodePanel::OnSourceChanged(TSharedPtr<FString> NewSource, ESelectInfo::Type SelectInfo)
{
    if (!NewSource.IsValid())
    {
        return;
    }

    URshipTimecodeSync* TimecodeSync = GetTimecodeSync();
    if (!TimecodeSync)
    {
        return;
    }

    // Map string to enum
    int32 Index = SourceOptions.IndexOfByPredicate([&](const TSharedPtr<FString>& Item) {
        return *Item == *NewSource;
    });

    if (Index != INDEX_NONE)
    {
        TimecodeSync->SetTimecodeSource(static_cast<ERshipTimecodeSource>(Index));
    }
}

TSharedRef<SWidget> SRshipTimecodePanel::GenerateSourceComboItem(TSharedPtr<FString> InItem)
{
    return SNew(STextBlock).Text(FText::FromString(*InItem));
}

FText SRshipTimecodePanel::GetCurrentSourceText() const
{
    int32 Index = static_cast<int32>(CurrentSource);
    if (SourceOptions.IsValidIndex(Index))
    {
        return FText::FromString(*SourceOptions[Index]);
    }
    return LOCTEXT("UnknownSource", "Unknown");
}

void SRshipTimecodePanel::OnModeChanged(TSharedPtr<FString> NewMode, ESelectInfo::Type SelectInfo)
{
    if (!NewMode.IsValid())
    {
        return;
    }

    URshipTimecodeSync* TimecodeSync = GetTimecodeSync();
    if (!TimecodeSync)
    {
        return;
    }

    // Map string to enum
    int32 Index = ModeOptions.IndexOfByPredicate([&](const TSharedPtr<FString>& Item) {
        return *Item == *NewMode;
    });

    if (Index != INDEX_NONE)
    {
        ERshipTimecodeMode NewModeEnum = static_cast<ERshipTimecodeMode>(Index);
        TimecodeSync->SetTimecodeMode(NewModeEnum);
        CurrentMode = NewModeEnum;

        // Update mode status text
        if (ModeStatusText.IsValid())
        {
            switch (NewModeEnum)
            {
            case ERshipTimecodeMode::Receive:
                ModeStatusText->SetText(LOCTEXT("ModeStatusReceive", "UE follows timecode from rship server"));
                break;
            case ERshipTimecodeMode::Publish:
                ModeStatusText->SetText(LOCTEXT("ModeStatusPublish", "UE publishes timecode as emitter (UE_Timecode/timecode)"));
                break;
            case ERshipTimecodeMode::Bidirectional:
                ModeStatusText->SetText(LOCTEXT("ModeStatusBidirectional", "UE follows rship AND publishes for monitoring"));
                break;
            }
        }
    }
}

TSharedRef<SWidget> SRshipTimecodePanel::GenerateModeComboItem(TSharedPtr<FString> InItem)
{
    return SNew(STextBlock).Text(FText::FromString(*InItem));
}

FText SRshipTimecodePanel::GetCurrentModeText() const
{
    int32 Index = static_cast<int32>(CurrentMode);
    if (ModeOptions.IsValidIndex(Index))
    {
        return FText::FromString(*ModeOptions[Index]);
    }
    return LOCTEXT("UnknownMode", "Unknown");
}

TSharedRef<ITableRow> SRshipTimecodePanel::GenerateCuePointRow(TSharedPtr<FRshipCuePointListItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(SRshipCuePointRow, OwnerTable)
        .Item(Item);
}

void SRshipTimecodePanel::OnCuePointSelectionChanged(TSharedPtr<FRshipCuePointListItem> Item, ESelectInfo::Type SelectInfo)
{
    SelectedCuePoint = Item;
}

FReply SRshipTimecodePanel::OnAddCuePointClicked()
{
    URshipTimecodeSync* TimecodeSync = GetTimecodeSync();
    if (!TimecodeSync)
    {
        return FReply::Handled();
    }

    FRshipCuePoint NewCue;
    NewCue.Id = FGuid::NewGuid().ToString();
    NewCue.Name = FString::Printf(TEXT("Cue %d"), CuePointItems.Num() + 1);
    NewCue.Timecode = TimecodeSync->GetCurrentTimecode();
    NewCue.FrameNumber = TimecodeSync->GetCurrentFrame();
    NewCue.bEnabled = true;

    TimecodeSync->AddCuePoint(NewCue);
    RefreshCuePointList();

    return FReply::Handled();
}

FReply SRshipTimecodePanel::OnRemoveCuePointClicked()
{
    if (!SelectedCuePoint.IsValid())
    {
        return FReply::Handled();
    }

    if (URshipTimecodeSync* TimecodeSync = GetTimecodeSync())
    {
        TimecodeSync->RemoveCuePoint(SelectedCuePoint->Id);
        SelectedCuePoint.Reset();
        RefreshCuePointList();
    }

    return FReply::Handled();
}

FReply SRshipTimecodePanel::OnClearCuePointsClicked()
{
    if (URshipTimecodeSync* TimecodeSync = GetTimecodeSync())
    {
        TimecodeSync->ClearCuePoints();
        SelectedCuePoint.Reset();
        RefreshCuePointList();
    }

    return FReply::Handled();
}

FReply SRshipTimecodePanel::OnInjectTestTimecodeClicked()
{
    URshipTimecodeSync* TimecodeSync = GetTimecodeSync();
    if (!TimecodeSync)
    {
        return FReply::Handled();
    }

    // Create a mock timecode event
    TSharedPtr<FJsonObject> MockData = MakeShareable(new FJsonObject);
    MockData->SetNumberField(TEXT("hours"), 1);
    MockData->SetNumberField(TEXT("minutes"), 0);
    MockData->SetNumberField(TEXT("seconds"), 0);
    MockData->SetNumberField(TEXT("frames"), 0);
    MockData->SetNumberField(TEXT("frameRate"), 30);
    MockData->SetStringField(TEXT("state"), TEXT("playing"));

    TimecodeSync->ProcessTimecodeEvent(MockData);

    return FReply::Handled();
}

// ============================================================================
// CUE POINT ROW WIDGET
// ============================================================================

void SRshipCuePointRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
    Item = InArgs._Item;
    SMultiColumnTableRow<TSharedPtr<FRshipCuePointListItem>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> SRshipCuePointRow::GenerateWidgetForColumn(const FName& ColumnName)
{
    if (!Item.IsValid())
    {
        return SNullWidget::NullWidget;
    }

    if (ColumnName == "Name")
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(4.0f, 2.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(8.0f)
                .HeightOverride(8.0f)
                [
                    SNew(SBorder)
                    .BorderBackgroundColor(Item->Color)
                ]
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .Padding(4.0f, 2.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Item->Name))
            ];
    }
    else if (ColumnName == "Timecode")
    {
        FString TCString = FString::Printf(TEXT("%02d:%02d:%02d:%02d"),
            Item->Timecode.Hours,
            Item->Timecode.Minutes,
            Item->Timecode.Seconds,
            Item->Timecode.Frames);

        return SNew(SBox)
            .Padding(4.0f, 2.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TCString))
                .Font(FCoreStyle::GetDefaultFontStyle("Mono", 10))
            ];
    }
    else if (ColumnName == "Status")
    {
        FText StatusText = Item->bFired
            ? LOCTEXT("CueFired", "Fired")
            : (Item->bEnabled ? LOCTEXT("CueReady", "Ready") : LOCTEXT("CueDisabled", "Disabled"));

        return SNew(SBox)
            .Padding(4.0f, 2.0f)
            [
                SNew(STextBlock)
                .Text(StatusText)
                .ColorAndOpacity(Item->bFired ? FLinearColor::Green : (Item->bEnabled ? FLinearColor::White : FSlateColor::UseSubduedForeground()))
            ];
    }

    return SNullWidget::NullWidget;
}

#undef LOCTEXT_NAMESPACE
