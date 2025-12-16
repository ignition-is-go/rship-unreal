// Copyright Rocketship. All Rights Reserved.

#include "SRshipTestPanel.h"
#include "RshipSubsystem.h"
#include "RshipTargetComponent.h"
#include "RshipTestUtilities.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "EngineUtils.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

#define LOCTEXT_NAMESPACE "SRshipTestPanel"

void SRshipTestPanel::Construct(const FArguments& InArgs)
{
	bStressTestRunning = false;
	StressTestPulsesPerSecond = 100;
	StressTestDuration = 10.0f;
	StressTestElapsed = 0.0f;
	TotalPulsesSent = 0;
	bSimulatingDisconnect = false;
	SimulatedLatencyMs = 0.0f;
	TimeSinceLastRefresh = 0.0f;

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			// Validation Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				BuildValidationSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Mock Pulse Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				BuildMockPulseSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Stress Test Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				BuildStressTestSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Connection Simulation Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				BuildConnectionSimSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Issues Section
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				BuildIssuesSection()
			]
		]
	];
}

void SRshipTestPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// Tick test utilities for stress testing
	if (URshipTestUtilities* Utilities = GetTestUtilities())
	{
		Utilities->Tick(InDeltaTime);

		// Update stress test status from utilities
		if (Utilities->IsStressTestRunning())
		{
			bStressTestRunning = true;
			FRshipStressTestResults Results = Utilities->GetStressTestResults();
			TotalPulsesSent = Results.TotalPulsesSent;
			StressTestElapsed = Results.ActualDuration;

			if (StressTestStatusText.IsValid())
			{
				float Progress = Utilities->GetStressTestProgress();
				StressTestStatusText->SetText(FText::Format(
					LOCTEXT("StressTestRunning", "Running... {0}% - {1} pulses"),
					FMath::RoundToInt(Progress * 100),
					TotalPulsesSent));
			}
		}
		else if (bStressTestRunning)
		{
			// Test just finished
			bStressTestRunning = false;
			FRshipStressTestResults Results = Utilities->GetStressTestResults();
			TotalPulsesSent = Results.TotalPulsesSent;

			if (StressTestStatusText.IsValid())
			{
				if (Results.bCompleted)
				{
					StressTestStatusText->SetText(FText::Format(
						LOCTEXT("StressTestComplete", "Complete - {0} pulses ({1}/sec)"),
						Results.TotalPulsesSent,
						FMath::RoundToInt(Results.EffectivePulsesPerSecond)));
					StressTestStatusText->SetColorAndOpacity(FLinearColor::Green);
				}
				else
				{
					StressTestStatusText->SetText(FText::Format(
						LOCTEXT("StressTestStopped", "Stopped - {0} pulses sent"),
						TotalPulsesSent));
					StressTestStatusText->SetColorAndOpacity(FLinearColor::Gray);
				}
			}
		}

		// Sync connection simulation state
		bSimulatingDisconnect = Utilities->IsSimulatingDisconnect();
		SimulatedLatencyMs = Utilities->GetSimulatedLatency();
	}

	TimeSinceLastRefresh += InDeltaTime;
	if (TimeSinceLastRefresh >= RefreshInterval)
	{
		TimeSinceLastRefresh = 0.0f;

		// Update connection status
		if (ConnectionStatusText.IsValid())
		{
			if (bSimulatingDisconnect)
			{
				ConnectionStatusText->SetText(LOCTEXT("ConnSimDisconnected", "Simulating: Disconnected"));
				ConnectionStatusText->SetColorAndOpacity(FLinearColor::Red);
			}
			else if (SimulatedLatencyMs > 0)
			{
				ConnectionStatusText->SetText(FText::Format(
					LOCTEXT("ConnSimLatency", "Simulating: {0}ms latency"),
					FMath::RoundToInt(SimulatedLatencyMs)));
				ConnectionStatusText->SetColorAndOpacity(FLinearColor::Yellow);
			}
			else
			{
				ConnectionStatusText->SetText(LOCTEXT("ConnSimNormal", "Normal connection"));
				ConnectionStatusText->SetColorAndOpacity(FLinearColor::Green);
			}
		}
	}
}

TSharedRef<SWidget> SRshipTestPanel::BuildValidationSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ValidationLabel", "Setup Validation"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ValidationDesc", "Validate your rship setup to detect potential issues"))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4, 0, 0)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 8, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("ValidateAllBtn", "Validate All"))
				.OnClicked(this, &SRshipTestPanel::OnValidateAllClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 8, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("ValidateTargetsBtn", "Targets"))
				.OnClicked(this, &SRshipTestPanel::OnValidateTargetsClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 8, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("ValidateBindingsBtn", "Bindings"))
				.OnClicked(this, &SRshipTestPanel::OnValidateBindingsClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("ValidateMaterialsBtn", "Materials"))
				.OnClicked(this, &SRshipTestPanel::OnValidateMaterialsClicked)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 8, 0, 0)
		[
			SAssignNew(ValidationStatusText, STextBlock)
			.Text(LOCTEXT("ValidationReady", "Ready to validate"))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
}

TSharedRef<SWidget> SRshipTestPanel::BuildMockPulseSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("MockPulseLabel", "Mock Pulse Injection"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("MockPulseDesc", "Test targets without connecting to server"))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]

		// Target ID
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
				SNew(SBox)
				.WidthOverride(80.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TargetIdLabel", "Target ID:"))
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(TargetIdInput, SEditableTextBox)
				.HintText(LOCTEXT("TargetIdHint", "e.g., light_01"))
			]
		]

		// Emitter ID
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
				SNew(SBox)
				.WidthOverride(80.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("EmitterIdLabel", "Emitter ID:"))
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(EmitterIdInput, SEditableTextBox)
				.HintText(LOCTEXT("EmitterIdHint", "e.g., intensity"))
			]
		]

		// Pulse Data
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
				SNew(SBox)
				.WidthOverride(80.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PulseDataLabel", "Data (JSON):"))
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(PulseDataInput, SEditableTextBox)
				.HintText(LOCTEXT("PulseDataHint", "e.g., {\"value\": 0.5}"))
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
				.Text(LOCTEXT("InjectPulseBtn", "Inject Pulse"))
				.OnClicked(this, &SRshipTestPanel::OnInjectPulseClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("InjectRandomBtn", "Inject Random"))
				.ToolTipText(LOCTEXT("InjectRandomTooltip", "Inject random values to all targets"))
				.OnClicked(this, &SRshipTestPanel::OnInjectRandomPulseClicked)
			]
		];
}

TSharedRef<SWidget> SRshipTestPanel::BuildStressTestSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("StressTestLabel", "Stress Testing"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("StressTestDesc", "Test system performance under high pulse rates"))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
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
				.Text(LOCTEXT("PulsesPerSecLabel", "Pulses/sec:"))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 16, 0)
			[
				SNew(SBox)
				.WidthOverride(80.0f)
				[
					SAssignNew(PulsesPerSecondInput, SEditableTextBox)
					.Text(FText::AsNumber(StressTestPulsesPerSecond))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 8, 0)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("DurationLabel", "Duration (s):"))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(80.0f)
				[
					SAssignNew(StressDurationInput, SEditableTextBox)
					.Text(FText::AsNumber(StressTestDuration))
				]
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
				.Text(LOCTEXT("StartStressBtn", "Start Stress Test"))
				.OnClicked(this, &SRshipTestPanel::OnStartStressTestClicked)
				.IsEnabled_Lambda([this]() { return !bStressTestRunning; })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("StopStressBtn", "Stop"))
				.OnClicked(this, &SRshipTestPanel::OnStopStressTestClicked)
				.IsEnabled_Lambda([this]() { return bStressTestRunning; })
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4, 0, 0)
		[
			SAssignNew(StressTestStatusText, STextBlock)
			.Text(LOCTEXT("StressTestReady", "Ready"))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
}

TSharedRef<SWidget> SRshipTestPanel::BuildConnectionSimSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ConnectionSimLabel", "Connection Simulation"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ConnectionSimDesc", "Simulate connection issues for testing resilience"))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4, 0, 0)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 8, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("SimDisconnectBtn", "Simulate Disconnect"))
				.OnClicked(this, &SRshipTestPanel::OnSimulateDisconnectClicked)
				.IsEnabled_Lambda([this]() { return !bSimulatingDisconnect; })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 8, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("SimReconnectBtn", "Simulate Reconnect"))
				.OnClicked(this, &SRshipTestPanel::OnSimulateReconnectClicked)
				.IsEnabled_Lambda([this]() { return bSimulatingDisconnect; })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("ResetConnBtn", "Reset"))
				.OnClicked(this, &SRshipTestPanel::OnResetConnectionClicked)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 8, 0, 0)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 8, 0)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("LatencyLabel", "Latency (ms):"))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 8, 0)
			[
				SNew(SBox)
				.WidthOverride(80.0f)
				[
					SAssignNew(LatencyMsInput, SEditableTextBox)
					.Text(TEXT("100"))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("ApplyLatencyBtn", "Apply Latency"))
				.OnClicked(this, &SRshipTestPanel::OnSimulateLatencyClicked)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4, 0, 0)
		[
			SAssignNew(ConnectionStatusText, STextBlock)
			.Text(LOCTEXT("ConnNormal", "Normal connection"))
			.ColorAndOpacity(FLinearColor::Green)
		];
}

TSharedRef<SWidget> SRshipTestPanel::BuildIssuesSection()
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
				.Text(LOCTEXT("IssuesLabel", "Validation Issues"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 8, 0)
			[
				SAssignNew(IssueCountText, STextBlock)
				.Text(LOCTEXT("IssueCount", "0 issues"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("ClearIssuesBtn", "Clear"))
				.OnClicked(this, &SRshipTestPanel::OnClearIssuesClicked)
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(0, 4, 0, 0)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SAssignNew(IssuesListView, SListView<TSharedPtr<FRshipTestPanelIssue>>)
				.ListItemsSource(&Issues)
				.OnGenerateRow(this, &SRshipTestPanel::OnGenerateIssueRow)
				.OnSelectionChanged(this, &SRshipTestPanel::OnIssueSelectionChanged)
				.SelectionMode(ESelectionMode::Single)
				.HeaderRow
				(
					SNew(SHeaderRow)

					+ SHeaderRow::Column("Severity")
					.DefaultLabel(LOCTEXT("ColSeverity", ""))
					.FixedWidth(24.0f)

					+ SHeaderRow::Column("Category")
					.DefaultLabel(LOCTEXT("ColCategory", "Category"))
					.FillWidth(0.15f)

					+ SHeaderRow::Column("Message")
					.DefaultLabel(LOCTEXT("ColMessage", "Message"))
					.FillWidth(0.5f)

					+ SHeaderRow::Column("Fix")
					.DefaultLabel(LOCTEXT("ColFix", "Suggested Fix"))
					.FillWidth(0.35f)
				)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4, 0, 0)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(8.0f)
			.Visibility_Lambda([this]() { return SelectedIssue.IsValid() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DetailsLabel", "Details:"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 4, 0, 0)
				[
					SAssignNew(SelectedIssueText, STextBlock)
					.AutoWrapText(true)
				]
			]
		];
}

TSharedRef<ITableRow> SRshipTestPanel::OnGenerateIssueRow(TSharedPtr<FRshipTestPanelIssue> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SRshipTestPanelIssueRow, OwnerTable)
		.Item(Item);
}

void SRshipTestPanel::OnIssueSelectionChanged(TSharedPtr<FRshipTestPanelIssue> Item, ESelectInfo::Type SelectInfo)
{
	SelectedIssue = Item;

	if (Item.IsValid() && SelectedIssueText.IsValid())
	{
		SelectedIssueText->SetText(FText::FromString(Item->Details.IsEmpty() ? Item->Message : Item->Details));
	}
}

FReply SRshipTestPanel::OnValidateAllClicked()
{
	Issues.Empty();
	ValidateTargets();
	ValidateBindings();
	ValidateMaterials();
	ValidateLiveLink();

	if (ValidationStatusText.IsValid())
	{
		int32 ErrorCount = 0;
		int32 WarningCount = 0;
		for (const auto& Issue : Issues)
		{
			if (Issue->Severity == ERshipTestSeverity::Error) ErrorCount++;
			else if (Issue->Severity == ERshipTestSeverity::Warning) WarningCount++;
		}

		if (ErrorCount > 0)
		{
			ValidationStatusText->SetText(FText::Format(
				LOCTEXT("ValidationErrors", "{0} errors, {1} warnings found"),
				ErrorCount, WarningCount));
			ValidationStatusText->SetColorAndOpacity(FLinearColor::Red);
		}
		else if (WarningCount > 0)
		{
			ValidationStatusText->SetText(FText::Format(
				LOCTEXT("ValidationWarnings", "{0} warnings found"),
				WarningCount));
			ValidationStatusText->SetColorAndOpacity(FLinearColor::Yellow);
		}
		else
		{
			ValidationStatusText->SetText(LOCTEXT("ValidationPassed", "All checks passed!"));
			ValidationStatusText->SetColorAndOpacity(FLinearColor::Green);
		}
	}

	if (IssueCountText.IsValid())
	{
		IssueCountText->SetText(FText::Format(
			LOCTEXT("IssueCountFmt", "{0} {0}|plural(one=issue,other=issues)"),
			Issues.Num()));
	}

	if (IssuesListView.IsValid())
	{
		IssuesListView->RequestListRefresh();
	}

	return FReply::Handled();
}

FReply SRshipTestPanel::OnValidateTargetsClicked()
{
	Issues.Empty();
	ValidateTargets();

	if (IssueCountText.IsValid())
	{
		IssueCountText->SetText(FText::Format(LOCTEXT("IssueCountFmt", "{0} {0}|plural(one=issue,other=issues)"), Issues.Num()));
	}
	if (IssuesListView.IsValid())
	{
		IssuesListView->RequestListRefresh();
	}

	return FReply::Handled();
}

FReply SRshipTestPanel::OnValidateBindingsClicked()
{
	Issues.Empty();
	ValidateBindings();

	if (IssueCountText.IsValid())
	{
		IssueCountText->SetText(FText::Format(LOCTEXT("IssueCountFmt", "{0} {0}|plural(one=issue,other=issues)"), Issues.Num()));
	}
	if (IssuesListView.IsValid())
	{
		IssuesListView->RequestListRefresh();
	}

	return FReply::Handled();
}

FReply SRshipTestPanel::OnValidateMaterialsClicked()
{
	Issues.Empty();
	ValidateMaterials();

	if (IssueCountText.IsValid())
	{
		IssueCountText->SetText(FText::Format(LOCTEXT("IssueCountFmt", "{0} {0}|plural(one=issue,other=issues)"), Issues.Num()));
	}
	if (IssuesListView.IsValid())
	{
		IssuesListView->RequestListRefresh();
	}

	return FReply::Handled();
}

FReply SRshipTestPanel::OnClearIssuesClicked()
{
	Issues.Empty();
	SelectedIssue = nullptr;

	if (IssueCountText.IsValid())
	{
		IssueCountText->SetText(LOCTEXT("IssueCount", "0 issues"));
	}
	if (ValidationStatusText.IsValid())
	{
		ValidationStatusText->SetText(LOCTEXT("ValidationReady", "Ready to validate"));
		ValidationStatusText->SetColorAndOpacity(FSlateColor::UseSubduedForeground());
	}
	if (IssuesListView.IsValid())
	{
		IssuesListView->RequestListRefresh();
	}

	return FReply::Handled();
}

FReply SRshipTestPanel::OnInjectPulseClicked()
{
	FString TargetId = TargetIdInput->GetText().ToString();
	FString EmitterId = EmitterIdInput->GetText().ToString();
	FString PulseData = PulseDataInput->GetText().ToString();

	if (TargetId.IsEmpty())
	{
		AddIssue(ERshipTestSeverity::Warning, TEXT("Mock Pulse"), TEXT("Target ID is required"));
	}
	else if (EmitterId.IsEmpty())
	{
		AddIssue(ERshipTestSeverity::Warning, TEXT("Mock Pulse"), TEXT("Emitter ID is required"));
	}
	else
	{
		URshipTestUtilities* Utilities = GetTestUtilities();
		if (Utilities)
		{
			if (Utilities->InjectMockPulse(TargetId, EmitterId, PulseData))
			{
				AddIssue(ERshipTestSeverity::Info, TEXT("Mock Pulse"),
					FString::Printf(TEXT("Injected pulse: %s.%s"), *TargetId, *EmitterId),
					PulseData);
			}
			else
			{
				AddIssue(ERshipTestSeverity::Error, TEXT("Mock Pulse"),
					FString::Printf(TEXT("Failed to inject pulse: %s.%s"), *TargetId, *EmitterId),
					TEXT("Target may not exist or pulse data is invalid"));
			}
		}
	}

	if (IssuesListView.IsValid())
	{
		IssuesListView->RequestListRefresh();
	}

	return FReply::Handled();
}

FReply SRshipTestPanel::OnInjectRandomPulseClicked()
{
	URshipTestUtilities* Utilities = GetTestUtilities();
	if (Utilities)
	{
		int32 PulsesInjected = Utilities->InjectRandomPulsesToAllTargets();
		if (PulsesInjected > 0)
		{
			AddIssue(ERshipTestSeverity::Info, TEXT("Mock Pulse"),
				FString::Printf(TEXT("Injected %d random pulses to all targets"), PulsesInjected));
		}
		else
		{
			AddIssue(ERshipTestSeverity::Warning, TEXT("Mock Pulse"),
				TEXT("No targets found to inject pulses to"));
		}
	}

	if (IssuesListView.IsValid())
	{
		IssuesListView->RequestListRefresh();
	}

	return FReply::Handled();
}

FReply SRshipTestPanel::OnStartStressTestClicked()
{
	StressTestPulsesPerSecond = FCString::Atoi(*PulsesPerSecondInput->GetText().ToString());
	StressTestDuration = FCString::Atof(*StressDurationInput->GetText().ToString());
	StressTestElapsed = 0.0f;
	TotalPulsesSent = 0;

	URshipTestUtilities* Utilities = GetTestUtilities();
	if (Utilities)
	{
		FRshipStressTestConfig Config;
		Config.PulsesPerSecond = StressTestPulsesPerSecond;
		Config.DurationSeconds = StressTestDuration;
		Config.bRandomizeValues = true;
		Utilities->StartStressTest(Config);
		bStressTestRunning = true;
	}

	if (StressTestStatusText.IsValid())
	{
		StressTestStatusText->SetText(LOCTEXT("StressTestStarting", "Starting..."));
		StressTestStatusText->SetColorAndOpacity(FLinearColor::Yellow);
	}

	return FReply::Handled();
}

FReply SRshipTestPanel::OnStopStressTestClicked()
{
	URshipTestUtilities* Utilities = GetTestUtilities();
	if (Utilities)
	{
		Utilities->StopStressTest();
	}
	bStressTestRunning = false;

	if (StressTestStatusText.IsValid())
	{
		StressTestStatusText->SetText(FText::Format(
			LOCTEXT("StressTestStopped", "Stopped - {0} pulses sent"),
			TotalPulsesSent));
		StressTestStatusText->SetColorAndOpacity(FLinearColor::Gray);
	}

	return FReply::Handled();
}

FReply SRshipTestPanel::OnSimulateDisconnectClicked()
{
	URshipTestUtilities* Utilities = GetTestUtilities();
	if (Utilities)
	{
		Utilities->SimulateDisconnect();
		bSimulatingDisconnect = true;
	}
	return FReply::Handled();
}

FReply SRshipTestPanel::OnSimulateReconnectClicked()
{
	URshipTestUtilities* Utilities = GetTestUtilities();
	if (Utilities)
	{
		Utilities->SimulateReconnect();
		bSimulatingDisconnect = false;
	}
	return FReply::Handled();
}

FReply SRshipTestPanel::OnSimulateLatencyClicked()
{
	SimulatedLatencyMs = FCString::Atof(*LatencyMsInput->GetText().ToString());
	URshipTestUtilities* Utilities = GetTestUtilities();
	if (Utilities)
	{
		Utilities->SetSimulatedLatency(SimulatedLatencyMs);
	}
	return FReply::Handled();
}

FReply SRshipTestPanel::OnResetConnectionClicked()
{
	URshipTestUtilities* Utilities = GetTestUtilities();
	if (Utilities)
	{
		Utilities->ResetConnectionSimulation();
	}
	bSimulatingDisconnect = false;
	SimulatedLatencyMs = 0.0f;
	return FReply::Handled();
}

void SRshipTestPanel::ValidateTargets()
{
	URshipTestUtilities* Utilities = GetTestUtilities();
	if (!Utilities)
	{
		AddIssue(ERshipTestSeverity::Warning, TEXT("Target"), TEXT("Test utilities not available"));
		return;
	}

	TArray<FRshipTestIssue> Results = Utilities->ValidateTargets();
	for (const FRshipTestIssue& Result : Results)
	{
		AddIssue(Result.Severity, Result.Category, Result.Message, Result.Details, Result.SuggestedFix);
	}
}

void SRshipTestPanel::ValidateBindings()
{
	// Binding validation is part of target validation
	// Add placeholder for now
	AddIssue(ERshipTestSeverity::Info, TEXT("Binding"), TEXT("Binding validation included in target checks"));
}

void SRshipTestPanel::ValidateMaterials()
{
	URshipTestUtilities* Utilities = GetTestUtilities();
	if (!Utilities)
	{
		AddIssue(ERshipTestSeverity::Warning, TEXT("Material"), TEXT("Test utilities not available"));
		return;
	}

	TArray<FRshipTestIssue> Results = Utilities->ValidateMaterialBindings();
	for (const FRshipTestIssue& Result : Results)
	{
		AddIssue(Result.Severity, Result.Category, Result.Message, Result.Details, Result.SuggestedFix);
	}
}

void SRshipTestPanel::ValidateLiveLink()
{
	URshipTestUtilities* Utilities = GetTestUtilities();
	if (!Utilities)
	{
		AddIssue(ERshipTestSeverity::Warning, TEXT("LiveLink"), TEXT("Test utilities not available"));
		return;
	}

	TArray<FRshipTestIssue> Results = Utilities->ValidateLiveLinkSetup();
	for (const FRshipTestIssue& Result : Results)
	{
		AddIssue(Result.Severity, Result.Category, Result.Message, Result.Details, Result.SuggestedFix);
	}
}

void SRshipTestPanel::AddIssue(ERshipTestSeverity Severity, const FString& Category, const FString& Message, const FString& Details, const FString& Fix)
{
	TSharedPtr<FRshipTestPanelIssue> Issue = MakeShared<FRshipTestPanelIssue>();
	Issue->Severity = Severity;
	Issue->Category = Category;
	Issue->Message = Message;
	Issue->Details = Details;
	Issue->FixSuggestion = Fix;
	Issues.Add(Issue);

	if (IssueCountText.IsValid())
	{
		IssueCountText->SetText(FText::Format(LOCTEXT("IssueCountFmt", "{0} {0}|plural(one=issue,other=issues)"), Issues.Num()));
	}
}

// ============================================================================
// SRshipTestPanelIssueRow
// ============================================================================

void SRshipTestPanelIssueRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	Item = InArgs._Item;
	SMultiColumnTableRow<TSharedPtr<FRshipTestPanelIssue>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> SRshipTestPanelIssueRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (!Item.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	if (ColumnName == "Severity")
	{
		FLinearColor Color;
		switch (Item->Severity)
		{
		case ERshipTestSeverity::Error: Color = FLinearColor::Red; break;
		case ERshipTestSeverity::Warning: Color = FLinearColor::Yellow; break;
		default: Color = FLinearColor::Gray; break;
		}

		return SNew(SBox)
			.Padding(FMargin(4, 2))
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.FilledCircle"))
				.ColorAndOpacity(Color)
			];
	}
	else if (ColumnName == "Category")
	{
		return SNew(SBox)
			.Padding(FMargin(4, 2))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->Category))
			];
	}
	else if (ColumnName == "Message")
	{
		return SNew(SBox)
			.Padding(FMargin(4, 2))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->Message))
			];
	}
	else if (ColumnName == "Fix")
	{
		return SNew(SBox)
			.Padding(FMargin(4, 2))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->FixSuggestion))
				.ColorAndOpacity(Item->FixSuggestion.IsEmpty() ? FSlateColor::UseSubduedForeground() : FSlateColor::UseForeground())
			];
	}

	return SNullWidget::NullWidget;
}

// ============================================================================
// GetTestUtilities
// ============================================================================

URshipTestUtilities* SRshipTestPanel::GetTestUtilities()
{
	if (!TestUtilities.IsValid())
	{
		TestUtilities = NewObject<URshipTestUtilities>();
		TestUtilities->AddToRoot(); // Prevent GC
	}
	return TestUtilities.Get();
}

#undef LOCTEXT_NAMESPACE
