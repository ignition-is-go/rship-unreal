// Copyright Rocketship. All Rights Reserved.

#include "SRshipNDIPanel.h"

#if RSHIP_EDITOR_HAS_NDI
#include "RshipNDIStreamComponent.h"
#include "RshipNDIStreamTypes.h"
#endif

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CineCameraActor.h"

#if WITH_EDITOR
#include "Editor.h"
#include "LevelEditorViewport.h"
#include "EditorViewportClient.h"
#endif

#define LOCTEXT_NAMESPACE "SRshipNDIPanel"

void SRshipNDIPanel::Construct(const FArguments& InArgs)
{
	TimeSinceLastRefresh = 0.0f;

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			// Overview Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				BuildOverviewSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Stream List
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0, 0, 0, 8)
			[
				BuildStreamListSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Selected Stream Details
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				BuildSelectedStreamSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Bulk Actions
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				BuildBulkActionsSection()
			]
		]
	];

	// Initial data load
	RefreshStreamList();
}

void SRshipNDIPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	TimeSinceLastRefresh += InDeltaTime;
	if (TimeSinceLastRefresh >= RefreshInterval)
	{
		TimeSinceLastRefresh = 0.0f;
		UpdateStreamStats();
	}
}

TSharedRef<SWidget> SRshipNDIPanel::BuildOverviewSection()
{
#if RSHIP_EDITOR_HAS_NDI
	bool bNDIAvailable = URshipNDIStreamComponent::IsNDISenderAvailable();
#else
	bool bNDIAvailable = false;
#endif

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("OverviewLabel", "NDI Streaming Overview"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(8.0f)
			[
				SNew(SVerticalBox)

				// NDI Available status
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 4)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("NDIAvailableLabel", "NDI Sender Library:"))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(NDIAvailableText, STextBlock)
						.Text(bNDIAvailable ? LOCTEXT("NDIAvailableYes", "Available") : LOCTEXT("NDIAvailableNo", "Not Found"))
						.ColorAndOpacity(bNDIAvailable ? FLinearColor::Green : FLinearColor::Red)
					]
				]

				// Total streams
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 4)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TotalStreamsLabel", "Total NDI Streams:"))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(TotalStreamsText, STextBlock)
						.Text(LOCTEXT("TotalStreamsDefault", "0"))
					]
				]

				// Active streams
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 4)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ActiveStreamsLabel", "Active Streams:"))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(ActiveStreamsText, STextBlock)
						.Text(LOCTEXT("ActiveStreamsDefault", "0"))
						.ColorAndOpacity(FLinearColor::Green)
					]
				]

				// Total receivers
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TotalReceiversLabel", "Connected Receivers:"))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(TotalReceiversText, STextBlock)
						.Text(LOCTEXT("TotalReceiversDefault", "0"))
					]
				]
			]
		];
}

TSharedRef<SWidget> SRshipNDIPanel::BuildStreamListSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("StreamListLabel", "NDI Streams"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("RefreshBtn", "Refresh"))
				.OnClicked(this, &SRshipNDIPanel::OnRefreshStreamsClicked)
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(0, 4, 0, 0)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SAssignNew(StreamListView, SListView<TSharedPtr<FRshipNDIStreamItem>>)
				.ListItemsSource(&StreamItems)
				.OnGenerateRow(this, &SRshipNDIPanel::OnGenerateStreamRow)
				.OnSelectionChanged(this, &SRshipNDIPanel::OnStreamSelectionChanged)
				.SelectionMode(ESelectionMode::Single)
				.HeaderRow
				(
					SNew(SHeaderRow)

					+ SHeaderRow::Column("Status")
					.DefaultLabel(LOCTEXT("ColStatus", ""))
					.FixedWidth(24.0f)

					+ SHeaderRow::Column("Actor")
					.DefaultLabel(LOCTEXT("ColActor", "Camera"))
					.FillWidth(0.25f)

					+ SHeaderRow::Column("StreamName")
					.DefaultLabel(LOCTEXT("ColStreamName", "Stream Name"))
					.FillWidth(0.25f)

					+ SHeaderRow::Column("Resolution")
					.DefaultLabel(LOCTEXT("ColResolution", "Resolution"))
					.FillWidth(0.15f)

					+ SHeaderRow::Column("FPS")
					.DefaultLabel(LOCTEXT("ColFPS", "FPS"))
					.FillWidth(0.1f)

					+ SHeaderRow::Column("Receivers")
					.DefaultLabel(LOCTEXT("ColReceivers", "Recv"))
					.FillWidth(0.1f)

					+ SHeaderRow::Column("Bandwidth")
					.DefaultLabel(LOCTEXT("ColBandwidth", "Mbps"))
					.FillWidth(0.15f)
				)
			]
		];
}

TSharedRef<SWidget> SRshipNDIPanel::BuildSelectedStreamSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SelectedStreamLabel", "Selected Stream Details"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(8.0f)
			[
				SNew(SVerticalBox)

				// Stream name
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 2)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 8, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SelectedNameLabel", "Name:"))
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SAssignNew(SelectedStreamNameText, STextBlock)
						.Text(LOCTEXT("SelectedNameDefault", "(none selected)"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]

				// Resolution and framerate
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 2)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 8, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SelectedResLabel", "Resolution:"))
					]

					+ SHorizontalBox::Slot()
					.FillWidth(0.5f)
					[
						SAssignNew(SelectedResolutionText, STextBlock)
						.Text(LOCTEXT("SelectedResDefault", "-"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(16, 0, 8, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SelectedFPSLabel", "Target FPS:"))
					]

					+ SHorizontalBox::Slot()
					.FillWidth(0.5f)
					[
						SAssignNew(SelectedFrameRateText, STextBlock)
						.Text(LOCTEXT("SelectedFPSDefault", "-"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]

				// Bandwidth and VRAM
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 2)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 8, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SelectedBWLabel", "Bandwidth:"))
					]

					+ SHorizontalBox::Slot()
					.FillWidth(0.5f)
					[
						SAssignNew(SelectedBandwidthText, STextBlock)
						.Text(LOCTEXT("SelectedBWDefault", "-"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(16, 0, 8, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SelectedVRAMLabel", "VRAM:"))
					]

					+ SHorizontalBox::Slot()
					.FillWidth(0.5f)
					[
						SAssignNew(SelectedVRAMText, STextBlock)
						.Text(LOCTEXT("SelectedVRAMDefault", "-"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]

				// Frames sent and dropped
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 2)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 8, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SelectedSentLabel", "Frames Sent:"))
					]

					+ SHorizontalBox::Slot()
					.FillWidth(0.5f)
					[
						SAssignNew(SelectedFramesSentText, STextBlock)
						.Text(LOCTEXT("SelectedSentDefault", "-"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(16, 0, 8, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SelectedDroppedLabel", "Dropped:"))
					]

					+ SHorizontalBox::Slot()
					.FillWidth(0.5f)
					[
						SAssignNew(SelectedDroppedFramesText, STextBlock)
						.Text(LOCTEXT("SelectedDroppedDefault", "-"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]

				// Receivers
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 4)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 8, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SelectedRecvLabel", "Connected Receivers:"))
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SAssignNew(SelectedReceiversText, STextBlock)
						.Text(LOCTEXT("SelectedRecvDefault", "-"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]

				// Control buttons
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 8, 0, 0)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 8, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("StartSelectedBtn", "Start Stream"))
						.OnClicked(this, &SRshipNDIPanel::OnStartSelectedClicked)
						.IsEnabled_Lambda([this]() { return SelectedStream.IsValid() && !SelectedStream->bIsStreaming; })
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 8, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("StopSelectedBtn", "Stop Stream"))
						.OnClicked(this, &SRshipNDIPanel::OnStopSelectedClicked)
						.IsEnabled_Lambda([this]() { return SelectedStream.IsValid() && SelectedStream->bIsStreaming; })
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("FocusCameraBtn", "Focus Camera"))
						.ToolTipText(LOCTEXT("FocusCameraTooltip", "Focus the viewport on the selected camera"))
						.OnClicked(this, &SRshipNDIPanel::OnFocusCameraClicked)
						.IsEnabled_Lambda([this]() { return SelectedStream.IsValid(); })
					]
				]
			]
		];
}

TSharedRef<SWidget> SRshipNDIPanel::BuildBulkActionsSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("BulkActionsLabel", "Bulk Actions"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 8, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("StartAllBtn", "Start All Streams"))
				.OnClicked(this, &SRshipNDIPanel::OnStartAllClicked)
				.IsEnabled_Lambda([this]() { return StreamItems.Num() > 0; })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("StopAllBtn", "Stop All Streams"))
				.OnClicked(this, &SRshipNDIPanel::OnStopAllClicked)
				.IsEnabled_Lambda([this]() { return StreamItems.Num() > 0; })
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 8, 0, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NDIHelp", "Attach URshipNDIStreamComponent to CineCameraActors to stream their output via NDI"))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
}

TSharedRef<ITableRow> SRshipNDIPanel::OnGenerateStreamRow(TSharedPtr<FRshipNDIStreamItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SRshipNDIStreamRow, OwnerTable)
		.Item(Item);
}

void SRshipNDIPanel::OnStreamSelectionChanged(TSharedPtr<FRshipNDIStreamItem> Item, ESelectInfo::Type SelectInfo)
{
	SelectedStream = Item;

	if (Item.IsValid())
	{
		SelectedStreamNameText->SetText(FText::FromString(Item->StreamName));
		SelectedStreamNameText->SetColorAndOpacity(FSlateColor::UseForeground());

		SelectedResolutionText->SetText(FText::FromString(Item->Resolution));
		SelectedResolutionText->SetColorAndOpacity(FSlateColor::UseForeground());

#if RSHIP_EDITOR_HAS_NDI
		if (Item->Component.IsValid())
		{
			const FRshipNDIStreamConfig& Config = Item->Component->Config;
			SelectedFrameRateText->SetText(FText::AsNumber(Config.FrameRate));
			SelectedFrameRateText->SetColorAndOpacity(FSlateColor::UseForeground());

			// Calculate bandwidth
			float BandwidthGBps = Config.GetBandwidthGBps();
			SelectedBandwidthText->SetText(FText::Format(LOCTEXT("BandwidthFmt", "{0} GB/s"), FText::AsNumber(BandwidthGBps, &FNumberFormattingOptions::DefaultWithGrouping().SetMaximumFractionalDigits(2))));
			SelectedBandwidthText->SetColorAndOpacity(FSlateColor::UseForeground());

			// Calculate VRAM
			int64 VRAMBytes = Config.GetVRAMUsageBytes();
			float VRAMMb = VRAMBytes / (1024.0f * 1024.0f);
			SelectedVRAMText->SetText(FText::Format(LOCTEXT("VRAMFmt", "{0} MB"), FText::AsNumber(VRAMMb, &FNumberFormattingOptions::DefaultWithGrouping().SetMaximumFractionalDigits(0))));
			SelectedVRAMText->SetColorAndOpacity(FSlateColor::UseForeground());

			// Stats
			FRshipNDIStreamStats Stats = Item->Component->GetStats();
			SelectedFramesSentText->SetText(FText::AsNumber(Stats.TotalFramesSent));
			SelectedFramesSentText->SetColorAndOpacity(FSlateColor::UseForeground());

			SelectedDroppedFramesText->SetText(FText::AsNumber(Stats.DroppedFrames));
			SelectedDroppedFramesText->SetColorAndOpacity(Stats.DroppedFrames > 0 ? FLinearColor::Yellow : FSlateColor::UseForeground());

			SelectedReceiversText->SetText(FText::AsNumber(Stats.ConnectedReceivers));
			SelectedReceiversText->SetColorAndOpacity(Stats.ConnectedReceivers > 0 ? FLinearColor::Green : FSlateColor::UseForeground());
		}
#else
		SelectedFrameRateText->SetText(LOCTEXT("NDINotAvailable", "N/A"));
		SelectedBandwidthText->SetText(LOCTEXT("NDINotAvailable", "N/A"));
		SelectedVRAMText->SetText(LOCTEXT("NDINotAvailable", "N/A"));
		SelectedFramesSentText->SetText(LOCTEXT("NDINotAvailable", "N/A"));
		SelectedDroppedFramesText->SetText(LOCTEXT("NDINotAvailable", "N/A"));
		SelectedReceiversText->SetText(LOCTEXT("NDINotAvailable", "N/A"));
#endif
	}
	else
	{
		// Clear all
		SelectedStreamNameText->SetText(LOCTEXT("SelectedNameDefault", "(none selected)"));
		SelectedStreamNameText->SetColorAndOpacity(FSlateColor::UseSubduedForeground());
		SelectedResolutionText->SetText(LOCTEXT("SelectedResDefault", "-"));
		SelectedResolutionText->SetColorAndOpacity(FSlateColor::UseSubduedForeground());
		SelectedFrameRateText->SetText(LOCTEXT("SelectedFPSDefault", "-"));
		SelectedFrameRateText->SetColorAndOpacity(FSlateColor::UseSubduedForeground());
		SelectedBandwidthText->SetText(LOCTEXT("SelectedBWDefault", "-"));
		SelectedBandwidthText->SetColorAndOpacity(FSlateColor::UseSubduedForeground());
		SelectedVRAMText->SetText(LOCTEXT("SelectedVRAMDefault", "-"));
		SelectedVRAMText->SetColorAndOpacity(FSlateColor::UseSubduedForeground());
		SelectedFramesSentText->SetText(LOCTEXT("SelectedSentDefault", "-"));
		SelectedFramesSentText->SetColorAndOpacity(FSlateColor::UseSubduedForeground());
		SelectedDroppedFramesText->SetText(LOCTEXT("SelectedDroppedDefault", "-"));
		SelectedDroppedFramesText->SetColorAndOpacity(FSlateColor::UseSubduedForeground());
		SelectedReceiversText->SetText(LOCTEXT("SelectedRecvDefault", "-"));
		SelectedReceiversText->SetColorAndOpacity(FSlateColor::UseSubduedForeground());
	}
}

FReply SRshipNDIPanel::OnRefreshStreamsClicked()
{
	RefreshStreamList();
	return FReply::Handled();
}

FReply SRshipNDIPanel::OnStartSelectedClicked()
{
#if RSHIP_EDITOR_HAS_NDI
	if (SelectedStream.IsValid() && SelectedStream->Component.IsValid())
	{
		SelectedStream->Component->StartStreaming();
		UpdateStreamStats();
	}
#endif
	return FReply::Handled();
}

FReply SRshipNDIPanel::OnStopSelectedClicked()
{
#if RSHIP_EDITOR_HAS_NDI
	if (SelectedStream.IsValid() && SelectedStream->Component.IsValid())
	{
		SelectedStream->Component->StopStreaming();
		UpdateStreamStats();
	}
#endif
	return FReply::Handled();
}

FReply SRshipNDIPanel::OnStartAllClicked()
{
#if RSHIP_EDITOR_HAS_NDI
	for (const auto& Item : StreamItems)
	{
		if (Item.IsValid() && Item->Component.IsValid() && !Item->bIsStreaming)
		{
			Item->Component->StartStreaming();
		}
	}
	UpdateStreamStats();
#endif
	return FReply::Handled();
}

FReply SRshipNDIPanel::OnStopAllClicked()
{
#if RSHIP_EDITOR_HAS_NDI
	for (const auto& Item : StreamItems)
	{
		if (Item.IsValid() && Item->Component.IsValid() && Item->bIsStreaming)
		{
			Item->Component->StopStreaming();
		}
	}
	UpdateStreamStats();
#endif
	return FReply::Handled();
}

FReply SRshipNDIPanel::OnFocusCameraClicked()
{
#if WITH_EDITOR && RSHIP_EDITOR_HAS_NDI
	if (SelectedStream.IsValid() && SelectedStream->Component.IsValid())
	{
		AActor* Owner = SelectedStream->Component->GetOwner();
		if (Owner)
		{
			GEditor->SelectNone(true, true);
			GEditor->SelectActor(Owner, true, true);
			GEditor->MoveViewportCamerasToActor(*Owner, false);
		}
	}
#endif
	return FReply::Handled();
}

void SRshipNDIPanel::RefreshStreamList()
{
	StreamItems.Empty();

#if WITH_EDITOR && RSHIP_EDITOR_HAS_NDI
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (World)
	{
		for (TActorIterator<ACineCameraActor> It(World); It; ++It)
		{
			ACineCameraActor* CameraActor = *It;
			if (URshipNDIStreamComponent* NDIComp = CameraActor->FindComponentByClass<URshipNDIStreamComponent>())
			{
				TSharedPtr<FRshipNDIStreamItem> Item = MakeShared<FRshipNDIStreamItem>();
				Item->Component = NDIComp;
				Item->ActorLabel = CameraActor->GetActorLabel();
				if (Item->ActorLabel.IsEmpty())
				{
					Item->ActorLabel = CameraActor->GetName();
				}
				Item->StreamName = NDIComp->Config.StreamName;
				Item->Resolution = FString::Printf(TEXT("%dx%d"), NDIComp->Config.Width, NDIComp->Config.Height);
				Item->bIsStreaming = NDIComp->IsStreaming();

				FRshipNDIStreamStats Stats = NDIComp->GetStats();
				Item->CurrentFPS = Stats.CurrentFPS;
				Item->ReceiverCount = Stats.ConnectedReceivers;
				Item->BandwidthMbps = Stats.BandwidthMbps;
				Item->TotalFramesSent = Stats.TotalFramesSent;
				Item->DroppedFrames = Stats.DroppedFrames;

				StreamItems.Add(Item);
			}
		}
	}
#endif

	// Update overview stats
	int32 ActiveCount = 0;
	int32 TotalReceivers = 0;
	for (const auto& Item : StreamItems)
	{
		if (Item->bIsStreaming) ActiveCount++;
		TotalReceivers += Item->ReceiverCount;
	}

	if (TotalStreamsText.IsValid())
	{
		TotalStreamsText->SetText(FText::AsNumber(StreamItems.Num()));
	}
	if (ActiveStreamsText.IsValid())
	{
		ActiveStreamsText->SetText(FText::AsNumber(ActiveCount));
		ActiveStreamsText->SetColorAndOpacity(ActiveCount > 0 ? FLinearColor::Green : FSlateColor::UseForeground());
	}
	if (TotalReceiversText.IsValid())
	{
		TotalReceiversText->SetText(FText::AsNumber(TotalReceivers));
	}

	if (StreamListView.IsValid())
	{
		StreamListView->RequestListRefresh();
	}
}

void SRshipNDIPanel::UpdateStreamStats()
{
#if RSHIP_EDITOR_HAS_NDI
	int32 ActiveCount = 0;
	int32 TotalReceivers = 0;

	for (auto& Item : StreamItems)
	{
		if (Item.IsValid() && Item->Component.IsValid())
		{
			Item->bIsStreaming = Item->Component->IsStreaming();

			FRshipNDIStreamStats Stats = Item->Component->GetStats();
			Item->CurrentFPS = Stats.CurrentFPS;
			Item->ReceiverCount = Stats.ConnectedReceivers;
			Item->BandwidthMbps = Stats.BandwidthMbps;
			Item->TotalFramesSent = Stats.TotalFramesSent;
			Item->DroppedFrames = Stats.DroppedFrames;

			if (Item->bIsStreaming) ActiveCount++;
			TotalReceivers += Item->ReceiverCount;
		}
	}

	// Update overview
	if (ActiveStreamsText.IsValid())
	{
		ActiveStreamsText->SetText(FText::AsNumber(ActiveCount));
		ActiveStreamsText->SetColorAndOpacity(ActiveCount > 0 ? FLinearColor::Green : FSlateColor::UseForeground());
	}
	if (TotalReceiversText.IsValid())
	{
		TotalReceiversText->SetText(FText::AsNumber(TotalReceivers));
	}

	// Update selected stream details if needed
	if (SelectedStream.IsValid() && SelectedStream->Component.IsValid())
	{
		FRshipNDIStreamStats Stats = SelectedStream->Component->GetStats();
		SelectedFramesSentText->SetText(FText::AsNumber(Stats.TotalFramesSent));
		SelectedDroppedFramesText->SetText(FText::AsNumber(Stats.DroppedFrames));
		SelectedDroppedFramesText->SetColorAndOpacity(Stats.DroppedFrames > 0 ? FLinearColor::Yellow : FSlateColor::UseForeground());
		SelectedReceiversText->SetText(FText::AsNumber(Stats.ConnectedReceivers));
		SelectedReceiversText->SetColorAndOpacity(Stats.ConnectedReceivers > 0 ? FLinearColor::Green : FSlateColor::UseForeground());
	}

	// Refresh the list view to update the status indicators
	if (StreamListView.IsValid())
	{
		StreamListView->RequestListRefresh();
	}
#endif
}

FLinearColor SRshipNDIPanel::GetStateColor(bool bIsStreaming, bool bHasError) const
{
	if (bHasError) return FLinearColor::Red;
	if (bIsStreaming) return FLinearColor::Green;
	return FLinearColor::Gray;
}

FText SRshipNDIPanel::GetStateText(bool bIsStreaming, bool bHasError) const
{
	if (bHasError) return LOCTEXT("StateError", "Error");
	if (bIsStreaming) return LOCTEXT("StateStreaming", "Streaming");
	return LOCTEXT("StateStopped", "Stopped");
}

// ============================================================================
// SRshipNDIStreamRow
// ============================================================================

void SRshipNDIStreamRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	Item = InArgs._Item;
	SMultiColumnTableRow<TSharedPtr<FRshipNDIStreamItem>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> SRshipNDIStreamRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (!Item.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	if (ColumnName == "Status")
	{
		FLinearColor StatusColor = Item->bIsStreaming ? FLinearColor::Green : FLinearColor::Gray;

		return SNew(SBox)
			.Padding(FMargin(4, 2))
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
				.BorderBackgroundColor(StatusColor)
				.Padding(4.0f)
			];
	}
	else if (ColumnName == "Actor")
	{
		return SNew(SBox)
			.Padding(FMargin(4, 2))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->ActorLabel))
			];
	}
	else if (ColumnName == "StreamName")
	{
		return SNew(SBox)
			.Padding(FMargin(4, 2))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->StreamName))
			];
	}
	else if (ColumnName == "Resolution")
	{
		return SNew(SBox)
			.Padding(FMargin(4, 2))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->Resolution))
			];
	}
	else if (ColumnName == "FPS")
	{
		FText FPSText = Item->bIsStreaming ?
			FText::AsNumber(FMath::RoundToInt(Item->CurrentFPS)) :
			LOCTEXT("FPSDash", "-");

		return SNew(SBox)
			.Padding(FMargin(4, 2))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FPSText)
				.ColorAndOpacity(Item->bIsStreaming ? FSlateColor::UseForeground() : FSlateColor::UseSubduedForeground())
			];
	}
	else if (ColumnName == "Receivers")
	{
		FText RecvText = Item->bIsStreaming ?
			FText::AsNumber(Item->ReceiverCount) :
			LOCTEXT("RecvDash", "-");

		return SNew(SBox)
			.Padding(FMargin(4, 2))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(RecvText)
				.ColorAndOpacity(Item->ReceiverCount > 0 ? FLinearColor::Green : FSlateColor::UseSubduedForeground())
			];
	}
	else if (ColumnName == "Bandwidth")
	{
		FText BWText = Item->bIsStreaming ?
			FText::Format(LOCTEXT("BWFmt", "{0}"), FText::AsNumber(FMath::RoundToInt(Item->BandwidthMbps))) :
			LOCTEXT("BWDash", "-");

		return SNew(SBox)
			.Padding(FMargin(4, 2))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(BWText)
				.ColorAndOpacity(Item->bIsStreaming ? FSlateColor::UseForeground() : FSlateColor::UseSubduedForeground())
			];
	}

	return SNullWidget::NullWidget;
}

#undef LOCTEXT_NAMESPACE
