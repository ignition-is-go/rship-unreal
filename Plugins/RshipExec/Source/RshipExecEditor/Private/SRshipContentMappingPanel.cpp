// Copyright Rocketship. All Rights Reserved.

#include "SRshipContentMappingPanel.h"
#include "SRshipModeSelector.h"
#include "SRshipMappingCanvas.h"
#include "SRshipAngleMaskWidget.h"
#include "SRshipContentModeSelector.h"
#include "RshipSubsystem.h"
#include "RshipContentMappingManager.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
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
#include "Components/MeshComponent.h"
#include "Engine/Selection.h"
#include "RshipContentMappingPreviewActor.h"
#include "Editor.h"
#include "RshipTargetComponent.h"
#include "RshipCameraManager.h"
#include "RshipSceneConverter.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "RshipCameraActor.h"
#include "HAL/PlatformApplicationMisc.h"

#define LOCTEXT_NAMESPACE "SRshipContentMappingPanel"

namespace
{
	const FString MapModeDirect = TEXT("direct");
	const FString MapModeFeed = TEXT("feed");
	const FString MapModePerspective = TEXT("perspective");
	const FString MapModeCustomMatrix = TEXT("custom-matrix");
	const FString MapModeCylindrical = TEXT("cylindrical");
	const FString MapModeSpherical = TEXT("spherical");
	const FString MapModeParallel = TEXT("parallel");
	const FString MapModeRadial = TEXT("radial");
	const FString MapModeMesh = TEXT("mesh");
	const FString MapModeFisheye = TEXT("fisheye");
	const FString MapModeCameraPlate = TEXT("camera-plate");
	const FString MapModeSpatial = TEXT("spatial");
	const FString MapModeDepthMap = TEXT("depth-map");

	FString NormalizeMapMode(const FString& InValue, const FString& DefaultValue)
	{
		if (InValue.Equals(TEXT("surface-feed"), ESearchCase::IgnoreCase)) return MapModeFeed;
		if (InValue.Equals(TEXT("surface-uv"), ESearchCase::IgnoreCase)) return MapModeDirect;
		if (InValue.Equals(TEXT("surface-projection"), ESearchCase::IgnoreCase)) return MapModePerspective;
		if (InValue.Equals(MapModeFeed, ESearchCase::IgnoreCase)) return MapModeFeed;
		if (InValue.Equals(MapModeDirect, ESearchCase::IgnoreCase)) return MapModeDirect;
		if (InValue.Equals(MapModePerspective, ESearchCase::IgnoreCase)) return MapModePerspective;
		if (InValue.Equals(MapModeCustomMatrix, ESearchCase::IgnoreCase) || InValue.Equals(TEXT("custom matrix"), ESearchCase::IgnoreCase) || InValue.Equals(TEXT("matrix"), ESearchCase::IgnoreCase)) return MapModeCustomMatrix;
		if (InValue.Equals(MapModeCylindrical, ESearchCase::IgnoreCase)) return MapModeCylindrical;
		if (InValue.Equals(MapModeSpherical, ESearchCase::IgnoreCase)) return MapModeSpherical;
		if (InValue.Equals(MapModeParallel, ESearchCase::IgnoreCase)) return MapModeParallel;
		if (InValue.Equals(MapModeRadial, ESearchCase::IgnoreCase)) return MapModeRadial;
		if (InValue.Equals(MapModeMesh, ESearchCase::IgnoreCase)) return MapModeMesh;
		if (InValue.Equals(MapModeFisheye, ESearchCase::IgnoreCase)) return MapModeFisheye;
		if (InValue.Equals(MapModeCameraPlate, ESearchCase::IgnoreCase) || InValue.Equals(TEXT("camera plate"), ESearchCase::IgnoreCase) || InValue.Equals(TEXT("cameraplate"), ESearchCase::IgnoreCase)) return MapModeCameraPlate;
		if (InValue.Equals(MapModeSpatial, ESearchCase::IgnoreCase)) return MapModeSpatial;
		if (InValue.Equals(MapModeDepthMap, ESearchCase::IgnoreCase) || InValue.Equals(TEXT("depth map"), ESearchCase::IgnoreCase) || InValue.Equals(TEXT("depthmap"), ESearchCase::IgnoreCase)) return MapModeDepthMap;
		return DefaultValue;
	}

	FString GetUvModeFromConfig(const TSharedPtr<FJsonObject>& Config)
	{
		if (!Config.IsValid())
		{
			return MapModeDirect;
		}
		if (Config->HasTypedField<EJson::String>(TEXT("uvMode")))
		{
			return NormalizeMapMode(Config->GetStringField(TEXT("uvMode")), MapModeDirect);
		}
		if (Config->HasTypedField<EJson::Object>(TEXT("feedRect")) || Config->HasTypedField<EJson::Array>(TEXT("feedRects")))
		{
			return MapModeFeed;
		}
		return MapModeDirect;
	}

	FString GetProjectionModeFromConfig(const TSharedPtr<FJsonObject>& Config)
	{
		if (!Config.IsValid())
		{
			return MapModePerspective;
		}
		if (Config->HasTypedField<EJson::String>(TEXT("projectionType")))
		{
			return NormalizeMapMode(Config->GetStringField(TEXT("projectionType")), MapModePerspective);
		}
		if (Config->HasTypedField<EJson::Object>(TEXT("customProjectionMatrix")) || Config->HasTypedField<EJson::Object>(TEXT("matrix")))
		{
			return MapModeCustomMatrix;
		}
		return MapModePerspective;
	}

	FString GetMappingModeFromState(const FRshipContentMappingState& Mapping)
	{
		if (Mapping.Type == TEXT("surface-uv"))
		{
			return GetUvModeFromConfig(Mapping.Config);
		}
	if (Mapping.Type == TEXT("surface-projection"))
	{
		return GetProjectionModeFromConfig(Mapping.Config);
	}
	return NormalizeMapMode(Mapping.Type, MapModeDirect);
}

	FText GetMappingDisplayLabel(const FRshipContentMappingState& Mapping)
	{
		const FString Mode = GetMappingModeFromState(Mapping);
		if (Mode == MapModeFeed) return LOCTEXT("MapModeFeedLabel", "Feed");
		if (Mode == MapModeDirect) return LOCTEXT("MapModeDirectLabel", "Direct");
		if (Mode == MapModeCustomMatrix) return LOCTEXT("MapModeCustomMatrixLabel", "Custom Matrix");
		if (Mode == MapModeCylindrical) return LOCTEXT("MapModeCylLabel", "Cylindrical");
		if (Mode == MapModeSpherical) return LOCTEXT("MapModeSphericalLabel", "Spherical");
		if (Mode == MapModeParallel) return LOCTEXT("MapModeParallelLabel", "Parallel");
		if (Mode == MapModeRadial) return LOCTEXT("MapModeRadialLabel", "Radial");
		if (Mode == MapModeMesh) return LOCTEXT("MapModeMeshLabel", "Mesh");
		if (Mode == MapModeFisheye) return LOCTEXT("MapModeFisheyeLabel", "Fisheye");
		if (Mode == MapModeCameraPlate) return LOCTEXT("MapModeCameraPlateLabel", "Camera Plate");
		if (Mode == MapModeSpatial) return LOCTEXT("MapModeSpatialLabel", "Spatial");
		if (Mode == MapModeDepthMap) return LOCTEXT("MapModeDepthMapLabel", "Depth Map");
		return LOCTEXT("MapModePerspectiveLabel", "Perspective");
	}

	bool IsProjectionMode(const FString& Mode);

	struct FMappingSurfaceSummaryInfo
	{
		FString DisplayName;
		FString MeshComponentName;
	};

	float GetMappingJsonNumber(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, float DefaultValue)
	{
		return (Obj.IsValid() && Obj->HasTypedField<EJson::Number>(Field)) ? static_cast<float>(Obj->GetNumberField(Field)) : DefaultValue;
	}

	FString BuildMappingCanvasSummary(const FRshipContentMappingState& Mapping)
	{
		const FString Mode = GetMappingModeFromState(Mapping);
		const TSharedPtr<FJsonObject> Config = Mapping.Config;

		if (Mapping.Type == TEXT("surface-uv"))
		{
			const FString UvMode = GetUvModeFromConfig(Config);
			FString Summary = UvMode.Equals(MapModeFeed, ESearchCase::IgnoreCase)
				? TEXT("Surface UV: Feed")
				: TEXT("Surface UV: Direct");

			if (Config.IsValid() && Config->HasTypedField<EJson::Object>(TEXT("uvTransform")))
			{
				const TSharedPtr<FJsonObject> Uv = Config->GetObjectField(TEXT("uvTransform"));
				const float ScaleU = GetMappingJsonNumber(Uv, TEXT("scaleU"), 1.0f);
				const float ScaleV = GetMappingJsonNumber(Uv, TEXT("scaleV"), 1.0f);
				const float OffsetU = GetMappingJsonNumber(Uv, TEXT("offsetU"), 0.0f);
				const float OffsetV = GetMappingJsonNumber(Uv, TEXT("offsetV"), 0.0f);
				const float Rot = GetMappingJsonNumber(Uv, TEXT("rotationDeg"), 0.0f);
				Summary += FString::Printf(TEXT(" (scale=%0.2f,%0.2f offset=%0.2f,%0.2f rot=%0.2f)"), ScaleU, ScaleV, OffsetU, OffsetV, Rot);
			}

			if (UvMode == MapModeFeed && Config.IsValid() && Config->HasTypedField<EJson::Object>(TEXT("feedRect")))
			{
				const TSharedPtr<FJsonObject> FeedRect = Config->GetObjectField(TEXT("feedRect"));
				const float U = GetMappingJsonNumber(FeedRect, TEXT("u"), 0.0f);
				const float V = GetMappingJsonNumber(FeedRect, TEXT("v"), 0.0f);
				const float Width = GetMappingJsonNumber(FeedRect, TEXT("width"), 1.0f);
				const float Height = GetMappingJsonNumber(FeedRect, TEXT("height"), 1.0f);
				Summary += FString::Printf(TEXT(" feedRect=%0.3f,%0.3f,%0.3f,%0.3f"), U, V, Width, Height);
			}
			return Summary;
		}

		if (IsProjectionMode(Mode))
		{
			FString Summary = GetMappingDisplayLabel(Mapping).ToString();
			if (Mode == MapModeCustomMatrix)
			{
				Summary += TEXT(" (custom matrix)");
			}

			if (Config.IsValid())
			{
				const float Fov = GetMappingJsonNumber(Config, TEXT("fov"), 60.0f);
				const float Aspect = GetMappingJsonNumber(Config, TEXT("aspectRatio"), 1.7778f);
				const float Near = GetMappingJsonNumber(Config, TEXT("near"), 10.0f);
				const float Far = GetMappingJsonNumber(Config, TEXT("far"), 10000.0f);
				Summary += FString::Printf(TEXT(" (fov=%0.1f aspect=%0.2f near=%0.2f far=%0.2f)"), Fov, Aspect, Near, Far);

				if (Config->HasTypedField<EJson::Object>(TEXT("cylindrical")))
				{
					const TSharedPtr<FJsonObject> Cyl = Config->GetObjectField(TEXT("cylindrical"));
					const FString Axis = Cyl->HasTypedField<EJson::String>(TEXT("axis")) ? Cyl->GetStringField(TEXT("axis")) : TEXT("y");
					const float Radius = GetMappingJsonNumber(Cyl, TEXT("radius"), 0.0f);
					const float Height = GetMappingJsonNumber(Cyl, TEXT("height"), 0.0f);
					const float StartAngle = GetMappingJsonNumber(Cyl, TEXT("startAngle"), 0.0f);
					const float EndAngle = GetMappingJsonNumber(Cyl, TEXT("endAngle"), 0.0f);
					Summary += FString::Printf(TEXT(" cyl(axis=%s radius=%0.1f height=%0.1f angle=%0.1f-%0.1f)"), *Axis, Radius, Height, StartAngle, EndAngle);
				}
			}
			return Summary;
		}

		if (Config.IsValid() && Config->HasTypedField<EJson::String>(TEXT("projectionType")))
		{
			return Config->GetStringField(TEXT("projectionType"));
		}

		return Mapping.Type.IsEmpty() ? TEXT("Mapping") : Mapping.Type;
	}

	FString BuildMappingScreenSummary(const FRshipContentMappingState& Mapping, const TMap<FString, FMappingSurfaceSummaryInfo>& SurfaceInfoById)
	{
		if (Mapping.SurfaceIds.Num() == 0)
		{
			return TEXT("No screens");
		}

		TSet<FString> SeenSurfaceIds;
		TArray<FString> MeshKeys;
		TMap<FString, TArray<FString>> ScreensByMesh;
		MeshKeys.Reserve(Mapping.SurfaceIds.Num());

		for (const FString& SurfaceId : Mapping.SurfaceIds)
		{
			if (SurfaceId.IsEmpty() || SeenSurfaceIds.Contains(SurfaceId))
			{
				continue;
			}

			SeenSurfaceIds.Add(SurfaceId);

			const FMappingSurfaceSummaryInfo* SurfaceInfo = SurfaceInfoById.Find(SurfaceId);
			const FString ScreenName = SurfaceInfo && !SurfaceInfo->DisplayName.IsEmpty() ? SurfaceInfo->DisplayName : SurfaceId;
			const FString MeshName = (SurfaceInfo && !SurfaceInfo->MeshComponentName.IsEmpty()) ? SurfaceInfo->MeshComponentName : TEXT("Unassigned mesh");

			TArray<FString>* ScreensForMesh = ScreensByMesh.Find(MeshName);
			if (ScreensForMesh == nullptr)
			{
				TArray<FString> InitialScreens;
				InitialScreens.Add(ScreenName);
				MeshKeys.Add(MeshName);
				ScreensByMesh.Add(MeshName, InitialScreens);
			}
			else
			{
				ScreensForMesh->Add(ScreenName);
			}
		}

		if (ScreensByMesh.Num() == 0)
		{
			return TEXT("No screens");
		}

		MeshKeys.Sort();
		TArray<FString> MeshEntries;
		MeshEntries.Reserve(MeshKeys.Num());

		for (const FString& MeshName : MeshKeys)
		{
			if (TArray<FString>* ScreensForMesh = ScreensByMesh.Find(MeshName))
			{
				ScreensForMesh->Sort();
				const FString Screens = FString::Join(*ScreensForMesh, TEXT(", "));
				MeshEntries.Add(FString::Printf(TEXT("%s: %s"), *MeshName, *Screens));
			}
		}

		return FString::Printf(TEXT("%s"), *FString::Join(MeshEntries, TEXT(" | ")));
	}

	bool IsProjectionMode(const FString& Mode)
	{
		return Mode == MapModePerspective || Mode == MapModeCylindrical || Mode == MapModeSpherical
			|| Mode == MapModeCustomMatrix || Mode == MapModeParallel || Mode == MapModeRadial || Mode == MapModeMesh || Mode == MapModeFisheye
			|| Mode == MapModeCameraPlate || Mode == MapModeSpatial || Mode == MapModeDepthMap;
	}

	constexpr int32 CustomProjectionMatrixElementCount = 16;

	const TCHAR* GetCustomProjectionMatrixFieldName(int32 Index)
	{
		static const TCHAR* Names[CustomProjectionMatrixElementCount] = {
			TEXT("m00"), TEXT("m01"), TEXT("m02"), TEXT("m03"),
			TEXT("m10"), TEXT("m11"), TEXT("m12"), TEXT("m13"),
			TEXT("m20"), TEXT("m21"), TEXT("m22"), TEXT("m23"),
			TEXT("m30"), TEXT("m31"), TEXT("m32"), TEXT("m33")
		};
		return (Index >= 0 && Index < CustomProjectionMatrixElementCount) ? Names[Index] : TEXT("m00");
	}

	float GetDefaultCustomProjectionMatrixValue(int32 Index)
	{
		const int32 Row = Index / 4;
		const int32 Col = Index % 4;
		return (Row == Col) ? 1.0f : 0.0f;
	}

	TSharedPtr<FJsonObject> GetCustomProjectionMatrixObject(const TSharedPtr<FJsonObject>& Config)
	{
		if (!Config.IsValid())
		{
			return nullptr;
		}
		if (Config->HasTypedField<EJson::Object>(TEXT("customProjectionMatrix")))
		{
			return Config->GetObjectField(TEXT("customProjectionMatrix"));
		}
		if (Config->HasTypedField<EJson::Object>(TEXT("matrix")))
		{
			return Config->GetObjectField(TEXT("matrix"));
		}
		return nullptr;
	}

	void SetCustomProjectionMatrixInputsToIdentity(const TArray<TSharedPtr<SSpinBox<float>>>& Inputs)
	{
		for (int32 Index = 0; Index < CustomProjectionMatrixElementCount && Index < Inputs.Num(); ++Index)
		{
			if (Inputs[Index].IsValid())
			{
				Inputs[Index]->SetValue(GetDefaultCustomProjectionMatrixValue(Index));
			}
		}
	}

	void PopulateCustomProjectionMatrixInputs(const TArray<TSharedPtr<SSpinBox<float>>>& Inputs, const TSharedPtr<FJsonObject>& Config)
	{
		const TSharedPtr<FJsonObject> MatrixObj = GetCustomProjectionMatrixObject(Config);
		for (int32 Index = 0; Index < CustomProjectionMatrixElementCount && Index < Inputs.Num(); ++Index)
		{
			if (!Inputs[Index].IsValid())
			{
				continue;
			}

			float Value = GetDefaultCustomProjectionMatrixValue(Index);
			if (MatrixObj.IsValid())
			{
				const FString FieldName = GetCustomProjectionMatrixFieldName(Index);
				if (MatrixObj->HasTypedField<EJson::Number>(FieldName))
				{
					Value = static_cast<float>(MatrixObj->GetNumberField(FieldName));
				}
			}
			Inputs[Index]->SetValue(Value);
		}
	}

	TSharedPtr<FJsonObject> BuildCustomProjectionMatrixObject(const TArray<TSharedPtr<SSpinBox<float>>>& Inputs)
	{
		TSharedPtr<FJsonObject> MatrixObj = MakeShared<FJsonObject>();
		for (int32 Index = 0; Index < CustomProjectionMatrixElementCount; ++Index)
		{
			const FString FieldName = GetCustomProjectionMatrixFieldName(Index);
			const float Value = (Index < Inputs.Num() && Inputs[Index].IsValid())
				? Inputs[Index]->GetValue()
				: GetDefaultCustomProjectionMatrixValue(Index);
			MatrixObj->SetNumberField(FieldName, Value);
		}
		return MatrixObj;
	}

	TSharedPtr<FJsonObject> BuildDefaultCustomProjectionMatrixObject()
	{
		TSharedPtr<FJsonObject> MatrixObj = MakeShared<FJsonObject>();
		for (int32 Index = 0; Index < CustomProjectionMatrixElementCount; ++Index)
		{
			const FString FieldName = GetCustomProjectionMatrixFieldName(Index);
			MatrixObj->SetNumberField(FieldName, GetDefaultCustomProjectionMatrixValue(Index));
		}
		return MatrixObj;
	}

	struct FQuickCreateDefaults
	{
		bool bHasValue = false;
		bool bQuickAdvanced = false;
		FString SourceType = TEXT("camera");
		FString MapMode = TEXT("direct");
		FString ProjectId;
		FString SourceId;
		FString TargetId;
		int32 Width = 1920;
		int32 Height = 1080;
		FString CaptureMode = TEXT("FinalColorLDR");
		int32 UvChannel = 0;
		FString MaterialSlots;
		FString MeshName;
		float Opacity = 1.0f;
		float FeedU = 0.0f;
		float FeedV = 0.0f;
		float FeedW = 1.0f;
		float FeedH = 1.0f;
	};

	FQuickCreateDefaults GQuickCreateDefaults;

	FText MakeBulkScopeLabel(int32 SelectedCount, int32 VisibleCount)
	{
		if (SelectedCount > 0)
		{
			return FText::Format(LOCTEXT("BulkScopeSelectedFmt", "Scope: Selected ({0})"), FText::AsNumber(SelectedCount));
		}
		return FText::Format(LOCTEXT("BulkScopeVisibleFmt", "Scope: Visible ({0})"), FText::AsNumber(VisibleCount));
	}
}

void SRshipContentMappingPanel::Construct(const FArguments& InArgs)
{
	MapCustomMatrixInputs.SetNum(CustomProjectionMatrixElementCount);

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
					SNew(SVerticalBox)

					// Mapping Canvas
					+ SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
					[
						SAssignNew(MappingCanvas, SRshipMappingCanvas)
						.DesiredHeight(220.0f)
						.OnFeedRectChanged_Lambda([this](float U, float V, float W, float H)
						{
							if (MapFeedUInput.IsValid()) MapFeedUInput->SetValue(U);
							if (MapFeedVInput.IsValid()) MapFeedVInput->SetValue(V);
							if (MapFeedWInput.IsValid()) MapFeedWInput->SetValue(W);
							if (MapFeedHInput.IsValid()) MapFeedHInput->SetValue(H);
						})
						.OnUvTransformChanged_Lambda([this](float ScaleU, float ScaleV, float OffsetU, float OffsetV, float RotDeg)
						{
							if (MapUvScaleUInput.IsValid()) MapUvScaleUInput->SetValue(ScaleU);
							if (MapUvScaleVInput.IsValid()) MapUvScaleVInput->SetValue(ScaleV);
							if (MapUvOffsetUInput.IsValid()) MapUvOffsetUInput->SetValue(OffsetU);
							if (MapUvOffsetVInput.IsValid()) MapUvOffsetVInput->SetValue(OffsetV);
							if (MapUvRotInput.IsValid()) MapUvRotInput->SetValue(RotDeg);
						})
					]

					// Original preview row
					+ SVerticalBox::Slot().AutoHeight()
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
		]
	];

	ResetForms();
	ApplyStoredQuickCreateDefaults();
	RefreshStatus();
}

SRshipContentMappingPanel::~SRshipContentMappingPanel()
{
	StopProjectionEdit();
}

UWorld* SRshipContentMappingPanel::GetEditorWorld() const
{
#if WITH_EDITOR
	if (GEditor)
	{
		if (UWorld* EditorWorld = GEditor->GetEditorWorldContext().World())
		{
			return EditorWorld;
		}
	}
#endif
	if (!GEngine)
	{
		return nullptr;
	}

	const TIndirectArray<FWorldContext>& Contexts = GEngine->GetWorldContexts();
	for (const FWorldContext& Context : Contexts)
	{
		if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Editor)
		{
			if (UWorld* World = Context.World())
			{
				return World;
			}
		}
	}

	for (const FWorldContext& Context : Contexts)
	{
		if (UWorld* World = Context.World())
		{
			return World;
		}
	}

	return nullptr;
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

	// Soft match if user typed a partial label (only accept if unambiguous)
	TArray<TSharedPtr<FRshipIdOption>> PartialMatches;
	for (const TSharedPtr<FRshipIdOption>& Option : TargetOptions)
	{
		if (!Option.IsValid())
		{
			continue;
		}

		if (Option->Id.Contains(Trimmed, ESearchCase::IgnoreCase)
			|| Option->Label.Contains(Trimmed, ESearchCase::IgnoreCase))
		{
			PartialMatches.Add(Option);
			continue;
		}

		if (Option->Actor.IsValid())
		{
			const FString ActorLabel = Option->Actor->GetActorLabel();
			if (!ActorLabel.IsEmpty() && ActorLabel.Contains(Trimmed, ESearchCase::IgnoreCase))
			{
				PartialMatches.Add(Option);
			}
		}
	}
	if (PartialMatches.Num() == 1)
	{
		const TSharedPtr<FRshipIdOption>& Option = PartialMatches[0];
		return Option->ResolvedId.IsEmpty() ? Option->Id : Option->ResolvedId;
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

FString SRshipContentMappingPanel::ResolveScreenIdInput(const FString& InText) const
{
	const FString Trimmed = InText.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return Trimmed;
	}

	// Prefer explicit matches from current screen options first.
	for (const TSharedPtr<FRshipIdOption>& Option : SurfaceOptions)
	{
		if (!Option.IsValid())
		{
			continue;
		}

		if (Option->Id.Equals(Trimmed, ESearchCase::IgnoreCase))
		{
			return Option->Id;
		}

		if (Option->Label.Equals(Trimmed, ESearchCase::IgnoreCase))
		{
			return Option->Id;
		}

		if (Option->ResolvedId.Equals(Trimmed, ESearchCase::IgnoreCase))
		{
			return Option->Id;
		}
	}

	// Soft match if user typed a partial label (only accept if unambiguous).
	TArray<TSharedPtr<FRshipIdOption>> PartialMatches;
	for (const TSharedPtr<FRshipIdOption>& Option : SurfaceOptions)
	{
		if (!Option.IsValid())
		{
			continue;
		}

		if (Option->Id.Contains(Trimmed, ESearchCase::IgnoreCase)
			|| Option->Label.Contains(Trimmed, ESearchCase::IgnoreCase)
			|| Option->ResolvedId.Contains(Trimmed, ESearchCase::IgnoreCase))
		{
			PartialMatches.Add(Option);
		}
	}

	if (PartialMatches.Num() == 1)
	{
		const TSharedPtr<FRshipIdOption>& Option = PartialMatches[0];
		return Option.IsValid() ? Option->Id : TEXT("");
	}

	// Fallback for backward compatibility.
	return ResolveTargetIdInput(Trimmed);
}

FString SRshipContentMappingPanel::ResolveTargetIdForActor(AActor* Actor) const
{
	if (!Actor)
	{
		return TEXT("");
	}

	for (const TSharedPtr<FRshipIdOption>& Option : TargetOptions)
	{
		if (Option.IsValid() && Option->Actor.Get() == Actor)
		{
			return Option->Id.IsEmpty() ? Option->ResolvedId : Option->Id;
		}
	}

	if (URshipTargetComponent* TargetComp = Actor->FindComponentByClass<URshipTargetComponent>())
	{
		URshipSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URshipSubsystem>() : nullptr;
		if (Subsystem && Subsystem->TargetComponents)
		{
			for (auto& Pair : *Subsystem->TargetComponents)
			{
				if (Pair.Value == TargetComp)
				{
					return Pair.Key;
				}
			}
		}

		if (!TargetComp->targetName.IsEmpty())
		{
			return TargetComp->targetName;
		}
	}

	return TEXT("");
}

FString SRshipContentMappingPanel::ResolveCameraIdForActor(AActor* Actor) const
{
	if (!Actor)
	{
		return TEXT("");
	}

	if (ARshipCameraActor* RshipCamera = Cast<ARshipCameraActor>(Actor))
	{
		return RshipCamera->CameraId;
	}

	for (const TSharedPtr<FRshipIdOption>& Option : CameraOptions)
	{
		if (!Option.IsValid())
		{
			continue;
		}

		if (Option->Actor.Get() == Actor)
		{
			if (Option->bRequiresConversion)
			{
				return ConvertSceneCamera(Actor);
			}
			return Option->ResolvedId.IsEmpty() ? Option->Id : Option->ResolvedId;
		}
	}

	if (Actor->FindComponentByClass<UCameraComponent>())
	{
		return ConvertSceneCamera(Actor);
	}

	return TEXT("");
}

bool SRshipContentMappingPanel::TryApplySelectionToTarget(TSharedPtr<SEditableTextBox> TargetInput, bool bAppend)
{
#if WITH_EDITOR
	if (!TargetInput.IsValid() || !GEditor)
	{
		return false;
	}

	USelection* Selection = GEditor->GetSelectedActors();
	if (!Selection)
	{
		return false;
	}

	TArray<FString> ResolvedIds;
	for (FSelectionIterator It(*Selection); It; ++It)
	{
		AActor* Actor = Cast<AActor>(*It);
		if (!Actor)
		{
			continue;
		}

		const FString ResolvedId = ResolveTargetIdForActor(Actor);
		if (!ResolvedId.IsEmpty())
		{
			ResolvedIds.AddUnique(ResolvedId);
		}
	}

	if (ResolvedIds.Num() == 0)
	{
		return false;
	}

	if (!bAppend)
	{
		TargetInput->SetText(FText::FromString(ResolvedIds[0]));
		return true;
	}

	FString Current = TargetInput->GetText().ToString();
	TArray<FString> Parts;
	Current.ParseIntoArray(Parts, TEXT(","), true);
	for (FString& Part : Parts)
	{
		Part = Part.TrimStartAndEnd();
	}
	for (const FString& ResolvedId : ResolvedIds)
	{
		if (!Parts.Contains(ResolvedId))
		{
			Parts.Add(ResolvedId);
		}
	}
	TargetInput->SetText(FText::FromString(FString::Join(Parts, TEXT(","))));
	return true;
#else
	return false;
#endif
}

bool SRshipContentMappingPanel::TryApplySelectionToCamera(TSharedPtr<SEditableTextBox> CameraInput)
{
#if WITH_EDITOR
	if (!CameraInput.IsValid() || !GEditor)
	{
		return false;
	}

	USelection* Selection = GEditor->GetSelectedActors();
	if (!Selection)
	{
		return false;
	}

	for (FSelectionIterator It(*Selection); It; ++It)
	{
		AActor* Actor = Cast<AActor>(*It);
		if (!Actor)
		{
			continue;
		}

		const FString CameraId = ResolveCameraIdForActor(Actor);
		if (!CameraId.IsEmpty())
		{
			CameraInput->SetText(FText::FromString(CameraId));
			return true;
		}
	}
#endif
	return false;
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

FRshipContentMappingState* SRshipContentMappingPanel::FindMappingById(const FString& MappingId, TArray<FRshipContentMappingState>& Mappings) const
{
	for (FRshipContentMappingState& Mapping : Mappings)
	{
		if (Mapping.Id == MappingId)
		{
			return &Mapping;
		}
	}
	return nullptr;
}

FRshipRenderContextState* SRshipContentMappingPanel::FindContextById(const FString& ContextId, TArray<FRshipRenderContextState>& Contexts) const
{
	for (FRshipRenderContextState& Context : Contexts)
	{
		if (Context.Id == ContextId)
		{
			return &Context;
		}
	}
	return nullptr;
}

bool SRshipContentMappingPanel::IsProjectionEditActiveFor(const FString& MappingId) const
{
	return !ActiveProjectionMappingId.IsEmpty() && ActiveProjectionMappingId == MappingId;
}

bool SRshipContentMappingPanel::IsProjectionPrecisionControlsVisible() const
{
	return IsProjectionMode(MapMode) && bShowProjectionPrecisionControls;
}

bool SRshipContentMappingPanel::IsProjectionPrecisionControlsCollapsed() const
{
	return IsProjectionMode(MapMode) && !bShowProjectionPrecisionControls;
}

EVisibility SRshipContentMappingPanel::GetProjectionPrecisionControlsVisibility() const
{
	return IsProjectionPrecisionControlsVisible() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SRshipContentMappingPanel::GetProjectionPrecisionControlsCollapsedVisibility() const
{
	return IsProjectionPrecisionControlsCollapsed() ? EVisibility::Visible : EVisibility::Collapsed;
}

bool SRshipContentMappingPanel::IsMappingConfigExpanded(const FString& MappingId) const
{
	return ExpandedMappingConfigRows.Contains(MappingId);
}

bool SRshipContentMappingPanel::IsInlineProjectionPrecisionExpanded(const FString& MappingId) const
{
	return ExpandedProjectionPrecisionRows.Contains(MappingId);
}

void SRshipContentMappingPanel::SetMappingConfigExpanded(const FString& MappingId, bool bExpanded)
{
	if (bExpanded)
	{
		ExpandedMappingConfigRows.Add(MappingId);
	}
	else
	{
		ExpandedMappingConfigRows.Remove(MappingId);
	}
}

void SRshipContentMappingPanel::SetInlineProjectionConfigExpanded(const FString& MappingId, bool bExpanded)
{
	if (bExpanded)
	{
		ExpandedProjectionPrecisionRows.Add(MappingId);
		ExpandedMappingConfigRows.Add(MappingId);
	}
	else
	{
		ExpandedProjectionPrecisionRows.Remove(MappingId);
		ExpandedMappingConfigRows.Remove(MappingId);
	}
}

void SRshipContentMappingPanel::ToggleMappingConfigExpanded(const FString& MappingId, bool bInlineProjection)
{
	if (bInlineProjection)
	{
		const bool bExpanded = !IsMappingConfigExpanded(MappingId);
		SetInlineProjectionConfigExpanded(MappingId, bExpanded);
		return;
	}

	SetMappingConfigExpanded(MappingId, !IsMappingConfigExpanded(MappingId));
}

bool SRshipContentMappingPanel::IsProjectionPrecisionControlsVisibleForInlineMapping(const FString& MappingId, bool bInlineProjection) const
{
	return bInlineProjection
		&& IsInlineProjectionPrecisionExpanded(MappingId)
		&& IsMappingConfigExpanded(MappingId);
}

bool SRshipContentMappingPanel::IsProjectionPrecisionControlsNoticeVisibleForInlineMapping(const FString& MappingId, bool bInlineProjection) const
{
	return bInlineProjection
		&& IsMappingConfigExpanded(MappingId)
		&& !IsInlineProjectionPrecisionExpanded(MappingId);
}

void SRshipContentMappingPanel::StartProjectionEdit(const FRshipContentMappingState& Mapping)
{
	const FString Mode = GetMappingModeFromState(Mapping);
	if (!IsProjectionMode(Mode))
	{
		return;
	}

	if (!GEngine)
	{
		return;
	}

	URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	URshipContentMappingManager* Manager = Subsystem ? Subsystem->GetContentMappingManager() : nullptr;
	if (!Manager)
	{
		return;
	}

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return;
	}

	if (!bCoveragePreviewEnabled)
	{
		bCoveragePreviewEnabled = true;
		Manager->SetCoveragePreviewEnabled(true);
		if (PreviewLabel.IsValid())
		{
			PreviewLabel->SetText(LOCTEXT("CoveragePreviewAuto", "Coverage preview enabled: red = unmapped pixels, live image = mapped."));
			PreviewLabel->SetColorAndOpacity(FLinearColor::White);
		}
	}

	ARshipContentMappingPreviewActor* Actor = ProjectionActor.Get();
	if (!Actor)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = FName(*FString::Printf(TEXT("RshipContentMappingProjector_%s"), *Mapping.Id));
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.ObjectFlags |= RF_Transient;
		Actor = World->SpawnActor<ARshipContentMappingPreviewActor>(SpawnParams);
		if (Actor)
		{
			Actor->SetActorHiddenInGame(true);
			Actor->SetIsTemporarilyHiddenInEditor(false);
			Actor->SetActorEnableCollision(false);
			ProjectionActor = Actor;
		}
	}

	if (!Actor)
	{
		ActiveProjectionMappingId.Reset();
		return;
	}

	bShowProjectionPrecisionControls = false;
	ActiveProjectionMappingId = Mapping.Id;

	TArray<FRshipRenderContextState> Contexts = Manager->GetRenderContexts();
	FRshipRenderContextState* ContextState = FindContextById(Mapping.ContextId, Contexts);
	const bool bHasProjectorConfig = Mapping.Config.IsValid() && Mapping.Config->HasTypedField<EJson::Object>(TEXT("projectorPosition"));
	const bool bHasCameraContext = ContextState && ContextState->CameraActor.IsValid();
	if (!bHasProjectorConfig && !bHasCameraContext)
	{
		FVector FallbackPos = FVector::ZeroVector;
		FRotator FallbackRot = FRotator::ZeroRotator;
		bool bFoundFallback = false;

		TArray<FRshipMappingSurfaceState> Surfaces = Manager->GetMappingSurfaces();
		for (const FString& SurfaceId : Mapping.SurfaceIds)
		{
			for (const FRshipMappingSurfaceState& Surface : Surfaces)
			{
				if (Surface.Id != SurfaceId)
				{
					continue;
				}
				if (UMeshComponent* Mesh = Surface.MeshComponent.Get())
				{
					const FBoxSphereBounds Bounds = Mesh->Bounds;
					const FVector Forward = Mesh->GetOwner() ? Mesh->GetOwner()->GetActorForwardVector() : FVector::ForwardVector;
					FallbackPos = Bounds.Origin + Forward * Bounds.SphereRadius * 1.5f;
					FallbackRot = Forward.Rotation();
					bFoundFallback = true;
					break;
				}
			}
			if (bFoundFallback)
			{
				break;
			}
		}

		if (bFoundFallback)
		{
			Actor->SetActorLocation(FallbackPos);
			Actor->SetActorRotation(FallbackRot);
			Actor->ProjectorPosition = FallbackPos;
			Actor->ProjectorRotation = FallbackRot;
			Actor->LineColor = FColor::Cyan;
			LastProjectorTransform = Actor->GetActorTransform();
		}
		else
		{
			SyncProjectionActorFromMapping(Mapping, ContextState);
		}
	}
	else
	{
		SyncProjectionActorFromMapping(Mapping, ContextState);
	}

#if WITH_EDITOR
	if (GEditor)
	{
		GEditor->SelectNone(false, true, false);
		GEditor->SelectActor(Actor, true, true, true);
		GEditor->NoteSelectionChange();
	}
#endif
}

void SRshipContentMappingPanel::StopProjectionEdit()
{
	ActiveProjectionMappingId.Reset();
	LastProjectorTransform = FTransform::Identity;
	ProjectorUpdateAccumulator = 0.0f;
	bShowProjectionPrecisionControls = false;

	if (ARshipContentMappingPreviewActor* Actor = ProjectionActor.Get())
	{
		Actor->Destroy();
	}
	ProjectionActor.Reset();
}

void SRshipContentMappingPanel::SetSelectedMappingId(const FString& NewSelectedMappingId)
{
	const bool bSelectionChanged = !NewSelectedMappingId.Equals(SelectedMappingId);
	if (bSelectionChanged && !ActiveProjectionMappingId.IsEmpty())
	{
		StopProjectionEdit();
	}
	if (bSelectionChanged)
	{
		bShowProjectionPrecisionControls = false;
	}
	SelectedMappingId = NewSelectedMappingId;
}

void SRshipContentMappingPanel::ClearSelectedMappingId()
{
	if (!ActiveProjectionMappingId.IsEmpty())
	{
		StopProjectionEdit();
	}
	SelectedMappingId.Reset();
}

void SRshipContentMappingPanel::SyncProjectionActorFromMapping(const FRshipContentMappingState& Mapping, const FRshipRenderContextState* ContextState)
{
	ARshipContentMappingPreviewActor* Actor = ProjectionActor.Get();
	if (!Actor)
	{
		StopProjectionEdit();
		return;
	}

	FVector Position = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	float Fov = 60.0f;
	float Aspect = 1.7778f;
	float NearClip = 10.0f;
	float FarClip = 10000.0f;

	if (Mapping.Config.IsValid())
	{
		if (Mapping.Config->HasTypedField<EJson::Object>(TEXT("projectorPosition")))
		{
			const TSharedPtr<FJsonObject> PosObj = Mapping.Config->GetObjectField(TEXT("projectorPosition"));
			Position.X = PosObj->GetNumberField(TEXT("x"));
			Position.Y = PosObj->GetNumberField(TEXT("y"));
			Position.Z = PosObj->GetNumberField(TEXT("z"));
		}
		if (Mapping.Config->HasTypedField<EJson::Object>(TEXT("projectorRotation")))
		{
			const TSharedPtr<FJsonObject> RotObj = Mapping.Config->GetObjectField(TEXT("projectorRotation"));
			Rotation = FRotator::MakeFromEuler(FVector(
				RotObj->GetNumberField(TEXT("x")),
				RotObj->GetNumberField(TEXT("y")),
				RotObj->GetNumberField(TEXT("z"))));
		}
		Fov = Mapping.Config->HasField(TEXT("fov")) ? Mapping.Config->GetNumberField(TEXT("fov")) : Fov;
		Aspect = Mapping.Config->HasField(TEXT("aspectRatio")) ? Mapping.Config->GetNumberField(TEXT("aspectRatio")) : Aspect;
		NearClip = Mapping.Config->HasField(TEXT("near")) ? Mapping.Config->GetNumberField(TEXT("near")) : NearClip;
		FarClip = Mapping.Config->HasField(TEXT("far")) ? Mapping.Config->GetNumberField(TEXT("far")) : FarClip;
	}
	else if (ContextState && ContextState->CameraActor.IsValid())
	{
		const ARshipCameraActor* CameraActor = ContextState->CameraActor.Get();
		Position = CameraActor->GetActorLocation();
		Rotation = CameraActor->GetActorRotation();
	}

	Actor->SetActorLocation(Position);
	Actor->SetActorRotation(Rotation);
	Actor->ProjectorPosition = Position;
	Actor->ProjectorRotation = Rotation;
	Actor->FOV = Fov;
	Actor->Aspect = Aspect;
	Actor->NearClip = NearClip;
	Actor->FarClip = FarClip;
	Actor->LineColor = FColor::Cyan;
	LastProjectorTransform = Actor->GetActorTransform();
}

void SRshipContentMappingPanel::UpdateProjectionFromActor(float DeltaTime)
{
	if (ActiveProjectionMappingId.IsEmpty())
	{
		return;
	}

	ARshipContentMappingPreviewActor* Actor = ProjectionActor.Get();
	if (!Actor)
	{
		StopProjectionEdit();
		return;
	}

	const FTransform CurrentTransform = Actor->GetActorTransform();
	const bool bTransformChanged = !CurrentTransform.Equals(LastProjectorTransform, 0.1f);
	if (!bTransformChanged)
	{
		ProjectorUpdateAccumulator = 0.0f;
		return;
	}

	ProjectorUpdateAccumulator += DeltaTime;
	if (ProjectorUpdateAccumulator < 0.08f)
	{
		return;
	}

	ProjectorUpdateAccumulator = 0.0f;
	LastProjectorTransform = CurrentTransform;

	if (!GEngine)
	{
		return;
	}

	URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	URshipContentMappingManager* Manager = Subsystem ? Subsystem->GetContentMappingManager() : nullptr;
	if (!Manager)
	{
		return;
	}

	TArray<FRshipContentMappingState> Mappings = Manager->GetMappings();
	FRshipContentMappingState* Mapping = FindMappingById(ActiveProjectionMappingId, Mappings);
	if (!Mapping)
	{
		StopProjectionEdit();
		return;
	}

	if (!IsProjectionMode(GetMappingModeFromState(*Mapping)))
	{
		StopProjectionEdit();
		return;
	}

	TSharedPtr<FJsonObject> Config = Mapping->Config.IsValid() ? Mapping->Config : MakeShared<FJsonObject>();
	FString ProjectionType = TEXT("perspective");
	if (Config->HasTypedField<EJson::String>(TEXT("projectionType")))
	{
		ProjectionType = Config->GetStringField(TEXT("projectionType"));
	}
	Config->SetStringField(TEXT("projectionType"), ProjectionType);

	const FVector Pos = CurrentTransform.GetLocation();
	const FRotator Rot = CurrentTransform.Rotator();

	TSharedPtr<FJsonObject> PosObj = MakeShared<FJsonObject>();
	PosObj->SetNumberField(TEXT("x"), Pos.X);
	PosObj->SetNumberField(TEXT("y"), Pos.Y);
	PosObj->SetNumberField(TEXT("z"), Pos.Z);
	Config->SetObjectField(TEXT("projectorPosition"), PosObj);

	const FVector Euler = Rot.Euler();
	TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
	RotObj->SetNumberField(TEXT("x"), Euler.X);
	RotObj->SetNumberField(TEXT("y"), Euler.Y);
	RotObj->SetNumberField(TEXT("z"), Euler.Z);
	Config->SetObjectField(TEXT("projectorRotation"), RotObj);

	Config->SetNumberField(TEXT("fov"), Actor->FOV);
	Config->SetNumberField(TEXT("aspectRatio"), Actor->Aspect);
	Config->SetNumberField(TEXT("near"), Actor->NearClip);
	Config->SetNumberField(TEXT("far"), Actor->FarClip);

	Mapping->Config = Config;
	Manager->UpdateMapping(*Mapping);
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
		if (MappingCanvas.IsValid())
		{
			MappingCanvas->SetBackgroundTexture(nullptr);
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

	// Forward texture to mapping canvas
	if (MappingCanvas.IsValid())
	{
		MappingCanvas->SetBackgroundTexture(Texture);
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
		TSet<const AActor*> AddedCameraActors;
		for (const TSharedPtr<FRshipIdOption>& Existing : CameraOptions)
		{
			if (Existing.IsValid() && Existing->Actor.IsValid())
			{
				AddedCameraActors.Add(Existing->Actor.Get());
			}
		}

		for (TActorIterator<ACameraActor> It(World); It; ++It)
		{
			ACameraActor* CameraActor = *It;
			if (!CameraActor || CameraActor->IsA<ARshipCameraActor>())
			{
				continue;
			}
			if (AddedCameraActors.Contains(CameraActor))
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
			AddedCameraActors.Add(CameraActor);
		}

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor || Actor->IsA<ARshipCameraActor>() || AddedCameraActors.Contains(Actor))
			{
				continue;
			}

			if (!Actor->FindComponentByClass<UCameraComponent>())
			{
				continue;
			}

			FString ConvertedId;
			if (Converter)
			{
				ConvertedId = Converter->GetConvertedEntityId(Actor);
			}
			if (!ConvertedId.IsEmpty() && ExistingCameraIds.Contains(ConvertedId))
			{
				continue;
			}

			const FString ActorLabel = Actor->GetActorLabel();
			const FString ClassName = Actor->GetClass() ? Actor->GetClass()->GetName() : TEXT("CameraActor");
			const bool bIsCine = ClassName.Contains(TEXT("CineCamera"));
			TSharedPtr<FRshipIdOption> Opt = MakeShared<FRshipIdOption>();
			Opt->bIsSceneCamera = true;
			Opt->Actor = Actor;
			Opt->ResolvedId = ConvertedId;
			Opt->bRequiresConversion = ConvertedId.IsEmpty();
			Opt->Id = ConvertedId.IsEmpty() ? ActorLabel : ConvertedId;
			const FString Prefix = bIsCine ? TEXT("Scene CineCamera") : TEXT("Scene Camera");
			Opt->Label = ConvertedId.IsEmpty()
				? FString::Printf(TEXT("%s: %s (convert)"), *Prefix, *ActorLabel)
				: FString::Printf(TEXT("%s: %s (%s)"), *Prefix, *ActorLabel, *ConvertedId);
			CameraOptions.Add(Opt);
			AddedCameraActors.Add(Actor);
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
		const FString LabelSurface = Surface.Name.IsEmpty() ? Surface.Id : Surface.Name;
		if (Surface.Name.IsEmpty())
		{
			Opt->Label = Surface.MeshComponentName.IsEmpty()
				? LabelSurface
				: FString::Printf(TEXT("%s [%s]"), *LabelSurface, *Surface.MeshComponentName);
		}
		else
		{
			Opt->Label = Surface.MeshComponentName.IsEmpty()
				? LabelSurface
				: FString::Printf(TEXT("%s [%s]"), *LabelSurface, *Surface.MeshComponentName);
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

	UpdateProjectionFromActor(InDeltaTime);
}

FReply SRshipContentMappingPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const bool bCommandLike = InKeyEvent.IsCommandDown() || InKeyEvent.IsControlDown();
	if (!bCommandLike)
	{
		return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}

	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Enter)
	{
		ExecuteQuickCreateMapping();
		return FReply::Handled();
	}
	if (Key == EKeys::D)
	{
		if (DuplicateSelectedMappings())
		{
			return FReply::Handled();
		}
	}
	if (Key == EKeys::E)
	{
		if (ToggleSelectedMappingsEnabled())
		{
			return FReply::Handled();
		}
	}
	if (Key == EKeys::RightBracket)
	{
		SetSelectedMappingsConfigExpanded(true);
		return FReply::Handled();
	}
	if (Key == EKeys::LeftBracket)
	{
		SetSelectedMappingsConfigExpanded(false);
		return FReply::Handled();
	}

	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SRshipContentMappingPanel::StoreQuickCreateDefaults()
{
	GQuickCreateDefaults.bHasValue = true;
	GQuickCreateDefaults.bQuickAdvanced = bQuickAdvanced;
	GQuickCreateDefaults.SourceType = QuickSourceType;
	GQuickCreateDefaults.MapMode = QuickMapMode;
	GQuickCreateDefaults.ProjectId = QuickProjectIdInput.IsValid() ? QuickProjectIdInput->GetText().ToString().TrimStartAndEnd() : TEXT("");
	GQuickCreateDefaults.SourceId = QuickSourceIdInput.IsValid() ? QuickSourceIdInput->GetText().ToString().TrimStartAndEnd() : TEXT("");
	GQuickCreateDefaults.TargetId = QuickTargetIdInput.IsValid() ? QuickTargetIdInput->GetText().ToString().TrimStartAndEnd() : TEXT("");
	GQuickCreateDefaults.Width = QuickWidthInput.IsValid() ? QuickWidthInput->GetValue() : 1920;
	GQuickCreateDefaults.Height = QuickHeightInput.IsValid() ? QuickHeightInput->GetValue() : 1080;
	GQuickCreateDefaults.CaptureMode = QuickCaptureModeInput.IsValid() ? QuickCaptureModeInput->GetText().ToString().TrimStartAndEnd() : TEXT("FinalColorLDR");
	GQuickCreateDefaults.UvChannel = QuickUvChannelInput.IsValid() ? QuickUvChannelInput->GetValue() : 0;
	GQuickCreateDefaults.MaterialSlots = QuickMaterialSlotsInput.IsValid() ? QuickMaterialSlotsInput->GetText().ToString().TrimStartAndEnd() : TEXT("");
	GQuickCreateDefaults.MeshName = QuickMeshNameInput.IsValid() ? QuickMeshNameInput->GetText().ToString().TrimStartAndEnd() : TEXT("");
	GQuickCreateDefaults.Opacity = QuickOpacityInput.IsValid() ? QuickOpacityInput->GetValue() : 1.0f;
	GQuickCreateDefaults.FeedU = QuickFeedUInput.IsValid() ? QuickFeedUInput->GetValue() : 0.0f;
	GQuickCreateDefaults.FeedV = QuickFeedVInput.IsValid() ? QuickFeedVInput->GetValue() : 0.0f;
	GQuickCreateDefaults.FeedW = QuickFeedWInput.IsValid() ? QuickFeedWInput->GetValue() : 1.0f;
	GQuickCreateDefaults.FeedH = QuickFeedHInput.IsValid() ? QuickFeedHInput->GetValue() : 1.0f;
}

void SRshipContentMappingPanel::ApplyStoredQuickCreateDefaults()
{
	if (!GQuickCreateDefaults.bHasValue)
	{
		return;
	}

	bQuickAdvanced = GQuickCreateDefaults.bQuickAdvanced;
	QuickSourceType = GQuickCreateDefaults.SourceType.IsEmpty() ? TEXT("camera") : GQuickCreateDefaults.SourceType;
	QuickMapMode = GQuickCreateDefaults.MapMode.IsEmpty() ? TEXT("direct") : GQuickCreateDefaults.MapMode;

	if (QuickProjectIdInput.IsValid()) QuickProjectIdInput->SetText(FText::FromString(GQuickCreateDefaults.ProjectId));
	if (QuickSourceIdInput.IsValid()) QuickSourceIdInput->SetText(FText::FromString(GQuickCreateDefaults.SourceId));
	if (QuickTargetIdInput.IsValid()) QuickTargetIdInput->SetText(FText::FromString(GQuickCreateDefaults.TargetId));
	if (QuickWidthInput.IsValid()) QuickWidthInput->SetValue(GQuickCreateDefaults.Width);
	if (QuickHeightInput.IsValid()) QuickHeightInput->SetValue(GQuickCreateDefaults.Height);
	if (QuickCaptureModeInput.IsValid()) QuickCaptureModeInput->SetText(FText::FromString(GQuickCreateDefaults.CaptureMode));
	if (QuickUvChannelInput.IsValid()) QuickUvChannelInput->SetValue(GQuickCreateDefaults.UvChannel);
	if (QuickMaterialSlotsInput.IsValid()) QuickMaterialSlotsInput->SetText(FText::FromString(GQuickCreateDefaults.MaterialSlots));
	if (QuickMeshNameInput.IsValid()) QuickMeshNameInput->SetText(FText::FromString(GQuickCreateDefaults.MeshName));
	if (QuickOpacityInput.IsValid()) QuickOpacityInput->SetValue(GQuickCreateDefaults.Opacity);
	if (QuickFeedUInput.IsValid()) QuickFeedUInput->SetValue(GQuickCreateDefaults.FeedU);
	if (QuickFeedVInput.IsValid()) QuickFeedVInput->SetValue(GQuickCreateDefaults.FeedV);
	if (QuickFeedWInput.IsValid()) QuickFeedWInput->SetValue(GQuickCreateDefaults.FeedW);
	if (QuickFeedHInput.IsValid()) QuickFeedHInput->SetValue(GQuickCreateDefaults.FeedH);
	if (QuickModeSelector.IsValid()) QuickModeSelector->SetSelectedMode(QuickMapMode);
}

bool SRshipContentMappingPanel::DuplicateSelectedMappings()
{
	if (!GEngine)
	{
		return false;
	}

	URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	if (!Subsystem)
	{
		return false;
	}

	URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
	if (!Manager)
	{
		return false;
	}

	TSet<FString> SourceIds = SelectedMappingRows;
	if (SourceIds.Num() == 0 && !SelectedMappingId.IsEmpty())
	{
		SourceIds.Add(SelectedMappingId);
	}
	if (SourceIds.Num() == 0)
	{
		return false;
	}

	const TArray<FRshipContentMappingState> Mappings = Manager->GetMappings();
	SelectedMappingRows.Empty();

	int32 DuplicatedCount = 0;
	for (const FRshipContentMappingState& Mapping : Mappings)
	{
		if (!SourceIds.Contains(Mapping.Id))
		{
			continue;
		}

		FRshipContentMappingState Duplicated = Mapping;
		Duplicated.Id.Reset();
		Duplicated.Name = Mapping.Name.IsEmpty()
			? FString::Printf(TEXT("%s Copy"), *Mapping.Id)
			: FString::Printf(TEXT("%s Copy"), *Mapping.Name);

		const FString NewMappingId = Manager->CreateMapping(Duplicated);
		if (!NewMappingId.IsEmpty())
		{
			++DuplicatedCount;
			SelectedMappingRows.Add(NewMappingId);
			SetSelectedMappingId(NewMappingId);
		}
	}

	if (DuplicatedCount == 0)
	{
		return false;
	}

	if (PreviewLabel.IsValid())
	{
		PreviewLabel->SetText(FText::FromString(FString::Printf(TEXT("Duplicated %d mapping(s)"), DuplicatedCount)));
		PreviewLabel->SetColorAndOpacity(FLinearColor::White);
	}

	bHasListHash = false;
	bHasPendingListHash = false;
	RefreshStatus();
	return true;
}

bool SRshipContentMappingPanel::ToggleSelectedMappingsEnabled()
{
	if (!GEngine)
	{
		return false;
	}

	URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	if (!Subsystem)
	{
		return false;
	}

	URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
	if (!Manager)
	{
		return false;
	}

	TSet<FString> TargetIds = SelectedMappingRows;
	if (TargetIds.Num() == 0 && !SelectedMappingId.IsEmpty())
	{
		TargetIds.Add(SelectedMappingId);
	}
	if (TargetIds.Num() == 0)
	{
		return false;
	}

	int32 UpdatedCount = 0;
	for (const FRshipContentMappingState& Mapping : Manager->GetMappings())
	{
		if (!TargetIds.Contains(Mapping.Id))
		{
			continue;
		}

		FRshipContentMappingState Updated = Mapping;
		Updated.bEnabled = !Mapping.bEnabled;
		if (Manager->UpdateMapping(Updated))
		{
			++UpdatedCount;
		}
	}

	if (UpdatedCount == 0)
	{
		return false;
	}

	if (PreviewLabel.IsValid())
	{
		PreviewLabel->SetText(FText::FromString(FString::Printf(TEXT("Toggled enabled state on %d mapping(s)"), UpdatedCount)));
		PreviewLabel->SetColorAndOpacity(FLinearColor::White);
	}

	RefreshStatus();
	return true;
}

void SRshipContentMappingPanel::SetSelectedMappingsConfigExpanded(bool bExpanded)
{
	TSet<FString> TargetIds = SelectedMappingRows;
	if (TargetIds.Num() == 0 && !SelectedMappingId.IsEmpty())
	{
		TargetIds.Add(SelectedMappingId);
	}
	if (TargetIds.Num() == 0)
	{
		return;
	}

	for (const FString& MappingId : TargetIds)
	{
		SetMappingConfigExpanded(MappingId, bExpanded);
	}

	bHasListHash = false;
	bHasPendingListHash = false;
	RefreshStatus();
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
				.Text(LOCTEXT("CountsUnknown", "Inputs: 0  Screens: 0  Mappings: 0"))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4, 0, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("HeaderNote", "Lightweight editor-side controls; full editing also available in rship client."))
			.ColorAndOpacity(FLinearColor::Gray)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 6, 0, 0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
				.IsChecked_Lambda([this]() { return bCoveragePreviewEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
				{
					bCoveragePreviewEnabled = (State == ECheckBoxState::Checked);
					if (GEngine)
					{
						if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
						{
							if (URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager())
							{
								Manager->SetCoveragePreviewEnabled(bCoveragePreviewEnabled);
							}
						}
					}
					if (PreviewLabel.IsValid())
					{
						PreviewLabel->SetText(bCoveragePreviewEnabled
							? LOCTEXT("CoveragePreviewOn", "Coverage preview enabled: red = unmapped pixels, live image = mapped.")
							: LOCTEXT("CoveragePreviewOff", "Coverage preview disabled."));
						PreviewLabel->SetColorAndOpacity(bCoveragePreviewEnabled ? FLinearColor::White : FLinearColor::Gray);
					}
				})
				[
					SNew(STextBlock).Text(LOCTEXT("CoveragePreviewToggle", "Coverage Preview"))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("StopProjectionEdit", "Stop Projection Edit"))
				.Visibility_Lambda([this]() { return ActiveProjectionMappingId.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
				.OnClicked_Lambda([this]()
				{
					StopProjectionEdit();
					return FReply::Handled();
				})
			]
		];
}

bool SRshipContentMappingPanel::ExecuteQuickCreateMapping()
{
	if (!GEngine)
	{
		return false;
	}

	URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	if (!Subsystem)
	{
		return false;
	}

	URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
	if (!Manager)
	{
		return false;
	}

	const FString ProjectId = QuickProjectIdInput.IsValid() ? QuickProjectIdInput->GetText().ToString().TrimStartAndEnd() : TEXT("");
	const FString SourceId = QuickSourceIdInput.IsValid() ? QuickSourceIdInput->GetText().ToString().TrimStartAndEnd() : TEXT("");
	const FString ScreenInput = QuickTargetIdInput.IsValid() ? QuickTargetIdInput->GetText().ToString().TrimStartAndEnd() : TEXT("");
	const TArray<FRshipMappingSurfaceState> Surfaces = Manager->GetMappingSurfaces();
	TMap<FString, const FRshipMappingSurfaceState*> SurfacesById;
	TMultiMap<FString, const FRshipMappingSurfaceState*> SurfacesByTarget;
	for (const FRshipMappingSurfaceState& Surface : Surfaces)
	{
		if (!Surface.Id.IsEmpty())
		{
			SurfacesById.Add(Surface.Id, &Surface);
		}
		if (!Surface.TargetId.IsEmpty())
		{
			SurfacesByTarget.Add(Surface.TargetId, &Surface);
		}
	}

	const int32 Width = bQuickAdvanced && QuickWidthInput.IsValid() ? QuickWidthInput->GetValue() : 0;
	const int32 Height = bQuickAdvanced && QuickHeightInput.IsValid() ? QuickHeightInput->GetValue() : 0;
	const FString CaptureMode = bQuickAdvanced && QuickCaptureModeInput.IsValid() ? QuickCaptureModeInput->GetText().ToString().TrimStartAndEnd() : TEXT("");
	const int32 UVChannel = QuickUvChannelInput.IsValid() ? QuickUvChannelInput->GetValue() : 0;
	const float Opacity = QuickOpacityInput.IsValid() ? QuickOpacityInput->GetValue() : 1.0f;
	const FString MeshName = bQuickAdvanced && QuickMeshNameInput.IsValid() ? QuickMeshNameInput->GetText().ToString().TrimStartAndEnd() : TEXT("");

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

	auto ParseTokenList = [](const FString& Text)
	{
		TArray<FString> Out;
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT(","), true);
		for (FString& Part : Parts)
		{
			const FString Token = Part.TrimStartAndEnd();
			if (!Token.IsEmpty() && !Out.Contains(Token))
			{
				Out.Add(Token);
			}
		}
		return Out;
	};

	auto NormalizeSurfaceIds = [](const TArray<FString>& SurfaceIdList)
	{
		TArray<FString> Out;
		Out.Reserve(SurfaceIdList.Num());
		for (const FString& SurfaceId : SurfaceIdList)
		{
			if (!SurfaceId.IsEmpty())
			{
				Out.Add(SurfaceId);
			}
		}
		Out.Sort();
		TArray<FString> Normalized;
		Normalized.Reserve(Out.Num());
		for (const FString& SurfaceId : Out)
		{
			if (Normalized.Num() == 0 || Normalized.Last() != SurfaceId)
			{
				Normalized.Add(SurfaceId);
			}
		}
		return Normalized;
	};

	const FString SlotsText = bQuickAdvanced && QuickMaterialSlotsInput.IsValid() ? QuickMaterialSlotsInput->GetText().ToString() : TEXT("");
	TArray<int32> RequestedSlots = SlotsText.IsEmpty() ? TArray<int32>() : ParseSlots(SlotsText);
	const TArray<FString> ScreenTokens = ParseTokenList(ScreenInput);
	TArray<FString> SurfaceIds;
	TArray<FString> SurfaceLabels;
	SurfaceIds.Reserve(ScreenTokens.Num());
	SurfaceLabels.Reserve(ScreenTokens.Num());
	TSet<FString> AddedSurfaceIds;

	for (const FString& RawToken : ScreenTokens)
	{
		const FString ResolvedScreenId = ResolveScreenIdInput(RawToken);

		const FRshipMappingSurfaceState* FoundSurface = nullptr;
		FString ScreenLabel;

		if (const FRshipMappingSurfaceState** Surface = SurfacesById.Find(ResolvedScreenId))
		{
			FoundSurface = *Surface;
			ScreenLabel = FoundSurface->Name.IsEmpty() ? FoundSurface->Id : FoundSurface->Name;
		}
		else
		{
			const FString ScreenTargetId = ResolveTargetIdInput(ResolvedScreenId);
			ScreenLabel = ShortTargetLabel(ScreenTargetId);

			if (!ScreenTargetId.IsEmpty())
			{
				TArray<const FRshipMappingSurfaceState*> TargetMatches;
				SurfacesByTarget.MultiFind(ScreenTargetId, TargetMatches);
				if (TargetMatches.Num() > 0)
				{
					for (const FRshipMappingSurfaceState* Candidate : TargetMatches)
					{
						if (!Candidate)
						{
							continue;
						}
						if (ProjectId.IsEmpty())
						{
							if (!Candidate->ProjectId.IsEmpty())
							{
								continue;
							}
						}
						else if (Candidate->ProjectId != ProjectId)
						{
							continue;
						}
						if (Candidate->UVChannel != UVChannel)
						{
							continue;
						}
						if (!MeshName.IsEmpty() && Candidate->MeshComponentName != MeshName)
						{
							continue;
						}
						if (RequestedSlots.Num() > 0)
						{
							TArray<int32> ExistingSlots = Candidate->MaterialSlots;
							ExistingSlots.Sort();
							if (ExistingSlots != RequestedSlots)
							{
								continue;
							}
						}
						FoundSurface = Candidate;
						ScreenLabel = Candidate->Name.IsEmpty() ? Candidate->Id : Candidate->Name;
						break;
					}
				}

				if (FoundSurface == nullptr)
				{
					FRshipMappingSurfaceState NewSurface;
					NewSurface.Name = ScreenLabel.IsEmpty() ? TEXT("Screen") : FString::Printf(TEXT("Screen %s"), *ScreenLabel);
					NewSurface.ProjectId = ProjectId;
					NewSurface.TargetId = ScreenTargetId;
					NewSurface.UVChannel = UVChannel;
					NewSurface.MaterialSlots = RequestedSlots;
					NewSurface.MeshComponentName = MeshName;
					NewSurface.bEnabled = true;
					const FString NewSurfaceId = Manager->CreateMappingSurface(NewSurface);
					if (!NewSurfaceId.IsEmpty())
					{
						if (!AddedSurfaceIds.Contains(NewSurfaceId))
						{
							AddedSurfaceIds.Add(NewSurfaceId);
							SurfaceIds.Add(NewSurfaceId);
						}
						SurfaceLabels.Add(NewSurface.Name);
						continue;
					}
				}
			}
		}

		if (FoundSurface != nullptr && !FoundSurface->Id.IsEmpty())
		{
			if (!AddedSurfaceIds.Contains(FoundSurface->Id))
			{
				AddedSurfaceIds.Add(FoundSurface->Id);
				SurfaceIds.Add(FoundSurface->Id);
				SurfaceLabels.Add(ScreenLabel.IsEmpty() ? FoundSurface->Id : ScreenLabel);
			}
		}
	}

	const TArray<FString> SortedSurfaceIds = NormalizeSurfaceIds(SurfaceIds);

	FString ContextId;
	if (!SourceId.IsEmpty())
	{
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
	}

	const bool bQuickIsUv = QuickMapMode == TEXT("direct") || QuickMapMode == TEXT("feed");
	const FString DesiredType = bQuickIsUv ? TEXT("surface-uv") : TEXT("surface-projection");
	const FString DesiredProjectionType = bQuickIsUv ? TEXT("") : QuickMapMode;
	const FString DesiredUvMode = (QuickMapMode == TEXT("feed")) ? TEXT("feed") : TEXT("direct");

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
		if (Mapping.Type != DesiredType) continue;
		if (DesiredType == TEXT("surface-uv"))
		{
			const FString ExistingUvMode = Mapping.Config.IsValid() && Mapping.Config->HasTypedField<EJson::String>(TEXT("uvMode"))
				? Mapping.Config->GetStringField(TEXT("uvMode"))
				: TEXT("direct");
			if (DesiredUvMode == TEXT("feed") && !ExistingUvMode.Equals(TEXT("feed"), ESearchCase::IgnoreCase)) continue;
			if (DesiredUvMode == TEXT("direct") && ExistingUvMode.Equals(TEXT("feed"), ESearchCase::IgnoreCase)) continue;
		}
		else
		{
			const FString ExistingProj = GetProjectionModeFromConfig(Mapping.Config);
			if (NormalizeMapMode(ExistingProj, MapModePerspective) != NormalizeMapMode(DesiredProjectionType, MapModePerspective)) continue;
		}
		if (Mapping.ContextId != ContextId) continue;
		if (NormalizeSurfaceIds(Mapping.SurfaceIds) != SortedSurfaceIds) continue;
		MappingId = Mapping.Id;
		break;
	}

	FString MappingName;
	if (SortedSurfaceIds.Num() == 0)
	{
		MappingName = TEXT("Map (Abstract)");
	}
	else if (SortedSurfaceIds.Num() == 1)
	{
		const FString* ScreenLabel = SurfaceLabels.Num() > 0 ? &SurfaceLabels[0] : nullptr;
		if (ScreenLabel)
		{
			MappingName = TEXT("Map ");
			MappingName += **ScreenLabel;
		}
		else
		{
			MappingName = TEXT("Map");
		}
	}
	else
	{
		MappingName = FString::Printf(TEXT("Map %d Screens"), SortedSurfaceIds.Num());
	}

	if (MappingId.IsEmpty())
	{
		FRshipContentMappingState NewMapping;
		NewMapping.Name = MappingName;
		NewMapping.ProjectId = ProjectId;
		NewMapping.Type = DesiredType;
		NewMapping.ContextId = ContextId;
		NewMapping.SurfaceIds = SortedSurfaceIds;
		NewMapping.Opacity = Opacity;
		NewMapping.bEnabled = true;
		NewMapping.Config = MakeShared<FJsonObject>();
		if (DesiredType == TEXT("surface-uv"))
		{
			NewMapping.Config->SetStringField(TEXT("uvMode"), DesiredUvMode);
			TSharedPtr<FJsonObject> Uv = MakeShared<FJsonObject>();
			Uv->SetNumberField(TEXT("scaleU"), 1.0);
			Uv->SetNumberField(TEXT("scaleV"), 1.0);
			Uv->SetNumberField(TEXT("offsetU"), 0.0);
			Uv->SetNumberField(TEXT("offsetV"), 0.0);
			Uv->SetNumberField(TEXT("rotationDeg"), 0.0);
			NewMapping.Config->SetObjectField(TEXT("uvTransform"), Uv);
			if (DesiredUvMode == TEXT("feed"))
			{
				TSharedPtr<FJsonObject> Feed = MakeShared<FJsonObject>();
				Feed->SetNumberField(TEXT("u"), QuickFeedUInput.IsValid() ? QuickFeedUInput->GetValue() : 0.0);
				Feed->SetNumberField(TEXT("v"), QuickFeedVInput.IsValid() ? QuickFeedVInput->GetValue() : 0.0);
				Feed->SetNumberField(TEXT("width"), QuickFeedWInput.IsValid() ? QuickFeedWInput->GetValue() : 1.0);
				Feed->SetNumberField(TEXT("height"), QuickFeedHInput.IsValid() ? QuickFeedHInput->GetValue() : 1.0);
				NewMapping.Config->SetObjectField(TEXT("feedRect"), Feed);
			}
		}
		else
		{
			NewMapping.Config->SetStringField(TEXT("projectionType"), DesiredProjectionType.IsEmpty() ? TEXT("perspective") : DesiredProjectionType);
			if (DesiredProjectionType.Equals(MapModeCustomMatrix, ESearchCase::IgnoreCase))
			{
				NewMapping.Config->SetObjectField(TEXT("customProjectionMatrix"), BuildDefaultCustomProjectionMatrixObject());
			}
			if (DesiredProjectionType.Equals(TEXT("cylindrical"), ESearchCase::IgnoreCase)
				|| DesiredProjectionType.Equals(TEXT("radial"), ESearchCase::IgnoreCase))
			{
				TSharedPtr<FJsonObject> Cyl = MakeShared<FJsonObject>();
				Cyl->SetStringField(TEXT("axis"), TEXT("y"));
				Cyl->SetNumberField(TEXT("radius"), 100.0);
				Cyl->SetNumberField(TEXT("height"), 1000.0);
				Cyl->SetNumberField(TEXT("startAngle"), 0.0);
				Cyl->SetNumberField(TEXT("endAngle"), 90.0);
				NewMapping.Config->SetObjectField(TEXT("cylindrical"), Cyl);
			}
			if (DesiredProjectionType.Equals(TEXT("spherical"), ESearchCase::IgnoreCase))
			{
				NewMapping.Config->SetNumberField(TEXT("sphereRadius"), 500.0);
				NewMapping.Config->SetNumberField(TEXT("horizontalArc"), 360.0);
				NewMapping.Config->SetNumberField(TEXT("verticalArc"), 180.0);
			}
			if (DesiredProjectionType.Equals(TEXT("parallel"), ESearchCase::IgnoreCase))
			{
				NewMapping.Config->SetNumberField(TEXT("sizeW"), 1000.0);
				NewMapping.Config->SetNumberField(TEXT("sizeH"), 1000.0);
			}
			if (DesiredProjectionType.Equals(TEXT("mesh"), ESearchCase::IgnoreCase))
			{
				TSharedPtr<FJsonObject> EpObj = MakeShared<FJsonObject>();
				EpObj->SetNumberField(TEXT("x"), 0.0);
				EpObj->SetNumberField(TEXT("y"), 0.0);
				EpObj->SetNumberField(TEXT("z"), 0.0);
				NewMapping.Config->SetObjectField(TEXT("eyepoint"), EpObj);
			}
			if (DesiredProjectionType.Equals(TEXT("fisheye"), ESearchCase::IgnoreCase))
			{
				NewMapping.Config->SetNumberField(TEXT("fisheyeFov"), 180.0);
				NewMapping.Config->SetStringField(TEXT("lensType"), TEXT("equidistant"));
			}
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

	SetSelectedMappingId(MappingId);
	LastPreviewMappingId = MappingId;
	StoreQuickCreateDefaults();
	if (PreviewLabel.IsValid())
	{
		PreviewLabel->SetText(LOCTEXT("QuickCreated", "Mapping created (context/surface reused when possible)."));
		PreviewLabel->SetColorAndOpacity(FLinearColor::White);
	}
	RefreshStatus();
	return true;
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
				.Text(LOCTEXT("QuickTitle", "Create Mapping"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 6)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("QuickNote", "Create a mapping with optional input/screen values, then refine later."))
				.ColorAndOpacity(FLinearColor::Gray)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
				[
					SNew(STextBlock).Text(LOCTEXT("QuickSourceLabel", "Input"))
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
					.Visibility_Lambda([this]()
					{
						return bQuickAdvanced ? EVisibility::Visible : EVisibility::Collapsed;
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
							const bool bIsCamera = QuickSourceType == TEXT("camera");
							const FString Current = QuickSourceIdInput.IsValid() ? QuickSourceIdInput->GetText().ToString().TrimStartAndEnd() : TEXT("");
							if (!Current.IsEmpty())
							{
								const TArray<TSharedPtr<FRshipIdOption>>& Options = bIsCamera ? CameraOptions : AssetOptions;
								for (const TSharedPtr<FRshipIdOption>& Option : Options)
								{
									if (!Option.IsValid())
									{
										continue;
									}
									if (Option->Id.Equals(Current, ESearchCase::IgnoreCase) || Option->ResolvedId.Equals(Current, ESearchCase::IgnoreCase))
									{
										return FText::FromString(Option->Label);
									}
								}
								return FText::FromString(Current);
							}
							return bIsCamera ? LOCTEXT("QuickPickCamera", "Pick Camera") : LOCTEXT("QuickPickAsset", "Pick Asset");
						})
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,6,0)
				[
					SNew(SButton)
					.Visibility_Lambda([this]() { return QuickSourceType == TEXT("camera") ? EVisibility::Visible : EVisibility::Collapsed; })
					.Text(LOCTEXT("QuickUseSelectedCamera", "Use Selected"))
					.OnClicked_Lambda([this]()
					{
						const bool bOk = TryApplySelectionToCamera(QuickSourceIdInput);
						if (!bOk && PreviewLabel.IsValid())
						{
							PreviewLabel->SetText(LOCTEXT("QuickSelectCameraFail", "Select a camera actor in the level to use it as the source."));
							PreviewLabel->SetColorAndOpacity(FLinearColor::Yellow);
						}
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot().FillWidth(0.6f)
				[
					SAssignNew(QuickProjectIdInput, SEditableTextBox)
					.HintText(LOCTEXT("QuickProjectHint", "ProjectId (optional)"))
					.Visibility_Lambda([this]()
					{
						return bQuickAdvanced ? EVisibility::Visible : EVisibility::Collapsed;
					})
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
				[
					SNew(STextBlock).Text(LOCTEXT("QuickTargetLabel", "Screens"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.2f).Padding(0,0,4,0)
				[
					SAssignNew(QuickTargetIdInput, SEditableTextBox)
					.HintText(LOCTEXT("QuickTargetHint", "Pick or type screens (comma-separated)"))
					.Visibility_Lambda([this]()
					{
						return bQuickAdvanced ? EVisibility::Visible : EVisibility::Collapsed;
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,6,0)
				[
					SNew(SComboButton)
					.OnGetMenuContent_Lambda([this]()
					{
						return BuildIdPickerMenu(SurfaceOptions, LOCTEXT("QuickNoScreens", "No screens found"), QuickTargetIdInput, true);
					})
					.ButtonContent()
					[
						SNew(STextBlock).Text_Lambda([this]()
						{
							const FString Current = QuickTargetIdInput.IsValid() ? QuickTargetIdInput->GetText().ToString().TrimStartAndEnd() : TEXT("");
							if (!Current.IsEmpty())
							{
								for (const TSharedPtr<FRshipIdOption>& Option : SurfaceOptions)
								{
									if (!Option.IsValid())
									{
										continue;
									}
									if (Option->Id.Equals(Current, ESearchCase::IgnoreCase) || Option->ResolvedId.Equals(Current, ESearchCase::IgnoreCase))
									{
										return FText::FromString(Option->Label);
									}
								}
								return FText::FromString(Current);
							}
							return LOCTEXT("QuickPickTarget", "Add Screen");
						})
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,6,0)
				[
					SNew(SButton)
					.Text(LOCTEXT("QuickUseSelectedTarget", "Use Selected"))
					.OnClicked_Lambda([this]()
					{
						const bool bOk = TryApplySelectionToTarget(QuickTargetIdInput, true);
						if (!bOk && PreviewLabel.IsValid())
						{
							PreviewLabel->SetText(LOCTEXT("QuickSelectTargetFail", "Select a screen actor (with a RshipTargetComponent) in the level."));
							PreviewLabel->SetColorAndOpacity(FLinearColor::Yellow);
						}
						return FReply::Handled();
					})
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
				SAssignNew(QuickModeSelector, SRshipModeSelector)
				.OnModeSelected_Lambda([this](const FString& Mode) { QuickMapMode = Mode; })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				SNew(SHorizontalBox)
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
				.Padding(0,0,6,0)
				[
					SNew(SButton)
					.Text(LOCTEXT("QuickReuseLastButton", "Reuse Last"))
					.OnClicked_Lambda([this]()
					{
						ApplyStoredQuickCreateDefaults();
						if (PreviewLabel.IsValid())
						{
							PreviewLabel->SetText(LOCTEXT("QuickReuseLastNote", "Loaded last quick-create values."));
							PreviewLabel->SetColorAndOpacity(FLinearColor::White);
						}
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("QuickCreateButton", "Create Mapping (Cmd/Ctrl+Enter)"))
					.OnClicked_Lambda([this]()
					{
						ExecuteQuickCreateMapping();
						return FReply::Handled();
					})
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				SNew(SHorizontalBox)
				.Visibility_Lambda([this]() { return QuickMapMode == TEXT("feed") ? EVisibility::Visible : EVisibility::Collapsed; })
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
				[
					SNew(STextBlock).Text(LOCTEXT("QuickFeedRectLabel", "Feed Rect (U V W H)"))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
				[
					SAssignNew(QuickFeedUInput, SSpinBox<float>)
					.MinValue(-10.0f).MaxValue(10.0f).Delta(0.01f).Value(0.0f)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
				[
					SAssignNew(QuickFeedVInput, SSpinBox<float>)
					.MinValue(-10.0f).MaxValue(10.0f).Delta(0.01f).Value(0.0f)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
				[
					SAssignNew(QuickFeedWInput, SSpinBox<float>)
					.MinValue(0.001f).MaxValue(10.0f).Delta(0.01f).Value(1.0f)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
				[
					SAssignNew(QuickFeedHInput, SSpinBox<float>)
					.MinValue(0.001f).MaxValue(10.0f).Delta(0.01f).Value(1.0f)
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
			.Text(LOCTEXT("ContextsTitle", "Inputs"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 8)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0,0,4,0)
			[
				SAssignNew(ContextFilterInput, SEditableTextBox)
				.HintText(LOCTEXT("ContextFilterHint", "Filter inputs..."))
				.Text_Lambda([this]() { return FText::FromString(ContextFilterText); })
				.OnTextChanged_Lambda([this](const FText& NewText)
				{
					ContextFilterText = NewText.ToString().TrimStartAndEnd();
					bHasListHash = false;
					bHasPendingListHash = false;
					RefreshStatus();
				})
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("ContextFilterClear", "Clear"))
				.IsEnabled_Lambda([this]() { return !ContextFilterText.IsEmpty(); })
				.OnClicked_Lambda([this]()
				{
					ContextFilterText.Reset();
					if (ContextFilterInput.IsValid())
					{
						ContextFilterInput->SetText(FText::GetEmpty());
					}
					bHasListHash = false;
					bHasPendingListHash = false;
					RefreshStatus();
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(6,0,0,0)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bContextErrorsOnly ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
				{
					bContextErrorsOnly = (NewState == ECheckBoxState::Checked);
					bHasListHash = false;
					bHasPendingListHash = false;
					RefreshStatus();
				})
				[
					SNew(STextBlock).Text(LOCTEXT("ContextErrorsOnly", "Errors"))
				]
			]
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
			.Text(LOCTEXT("SurfacesTitle", "Screens"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 8)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0,0,4,0)
			[
				SAssignNew(SurfaceFilterInput, SEditableTextBox)
				.HintText(LOCTEXT("SurfaceFilterHint", "Filter screens..."))
				.Text_Lambda([this]() { return FText::FromString(SurfaceFilterText); })
				.OnTextChanged_Lambda([this](const FText& NewText)
				{
					SurfaceFilterText = NewText.ToString().TrimStartAndEnd();
					bHasListHash = false;
					bHasPendingListHash = false;
					RefreshStatus();
				})
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("SurfaceFilterClear", "Clear"))
				.IsEnabled_Lambda([this]() { return !SurfaceFilterText.IsEmpty(); })
				.OnClicked_Lambda([this]()
				{
					SurfaceFilterText.Reset();
					if (SurfaceFilterInput.IsValid())
					{
						SurfaceFilterInput->SetText(FText::GetEmpty());
					}
					bHasListHash = false;
					bHasPendingListHash = false;
					RefreshStatus();
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(6,0,0,0)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bSurfaceErrorsOnly ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
				{
					bSurfaceErrorsOnly = (NewState == ECheckBoxState::Checked);
					bHasListHash = false;
					bHasPendingListHash = false;
					RefreshStatus();
				})
				[
					SNew(STextBlock).Text(LOCTEXT("SurfaceErrorsOnly", "Errors"))
				]
			]
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
			.Text(LOCTEXT("MappingsTitle", "Mappings"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 6)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0,0,4,0)
			[
				SAssignNew(MappingFilterInput, SEditableTextBox)
				.HintText(LOCTEXT("MappingFilterHint", "Filter mappings..."))
				.Text_Lambda([this]() { return FText::FromString(MappingFilterText); })
				.OnTextChanged_Lambda([this](const FText& NewText)
				{
					MappingFilterText = NewText.ToString().TrimStartAndEnd();
					bHasListHash = false;
					bHasPendingListHash = false;
					RefreshStatus();
				})
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("MappingFilterClear", "Clear"))
				.IsEnabled_Lambda([this]() { return !MappingFilterText.IsEmpty(); })
				.OnClicked_Lambda([this]()
				{
					MappingFilterText.Reset();
					if (MappingFilterInput.IsValid())
					{
						MappingFilterInput->SetText(FText::GetEmpty());
					}
					bHasListHash = false;
					bHasPendingListHash = false;
					RefreshStatus();
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(6,0,0,0)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bMappingErrorsOnly ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
				{
					bMappingErrorsOnly = (NewState == ECheckBoxState::Checked);
					bHasListHash = false;
					bHasPendingListHash = false;
					RefreshStatus();
				})
				[
					SNew(STextBlock).Text(LOCTEXT("MappingErrorsOnly", "Errors"))
				]
			]
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
				SNew(STextBlock).Text(LOCTEXT("CtxFormTitle", "Input")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
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

			+ SVerticalBox::Slot().AutoHeight().Padding(0,6,0,2)
			[
				SNew(SSeparator)
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
			[
				SAssignNew(MapModeSelector, SRshipModeSelector)
					.OnModeSelected_Lambda([this](const FString& Mode)
					{
						MapMode = Mode;
						bShowProjectionPrecisionControls = false;
						if (!IsProjectionMode(Mode))
						{
							StopProjectionEdit();
						}
						RebuildFeedRectList();
						if (MappingCanvas.IsValid())
						{
							MappingCanvas->SetDisplayMode(Mode);
						}
					})
			]

				+ SVerticalBox::Slot().AutoHeight().Padding(0,4,0,2)
				[
					SNew(SVerticalBox)
					.Visibility_Lambda([this]() { return (MapMode == TEXT("direct") || MapMode == TEXT("feed")) ? EVisibility::Visible : EVisibility::Collapsed; })
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text(LOCTEXT("MapUvTransformHeader", "UV Transform"))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
					[
						SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
					[
						SNew(STextBlock).Text(LOCTEXT("MapUvScale", "Scale U/V"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapUvScaleUInput, SSpinBox<float>).MinValue(0.01f).MaxValue(100.0f).Delta(0.05f).Value(1.0f)
						.OnValueChanged_Lambda([this](float Val)
						{
							if (MappingCanvas.IsValid())
							{
								MappingCanvas->SetUvTransform(Val,
									MapUvScaleVInput.IsValid() ? MapUvScaleVInput->GetValue() : 1.0f,
									MapUvOffsetUInput.IsValid() ? MapUvOffsetUInput->GetValue() : 0.0f,
									MapUvOffsetVInput.IsValid() ? MapUvOffsetVInput->GetValue() : 0.0f,
									MapUvRotInput.IsValid() ? MapUvRotInput->GetValue() : 0.0f);
							}
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
					[
						SAssignNew(MapUvScaleVInput, SSpinBox<float>).MinValue(0.01f).MaxValue(100.0f).Delta(0.05f).Value(1.0f)
						.OnValueChanged_Lambda([this](float Val)
						{
							if (MappingCanvas.IsValid())
							{
								MappingCanvas->SetUvTransform(
									MapUvScaleUInput.IsValid() ? MapUvScaleUInput->GetValue() : 1.0f,
									Val,
									MapUvOffsetUInput.IsValid() ? MapUvOffsetUInput->GetValue() : 0.0f,
									MapUvOffsetVInput.IsValid() ? MapUvOffsetVInput->GetValue() : 0.0f,
									MapUvRotInput.IsValid() ? MapUvRotInput->GetValue() : 0.0f);
							}
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
					[
						SNew(STextBlock).Text(LOCTEXT("MapUvOffset", "Offset U/V"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapUvOffsetUInput, SSpinBox<float>).MinValue(-10.0f).MaxValue(10.0f).Delta(0.01f).Value(0.0f)
						.OnValueChanged_Lambda([this](float Val)
						{
							if (MappingCanvas.IsValid())
							{
								MappingCanvas->SetUvTransform(
									MapUvScaleUInput.IsValid() ? MapUvScaleUInput->GetValue() : 1.0f,
									MapUvScaleVInput.IsValid() ? MapUvScaleVInput->GetValue() : 1.0f,
									Val,
									MapUvOffsetVInput.IsValid() ? MapUvOffsetVInput->GetValue() : 0.0f,
									MapUvRotInput.IsValid() ? MapUvRotInput->GetValue() : 0.0f);
							}
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
					[
						SAssignNew(MapUvOffsetVInput, SSpinBox<float>).MinValue(-10.0f).MaxValue(10.0f).Delta(0.01f).Value(0.0f)
						.OnValueChanged_Lambda([this](float Val)
						{
							if (MappingCanvas.IsValid())
							{
								MappingCanvas->SetUvTransform(
									MapUvScaleUInput.IsValid() ? MapUvScaleUInput->GetValue() : 1.0f,
									MapUvScaleVInput.IsValid() ? MapUvScaleVInput->GetValue() : 1.0f,
									MapUvOffsetUInput.IsValid() ? MapUvOffsetUInput->GetValue() : 0.0f,
									Val,
									MapUvRotInput.IsValid() ? MapUvRotInput->GetValue() : 0.0f);
							}
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
					[
						SNew(STextBlock).Text(LOCTEXT("MapUvRot", "Rotation"))
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SAssignNew(MapUvRotInput, SSpinBox<float>).MinValue(-360.0f).MaxValue(360.0f).Delta(1.0f).Value(0.0f)
						.OnValueChanged_Lambda([this](float Val)
						{
							if (MappingCanvas.IsValid())
							{
								MappingCanvas->SetUvTransform(
									MapUvScaleUInput.IsValid() ? MapUvScaleUInput->GetValue() : 1.0f,
									MapUvScaleVInput.IsValid() ? MapUvScaleVInput->GetValue() : 1.0f,
									MapUvOffsetUInput.IsValid() ? MapUvOffsetUInput->GetValue() : 0.0f,
									MapUvOffsetVInput.IsValid() ? MapUvOffsetVInput->GetValue() : 0.0f,
									Val);
							}
						})
					]
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,4,0,2)
			[
				SNew(SVerticalBox)
				.Visibility_Lambda([this]() { return MapMode == TEXT("feed") ? EVisibility::Visible : EVisibility::Collapsed; })
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(LOCTEXT("MapFeedHeader", "Feed Rect"))
				]
					+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
					[
						SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
					[
						SNew(STextBlock).Text(LOCTEXT("MapFeedDefault", "Default (U V W H)"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapFeedUInput, SSpinBox<float>).MinValue(-10.0f).MaxValue(10.0f).Delta(0.01f).Value(0.0f)
						.OnValueChanged_Lambda([this](float Val)
						{
							if (MappingCanvas.IsValid())
							{
								MappingCanvas->SetFeedRect(Val,
									MapFeedVInput.IsValid() ? MapFeedVInput->GetValue() : 0.0f,
									MapFeedWInput.IsValid() ? MapFeedWInput->GetValue() : 1.0f,
									MapFeedHInput.IsValid() ? MapFeedHInput->GetValue() : 1.0f);
							}
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapFeedVInput, SSpinBox<float>).MinValue(-10.0f).MaxValue(10.0f).Delta(0.01f).Value(0.0f)
						.OnValueChanged_Lambda([this](float Val)
						{
							if (MappingCanvas.IsValid())
							{
								MappingCanvas->SetFeedRect(
									MapFeedUInput.IsValid() ? MapFeedUInput->GetValue() : 0.0f,
									Val,
									MapFeedWInput.IsValid() ? MapFeedWInput->GetValue() : 1.0f,
									MapFeedHInput.IsValid() ? MapFeedHInput->GetValue() : 1.0f);
							}
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapFeedWInput, SSpinBox<float>).MinValue(0.001f).MaxValue(10.0f).Delta(0.01f).Value(1.0f)
						.OnValueChanged_Lambda([this](float Val)
						{
							if (MappingCanvas.IsValid())
							{
								MappingCanvas->SetFeedRect(
									MapFeedUInput.IsValid() ? MapFeedUInput->GetValue() : 0.0f,
									MapFeedVInput.IsValid() ? MapFeedVInput->GetValue() : 0.0f,
									Val,
									MapFeedHInput.IsValid() ? MapFeedHInput->GetValue() : 1.0f);
							}
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,6,0)
					[
						SAssignNew(MapFeedHInput, SSpinBox<float>).MinValue(0.001f).MaxValue(10.0f).Delta(0.01f).Value(1.0f)
						.OnValueChanged_Lambda([this](float Val)
						{
							if (MappingCanvas.IsValid())
							{
								MappingCanvas->SetFeedRect(
									MapFeedUInput.IsValid() ? MapFeedUInput->GetValue() : 0.0f,
									MapFeedVInput.IsValid() ? MapFeedVInput->GetValue() : 0.0f,
									MapFeedWInput.IsValid() ? MapFeedWInput->GetValue() : 1.0f,
									Val);
							}
						})
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("MapFeedApplyAll", "Apply to Screens"))
						.OnClicked_Lambda([this]()
						{
							if (!MapSurfacesInput.IsValid())
							{
								return FReply::Handled();
							}

							TArray<FString> SurfaceIds;
							MapSurfacesInput->GetText().ToString().ParseIntoArray(SurfaceIds, TEXT(","), true);
							for (FString& SurfaceId : SurfaceIds)
							{
								SurfaceId = SurfaceId.TrimStartAndEnd();
							}
							SurfaceIds.RemoveAll([](const FString& SurfaceId) { return SurfaceId.IsEmpty(); });

							FFeedRect Rect;
							Rect.U = MapFeedUInput.IsValid() ? MapFeedUInput->GetValue() : 0.0f;
							Rect.V = MapFeedVInput.IsValid() ? MapFeedVInput->GetValue() : 0.0f;
							Rect.W = MapFeedWInput.IsValid() ? MapFeedWInput->GetValue() : 1.0f;
							Rect.H = MapFeedHInput.IsValid() ? MapFeedHInput->GetValue() : 1.0f;

							for (const FString& SurfaceId : SurfaceIds)
							{
								MapFeedRectOverrides.Add(SurfaceId, Rect);
							}
							RebuildFeedRectList();
							return FReply::Handled();
						})
					]
				]
					+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
					[
						SNew(STextBlock).Text(LOCTEXT("MapFeedOverrides", "Screen Overrides"))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
						[
							SNew(SButton)
							.Text(LOCTEXT("MapFeedCopyTable", "Copy Table"))
							.OnClicked_Lambda([this]()
							{
								TArray<FString> SurfaceIds;
								if (MapSurfacesInput.IsValid())
								{
									MapSurfacesInput->GetText().ToString().ParseIntoArray(SurfaceIds, TEXT(","), true);
									for (FString& SurfaceId : SurfaceIds)
									{
										SurfaceId = SurfaceId.TrimStartAndEnd();
									}
									SurfaceIds.RemoveAll([](const FString& SurfaceId) { return SurfaceId.IsEmpty(); });
								}

								FFeedRect DefaultRect;
								DefaultRect.U = MapFeedUInput.IsValid() ? MapFeedUInput->GetValue() : 0.0f;
								DefaultRect.V = MapFeedVInput.IsValid() ? MapFeedVInput->GetValue() : 0.0f;
								DefaultRect.W = MapFeedWInput.IsValid() ? MapFeedWInput->GetValue() : 1.0f;
								DefaultRect.H = MapFeedHInput.IsValid() ? MapFeedHInput->GetValue() : 1.0f;

								TArray<FString> Lines;
								Lines.Add(TEXT("surfaceId\tu\tv\twidth\theight"));
								for (const FString& SurfaceId : SurfaceIds)
								{
									const FFeedRect* Found = MapFeedRectOverrides.Find(SurfaceId);
									const FFeedRect& Rect = Found ? *Found : DefaultRect;
									Lines.Add(FString::Printf(TEXT("%s\t%.6f\t%.6f\t%.6f\t%.6f"), *SurfaceId, Rect.U, Rect.V, Rect.W, Rect.H));
								}

								const FString ClipboardText = FString::Join(Lines, TEXT("\n"));
								FPlatformApplicationMisc::ClipboardCopy(*ClipboardText);
								return FReply::Handled();
							})
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
						[
							SNew(SButton)
							.Text(LOCTEXT("MapFeedPasteTable", "Paste Table"))
							.OnClicked_Lambda([this]()
							{
								FString ClipboardText;
								FPlatformApplicationMisc::ClipboardPaste(ClipboardText);
								if (ClipboardText.IsEmpty())
								{
									return FReply::Handled();
								}

								TSet<FString> AllowedSurfaceIds;
								if (MapSurfacesInput.IsValid())
								{
									TArray<FString> SurfaceIds;
									MapSurfacesInput->GetText().ToString().ParseIntoArray(SurfaceIds, TEXT(","), true);
									for (FString& SurfaceId : SurfaceIds)
									{
										SurfaceId = SurfaceId.TrimStartAndEnd();
										if (!SurfaceId.IsEmpty())
										{
											AllowedSurfaceIds.Add(SurfaceId);
										}
									}
								}

								TArray<FString> Lines;
								ClipboardText.ParseIntoArrayLines(Lines, true);
								for (const FString& Line : Lines)
								{
									const FString Trimmed = Line.TrimStartAndEnd();
									if (Trimmed.IsEmpty() || Trimmed.StartsWith(TEXT("surfaceId"), ESearchCase::IgnoreCase))
									{
										continue;
									}

									TArray<FString> Parts;
									Trimmed.ParseIntoArray(Parts, TEXT("\t"), true);
									if (Parts.Num() < 5)
									{
										Parts.Empty();
										Trimmed.ParseIntoArray(Parts, TEXT(","), true);
									}
									if (Parts.Num() < 5)
									{
										continue;
									}

									for (FString& Part : Parts)
									{
										Part = Part.TrimStartAndEnd();
									}

									const FString SurfaceId = Parts[0];
									if (SurfaceId.IsEmpty())
									{
										continue;
									}
									if (AllowedSurfaceIds.Num() > 0 && !AllowedSurfaceIds.Contains(SurfaceId))
									{
										continue;
									}

									FFeedRect Rect;
									Rect.U = FCString::Atof(*Parts[1]);
									Rect.V = FCString::Atof(*Parts[2]);
									Rect.W = FMath::Max(0.001f, FCString::Atof(*Parts[3]));
									Rect.H = FMath::Max(0.001f, FCString::Atof(*Parts[4]));
									MapFeedRectOverrides.Add(SurfaceId, Rect);
								}

								RebuildFeedRectList();
								return FReply::Handled();
							})
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton)
							.Text(LOCTEXT("MapFeedResetOverrides", "Reset Overrides"))
							.OnClicked_Lambda([this]()
							{
								MapFeedRectOverrides.Empty();
								RebuildFeedRectList();
								return FReply::Handled();
							})
						]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SAssignNew(MapFeedRectList, SVerticalBox)
					]
				]

			+ SVerticalBox::Slot().AutoHeight().Padding(0,4,0,2)
			[
				SNew(SVerticalBox)
				.Visibility_Lambda([this]()
				{
					return IsProjectionMode(MapMode) ? EVisibility::Visible : EVisibility::Collapsed;
				})
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
					[
						SNew(STextBlock).Text(LOCTEXT("MapProjHeader", "Projection"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,6,0)
					[
						SNew(SButton)
						.IsEnabled_Lambda([this]()
						{
							return IsProjectionMode(MapMode) && !SelectedMappingId.IsEmpty();
						})
						.Text_Lambda([this]()
						{
							if (!IsProjectionMode(MapMode) || SelectedMappingId.IsEmpty())
							{
								return LOCTEXT("MapEditProjCreatePrompt", "Save then Edit");
							}
							return IsProjectionEditActiveFor(SelectedMappingId)
								? LOCTEXT("MapEditingProj", "Editing Projection")
								: LOCTEXT("MapEditProj", "Edit Projection");
						})
						.OnClicked_Lambda([this]()
						{
							if (!IsProjectionMode(MapMode) || SelectedMappingId.IsEmpty() || !GEngine)
							{
								return FReply::Handled();
							}

							URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
							URshipContentMappingManager* Manager = Subsystem ? Subsystem->GetContentMappingManager() : nullptr;
							if (!Manager)
							{
								return FReply::Handled();
							}

							TArray<FRshipContentMappingState> Mappings = Manager->GetMappings();
							FRshipContentMappingState* Mapping = FindMappingById(SelectedMappingId, Mappings);
							if (!Mapping || !IsProjectionMode(GetMappingModeFromState(*Mapping)))
							{
								return FReply::Handled();
							}

							if (IsProjectionEditActiveFor(SelectedMappingId))
							{
								StopProjectionEdit();
							}
							else
							{
								StartProjectionEdit(*Mapping);
							}
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Visibility_Lambda([this]() { return IsProjectionMode(MapMode) ? EVisibility::Visible : EVisibility::Collapsed; })
						.Text_Lambda([this]()
						{
							return GetProjectionPrecisionControlsVisibility() == EVisibility::Visible
								? LOCTEXT("MapHidePrecision", "Hide Precision Controls")
								: LOCTEXT("MapShowPrecision", "Precision Controls");
						})
						.OnClicked_Lambda([this]()
						{
							bShowProjectionPrecisionControls = !bShowProjectionPrecisionControls;
							return FReply::Handled();
						})
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
					[
						SNew(SHorizontalBox).Visibility_Lambda([this]()
						{
							return GetProjectionPrecisionControlsCollapsedVisibility();
						})
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("MapProjectionManipulatorNotice", "Projection transforms are edited in the viewport manipulator. Open Precision Controls for numeric edits."))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
				[
						SNew(SHorizontalBox)
						.Visibility_Lambda([this]()
						{
							return GetProjectionPrecisionControlsVisibility();
						})
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
					[
						SNew(STextBlock).Text(LOCTEXT("MapProjPos", "Position X/Y/Z"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapProjPosXInput, SSpinBox<float>).MinValue(-100000.0f).MaxValue(100000.0f).Delta(1.0f).Value(0.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapProjPosYInput, SSpinBox<float>).MinValue(-100000.0f).MaxValue(100000.0f).Delta(1.0f).Value(0.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SAssignNew(MapProjPosZInput, SSpinBox<float>).MinValue(-100000.0f).MaxValue(100000.0f).Delta(1.0f).Value(0.0f)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
				[
						SNew(SHorizontalBox)
						.Visibility_Lambda([this]()
						{
							return GetProjectionPrecisionControlsVisibility();
						})
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
					[
						SNew(STextBlock).Text(LOCTEXT("MapProjRot", "Rotation X/Y/Z"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapProjRotXInput, SSpinBox<float>).MinValue(-360.0f).MaxValue(360.0f).Delta(1.0f).Value(0.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapProjRotYInput, SSpinBox<float>).MinValue(-360.0f).MaxValue(360.0f).Delta(1.0f).Value(0.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SAssignNew(MapProjRotZInput, SSpinBox<float>).MinValue(-360.0f).MaxValue(360.0f).Delta(1.0f).Value(0.0f)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
				[
						SNew(SHorizontalBox)
						.Visibility_Lambda([this]()
						{
							return GetProjectionPrecisionControlsVisibility();
						})
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
					[
						SNew(STextBlock).Text(LOCTEXT("MapProjParams", "FOV / Aspect / Near / Far"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapProjFovInput, SSpinBox<float>).MinValue(1.0f).MaxValue(179.0f).Delta(1.0f).Value(60.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapProjAspectInput, SSpinBox<float>).MinValue(0.1f).MaxValue(10.0f).Delta(0.05f).Value(1.7778f)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapProjNearInput, SSpinBox<float>).MinValue(0.01f).MaxValue(10000.0f).Delta(1.0f).Value(10.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SAssignNew(MapProjFarInput, SSpinBox<float>).MinValue(1.0f).MaxValue(200000.0f).Delta(10.0f).Value(10000.0f)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
				[
						SNew(SHorizontalBox)
						.Visibility_Lambda([this]()
						{
							return (GetProjectionPrecisionControlsVisibility() == EVisibility::Visible
								&& (MapMode == TEXT("cylindrical") || MapMode == TEXT("radial")))
								? EVisibility::Visible : EVisibility::Collapsed;
						})
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
					[
						SNew(STextBlock).Text(LOCTEXT("MapCylLabel", "Cylinder Axis/Radius/Height/Start/End"))
					]
					+ SHorizontalBox::Slot().FillWidth(0.6f).Padding(0,0,4,0)
					[
						SAssignNew(MapCylAxisInput, SEditableTextBox).Text(FText::FromString(TEXT("y")))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapCylRadiusInput, SSpinBox<float>).MinValue(0.01f).MaxValue(100000.0f).Delta(1.0f).Value(100.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapCylHeightInput, SSpinBox<float>).MinValue(0.01f).MaxValue(100000.0f).Delta(1.0f).Value(1000.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapCylStartInput, SSpinBox<float>).MinValue(-360.0f).MaxValue(360.0f).Delta(1.0f).Value(0.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SAssignNew(MapCylEndInput, SSpinBox<float>).MinValue(-360.0f).MaxValue(360.0f).Delta(1.0f).Value(90.0f)
					]
				]
			]

							+ SVerticalBox::Slot().AutoHeight().Padding(0,4,0,2)
							[
								SNew(SVerticalBox)
									.Visibility_Lambda([this]()
									{
										return GetProjectionPrecisionControlsVisibility() == EVisibility::Visible && MapMode == MapModeCustomMatrix
											? EVisibility::Visible : EVisibility::Collapsed;
									})
					+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(LOCTEXT("MapCustomMatrixHeader", "Custom Projection Matrix (4x4)"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapCustomMatrixInputs[0], SSpinBox<float>).MinValue(-1000.0f).MaxValue(1000.0f).Delta(0.01f).Value(1.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapCustomMatrixInputs[1], SSpinBox<float>).MinValue(-1000.0f).MaxValue(1000.0f).Delta(0.01f).Value(0.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapCustomMatrixInputs[2], SSpinBox<float>).MinValue(-1000.0f).MaxValue(1000.0f).Delta(0.01f).Value(0.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SAssignNew(MapCustomMatrixInputs[3], SSpinBox<float>).MinValue(-1000.0f).MaxValue(1000.0f).Delta(0.01f).Value(0.0f)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapCustomMatrixInputs[4], SSpinBox<float>).MinValue(-1000.0f).MaxValue(1000.0f).Delta(0.01f).Value(0.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapCustomMatrixInputs[5], SSpinBox<float>).MinValue(-1000.0f).MaxValue(1000.0f).Delta(0.01f).Value(1.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapCustomMatrixInputs[6], SSpinBox<float>).MinValue(-1000.0f).MaxValue(1000.0f).Delta(0.01f).Value(0.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SAssignNew(MapCustomMatrixInputs[7], SSpinBox<float>).MinValue(-1000.0f).MaxValue(1000.0f).Delta(0.01f).Value(0.0f)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapCustomMatrixInputs[8], SSpinBox<float>).MinValue(-1000.0f).MaxValue(1000.0f).Delta(0.01f).Value(0.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapCustomMatrixInputs[9], SSpinBox<float>).MinValue(-1000.0f).MaxValue(1000.0f).Delta(0.01f).Value(0.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapCustomMatrixInputs[10], SSpinBox<float>).MinValue(-1000.0f).MaxValue(1000.0f).Delta(0.01f).Value(1.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SAssignNew(MapCustomMatrixInputs[11], SSpinBox<float>).MinValue(-1000.0f).MaxValue(1000.0f).Delta(0.01f).Value(0.0f)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapCustomMatrixInputs[12], SSpinBox<float>).MinValue(-1000.0f).MaxValue(1000.0f).Delta(0.01f).Value(0.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapCustomMatrixInputs[13], SSpinBox<float>).MinValue(-1000.0f).MaxValue(1000.0f).Delta(0.01f).Value(0.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapCustomMatrixInputs[14], SSpinBox<float>).MinValue(-1000.0f).MaxValue(1000.0f).Delta(0.01f).Value(0.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SAssignNew(MapCustomMatrixInputs[15], SSpinBox<float>).MinValue(-1000.0f).MaxValue(1000.0f).Delta(0.01f).Value(1.0f)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
				[
					SNew(SButton)
					.Text(LOCTEXT("MapCustomMatrixResetIdentity", "Reset Identity"))
					.OnClicked_Lambda([this]()
					{
						SetCustomProjectionMatrixInputsToIdentity(MapCustomMatrixInputs);
						return FReply::Handled();
					})
				]
			]


			// Parallel-specific: Size W/H
					+ SVerticalBox::Slot().AutoHeight().Padding(0,4,0,2)
					[
						SNew(SVerticalBox)
							.Visibility_Lambda([this]() { return GetProjectionPrecisionControlsVisibility() == EVisibility::Visible && MapMode == TEXT("parallel")
								? EVisibility::Visible : EVisibility::Collapsed; })
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(LOCTEXT("MapParallelHeader", "Parallel Projection Size"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
					[
						SNew(STextBlock).Text(LOCTEXT("MapParallelWH", "Width / Height"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapParallelSizeWInput, SSpinBox<float>).MinValue(0.01f).MaxValue(100000.0f).Delta(10.0f).Value(1000.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SAssignNew(MapParallelSizeHInput, SSpinBox<float>).MinValue(0.01f).MaxValue(100000.0f).Delta(10.0f).Value(1000.0f)
					]
				]
			]

			// Spherical-specific: Radius, HArc, VArc
					+ SVerticalBox::Slot().AutoHeight().Padding(0,4,0,2)
					[
						SNew(SVerticalBox)
							.Visibility_Lambda([this]() { return GetProjectionPrecisionControlsVisibility() == EVisibility::Visible && MapMode == TEXT("spherical")
								? EVisibility::Visible : EVisibility::Collapsed; })
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(LOCTEXT("MapSphHeader", "Spherical"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
					[
						SNew(STextBlock).Text(LOCTEXT("MapSphParams", "Radius / H-Arc / V-Arc"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapSphRadiusInput, SSpinBox<float>).MinValue(0.01f).MaxValue(100000.0f).Delta(10.0f).Value(500.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapSphHArcInput, SSpinBox<float>).MinValue(0.0f).MaxValue(360.0f).Delta(1.0f).Value(360.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SAssignNew(MapSphVArcInput, SSpinBox<float>).MinValue(0.0f).MaxValue(360.0f).Delta(1.0f).Value(180.0f)
					]
				]
			]

			// Mesh-specific: Eyepoint
					+ SVerticalBox::Slot().AutoHeight().Padding(0,4,0,2)
					[
						SNew(SVerticalBox)
							.Visibility_Lambda([this]() { return GetProjectionPrecisionControlsVisibility() == EVisibility::Visible && MapMode == TEXT("mesh")
								? EVisibility::Visible : EVisibility::Collapsed; })
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(LOCTEXT("MapMeshHeader", "Mesh Mapping"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
					[
						SNew(STextBlock).Text(LOCTEXT("MapMeshEye", "Eyepoint X/Y/Z"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapMeshEyeXInput, SSpinBox<float>).MinValue(-100000.0f).MaxValue(100000.0f).Delta(1.0f).Value(0.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SAssignNew(MapMeshEyeYInput, SSpinBox<float>).MinValue(-100000.0f).MaxValue(100000.0f).Delta(1.0f).Value(0.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SAssignNew(MapMeshEyeZInput, SSpinBox<float>).MinValue(-100000.0f).MaxValue(100000.0f).Delta(1.0f).Value(0.0f)
					]
				]
			]

			// Fisheye-specific: FOV, Lens Type
					+ SVerticalBox::Slot().AutoHeight().Padding(0,4,0,2)
					[
						SNew(SVerticalBox)
							.Visibility_Lambda([this]() { return GetProjectionPrecisionControlsVisibility() == EVisibility::Visible && MapMode == TEXT("fisheye")
								? EVisibility::Visible : EVisibility::Collapsed; })
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(LOCTEXT("MapFishHeader", "Fisheye"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
					[
						SNew(STextBlock).Text(LOCTEXT("MapFishFov", "FOV"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
					[
						SAssignNew(MapFisheyeFovInput, SSpinBox<float>).MinValue(1.0f).MaxValue(360.0f).Delta(1.0f).Value(180.0f)
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
					[
						SNew(STextBlock).Text(LOCTEXT("MapFishLens", "Lens Type"))
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SAssignNew(MapFisheyeLensInput, SEditableTextBox).Text(FText::FromString(TEXT("equidistant")))
					]
				]
			]

			// Content mode (UV modes)
			+ SVerticalBox::Slot().AutoHeight().Padding(0,4,0,2)
			[
				SNew(SVerticalBox)
				.Visibility_Lambda([this]() { return (MapMode == TEXT("direct") || MapMode == TEXT("feed")) ? EVisibility::Visible : EVisibility::Collapsed; })
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(LOCTEXT("MapContentModeLabel", "Content Mode"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
				[
					SAssignNew(ContentModeSelector, SRshipContentModeSelector)
					.OnContentModeSelected_Lambda([this](const FString& Mode)
					{
						if (MapContentModeInput.IsValid())
						{
							MapContentModeInput->SetText(FText::FromString(Mode));
						}
					})
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SAssignNew(MapContentModeInput, SEditableTextBox)
					.Text(FText::FromString(TEXT("stretch")))
					.Visibility(EVisibility::Collapsed)
				]
			]

			// Masking (projection modes)
			+ SVerticalBox::Slot().AutoHeight().Padding(0,4,0,2)
			[
				SNew(SVerticalBox)
				.Visibility_Lambda([this]()
				{
					return (MapMode == MapModePerspective || MapMode == MapModeCylindrical || MapMode == MapModeSpherical
						|| MapMode == MapModeCustomMatrix || MapMode == MapModeParallel || MapMode == MapModeRadial || MapMode == MapModeFisheye
						|| MapMode == MapModeCameraPlate || MapMode == MapModeSpatial || MapMode == MapModeDepthMap)
						? EVisibility::Visible : EVisibility::Collapsed;
				})
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(LOCTEXT("MapMaskHeader", "Masking"))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
					[
						SAssignNew(AngleMaskWidget, SRshipAngleMaskWidget)
						.OnAngleMaskChanged_Lambda([this](float StartDeg, float EndDeg)
						{
							if (MapMaskStartInput.IsValid()) MapMaskStartInput->SetValue(StartDeg);
							if (MapMaskEndInput.IsValid()) MapMaskEndInput->SetValue(EndDeg);
						})
					]
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
								[
									SNew(STextBlock).Text(LOCTEXT("MapMaskAngle", "Angle Start/End"))
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
							[
								SAssignNew(MapMaskStartInput, SSpinBox<float>).MinValue(0.0f).MaxValue(360.0f).Delta(1.0f).Value(0.0f)
								.OnValueChanged_Lambda([this](float Val)
								{
									if (AngleMaskWidget.IsValid()) AngleMaskWidget->SetAngles(Val, MapMaskEndInput.IsValid() ? MapMaskEndInput->GetValue() : 360.0f);
								})
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
							[
								SAssignNew(MapMaskEndInput, SSpinBox<float>).MinValue(0.0f).MaxValue(360.0f).Delta(1.0f).Value(360.0f)
								.OnValueChanged_Lambda([this](float Val)
								{
									if (AngleMaskWidget.IsValid()) AngleMaskWidget->SetAngles(MapMaskStartInput.IsValid() ? MapMaskStartInput->GetValue() : 0.0f, Val);
								})
							]
						]
							+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
							[
								SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
							[
								SAssignNew(MapClipOutsideInput, SCheckBox)
								[
									SNew(STextBlock).Text(LOCTEXT("MapClipOutside", "Clip Outside"))
								]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
							[
								SNew(STextBlock).Text(LOCTEXT("MapBorderLabel", "Border Expansion"))
							]
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SAssignNew(MapBorderExpansionInput, SSpinBox<float>).MinValue(0.0f).MaxValue(1.0f).Delta(0.01f).Value(0.0f)
							]
						]
					]
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
					.Text_Lambda([this]() { return SelectedContextId.IsEmpty() ? LOCTEXT("CtxCreate", "Create Input") : LOCTEXT("CtxSave", "Save Input"); })
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
					.Text(LOCTEXT("CtxReset", "New Input"))
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
				SNew(STextBlock).Text(LOCTEXT("SurfFormTitle", "Screen")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
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
					SNew(STextBlock).Text(LOCTEXT("SurfTarget", "Screen Target"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SAssignNew(SurfTargetInput, SEditableTextBox)
					.HintText(LOCTEXT("SurfTargetHint", "Pick or type screen target"))
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
				+ SHorizontalBox::Slot().AutoWidth().Padding(4,0,0,0)
				[
					SNew(SButton)
					.Text(LOCTEXT("SurfUseSelected", "Use Selected"))
					.OnClicked_Lambda([this]()
					{
#if WITH_EDITOR
						const bool bOk = TryApplySelectionToTarget(SurfTargetInput, false);
						if (bOk && SurfMeshInput.IsValid() && GEditor)
						{
							if (USelection* Selection = GEditor->GetSelectedActors())
							{
								for (FSelectionIterator It(*Selection); It; ++It)
								{
									if (AActor* Actor = Cast<AActor>(*It))
									{
										TArray<UMeshComponent*> MeshComponents;
										Actor->GetComponents(MeshComponents);
										if (MeshComponents.Num() > 0 && MeshComponents[0])
										{
											SurfMeshInput->SetText(FText::FromString(MeshComponents[0]->GetName()));
											break;
										}
									}
								}
							}
						}
#endif
						return FReply::Handled();
					})
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
					SNew(STextBlock).Text(LOCTEXT("SurfSlots", "Material Slots"))
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
					SNew(STextBlock).Text(LOCTEXT("SurfMesh", "Mesh Component (optional)"))
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
					.Text_Lambda([this]() { return SelectedSurfaceId.IsEmpty() ? LOCTEXT("SurfCreate", "Create Screen") : LOCTEXT("SurfSave", "Save Screen"); })
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
					.Text(LOCTEXT("SurfReset", "New Screen"))
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
				SNew(STextBlock).Text(LOCTEXT("MapFormTitle", "Mapping")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
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
					SNew(STextBlock).Text(LOCTEXT("MapContext", "Input"))
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
					SNew(STextBlock).Text(LOCTEXT("MapSurfaces", "Screens (comma)"))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SAssignNew(MapSurfacesInput, SEditableTextBox)
					.OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type)
					{
						RebuildFeedRectList();
					})
					.OnTextChanged_Lambda([this](const FText&)
					{
						RebuildFeedRectList();
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(6,0,0,0)
				[
					SNew(SComboButton)
					.OnGetMenuContent_Lambda([this]()
					{
						return BuildIdPickerMenu(SurfaceOptions, LOCTEXT("MapNoSurfaces", "No screens found"), MapSurfacesInput, true);
					})
					.ButtonContent()
					[
						SNew(STextBlock).Text(LOCTEXT("MapAddSurface", "Add Screen"))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4,0,0,0)
				[
					SNew(SButton)
					.Text(LOCTEXT("MapClearSurfaces", "Clear Screens"))
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
							const FString NormalizedMode = NormalizeMapMode(MapMode, MapModeDirect);
							const bool bUvMode = (NormalizedMode == MapModeDirect || NormalizedMode == MapModeFeed);
							State.Type = bUvMode ? TEXT("surface-uv") : TEXT("surface-projection");
							State.ContextId = MapContextInput.IsValid() ? MapContextInput->GetText().ToString() : TEXT("");
							State.Opacity = MapOpacityInput.IsValid() ? MapOpacityInput->GetValue() : 1.0f;
							State.bEnabled = !MapEnabledInput.IsValid() || MapEnabledInput->IsChecked();

							if (MapSurfacesInput.IsValid())
							{
								TArray<FString> Parts;
								MapSurfacesInput->GetText().ToString().ParseIntoArray(Parts, TEXT(","), true);
								for (FString& Part : Parts)
								{
									Part = Part.TrimStartAndEnd();
								}
								Parts.RemoveAll([](const FString& Part) { return Part.IsEmpty(); });
								State.SurfaceIds = Parts;
							}

							// Build config
							TSharedPtr<FJsonObject> Config = MakeShared<FJsonObject>();
							if (bUvMode)
							{
								Config->SetStringField(TEXT("uvMode"), (NormalizedMode == MapModeFeed) ? MapModeFeed : MapModeDirect);
								TSharedPtr<FJsonObject> Uv = MakeShared<FJsonObject>();
								Uv->SetNumberField(TEXT("scaleU"), MapUvScaleUInput.IsValid() ? MapUvScaleUInput->GetValue() : 1.0);
								Uv->SetNumberField(TEXT("scaleV"), MapUvScaleVInput.IsValid() ? MapUvScaleVInput->GetValue() : 1.0);
								Uv->SetNumberField(TEXT("offsetU"), MapUvOffsetUInput.IsValid() ? MapUvOffsetUInput->GetValue() : 0.0);
								Uv->SetNumberField(TEXT("offsetV"), MapUvOffsetVInput.IsValid() ? MapUvOffsetVInput->GetValue() : 0.0);
								Uv->SetNumberField(TEXT("rotationDeg"), MapUvRotInput.IsValid() ? MapUvRotInput->GetValue() : 0.0);
								Config->SetObjectField(TEXT("uvTransform"), Uv);

								if (NormalizedMode == MapModeFeed)
								{
									TSharedPtr<FJsonObject> Feed = MakeShared<FJsonObject>();
									Feed->SetNumberField(TEXT("u"), MapFeedUInput.IsValid() ? MapFeedUInput->GetValue() : 0.0);
									Feed->SetNumberField(TEXT("v"), MapFeedVInput.IsValid() ? MapFeedVInput->GetValue() : 0.0);
									Feed->SetNumberField(TEXT("width"), MapFeedWInput.IsValid() ? MapFeedWInput->GetValue() : 1.0);
									Feed->SetNumberField(TEXT("height"), MapFeedHInput.IsValid() ? MapFeedHInput->GetValue() : 1.0);
									Config->SetObjectField(TEXT("feedRect"), Feed);

									if (State.SurfaceIds.Num() > 0)
									{
										TArray<TSharedPtr<FJsonValue>> Rects;
										for (const FString& SurfaceId : State.SurfaceIds)
										{
											if (const FFeedRect* Rect = MapFeedRectOverrides.Find(SurfaceId))
											{
												TSharedPtr<FJsonObject> RectObj = MakeShared<FJsonObject>();
												RectObj->SetStringField(TEXT("surfaceId"), SurfaceId);
												RectObj->SetNumberField(TEXT("u"), Rect->U);
												RectObj->SetNumberField(TEXT("v"), Rect->V);
												RectObj->SetNumberField(TEXT("width"), Rect->W);
												RectObj->SetNumberField(TEXT("height"), Rect->H);
												Rects.Add(MakeShared<FJsonValueObject>(RectObj));
											}
										}
										if (Rects.Num() > 0)
										{
											Config->SetArrayField(TEXT("feedRects"), Rects);
										}
									}
								}
							}
							else
							{
								Config->SetStringField(TEXT("projectionType"), NormalizedMode);

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
								if (NormalizedMode == MapModeCustomMatrix)
								{
									Config->SetObjectField(TEXT("customProjectionMatrix"), BuildCustomProjectionMatrixObject(MapCustomMatrixInputs));
								}
								else
								{
									Config->RemoveField(TEXT("customProjectionMatrix"));
									Config->RemoveField(TEXT("matrix"));
								}

								const FString Axis = MapCylAxisInput.IsValid() ? MapCylAxisInput->GetText().ToString() : TEXT("");
								if ((NormalizedMode == MapModeCylindrical || NormalizedMode == MapModeRadial) && !Axis.IsEmpty())
								{
									TSharedPtr<FJsonObject> Cyl = MakeShared<FJsonObject>();
									Cyl->SetStringField(TEXT("axis"), Axis);
									Cyl->SetNumberField(TEXT("radius"), MapCylRadiusInput.IsValid() ? MapCylRadiusInput->GetValue() : 100.0);
									Cyl->SetNumberField(TEXT("height"), MapCylHeightInput.IsValid() ? MapCylHeightInput->GetValue() : 1000.0);
									Cyl->SetNumberField(TEXT("startAngle"), MapCylStartInput.IsValid() ? MapCylStartInput->GetValue() : 0.0);
									Cyl->SetNumberField(TEXT("endAngle"), MapCylEndInput.IsValid() ? MapCylEndInput->GetValue() : 90.0);
									Config->SetObjectField(TEXT("cylindrical"), Cyl);
								}

								if (NormalizedMode == MapModeParallel)
								{
									Config->SetNumberField(TEXT("sizeW"), MapParallelSizeWInput.IsValid() ? MapParallelSizeWInput->GetValue() : 1000.0);
									Config->SetNumberField(TEXT("sizeH"), MapParallelSizeHInput.IsValid() ? MapParallelSizeHInput->GetValue() : 1000.0);
								}

								if (NormalizedMode == MapModeSpherical)
								{
									Config->SetNumberField(TEXT("sphereRadius"), MapSphRadiusInput.IsValid() ? MapSphRadiusInput->GetValue() : 500.0);
									Config->SetNumberField(TEXT("horizontalArc"), MapSphHArcInput.IsValid() ? MapSphHArcInput->GetValue() : 360.0);
									Config->SetNumberField(TEXT("verticalArc"), MapSphVArcInput.IsValid() ? MapSphVArcInput->GetValue() : 180.0);
								}

								if (NormalizedMode == MapModeMesh)
								{
									TSharedPtr<FJsonObject> EpObj = MakeShared<FJsonObject>();
									EpObj->SetNumberField(TEXT("x"), MapMeshEyeXInput.IsValid() ? MapMeshEyeXInput->GetValue() : 0.0);
									EpObj->SetNumberField(TEXT("y"), MapMeshEyeYInput.IsValid() ? MapMeshEyeYInput->GetValue() : 0.0);
									EpObj->SetNumberField(TEXT("z"), MapMeshEyeZInput.IsValid() ? MapMeshEyeZInput->GetValue() : 0.0);
									Config->SetObjectField(TEXT("eyepoint"), EpObj);
								}

								if (NormalizedMode == MapModeFisheye)
								{
									Config->SetNumberField(TEXT("fisheyeFov"), MapFisheyeFovInput.IsValid() ? MapFisheyeFovInput->GetValue() : 180.0);
									Config->SetStringField(TEXT("lensType"), MapFisheyeLensInput.IsValid() ? MapFisheyeLensInput->GetText().ToString() : TEXT("equidistant"));
								}

								// Masking and border expansion (all projection modes)
								Config->SetNumberField(TEXT("angleMaskStart"), MapMaskStartInput.IsValid() ? MapMaskStartInput->GetValue() : 0.0);
								Config->SetNumberField(TEXT("angleMaskEnd"), MapMaskEndInput.IsValid() ? MapMaskEndInput->GetValue() : 360.0);
								Config->SetBoolField(TEXT("clipOutsideRegion"), MapClipOutsideInput.IsValid() && MapClipOutsideInput->IsChecked());
								Config->SetNumberField(TEXT("borderExpansion"), MapBorderExpansionInput.IsValid() ? MapBorderExpansionInput->GetValue() : 0.0);
							}

							// Content mode for UV mappings
							if (bUvMode)
							{
								const FString ContentModeStr = MapContentModeInput.IsValid() ? MapContentModeInput->GetText().ToString().TrimStartAndEnd() : TEXT("stretch");
								if (!ContentModeStr.IsEmpty() && !ContentModeStr.Equals(TEXT("stretch"), ESearchCase::IgnoreCase))
								{
									Config->SetStringField(TEXT("contentMode"), ContentModeStr);
								}
							}

							State.Config = Config;

								if (State.Id.IsEmpty())
								{
									SetSelectedMappingId(Manager->CreateMapping(State));
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
						.Text(LOCTEXT("MapReset", "New Mapping"))
						.OnClicked_Lambda([this]()
						{
							ClearSelectedMappingId();
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
	QuickMapMode = TEXT("direct");
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
	if (QuickFeedUInput.IsValid()) QuickFeedUInput->SetValue(0.0f);
	if (QuickFeedVInput.IsValid()) QuickFeedVInput->SetValue(0.0f);
	if (QuickFeedWInput.IsValid()) QuickFeedWInput->SetValue(1.0f);
	if (QuickFeedHInput.IsValid()) QuickFeedHInput->SetValue(1.0f);

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
	MapMode = TEXT("direct");
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
	if (MapParallelSizeWInput.IsValid()) MapParallelSizeWInput->SetValue(1000.f);
	if (MapParallelSizeHInput.IsValid()) MapParallelSizeHInput->SetValue(1000.f);
	if (MapSphRadiusInput.IsValid()) MapSphRadiusInput->SetValue(500.f);
	if (MapSphHArcInput.IsValid()) MapSphHArcInput->SetValue(360.f);
	if (MapSphVArcInput.IsValid()) MapSphVArcInput->SetValue(180.f);
	if (MapFisheyeFovInput.IsValid()) MapFisheyeFovInput->SetValue(180.f);
	if (MapFisheyeLensInput.IsValid()) MapFisheyeLensInput->SetText(FText::FromString(TEXT("equidistant")));
	if (MapMeshEyeXInput.IsValid()) MapMeshEyeXInput->SetValue(0.f);
	if (MapMeshEyeYInput.IsValid()) MapMeshEyeYInput->SetValue(0.f);
	if (MapMeshEyeZInput.IsValid()) MapMeshEyeZInput->SetValue(0.f);
	if (MapContentModeInput.IsValid()) MapContentModeInput->SetText(FText::FromString(TEXT("stretch")));
	if (MapMaskStartInput.IsValid()) MapMaskStartInput->SetValue(0.f);
	if (MapMaskEndInput.IsValid()) MapMaskEndInput->SetValue(360.f);
	if (MapClipOutsideInput.IsValid()) MapClipOutsideInput->SetIsChecked(ECheckBoxState::Unchecked);
	if (MapBorderExpansionInput.IsValid()) MapBorderExpansionInput->SetValue(0.f);
	if (MapUvScaleUInput.IsValid()) MapUvScaleUInput->SetValue(1.f);
	if (MapUvScaleVInput.IsValid()) MapUvScaleVInput->SetValue(1.f);
	if (MapUvOffsetUInput.IsValid()) MapUvOffsetUInput->SetValue(0.f);
	if (MapUvOffsetVInput.IsValid()) MapUvOffsetVInput->SetValue(0.f);
	if (MapUvRotInput.IsValid()) MapUvRotInput->SetValue(0.f);
	if (MapFeedUInput.IsValid()) MapFeedUInput->SetValue(0.f);
	if (MapFeedVInput.IsValid()) MapFeedVInput->SetValue(0.f);
	if (MapFeedWInput.IsValid()) MapFeedWInput->SetValue(1.f);
	if (MapFeedHInput.IsValid()) MapFeedHInput->SetValue(1.f);
	SetCustomProjectionMatrixInputsToIdentity(MapCustomMatrixInputs);
	MapFeedRectOverrides.Empty();
	RebuildFeedRectList();

	// Reset graphical widgets
	if (QuickModeSelector.IsValid()) QuickModeSelector->SetSelectedMode(TEXT("direct"));
	if (MapModeSelector.IsValid()) MapModeSelector->SetSelectedMode(TEXT("direct"));
	if (MappingCanvas.IsValid())
	{
		MappingCanvas->SetFeedRect(0.0f, 0.0f, 1.0f, 1.0f);
		MappingCanvas->SetUvTransform(1.0f, 1.0f, 0.0f, 0.0f, 0.0f);
		MappingCanvas->SetDisplayMode(TEXT("direct"));
		MappingCanvas->SetBackgroundTexture(nullptr);
	}
	if (AngleMaskWidget.IsValid()) AngleMaskWidget->SetAngles(0.0f, 360.0f);
	if (ContentModeSelector.IsValid()) ContentModeSelector->SetSelectedMode(TEXT("stretch"));
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
	SetSelectedMappingId(State.Id);
	if (MapNameInput.IsValid()) MapNameInput->SetText(FText::FromString(State.Name));
	if (MapProjectInput.IsValid()) MapProjectInput->SetText(FText::FromString(State.ProjectId));
	if (MapContextInput.IsValid()) MapContextInput->SetText(FText::FromString(State.ContextId));
	if (MapSurfacesInput.IsValid())
	{
		FString Surfaces = FString::Join(State.SurfaceIds, TEXT(","));
		MapSurfacesInput->SetText(FText::FromString(Surfaces));
	}
	if (MapOpacityInput.IsValid()) MapOpacityInput->SetValue(State.Opacity);
	if (MapEnabledInput.IsValid()) MapEnabledInput->SetIsChecked(State.bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);

	MapMode = GetMappingModeFromState(State);
	SetCustomProjectionMatrixInputsToIdentity(MapCustomMatrixInputs);
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
		auto GetNum = [](const TSharedPtr<FJsonObject>& Obj, const FString& Field, double DefaultVal)->double
		{
			return (Obj.IsValid() && Obj->HasTypedField<EJson::Number>(Field)) ? Obj->GetNumberField(Field) : DefaultVal;
		};
		if (State.Type == TEXT("surface-uv"))
		{
			if (State.Config->HasTypedField<EJson::Object>(TEXT("feedRect")))
			{
				TSharedPtr<FJsonObject> Feed = State.Config->GetObjectField(TEXT("feedRect"));
				if (MapFeedUInput.IsValid()) MapFeedUInput->SetValue(GetNum(Feed, TEXT("u"), 0.0));
				if (MapFeedVInput.IsValid()) MapFeedVInput->SetValue(GetNum(Feed, TEXT("v"), 0.0));
				if (MapFeedWInput.IsValid()) MapFeedWInput->SetValue(GetNum(Feed, TEXT("width"), 1.0));
				if (MapFeedHInput.IsValid()) MapFeedHInput->SetValue(GetNum(Feed, TEXT("height"), 1.0));
			}
			MapFeedRectOverrides.Empty();
			if (State.Config->HasTypedField<EJson::Array>(TEXT("feedRects")))
			{
				const TArray<TSharedPtr<FJsonValue>> Rects = State.Config->GetArrayField(TEXT("feedRects"));
				for (const TSharedPtr<FJsonValue>& Value : Rects)
				{
					if (!Value.IsValid() || Value->Type != EJson::Object)
					{
						continue;
					}
					TSharedPtr<FJsonObject> RectObj = Value->AsObject();
					if (!RectObj.IsValid() || !RectObj->HasTypedField<EJson::String>(TEXT("surfaceId")))
					{
						continue;
					}
					const FString SurfaceId = RectObj->GetStringField(TEXT("surfaceId"));
					FFeedRect Rect;
					Rect.U = GetNum(RectObj, TEXT("u"), 0.0);
					Rect.V = GetNum(RectObj, TEXT("v"), 0.0);
					Rect.W = GetNum(RectObj, TEXT("width"), 1.0);
					Rect.H = GetNum(RectObj, TEXT("height"), 1.0);
					MapFeedRectOverrides.Add(SurfaceId, Rect);
				}
			}
		}
		else if (State.Type == TEXT("surface-projection"))
		{
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

			// Parallel
			if (MapParallelSizeWInput.IsValid()) MapParallelSizeWInput->SetValue(GetNum(State.Config, TEXT("sizeW"), 1000.0));
			if (MapParallelSizeHInput.IsValid()) MapParallelSizeHInput->SetValue(GetNum(State.Config, TEXT("sizeH"), 1000.0));

			// Spherical
			if (MapSphRadiusInput.IsValid()) MapSphRadiusInput->SetValue(GetNum(State.Config, TEXT("sphereRadius"), 500.0));
			if (MapSphHArcInput.IsValid()) MapSphHArcInput->SetValue(GetNum(State.Config, TEXT("horizontalArc"), 360.0));
			if (MapSphVArcInput.IsValid()) MapSphVArcInput->SetValue(GetNum(State.Config, TEXT("verticalArc"), 180.0));

			// Mesh
			if (State.Config->HasTypedField<EJson::Object>(TEXT("eyepoint")))
			{
				TSharedPtr<FJsonObject> Ep = State.Config->GetObjectField(TEXT("eyepoint"));
				if (MapMeshEyeXInput.IsValid()) MapMeshEyeXInput->SetValue(GetNum(Ep, TEXT("x"), 0.0));
				if (MapMeshEyeYInput.IsValid()) MapMeshEyeYInput->SetValue(GetNum(Ep, TEXT("y"), 0.0));
				if (MapMeshEyeZInput.IsValid()) MapMeshEyeZInput->SetValue(GetNum(Ep, TEXT("z"), 0.0));
			}

			// Fisheye
			if (MapFisheyeFovInput.IsValid()) MapFisheyeFovInput->SetValue(GetNum(State.Config, TEXT("fisheyeFov"), 180.0));
			if (MapFisheyeLensInput.IsValid())
			{
				const FString LensStr = (State.Config->HasTypedField<EJson::String>(TEXT("lensType")))
					? State.Config->GetStringField(TEXT("lensType")) : TEXT("equidistant");
				MapFisheyeLensInput->SetText(FText::FromString(LensStr));
			}

			// Masking
			if (MapMaskStartInput.IsValid()) MapMaskStartInput->SetValue(GetNum(State.Config, TEXT("angleMaskStart"), 0.0));
			if (MapMaskEndInput.IsValid()) MapMaskEndInput->SetValue(GetNum(State.Config, TEXT("angleMaskEnd"), 360.0));
			if (MapClipOutsideInput.IsValid())
			{
				bool bClip = State.Config->HasTypedField<EJson::Boolean>(TEXT("clipOutsideRegion"))
					? State.Config->GetBoolField(TEXT("clipOutsideRegion")) : false;
				MapClipOutsideInput->SetIsChecked(bClip ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
			}
			if (MapBorderExpansionInput.IsValid()) MapBorderExpansionInput->SetValue(GetNum(State.Config, TEXT("borderExpansion"), 0.0));
			PopulateCustomProjectionMatrixInputs(MapCustomMatrixInputs, State.Config);
		}

		// Content mode
		if (State.Config->HasTypedField<EJson::String>(TEXT("contentMode")))
		{
			if (MapContentModeInput.IsValid()) MapContentModeInput->SetText(FText::FromString(State.Config->GetStringField(TEXT("contentMode"))));
		}
		else
		{
			if (MapContentModeInput.IsValid()) MapContentModeInput->SetText(FText::FromString(TEXT("stretch")));
		}
	}
	RebuildFeedRectList();

	// Sync graphical widgets
	if (MapModeSelector.IsValid()) MapModeSelector->SetSelectedMode(MapMode);
	if (MappingCanvas.IsValid())
	{
		MappingCanvas->SetDisplayMode(MapMode);
		MappingCanvas->SetFeedRect(
			MapFeedUInput.IsValid() ? MapFeedUInput->GetValue() : 0.0f,
			MapFeedVInput.IsValid() ? MapFeedVInput->GetValue() : 0.0f,
			MapFeedWInput.IsValid() ? MapFeedWInput->GetValue() : 1.0f,
			MapFeedHInput.IsValid() ? MapFeedHInput->GetValue() : 1.0f);
		MappingCanvas->SetUvTransform(
			MapUvScaleUInput.IsValid() ? MapUvScaleUInput->GetValue() : 1.0f,
			MapUvScaleVInput.IsValid() ? MapUvScaleVInput->GetValue() : 1.0f,
			MapUvOffsetUInput.IsValid() ? MapUvOffsetUInput->GetValue() : 0.0f,
			MapUvOffsetVInput.IsValid() ? MapUvOffsetVInput->GetValue() : 0.0f,
			MapUvRotInput.IsValid() ? MapUvRotInput->GetValue() : 0.0f);
	}
	if (AngleMaskWidget.IsValid())
	{
		AngleMaskWidget->SetAngles(
			MapMaskStartInput.IsValid() ? MapMaskStartInput->GetValue() : 0.0f,
			MapMaskEndInput.IsValid() ? MapMaskEndInput->GetValue() : 360.0f);
	}
	if (ContentModeSelector.IsValid())
	{
		const FString ContentMode = MapContentModeInput.IsValid() ? MapContentModeInput->GetText().ToString() : TEXT("stretch");
		ContentModeSelector->SetSelectedMode(ContentMode);
	}
}

void SRshipContentMappingPanel::RebuildFeedRectList()
{
	if (!MapFeedRectList.IsValid())
	{
		return;
	}

	MapFeedRectList->ClearChildren();

	if (MapMode != TEXT("feed"))
	{
		return;
	}

	TArray<FString> SurfaceIds;
	if (MapSurfacesInput.IsValid())
	{
		MapSurfacesInput->GetText().ToString().ParseIntoArray(SurfaceIds, TEXT(","), true);
		for (FString& SurfaceId : SurfaceIds)
		{
			SurfaceId = SurfaceId.TrimStartAndEnd();
		}
		SurfaceIds.RemoveAll([](const FString& SurfaceId) { return SurfaceId.IsEmpty(); });
	}

	if (SurfaceIds.Num() == 0)
	{
		MapFeedRectList->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock).Text(LOCTEXT("FeedRectEmpty", "Add screens to edit feed rectangles."))
		];
		return;
	}

	auto DefaultRect = [this]()
	{
		FFeedRect Rect;
		Rect.U = MapFeedUInput.IsValid() ? MapFeedUInput->GetValue() : 0.0f;
		Rect.V = MapFeedVInput.IsValid() ? MapFeedVInput->GetValue() : 0.0f;
		Rect.W = MapFeedWInput.IsValid() ? MapFeedWInput->GetValue() : 1.0f;
		Rect.H = MapFeedHInput.IsValid() ? MapFeedHInput->GetValue() : 1.0f;
		return Rect;
	};

	const FFeedRect Default = DefaultRect();

	for (const FString& SurfaceId : SurfaceIds)
	{
		const bool bHadRect = MapFeedRectOverrides.Contains(SurfaceId);
		FFeedRect& Rect = MapFeedRectOverrides.FindOrAdd(SurfaceId);
		if (!bHadRect)
		{
			Rect = Default;
		}

		FString Label = SurfaceId;
		for (const TSharedPtr<FRshipIdOption>& Option : SurfaceOptions)
		{
			if (Option.IsValid() && Option->Id == SurfaceId)
			{
				Label = Option->Label;
				break;
			}
		}

		MapFeedRectList->AddSlot()
		.AutoHeight()
		.Padding(0, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.6f).VAlign(VAlign_Center).Padding(0,0,6,0)
			[
				SNew(STextBlock).Text(FText::FromString(Label))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
			[
				SNew(SSpinBox<float>)
				.MinValue(-10.0f).MaxValue(10.0f).Delta(0.01f)
				.Value_Lambda([this, SurfaceId]() -> float
				{
					if (const FFeedRect* Found = MapFeedRectOverrides.Find(SurfaceId))
					{
						return Found->U;
					}
					return 0.0f;
				})
				.OnValueChanged_Lambda([this, SurfaceId](float NewValue)
				{
					FFeedRect& LocalRect = MapFeedRectOverrides.FindOrAdd(SurfaceId);
					LocalRect.U = NewValue;
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
			[
				SNew(SSpinBox<float>)
				.MinValue(-10.0f).MaxValue(10.0f).Delta(0.01f)
				.Value_Lambda([this, SurfaceId]() -> float
				{
					if (const FFeedRect* Found = MapFeedRectOverrides.Find(SurfaceId))
					{
						return Found->V;
					}
					return 0.0f;
				})
				.OnValueChanged_Lambda([this, SurfaceId](float NewValue)
				{
					FFeedRect& LocalRect = MapFeedRectOverrides.FindOrAdd(SurfaceId);
					LocalRect.V = NewValue;
				})
			]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
				[
					SNew(SSpinBox<float>)
					.MinValue(0.001f).MaxValue(10.0f).Delta(0.01f)
				.Value_Lambda([this, SurfaceId]() -> float
				{
					if (const FFeedRect* Found = MapFeedRectOverrides.Find(SurfaceId))
					{
						return Found->W;
					}
					return 1.0f;
				})
				.OnValueChanged_Lambda([this, SurfaceId](float NewValue)
				{
					FFeedRect& LocalRect = MapFeedRectOverrides.FindOrAdd(SurfaceId);
					LocalRect.W = NewValue;
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
			[
				SNew(SSpinBox<float>)
				.MinValue(0.001f).MaxValue(10.0f).Delta(0.01f)
				.Value_Lambda([this, SurfaceId]() -> float
				{
					if (const FFeedRect* Found = MapFeedRectOverrides.Find(SurfaceId))
					{
						return Found->H;
					}
					return 1.0f;
				})
				.OnValueChanged_Lambda([this, SurfaceId](float NewValue)
				{
						FFeedRect& LocalRect = MapFeedRectOverrides.FindOrAdd(SurfaceId);
						LocalRect.H = NewValue;
					})
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("FeedRectResetRow", "Reset"))
					.OnClicked_Lambda([this, SurfaceId]()
					{
						MapFeedRectOverrides.Remove(SurfaceId);
						RebuildFeedRectList();
						return FReply::Handled();
					})
				]
			];
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
			ContextList->AddSlot()[SNew(STextBlock).Text(LOCTEXT("ContextsDisabled", "No inputs (disabled)"))];
		}
		if (SurfaceList.IsValid())
		{
			SurfaceList->ClearChildren();
			SurfaceList->AddSlot()[SNew(STextBlock).Text(LOCTEXT("SurfacesDisabled", "No screens (disabled)"))];
		}
		if (MappingList.IsValid())
		{
			MappingList->ClearChildren();
			MappingList->AddSlot()[SNew(STextBlock).Text(LOCTEXT("MappingsDisabled", "No mappings (disabled)"))];
		}
		return;
	}

	bCoveragePreviewEnabled = Manager->IsCoveragePreviewEnabled();

	const TArray<FRshipRenderContextState> Contexts = Manager->GetRenderContexts();
	const TArray<FRshipMappingSurfaceState> Surfaces = Manager->GetMappingSurfaces();
	const TArray<FRshipContentMappingState> Mappings = Manager->GetMappings();
	RebuildPickerOptions(Contexts, Surfaces);

	auto HashString = [](uint32& Hash, const FString& Value)
	{
		Hash = HashCombineFast(Hash, GetTypeHash(Value));
	};
	auto HashInt = [](uint32& Hash, int32 Value)
	{
		Hash = HashCombineFast(Hash, GetTypeHash(Value));
	};
	auto HashFloat = [](uint32& Hash, float Value)
	{
		Hash = HashCombineFast(Hash, GetTypeHash(Value));
	};
	auto HashBool = [](uint32& Hash, bool Value)
	{
		Hash = HashCombineFast(Hash, GetTypeHash(Value));
	};
	auto GetNumField = [](const TSharedPtr<FJsonObject>& Obj, const FString& Field, float DefaultValue) -> float
	{
		return (Obj.IsValid() && Obj->HasTypedField<EJson::Number>(Field)) ? static_cast<float>(Obj->GetNumberField(Field)) : DefaultValue;
	};

	uint32 SnapshotHash = 0;
	HashString(SnapshotHash, ContextFilterText);
	HashString(SnapshotHash, SurfaceFilterText);
	HashString(SnapshotHash, MappingFilterText);
	HashBool(SnapshotHash, bContextErrorsOnly);
	HashBool(SnapshotHash, bSurfaceErrorsOnly);
	HashBool(SnapshotHash, bMappingErrorsOnly);

	for (const FRshipRenderContextState& Context : Contexts)
	{
		HashString(SnapshotHash, Context.Id);
		HashString(SnapshotHash, Context.Name);
		HashString(SnapshotHash, Context.ProjectId);
		HashString(SnapshotHash, Context.SourceType);
		HashString(SnapshotHash, Context.CameraId);
		HashString(SnapshotHash, Context.AssetId);
		HashString(SnapshotHash, Context.CaptureMode);
		HashString(SnapshotHash, Context.LastError);
		HashInt(SnapshotHash, Context.Width);
		HashInt(SnapshotHash, Context.Height);
		HashBool(SnapshotHash, Context.bEnabled);
	}

	for (const FRshipMappingSurfaceState& Surface : Surfaces)
	{
		HashString(SnapshotHash, Surface.Id);
		HashString(SnapshotHash, Surface.Name);
		HashString(SnapshotHash, Surface.ProjectId);
		HashString(SnapshotHash, Surface.TargetId);
		HashString(SnapshotHash, Surface.MeshComponentName);
		HashString(SnapshotHash, Surface.LastError);
		HashInt(SnapshotHash, Surface.UVChannel);
		HashBool(SnapshotHash, Surface.bEnabled);
		TArray<int32> Slots = Surface.MaterialSlots;
		Slots.Sort();
		for (int32 Slot : Slots)
		{
			HashInt(SnapshotHash, Slot);
		}
	}

	for (const FRshipContentMappingState& Mapping : Mappings)
	{
		HashString(SnapshotHash, Mapping.Id);
		HashString(SnapshotHash, Mapping.Name);
		HashString(SnapshotHash, Mapping.ProjectId);
		HashString(SnapshotHash, Mapping.Type);
		HashString(SnapshotHash, Mapping.ContextId);
		HashString(SnapshotHash, Mapping.LastError);
		HashBool(SnapshotHash, Mapping.bEnabled);
		HashFloat(SnapshotHash, Mapping.Opacity);
		if (Mapping.Config.IsValid())
		{
			if (Mapping.Config->HasTypedField<EJson::String>(TEXT("projectionType")))
			{
				HashString(SnapshotHash, Mapping.Config->GetStringField(TEXT("projectionType")));
			}
			if (Mapping.Config->HasTypedField<EJson::String>(TEXT("uvMode")))
			{
				HashString(SnapshotHash, Mapping.Config->GetStringField(TEXT("uvMode")));
			}
			if (Mapping.Config->HasTypedField<EJson::Object>(TEXT("feedRect")))
			{
				const TSharedPtr<FJsonObject> FeedRect = Mapping.Config->GetObjectField(TEXT("feedRect"));
				HashFloat(SnapshotHash, GetNumField(FeedRect, TEXT("u"), 0.0f));
				HashFloat(SnapshotHash, GetNumField(FeedRect, TEXT("v"), 0.0f));
				HashFloat(SnapshotHash, GetNumField(FeedRect, TEXT("width"), 1.0f));
				HashFloat(SnapshotHash, GetNumField(FeedRect, TEXT("height"), 1.0f));
			}
			const TSharedPtr<FJsonObject> MatrixObj = GetCustomProjectionMatrixObject(Mapping.Config);
			if (MatrixObj.IsValid())
			{
				for (int32 Index = 0; Index < CustomProjectionMatrixElementCount; ++Index)
				{
					const FString FieldName = GetCustomProjectionMatrixFieldName(Index);
					HashFloat(SnapshotHash, GetNumField(MatrixObj, *FieldName, GetDefaultCustomProjectionMatrixValue(Index)));
				}
			}
		}
		TArray<FString> SurfaceIds = Mapping.SurfaceIds;
		SurfaceIds.Sort();
		for (const FString& SurfaceId : SurfaceIds)
		{
			HashString(SnapshotHash, SurfaceId);
		}
	}

	bool bRebuildLists = false;
	if (!bHasListHash)
	{
		LastListHash = SnapshotHash;
		bHasListHash = true;
		bHasPendingListHash = false;
		bRebuildLists = true;
	}
	else if (SnapshotHash != LastListHash)
	{
		if (bHasPendingListHash && PendingListHash == SnapshotHash)
		{
			LastListHash = SnapshotHash;
			bHasPendingListHash = false;
			bRebuildLists = true;
		}
		else
		{
			PendingListHash = SnapshotHash;
			bHasPendingListHash = true;
			bRebuildLists = false;
		}
	}
	else
	{
		bHasPendingListHash = false;
	}

	if (!bRebuildLists)
	{
		return;
	}

	TArray<FRshipRenderContextState> SortedContexts = Contexts;
	TArray<FRshipMappingSurfaceState> SortedSurfaces = Surfaces;
	TArray<FRshipContentMappingState> SortedMappings = Mappings;
	SortedContexts.Sort([](const FRshipRenderContextState& A, const FRshipRenderContextState& B)
	{
		const FString ADisplay = A.Name.IsEmpty() ? A.Id : A.Name;
		const FString BDisplay = B.Name.IsEmpty() ? B.Id : B.Name;
		if (!ADisplay.Equals(BDisplay, ESearchCase::IgnoreCase))
		{
			return ADisplay < BDisplay;
		}
		return A.Id < B.Id;
	});
	SortedSurfaces.Sort([](const FRshipMappingSurfaceState& A, const FRshipMappingSurfaceState& B)
	{
		const FString ADisplay = A.Name.IsEmpty() ? A.Id : A.Name;
		const FString BDisplay = B.Name.IsEmpty() ? B.Id : B.Name;
		if (!ADisplay.Equals(BDisplay, ESearchCase::IgnoreCase))
		{
			return ADisplay < BDisplay;
		}
		return A.Id < B.Id;
	});
	SortedMappings.Sort([](const FRshipContentMappingState& A, const FRshipContentMappingState& B)
	{
		const FString ADisplay = A.Name.IsEmpty() ? A.Id : A.Name;
		const FString BDisplay = B.Name.IsEmpty() ? B.Id : B.Name;
		if (!ADisplay.Equals(BDisplay, ESearchCase::IgnoreCase))
		{
			return ADisplay < BDisplay;
		}
		return A.Id < B.Id;
	});

	TSet<FString> ValidContextIds;
	for (const FRshipRenderContextState& Context : SortedContexts)
	{
		ValidContextIds.Add(Context.Id);
	}
	for (auto It = SelectedContextRows.CreateIterator(); It; ++It)
	{
		if (!ValidContextIds.Contains(*It))
		{
			It.RemoveCurrent();
		}
	}

	TSet<FString> ValidSurfaceIds;
	for (const FRshipMappingSurfaceState& Surface : SortedSurfaces)
	{
		ValidSurfaceIds.Add(Surface.Id);
	}
	for (auto It = SelectedSurfaceRows.CreateIterator(); It; ++It)
	{
		if (!ValidSurfaceIds.Contains(*It))
		{
			It.RemoveCurrent();
		}
	}

	TSet<FString> ValidMappingIds;
	for (const FRshipContentMappingState& Mapping : SortedMappings)
	{
		ValidMappingIds.Add(Mapping.Id);
	}
	for (auto It = SelectedMappingRows.CreateIterator(); It; ++It)
	{
		if (!ValidMappingIds.Contains(*It))
		{
			It.RemoveCurrent();
		}
	}
		for (auto It = ExpandedMappingConfigRows.CreateIterator(); It; ++It)
		{
			if (!ValidMappingIds.Contains(*It))
			{
				It.RemoveCurrent();
			}
		}
		for (auto It = ExpandedProjectionPrecisionRows.CreateIterator(); It; ++It)
		{
			if (!ValidMappingIds.Contains(*It))
			{
				It.RemoveCurrent();
			}
		}
		for (const FRshipContentMappingState& Mapping : SortedMappings)
		{
				if (!IsProjectionMode(GetMappingModeFromState(Mapping)) && IsInlineProjectionPrecisionExpanded(Mapping.Id))
				{
					ExpandedProjectionPrecisionRows.Remove(Mapping.Id);
				}
		}

		auto MatchesFilter = [](const FString& Filter, const FString& Value) -> bool
		{
		return Filter.IsEmpty() || Value.Contains(Filter, ESearchCase::IgnoreCase);
	};

	TArray<FRshipRenderContextState> VisibleContexts;
	TArray<FRshipMappingSurfaceState> VisibleSurfaces;
	TArray<FRshipContentMappingState> VisibleMappings;

	for (const FRshipRenderContextState& Context : SortedContexts)
	{
		if (bContextErrorsOnly && Context.LastError.IsEmpty())
		{
			continue;
		}
		const FString SearchText = FString::Printf(
			TEXT("%s %s %s %s %s %s %s"),
			*Context.Name,
			*Context.Id,
			*Context.ProjectId,
			*Context.SourceType,
			*Context.CameraId,
			*Context.AssetId,
			*Context.LastError);
		if (MatchesFilter(ContextFilterText, SearchText))
		{
			VisibleContexts.Add(Context);
		}
	}

	for (const FRshipMappingSurfaceState& Surface : SortedSurfaces)
	{
		if (bSurfaceErrorsOnly && Surface.LastError.IsEmpty())
		{
			continue;
		}
		const FString SearchText = FString::Printf(
			TEXT("%s %s %s %s %s %s"),
			*Surface.Name,
			*Surface.Id,
			*Surface.ProjectId,
			*Surface.TargetId,
			*Surface.MeshComponentName,
			*Surface.LastError);
		if (MatchesFilter(SurfaceFilterText, SearchText))
		{
			VisibleSurfaces.Add(Surface);
		}
	}

	for (const FRshipContentMappingState& Mapping : SortedMappings)
	{
		if (bMappingErrorsOnly && Mapping.LastError.IsEmpty())
		{
			continue;
		}
		const FString SearchText = FString::Printf(
			TEXT("%s %s %s %s %s %s %s"),
			*Mapping.Name,
			*Mapping.Id,
			*Mapping.ProjectId,
			*Mapping.Type,
			*GetMappingDisplayLabel(Mapping).ToString(),
			*Mapping.ContextId,
			*Mapping.LastError);
		if (MatchesFilter(MappingFilterText, SearchText))
		{
			VisibleMappings.Add(Mapping);
		}
	}

	if (!ActiveProjectionMappingId.IsEmpty())
	{
		bool bFoundActive = false;
		for (const FRshipContentMappingState& Mapping : SortedMappings)
		{
				if (Mapping.Id == ActiveProjectionMappingId)
				{
					bFoundActive = true;
					if (!IsProjectionMode(GetMappingModeFromState(Mapping)))
					{
						StopProjectionEdit();
					}
					break;
			}
		}
		if (!bFoundActive)
		{
			StopProjectionEdit();
		}
	}

	if (CountsText.IsValid())
	{
		const bool bHasAnyFilter = !ContextFilterText.IsEmpty() || !SurfaceFilterText.IsEmpty() || !MappingFilterText.IsEmpty();
		if (bHasAnyFilter)
		{
			CountsText->SetText(FText::Format(
				LOCTEXT("CountsFormatFiltered", "Inputs: {0}/{1}  Screens: {2}/{3}  Mappings: {4}/{5}"),
				FText::AsNumber(VisibleContexts.Num()),
				FText::AsNumber(Contexts.Num()),
				FText::AsNumber(VisibleSurfaces.Num()),
				FText::AsNumber(Surfaces.Num()),
				FText::AsNumber(VisibleMappings.Num()),
				FText::AsNumber(Mappings.Num())));
		}
		else
		{
			CountsText->SetText(FText::Format(
				LOCTEXT("CountsFormat", "Inputs: {0}  Screens: {1}  Mappings: {2}"),
				FText::AsNumber(Contexts.Num()),
				FText::AsNumber(Surfaces.Num()),
				FText::AsNumber(Mappings.Num())));
		}
	}

	if (ContextList.IsValid() && bRebuildLists)
	{
		ContextList->ClearChildren();
		if (SortedContexts.Num() == 0)
		{
			ContextList->AddSlot()[SNew(STextBlock).Text(LOCTEXT("NoContexts", "No inputs"))];
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

			if (VisibleContexts.Num() > 0)
			{
				ContextList->AddSlot()
				.AutoHeight()
				.Padding(0, 0, 0, 6)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)
					[
						SNew(STextBlock).Text(LOCTEXT("CtxBulkLabel", "Bulk:"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,6,0).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							return FText::Format(LOCTEXT("CtxBulkSelectedFmt", "Selected {0}"), FText::AsNumber(SelectedContextRows.Num()));
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,10,0).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text_Lambda([this, VisibleCount = VisibleContexts.Num()]()
						{
							return MakeBulkScopeLabel(SelectedContextRows.Num(), VisibleCount);
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SNew(SButton)
						.Text(LOCTEXT("CtxSelectVisible", "Select Visible"))
						.OnClicked_Lambda([this, VisibleContexts]()
						{
							for (const FRshipRenderContextState& Context : VisibleContexts)
							{
								SelectedContextRows.Add(Context.Id);
							}
							bHasListHash = false;
							bHasPendingListHash = false;
							RefreshStatus();
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
					[
						SNew(SButton)
						.Text(LOCTEXT("CtxClearSelection", "Clear Selection"))
						.OnClicked_Lambda([this]()
						{
							SelectedContextRows.Empty();
							bHasListHash = false;
							bHasPendingListHash = false;
							RefreshStatus();
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SNew(SButton)
						.Text(LOCTEXT("CtxBulkEnable", "Enable"))
						.OnClicked_Lambda([this, VisibleContexts]()
						{
							if (!GEngine) return FReply::Handled();
							if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
							{
								if (URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager())
								{
									const bool bUseSelection = SelectedContextRows.Num() > 0;
									for (const FRshipRenderContextState& Context : VisibleContexts)
									{
										if (bUseSelection && !SelectedContextRows.Contains(Context.Id))
										{
											continue;
										}
										if (!Context.bEnabled)
										{
											FRshipRenderContextState Updated = Context;
											Updated.bEnabled = true;
											Manager->UpdateRenderContext(Updated);
										}
									}
								}
							}
							RefreshStatus();
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("CtxBulkDisable", "Disable"))
						.OnClicked_Lambda([this, VisibleContexts]()
						{
							if (!GEngine) return FReply::Handled();
							if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
							{
								if (URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager())
								{
									const bool bUseSelection = SelectedContextRows.Num() > 0;
									for (const FRshipRenderContextState& Context : VisibleContexts)
									{
										if (bUseSelection && !SelectedContextRows.Contains(Context.Id))
										{
											continue;
										}
										if (Context.bEnabled)
										{
											FRshipRenderContextState Updated = Context;
											Updated.bEnabled = false;
											Manager->UpdateRenderContext(Updated);
										}
									}
								}
							}
								RefreshStatus();
								return FReply::Handled();
							})
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("CtxBulkDelete", "Delete"))
						.OnClicked_Lambda([this, VisibleContexts]()
						{
							if (!GEngine) return FReply::Handled();
							URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
							if (!Subsystem) return FReply::Handled();
							URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
							if (!Manager) return FReply::Handled();

							const bool bUseSelection = SelectedContextRows.Num() > 0;
							for (const FRshipRenderContextState& Context : VisibleContexts)
							{
								if (bUseSelection && !SelectedContextRows.Contains(Context.Id))
								{
									continue;
								}
								Manager->DeleteRenderContext(Context.Id);
								if (SelectedContextId == Context.Id)
								{
									SelectedContextId.Reset();
								}
							}

							SelectedContextRows.Empty();
							bHasListHash = false;
							bHasPendingListHash = false;
							RefreshStatus();
							return FReply::Handled();
						})
					]
				];
			}

			if (VisibleContexts.Num() == 0)
			{
				ContextList->AddSlot()[SNew(STextBlock).Text(LOCTEXT("NoContextsMatchFilter", "No inputs match the current filter"))];
			}

			for (const FRshipRenderContextState& Context : VisibleContexts)
			{
				const FString Name = Context.Name.IsEmpty() ? Context.Id : Context.Name;
				const FString SourceType = Context.SourceType.IsEmpty() ? TEXT("camera") : Context.SourceType;
				const FString ProjectText = Context.ProjectId.IsEmpty() ? TEXT("(default)") : Context.ProjectId;
				const FString ErrorSuffix = Context.LastError.IsEmpty() ? TEXT("") : FString::Printf(TEXT(" - %s"), *Context.LastError);
				const FString Line = FString::Printf(TEXT("%s [%s] (res=%dx%d, capture=%s, project=%s, %s)%s"),
					*Name,
					*SourceType,
					Context.Width,
					Context.Height,
					Context.CaptureMode.IsEmpty() ? TEXT("default") : *Context.CaptureMode,
					*ProjectText,
					Context.bEnabled ? TEXT("enabled") : TEXT("disabled"),
					*ErrorSuffix);

				const bool bHasError = !Context.LastError.IsEmpty();

				ContextList->AddSlot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,2,6,0)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([this, ContextId = Context.Id]()
						{
							return SelectedContextRows.Contains(ContextId) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						})
						.OnCheckStateChanged_Lambda([this, ContextId = Context.Id](ECheckBoxState NewState)
						{
							if (NewState == ECheckBoxState::Checked)
							{
								SelectedContextRows.Add(ContextId);
							}
							else
							{
								SelectedContextRows.Remove(ContextId);
							}
						})
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0,0,4,0)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f)
							[
								SNew(STextBlock)
									.Text(FText::FromString(Line))
									.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
									.ColorAndOpacity(bHasError ? FLinearColor(1.f, 0.5f, 0.4f, 1.f) : FLinearColor::White)
									.AutoWrapText(false)
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(1,0,0,0)
						[
							SNew(STextBlock)
								.Text(FText::FromString(Context.LastError))
								.Visibility(bHasError ? EVisibility::Visible : EVisibility::Collapsed)
								.ColorAndOpacity(FLinearColor(1.f, 0.35f, 0.25f, 1.f))
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 7))
								.AutoWrapText(true)
						]
						]
				];
			}
		}
	}

	if (SurfaceList.IsValid() && bRebuildLists)
	{
		SurfaceList->ClearChildren();
		if (SortedSurfaces.Num() == 0)
		{
			SurfaceList->AddSlot()[SNew(STextBlock).Text(LOCTEXT("NoSurfaces", "No screens"))];
		}
		else
		{

			if (VisibleSurfaces.Num() > 0)
			{
				SurfaceList->AddSlot()
				.AutoHeight()
				.Padding(0, 0, 0, 6)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)
					[
						SNew(STextBlock).Text(LOCTEXT("SurfBulkLabel", "Bulk:"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,6,0).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							return FText::Format(LOCTEXT("SurfBulkSelectedFmt", "Selected {0}"), FText::AsNumber(SelectedSurfaceRows.Num()));
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,10,0).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text_Lambda([this, VisibleCount = VisibleSurfaces.Num()]()
						{
							return MakeBulkScopeLabel(SelectedSurfaceRows.Num(), VisibleCount);
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SNew(SButton)
						.Text(LOCTEXT("SurfSelectVisible", "Select Visible"))
						.OnClicked_Lambda([this, VisibleSurfaces]()
						{
							for (const FRshipMappingSurfaceState& Surface : VisibleSurfaces)
							{
								SelectedSurfaceRows.Add(Surface.Id);
							}
							bHasListHash = false;
							bHasPendingListHash = false;
							RefreshStatus();
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)
					[
						SNew(SButton)
						.Text(LOCTEXT("SurfClearSelection", "Clear Selection"))
						.OnClicked_Lambda([this]()
						{
							SelectedSurfaceRows.Empty();
							bHasListHash = false;
							bHasPendingListHash = false;
							RefreshStatus();
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SNew(SButton)
						.Text(LOCTEXT("SurfBulkEnable", "Enable"))
						.OnClicked_Lambda([this, VisibleSurfaces]()
						{
							if (!GEngine) return FReply::Handled();
							if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
							{
								if (URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager())
								{
									const bool bUseSelection = SelectedSurfaceRows.Num() > 0;
									for (const FRshipMappingSurfaceState& Surface : VisibleSurfaces)
									{
										if (bUseSelection && !SelectedSurfaceRows.Contains(Surface.Id))
										{
											continue;
										}
										if (!Surface.bEnabled)
										{
											FRshipMappingSurfaceState Updated = Surface;
											Updated.bEnabled = true;
											Manager->UpdateMappingSurface(Updated);
										}
									}
								}
							}
							RefreshStatus();
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton)
							.Text(LOCTEXT("SurfBulkDisable", "Disable"))
							.OnClicked_Lambda([this, VisibleSurfaces]()
							{
								if (!GEngine) return FReply::Handled();
								if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
								{
									if (URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager())
									{
										const bool bUseSelection = SelectedSurfaceRows.Num() > 0;
										for (const FRshipMappingSurfaceState& Surface : VisibleSurfaces)
										{
											if (bUseSelection && !SelectedSurfaceRows.Contains(Surface.Id))
											{
												continue;
											}
											if (Surface.bEnabled)
											{
												FRshipMappingSurfaceState Updated = Surface;
												Updated.bEnabled = false;
												Manager->UpdateMappingSurface(Updated);
											}
										}
									}
								}
								RefreshStatus();
								return FReply::Handled();
							})
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
						[
							SNew(SButton)
							.Text(LOCTEXT("SurfBulkDelete", "Delete"))
							.OnClicked_Lambda([this, VisibleSurfaces]()
							{
								if (!GEngine) return FReply::Handled();
								URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
								if (!Subsystem) return FReply::Handled();
								URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
								if (!Manager) return FReply::Handled();

								const bool bUseSelection = SelectedSurfaceRows.Num() > 0;
								for (const FRshipMappingSurfaceState& Surface : VisibleSurfaces)
								{
									if (bUseSelection && !SelectedSurfaceRows.Contains(Surface.Id))
									{
										continue;
									}
									Manager->DeleteMappingSurface(Surface.Id);
									if (SelectedSurfaceId == Surface.Id)
									{
										SelectedSurfaceId.Reset();
									}
								}

								SelectedSurfaceRows.Empty();
								bHasListHash = false;
								bHasPendingListHash = false;
								RefreshStatus();
								return FReply::Handled();
							})
						]
					];
				}

			if (VisibleSurfaces.Num() == 0)
			{
				SurfaceList->AddSlot()[SNew(STextBlock).Text(LOCTEXT("NoSurfacesMatchFilter", "No screens match the current filter"))];
			}

			for (const FRshipMappingSurfaceState& Surface : VisibleSurfaces)
			{
				const FString Name = Surface.Name.IsEmpty() ? Surface.Id : Surface.Name;
				const FString MeshName = Surface.MeshComponentName.IsEmpty() ? TEXT("No Mesh") : Surface.MeshComponentName;
				TArray<FString> SlotValues;
				SlotValues.Reserve(Surface.MaterialSlots.Num());
				for (int32 Slot : Surface.MaterialSlots)
				{
					SlotValues.Add(FString::FromInt(Slot));
				}
				const FString SlotSummary = SlotValues.Num() == 0 ? TEXT("all") : FString::Join(SlotValues, TEXT(","));
				const FString ProjectText = Surface.ProjectId.IsEmpty() ? TEXT("(default)") : Surface.ProjectId;
				const FString Status = Surface.bEnabled ? TEXT("enabled") : TEXT("disabled");
				const FString ErrorSuffix = Surface.LastError.IsEmpty() ? TEXT("") : FString::Printf(TEXT(" - %s"), *Surface.LastError);
				const FString Line = FString::Printf(TEXT("%s | mesh=%s | uv=%d | slots=%s | project=%s | %s%s"),
					*Name,
					*MeshName,
					Surface.UVChannel,
					*SlotSummary,
					*ProjectText,
					*Status,
					*ErrorSuffix);
				const bool bHasError = !Surface.LastError.IsEmpty();

				SurfaceList->AddSlot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0,2,6,0)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([this, SurfaceId = Surface.Id]()
						{
							return SelectedSurfaceRows.Contains(SurfaceId) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						})
						.OnCheckStateChanged_Lambda([this, SurfaceId = Surface.Id](ECheckBoxState NewState)
						{
							if (NewState == ECheckBoxState::Checked)
							{
								SelectedSurfaceRows.Add(SurfaceId);
							}
							else
							{
								SelectedSurfaceRows.Remove(SurfaceId);
							}
						})
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0,0,4,0)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
								.Text(FText::FromString(Line))
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
								.ColorAndOpacity(bHasError ? FLinearColor(1.f, 0.5f, 0.4f, 1.f) : FLinearColor::White)
								.AutoWrapText(false)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(1,0,0,0)
						[
							SNew(STextBlock)
								.Text(FText::FromString(Surface.LastError))
								.Visibility(bHasError ? EVisibility::Visible : EVisibility::Collapsed)
								.ColorAndOpacity(FLinearColor(1.f, 0.35f, 0.25f, 1.f))
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 7))
								.AutoWrapText(true)
						]
					]
				];
			}
		}
	}


	if (MappingList.IsValid() && bRebuildLists)
	{
		TMap<FString, FMappingSurfaceSummaryInfo> SurfaceInfoById;
		SurfaceInfoById.Reserve(SortedSurfaces.Num());
		for (const FRshipMappingSurfaceState& Surface : SortedSurfaces)
		{
			const FString SurfaceName = Surface.Name.IsEmpty() ? Surface.Id : Surface.Name;
			SurfaceInfoById.Add(Surface.Id, { SurfaceName, Surface.MeshComponentName });
		}

		MappingList->ClearChildren();
		if (SortedMappings.Num() == 0)
		{
			MappingList->AddSlot()
			[
				SNew(STextBlock).Text(LOCTEXT("NoMappings", "No mappings"))
			];
		}
		else
		{
			if (VisibleMappings.Num() > 0)
			{
				MappingList->AddSlot()
				.AutoHeight()
				.Padding(0, 0, 0, 6)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
					[
						SNew(STextBlock).Text(LOCTEXT("MapBulkLabel", "Bulk:"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							return FText::Format(LOCTEXT("MapBulkSelectedFmt", "Selected {0}"), FText::AsNumber(SelectedMappingRows.Num()));
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text_Lambda([this, VisibleCount = VisibleMappings.Num()]()
						{
							return MakeBulkScopeLabel(SelectedMappingRows.Num(), VisibleCount);
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("MapSelectVisible", "Select Visible"))
						.OnClicked_Lambda([this, VisibleMappings]()
						{
							for (const FRshipContentMappingState& Mapping : VisibleMappings)
							{
								SelectedMappingRows.Add(Mapping.Id);
							}
							bHasListHash = false;
							bHasPendingListHash = false;
							RefreshStatus();
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("MapClearSelection", "Clear Selection"))
						.OnClicked_Lambda([this]()
						{
							SelectedMappingRows.Empty();
							bHasListHash = false;
							bHasPendingListHash = false;
							RefreshStatus();
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("MapBulkEnable", "Enable"))
						.OnClicked_Lambda([this, VisibleMappings]()
						{
							if (!GEngine) return FReply::Handled();
							URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
							if (!Subsystem) return FReply::Handled();
							URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
							if (!Manager) return FReply::Handled();
							for (const FRshipContentMappingState& Mapping : VisibleMappings)
							{
								if (!SelectedMappingRows.IsEmpty() && !SelectedMappingRows.Contains(Mapping.Id))
								{
									continue;
								}
								if (Mapping.bEnabled)
								{
									continue;
								}
								FRshipContentMappingState Updated = Mapping;
								Updated.bEnabled = true;
								Manager->UpdateMapping(Updated);
							}
							RefreshStatus();
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("MapBulkDisable", "Disable"))
						.OnClicked_Lambda([this, VisibleMappings]()
						{
							if (!GEngine) return FReply::Handled();
							URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
							if (!Subsystem) return FReply::Handled();
							URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
							if (!Manager) return FReply::Handled();
							for (const FRshipContentMappingState& Mapping : VisibleMappings)
							{
								if (!SelectedMappingRows.IsEmpty() && !SelectedMappingRows.Contains(Mapping.Id))
								{
									continue;
								}
								if (!Mapping.bEnabled)
								{
									continue;
								}
								FRshipContentMappingState Updated = Mapping;
								Updated.bEnabled = false;
								Manager->UpdateMapping(Updated);
							}
							RefreshStatus();
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("MapBulkDuplicate", "Duplicate"))
						.OnClicked_Lambda([this]()
						{
							DuplicateSelectedMappings();
							return FReply::Handled();
						})
					]
				];
			}

			if (VisibleMappings.Num() == 0)
			{
				MappingList->AddSlot()
				[
					SNew(STextBlock).Text(LOCTEXT("NoMappingsMatchFilter", "No mappings match the current filter"))
				];
			}

			for (const FRshipContentMappingState& Mapping : VisibleMappings)
			{
				const FString MappingId = Mapping.Id;
				const FString MappingName = Mapping.Name.IsEmpty() ? Mapping.Id : Mapping.Name;
				const FString MappingType = GetMappingDisplayLabel(Mapping).ToString();
				const FString CanvasSummary = BuildMappingCanvasSummary(Mapping);
				const FString ScreenSummary = BuildMappingScreenSummary(Mapping, SurfaceInfoById);
				const FString RowText = FString::Printf(TEXT("%s [%s] | Canvas: %s | Screens by mesh: %s | Opacity %.2f"),
					*MappingName,
					*MappingType,
					*CanvasSummary,
					*ScreenSummary,
					Mapping.Opacity);
				const bool bHasError = !Mapping.LastError.IsEmpty();

				MappingList->AddSlot()
				.AutoHeight()
				.Padding(0, 0, 0, 1)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([this, MappingId]()
						{
							return SelectedMappingRows.Contains(MappingId) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						})
						.OnCheckStateChanged_Lambda([this, MappingId](ECheckBoxState NewState)
						{
							if (NewState == ECheckBoxState::Checked)
							{
								SelectedMappingRows.Add(MappingId);
							}
							else
							{
								SelectedMappingRows.Remove(MappingId);
							}
						})
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0, 0, 4, 0)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock)
								.Text(FText::FromString(RowText))
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
								.ColorAndOpacity(bHasError ? FLinearColor(1.f, 0.5f, 0.4f, 1.f) : FLinearColor(0.95f, 0.95f, 0.95f, 1.f))
								.AutoWrapText(false)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(1, 0, 0, 0)
							[
								SNew(STextBlock)
								.Text(FText::FromString(Mapping.LastError))
								.Visibility(bHasError ? EVisibility::Visible : EVisibility::Collapsed)
								.ColorAndOpacity(FLinearColor(1.f, 0.35f, 0.25f, 1.f))
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 7))
								.AutoWrapText(true)
							]
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
