// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Input/SSlider.h"

/**
 * Fixture item for the library tree
 */
struct FRshipFixtureItem
{
	FString Name;
	FString Manufacturer;
	FString Type;           // Spot, Wash, Profile, etc.
	FString GdtfFile;       // Associated GDTF file
	bool bIsCategory;       // True if this is a folder/category
	TArray<TSharedPtr<FRshipFixtureItem>> Children;

	FRshipFixtureItem()
		: bIsCategory(false)
	{}
};

/**
 * Visualization quality settings
 */
enum class ERshipFixtureVizQuality : uint8
{
	Low,        // Basic beam, no gobo
	Medium,     // Volumetric beam, basic gobo
	High,       // Full volumetric, gobo, IES
	Ultra       // Ray-traced, full effects
};

/**
 * Fixture panel for browsing fixture library and configuring visualization
 *
 * Features:
 * - Browse available fixture types (from GDTF library)
 * - Configure visualization quality settings
 * - IES profile assignment
 * - Beam visualization options
 * - Gobo projection settings
 */
class SRshipFixturePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRshipFixturePanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	// UI Section builders
	TSharedRef<SWidget> BuildLibrarySection();
	TSharedRef<SWidget> BuildDetailsSection();
	TSharedRef<SWidget> BuildVisualizationSection();
	TSharedRef<SWidget> BuildBeamSettingsSection();
	TSharedRef<SWidget> BuildPerformanceSection();

	// Tree view callbacks
	TSharedRef<ITableRow> OnGenerateFixtureRow(TSharedPtr<FRshipFixtureItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetFixtureChildren(TSharedPtr<FRshipFixtureItem> Item, TArray<TSharedPtr<FRshipFixtureItem>>& OutChildren);
	void OnFixtureSelectionChanged(TSharedPtr<FRshipFixtureItem> Item, ESelectInfo::Type SelectInfo);
	void OnFixtureDoubleClick(TSharedPtr<FRshipFixtureItem> Item);

	// Button callbacks
	FReply OnRefreshLibraryClicked();
	FReply OnSyncFromAssetStoreClicked();
	FReply OnSpawnFixtureClicked();
	FReply OnApplyVizSettingsClicked();
	FReply OnResetVizSettingsClicked();

	// Quality change
	void OnQualityChanged(int32 NewQuality);

	// Data operations
	void RefreshFixtureLibrary();
	void RefreshStatus();
	void BuildCategoryTree();

	// Cached UI elements
	TSharedPtr<STextBlock> FixtureCountText;
	TSharedPtr<STextBlock> SelectedFixtureText;
	TSharedPtr<STextBlock> FixtureDetailsText;
	TSharedPtr<STextBlock> ActiveFixturesText;
	TSharedPtr<STextBlock> PerformanceText;
	TSharedPtr<SEditableTextBox> SearchBox;
	TSharedPtr<SSlider> BeamIntensitySlider;
	TSharedPtr<SSlider> BeamLengthSlider;
	TSharedPtr<SSlider> VolumetricDensitySlider;
	TSharedPtr<SCheckBox> EnableGoboCheckbox;
	TSharedPtr<SCheckBox> EnableIESCheckbox;
	TSharedPtr<SCheckBox> EnableColorTempCheckbox;

	// Fixture tree
	TArray<TSharedPtr<FRshipFixtureItem>> RootFixtureItems;
	TSharedPtr<STreeView<TSharedPtr<FRshipFixtureItem>>> FixtureTreeView;
	TSharedPtr<FRshipFixtureItem> SelectedFixture;

	// Current quality setting
	ERshipFixtureVizQuality CurrentQuality;

	// Visualization settings
	float BeamIntensity;
	float BeamLength;
	float VolumetricDensity;
	bool bEnableGobo;
	bool bEnableIES;
	bool bEnableColorTemp;

	// Refresh timing
	float TimeSinceLastRefresh;
	static constexpr float RefreshInterval = 2.0f;
};

/**
 * Row widget for fixture tree
 */
class SRshipFixtureRow : public STableRow<TSharedPtr<FRshipFixtureItem>>
{
public:
	SLATE_BEGIN_ARGS(SRshipFixtureRow) {}
		SLATE_ARGUMENT(TSharedPtr<FRshipFixtureItem>, Item)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

private:
	TSharedPtr<FRshipFixtureItem> Item;
};
