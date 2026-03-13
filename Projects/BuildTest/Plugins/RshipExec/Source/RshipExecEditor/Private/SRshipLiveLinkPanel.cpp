// Copyright Rocketship. All Rights Reserved.

#include "SRshipLiveLinkPanel.h"
#include "RshipSubsystem.h"
#include "RshipLiveLinkSource.h"

#include "Engine/Engine.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

#if WITH_EDITOR
#include "ILiveLinkClient.h"
#include "LiveLinkClient.h"
#include "Roles/LiveLinkTransformRole.h"
#include "Roles/LiveLinkCameraRole.h"
#include "Roles/LiveLinkLightRole.h"
#endif

#define LOCTEXT_NAMESPACE "SRshipLiveLinkPanel"

void SRshipLiveLinkPanel::Construct(const FArguments& InArgs)
{
	CurrentMode = 0; // Default to Consume
	TimeSinceLastRefresh = 0.0f;

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			// Mode Selection
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				BuildModeSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Sources Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				BuildSourcesSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Subjects List
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0, 0, 0, 8)
			[
				BuildSubjectsSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Mapping Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				BuildMappingSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Status Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				BuildStatusSection()
			]
		]
	];

	// Initial data load
	RefreshSubjectList();
}

void SRshipLiveLinkPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	TimeSinceLastRefresh += InDeltaTime;
	if (TimeSinceLastRefresh >= RefreshInterval)
	{
		TimeSinceLastRefresh = 0.0f;
		RefreshStatus();
	}
}

TSharedRef<SWidget> SRshipLiveLinkPanel::BuildModeSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ModeLabel", "Integration Mode"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(SSegmentedControl<int32>)
			.OnValueChanged(this, &SRshipLiveLinkPanel::OnModeChanged)
			+ SSegmentedControl<int32>::Slot(0)
			.Text(LOCTEXT("ModeConsume", "Consume"))
			.ToolTip(LOCTEXT("ModeConsumeTooltip", "Receive LiveLink data from rship"))
			+ SSegmentedControl<int32>::Slot(1)
			.Text(LOCTEXT("ModePublish", "Publish"))
			.ToolTip(LOCTEXT("ModePublishTooltip", "Send LiveLink data to rship"))
			+ SSegmentedControl<int32>::Slot(2)
			.Text(LOCTEXT("ModeBidirectional", "Both"))
			.ToolTip(LOCTEXT("ModeBidirectionalTooltip", "Both consume from and publish to rship"))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4, 0, 0)
		[
			SAssignNew(ModeDescriptionText, STextBlock)
			.Text(LOCTEXT("ModeDescConsume", "Receiving LiveLink subjects from rship pulses"))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
}

TSharedRef<SWidget> SRshipLiveLinkPanel::BuildSourcesSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SourcesLabel", "LiveLink Sources"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SourcesDesc", "Active sources providing LiveLink data"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8, 0, 0, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("RefreshBtn", "Refresh"))
				.OnClicked(this, &SRshipLiveLinkPanel::OnRefreshClicked)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 8, 0, 0)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(8.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Check"))
						.ColorAndOpacity(FLinearColor::Green)
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(8, 0, 0, 0)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("RshipSource", "Rocketship LiveLink Source"))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SAssignNew(ConnectionStatusText, STextBlock)
						.Text(LOCTEXT("SourceDisconnected", "Disconnected"))
						.ColorAndOpacity(FLinearColor::Red)
					]
				]
			]
		];
}

TSharedRef<SWidget> SRshipLiveLinkPanel::BuildSubjectsSection()
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
				.Text(LOCTEXT("SubjectsLabel", "LiveLink Subjects"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SAssignNew(SubjectCountText, STextBlock)
				.Text(LOCTEXT("SubjectCount", "0 subjects"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(0, 4, 0, 0)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SAssignNew(SubjectListView, SListView<TSharedPtr<FRshipLiveLinkSubjectItem>>)
				.ListItemsSource(&SubjectItems)
				.OnGenerateRow(this, &SRshipLiveLinkPanel::OnGenerateSubjectRow)
				.OnSelectionChanged(this, &SRshipLiveLinkPanel::OnSubjectSelectionChanged)
				.SelectionMode(ESelectionMode::Single)
				.HeaderRow
				(
					SNew(SHeaderRow)

					+ SHeaderRow::Column("Name")
					.DefaultLabel(LOCTEXT("ColName", "Subject"))
					.FillWidth(0.3f)

					+ SHeaderRow::Column("Role")
					.DefaultLabel(LOCTEXT("ColRole", "Role"))
					.FillWidth(0.2f)

					+ SHeaderRow::Column("Direction")
					.DefaultLabel(LOCTEXT("ColDirection", "Direction"))
					.FillWidth(0.2f)

					+ SHeaderRow::Column("EmitterId")
					.DefaultLabel(LOCTEXT("ColEmitter", "Emitter ID"))
					.FillWidth(0.2f)

					+ SHeaderRow::Column("Status")
					.DefaultLabel(LOCTEXT("ColStatus", "Status"))
					.FillWidth(0.1f)
				)
			]
		];
}

TSharedRef<SWidget> SRshipLiveLinkPanel::BuildMappingSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("MappingLabel", "Emitter Mapping"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 8, 0)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SelectedLabel", "Selected:"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(SelectedSubjectText, STextBlock)
				.Text(LOCTEXT("NoneSelected", "(none)"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4, 0, 0)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 8, 0)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("EmitterIdLabel", "Emitter ID:"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0, 0, 8, 0)
			[
				SAssignNew(EmitterIdInput, SEditableTextBox)
				.HintText(LOCTEXT("EmitterIdHint", "Enter rship emitter ID"))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("MapBtn", "Map"))
				.OnClicked(this, &SRshipLiveLinkPanel::OnMapToEmitterClicked)
				.IsEnabled_Lambda([this]() { return SelectedSubject.IsValid(); })
			]
		]

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
				.Text(LOCTEXT("PublishSelectedBtn", "Publish Selected"))
				.OnClicked(this, &SRshipLiveLinkPanel::OnPublishSelectedClicked)
				.IsEnabled_Lambda([this]() { return SelectedSubject.IsValid() && CurrentMode != 0; })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("StopPublishingBtn", "Stop Publishing"))
				.OnClicked(this, &SRshipLiveLinkPanel::OnStopPublishingClicked)
				.IsEnabled_Lambda([this]() { return SelectedSubject.IsValid() && SelectedSubject->bIsPublishedToRship; })
			]
		];
}

TSharedRef<SWidget> SRshipLiveLinkPanel::BuildStatusSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("StatusLabel", "Status"))
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

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("FrameRateLabel", "Average Frame Rate:"))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(FrameRateText, STextBlock)
						.Text(LOCTEXT("FrameRateValue", "-- fps"))
					]
				]
			]
		];
}

TSharedRef<ITableRow> SRshipLiveLinkPanel::OnGenerateSubjectRow(TSharedPtr<FRshipLiveLinkSubjectItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SRshipLiveLinkSubjectRow, OwnerTable)
		.Item(Item);
}

void SRshipLiveLinkPanel::OnSubjectSelectionChanged(TSharedPtr<FRshipLiveLinkSubjectItem> Item, ESelectInfo::Type SelectInfo)
{
	SelectedSubject = Item;

	if (Item.IsValid())
	{
		SelectedSubjectText->SetText(FText::FromName(Item->SubjectName));
		EmitterIdInput->SetText(FText::FromString(Item->RshipEmitterId));
	}
	else
	{
		SelectedSubjectText->SetText(LOCTEXT("NoneSelected", "(none)"));
		EmitterIdInput->SetText(FText::GetEmpty());
	}
}

FReply SRshipLiveLinkPanel::OnRefreshClicked()
{
	RefreshSubjectList();
	return FReply::Handled();
}

FReply SRshipLiveLinkPanel::OnPublishSelectedClicked()
{
	if (SelectedSubject.IsValid())
	{
		// Get the LiveLink service and add emitter mapping
		if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
		{
			if (URshipLiveLinkService* LiveLinkService = Subsystem->GetLiveLinkService())
			{
				FRshipLiveLinkEmitterMapping Mapping;
				Mapping.SubjectName = SelectedSubject->SubjectName;
				Mapping.TargetId = TEXT("UE_LiveLink");
				Mapping.EmitterId = SelectedSubject->RshipEmitterId.IsEmpty()
					? SelectedSubject->SubjectName.ToString()
					: SelectedSubject->RshipEmitterId;
				Mapping.PublishRateHz = 30.0f;
				Mapping.bEnabled = true;

				LiveLinkService->AddEmitterMapping(Mapping);
				SelectedSubject->bIsPublishedToRship = true;
				SelectedSubject->RshipEmitterId = Mapping.EmitterId;
			}
		}
		SubjectListView->RequestListRefresh();
	}
	return FReply::Handled();
}

FReply SRshipLiveLinkPanel::OnStopPublishingClicked()
{
	if (SelectedSubject.IsValid())
	{
		// Get the LiveLink service and remove emitter mapping
		if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
		{
			if (URshipLiveLinkService* LiveLinkService = Subsystem->GetLiveLinkService())
			{
				LiveLinkService->RemoveEmitterMapping(SelectedSubject->SubjectName);
				SelectedSubject->bIsPublishedToRship = false;
				SelectedSubject->RshipEmitterId.Empty();
			}
		}
		SubjectListView->RequestListRefresh();
	}
	return FReply::Handled();
}

FReply SRshipLiveLinkPanel::OnMapToEmitterClicked()
{
	if (SelectedSubject.IsValid())
	{
		SelectedSubject->RshipEmitterId = EmitterIdInput->GetText().ToString();

		// If already publishing, update the mapping
		if (SelectedSubject->bIsPublishedToRship)
		{
			if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
			{
				if (URshipLiveLinkService* LiveLinkService = Subsystem->GetLiveLinkService())
				{
					// Remove old and add new with updated emitter ID
					LiveLinkService->RemoveEmitterMapping(SelectedSubject->SubjectName);

					FRshipLiveLinkEmitterMapping Mapping;
					Mapping.SubjectName = SelectedSubject->SubjectName;
					Mapping.TargetId = TEXT("UE_LiveLink");
					Mapping.EmitterId = SelectedSubject->RshipEmitterId;
					Mapping.PublishRateHz = 30.0f;
					Mapping.bEnabled = true;

					LiveLinkService->AddEmitterMapping(Mapping);
				}
			}
		}
		SubjectListView->RequestListRefresh();
	}
	return FReply::Handled();
}

void SRshipLiveLinkPanel::OnModeChanged(int32 NewMode)
{
	CurrentMode = NewMode;

	FText Description;
	ERshipLiveLinkMode ServiceMode;

	switch (NewMode)
	{
	case 0:
		Description = LOCTEXT("ModeDescConsume", "Receiving LiveLink subjects from rship pulses");
		ServiceMode = ERshipLiveLinkMode::Consume;
		break;
	case 1:
		Description = LOCTEXT("ModeDescPublish", "Publishing UE LiveLink subjects to rship as emitters");
		ServiceMode = ERshipLiveLinkMode::Publish;
		break;
	case 2:
	default:
		Description = LOCTEXT("ModeDescBoth", "Both receiving from and publishing to rship");
		ServiceMode = ERshipLiveLinkMode::Bidirectional;
		break;
	}

	if (ModeDescriptionText.IsValid())
	{
		ModeDescriptionText->SetText(Description);
	}

	// Notify RshipLiveLinkService of mode change
	if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
	{
		if (URshipLiveLinkService* LiveLinkService = Subsystem->GetLiveLinkService())
		{
			LiveLinkService->SetMode(ServiceMode);
		}
	}
}

void SRshipLiveLinkPanel::RefreshSubjectList()
{
	SubjectItems.Empty();

#if WITH_EDITOR
	// Get LiveLink client
	ILiveLinkClient* LiveLinkClient = &IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	if (LiveLinkClient)
	{
		TArray<FLiveLinkSubjectKey> SubjectKeys = LiveLinkClient->GetSubjects(true, true);

		for (const FLiveLinkSubjectKey& Key : SubjectKeys)
		{
			TSharedPtr<FRshipLiveLinkSubjectItem> Item = MakeShared<FRshipLiveLinkSubjectItem>();
			Item->SubjectName = Key.SubjectName;

			// Determine role
			TSubclassOf<ULiveLinkRole> RoleClass = LiveLinkClient->GetSubjectRole_AnyThread(Key);
			if (RoleClass)
			{
				Item->Role = RoleClass->GetName();
				// Clean up role name (remove "LiveLink" prefix and "Role" suffix)
				Item->Role.RemoveFromStart(TEXT("LiveLink"));
				Item->Role.RemoveFromEnd(TEXT("Role"));
			}
			else
			{
				Item->Role = TEXT("Unknown");
			}

			// Check if from rship source
			FText SourceType = LiveLinkClient->GetSourceType(Key.Source);
			Item->bIsFromRship = SourceType.ToString().Contains(TEXT("Rship")) ||
			                     SourceType.ToString().Contains(TEXT("Rocketship"));

			// Status
			bool bIsSubjectValid = LiveLinkClient->IsSubjectValid(Key);
			Item->Status = bIsSubjectValid ? TEXT("Active") : TEXT("Stale");

			SubjectItems.Add(Item);
		}
	}
#endif

	// Update count text
	if (SubjectCountText.IsValid())
	{
		SubjectCountText->SetText(FText::Format(
			LOCTEXT("SubjectCountFmt", "{0} {0}|plural(one=subject,other=subjects)"),
			SubjectItems.Num()));
	}

	if (SubjectListView.IsValid())
	{
		SubjectListView->RequestListRefresh();
	}
}

void SRshipLiveLinkPanel::RefreshStatus()
{
	// Update connection status
	if (ConnectionStatusText.IsValid())
	{
		if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
		{
			bool bConnected = Subsystem->IsConnected();
			ConnectionStatusText->SetText(bConnected ?
				LOCTEXT("SourceConnected", "Connected") :
				LOCTEXT("SourceDisconnected", "Disconnected"));
			ConnectionStatusText->SetColorAndOpacity(bConnected ? FLinearColor::Green : FLinearColor::Red);
		}
	}

	// Update frame rate (would need LiveLink metrics)
	// For now just show placeholder
	if (FrameRateText.IsValid())
	{
		FrameRateText->SetText(LOCTEXT("FrameRatePlaceholder", "60 fps"));
	}
}

// ============================================================================
// SRshipLiveLinkSubjectRow
// ============================================================================

void SRshipLiveLinkSubjectRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	Item = InArgs._Item;
	SMultiColumnTableRow<TSharedPtr<FRshipLiveLinkSubjectItem>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> SRshipLiveLinkSubjectRow::GenerateWidgetForColumn(const FName& ColumnName)
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
			.Padding(4, 2)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush(Item->bIsFromRship ? "Icons.Import" : "Icons.Export"))
				.ColorAndOpacity(Item->bIsFromRship ? FLinearColor::Green : FLinearColor::Yellow)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(4, 2)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromName(Item->SubjectName))
			];
	}
	else if (ColumnName == "Role")
	{
		return SNew(SBox)
			.Padding(FMargin(4, 2))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->Role))
			];
	}
	else if (ColumnName == "Direction")
	{
		FString Direction;
		if (Item->bIsFromRship && Item->bIsPublishedToRship)
		{
			Direction = TEXT("Bidirectional");
		}
		else if (Item->bIsFromRship)
		{
			Direction = TEXT("From Rship");
		}
		else if (Item->bIsPublishedToRship)
		{
			Direction = TEXT("To Rship");
		}
		else
		{
			Direction = TEXT("Local");
		}

		return SNew(SBox)
			.Padding(FMargin(4, 2))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Direction))
			];
	}
	else if (ColumnName == "EmitterId")
	{
		return SNew(SBox)
			.Padding(FMargin(4, 2))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->RshipEmitterId.IsEmpty() ? TEXT("-") : Item->RshipEmitterId))
				.ColorAndOpacity(Item->RshipEmitterId.IsEmpty() ? FSlateColor::UseSubduedForeground() : FSlateColor::UseForeground())
			];
	}
	else if (ColumnName == "Status")
	{
		FLinearColor StatusColor = Item->Status == TEXT("Active") ? FLinearColor::Green : FLinearColor::Gray;

		return SNew(SBox)
			.Padding(FMargin(4, 2))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->Status))
				.ColorAndOpacity(StatusColor)
			];
	}

	return SNullWidget::NullWidget;
}

#undef LOCTEXT_NAMESPACE
