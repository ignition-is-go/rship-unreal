// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"

DECLARE_DELEGATE_OneParam(FOnModeSelected, const FString& /*Mode*/);

class SRshipModeSelector : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRshipModeSelector) {}
		SLATE_EVENT(FOnModeSelected, OnModeSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetSelectedMode(const FString& InMode);
	FString GetSelectedMode() const { return SelectedMode; }

private:
	FText GetModeLabel(const FString& Mode) const;
	void BuildModeItems();
	TSharedPtr<FString> FindItemForMode(const FString& Mode) const;

	FString SelectedMode = TEXT("direct");
	FOnModeSelected OnModeSelected;
	TArray<TSharedPtr<FString>> ModeItems;
	TMap<FString, FText> ModeLabels;
	TSharedPtr<FString> SelectedModeItem;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> ModeCombo;
};
