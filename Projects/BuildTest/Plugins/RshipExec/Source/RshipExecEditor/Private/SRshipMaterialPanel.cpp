// Copyright Rocketship. All Rights Reserved.

#include "SRshipMaterialPanel.h"
#include "RshipSubsystem.h"
#include "RshipSubstrateMaterialBinding.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/MeshComponent.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorActorFolders.h"
#endif

#define LOCTEXT_NAMESPACE "SRshipMaterialPanel"

void SRshipMaterialPanel::Construct(const FArguments& InArgs)
{
	TimeSinceLastRefresh = 0.0f;

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			// Material Selection
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				BuildMaterialSelectionSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Substrate Info
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				BuildSubstrateInfoSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Parameters List
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0, 0, 0, 8)
			[
				BuildParametersSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Bindings Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				BuildBindingsSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Presets Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				BuildPresetsSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			// Test Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				BuildTestSection()
			]
		]
	];

	// Initial data load
	RefreshMaterialList();
}

void SRshipMaterialPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	TimeSinceLastRefresh += InDeltaTime;
	if (TimeSinceLastRefresh >= RefreshInterval)
	{
		TimeSinceLastRefresh = 0.0f;
		RefreshStatus();
	}
}

TSharedRef<SWidget> SRshipMaterialPanel::BuildMaterialSelectionSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("MaterialSelectionLabel", "Material Selection"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0, 0, 8, 0)
			[
				SAssignNew(MaterialComboBox, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&MaterialOptions)
				.OnGenerateWidget(this, &SRshipMaterialPanel::OnGenerateMaterialWidget)
				.OnSelectionChanged(this, &SRshipMaterialPanel::OnMaterialSelected)
				[
					SNew(STextBlock)
					.Text(this, &SRshipMaterialPanel::GetSelectedMaterialText)
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("RefreshMaterialsBtn", "Refresh"))
				.OnClicked(this, &SRshipMaterialPanel::OnRefreshMaterialsClicked)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4, 0, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("MaterialSelectionHelp", "Select a material from the current level to configure bindings"))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
}

TSharedRef<SWidget> SRshipMaterialPanel::BuildSubstrateInfoSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SubstrateInfoLabel", "Substrate Status"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(8.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SubstrateEnabledLabel", "Substrate Enabled:"))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(SubstrateStatusText, STextBlock)
					.Text(LOCTEXT("SubstrateUnknown", "No material selected"))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4, 0, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SubstrateHelp", "Substrate materials (UE 5.5+) support advanced shading parameters"))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
}

TSharedRef<SWidget> SRshipMaterialPanel::BuildParametersSection()
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
				.Text(LOCTEXT("ParametersLabel", "Material Parameters"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SAssignNew(ParameterCountText, STextBlock)
				.Text(LOCTEXT("ParameterCount", "0 parameters"))
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
				SAssignNew(ParameterListView, SListView<TSharedPtr<FRshipMaterialParameterItem>>)
				.ListItemsSource(&ParameterItems)
				.OnGenerateRow(this, &SRshipMaterialPanel::OnGenerateParameterRow)
				.OnSelectionChanged(this, &SRshipMaterialPanel::OnParameterSelectionChanged)
				.SelectionMode(ESelectionMode::Single)
				.HeaderRow
				(
					SNew(SHeaderRow)

					+ SHeaderRow::Column("Name")
					.DefaultLabel(LOCTEXT("ColName", "Parameter"))
					.FillWidth(0.3f)

					+ SHeaderRow::Column("Type")
					.DefaultLabel(LOCTEXT("ColType", "Type"))
					.FillWidth(0.15f)

					+ SHeaderRow::Column("Value")
					.DefaultLabel(LOCTEXT("ColValue", "Current Value"))
					.FillWidth(0.25f)

					+ SHeaderRow::Column("EmitterId")
					.DefaultLabel(LOCTEXT("ColEmitter", "Bound Emitter"))
					.FillWidth(0.2f)

					+ SHeaderRow::Column("Status")
					.DefaultLabel(LOCTEXT("ColStatus", "Status"))
					.FillWidth(0.1f)
				)
			]
		];
}

TSharedRef<SWidget> SRshipMaterialPanel::BuildBindingsSection()
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
				.Text(LOCTEXT("BindingsLabel", "Parameter Binding"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SAssignNew(BoundCountText, STextBlock)
				.Text(LOCTEXT("BoundCount", "0 bound"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
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
				.Text(LOCTEXT("SelectedParamLabel", "Selected:"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(SelectedParameterText, STextBlock)
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
			.Padding(0, 0, 4, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("BindBtn", "Bind"))
				.OnClicked(this, &SRshipMaterialPanel::OnBindParameterClicked)
				.IsEnabled_Lambda([this]() { return SelectedParameter.IsValid(); })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("UnbindBtn", "Unbind"))
				.OnClicked(this, &SRshipMaterialPanel::OnUnbindParameterClicked)
				.IsEnabled_Lambda([this]() { return SelectedParameter.IsValid() && SelectedParameter->bIsBound; })
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
				.Text(LOCTEXT("BindAllBtn", "Bind All Parameters"))
				.ToolTipText(LOCTEXT("BindAllTooltip", "Auto-generate emitter IDs for all parameters"))
				.OnClicked(this, &SRshipMaterialPanel::OnBindAllClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("ClearAllBtn", "Clear All Bindings"))
				.OnClicked(this, &SRshipMaterialPanel::OnClearAllBindingsClicked)
			]
		];
}

TSharedRef<SWidget> SRshipMaterialPanel::BuildPresetsSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PresetsLabel", "Material Presets"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0, 0, 8, 0)
			[
				SAssignNew(PresetComboBox, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&PresetOptions)
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
				{
					return SNew(STextBlock).Text(FText::FromString(*Item));
				})
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo)
				{
					SelectedPreset = Item;
				})
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						return SelectedPreset.IsValid() ? FText::FromString(*SelectedPreset) : LOCTEXT("SelectPreset", "Select Preset...");
					})
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 4, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("LoadPresetBtn", "Load"))
				.OnClicked(this, &SRshipMaterialPanel::OnLoadPresetClicked)
				.IsEnabled_Lambda([this]() { return SelectedPreset.IsValid(); })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("DeletePresetBtn", "Delete"))
				.OnClicked(this, &SRshipMaterialPanel::OnDeletePresetClicked)
				.IsEnabled_Lambda([this]() { return SelectedPreset.IsValid(); })
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 8, 0, 0)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0, 0, 8, 0)
			[
				SAssignNew(PresetNameInput, SEditableTextBox)
				.HintText(LOCTEXT("PresetNameHint", "New preset name"))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("SavePresetBtn", "Save Current as Preset"))
				.OnClicked(this, &SRshipMaterialPanel::OnSavePresetClicked)
			]
		];
}

TSharedRef<SWidget> SRshipMaterialPanel::BuildTestSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TestLabel", "Testing"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 8, 0)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TransitionDurationLabel", "Transition Duration (s):"))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 8, 0)
			[
				SNew(SBox)
				.WidthOverride(80.0f)
				[
					SAssignNew(TransitionDurationInput, SEditableTextBox)
					.Text(LOCTEXT("DefaultDuration", "1.0"))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("TestTransitionBtn", "Test Transition"))
				.ToolTipText(LOCTEXT("TestTransitionTooltip", "Smoothly transition to a random parameter state"))
				.OnClicked(this, &SRshipMaterialPanel::OnTestTransitionClicked)
				.IsEnabled_Lambda([this]() { return CurrentMaterial.IsValid(); })
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4, 0, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TestHelp", "Use transitions to smoothly interpolate between material states"))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
}

TSharedRef<ITableRow> SRshipMaterialPanel::OnGenerateParameterRow(TSharedPtr<FRshipMaterialParameterItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SRshipMaterialParameterRow, OwnerTable)
		.Item(Item);
}

void SRshipMaterialPanel::OnParameterSelectionChanged(TSharedPtr<FRshipMaterialParameterItem> Item, ESelectInfo::Type SelectInfo)
{
	SelectedParameter = Item;

	if (Item.IsValid())
	{
		SelectedParameterText->SetText(FText::FromName(Item->ParameterName));
		EmitterIdInput->SetText(FText::FromString(Item->BoundEmitterId));
	}
	else
	{
		SelectedParameterText->SetText(LOCTEXT("NoneSelected", "(none)"));
		EmitterIdInput->SetText(FText::GetEmpty());
	}
}

TSharedRef<SWidget> SRshipMaterialPanel::OnGenerateMaterialWidget(TSharedPtr<FString> InItem)
{
	return SNew(STextBlock).Text(FText::FromString(*InItem));
}

void SRshipMaterialPanel::OnMaterialSelected(TSharedPtr<FString> InItem, ESelectInfo::Type SelectInfo)
{
	SelectedMaterial = InItem;

	// Find the actual material
	CurrentMaterial = nullptr;

	if (InItem.IsValid())
	{
#if WITH_EDITOR
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (World)
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				AActor* Actor = *It;
				TArray<UMeshComponent*> MeshComponents;
				Actor->GetComponents<UMeshComponent>(MeshComponents);

				for (UMeshComponent* MeshComp : MeshComponents)
				{
					for (int32 i = 0; i < MeshComp->GetNumMaterials(); i++)
					{
						UMaterialInterface* Mat = MeshComp->GetMaterial(i);
						if (Mat && Mat->GetName() == *InItem)
						{
							CurrentMaterial = Mat;
							break;
						}
					}
					if (CurrentMaterial.IsValid()) break;
				}
				if (CurrentMaterial.IsValid()) break;
			}
		}
#endif

		RefreshParameterList();

		// Update Substrate status
		if (SubstrateStatusText.IsValid() && CurrentMaterial.IsValid())
		{
			bool bIsSubstrate = IsSubstrateMaterial(CurrentMaterial.Get());
			SubstrateStatusText->SetText(bIsSubstrate ?
				LOCTEXT("SubstrateYes", "Yes - Advanced parameters available") :
				LOCTEXT("SubstrateNo", "No - Standard material"));
			SubstrateStatusText->SetColorAndOpacity(bIsSubstrate ? FLinearColor::Green : FLinearColor::Yellow);
		}
	}
	else
	{
		ParameterItems.Empty();
		if (ParameterListView.IsValid())
		{
			ParameterListView->RequestListRefresh();
		}

		if (SubstrateStatusText.IsValid())
		{
			SubstrateStatusText->SetText(LOCTEXT("SubstrateUnknown", "No material selected"));
			SubstrateStatusText->SetColorAndOpacity(FSlateColor::UseSubduedForeground());
		}
	}
}

FText SRshipMaterialPanel::GetSelectedMaterialText() const
{
	return SelectedMaterial.IsValid() ? FText::FromString(*SelectedMaterial) : LOCTEXT("SelectMaterial", "Select Material...");
}

FReply SRshipMaterialPanel::OnRefreshMaterialsClicked()
{
	RefreshMaterialList();
	return FReply::Handled();
}

FReply SRshipMaterialPanel::OnBindParameterClicked()
{
	if (SelectedParameter.IsValid())
	{
		SelectedParameter->BoundEmitterId = EmitterIdInput->GetText().ToString();
		SelectedParameter->bIsBound = !SelectedParameter->BoundEmitterId.IsEmpty();
		ParameterListView->RequestListRefresh();
		RefreshStatus();
	}
	return FReply::Handled();
}

FReply SRshipMaterialPanel::OnUnbindParameterClicked()
{
	if (SelectedParameter.IsValid())
	{
		SelectedParameter->BoundEmitterId.Empty();
		SelectedParameter->bIsBound = false;
		EmitterIdInput->SetText(FText::GetEmpty());
		ParameterListView->RequestListRefresh();
		RefreshStatus();
	}
	return FReply::Handled();
}

FReply SRshipMaterialPanel::OnBindAllClicked()
{
	if (!SelectedMaterial.IsValid()) return FReply::Handled();

	FString MaterialName = *SelectedMaterial;
	MaterialName = MaterialName.Replace(TEXT(" "), TEXT("_"));

	for (auto& Item : ParameterItems)
	{
		if (!Item->bIsBound)
		{
			// Generate emitter ID: material_paramname
			Item->BoundEmitterId = FString::Printf(TEXT("%s_%s"), *MaterialName, *Item->ParameterName.ToString());
			Item->bIsBound = true;
		}
	}

	ParameterListView->RequestListRefresh();
	RefreshStatus();
	return FReply::Handled();
}

FReply SRshipMaterialPanel::OnClearAllBindingsClicked()
{
	for (auto& Item : ParameterItems)
	{
		Item->BoundEmitterId.Empty();
		Item->bIsBound = false;
	}

	ParameterListView->RequestListRefresh();
	RefreshStatus();
	return FReply::Handled();
}

FReply SRshipMaterialPanel::OnSavePresetClicked()
{
	FString PresetName = PresetNameInput->GetText().ToString();
	if (PresetName.IsEmpty()) return FReply::Handled();

	// Check if preset already exists
	bool bExists = false;
	for (const auto& Option : PresetOptions)
	{
		if (*Option == PresetName)
		{
			bExists = true;
			break;
		}
	}

	if (!bExists)
	{
		PresetOptions.Add(MakeShared<FString>(PresetName));
	}

	// Get the Substrate manager and save the preset
#if WITH_EDITOR
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (World)
	{
		// Find a Substrate binding to get current state from
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (URshipSubstrateMaterialBinding* Binding = Actor->FindComponentByClass<URshipSubstrateMaterialBinding>())
			{
				// Get current state and save as preset
				FRshipSubstrateMaterialState CurrentState = Binding->GetCurrentState();

				FRshipSubstratePreset NewPreset;
				NewPreset.PresetName = PresetName;
				NewPreset.State = CurrentState;

				// Add to manager if available
				if (GEngine)
				{
					if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
					{
						if (URshipSubstrateMaterialManager* Manager = Subsystem->GetSubstrateMaterialManager())
						{
							Manager->AddPreset(NewPreset);
						}
					}
				}

				break;
			}
		}
	}
#endif

	// Clear the input
	PresetNameInput->SetText(FText::GetEmpty());

	// Refresh combo box
	if (PresetComboBox.IsValid())
	{
		PresetComboBox->RefreshOptions();
	}

	return FReply::Handled();
}

FReply SRshipMaterialPanel::OnLoadPresetClicked()
{
	if (!SelectedPreset.IsValid()) return FReply::Handled();

	FString PresetName = *SelectedPreset;

	// Parse transition duration
	float Duration = 1.0f;
	FString DurationStr = TransitionDurationInput->GetText().ToString();
	if (!DurationStr.IsEmpty())
	{
		Duration = FCString::Atof(*DurationStr);
		Duration = FMath::Clamp(Duration, 0.1f, 10.0f);
	}

#if WITH_EDITOR
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (World)
	{
		// Find Substrate bindings and transition to preset
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (URshipSubstrateMaterialBinding* Binding = Actor->FindComponentByClass<URshipSubstrateMaterialBinding>())
			{
				// Check if this binding uses our selected material
				bool bUsesSelectedMaterial = false;
				if (CurrentMaterial.IsValid())
				{
					TArray<UMaterialInstanceDynamic*> DynamicMats = Binding->GetDynamicMaterials();
					for (UMaterialInstanceDynamic* DynMat : DynamicMats)
					{
						if (DynMat && DynMat->GetMaterial() == CurrentMaterial.Get())
						{
							bUsesSelectedMaterial = true;
							break;
						}
					}
				}

				if (bUsesSelectedMaterial)
				{
					// Transition to the preset
					Binding->TransitionToPreset(PresetName, Duration);
				}
			}
		}
	}
#endif

	return FReply::Handled();
}

FReply SRshipMaterialPanel::OnDeletePresetClicked()
{
	if (SelectedPreset.IsValid())
	{
		FString PresetName = *SelectedPreset;
		PresetOptions.Remove(SelectedPreset);
		SelectedPreset = nullptr;

		// Also delete from bindings that have this preset
#if WITH_EDITOR
		UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (World)
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				AActor* Actor = *It;
				if (URshipSubstrateMaterialBinding* Binding = Actor->FindComponentByClass<URshipSubstrateMaterialBinding>())
				{
					Binding->DeletePreset(PresetName);
				}
			}
		}
#endif

		if (PresetComboBox.IsValid())
		{
			PresetComboBox->RefreshOptions();
		}
	}
	return FReply::Handled();
}

FReply SRshipMaterialPanel::OnTestTransitionClicked()
{
	if (!CurrentMaterial.IsValid()) return FReply::Handled();

	// Parse transition duration
	float Duration = 1.0f;
	FString DurationStr = TransitionDurationInput->GetText().ToString();
	if (!DurationStr.IsEmpty())
	{
		Duration = FCString::Atof(*DurationStr);
		Duration = FMath::Clamp(Duration, 0.1f, 10.0f);
	}

	// Generate a random target state for demonstration
	FRshipSubstrateMaterialState RandomState;
	RandomState.BaseColor = FLinearColor(FMath::FRand(), FMath::FRand(), FMath::FRand(), 1.0f);
	RandomState.Roughness = FMath::FRand();
	RandomState.Metallic = FMath::FRand();
	RandomState.EmissiveIntensity = FMath::FRand() * 5.0f;
	RandomState.EmissiveColor = FLinearColor(FMath::FRand(), FMath::FRand(), FMath::FRand(), 1.0f);
	RandomState.Opacity = 0.5f + FMath::FRand() * 0.5f; // Keep at least 50% opacity

#if WITH_EDITOR
	// Find any Substrate binding components on actors using this material
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (World)
	{
		bool bFoundBinding = false;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (URshipSubstrateMaterialBinding* Binding = Actor->FindComponentByClass<URshipSubstrateMaterialBinding>())
			{
				// Check if this binding uses our material
				TArray<UMaterialInstanceDynamic*> DynamicMats = Binding->GetDynamicMaterials();
				for (UMaterialInstanceDynamic* DynMat : DynamicMats)
				{
					if (DynMat && DynMat->GetMaterial() == CurrentMaterial.Get())
					{
						// Found a binding using this material - transition it
						Binding->TransitionToState(RandomState, Duration);
						bFoundBinding = true;
						break;
					}
				}
				if (bFoundBinding) break;
			}
		}

		if (!bFoundBinding)
		{
			// No binding component found - log a message
			UE_LOG(LogTemp, Warning, TEXT("No URshipSubstrateMaterialBinding component found using the selected material. Add a Substrate Material Binding component to test transitions."));
		}
	}
#endif

	return FReply::Handled();
}

void SRshipMaterialPanel::RefreshMaterialList()
{
	MaterialOptions.Empty();

#if WITH_EDITOR
	TSet<FString> UniqueNames;

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (World)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			TArray<UMeshComponent*> MeshComponents;
			Actor->GetComponents<UMeshComponent>(MeshComponents);

			for (UMeshComponent* MeshComp : MeshComponents)
			{
				for (int32 i = 0; i < MeshComp->GetNumMaterials(); i++)
				{
					UMaterialInterface* Mat = MeshComp->GetMaterial(i);
					if (Mat && !UniqueNames.Contains(Mat->GetName()))
					{
						UniqueNames.Add(Mat->GetName());
						MaterialOptions.Add(MakeShared<FString>(Mat->GetName()));
					}
				}
			}
		}
	}
#endif

	if (MaterialComboBox.IsValid())
	{
		MaterialComboBox->RefreshOptions();
	}
}

void SRshipMaterialPanel::RefreshParameterList()
{
	ParameterItems.Empty();

	if (CurrentMaterial.IsValid())
	{
		CollectMaterialParameters(CurrentMaterial.Get());
	}

	// Update count
	if (ParameterCountText.IsValid())
	{
		ParameterCountText->SetText(FText::Format(
			LOCTEXT("ParameterCountFmt", "{0} {0}|plural(one=parameter,other=parameters)"),
			ParameterItems.Num()));
	}

	if (ParameterListView.IsValid())
	{
		ParameterListView->RequestListRefresh();
	}
}

void SRshipMaterialPanel::RefreshStatus()
{
	int32 BoundCount = 0;
	for (const auto& Item : ParameterItems)
	{
		if (Item->bIsBound) BoundCount++;
	}

	if (BoundCountText.IsValid())
	{
		BoundCountText->SetText(FText::Format(LOCTEXT("BoundCountFmt", "{0} bound"), BoundCount));
	}
}

bool SRshipMaterialPanel::IsSubstrateMaterial(UMaterialInterface* Material) const
{
	return URshipSubstrateMaterialBinding::IsSubstrateMaterial(Material);
}

void SRshipMaterialPanel::CollectMaterialParameters(UMaterialInterface* Material)
{
	if (!Material) return;

	// Collect scalar parameters
	TArray<FMaterialParameterInfo> ScalarParams;
	TArray<FGuid> ScalarGuids;
	Material->GetAllScalarParameterInfo(ScalarParams, ScalarGuids);

	for (const FMaterialParameterInfo& Info : ScalarParams)
	{
		TSharedPtr<FRshipMaterialParameterItem> Item = MakeShared<FRshipMaterialParameterItem>();
		Item->ParameterName = Info.Name;
		Item->ParameterType = TEXT("Scalar");

		float Value = 0.0f;
		if (Material->GetScalarParameterValue(Info, Value))
		{
			Item->CurrentValue = FString::Printf(TEXT("%.3f"), Value);
		}

		ParameterItems.Add(Item);
	}

	// Collect vector parameters
	TArray<FMaterialParameterInfo> VectorParams;
	TArray<FGuid> VectorGuids;
	Material->GetAllVectorParameterInfo(VectorParams, VectorGuids);

	for (const FMaterialParameterInfo& Info : VectorParams)
	{
		TSharedPtr<FRshipMaterialParameterItem> Item = MakeShared<FRshipMaterialParameterItem>();
		Item->ParameterName = Info.Name;
		Item->ParameterType = TEXT("Vector");

		FLinearColor Value;
		if (Material->GetVectorParameterValue(Info, Value))
		{
			Item->CurrentValue = FString::Printf(TEXT("(%.2f, %.2f, %.2f, %.2f)"),
				Value.R, Value.G, Value.B, Value.A);
		}

		ParameterItems.Add(Item);
	}

	// Collect texture parameters (name only, no binding support yet)
	TArray<FMaterialParameterInfo> TextureParams;
	TArray<FGuid> TextureGuids;
	Material->GetAllTextureParameterInfo(TextureParams, TextureGuids);

	for (const FMaterialParameterInfo& Info : TextureParams)
	{
		TSharedPtr<FRshipMaterialParameterItem> Item = MakeShared<FRshipMaterialParameterItem>();
		Item->ParameterName = Info.Name;
		Item->ParameterType = TEXT("Texture");

		UTexture* Texture = nullptr;
		if (Material->GetTextureParameterValue(Info, Texture) && Texture)
		{
			Item->CurrentValue = Texture->GetName();
		}
		else
		{
			Item->CurrentValue = TEXT("(none)");
		}

		ParameterItems.Add(Item);
	}
}

// ============================================================================
// SRshipMaterialParameterRow
// ============================================================================

void SRshipMaterialParameterRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	Item = InArgs._Item;
	SMultiColumnTableRow<TSharedPtr<FRshipMaterialParameterItem>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> SRshipMaterialParameterRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (!Item.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	if (ColumnName == "Name")
	{
		return SNew(SBox)
			.Padding(FMargin(4, 2))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromName(Item->ParameterName))
			];
	}
	else if (ColumnName == "Type")
	{
		return SNew(SBox)
			.Padding(FMargin(4, 2))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->ParameterType))
			];
	}
	else if (ColumnName == "Value")
	{
		return SNew(SBox)
			.Padding(FMargin(4, 2))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->CurrentValue))
			];
	}
	else if (ColumnName == "EmitterId")
	{
		return SNew(SBox)
			.Padding(FMargin(4, 2))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->BoundEmitterId.IsEmpty() ? TEXT("-") : Item->BoundEmitterId))
				.ColorAndOpacity(Item->BoundEmitterId.IsEmpty() ? FSlateColor::UseSubduedForeground() : FSlateColor::UseForeground())
			];
	}
	else if (ColumnName == "Status")
	{
		FLinearColor StatusColor = Item->bIsBound ? FLinearColor::Green : FLinearColor::Gray;
		FText StatusText = Item->bIsBound ? LOCTEXT("StatusBound", "Bound") : LOCTEXT("StatusUnbound", "-");

		return SNew(SBox)
			.Padding(FMargin(4, 2))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(StatusText)
				.ColorAndOpacity(StatusColor)
			];
	}

	return SNullWidget::NullWidget;
}

#undef LOCTEXT_NAMESPACE
