// Copyright Rocketship. All Rights Reserved.

#include "SRshipFixturePanel.h"
#include "RshipSubsystem.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "SRshipFixturePanel"

void SRshipFixturePanel::Construct(const FArguments& InArgs)
{
	CurrentQuality = ERshipFixtureVizQuality::Medium;
	BeamIntensity = 1.0f;
	BeamLength = 10.0f;
	VolumetricDensity = 0.5f;
	bEnableGobo = true;
	bEnableIES = true;
	bEnableColorTemp = true;
	TimeSinceLastRefresh = 0.0f;

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			// Library Section
			+ SVerticalBox::Slot()
			.FillHeight(0.5f)
			.Padding(0, 0, 0, 8)
			[
				BuildLibrarySection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Details Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				BuildDetailsSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Visualization Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				BuildVisualizationSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Beam Settings
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				BuildBeamSettingsSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Performance Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				BuildPerformanceSection()
			]
		]
	];

	// Build initial fixture library
	RefreshFixtureLibrary();
}

void SRshipFixturePanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	TimeSinceLastRefresh += InDeltaTime;
	if (TimeSinceLastRefresh >= RefreshInterval)
	{
		TimeSinceLastRefresh = 0.0f;
		RefreshStatus();
	}
}

TSharedRef<SWidget> SRshipFixturePanel::BuildLibrarySection()
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
				.Text(LOCTEXT("LibraryLabel", "Fixture Library"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SAssignNew(FixtureCountText, STextBlock)
				.Text(LOCTEXT("FixtureCount", "0 fixtures"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0, 0, 8, 0)
			[
				SNew(SSearchBox)
				.HintText(LOCTEXT("SearchHint", "Search fixtures..."))
				.OnTextChanged_Lambda([this](const FText& NewText)
				{
					// TODO: Filter fixture tree based on search
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 4, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("RefreshBtn", "Refresh"))
				.OnClicked(this, &SRshipFixturePanel::OnRefreshLibraryClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("SyncBtn", "Sync"))
				.ToolTipText(LOCTEXT("SyncTooltip", "Sync GDTF files from asset store"))
				.OnClicked(this, &SRshipFixturePanel::OnSyncFromAssetStoreClicked)
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(0, 4, 0, 0)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SAssignNew(FixtureTreeView, STreeView<TSharedPtr<FRshipFixtureItem>>)
				.TreeItemsSource(&RootFixtureItems)
				.OnGenerateRow(this, &SRshipFixturePanel::OnGenerateFixtureRow)
				.OnGetChildren(this, &SRshipFixturePanel::OnGetFixtureChildren)
				.OnSelectionChanged(this, &SRshipFixturePanel::OnFixtureSelectionChanged)
				.OnMouseButtonDoubleClick(this, &SRshipFixturePanel::OnFixtureDoubleClick)
				.SelectionMode(ESelectionMode::Single)
			]
		];
}

TSharedRef<SWidget> SRshipFixturePanel::BuildDetailsSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("DetailsLabel", "Fixture Details"))
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
				.Padding(0, 0, 0, 4)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0, 0, 8, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SelectedLabel", "Selected:"))
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SAssignNew(SelectedFixtureText, STextBlock)
						.Text(LOCTEXT("NoneSelected", "(none)"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(FixtureDetailsText, STextBlock)
					.Text(LOCTEXT("SelectFixture", "Select a fixture to view details"))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.AutoWrapText(true)
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 8, 0, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("SpawnBtn", "Spawn Fixture in Level"))
			.HAlign(HAlign_Center)
			.OnClicked(this, &SRshipFixturePanel::OnSpawnFixtureClicked)
			.IsEnabled_Lambda([this]() { return SelectedFixture.IsValid() && !SelectedFixture->bIsCategory; })
		];
}

TSharedRef<SWidget> SRshipFixturePanel::BuildVisualizationSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("VisualizationLabel", "Visualization Quality"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(SSegmentedControl<int32>)
			.OnValueChanged(this, &SRshipFixturePanel::OnQualityChanged)
			+ SSegmentedControl<int32>::Slot(0)
			.Text(LOCTEXT("QualityLow", "Low"))
			.ToolTip(LOCTEXT("QualityLowTooltip", "Basic beam rendering"))
			+ SSegmentedControl<int32>::Slot(1)
			.Text(LOCTEXT("QualityMedium", "Medium"))
			.ToolTip(LOCTEXT("QualityMediumTooltip", "Volumetric beams with basic effects"))
			+ SSegmentedControl<int32>::Slot(2)
			.Text(LOCTEXT("QualityHigh", "High"))
			.ToolTip(LOCTEXT("QualityHighTooltip", "Full volumetric with gobo and IES"))
			+ SSegmentedControl<int32>::Slot(3)
			.Text(LOCTEXT("QualityUltra", "Ultra"))
			.ToolTip(LOCTEXT("QualityUltraTooltip", "Ray-traced with all effects"))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 8, 0, 0)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 16, 0)
			[
				SAssignNew(EnableGoboCheckbox, SCheckBox)
				.IsChecked(bEnableGobo ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
				{
					bEnableGobo = (NewState == ECheckBoxState::Checked);
				})
				[
					SNew(STextBlock)
					.Text(LOCTEXT("EnableGobo", "Gobo Projection"))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 16, 0)
			[
				SAssignNew(EnableIESCheckbox, SCheckBox)
				.IsChecked(bEnableIES ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
				{
					bEnableIES = (NewState == ECheckBoxState::Checked);
				})
				[
					SNew(STextBlock)
					.Text(LOCTEXT("EnableIES", "IES Profiles"))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(EnableColorTempCheckbox, SCheckBox)
				.IsChecked(bEnableColorTemp ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
				{
					bEnableColorTemp = (NewState == ECheckBoxState::Checked);
				})
				[
					SNew(STextBlock)
					.Text(LOCTEXT("EnableColorTemp", "Color Temperature"))
				]
			]
		];
}

TSharedRef<SWidget> SRshipFixturePanel::BuildBeamSettingsSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("BeamSettingsLabel", "Beam Settings"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		// Beam Intensity
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
				SNew(SBox)
				.WidthOverride(120.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BeamIntensityLabel", "Beam Intensity:"))
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(BeamIntensitySlider, SSlider)
				.Value(BeamIntensity)
				.OnValueChanged_Lambda([this](float NewValue)
				{
					BeamIntensity = NewValue;
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8, 0, 0, 0)
			[
				SNew(SBox)
				.WidthOverride(50.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						return FText::Format(LOCTEXT("IntensityPercent", "{0}%"), FMath::RoundToInt(BeamIntensity * 100));
					})
				]
			]
		]

		// Beam Length
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
				SNew(SBox)
				.WidthOverride(120.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BeamLengthLabel", "Beam Length:"))
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(BeamLengthSlider, SSlider)
				.Value(BeamLength / 50.0f) // Normalize to 0-1 for slider
				.OnValueChanged_Lambda([this](float NewValue)
				{
					BeamLength = NewValue * 50.0f;
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8, 0, 0, 0)
			[
				SNew(SBox)
				.WidthOverride(50.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						return FText::Format(LOCTEXT("LengthMeters", "{0}m"), FMath::RoundToInt(BeamLength));
					})
				]
			]
		]

		// Volumetric Density
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 8, 0)
			[
				SNew(SBox)
				.WidthOverride(120.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("VolumetricLabel", "Volumetric Density:"))
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(VolumetricDensitySlider, SSlider)
				.Value(VolumetricDensity)
				.OnValueChanged_Lambda([this](float NewValue)
				{
					VolumetricDensity = NewValue;
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8, 0, 0, 0)
			[
				SNew(SBox)
				.WidthOverride(50.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						return FText::Format(LOCTEXT("DensityPercent", "{0}%"), FMath::RoundToInt(VolumetricDensity * 100));
					})
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
				.Text(LOCTEXT("ApplyBtn", "Apply Settings"))
				.OnClicked(this, &SRshipFixturePanel::OnApplyVizSettingsClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("ResetBtn", "Reset to Defaults"))
				.OnClicked(this, &SRshipFixturePanel::OnResetVizSettingsClicked)
			]
		];
}

TSharedRef<SWidget> SRshipFixturePanel::BuildPerformanceSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PerformanceLabel", "Performance"))
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
						.Text(LOCTEXT("ActiveFixturesLabel", "Active Fixtures:"))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(ActiveFixturesText, STextBlock)
						.Text(LOCTEXT("ActiveFixturesValue", "0"))
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 4, 0, 0)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("RenderCostLabel", "Estimated Render Cost:"))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(PerformanceText, STextBlock)
						.Text(LOCTEXT("PerformanceValue", "Low"))
						.ColorAndOpacity(FLinearColor::Green)
					]
				]
			]
		];
}

TSharedRef<ITableRow> SRshipFixturePanel::OnGenerateFixtureRow(TSharedPtr<FRshipFixtureItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SRshipFixtureRow, OwnerTable)
		.Item(Item);
}

void SRshipFixturePanel::OnGetFixtureChildren(TSharedPtr<FRshipFixtureItem> Item, TArray<TSharedPtr<FRshipFixtureItem>>& OutChildren)
{
	if (Item.IsValid())
	{
		OutChildren = Item->Children;
	}
}

void SRshipFixturePanel::OnFixtureSelectionChanged(TSharedPtr<FRshipFixtureItem> Item, ESelectInfo::Type SelectInfo)
{
	SelectedFixture = Item;

	if (Item.IsValid())
	{
		SelectedFixtureText->SetText(FText::FromString(Item->Name));

		if (Item->bIsCategory)
		{
			FixtureDetailsText->SetText(FText::Format(
				LOCTEXT("CategoryDetails", "Category: {0}\n{1} fixtures"),
				FText::FromString(Item->Name),
				Item->Children.Num()));
		}
		else
		{
			FixtureDetailsText->SetText(FText::Format(
				LOCTEXT("FixtureDetails", "Manufacturer: {0}\nType: {1}\nGDTF: {2}"),
				FText::FromString(Item->Manufacturer),
				FText::FromString(Item->Type),
				FText::FromString(Item->GdtfFile)));
		}
	}
	else
	{
		SelectedFixtureText->SetText(LOCTEXT("NoneSelected", "(none)"));
		FixtureDetailsText->SetText(LOCTEXT("SelectFixture", "Select a fixture to view details"));
	}
}

void SRshipFixturePanel::OnFixtureDoubleClick(TSharedPtr<FRshipFixtureItem> Item)
{
	if (Item.IsValid() && !Item->bIsCategory)
	{
		OnSpawnFixtureClicked();
	}
}

FReply SRshipFixturePanel::OnRefreshLibraryClicked()
{
	RefreshFixtureLibrary();
	return FReply::Handled();
}

FReply SRshipFixturePanel::OnSyncFromAssetStoreClicked()
{
	// TODO: Trigger sync from RshipAssetStoreClient (Phase 5)
	return FReply::Handled();
}

FReply SRshipFixturePanel::OnSpawnFixtureClicked()
{
	// TODO: Spawn fixture visualizer actor in level
	return FReply::Handled();
}

FReply SRshipFixturePanel::OnApplyVizSettingsClicked()
{
	// TODO: Apply visualization settings to all fixture visualizers
	return FReply::Handled();
}

FReply SRshipFixturePanel::OnResetVizSettingsClicked()
{
	CurrentQuality = ERshipFixtureVizQuality::Medium;
	BeamIntensity = 1.0f;
	BeamLength = 10.0f;
	VolumetricDensity = 0.5f;
	bEnableGobo = true;
	bEnableIES = true;
	bEnableColorTemp = true;

	// Update UI
	if (BeamIntensitySlider.IsValid()) BeamIntensitySlider->SetValue(BeamIntensity);
	if (BeamLengthSlider.IsValid()) BeamLengthSlider->SetValue(BeamLength / 50.0f);
	if (VolumetricDensitySlider.IsValid()) VolumetricDensitySlider->SetValue(VolumetricDensity);
	if (EnableGoboCheckbox.IsValid()) EnableGoboCheckbox->SetIsChecked(bEnableGobo ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	if (EnableIESCheckbox.IsValid()) EnableIESCheckbox->SetIsChecked(bEnableIES ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	if (EnableColorTempCheckbox.IsValid()) EnableColorTempCheckbox->SetIsChecked(bEnableColorTemp ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);

	return FReply::Handled();
}

void SRshipFixturePanel::OnQualityChanged(int32 NewQuality)
{
	CurrentQuality = static_cast<ERshipFixtureVizQuality>(NewQuality);

	// Auto-adjust settings based on quality
	switch (CurrentQuality)
	{
	case ERshipFixtureVizQuality::Low:
		bEnableGobo = false;
		bEnableIES = false;
		VolumetricDensity = 0.0f;
		break;
	case ERshipFixtureVizQuality::Medium:
		bEnableGobo = true;
		bEnableIES = false;
		VolumetricDensity = 0.3f;
		break;
	case ERshipFixtureVizQuality::High:
		bEnableGobo = true;
		bEnableIES = true;
		VolumetricDensity = 0.5f;
		break;
	case ERshipFixtureVizQuality::Ultra:
		bEnableGobo = true;
		bEnableIES = true;
		VolumetricDensity = 1.0f;
		break;
	}

	// Update checkboxes
	if (EnableGoboCheckbox.IsValid()) EnableGoboCheckbox->SetIsChecked(bEnableGobo ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	if (EnableIESCheckbox.IsValid()) EnableIESCheckbox->SetIsChecked(bEnableIES ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	if (VolumetricDensitySlider.IsValid()) VolumetricDensitySlider->SetValue(VolumetricDensity);
}

void SRshipFixturePanel::RefreshFixtureLibrary()
{
	RootFixtureItems.Empty();

	// Build category tree with placeholder data
	// TODO: Load from actual GDTF files synced from asset store

	// Martin category
	auto MartinCategory = MakeShared<FRshipFixtureItem>();
	MartinCategory->Name = TEXT("Martin");
	MartinCategory->bIsCategory = true;

	auto MartinViper = MakeShared<FRshipFixtureItem>();
	MartinViper->Name = TEXT("MAC Viper Profile");
	MartinViper->Manufacturer = TEXT("Martin");
	MartinViper->Type = TEXT("Profile");
	MartinViper->GdtfFile = TEXT("Martin_MAC_Viper_Profile.gdtf");
	MartinCategory->Children.Add(MartinViper);

	auto MartinQuantum = MakeShared<FRshipFixtureItem>();
	MartinQuantum->Name = TEXT("MAC Quantum Wash");
	MartinQuantum->Manufacturer = TEXT("Martin");
	MartinQuantum->Type = TEXT("Wash");
	MartinQuantum->GdtfFile = TEXT("Martin_MAC_Quantum_Wash.gdtf");
	MartinCategory->Children.Add(MartinQuantum);

	RootFixtureItems.Add(MartinCategory);

	// Robe category
	auto RobeCategory = MakeShared<FRshipFixtureItem>();
	RobeCategory->Name = TEXT("Robe");
	RobeCategory->bIsCategory = true;

	auto RobeT1 = MakeShared<FRshipFixtureItem>();
	RobeT1->Name = TEXT("Robin T1 Profile");
	RobeT1->Manufacturer = TEXT("Robe");
	RobeT1->Type = TEXT("Profile");
	RobeT1->GdtfFile = TEXT("Robe_Robin_T1_Profile.gdtf");
	RobeCategory->Children.Add(RobeT1);

	auto RobeMegaPointe = MakeShared<FRshipFixtureItem>();
	RobeMegaPointe->Name = TEXT("MegaPointe");
	RobeMegaPointe->Manufacturer = TEXT("Robe");
	RobeMegaPointe->Type = TEXT("Beam");
	RobeMegaPointe->GdtfFile = TEXT("Robe_MegaPointe.gdtf");
	RobeCategory->Children.Add(RobeMegaPointe);

	RootFixtureItems.Add(RobeCategory);

	// Generic category
	auto GenericCategory = MakeShared<FRshipFixtureItem>();
	GenericCategory->Name = TEXT("Generic");
	GenericCategory->bIsCategory = true;

	auto GenericPar = MakeShared<FRshipFixtureItem>();
	GenericPar->Name = TEXT("PAR 64");
	GenericPar->Manufacturer = TEXT("Generic");
	GenericPar->Type = TEXT("Par");
	GenericPar->GdtfFile = TEXT("");
	GenericCategory->Children.Add(GenericPar);

	auto GenericFresnel = MakeShared<FRshipFixtureItem>();
	GenericFresnel->Name = TEXT("Fresnel 2kW");
	GenericFresnel->Manufacturer = TEXT("Generic");
	GenericFresnel->Type = TEXT("Fresnel");
	GenericFresnel->GdtfFile = TEXT("");
	GenericCategory->Children.Add(GenericFresnel);

	RootFixtureItems.Add(GenericCategory);

	// Count all fixtures
	int32 TotalCount = 0;
	for (const auto& Category : RootFixtureItems)
	{
		TotalCount += Category->Children.Num();
	}

	if (FixtureCountText.IsValid())
	{
		FixtureCountText->SetText(FText::Format(
			LOCTEXT("FixtureCountFmt", "{0} {0}|plural(one=fixture,other=fixtures)"),
			TotalCount));
	}

	if (FixtureTreeView.IsValid())
	{
		FixtureTreeView->RequestTreeRefresh();
	}
}

void SRshipFixturePanel::RefreshStatus()
{
	// TODO: Get actual counts from scene
	if (ActiveFixturesText.IsValid())
	{
		ActiveFixturesText->SetText(LOCTEXT("ActiveFixturesPlaceholder", "0"));
	}

	// Update performance estimate based on quality
	if (PerformanceText.IsValid())
	{
		switch (CurrentQuality)
		{
		case ERshipFixtureVizQuality::Low:
			PerformanceText->SetText(LOCTEXT("PerfLow", "Low"));
			PerformanceText->SetColorAndOpacity(FLinearColor::Green);
			break;
		case ERshipFixtureVizQuality::Medium:
			PerformanceText->SetText(LOCTEXT("PerfMedium", "Medium"));
			PerformanceText->SetColorAndOpacity(FLinearColor::Yellow);
			break;
		case ERshipFixtureVizQuality::High:
			PerformanceText->SetText(LOCTEXT("PerfHigh", "High"));
			PerformanceText->SetColorAndOpacity(FLinearColor(1.0f, 0.5f, 0.0f));
			break;
		case ERshipFixtureVizQuality::Ultra:
			PerformanceText->SetText(LOCTEXT("PerfUltra", "Very High"));
			PerformanceText->SetColorAndOpacity(FLinearColor::Red);
			break;
		}
	}
}

// ============================================================================
// SRshipFixtureRow
// ============================================================================

void SRshipFixtureRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	Item = InArgs._Item;

	STableRow<TSharedPtr<FRshipFixtureItem>>::ConstructInternal(
		STableRow::FArguments()
		.ShowSelection(true),
		InOwnerTableView);

	if (Item.IsValid())
	{
		ChildSlot
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4, 2)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush(Item->bIsCategory ? "Icons.FolderClosed" : "ClassIcon.Light"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(4, 2)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->Name))
				.Font(Item->bIsCategory ? FCoreStyle::GetDefaultFontStyle("Bold", 9) : FCoreStyle::GetDefaultFontStyle("Regular", 9))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4, 2)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->bIsCategory ? TEXT("") : Item->Type))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		];
	}
}

#undef LOCTEXT_NAMESPACE
