// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

/**
 * Material parameter binding item for the list view
 */
struct FRshipMaterialParameterItem
{
	FName ParameterName;
	FString ParameterType;    // Scalar, Vector, Texture
	FString CurrentValue;     // Current value as string
	FString BoundEmitterId;   // Mapped emitter ID (if bound)
	bool bIsBound;

	FRshipMaterialParameterItem()
		: bIsBound(false)
	{}
};

/**
 * Material preset item
 */
struct FRshipMaterialPresetItem
{
	FString PresetName;
	TMap<FName, float> ScalarValues;
	TMap<FName, FLinearColor> VectorValues;
};

/**
 * Material panel for managing Substrate material bindings and rship integration
 *
 * Features:
 * - View available materials in scene
 * - Detect Substrate-enabled materials
 * - Bind material parameters to rship emitters
 * - Manage material presets
 * - Test parameter transitions
 */
class SRshipMaterialPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRshipMaterialPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	// UI Section builders
	TSharedRef<SWidget> BuildMaterialSelectionSection();
	TSharedRef<SWidget> BuildSubstrateInfoSection();
	TSharedRef<SWidget> BuildParametersSection();
	TSharedRef<SWidget> BuildBindingsSection();
	TSharedRef<SWidget> BuildPresetsSection();
	TSharedRef<SWidget> BuildTestSection();

	// List view callbacks
	TSharedRef<ITableRow> OnGenerateParameterRow(TSharedPtr<FRshipMaterialParameterItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnParameterSelectionChanged(TSharedPtr<FRshipMaterialParameterItem> Item, ESelectInfo::Type SelectInfo);

	// Material selection
	TSharedRef<SWidget> OnGenerateMaterialWidget(TSharedPtr<FString> InItem);
	void OnMaterialSelected(TSharedPtr<FString> InItem, ESelectInfo::Type SelectInfo);
	FText GetSelectedMaterialText() const;

	// Button callbacks
	FReply OnRefreshMaterialsClicked();
	FReply OnBindParameterClicked();
	FReply OnUnbindParameterClicked();
	FReply OnBindAllClicked();
	FReply OnClearAllBindingsClicked();
	FReply OnSavePresetClicked();
	FReply OnLoadPresetClicked();
	FReply OnDeletePresetClicked();
	FReply OnTestTransitionClicked();

	// Data refresh
	void RefreshMaterialList();
	void RefreshParameterList();
	void RefreshStatus();

	// Helpers
	bool IsSubstrateMaterial(class UMaterialInterface* Material) const;
	void CollectMaterialParameters(class UMaterialInterface* Material);

	// Cached UI elements
	TSharedPtr<STextBlock> SubstrateStatusText;
	TSharedPtr<STextBlock> ParameterCountText;
	TSharedPtr<STextBlock> BoundCountText;
	TSharedPtr<STextBlock> SelectedParameterText;
	TSharedPtr<SEditableTextBox> EmitterIdInput;
	TSharedPtr<SEditableTextBox> PresetNameInput;
	TSharedPtr<SEditableTextBox> TransitionDurationInput;

	// Material list
	TArray<TSharedPtr<FString>> MaterialOptions;
	TSharedPtr<FString> SelectedMaterial;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> MaterialComboBox;

	// Parameter list
	TArray<TSharedPtr<FRshipMaterialParameterItem>> ParameterItems;
	TSharedPtr<SListView<TSharedPtr<FRshipMaterialParameterItem>>> ParameterListView;
	TSharedPtr<FRshipMaterialParameterItem> SelectedParameter;

	// Preset list
	TArray<TSharedPtr<FString>> PresetOptions;
	TSharedPtr<FString> SelectedPreset;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> PresetComboBox;

	// Currently selected material instance
	TWeakObjectPtr<class UMaterialInterface> CurrentMaterial;

	// Refresh timing
	float TimeSinceLastRefresh;
	static constexpr float RefreshInterval = 1.0f; // 1Hz refresh
};

/**
 * Row widget for material parameter list
 */
class SRshipMaterialParameterRow : public SMultiColumnTableRow<TSharedPtr<FRshipMaterialParameterItem>>
{
public:
	SLATE_BEGIN_ARGS(SRshipMaterialParameterRow) {}
		SLATE_ARGUMENT(TSharedPtr<FRshipMaterialParameterItem>, Item)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
	TSharedPtr<FRshipMaterialParameterItem> Item;
};
