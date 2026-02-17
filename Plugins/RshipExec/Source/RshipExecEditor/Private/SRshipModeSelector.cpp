// Copyright Rocketship. All Rights Reserved.

#include "SRshipModeSelector.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SRshipModeSelector"

void SRshipModeSelector::BuildModeItems()
{
	ModeItems =
	{
		MakeShared<FString>(TEXT("direct")),
		MakeShared<FString>(TEXT("feed")),
		MakeShared<FString>(TEXT("perspective")),
		MakeShared<FString>(TEXT("custom-matrix")),
		MakeShared<FString>(TEXT("cylindrical")),
		MakeShared<FString>(TEXT("spherical")),
		MakeShared<FString>(TEXT("parallel")),
		MakeShared<FString>(TEXT("radial")),
		MakeShared<FString>(TEXT("mesh")),
		MakeShared<FString>(TEXT("fisheye")),
		MakeShared<FString>(TEXT("camera-plate")),
		MakeShared<FString>(TEXT("spatial")),
		MakeShared<FString>(TEXT("depth-map"))
	};

	ModeLabels.Empty();
	ModeLabels.Add(TEXT("direct"), LOCTEXT("MapModeDirectLabel", "Direct"));
	ModeLabels.Add(TEXT("feed"), LOCTEXT("MapModeFeedLabel", "Feed"));
	ModeLabels.Add(TEXT("perspective"), LOCTEXT("MapModePerspectiveLabel", "Perspective"));
	ModeLabels.Add(TEXT("custom-matrix"), LOCTEXT("MapModeCustomMatrixLabel", "Custom Matrix"));
	ModeLabels.Add(TEXT("cylindrical"), LOCTEXT("MapModeCylLabel", "Cylindrical"));
	ModeLabels.Add(TEXT("spherical"), LOCTEXT("MapModeSphericalLabel", "Spherical"));
	ModeLabels.Add(TEXT("parallel"), LOCTEXT("MapModeParallelLabel", "Parallel"));
	ModeLabels.Add(TEXT("radial"), LOCTEXT("MapModeRadialLabel", "Radial"));
	ModeLabels.Add(TEXT("mesh"), LOCTEXT("MapModeMeshLabel", "Mesh"));
	ModeLabels.Add(TEXT("fisheye"), LOCTEXT("MapModeFisheyeLabel", "Fisheye"));
	ModeLabels.Add(TEXT("camera-plate"), LOCTEXT("MapModeCameraPlateLabel", "Camera Plate"));
	ModeLabels.Add(TEXT("spatial"), LOCTEXT("MapModeSpatialLabel", "Spatial"));
	ModeLabels.Add(TEXT("depth-map"), LOCTEXT("MapModeDepthMapLabel", "Depth Map"));
}

FText SRshipModeSelector::GetModeLabel(const FString& Mode) const
{
	if (const FText* Found = ModeLabels.Find(Mode))
	{
		return *Found;
	}
	return LOCTEXT("MapModeUnknownLabel", "Perspective");
}

TSharedPtr<FString> SRshipModeSelector::FindItemForMode(const FString& Mode) const
{
	for (const TSharedPtr<FString>& Item : ModeItems)
	{
		if (Item.IsValid() && Item->Equals(Mode))
		{
			return Item;
		}
	}
	return nullptr;
}

void SRshipModeSelector::Construct(const FArguments& InArgs)
{
	OnModeSelected = InArgs._OnModeSelected;
	BuildModeItems();

	SelectedModeItem = FindItemForMode(SelectedMode);
	if (!SelectedModeItem.IsValid())
	{
		SelectedModeItem = ModeItems.Num() > 0 ? ModeItems[0] : nullptr;
		SelectedMode = SelectedModeItem.IsValid() ? *SelectedModeItem : TEXT("direct");
	}

	ChildSlot
	[
		SAssignNew(ModeCombo, SComboBox<TSharedPtr<FString>>)
		.OptionsSource(&ModeItems)
		.InitiallySelectedItem(SelectedModeItem)
		.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Selected, ESelectInfo::Type)
		{
			if (!Selected.IsValid())
			{
				return;
			}
			SelectedMode = *Selected;
			SelectedModeItem = Selected;
			OnModeSelected.ExecuteIfBound(SelectedMode);
		})
		.OnGenerateWidget_Lambda([this](TSharedPtr<FString> Item)
		{
			return SNew(STextBlock)
				.Text(Item.IsValid() ? GetModeLabel(*Item) : FText::GetEmpty())
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 7));
		})
		[
			SNew(STextBlock)
				.Text_Lambda([this]() { return GetModeLabel(SelectedMode); })
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 7))
		]
	];
}

void SRshipModeSelector::SetSelectedMode(const FString& InMode)
{
	const FString NormalizedMode = InMode.IsEmpty() ? TEXT("direct") : InMode;
	SelectedMode = NormalizedMode;
	SelectedModeItem = FindItemForMode(SelectedMode);
	if (!SelectedModeItem.IsValid() && ModeItems.Num() > 0)
	{
		SelectedModeItem = ModeItems[0];
		SelectedMode = *SelectedModeItem;
	}
	if (ModeCombo.IsValid())
	{
		ModeCombo->SetSelectedItem(SelectedModeItem);
	}
}

#undef LOCTEXT_NAMESPACE
