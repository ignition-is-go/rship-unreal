// Copyright Rocketship. All Rights Reserved.

#include "SRshipContentMappingPanel.h"
#include "RshipSubsystem.h"
#include "RshipContentMappingManager.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Dom/JsonObject.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Engine/Engine.h"
#include "SlateOptMacros.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "RshipContentMappingPreviewActor.h"
#include "Editor.h"
#include "RshipTargetComponent.h"
#include "RshipCameraManager.h"
#include "RshipSceneConverter.h"
#include "Camera/CameraActor.h"
#include "RshipCameraActor.h"

#define LOCTEXT_NAMESPACE "SRshipContentMappingPanel"

void SRshipContentMappingPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				BuildHeaderSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4, 0, 8)
			[
				BuildQuickMappingSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				SNew(SSeparator)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				BuildContextsSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				BuildSurfacesSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SSeparator)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				BuildMappingsSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 12, 0, 0)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(8.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0,0,8,0)
					[
						SAssignNew(PreviewBorder, SBorder)
						.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
						.BorderBackgroundColor(FLinearColor(0.1f,0.1f,0.1f,1.f))
						.Padding(2.0f)
						[
							SAssignNew(PreviewImage, SImage)
							.Image(FAppStyle::GetBrush("WhiteBrush"))
							.ColorAndOpacity(FLinearColor::White)
							.DesiredSizeOverride(FVector2D(160, 90))
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[
						SAssignNew(PreviewLabel, STextBlock)
						.Text(LOCTEXT("PreviewLabel", "Select a mapping to preview.\n(Currently shows last resolved texture or status only.)"))
						.ColorAndOpacity(FLinearColor::Gray)
						.AutoWrapText(true)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SCheckBox)
							.IsChecked_Lambda([this]()
							{
								UWorld* World = GetEditorWorld();
								if (!World) return ECheckBoxState::Unchecked;
								for (TActorIterator<ARshipContentMappingPreviewActor> It(World); It; ++It)
								{
									return ECheckBoxState::Checked;
								}
								return ECheckBoxState::Unchecked;
							})
							.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
							{
								UWorld* World = GetEditorWorld();
								if (!World) return;
								if (NewState == ECheckBoxState::Checked)
								{
									FActorSpawnParameters Params;
									Params.Name = TEXT("RshipContentMappingPreview");
									World->SpawnActor<ARshipContentMappingPreviewActor>(Params);
									if (PreviewLabel.IsValid())
									{
										PreviewLabel->SetText(LOCTEXT("GizmoSpawned", "Projector gizmo enabled (updates on preview)."));
										PreviewLabel->SetColorAndOpacity(FLinearColor::White);
									}
								}
								else
								{
									for (TActorIterator<ARshipContentMappingPreviewActor> It(World); It; ++It)
									{
										It->Destroy();
									}
								}
							})
							[
								SNew(STextBlock).Text(LOCTEXT("ToggleGizmo", "Projector Gizmo"))
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)
						[
							SNew(SCheckBox)
							.IsChecked_Lambda([]()
							{
								if (!GEngine) return ECheckBoxState::Unchecked;
								URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
								if (!Subsystem) return ECheckBoxState::Unchecked;
								if (URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager())
								{
									return Manager->IsDebugOverlayEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
								}
								return ECheckBoxState::Unchecked;
							})
							.OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
							{
								if (!GEngine) return;
								URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
								if (!Subsystem) return;
								if (URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager())
								{
									Manager->SetDebugOverlayEnabled(NewState == ECheckBoxState::Checked);
								}
							})
							[
								SNew(STextBlock).Text(LOCTEXT("ToggleOverlay", "Viewport Overlay"))
							]
						]
					]
				]
			]
		]
	];

	ResetForms();
	RefreshStatus();
}

SRshipContentMappingPanel::~SRshipContentMappingPanel()
{
}

UWorld* SRshipContentMappingPanel::GetEditorWorld() const
{
#if WITH_EDITOR
	if (GEditor)
	{
		return GEditor->GetEditorWorldContext().World();
	}
#endif
	return GEngine ? GEngine->GetWorld() : nullptr;
}

FString SRshipContentMappingPanel::ResolveTargetIdInput(const FString& InText) const
{
	const FString Trimmed = InText.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return Trimmed;
	}

	if (Trimmed.Contains(TEXT(":")))
	{
		return Trimmed;
	}

	// Prefer explicit matches from current target options
	for (const TSharedPtr<FRshipIdOption>& Option : TargetOptions)
	{
		if (!Option.IsValid())
		{
			continue;
		}

		if (Option->Id.Equals(Trimmed, ESearchCase::IgnoreCase))
		{
			return Option->ResolvedId.IsEmpty() ? Option->Id : Option->ResolvedId;
		}

		if (Option->Actor.IsValid())
		{
			const FString ActorLabel = Option->Actor->GetActorLabel();
			if (!ActorLabel.IsEmpty() && ActorLabel.Equals(Trimmed, ESearchCase::IgnoreCase))
			{
				return Option->ResolvedId.IsEmpty() ? Option->Id : Option->ResolvedId;
			}
		}
	}

	URshipSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URshipSubsystem>() : nullptr;
	if (Subsystem && Subsystem->TargetComponents)
	{
		for (auto& Pair : *Subsystem->TargetComponents)
		{
			URshipTargetComponent* Component = Pair.Value;
			if (!Component)
			{
				continue;
			}

			const FString ShortId = Component->targetName;
			if (!ShortId.IsEmpty() && ShortId.Equals(Trimmed, ESearchCase::IgnoreCase))
			{
				return Pair.Key;
			}

			if (AActor* Owner = Component->GetOwner())
			{
				const FString ActorLabel = Owner->GetActorLabel();
				if (!ActorLabel.IsEmpty() && ActorLabel.Equals(Trimmed, ESearchCase::IgnoreCase))
				{
					return Pair.Key;
				}
			}
		}

		const FString ServiceId = Subsystem->GetServiceId();
		if (!ServiceId.IsEmpty())
		{
			return ServiceId + TEXT(":") + Trimmed;
		}
	}

	return Trimmed;
}

FString SRshipContentMappingPanel::ShortTargetLabel(const FString& TargetId)
{
	FString ShortId;
	if (TargetId.Split(TEXT(":"), nullptr, &ShortId))
	{
		return ShortId;
	}
	return TargetId;
}

void SRshipContentMappingPanel::UpdatePreviewImage(UTexture* Texture, const FRshipContentMappingState& Mapping)
{
	if (!PreviewImage.IsValid())
	{
		return;
	}

	if (!Texture)
	{
		PreviewImage->SetImage(FAppStyle::GetBrush("WhiteBrush"));
		ActivePreviewBrush.SetResourceObject(nullptr);
		bHasActivePreviewBrush = false;
		LastPreviewTexture = nullptr;
		if (PreviewLabel.IsValid())
		{
			PreviewLabel->SetText(FText::FromString(FString::Printf(TEXT("No texture available for %s"), *Mapping.Name)));
			PreviewLabel->SetColorAndOpacity(FLinearColor::Yellow);
		}
		return;
	}

	if (Texture != LastPreviewTexture || !bHasActivePreviewBrush)
	{
		LastPreviewTexture = Texture;
		ActivePreviewBrush = FSlateBrush();
		ActivePreviewBrush.SetResourceObject(Texture);
		ActivePreviewBrush.ImageSize = FVector2D(160, 90);
		ActivePreviewBrush.DrawAs = ESlateBrushDrawType::Image;
		PreviewImage->SetImage(&ActivePreviewBrush);
		bHasActivePreviewBrush = true;
	}
	if (PreviewLabel.IsValid())
	{
		const int32 PreviewWidth = FMath::RoundToInt(Texture->GetSurfaceWidth());
		const int32 PreviewHeight = FMath::RoundToInt(Texture->GetSurfaceHeight());
		PreviewLabel->SetText(FText::FromString(FString::Printf(TEXT("Previewing %s (%dx%d)"), *Mapping.Name, PreviewWidth, PreviewHeight)));
		PreviewLabel->SetColorAndOpacity(FLinearColor::White);
	}

	// Update gizmo if present
	UWorld* World = GetEditorWorld();
	if (World)
	{
		for (TActorIterator<ARshipContentMappingPreviewActor> It(World); It; ++It)
		{
			ARshipContentMappingPreviewActor* Gizmo = *It;
			Gizmo->ProjectorPosition = FVector::ZeroVector;
			Gizmo->ProjectorRotation = FRotator::ZeroRotator;
			if (Mapping.Config.IsValid())
			{
				auto GetNum = [](const TSharedPtr<FJsonObject>& Obj, const FString& Field, float DefaultVal)->float
				{
					return (Obj.IsValid() && Obj->HasTypedField<EJson::Number>(Field)) ? static_cast<float>(Obj->GetNumberField(Field)) : DefaultVal;
				};
				if (Mapping.Config->HasTypedField<EJson::Object>(TEXT("projectorPosition")))
				{
					TSharedPtr<FJsonObject> Pos = Mapping.Config->GetObjectField(TEXT("projectorPosition"));
					Gizmo->ProjectorPosition = FVector(
						GetNum(Pos, TEXT("x"), 0.f),
						GetNum(Pos, TEXT("y"), 0.f),
						GetNum(Pos, TEXT("z"), 0.f));
				}
				if (Mapping.Config->HasTypedField<EJson::Object>(TEXT("projectorRotation")))
				{
					TSharedPtr<FJsonObject> Rot = Mapping.Config->GetObjectField(TEXT("projectorRotation"));
					Gizmo->ProjectorRotation = FRotator(
						GetNum(Rot, TEXT("x"), 0.f),
						GetNum(Rot, TEXT("y"), 0.f),
						GetNum(Rot, TEXT("z"), 0.f));
				}
				Gizmo->FOV = GetNum(Mapping.Config, TEXT("fov"), 60.f);
				Gizmo->Aspect = GetNum(Mapping.Config, TEXT("aspectRatio"), 1.7778f);
				Gizmo->NearClip = GetNum(Mapping.Config, TEXT("near"), 10.f);
				Gizmo->FarClip = GetNum(Mapping.Config, TEXT("far"), 10000.f);
			}
		}
	}
}

TSharedRef<SWidget> SRshipContentMappingPanel::BuildIdPickerMenu(const TArray<TSharedPtr<FRshipIdOption>>& Options, const FText& EmptyText, TSharedPtr<SEditableTextBox> TargetInput, bool bAppend)
{
	FMenuBuilder MenuBuilder(true, nullptr);
	if (Options.Num() == 0)
	{
		MenuBuilder.AddMenuEntry(EmptyText, FText(), FSlateIcon(), FUIAction());
		return MenuBuilder.MakeWidget();
	}

	for (const TSharedPtr<FRshipIdOption>& Option : Options)
	{
		if (!Option.IsValid())
		{
			continue;
		}

		const FString OptionId = Option->Id;
		const FString OptionLabel = Option->Label;
		const FString OptionTooltip = Option->ResolvedId.IsEmpty() ? OptionId : Option->ResolvedId;
		MenuBuilder.AddMenuEntry(
			FText::FromString(OptionLabel),
			FText::FromString(OptionTooltip),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this, TargetInput, Option, OptionId, bAppend]()
			{
				if (!TargetInput.IsValid())
				{
					return;
				}

				FString SelectedId = OptionId;
				if (Option.IsValid() && Option->bIsSceneCamera)
				{
					if (!Option->ResolvedId.IsEmpty())
					{
						SelectedId = Option->ResolvedId;
					}
					else if (Option->bRequiresConversion)
					{
						SelectedId = ConvertSceneCamera(Option->Actor.Get());
						if (!SelectedId.IsEmpty())
						{
							Option->ResolvedId = SelectedId;
							Option->bRequiresConversion = false;
							Option->Id = SelectedId;
							const FString ActorLabel = Option->Actor.IsValid() ? Option->Actor->GetActorLabel() : TEXT("Scene Camera");
							Option->Label = FString::Printf(TEXT("Scene Camera: %s (%s)"), *ActorLabel, *SelectedId);
							RefreshStatus();
						}
					}
				}

				if (SelectedId.IsEmpty())
				{
					return;
				}

				if (!bAppend)
				{
					TargetInput->SetText(FText::FromString(SelectedId));
					return;
				}

				FString Current = TargetInput->GetText().ToString();
				TArray<FString> Parts;
				Current.ParseIntoArray(Parts, TEXT(","), true);
				for (FString& Part : Parts)
				{
					Part = Part.TrimStartAndEnd();
				}
				if (!Parts.Contains(SelectedId))
				{
					Parts.Add(SelectedId);
				}
				TargetInput->SetText(FText::FromString(FString::Join(Parts, TEXT(","))));
			}))
		);
	}

	return MenuBuilder.MakeWidget();
}

void SRshipContentMappingPanel::RebuildPickerOptions(const TArray<FRshipRenderContextState>& Contexts, const TArray<FRshipMappingSurfaceState>& Surfaces)
{
	TargetOptions.Reset();
	CameraOptions.Reset();
	AssetOptions.Reset();
	ContextOptions.Reset();
	SurfaceOptions.Reset();

	URshipSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URshipSubsystem>() : nullptr;
	TSet<FString> ExistingCameraIds;

	if (Subsystem && Subsystem->TargetComponents)
	{
		for (auto& Pair : *Subsystem->TargetComponents)
		{
			URshipTargetComponent* Component = Pair.Value;
			if (!Component || !Component->IsValidLowLevel())
			{
				continue;
			}

			const FString TargetId = Component->targetName;
			const FString FullTargetId = Pair.Key;
			const FString DisplayName = Component->GetOwner() ? Component->GetOwner()->GetActorLabel() : TargetId;
			TSharedPtr<FRshipIdOption> Opt = MakeShared<FRshipIdOption>();
			Opt->Id = TargetId;
			Opt->ResolvedId = FullTargetId;
			Opt->Actor = Component->GetOwner();
			Opt->Label = DisplayName.IsEmpty() ? TargetId : FString::Printf(TEXT("%s (%s)"), *DisplayName, *TargetId);
			TargetOptions.Add(Opt);
		}
	}

	if (Subsystem)
	{
		if (URshipCameraManager* CamMgr = Subsystem->GetCameraManager())
		{
			const TArray<FRshipCameraInfo> Cameras = CamMgr->GetAllCameras();
			for (const FRshipCameraInfo& Cam : Cameras)
			{
				if (Cam.Id.IsEmpty())
				{
					continue;
				}
				TSharedPtr<FRshipIdOption> Opt = MakeShared<FRshipIdOption>();
				Opt->Id = Cam.Id;
				Opt->Label = Cam.Name.IsEmpty() ? Cam.Id : FString::Printf(TEXT("%s (%s)"), *Cam.Name, *Cam.Id);
				CameraOptions.Add(Opt);
				ExistingCameraIds.Add(Cam.Id);
			}
		}
	}

	UWorld* World = GetEditorWorld();
	if (World)
	{
		URshipSceneConverter* Converter = Subsystem ? Subsystem->GetSceneConverter() : nullptr;
		for (TActorIterator<ACameraActor> It(World); It; ++It)
		{
			ACameraActor* CameraActor = *It;
			if (!CameraActor || CameraActor->IsA<ARshipCameraActor>())
			{
				continue;
			}

			FString ConvertedId;
			if (Converter)
			{
				ConvertedId = Converter->GetConvertedEntityId(CameraActor);
			}
			if (!ConvertedId.IsEmpty() && ExistingCameraIds.Contains(ConvertedId))
			{
				continue;
			}

			const FString ActorLabel = CameraActor->GetActorLabel();
			const FString ClassName = CameraActor->GetClass() ? CameraActor->GetClass()->GetName() : TEXT("CameraActor");
			const bool bIsCine = ClassName.Contains(TEXT("CineCameraActor"));
			TSharedPtr<FRshipIdOption> Opt = MakeShared<FRshipIdOption>();
			Opt->bIsSceneCamera = true;
			Opt->Actor = CameraActor;
			Opt->ResolvedId = ConvertedId;
			Opt->bRequiresConversion = ConvertedId.IsEmpty();
			Opt->Id = ConvertedId.IsEmpty() ? ActorLabel : ConvertedId;
			const FString Prefix = bIsCine ? TEXT("Scene CineCamera") : TEXT("Scene Camera");
			Opt->Label = ConvertedId.IsEmpty()
				? FString::Printf(TEXT("%s: %s (convert)"), *Prefix, *ActorLabel)
				: FString::Printf(TEXT("%s: %s (%s)"), *Prefix, *ActorLabel, *ConvertedId);
			CameraOptions.Add(Opt);
		}
	}

	TSet<FString> AssetIds;
	for (const FRshipRenderContextState& Ctx : Contexts)
	{
		if (!Ctx.Id.IsEmpty())
		{
			TSharedPtr<FRshipIdOption> Opt = MakeShared<FRshipIdOption>();
			Opt->Id = Ctx.Id;
			Opt->Label = Ctx.Name.IsEmpty() ? Ctx.Id : FString::Printf(TEXT("%s (%s)"), *Ctx.Name, *Ctx.Id);
			ContextOptions.Add(Opt);
		}
		if (!Ctx.AssetId.IsEmpty())
		{
			AssetIds.Add(Ctx.AssetId);
		}
	}

	for (const FRshipMappingSurfaceState& Surface : Surfaces)
	{
		if (Surface.Id.IsEmpty())
		{
			continue;
		}
		TSharedPtr<FRshipIdOption> Opt = MakeShared<FRshipIdOption>();
		Opt->Id = Surface.Id;
		if (Surface.Name.IsEmpty())
		{
			Opt->Label = Surface.TargetId.IsEmpty() ? Surface.Id : FString::Printf(TEXT("%s (%s)"), *Surface.TargetId, *Surface.Id);
		}
		else
		{
			Opt->Label = Surface.TargetId.IsEmpty() ? Surface.Name : FString::Printf(TEXT("%s [%s]"), *Surface.Name, *Surface.TargetId);
		}
		SurfaceOptions.Add(Opt);
	}

	for (const FString& AssetId : AssetIds)
	{
		TSharedPtr<FRshipIdOption> Opt = MakeShared<FRshipIdOption>();
		Opt->Id = AssetId;
		Opt->Label = AssetId;
		AssetOptions.Add(Opt);
	}
}

FString SRshipContentMappingPanel::ConvertSceneCamera(AActor* Actor) const
{
	if (!Actor || !GEngine)
	{
		return TEXT("");
	}

	URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	if (!Subsystem)
	{
		return TEXT("");
	}

	URshipSceneConverter* Converter = Subsystem->GetSceneConverter();
	if (!Converter)
	{
		return TEXT("");
	}

	FRshipDiscoveryOptions Options;
	Options.bIncludeCameras = true;
	Options.bIncludeDirectionalLights = false;
	Options.bIncludePointLights = false;
	Options.bIncludeRectLights = false;
	Options.bIncludeSpotLights = false;
	Options.bSkipAlreadyConverted = false;

	Converter->DiscoverScene(Options);
	const TArray<FRshipDiscoveredCamera> Cameras = Converter->GetDiscoveredCameras();
	for (const FRshipDiscoveredCamera& Camera : Cameras)
	{
		if (Camera.CameraActor == Actor)
		{
			FRshipConversionOptions ConvOptions;
			ConvOptions.bSpawnVisualizationActor = false;
			ConvOptions.bEnableTransformSync = true;
			FRshipConversionResult Result = Converter->ConvertCamera(Camera, ConvOptions);
			if (Result.bSuccess)
			{
				return Result.EntityId;
			}
			return TEXT("");
		}
	}

	return TEXT("");
}
void SRshipContentMappingPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	TimeSinceLastRefresh += InDeltaTime;
	if (TimeSinceLastRefresh >= RefreshInterval)
	{
		TimeSinceLastRefresh = 0.0f;
		RefreshStatus();
	}
}

TSharedRef<SWidget> SRshipContentMappingPanel::BuildHeaderSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("HeaderTitle", "Content Mapping"))
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
				SAssignNew(ConnectionText, STextBlock)
				.Text(LOCTEXT("ConnectionUnknown", "Status: Unknown"))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("RefreshButton", "Refresh"))
				.OnClicked_Lambda([this]()
				{
					RefreshStatus();
					return FReply::Handled();
				})
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4, 0, 0)
		[
			SAssignNew(CountsText, STextBlock)
			.Text(LOCTEXT("CountsUnknown", "Contexts: 0  Surfaces: 0  Mappings: 0"))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4, 0, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("HeaderNote", "Lightweight editor-side controls; full editing also available in rship client."))
			.ColorAndOpacity(FLinearColor::Gray)
		];
}

TSharedRef<SWidget> SRshipContentMappingPanel::BuildQuickMappingSection()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(8.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("QuickTitle", "Quick Mapping"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 6)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("QuickNote", "Fast create: pick a source + target and we build the underlying entities automatically."))
				.ColorAndOpacity(FLinearColor::Gray)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
				[
					SNew(STextBlock).Text(LOCTEXT("QuickSourceLabel", "Source"))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
				[
					SNew(SCheckBox)
					.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
					.IsChecked_Lambda([this]() { return QuickSourceType == TEXT("camera") ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
					{
						if (State == ECheckBoxState::Checked)
						{
							QuickSourceType = TEXT("camera");
						}
					})
					[
						SNew(STextBlock).Text(LOCTEXT("QuickSourceCamera", "Camera"))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
				[
					SNew(SCheckBox)
					.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
					.IsChecked_Lambda([this]() { return QuickSourceType == TEXT("asset-store") ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
					{
						if (State == ECheckBoxState::Checked)
						{
							QuickSourceType = TEXT("asset-store");
						}
					})
					[
						SNew(STextBlock).Text(LOCTEXT("QuickSourceAsset", "Asset"))
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(4,0,4,0)
				[
					SAssignNew(QuickSourceIdInput, SEditableTextBox)
					.HintText_Lambda([this]()
					{
						return FText::FromString(QuickSourceType == TEXT("camera") ? TEXT("CameraId") : TEXT("AssetId"));
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,6,0)
				[
					SNew(SComboButton)
					.OnGetMenuContent_Lambda([this]()
					{
						const bool bIsCamera = QuickSourceType == TEXT("camera");
						const TArray<TSharedPtr<FRshipIdOption>>& Options = bIsCamera ? CameraOptions : AssetOptions;
						const FText EmptyText = bIsCamera ? LOCTEXT("QuickNoCameras", "No cameras found") : LOCTEXT("QuickNoAssets", "No assets found");
						return BuildIdPickerMenu(Options, EmptyText, QuickSourceIdInput, false);
					})
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							return QuickSourceType == TEXT("camera") ? LOCTEXT("QuickPickCamera", "Pick Camera") : LOCTEXT("QuickPickAsset", "Pick Asset");
						})
					]
				]
				+ SHorizontalBox::Slot().FillWidth(0.6f)
				[
					SAssignNew(QuickProjectIdInput, SEditableTextBox)
					.HintText(LOCTEXT("QuickProjectHint", "ProjectId (optional)"))
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
				[
					SNew(STextBlock).Text(LOCTEXT("QuickTargetLabel", "Target"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.2f).Padding(0,0,4,0)
				[
					SAssignNew(QuickTargetIdInput, SEditableTextBox)
					.HintText(LOCTEXT("QuickTargetHint", "Pick or type target name"))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,6,0)
				[
					SNew(SComboButton)
					.OnGetMenuContent_Lambda([this]()
					{
						return BuildIdPickerMenu(TargetOptions, LOCTEXT("QuickNoTargets", "No targets found"), QuickTargetIdInput, false);
					})
					.ButtonContent()
					[
						SNew(STextBlock).Text(LOCTEXT("QuickPickTarget", "Pick Target"))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,4,0)
				[
					SNew(STextBlock).Text(LOCTEXT("QuickUvLabel", "UV"))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
				[
					SAssignNew(QuickUvChannelInput, SSpinBox<int32>)
					.MinValue(0).MaxValue(7).Value(0)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,4,0)
				[
					SNew(STextBlock).Text(LOCTEXT("QuickOpacityLabel", "Opacity"))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
				[
					SAssignNew(QuickOpacityInput, SSpinBox<float>)
					.MinValue(0.0f).MaxValue(1.0f).Delta(0.05f).Value(1.0f)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
				[
					SNew(SCheckBox)
					.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
					.IsChecked_Lambda([this]() { return QuickMappingType == TEXT("surface-uv") ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
					{
						if (State == ECheckBoxState::Checked)
						{
							QuickMappingType = TEXT("surface-uv");
						}
					})
					[
						SNew(STextBlock).Text(LOCTEXT("QuickModeUV", "UV"))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
				[
					SNew(SCheckBox)
					.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
					.IsChecked_Lambda([this]() { return QuickMappingType == TEXT("surface-projection") ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
					{
						if (State == ECheckBoxState::Checked)
						{
							QuickMappingType = TEXT("surface-projection");
						}
					})
					[
						SNew(STextBlock).Text(LOCTEXT("QuickModeProj", "Projection"))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([this]() { return bQuickAdvanced ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
					{
						bQuickAdvanced = (State == ECheckBoxState::Checked);
					})
					[
						SNew(STextBlock).Text(LOCTEXT("QuickAdvanced", "Advanced"))
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(SSpacer)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("QuickCreateButton", "Create Mapping"))
					.OnClicked_Lambda([this]()
					{
						if (!GEngine) return FReply::Handled();
						URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
						if (!Subsystem) return FReply::Handled();
						URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
						if (!Manager) return FReply::Handled();

						const FString ProjectId = QuickProjectIdInput.IsValid() ? QuickProjectIdInput->GetText().ToString().TrimStartAndEnd() : TEXT("");
						const FString SourceId = QuickSourceIdInput.IsValid() ? QuickSourceIdInput->GetText().ToString().TrimStartAndEnd() : TEXT("");
						const FString TargetIdInput = QuickTargetIdInput.IsValid() ? QuickTargetIdInput->GetText().ToString().TrimStartAndEnd() : TEXT("");
						const FString TargetId = ResolveTargetIdInput(TargetIdInput);
						const FString TargetLabel = ShortTargetLabel(TargetId);
						const int32 Width = bQuickAdvanced && QuickWidthInput.IsValid() ? QuickWidthInput->GetValue() : 0;
						const int32 Height = bQuickAdvanced && QuickHeightInput.IsValid() ? QuickHeightInput->GetValue() : 0;
						const FString CaptureMode = bQuickAdvanced && QuickCaptureModeInput.IsValid() ? QuickCaptureModeInput->GetText().ToString().TrimStartAndEnd() : TEXT("");
						const int32 UVChannel = QuickUvChannelInput.IsValid() ? QuickUvChannelInput->GetValue() : 0;
						const float Opacity = QuickOpacityInput.IsValid() ? QuickOpacityInput->GetValue() : 1.0f;
						const FString MeshName = bQuickAdvanced && QuickMeshNameInput.IsValid() ? QuickMeshNameInput->GetText().ToString().TrimStartAndEnd() : TEXT("");

						if (SourceId.IsEmpty() || TargetId.IsEmpty())
						{
							if (PreviewLabel.IsValid())
							{
								PreviewLabel->SetText(LOCTEXT("QuickMissing", "Source and target are required."));
								PreviewLabel->SetColorAndOpacity(FLinearColor::Red);
							}
							return FReply::Handled();
						}

						auto ParseSlots = [](const FString& Text)
						{
							TArray<int32> Out;
							TArray<FString> Parts;
							Text.ParseIntoArray(Parts, TEXT(","), true);
							for (const FString& Part : Parts)
							{
								const int32 Value = FCString::Atoi(*Part);
								Out.Add(Value);
							}
							Out.Sort();
							return Out;
						};

						const FString SlotsText = bQuickAdvanced && QuickMaterialSlotsInput.IsValid() ? QuickMaterialSlotsInput->GetText().ToString() : TEXT("");
						TArray<int32> RequestedSlots = SlotsText.IsEmpty() ? TArray<int32>() : ParseSlots(SlotsText);

						FString ContextId;
						const TArray<FRshipRenderContextState> Contexts = Manager->GetRenderContexts();
						for (const FRshipRenderContextState& Ctx : Contexts)
						{
							if (ProjectId.IsEmpty())
							{
								if (!Ctx.ProjectId.IsEmpty()) continue;
							}
							else if (Ctx.ProjectId != ProjectId)
							{
								continue;
							}
							if (Ctx.SourceType != QuickSourceType) continue;
							if (QuickSourceType == TEXT("camera") && Ctx.CameraId != SourceId) continue;
							if (QuickSourceType == TEXT("asset-store") && Ctx.AssetId != SourceId) continue;
							if (Width > 0 && Ctx.Width != Width) continue;
							if (Height > 0 && Ctx.Height != Height) continue;
							if (!CaptureMode.IsEmpty() && Ctx.CaptureMode != CaptureMode) continue;
							ContextId = Ctx.Id;
							break;
						}

						if (ContextId.IsEmpty())
						{
							FRshipRenderContextState NewCtx;
							NewCtx.Name = FString::Printf(TEXT("Ctx %s"), *SourceId);
							NewCtx.ProjectId = ProjectId;
							NewCtx.SourceType = QuickSourceType;
							if (QuickSourceType == TEXT("camera"))
							{
								NewCtx.CameraId = SourceId;
							}
							else
							{
								NewCtx.AssetId = SourceId;
							}
							NewCtx.Width = Width;
							NewCtx.Height = Height;
							NewCtx.CaptureMode = CaptureMode.IsEmpty() ? TEXT("FinalColorLDR") : CaptureMode;
							NewCtx.bEnabled = true;
							ContextId = Manager->CreateRenderContext(NewCtx);
						}

						FString SurfaceId;
						const TArray<FRshipMappingSurfaceState> Surfaces = Manager->GetMappingSurfaces();
						for (const FRshipMappingSurfaceState& Surface : Surfaces)
						{
							if (ProjectId.IsEmpty())
							{
								if (!Surface.ProjectId.IsEmpty()) continue;
							}
							else if (Surface.ProjectId != ProjectId)
							{
								continue;
							}
							if (Surface.TargetId != TargetId) continue;
							if (Surface.UVChannel != UVChannel) continue;
							if (!MeshName.IsEmpty() && Surface.MeshComponentName != MeshName) continue;
							if (RequestedSlots.Num() > 0)
							{
								TArray<int32> ExistingSlots = Surface.MaterialSlots;
								ExistingSlots.Sort();
								if (ExistingSlots != RequestedSlots) continue;
							}
							SurfaceId = Surface.Id;
							break;
						}

						if (SurfaceId.IsEmpty())
						{
							FRshipMappingSurfaceState NewSurface;
							NewSurface.Name = FString::Printf(TEXT("Surface %s"), *TargetLabel);
							NewSurface.ProjectId = ProjectId;
							NewSurface.TargetId = TargetId;
							NewSurface.UVChannel = UVChannel;
							NewSurface.MaterialSlots = RequestedSlots;
							NewSurface.MeshComponentName = MeshName;
							NewSurface.bEnabled = true;
							SurfaceId = Manager->CreateMappingSurface(NewSurface);
						}

						FString MappingId;
						const TArray<FRshipContentMappingState> Mappings = Manager->GetMappings();
						for (const FRshipContentMappingState& Mapping : Mappings)
						{
							if (ProjectId.IsEmpty())
							{
								if (!Mapping.ProjectId.IsEmpty()) continue;
							}
							else if (Mapping.ProjectId != ProjectId)
							{
								continue;
							}
							if (Mapping.Type != QuickMappingType) continue;
							if (Mapping.ContextId != ContextId) continue;
							if (Mapping.SurfaceIds.Num() == 1 && Mapping.SurfaceIds[0] == SurfaceId)
							{
								MappingId = Mapping.Id;
								break;
							}
						}

						if (MappingId.IsEmpty())
						{
							FRshipContentMappingState NewMapping;
							NewMapping.Name = FString::Printf(TEXT("Map %s"), *TargetLabel);
							NewMapping.ProjectId = ProjectId;
							NewMapping.Type = QuickMappingType;
							NewMapping.ContextId = ContextId;
							NewMapping.SurfaceIds = { SurfaceId };
							NewMapping.Opacity = Opacity;
							NewMapping.bEnabled = true;
							if (QuickMappingType == TEXT("surface-projection"))
							{
								NewMapping.Config = MakeShared<FJsonObject>();
								NewMapping.Config->SetStringField(TEXT("projectionType"), TEXT("perspective"));
							}
							MappingId = Manager->CreateMapping(NewMapping);
						}
						else
						{
							FRshipContentMappingState UpdateMapping;
							for (const FRshipContentMappingState& Mapping : Mappings)
							{
								if (Mapping.Id == MappingId)
								{
									UpdateMapping = Mapping;
									break;
								}
							}
							UpdateMapping.Opacity = Opacity;
							Manager->UpdateMapping(UpdateMapping);
						}

						SelectedMappingId = MappingId;
						LastPreviewMappingId = MappingId;
						if (PreviewLabel.IsValid())
						{
							PreviewLabel->SetText(LOCTEXT("QuickCreated", "Mapping created (context/surface reused when possible)."));
							PreviewLabel->SetColorAndOpacity(FLinearColor::White);
						}
						RefreshStatus();
						return FReply::Handled();
					})
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
			[
				SNew(SVerticalBox)
				.Visibility_Lambda([this]() { return bQuickAdvanced ? EVisibility::Visible : EVisibility::Collapsed; })
				+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
					[
						SNew(STextBlock).Text(LOCTEXT("QuickResLabel", "Resolution"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(QuickWidthInput, SSpinBox<int32>).MinValue(0).MaxValue(8192).Value(1920)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
					[
						SAssignNew(QuickHeightInput, SSpinBox<int32>).MinValue(0).MaxValue(8192).Value(1080)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
					[
						SNew(STextBlock).Text(LOCTEXT("QuickCaptureLabel", "Capture"))
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0,0,6,0)
					[
						SAssignNew(QuickCaptureModeInput, SEditableTextBox)
						.Text(LOCTEXT("QuickCaptureDefault", "FinalColorLDR"))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
					[
						SNew(STextBlock).Text(LOCTEXT("QuickSlotsLabel", "Slots"))
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0,0,6,0)
					[
						SAssignNew(QuickMaterialSlotsInput, SEditableTextBox)
						.HintText(LOCTEXT("QuickSlotsHint", "Material slots (comma-separated, optional)"))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
					[
						SNew(STextBlock).Text(LOCTEXT("QuickMeshLabel", "Mesh"))
					]
					+ SHorizontalBox::Slot().FillWidth(0.8f)
					[
						SAssignNew(QuickMeshNameInput, SEditableTextBox)
						.HintText(LOCTEXT("QuickMeshHint", "Mesh component name (optional)"))
					]
				]
			]
		];
}

TSharedRef<SWidget> SRshipContentMappingPanel::BuildContextsSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ContextsTitle", "Render Contexts"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(ContextList, SVerticalBox)
		];
}

TSharedRef<SWidget> SRshipContentMappingPanel::BuildSurfacesSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SurfacesTitle", "Mapping Surfaces"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(SurfaceList, SVerticalBox)
		];
}

TSharedRef<SWidget> SRshipContentMappingPanel::BuildMappingsSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("MappingsTitle", "Surface Mappings"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(MappingList, SVerticalBox)
		];
}

TSharedRef<SWidget> SRshipContentMappingPanel::BuildContextForm()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(LOCTEXT("CtxFormTitle", "Render Context")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
				[
					SNew(STextBlock).Text(LOCTEXT("CtxName", "Name"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SAssignNew(CtxNameInput, SEditableTextBox)
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
				[
					SNew(STextBlock).Text(LOCTEXT("CtxProject", "ProjectId"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SAssignNew(CtxProjectInput, SEditableTextBox)
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
				[
					SNew(STextBlock).Text(LOCTEXT("CtxSourceType", "SourceType (camera/asset-store)"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SAssignNew(CtxSourceTypeInput, SEditableTextBox).Text(FText::FromString(TEXT("camera")))
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
				[
					SNew(STextBlock).Text(LOCTEXT("CtxCamera", "CameraId"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SAssignNew(CtxCameraInput, SEditableTextBox)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(6,0,0,0)
				[
					SNew(SComboButton)
					.OnGetMenuContent_Lambda([this]()
					{
						return BuildIdPickerMenu(CameraOptions, LOCTEXT("CtxNoCameras", "No cameras found"), CtxCameraInput, false);
					})
					.ButtonContent()
					[
						SNew(STextBlock).Text(LOCTEXT("CtxPickCamera", "Pick"))
					]
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
				[
					SNew(STextBlock).Text(LOCTEXT("CtxAsset", "AssetId"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SAssignNew(CtxAssetInput, SEditableTextBox)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(6,0,0,0)
				[
					SNew(SComboButton)
					.OnGetMenuContent_Lambda([this]()
					{
						return BuildIdPickerMenu(AssetOptions, LOCTEXT("CtxNoAssets", "No assets found"), CtxAssetInput, false);
					})
					.ButtonContent()
					[
						SNew(STextBlock).Text(LOCTEXT("CtxPickAsset", "Pick"))
					]
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
				[
					SNew(STextBlock).Text(LOCTEXT("CtxResolution", "Width / Height"))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,6,0)
				[
					SAssignNew(CtxWidthInput, SSpinBox<int32>).MinValue(0).MaxValue(8192).Value(1920)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SAssignNew(CtxHeightInput, SSpinBox<int32>).MinValue(0).MaxValue(8192).Value(1080)
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
				[
					SNew(STextBlock).Text(LOCTEXT("CtxCapture", "CaptureMode"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SAssignNew(CtxCaptureInput, SEditableTextBox).Text(FText::FromString(TEXT("FinalColorLDR")))
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,4,0,2)
			[
				SAssignNew(CtxEnabledInput, SCheckBox).IsChecked(ECheckBoxState::Checked)
				[
					SNew(STextBlock).Text(LOCTEXT("CtxEnabled", "Enabled"))
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,6,0)
				[
					SNew(SButton)
					.Text_Lambda([this]() { return SelectedContextId.IsEmpty() ? LOCTEXT("CtxCreate", "Create Context") : LOCTEXT("CtxSave", "Save Context"); })
					.OnClicked_Lambda([this]()
					{
						if (!GEngine) return FReply::Handled();
						URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
						if (!Subsystem) return FReply::Handled();
						if (URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager())
						{
							FRshipRenderContextState State;
							State.Id = SelectedContextId;
							State.Name = CtxNameInput.IsValid() ? CtxNameInput->GetText().ToString() : TEXT("");
							State.ProjectId = CtxProjectInput.IsValid() ? CtxProjectInput->GetText().ToString() : TEXT("");
							State.SourceType = CtxSourceTypeInput.IsValid() ? CtxSourceTypeInput->GetText().ToString() : TEXT("camera");
							State.CameraId = CtxCameraInput.IsValid() ? CtxCameraInput->GetText().ToString() : TEXT("");
							State.AssetId = CtxAssetInput.IsValid() ? CtxAssetInput->GetText().ToString() : TEXT("");
							State.Width = CtxWidthInput.IsValid() ? CtxWidthInput->GetValue() : 0;
							State.Height = CtxHeightInput.IsValid() ? CtxHeightInput->GetValue() : 0;
							State.CaptureMode = CtxCaptureInput.IsValid() ? CtxCaptureInput->GetText().ToString() : TEXT("");
							State.bEnabled = !CtxEnabledInput.IsValid() || CtxEnabledInput->IsChecked();

							if (State.Id.IsEmpty())
							{
								SelectedContextId = Manager->CreateRenderContext(State);
							}
							else
							{
								Manager->UpdateRenderContext(State);
							}
							RefreshStatus();
						}
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("CtxReset", "New"))
					.OnClicked_Lambda([this]()
					{
						SelectedContextId.Reset();
						ResetForms();
						return FReply::Handled();
					})
				]
			]
		];
}

TSharedRef<SWidget> SRshipContentMappingPanel::BuildSurfaceForm()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(LOCTEXT("SurfFormTitle", "Mapping Surface")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
				[
					SNew(STextBlock).Text(LOCTEXT("SurfName", "Name"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SAssignNew(SurfNameInput, SEditableTextBox)
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
				[
					SNew(STextBlock).Text(LOCTEXT("SurfProject", "ProjectId"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SAssignNew(SurfProjectInput, SEditableTextBox)
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
				[
					SNew(STextBlock).Text(LOCTEXT("SurfTarget", "TargetId"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SAssignNew(SurfTargetInput, SEditableTextBox)
					.HintText(LOCTEXT("SurfTargetHint", "Pick or type target name"))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(6,0,0,0)
				[
					SNew(SComboButton)
					.OnGetMenuContent_Lambda([this]()
					{
						return BuildIdPickerMenu(TargetOptions, LOCTEXT("SurfNoTargets", "No targets found"), SurfTargetInput, false);
					})
					.ButtonContent()
					[
						SNew(STextBlock).Text(LOCTEXT("SurfPickTarget", "Pick"))
					]
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
				[
					SNew(STextBlock).Text(LOCTEXT("SurfUV", "UV Channel"))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SAssignNew(SurfUVInput, SSpinBox<int32>).MinValue(0).MaxValue(7).Value(0)
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
				[
					SNew(STextBlock).Text(LOCTEXT("SurfSlots", "Material Slots (comma)"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SAssignNew(SurfSlotsInput, SEditableTextBox)
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
				[
					SNew(STextBlock).Text(LOCTEXT("SurfMesh", "Mesh Component Name (optional)"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SAssignNew(SurfMeshInput, SEditableTextBox)
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,4,0,2)
			[
				SAssignNew(SurfEnabledInput, SCheckBox).IsChecked(ECheckBoxState::Checked)
				[
					SNew(STextBlock).Text(LOCTEXT("SurfEnabled", "Enabled"))
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,6,0)
				[
					SNew(SButton)
					.Text_Lambda([this]() { return SelectedSurfaceId.IsEmpty() ? LOCTEXT("SurfCreate", "Create Surface") : LOCTEXT("SurfSave", "Save Surface"); })
					.OnClicked_Lambda([this]()
					{
						if (!GEngine) return FReply::Handled();
						URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
						if (!Subsystem) return FReply::Handled();
						if (URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager())
						{
							FRshipMappingSurfaceState State;
							State.Id = SelectedSurfaceId;
							State.Name = SurfNameInput.IsValid() ? SurfNameInput->GetText().ToString() : TEXT("");
							State.ProjectId = SurfProjectInput.IsValid() ? SurfProjectInput->GetText().ToString() : TEXT("");
							const FString TargetInput = SurfTargetInput.IsValid() ? SurfTargetInput->GetText().ToString() : TEXT("");
							State.TargetId = ResolveTargetIdInput(TargetInput);
							State.UVChannel = SurfUVInput.IsValid() ? SurfUVInput->GetValue() : 0;
							State.MeshComponentName = SurfMeshInput.IsValid() ? SurfMeshInput->GetText().ToString() : TEXT("");
							State.bEnabled = !SurfEnabledInput.IsValid() || SurfEnabledInput->IsChecked();

							if (SurfSlotsInput.IsValid())
							{
								TArray<FString> Parts;
								SurfSlotsInput->GetText().ToString().ParseIntoArray(Parts, TEXT(","), true);
								State.MaterialSlots.Empty();
								for (const FString& P : Parts)
								{
									if (!P.IsEmpty())
									{
										State.MaterialSlots.Add(FCString::Atoi(*P));
									}
								}
							}

							if (State.Id.IsEmpty())
							{
								SelectedSurfaceId = Manager->CreateMappingSurface(State);
							}
							else
							{
								Manager->UpdateMappingSurface(State);
							}
							RefreshStatus();
						}
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("SurfReset", "New"))
					.OnClicked_Lambda([this]()
					{
						SelectedSurfaceId.Reset();
						ResetForms();
						return FReply::Handled();
					})
				]
			]
		];
}

TSharedRef<SWidget> SRshipContentMappingPanel::BuildMappingForm()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(LOCTEXT("MapFormTitle", "Surface Mapping")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
				[
					SNew(STextBlock).Text(LOCTEXT("MapName", "Name"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SAssignNew(MapNameInput, SEditableTextBox)
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
				[
					SNew(STextBlock).Text(LOCTEXT("MapProject", "ProjectId"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SAssignNew(MapProjectInput, SEditableTextBox)
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
				[
					SNew(STextBlock).Text(LOCTEXT("MapType", "Type (surface-uv/surface-projection)"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SAssignNew(MapTypeInput, SEditableTextBox).Text(FText::FromString(TEXT("surface-uv")))
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
				[
					SNew(STextBlock).Text(LOCTEXT("MapContext", "ContextId"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SAssignNew(MapContextInput, SEditableTextBox)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(6,0,0,0)
				[
					SNew(SComboButton)
					.OnGetMenuContent_Lambda([this]()
					{
						return BuildIdPickerMenu(ContextOptions, LOCTEXT("MapNoContexts", "No contexts found"), MapContextInput, false);
					})
					.ButtonContent()
					[
						SNew(STextBlock).Text(LOCTEXT("MapPickContext", "Pick"))
					]
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
				[
					SNew(STextBlock).Text(LOCTEXT("MapSurfaces", "SurfaceIds (comma)"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SAssignNew(MapSurfacesInput, SEditableTextBox)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(6,0,0,0)
				[
					SNew(SComboButton)
					.OnGetMenuContent_Lambda([this]()
					{
						return BuildIdPickerMenu(SurfaceOptions, LOCTEXT("MapNoSurfaces", "No surfaces found"), MapSurfacesInput, true);
					})
					.ButtonContent()
					[
						SNew(STextBlock).Text(LOCTEXT("MapAddSurface", "Add"))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4,0,0,0)
				[
					SNew(SButton)
					.Text(LOCTEXT("MapClearSurfaces", "Clear"))
					.OnClicked_Lambda([this]()
					{
						if (MapSurfacesInput.IsValid())
						{
							MapSurfacesInput->SetText(FText::GetEmpty());
						}
						return FReply::Handled();
					})
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
				[
					SNew(STextBlock).Text(LOCTEXT("MapOpacity", "Opacity"))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SAssignNew(MapOpacityInput, SSpinBox<float>).MinValue(0.0f).MaxValue(1.0f).Delta(0.05f).Value(1.0f)
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,4,0,2)
			[
				SAssignNew(MapEnabledInput, SCheckBox).IsChecked(ECheckBoxState::Checked)
				[
					SNew(STextBlock).Text(LOCTEXT("MapEnabled", "Enabled"))
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,6,0)
				[
					SNew(SButton)
					.Text_Lambda([this]() { return SelectedMappingId.IsEmpty() ? LOCTEXT("MapCreate", "Create Mapping") : LOCTEXT("MapSave", "Save Mapping"); })
					.OnClicked_Lambda([this]()
					{
						if (!GEngine) return FReply::Handled();
						URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
						if (!Subsystem) return FReply::Handled();
						if (URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager())
						{
							FRshipContentMappingState State;
							State.Id = SelectedMappingId;
							State.Name = MapNameInput.IsValid() ? MapNameInput->GetText().ToString() : TEXT("");
							State.ProjectId = MapProjectInput.IsValid() ? MapProjectInput->GetText().ToString() : TEXT("");
							State.Type = MapTypeInput.IsValid() ? MapTypeInput->GetText().ToString() : TEXT("surface-uv");
							State.ContextId = MapContextInput.IsValid() ? MapContextInput->GetText().ToString() : TEXT("");
							State.Opacity = MapOpacityInput.IsValid() ? MapOpacityInput->GetValue() : 1.0f;
							State.bEnabled = !MapEnabledInput.IsValid() || MapEnabledInput->IsChecked();

							if (MapSurfacesInput.IsValid())
							{
								TArray<FString> Parts;
								MapSurfacesInput->GetText().ToString().ParseIntoArray(Parts, TEXT(","), true);
								State.SurfaceIds = Parts;
							}

							// Build config
							TSharedPtr<FJsonObject> Config = MakeShared<FJsonObject>();
							if (State.Type == TEXT("surface-uv"))
							{
								TSharedPtr<FJsonObject> Uv = MakeShared<FJsonObject>();
								Uv->SetNumberField(TEXT("scaleU"), MapUvScaleUInput.IsValid() ? MapUvScaleUInput->GetValue() : 1.0);
								Uv->SetNumberField(TEXT("scaleV"), MapUvScaleVInput.IsValid() ? MapUvScaleVInput->GetValue() : 1.0);
								Uv->SetNumberField(TEXT("offsetU"), MapUvOffsetUInput.IsValid() ? MapUvOffsetUInput->GetValue() : 0.0);
								Uv->SetNumberField(TEXT("offsetV"), MapUvOffsetVInput.IsValid() ? MapUvOffsetVInput->GetValue() : 0.0);
								Uv->SetNumberField(TEXT("rotationDeg"), MapUvRotInput.IsValid() ? MapUvRotInput->GetValue() : 0.0);
								Config->SetObjectField(TEXT("uvTransform"), Uv);
							}
							else if (State.Type == TEXT("surface-projection"))
							{
								const FString ProjType = MapProjectionTypeInput.IsValid() ? MapProjectionTypeInput->GetText().ToString() : TEXT("perspective");
								Config->SetStringField(TEXT("projectionType"), ProjType);

								TSharedPtr<FJsonObject> Pos = MakeShared<FJsonObject>();
								Pos->SetNumberField(TEXT("x"), MapProjPosXInput.IsValid() ? MapProjPosXInput->GetValue() : 0.0);
								Pos->SetNumberField(TEXT("y"), MapProjPosYInput.IsValid() ? MapProjPosYInput->GetValue() : 0.0);
								Pos->SetNumberField(TEXT("z"), MapProjPosZInput.IsValid() ? MapProjPosZInput->GetValue() : 0.0);
								Config->SetObjectField(TEXT("projectorPosition"), Pos);

								TSharedPtr<FJsonObject> Rot = MakeShared<FJsonObject>();
								Rot->SetNumberField(TEXT("x"), MapProjRotXInput.IsValid() ? MapProjRotXInput->GetValue() : 0.0);
								Rot->SetNumberField(TEXT("y"), MapProjRotYInput.IsValid() ? MapProjRotYInput->GetValue() : 0.0);
								Rot->SetNumberField(TEXT("z"), MapProjRotZInput.IsValid() ? MapProjRotZInput->GetValue() : 0.0);
								Config->SetObjectField(TEXT("projectorRotation"), Rot);

								Config->SetNumberField(TEXT("fov"), MapProjFovInput.IsValid() ? MapProjFovInput->GetValue() : 60.0);
								Config->SetNumberField(TEXT("aspectRatio"), MapProjAspectInput.IsValid() ? MapProjAspectInput->GetValue() : 1.7778);
								Config->SetNumberField(TEXT("near"), MapProjNearInput.IsValid() ? MapProjNearInput->GetValue() : 10.0);
								Config->SetNumberField(TEXT("far"), MapProjFarInput.IsValid() ? MapProjFarInput->GetValue() : 10000.0);

								const FString Axis = MapCylAxisInput.IsValid() ? MapCylAxisInput->GetText().ToString() : TEXT("");
								if (!Axis.IsEmpty())
								{
									TSharedPtr<FJsonObject> Cyl = MakeShared<FJsonObject>();
									Cyl->SetStringField(TEXT("axis"), Axis);
									Cyl->SetNumberField(TEXT("radius"), MapCylRadiusInput.IsValid() ? MapCylRadiusInput->GetValue() : 100.0);
									Cyl->SetNumberField(TEXT("height"), MapCylHeightInput.IsValid() ? MapCylHeightInput->GetValue() : 1000.0);
									Cyl->SetNumberField(TEXT("startAngle"), MapCylStartInput.IsValid() ? MapCylStartInput->GetValue() : 0.0);
									Cyl->SetNumberField(TEXT("endAngle"), MapCylEndInput.IsValid() ? MapCylEndInput->GetValue() : 90.0);
									Config->SetObjectField(TEXT("cylindrical"), Cyl);
								}
							}
							State.Config = Config;

							if (State.Id.IsEmpty())
							{
								SelectedMappingId = Manager->CreateMapping(State);
							}
							else
							{
								Manager->UpdateMapping(State);
							}
							RefreshStatus();
						}
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("MapReset", "New"))
					.OnClicked_Lambda([this]()
					{
						SelectedMappingId.Reset();
						ResetForms();
						return FReply::Handled();
					})
				]
			]
		];
}

void SRshipContentMappingPanel::ResetForms()
{
	QuickSourceType = TEXT("camera");
	QuickMappingType = TEXT("surface-uv");
	bQuickAdvanced = false;
	if (QuickProjectIdInput.IsValid()) QuickProjectIdInput->SetText(FText::GetEmpty());
	if (QuickSourceIdInput.IsValid()) QuickSourceIdInput->SetText(FText::GetEmpty());
	if (QuickTargetIdInput.IsValid()) QuickTargetIdInput->SetText(FText::GetEmpty());
	if (QuickWidthInput.IsValid()) QuickWidthInput->SetValue(1920);
	if (QuickHeightInput.IsValid()) QuickHeightInput->SetValue(1080);
	if (QuickCaptureModeInput.IsValid()) QuickCaptureModeInput->SetText(FText::FromString(TEXT("FinalColorLDR")));
	if (QuickUvChannelInput.IsValid()) QuickUvChannelInput->SetValue(0);
	if (QuickMaterialSlotsInput.IsValid()) QuickMaterialSlotsInput->SetText(FText::GetEmpty());
	if (QuickMeshNameInput.IsValid()) QuickMeshNameInput->SetText(FText::GetEmpty());
	if (QuickOpacityInput.IsValid()) QuickOpacityInput->SetValue(1.0f);

	if (CtxNameInput.IsValid()) CtxNameInput->SetText(FText::GetEmpty());
	if (CtxProjectInput.IsValid()) CtxProjectInput->SetText(FText::GetEmpty());
	if (CtxSourceTypeInput.IsValid()) CtxSourceTypeInput->SetText(FText::FromString(TEXT("camera")));
	if (CtxCameraInput.IsValid()) CtxCameraInput->SetText(FText::GetEmpty());
	if (CtxAssetInput.IsValid()) CtxAssetInput->SetText(FText::GetEmpty());
	if (CtxWidthInput.IsValid()) CtxWidthInput->SetValue(1920);
	if (CtxHeightInput.IsValid()) CtxHeightInput->SetValue(1080);
	if (CtxCaptureInput.IsValid()) CtxCaptureInput->SetText(FText::FromString(TEXT("FinalColorLDR")));
	if (CtxEnabledInput.IsValid()) CtxEnabledInput->SetIsChecked(ECheckBoxState::Checked);

	if (SurfNameInput.IsValid()) SurfNameInput->SetText(FText::GetEmpty());
	if (SurfProjectInput.IsValid()) SurfProjectInput->SetText(FText::GetEmpty());
	if (SurfTargetInput.IsValid()) SurfTargetInput->SetText(FText::GetEmpty());
	if (SurfUVInput.IsValid()) SurfUVInput->SetValue(0);
	if (SurfSlotsInput.IsValid()) SurfSlotsInput->SetText(FText::GetEmpty());
	if (SurfMeshInput.IsValid()) SurfMeshInput->SetText(FText::GetEmpty());
	if (SurfEnabledInput.IsValid()) SurfEnabledInput->SetIsChecked(ECheckBoxState::Checked);

	if (MapNameInput.IsValid()) MapNameInput->SetText(FText::GetEmpty());
	if (MapProjectInput.IsValid()) MapProjectInput->SetText(FText::GetEmpty());
	if (MapTypeInput.IsValid()) MapTypeInput->SetText(FText::FromString(TEXT("surface-uv")));
	if (MapProjectionTypeInput.IsValid()) MapProjectionTypeInput->SetText(FText::FromString(TEXT("perspective")));
	if (MapContextInput.IsValid()) MapContextInput->SetText(FText::GetEmpty());
	if (MapSurfacesInput.IsValid()) MapSurfacesInput->SetText(FText::GetEmpty());
	if (MapOpacityInput.IsValid()) MapOpacityInput->SetValue(1.0f);
	if (MapEnabledInput.IsValid()) MapEnabledInput->SetIsChecked(ECheckBoxState::Checked);
	if (MapProjPosXInput.IsValid()) MapProjPosXInput->SetValue(0.f);
	if (MapProjPosYInput.IsValid()) MapProjPosYInput->SetValue(0.f);
	if (MapProjPosZInput.IsValid()) MapProjPosZInput->SetValue(0.f);
	if (MapProjRotXInput.IsValid()) MapProjRotXInput->SetValue(0.f);
	if (MapProjRotYInput.IsValid()) MapProjRotYInput->SetValue(0.f);
	if (MapProjRotZInput.IsValid()) MapProjRotZInput->SetValue(0.f);
	if (MapProjFovInput.IsValid()) MapProjFovInput->SetValue(60.f);
	if (MapProjAspectInput.IsValid()) MapProjAspectInput->SetValue(1.7778f);
	if (MapProjNearInput.IsValid()) MapProjNearInput->SetValue(10.f);
	if (MapProjFarInput.IsValid()) MapProjFarInput->SetValue(10000.f);
	if (MapCylAxisInput.IsValid()) MapCylAxisInput->SetText(FText::FromString(TEXT("y")));
	if (MapCylRadiusInput.IsValid()) MapCylRadiusInput->SetValue(100.f);
	if (MapCylHeightInput.IsValid()) MapCylHeightInput->SetValue(1000.f);
	if (MapCylStartInput.IsValid()) MapCylStartInput->SetValue(0.f);
	if (MapCylEndInput.IsValid()) MapCylEndInput->SetValue(90.f);
	if (MapUvScaleUInput.IsValid()) MapUvScaleUInput->SetValue(1.f);
	if (MapUvScaleVInput.IsValid()) MapUvScaleVInput->SetValue(1.f);
	if (MapUvOffsetUInput.IsValid()) MapUvOffsetUInput->SetValue(0.f);
	if (MapUvOffsetVInput.IsValid()) MapUvOffsetVInput->SetValue(0.f);
	if (MapUvRotInput.IsValid()) MapUvRotInput->SetValue(0.f);
}

void SRshipContentMappingPanel::PopulateContextForm(const FRshipRenderContextState& State)
{
	SelectedContextId = State.Id;
	if (CtxNameInput.IsValid()) CtxNameInput->SetText(FText::FromString(State.Name));
	if (CtxProjectInput.IsValid()) CtxProjectInput->SetText(FText::FromString(State.ProjectId));
	if (CtxSourceTypeInput.IsValid()) CtxSourceTypeInput->SetText(FText::FromString(State.SourceType));
	if (CtxCameraInput.IsValid()) CtxCameraInput->SetText(FText::FromString(State.CameraId));
	if (CtxAssetInput.IsValid()) CtxAssetInput->SetText(FText::FromString(State.AssetId));
	if (CtxWidthInput.IsValid()) CtxWidthInput->SetValue(State.Width);
	if (CtxHeightInput.IsValid()) CtxHeightInput->SetValue(State.Height);
	if (CtxCaptureInput.IsValid()) CtxCaptureInput->SetText(FText::FromString(State.CaptureMode));
	if (CtxEnabledInput.IsValid()) CtxEnabledInput->SetIsChecked(State.bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
}

void SRshipContentMappingPanel::PopulateSurfaceForm(const FRshipMappingSurfaceState& State)
{
	SelectedSurfaceId = State.Id;
	if (SurfNameInput.IsValid()) SurfNameInput->SetText(FText::FromString(State.Name));
	if (SurfProjectInput.IsValid()) SurfProjectInput->SetText(FText::FromString(State.ProjectId));
	if (SurfTargetInput.IsValid()) SurfTargetInput->SetText(FText::FromString(ShortTargetLabel(State.TargetId)));
	if (SurfUVInput.IsValid()) SurfUVInput->SetValue(State.UVChannel);
	if (SurfSlotsInput.IsValid())
	{
		FString Slots;
		for (int32 Slot : State.MaterialSlots)
		{
			if (!Slots.IsEmpty()) Slots += TEXT(",");
			Slots += FString::FromInt(Slot);
		}
		SurfSlotsInput->SetText(FText::FromString(Slots));
	}
	if (SurfMeshInput.IsValid()) SurfMeshInput->SetText(FText::FromString(State.MeshComponentName));
	if (SurfEnabledInput.IsValid()) SurfEnabledInput->SetIsChecked(State.bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
}

void SRshipContentMappingPanel::PopulateMappingForm(const FRshipContentMappingState& State)
{
	SelectedMappingId = State.Id;
	if (MapNameInput.IsValid()) MapNameInput->SetText(FText::FromString(State.Name));
	if (MapProjectInput.IsValid()) MapProjectInput->SetText(FText::FromString(State.ProjectId));
	if (MapTypeInput.IsValid()) MapTypeInput->SetText(FText::FromString(State.Type));
	if (MapProjectionTypeInput.IsValid()) MapProjectionTypeInput->SetText(FText::FromString(TEXT("perspective")));
	if (MapContextInput.IsValid()) MapContextInput->SetText(FText::FromString(State.ContextId));
	if (MapSurfacesInput.IsValid())
	{
		FString Surfaces = FString::Join(State.SurfaceIds, TEXT(","));
		MapSurfacesInput->SetText(FText::FromString(Surfaces));
	}
	if (MapOpacityInput.IsValid()) MapOpacityInput->SetValue(State.Opacity);
	if (MapEnabledInput.IsValid()) MapEnabledInput->SetIsChecked(State.bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);

	if (State.Config.IsValid())
	{
		if (State.Type == TEXT("surface-uv") && State.Config->HasTypedField<EJson::Object>(TEXT("uvTransform")))
		{
			TSharedPtr<FJsonObject> Uv = State.Config->GetObjectField(TEXT("uvTransform"));
			if (MapUvScaleUInput.IsValid()) MapUvScaleUInput->SetValue(Uv->GetNumberField(TEXT("scaleU")));
			if (MapUvScaleVInput.IsValid()) MapUvScaleVInput->SetValue(Uv->GetNumberField(TEXT("scaleV")));
			if (MapUvOffsetUInput.IsValid()) MapUvOffsetUInput->SetValue(Uv->GetNumberField(TEXT("offsetU")));
			if (MapUvOffsetVInput.IsValid()) MapUvOffsetVInput->SetValue(Uv->GetNumberField(TEXT("offsetV")));
			if (MapUvRotInput.IsValid()) MapUvRotInput->SetValue(Uv->GetNumberField(TEXT("rotationDeg")));
		}
		else if (State.Type == TEXT("surface-projection"))
		{
			if (State.Config->HasTypedField<EJson::String>(TEXT("projectionType")) && MapProjectionTypeInput.IsValid())
			{
				MapProjectionTypeInput->SetText(FText::FromString(State.Config->GetStringField(TEXT("projectionType"))));
			}
			auto GetNum = [](const TSharedPtr<FJsonObject>& Obj, const FString& Field, double DefaultVal)->double
			{
				return (Obj.IsValid() && Obj->HasTypedField<EJson::Number>(Field)) ? Obj->GetNumberField(Field) : DefaultVal;
			};
			if (State.Config->HasTypedField<EJson::Object>(TEXT("projectorPosition")))
			{
				TSharedPtr<FJsonObject> Pos = State.Config->GetObjectField(TEXT("projectorPosition"));
				if (MapProjPosXInput.IsValid()) MapProjPosXInput->SetValue(GetNum(Pos, TEXT("x"), 0.0));
				if (MapProjPosYInput.IsValid()) MapProjPosYInput->SetValue(GetNum(Pos, TEXT("y"), 0.0));
				if (MapProjPosZInput.IsValid()) MapProjPosZInput->SetValue(GetNum(Pos, TEXT("z"), 0.0));
			}
			if (State.Config->HasTypedField<EJson::Object>(TEXT("projectorRotation")))
			{
				TSharedPtr<FJsonObject> Rot = State.Config->GetObjectField(TEXT("projectorRotation"));
				if (MapProjRotXInput.IsValid()) MapProjRotXInput->SetValue(GetNum(Rot, TEXT("x"), 0.0));
				if (MapProjRotYInput.IsValid()) MapProjRotYInput->SetValue(GetNum(Rot, TEXT("y"), 0.0));
				if (MapProjRotZInput.IsValid()) MapProjRotZInput->SetValue(GetNum(Rot, TEXT("z"), 0.0));
			}
			if (MapProjFovInput.IsValid()) MapProjFovInput->SetValue(GetNum(State.Config, TEXT("fov"), 60.0));
			if (MapProjAspectInput.IsValid()) MapProjAspectInput->SetValue(GetNum(State.Config, TEXT("aspectRatio"), 1.7778));
			if (MapProjNearInput.IsValid()) MapProjNearInput->SetValue(GetNum(State.Config, TEXT("near"), 10.0));
			if (MapProjFarInput.IsValid()) MapProjFarInput->SetValue(GetNum(State.Config, TEXT("far"), 10000.0));

			if (State.Config->HasTypedField<EJson::Object>(TEXT("cylindrical")))
			{
				TSharedPtr<FJsonObject> Cyl = State.Config->GetObjectField(TEXT("cylindrical"));
				if (MapCylAxisInput.IsValid() && Cyl->HasTypedField<EJson::String>(TEXT("axis"))) MapCylAxisInput->SetText(FText::FromString(Cyl->GetStringField(TEXT("axis"))));
				if (MapCylRadiusInput.IsValid()) MapCylRadiusInput->SetValue(GetNum(Cyl, TEXT("radius"), 100.0));
				if (MapCylHeightInput.IsValid()) MapCylHeightInput->SetValue(GetNum(Cyl, TEXT("height"), 1000.0));
				if (MapCylStartInput.IsValid()) MapCylStartInput->SetValue(GetNum(Cyl, TEXT("startAngle"), 0.0));
				if (MapCylEndInput.IsValid()) MapCylEndInput->SetValue(GetNum(Cyl, TEXT("endAngle"), 90.0));
			}
		}
	}
}

void SRshipContentMappingPanel::RefreshStatus()
{
	if (!GEngine)
	{
		return;
	}

	URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	if (!Subsystem)
	{
		if (ConnectionText.IsValid())
		{
			ConnectionText->SetText(LOCTEXT("SubsystemMissing", "Status: Subsystem unavailable"));
		}
		return;
	}

	const bool bConnected = Subsystem->IsConnected();
	if (ConnectionText.IsValid())
	{
		ConnectionText->SetText(bConnected ? LOCTEXT("Connected", "Status: Connected") : LOCTEXT("Disconnected", "Status: Offline"));
		ConnectionText->SetColorAndOpacity(bConnected ? FLinearColor::Green : FLinearColor::Yellow);
	}

	URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
	if (!Manager)
	{
		if (CountsText.IsValid())
		{
			CountsText->SetText(LOCTEXT("ContentMappingDisabled", "Content mapping is disabled"));
		}
		if (ContextList.IsValid())
		{
			ContextList->ClearChildren();
			ContextList->AddSlot()[SNew(STextBlock).Text(LOCTEXT("ContextsDisabled", "No contexts (disabled)"))];
		}
		if (SurfaceList.IsValid())
		{
			SurfaceList->ClearChildren();
			SurfaceList->AddSlot()[SNew(STextBlock).Text(LOCTEXT("SurfacesDisabled", "No surfaces (disabled)"))];
		}
		if (MappingList.IsValid())
		{
			MappingList->ClearChildren();
			MappingList->AddSlot()[SNew(STextBlock).Text(LOCTEXT("MappingsDisabled", "No mappings (disabled)"))];
		}
		return;
	}

	const TArray<FRshipRenderContextState> Contexts = Manager->GetRenderContexts();
	const TArray<FRshipMappingSurfaceState> Surfaces = Manager->GetMappingSurfaces();
	const TArray<FRshipContentMappingState> Mappings = Manager->GetMappings();
	RebuildPickerOptions(Contexts, Surfaces);

	if (CountsText.IsValid())
	{
		CountsText->SetText(FText::Format(
			LOCTEXT("CountsFormat", "Contexts: {0}  Surfaces: {1}  Mappings: {2}"),
			FText::AsNumber(Contexts.Num()),
			FText::AsNumber(Surfaces.Num()),
			FText::AsNumber(Mappings.Num())));
	}

	if (ContextList.IsValid())
	{
		ContextList->ClearChildren();
		if (Contexts.Num() == 0)
		{
			ContextList->AddSlot()[SNew(STextBlock).Text(LOCTEXT("NoContexts", "No render contexts"))];
		}
		else
		{
			// Quick-create row
			{
				TSharedPtr<SEditableTextBox> NameBox;
				TSharedPtr<SEditableTextBox> ProjectBox;
				TSharedPtr<SEditableTextBox> SourceBox;
				TSharedPtr<SEditableTextBox> CameraBox;
				TSharedPtr<SEditableTextBox> AssetBox;
				TSharedPtr<SSpinBox<int32>> WidthBox;
				TSharedPtr<SSpinBox<int32>> HeightBox;
				TSharedPtr<SEditableTextBox> CaptureBox;
				TSharedPtr<SCheckBox> EnabledBox;

				ContextList->AddSlot()
				.AutoHeight()
				.Padding(0, 0, 0, 6)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,4,0)
					[
						SNew(STextBlock).Text(LOCTEXT("CtxNewLabel", "New"))
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0,0,4,0)
					[
						SAssignNew(NameBox, SEditableTextBox).HintText(LOCTEXT("CtxNameHint", "Name"))
					]
					+ SHorizontalBox::Slot().FillWidth(0.8f).Padding(0,0,4,0)
					[
						SAssignNew(ProjectBox, SEditableTextBox).HintText(LOCTEXT("CtxProjectHint", "ProjectId"))
					]
					+ SHorizontalBox::Slot().FillWidth(0.8f).Padding(0,0,4,0)
					[
						SAssignNew(SourceBox, SEditableTextBox).Text(FText::FromString(TEXT("camera")))
					]
					+ SHorizontalBox::Slot().FillWidth(0.8f).Padding(0,0,4,0)
					[
						SAssignNew(CameraBox, SEditableTextBox).HintText(LOCTEXT("CtxCamHint", "CameraId"))
					]
					+ SHorizontalBox::Slot().FillWidth(0.8f).Padding(0,0,4,0)
					[
						SAssignNew(AssetBox, SEditableTextBox).HintText(LOCTEXT("CtxAssetHint", "AssetId"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(WidthBox, SSpinBox<int32>).MinValue(0).MaxValue(8192).Value(1920)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(HeightBox, SSpinBox<int32>).MinValue(0).MaxValue(8192).Value(1080)
					]
					+ SHorizontalBox::Slot().FillWidth(0.8f).Padding(0,0,4,0)
					[
						SAssignNew(CaptureBox, SEditableTextBox).Text(FText::FromString(TEXT("FinalColorLDR")))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,4,0)
					[
						SAssignNew(EnabledBox, SCheckBox).IsChecked(ECheckBoxState::Checked)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("CtxCreateBtn", "Create"))
						.OnClicked_Lambda([this, NameBox, ProjectBox, SourceBox, CameraBox, AssetBox, WidthBox, HeightBox, CaptureBox, EnabledBox]()
						{
							if (!GEngine) return FReply::Handled();
							if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
							{
								if (URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager())
								{
									FRshipRenderContextState State;
									State.Name = NameBox.IsValid() ? NameBox->GetText().ToString() : TEXT("");
									State.ProjectId = ProjectBox.IsValid() ? ProjectBox->GetText().ToString() : TEXT("");
									State.SourceType = SourceBox.IsValid() ? SourceBox->GetText().ToString() : TEXT("camera");
									State.CameraId = CameraBox.IsValid() ? CameraBox->GetText().ToString() : TEXT("");
									State.AssetId = AssetBox.IsValid() ? AssetBox->GetText().ToString() : TEXT("");
									State.Width = WidthBox.IsValid() ? WidthBox->GetValue() : 0;
									State.Height = HeightBox.IsValid() ? HeightBox->GetValue() : 0;
									State.CaptureMode = CaptureBox.IsValid() ? CaptureBox->GetText().ToString() : TEXT("");
									State.bEnabled = !EnabledBox.IsValid() || EnabledBox->IsChecked();
									SelectedContextId = Manager->CreateRenderContext(State);
									RefreshStatus();
								}
							}
							return FReply::Handled();
						})
					]
				];
			}

			for (const FRshipRenderContextState& Context : Contexts)
			{
				const FString Name = Context.Name.IsEmpty() ? Context.Id : Context.Name;
				const FString Status = Context.bEnabled ? TEXT("enabled") : TEXT("disabled");
				const FString ErrorSuffix = Context.LastError.IsEmpty() ? TEXT("") : FString::Printf(TEXT(" - %s"), *Context.LastError);
				const FString Line = FString::Printf(TEXT("%s [%s] (%s)%s"), *Name, *Context.SourceType, *Status, *ErrorSuffix);

				// Per-row edit controls
				TSharedPtr<SEditableTextBox> NameBox;
				TSharedPtr<SEditableTextBox> ProjectBox;
				TSharedPtr<SEditableTextBox> SourceBox;
				TSharedPtr<SEditableTextBox> CameraBox;
				TSharedPtr<SEditableTextBox> AssetBox;
				TSharedPtr<SSpinBox<int32>> WidthBox;
				TSharedPtr<SSpinBox<int32>> HeightBox;
				TSharedPtr<SEditableTextBox> CaptureBox;
				TSharedPtr<SCheckBox> EnabledBox;

				ContextList->AddSlot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0,0,4,0)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
								.BorderBackgroundColor(Context.SourceType == TEXT("camera") ? FLinearColor(0.2f,0.8f,0.4f,1.0f) : FLinearColor(0.8f,0.6f,0.2f,1.0f))
								.Padding(FMargin(4,1))
								[
									SNew(STextBlock)
									.Text(Context.SourceType == TEXT("camera") ? LOCTEXT("BadgeCam", "CAM") : LOCTEXT("BadgeAsset", "ASSET"))
									.ColorAndOpacity(FLinearColor::Black)
								]
							]
							+ SHorizontalBox::Slot().FillWidth(1.0f)
							[
								SAssignNew(NameBox, SEditableTextBox).Text(FText::FromString(Context.Name))
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
						[
							SAssignNew(ProjectBox, SEditableTextBox).Text(FText::FromString(Context.ProjectId))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
						[
							SAssignNew(SourceBox, SEditableTextBox).Text(FText::FromString(Context.SourceType))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
						[
							SAssignNew(CameraBox, SEditableTextBox).Text(FText::FromString(Context.CameraId))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
						[
							SAssignNew(AssetBox, SEditableTextBox).Text(FText::FromString(Context.AssetId))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SAssignNew(WidthBox, SSpinBox<int32>).MinValue(0).MaxValue(8192).Value(Context.Width)
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(4,0,0,0)
							[
								SAssignNew(HeightBox, SSpinBox<int32>).MinValue(0).MaxValue(8192).Value(Context.Height)
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
						[
							SAssignNew(CaptureBox, SEditableTextBox).Text(FText::FromString(Context.CaptureMode))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
						[
							SAssignNew(EnabledBox, SCheckBox).IsChecked(Context.bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
							[
								SNew(STextBlock).Text(FText::FromString(Line)).ColorAndOpacity(Context.LastError.IsEmpty() ? FLinearColor::White : FLinearColor::Red)
							]
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,4,0)
					[
						SNew(SButton)
						.Text(LOCTEXT("CtxSaveInline", "Save"))
						.OnClicked_Lambda([this, Context, NameBox, ProjectBox, SourceBox, CameraBox, AssetBox, WidthBox, HeightBox, CaptureBox, EnabledBox]()
						{
							if (!GEngine) return FReply::Handled();
							if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
							{
								if (URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager())
								{
									FRshipRenderContextState State = Context;
									State.Name = NameBox.IsValid() ? NameBox->GetText().ToString() : State.Name;
									State.ProjectId = ProjectBox.IsValid() ? ProjectBox->GetText().ToString() : State.ProjectId;
									State.SourceType = SourceBox.IsValid() ? SourceBox->GetText().ToString() : State.SourceType;
									State.CameraId = CameraBox.IsValid() ? CameraBox->GetText().ToString() : State.CameraId;
									State.AssetId = AssetBox.IsValid() ? AssetBox->GetText().ToString() : State.AssetId;
									State.Width = WidthBox.IsValid() ? WidthBox->GetValue() : State.Width;
									State.Height = HeightBox.IsValid() ? HeightBox->GetValue() : State.Height;
									State.CaptureMode = CaptureBox.IsValid() ? CaptureBox->GetText().ToString() : State.CaptureMode;
									State.bEnabled = !EnabledBox.IsValid() || EnabledBox->IsChecked();
									Manager->UpdateRenderContext(State);
									RefreshStatus();
								}
							}
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("CtxDeleteInline", "Delete"))
						.OnClicked_Lambda([this, Context]()
						{
							if (!GEngine) return FReply::Handled();
							if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
							{
								if (URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager())
								{
									Manager->DeleteRenderContext(Context.Id);
									if (SelectedContextId == Context.Id) { SelectedContextId.Reset(); }
									RefreshStatus();
								}
							}
							return FReply::Handled();
						})
					]
				];
			}
		}
	}

	if (SurfaceList.IsValid())
	{
		SurfaceList->ClearChildren();
		if (Surfaces.Num() == 0)
		{
			SurfaceList->AddSlot()[SNew(STextBlock).Text(LOCTEXT("NoSurfaces", "No mapping surfaces"))];
		}
		else
		{
			// Quick-create surface
			{
				TSharedPtr<SEditableTextBox> NameBox;
				TSharedPtr<SEditableTextBox> ProjectBox;
				TSharedPtr<SEditableTextBox> TargetBox;
				TSharedPtr<SSpinBox<int32>> UVBox;
				TSharedPtr<SEditableTextBox> SlotsBox;
				TSharedPtr<SEditableTextBox> MeshBox;
				TSharedPtr<SCheckBox> EnabledBox;

				SurfaceList->AddSlot()
				.AutoHeight()
				.Padding(0,0,0,6)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,4,0)
					[
						SNew(STextBlock).Text(LOCTEXT("SurfNew", "New"))
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0,0,4,0)
					[
						SAssignNew(NameBox, SEditableTextBox).HintText(LOCTEXT("SurfNameHint", "Name"))
					]
					+ SHorizontalBox::Slot().FillWidth(0.8f).Padding(0,0,4,0)
					[
						SAssignNew(ProjectBox, SEditableTextBox).HintText(LOCTEXT("SurfProjHint", "ProjectId"))
					]
					+ SHorizontalBox::Slot().FillWidth(0.8f).Padding(0,0,4,0)
					[
					SAssignNew(TargetBox, SEditableTextBox).HintText(LOCTEXT("SurfTargetHint", "Pick or type target name"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(UVBox, SSpinBox<int32>).MinValue(0).MaxValue(7).Value(0)
					]
					+ SHorizontalBox::Slot().FillWidth(0.8f).Padding(0,0,4,0)
					[
						SAssignNew(SlotsBox, SEditableTextBox).HintText(LOCTEXT("SurfSlotsHint", "Slots comma"))
					]
					+ SHorizontalBox::Slot().FillWidth(0.8f).Padding(0,0,4,0)
					[
						SAssignNew(MeshBox, SEditableTextBox).HintText(LOCTEXT("SurfMeshHint", "Mesh name (opt)"))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,4,0)
					[
						SAssignNew(EnabledBox, SCheckBox).IsChecked(ECheckBoxState::Checked)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("SurfCreateBtn", "Create"))
						.OnClicked_Lambda([this, NameBox, ProjectBox, TargetBox, UVBox, SlotsBox, MeshBox, EnabledBox]()
						{
							if (!GEngine) return FReply::Handled();
							if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
							{
								if (URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager())
								{
									FRshipMappingSurfaceState State;
									State.Name = NameBox.IsValid() ? NameBox->GetText().ToString() : TEXT("");
									State.ProjectId = ProjectBox.IsValid() ? ProjectBox->GetText().ToString() : TEXT("");
									const FString TargetInput = TargetBox.IsValid() ? TargetBox->GetText().ToString() : TEXT("");
									State.TargetId = ResolveTargetIdInput(TargetInput);
									State.UVChannel = UVBox.IsValid() ? UVBox->GetValue() : 0;
									State.MeshComponentName = MeshBox.IsValid() ? MeshBox->GetText().ToString() : TEXT("");
									State.bEnabled = !EnabledBox.IsValid() || EnabledBox->IsChecked();
									if (SlotsBox.IsValid())
									{
										TArray<FString> Parts;
										SlotsBox->GetText().ToString().ParseIntoArray(Parts, TEXT(","), true);
										for (const FString& P : Parts)
										{
											if (!P.IsEmpty()) State.MaterialSlots.Add(FCString::Atoi(*P));
										}
									}
									SelectedSurfaceId = Manager->CreateMappingSurface(State);
									RefreshStatus();
								}
							}
							return FReply::Handled();
						})
					]
				];
			}

			for (const FRshipMappingSurfaceState& Surface : Surfaces)
			{
				const FString Name = Surface.Name.IsEmpty() ? Surface.Id : Surface.Name;
				const FString Status = Surface.bEnabled ? TEXT("enabled") : TEXT("disabled");
				const FString ErrorSuffix = Surface.LastError.IsEmpty() ? TEXT("") : FString::Printf(TEXT(" - %s"), *Surface.LastError);
				const FString Line = FString::Printf(TEXT("%s [uv:%d] (%s)%s"), *Name, Surface.UVChannel, *Status, *ErrorSuffix);

				TSharedPtr<SEditableTextBox> NameBox;
				TSharedPtr<SEditableTextBox> ProjectBox;
				TSharedPtr<SEditableTextBox> TargetBox;
				TSharedPtr<SSpinBox<int32>> UVBox;
				TSharedPtr<SEditableTextBox> SlotsBox;
				TSharedPtr<SEditableTextBox> MeshBox;
				TSharedPtr<SCheckBox> EnabledBox;

				SurfaceList->AddSlot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0,0,4,0)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
								.BorderBackgroundColor(FLinearColor(0.6f,0.7f,1.0f,1.0f))
								.Padding(FMargin(4,1))
								[
									SNew(STextBlock)
									.Text(LOCTEXT("BadgeSurface", "SURF"))
									.ColorAndOpacity(FLinearColor::Black)
								]
							]
							+ SHorizontalBox::Slot().FillWidth(1.0f)
							[
								SAssignNew(NameBox, SEditableTextBox).Text(FText::FromString(Surface.Name))
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
						[
							SAssignNew(ProjectBox, SEditableTextBox).Text(FText::FromString(Surface.ProjectId))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
						[
							SAssignNew(TargetBox, SEditableTextBox).Text(FText::FromString(ShortTargetLabel(Surface.TargetId)))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
						[
							SAssignNew(UVBox, SSpinBox<int32>).MinValue(0).MaxValue(7).Value(Surface.UVChannel)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
						[
							SAssignNew(SlotsBox, SEditableTextBox).Text(FText::FromString(FString::JoinBy(Surface.MaterialSlots, TEXT(","), [](int32 S){ return FString::FromInt(S);})))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
						[
							SAssignNew(MeshBox, SEditableTextBox).Text(FText::FromString(Surface.MeshComponentName))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
						[
							SAssignNew(EnabledBox, SCheckBox).IsChecked(Surface.bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
							[
								SNew(STextBlock).Text(FText::FromString(Line)).ColorAndOpacity(Surface.LastError.IsEmpty() ? FLinearColor::White : FLinearColor::Red)
							]
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,4,0)
					[
						SNew(SButton)
						.Text(LOCTEXT("SurfSaveInline", "Save"))
						.OnClicked_Lambda([this, Surface, NameBox, ProjectBox, TargetBox, UVBox, SlotsBox, MeshBox, EnabledBox]()
						{
							if (!GEngine) return FReply::Handled();
							if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
							{
								if (URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager())
								{
									FRshipMappingSurfaceState State = Surface;
									State.Name = NameBox.IsValid() ? NameBox->GetText().ToString() : State.Name;
									State.ProjectId = ProjectBox.IsValid() ? ProjectBox->GetText().ToString() : State.ProjectId;
									if (TargetBox.IsValid())
									{
										const FString TargetInput = TargetBox->GetText().ToString();
										State.TargetId = ResolveTargetIdInput(TargetInput);
									}
									State.UVChannel = UVBox.IsValid() ? UVBox->GetValue() : State.UVChannel;
									State.MeshComponentName = MeshBox.IsValid() ? MeshBox->GetText().ToString() : State.MeshComponentName;
									State.MaterialSlots.Empty();
									if (SlotsBox.IsValid())
									{
										TArray<FString> Parts;
										SlotsBox->GetText().ToString().ParseIntoArray(Parts, TEXT(","), true);
										for (const FString& P : Parts)
										{
											if (!P.IsEmpty()) State.MaterialSlots.Add(FCString::Atoi(*P));
										}
									}
									State.bEnabled = !EnabledBox.IsValid() || EnabledBox->IsChecked();
									Manager->UpdateMappingSurface(State);
									RefreshStatus();
								}
							}
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("SurfDeleteInline", "Delete"))
						.OnClicked_Lambda([this, Surface]()
						{
							if (!GEngine) return FReply::Handled();
							if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
							{
								if (URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager())
								{
									Manager->DeleteMappingSurface(Surface.Id);
									if (SelectedSurfaceId == Surface.Id) { SelectedSurfaceId.Reset(); }
									RefreshStatus();
								}
							}
							return FReply::Handled();
						})
					]
				];
			}
		}
	}

	if (MappingList.IsValid())
	{
		MappingList->ClearChildren();
		if (Mappings.Num() == 0)
		{
			MappingList->AddSlot()[SNew(STextBlock).Text(LOCTEXT("NoMappings", "No surface mappings"))];
		}
		else
		{
			// Quick-create mapping
			{
				TSharedPtr<SEditableTextBox> NameBox;
				TSharedPtr<SEditableTextBox> ProjectBox;
				TSharedPtr<SEditableTextBox> TypeBox;
				TSharedPtr<SEditableTextBox> ContextBox;
				TSharedPtr<SEditableTextBox> SurfacesBox;
				TSharedPtr<SSpinBox<float>> OpacityBox;
				TSharedPtr<SCheckBox> EnabledBox;

				MappingList->AddSlot()
				.AutoHeight()
				.Padding(0,0,0,6)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,4,0)
					[
						SNew(STextBlock).Text(LOCTEXT("MapNew", "New"))
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0,0,4,0)
					[
						SAssignNew(NameBox, SEditableTextBox).HintText(LOCTEXT("MapNameHint", "Name"))
					]
					+ SHorizontalBox::Slot().FillWidth(0.8f).Padding(0,0,4,0)
					[
						SAssignNew(ProjectBox, SEditableTextBox).HintText(LOCTEXT("MapProjHint", "ProjectId"))
					]
					+ SHorizontalBox::Slot().FillWidth(0.8f).Padding(0,0,4,0)
					[
						SAssignNew(TypeBox, SEditableTextBox).Text(FText::FromString(TEXT("surface-uv")))
					]
					+ SHorizontalBox::Slot().FillWidth(0.8f).Padding(0,0,4,0)
					[
						SAssignNew(ContextBox, SEditableTextBox).HintText(LOCTEXT("MapCtxHint", "ContextId"))
					]
					+ SHorizontalBox::Slot().FillWidth(0.8f).Padding(0,0,4,0)
					[
						SAssignNew(SurfacesBox, SEditableTextBox).HintText(LOCTEXT("MapSurfacesHint", "SurfaceIds comma"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(OpacityBox, SSpinBox<float>).MinValue(0.0f).MaxValue(1.0f).Delta(0.05f).Value(1.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,4,0)
					[
						SAssignNew(EnabledBox, SCheckBox).IsChecked(ECheckBoxState::Checked)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("MapCreateBtn", "Create"))
						.OnClicked_Lambda([this, NameBox, ProjectBox, TypeBox, ContextBox, SurfacesBox, OpacityBox, EnabledBox]()
						{
						if (!GEngine) return FReply::Handled();
						if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
						{
							if (URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager())
							{
								FRshipContentMappingState State;
								State.Name = NameBox.IsValid() ? NameBox->GetText().ToString() : TEXT("");
								State.ProjectId = ProjectBox.IsValid() ? ProjectBox->GetText().ToString() : TEXT("");
								State.Type = TypeBox.IsValid() ? TypeBox->GetText().ToString() : TEXT("surface-uv");
								State.ContextId = ContextBox.IsValid() ? ContextBox->GetText().ToString() : TEXT("");
								State.Opacity = OpacityBox.IsValid() ? OpacityBox->GetValue() : 1.0f;
								State.bEnabled = !EnabledBox.IsValid() || EnabledBox->IsChecked();
								if (SurfacesBox.IsValid())
								{
									TArray<FString> Parts;
									SurfacesBox->GetText().ToString().ParseIntoArray(Parts, TEXT(","), true);
									State.SurfaceIds = Parts;
								}
								SelectedMappingId = Manager->CreateMapping(State);
								// show preview label update
								if (PreviewLabel.IsValid())
								{
									PreviewLabel->SetText(FText::FromString(FString::Printf(TEXT("Created mapping %s"), *State.Name)));
									PreviewLabel->SetColorAndOpacity(FLinearColor::White);
								}
								RefreshStatus();
							}
						}
						return FReply::Handled();
					})
					]
				];
			}

			for (const FRshipContentMappingState& Mapping : Mappings)
			{
				const FString Name = Mapping.Name.IsEmpty() ? Mapping.Id : Mapping.Name;
				const FString Status = Mapping.bEnabled ? TEXT("enabled") : TEXT("disabled");
				const FString ErrorSuffix = Mapping.LastError.IsEmpty() ? TEXT("") : FString::Printf(TEXT(" - %s"), *Mapping.LastError);
				const FString Line = FString::Printf(TEXT("%s [%s] (opacity: %.2f, %s)%s"), *Name, *Mapping.Type, Mapping.Opacity, *Status, *ErrorSuffix);

				TSharedPtr<SEditableTextBox> NameBox;
				TSharedPtr<SEditableTextBox> ProjectBox;
				TSharedPtr<SEditableTextBox> TypeBox;
				TSharedPtr<SEditableTextBox> ContextBox;
				TSharedPtr<SEditableTextBox> SurfacesBox;
				TSharedPtr<SSpinBox<float>> OpacityBox;
				TSharedPtr<SCheckBox> EnabledBox;
				TSharedPtr<SEditableTextBox> ProjTypeBox;
				TSharedPtr<SSpinBox<float>> PosXBox; TSharedPtr<SSpinBox<float>> PosYBox; TSharedPtr<SSpinBox<float>> PosZBox;
				TSharedPtr<SSpinBox<float>> RotXBox; TSharedPtr<SSpinBox<float>> RotYBox; TSharedPtr<SSpinBox<float>> RotZBox;
				TSharedPtr<SSpinBox<float>> FovBox; TSharedPtr<SSpinBox<float>> AspectBox; TSharedPtr<SSpinBox<float>> NearBox; TSharedPtr<SSpinBox<float>> FarBox;
				TSharedPtr<SEditableTextBox> CylAxisBox; TSharedPtr<SSpinBox<float>> CylRadiusBox; TSharedPtr<SSpinBox<float>> CylHeightBox; TSharedPtr<SSpinBox<float>> CylStartBox; TSharedPtr<SSpinBox<float>> CylEndBox;
				TSharedPtr<SSpinBox<float>> UScaleBox; TSharedPtr<SSpinBox<float>> VScaleBox; TSharedPtr<SSpinBox<float>> UOffBox; TSharedPtr<SSpinBox<float>> VOffBox; TSharedPtr<SSpinBox<float>> URotBox;

				MappingList->AddSlot()
				.AutoHeight()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0,0,4,0)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
								[
									SNew(SBorder)
									.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
									.BorderBackgroundColor(Mapping.Type == TEXT("surface-uv") ? FLinearColor(0.2f,0.6f,1.0f,1.0f) : FLinearColor(1.0f,0.6f,0.2f,1.0f))
									.Padding(FMargin(4,1))
									[
										SNew(STextBlock)
										.Text(Mapping.Type == TEXT("surface-uv") ? LOCTEXT("BadgeUV", "UV") : LOCTEXT("BadgeProj", "PROJ"))
										.ColorAndOpacity(FLinearColor::Black)
									]
								]
								+ SHorizontalBox::Slot().FillWidth(1.0f)
								[
									SAssignNew(NameBox, SEditableTextBox).Text(FText::FromString(Mapping.Name))
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
							[
								SAssignNew(ProjectBox, SEditableTextBox).Text(FText::FromString(Mapping.ProjectId))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
							[
								SAssignNew(TypeBox, SEditableTextBox).Text(FText::FromString(Mapping.Type))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
							[
								SAssignNew(ContextBox, SEditableTextBox).Text(FText::FromString(Mapping.ContextId))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
							[
								SAssignNew(SurfacesBox, SEditableTextBox).Text(FText::FromString(FString::Join(Mapping.SurfaceIds, TEXT(","))))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
							[
								SAssignNew(OpacityBox, SSpinBox<float>).MinValue(0.0f).MaxValue(1.0f).Delta(0.05f).Value(Mapping.Opacity)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
							[
								SAssignNew(EnabledBox, SCheckBox).IsChecked(Mapping.bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
								[
									SNew(STextBlock).Text(FText::FromString(Line)).ColorAndOpacity(Mapping.LastError.IsEmpty() ? FLinearColor::White : FLinearColor::Red)
								]
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,4,0)
						[
							SNew(SButton)
							.Text(LOCTEXT("MapSaveInline", "Save"))
							.OnClicked_Lambda([this, Mapping, NameBox, ProjectBox, TypeBox, ContextBox, SurfacesBox, OpacityBox, EnabledBox]()
							{
								if (!GEngine) return FReply::Handled();
								if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
								{
									if (URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager())
									{
										FRshipContentMappingState State = Mapping;
										State.Name = NameBox.IsValid() ? NameBox->GetText().ToString() : State.Name;
										State.ProjectId = ProjectBox.IsValid() ? ProjectBox->GetText().ToString() : State.ProjectId;
										State.Type = TypeBox.IsValid() ? TypeBox->GetText().ToString() : State.Type;
										State.ContextId = ContextBox.IsValid() ? ContextBox->GetText().ToString() : State.ContextId;
										State.Opacity = OpacityBox.IsValid() ? OpacityBox->GetValue() : State.Opacity;
										State.bEnabled = !EnabledBox.IsValid() || EnabledBox->IsChecked();
											if (SurfacesBox.IsValid())
											{
												TArray<FString> Parts;
												SurfacesBox->GetText().ToString().ParseIntoArray(Parts, TEXT(","), true);
												State.SurfaceIds = Parts;
											}
											Manager->UpdateMapping(State);
											if (PreviewLabel.IsValid())
											{
												PreviewLabel->SetText(FText::FromString(FString::Printf(TEXT("Saved %s"), *State.Name)));
												PreviewLabel->SetColorAndOpacity(FLinearColor::White);
											}
											RefreshStatus();
										}
									}
									return FReply::Handled();
								})
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton)
							.Text(LOCTEXT("MapDeleteInline", "Delete"))
							.OnClicked_Lambda([this, Mapping]()
							{
								if (!GEngine) return FReply::Handled();
								if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
								{
									if (URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager())
									{
										Manager->DeleteMapping(Mapping.Id);
										if (SelectedMappingId == Mapping.Id) { SelectedMappingId.Reset(); }
										if (PreviewLabel.IsValid())
										{
											PreviewLabel->SetText(FText::FromString(FString::Printf(TEXT("Deleted %s"), *Mapping.Name)));
											PreviewLabel->SetColorAndOpacity(FLinearColor::Gray);
										}
										RefreshStatus();
									}
								}
								return FReply::Handled();
							})
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4,0,0,0)
						[
							SNew(SButton)
							.Text(LOCTEXT("MapPreviewBtn", "Preview"))
							.OnClicked_Lambda([this, Mapping]()
							{
								if (!GEngine) return FReply::Handled();
								if (PreviewLabel.IsValid())
								{
									PreviewLabel->SetText(FText::FromString(FString::Printf(TEXT("Preview mapping %s (type %s)"), *Mapping.Name, *Mapping.Type)));
									PreviewLabel->SetColorAndOpacity(FLinearColor::White);
								}

								URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
								const URshipContentMappingManager* Manager = Subsystem ? Subsystem->GetContentMappingManager() : nullptr;
								UTexture* Tex = nullptr;
								if (Manager)
								{
									const TArray<FRshipRenderContextState> Contexts = Manager->GetRenderContexts();
									for (const FRshipRenderContextState& CtxState : Contexts)
									{
										if (CtxState.Id == Mapping.ContextId)
										{
											Tex = CtxState.ResolvedTexture;
											break;
										}
									}
								}
								LastPreviewMappingId = Mapping.Id;
								UpdatePreviewImage(Tex, Mapping);
								return FReply::Handled();
							})
						]
					]

					// Projection / UV detail row
					+ SVerticalBox::Slot().AutoHeight().Padding(0,4,0,8)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0,0,4,0)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
							[
								SAssignNew(ProjTypeBox, SEditableTextBox).Text(Mapping.Config.IsValid() && Mapping.Config->HasTypedField<EJson::String>(TEXT("projectionType"))
									? FText::FromString(Mapping.Config->GetStringField(TEXT("projectionType")))
									: FText::FromString(TEXT("perspective")))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(LOCTEXT("PosLabel", "Pos"))]
								+ SHorizontalBox::Slot().AutoWidth().Padding(2,0)[SAssignNew(PosXBox, SSpinBox<float>).MinValue(-100000).MaxValue(100000).Value(0.f)]
								+ SHorizontalBox::Slot().AutoWidth().Padding(2,0)[SAssignNew(PosYBox, SSpinBox<float>).MinValue(-100000).MaxValue(100000).Value(0.f)]
								+ SHorizontalBox::Slot().AutoWidth().Padding(2,0)[SAssignNew(PosZBox, SSpinBox<float>).MinValue(-100000).MaxValue(100000).Value(0.f)]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(LOCTEXT("RotLabel", "Rot"))]
								+ SHorizontalBox::Slot().AutoWidth().Padding(2,0)[SAssignNew(RotXBox, SSpinBox<float>).MinValue(-360).MaxValue(360).Value(0.f)]
								+ SHorizontalBox::Slot().AutoWidth().Padding(2,0)[SAssignNew(RotYBox, SSpinBox<float>).MinValue(-360).MaxValue(360).Value(0.f)]
								+ SHorizontalBox::Slot().AutoWidth().Padding(2,0)[SAssignNew(RotZBox, SSpinBox<float>).MinValue(-360).MaxValue(360).Value(0.f)]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(LOCTEXT("ProjParams", "Fov/Aspect/Near/Far"))]
								+ SHorizontalBox::Slot().AutoWidth().Padding(2,0)[SAssignNew(FovBox, SSpinBox<float>).MinValue(1).MaxValue(179).Value(60.f)]
								+ SHorizontalBox::Slot().AutoWidth().Padding(2,0)[SAssignNew(AspectBox, SSpinBox<float>).MinValue(0.1f).MaxValue(10.f).Delta(0.05f).Value(1.7778f)]
								+ SHorizontalBox::Slot().AutoWidth().Padding(2,0)[SAssignNew(NearBox, SSpinBox<float>).MinValue(0.01f).MaxValue(10000.f).Delta(1.f).Value(10.f)]
								+ SHorizontalBox::Slot().AutoWidth().Padding(2,0)[SAssignNew(FarBox, SSpinBox<float>).MinValue(1.f).MaxValue(200000.f).Delta(10.f).Value(10000.f)]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(LOCTEXT("CylLabel", "Cyl (axis radius height start end)"))]
								+ SHorizontalBox::Slot().FillWidth(0.6f).Padding(2,0)
								[
									SAssignNew(CylAxisBox, SEditableTextBox).Text(FText::FromString(TEXT("y")))
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(2,0)[SAssignNew(CylRadiusBox, SSpinBox<float>).MinValue(0.01f).MaxValue(100000.f).Delta(1.f).Value(100.f)]
								+ SHorizontalBox::Slot().AutoWidth().Padding(2,0)[SAssignNew(CylHeightBox, SSpinBox<float>).MinValue(0.01f).MaxValue(100000.f).Delta(1.f).Value(1000.f)]
								+ SHorizontalBox::Slot().AutoWidth().Padding(2,0)[SAssignNew(CylStartBox, SSpinBox<float>).MinValue(-360.f).MaxValue(360.f).Delta(1.f).Value(0.f)]
								+ SHorizontalBox::Slot().AutoWidth().Padding(2,0)[SAssignNew(CylEndBox, SSpinBox<float>).MinValue(-360.f).MaxValue(360.f).Delta(1.f).Value(90.f)]
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock).Text(LOCTEXT("UvLabel", "UV Transform"))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(LOCTEXT("ScaleLabel", "Scale U/V"))]
								+ SHorizontalBox::Slot().AutoWidth().Padding(2,0)[SAssignNew(UScaleBox, SSpinBox<float>).MinValue(0.01f).MaxValue(100.f).Delta(0.05f).Value(1.f)]
								+ SHorizontalBox::Slot().AutoWidth().Padding(2,0)[SAssignNew(VScaleBox, SSpinBox<float>).MinValue(0.01f).MaxValue(100.f).Delta(0.05f).Value(1.f)]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(LOCTEXT("OffsetLabel", "Offset U/V"))]
								+ SHorizontalBox::Slot().AutoWidth().Padding(2,0)[SAssignNew(UOffBox, SSpinBox<float>).MinValue(-10.f).MaxValue(10.f).Delta(0.01f).Value(0.f)]
								+ SHorizontalBox::Slot().AutoWidth().Padding(2,0)[SAssignNew(VOffBox, SSpinBox<float>).MinValue(-10.f).MaxValue(10.f).Delta(0.01f).Value(0.f)]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(LOCTEXT("RotLabel2", "Rotation"))]
								+ SHorizontalBox::Slot().AutoWidth().Padding(2,0)[SAssignNew(URotBox, SSpinBox<float>).MinValue(-360.f).MaxValue(360.f).Delta(1.f).Value(0.f)]
							]
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SButton)
							.Text(LOCTEXT("MapSaveConfig", "Save Config"))
							.OnClicked_Lambda([this, Mapping, ProjTypeBox, PosXBox, PosYBox, PosZBox, RotXBox, RotYBox, RotZBox, FovBox, AspectBox, NearBox, FarBox, CylAxisBox, CylRadiusBox, CylHeightBox, CylStartBox, CylEndBox, UScaleBox, VScaleBox, UOffBox, VOffBox, URotBox]()
							{
								if (!GEngine) return FReply::Handled();
								if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
								{
									if (URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager())
									{
										FRshipContentMappingState State = Mapping;
										TSharedPtr<FJsonObject> Config = MakeShared<FJsonObject>();
										if (State.Type == TEXT("surface-uv"))
										{
											TSharedPtr<FJsonObject> Uv = MakeShared<FJsonObject>();
											Uv->SetNumberField(TEXT("scaleU"), UScaleBox.IsValid() ? UScaleBox->GetValue() : 1.0);
											Uv->SetNumberField(TEXT("scaleV"), VScaleBox.IsValid() ? VScaleBox->GetValue() : 1.0);
											Uv->SetNumberField(TEXT("offsetU"), UOffBox.IsValid() ? UOffBox->GetValue() : 0.0);
											Uv->SetNumberField(TEXT("offsetV"), VOffBox.IsValid() ? VOffBox->GetValue() : 0.0);
											Uv->SetNumberField(TEXT("rotationDeg"), URotBox.IsValid() ? URotBox->GetValue() : 0.0);
											Config->SetObjectField(TEXT("uvTransform"), Uv);
										}
										else
										{
											Config->SetStringField(TEXT("projectionType"), ProjTypeBox.IsValid() ? ProjTypeBox->GetText().ToString() : TEXT("perspective"));
											TSharedPtr<FJsonObject> Pos = MakeShared<FJsonObject>();
											Pos->SetNumberField(TEXT("x"), PosXBox.IsValid() ? PosXBox->GetValue() : 0.0);
											Pos->SetNumberField(TEXT("y"), PosYBox.IsValid() ? PosYBox->GetValue() : 0.0);
											Pos->SetNumberField(TEXT("z"), PosZBox.IsValid() ? PosZBox->GetValue() : 0.0);
											Config->SetObjectField(TEXT("projectorPosition"), Pos);
											TSharedPtr<FJsonObject> Rot = MakeShared<FJsonObject>();
											Rot->SetNumberField(TEXT("x"), RotXBox.IsValid() ? RotXBox->GetValue() : 0.0);
											Rot->SetNumberField(TEXT("y"), RotYBox.IsValid() ? RotYBox->GetValue() : 0.0);
											Rot->SetNumberField(TEXT("z"), RotZBox.IsValid() ? RotZBox->GetValue() : 0.0);
											Config->SetObjectField(TEXT("projectorRotation"), Rot);
											Config->SetNumberField(TEXT("fov"), FovBox.IsValid() ? FovBox->GetValue() : 60.0);
											Config->SetNumberField(TEXT("aspectRatio"), AspectBox.IsValid() ? AspectBox->GetValue() : 1.7778);
											Config->SetNumberField(TEXT("near"), NearBox.IsValid() ? NearBox->GetValue() : 10.0);
											Config->SetNumberField(TEXT("far"), FarBox.IsValid() ? FarBox->GetValue() : 10000.0);
											if (CylAxisBox.IsValid() && !CylAxisBox->GetText().IsEmpty())
											{
												TSharedPtr<FJsonObject> Cyl = MakeShared<FJsonObject>();
												Cyl->SetStringField(TEXT("axis"), CylAxisBox->GetText().ToString());
												Cyl->SetNumberField(TEXT("radius"), CylRadiusBox.IsValid() ? CylRadiusBox->GetValue() : 100.0);
												Cyl->SetNumberField(TEXT("height"), CylHeightBox.IsValid() ? CylHeightBox->GetValue() : 1000.0);
												Cyl->SetNumberField(TEXT("startAngle"), CylStartBox.IsValid() ? CylStartBox->GetValue() : 0.0);
												Cyl->SetNumberField(TEXT("endAngle"), CylEndBox.IsValid() ? CylEndBox->GetValue() : 90.0);
												Config->SetObjectField(TEXT("cylindrical"), Cyl);
											}
										}
										State.Config = Config;
										Manager->UpdateMapping(State);
										if (PreviewLabel.IsValid())
										{
											PreviewLabel->SetText(FText::FromString(FString::Printf(TEXT("Saved config for %s"), *State.Name)));
											PreviewLabel->SetColorAndOpacity(FLinearColor::White);
										}
										RefreshStatus();
									}
								}
								return FReply::Handled();
							})
						]
					]
				];
			}
		}
	}

	// Live update preview/gizmo
	if (!LastPreviewMappingId.IsEmpty())
	{
		const FRshipContentMappingState* PreviewMapping = nullptr;
		for (const FRshipContentMappingState& Mapping : Mappings)
		{
			if (Mapping.Id == LastPreviewMappingId)
			{
				PreviewMapping = &Mapping;
				break;
			}
		}

		if (PreviewMapping)
		{
			UTexture* Tex = nullptr;
			for (const FRshipRenderContextState& Ctx : Contexts)
			{
				if (Ctx.Id == PreviewMapping->ContextId)
				{
					Tex = Ctx.ResolvedTexture;
					break;
				}
			}
			UpdatePreviewImage(Tex, *PreviewMapping);
		}
	}
}

#undef LOCTEXT_NAMESPACE
