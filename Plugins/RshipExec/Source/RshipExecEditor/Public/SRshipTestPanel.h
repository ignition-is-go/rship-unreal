// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "RshipTestUtilities.h"

class URshipTestUtilities;

/**
 * Validation issue item for the panel UI
 * (Wraps FRshipValidationResult for display)
 * Named differently from FRshipValidationIssue in RshipSceneValidator.h to avoid ODR violations
 */
struct FRshipTestPanelIssue
{
	ERshipTestSeverity Severity = ERshipTestSeverity::Info;
	FString Category;
	FString Message;
	FString Details;
	FString FixSuggestion;

	FRshipTestPanelIssue() = default;

	// Convert from validation result
	FRshipTestPanelIssue(const FRshipValidationResult& Result)
		: Severity(Result.Severity)
		, Category(Result.Category)
		, Message(Result.Message)
		, Details(Result.Details)
		, FixSuggestion(Result.SuggestedFix)
	{}

	FString GetSeverityString() const
	{
		switch (Severity)
		{
		case ERshipTestSeverity::Info: return TEXT("Info");
		case ERshipTestSeverity::Warning: return TEXT("Warning");
		case ERshipTestSeverity::Error: return TEXT("Error");
		default: return TEXT("Unknown");
		}
	}
};

/**
 * Test panel for validation, mock data injection, and stress testing
 *
 * Features:
 * - Validate target/binding setup
 * - Inject mock pulses for testing without server
 * - Stress test with configurable pulse rates
 * - Simulate connection issues
 * - View validation issues and warnings
 */
class SRshipTestPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRshipTestPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	// UI Section builders
	TSharedRef<SWidget> BuildValidationSection();
	TSharedRef<SWidget> BuildMockPulseSection();
	TSharedRef<SWidget> BuildStressTestSection();
	TSharedRef<SWidget> BuildConnectionSimSection();
	TSharedRef<SWidget> BuildIssuesSection();

	// List view callbacks
	TSharedRef<ITableRow> OnGenerateIssueRow(TSharedPtr<FRshipTestPanelIssue> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnIssueSelectionChanged(TSharedPtr<FRshipTestPanelIssue> Item, ESelectInfo::Type SelectInfo);

	// Button callbacks
	FReply OnValidateAllClicked();
	FReply OnValidateTargetsClicked();
	FReply OnValidateBindingsClicked();
	FReply OnValidateMaterialsClicked();
	FReply OnClearIssuesClicked();

	// Mock pulse callbacks
	FReply OnInjectPulseClicked();
	FReply OnInjectRandomPulseClicked();

	// Stress test callbacks
	FReply OnStartStressTestClicked();
	FReply OnStopStressTestClicked();

	// Connection sim callbacks
	FReply OnSimulateDisconnectClicked();
	FReply OnSimulateReconnectClicked();
	FReply OnSimulateLatencyClicked();
	FReply OnResetConnectionClicked();

	// Validation operations
	void ValidateTargets();
	void ValidateBindings();
	void ValidateMaterials();
	void ValidateLiveLink();
	void AddIssue(ERshipTestSeverity Severity, const FString& Category, const FString& Message, const FString& Details = TEXT(""), const FString& Fix = TEXT(""));

	// Cached UI elements
	TSharedPtr<STextBlock> ValidationStatusText;
	TSharedPtr<STextBlock> IssueCountText;
	TSharedPtr<STextBlock> SelectedIssueText;
	TSharedPtr<STextBlock> StressTestStatusText;
	TSharedPtr<STextBlock> ConnectionStatusText;

	// Mock pulse inputs
	TSharedPtr<SEditableTextBox> TargetIdInput;
	TSharedPtr<SEditableTextBox> EmitterIdInput;
	TSharedPtr<SEditableTextBox> PulseDataInput;

	// Stress test inputs
	TSharedPtr<SEditableTextBox> PulsesPerSecondInput;
	TSharedPtr<SEditableTextBox> StressDurationInput;

	// Connection sim inputs
	TSharedPtr<SEditableTextBox> LatencyMsInput;

	// Issues list
	TArray<TSharedPtr<FRshipTestPanelIssue>> Issues;
	TSharedPtr<SListView<TSharedPtr<FRshipTestPanelIssue>>> IssuesListView;
	TSharedPtr<FRshipTestPanelIssue> SelectedIssue;

	// Stress test state
	bool bStressTestRunning;
	int32 StressTestPulsesPerSecond;
	float StressTestDuration;
	float StressTestElapsed;
	int32 TotalPulsesSent;

	// Connection sim state
	bool bSimulatingDisconnect;
	float SimulatedLatencyMs;

	// Refresh timing
	float TimeSinceLastRefresh;
	static constexpr float RefreshInterval = 0.5f;

	// Test utilities instance (prevent GC via AddToRoot)
	TWeakObjectPtr<URshipTestUtilities> TestUtilities;

	// Get or create test utilities
	URshipTestUtilities* GetTestUtilities();
};

/**
 * Row widget for validation issues list
 */
class SRshipTestPanelIssueRow : public SMultiColumnTableRow<TSharedPtr<FRshipTestPanelIssue>>
{
public:
	SLATE_BEGIN_ARGS(SRshipTestPanelIssueRow) {}
		SLATE_ARGUMENT(TSharedPtr<FRshipTestPanelIssue>, Item)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
	TSharedPtr<FRshipTestPanelIssue> Item;
};
