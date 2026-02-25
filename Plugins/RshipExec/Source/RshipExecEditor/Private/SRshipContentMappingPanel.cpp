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
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SWindow.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Engine/Engine.h"
#include "SlateOptMacros.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "Components/MeshComponent.h"
#include "Components/ActorComponent.h"
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
#include "ScopedTransaction.h"
#include "Misc/Crc.h"

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
	const FMargin CompactMappingButtonPadding(1.0f, 0.0f);
	const FSlateFontInfo CompactMappingButtonFont = FCoreStyle::GetDefaultFontStyle("Regular", 8);
	const FSlateFontInfo MappingListHeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);
	const FSlateFontInfo MappingListFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);
	const FSlateFontInfo CompactErrorFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);
	const FVector2D MappingTypeIconSize(4.0f, 4.0f);

	void EnsureContextDefaultsForMapping(URshipContentMappingManager* Manager, const FString& ContextId)
	{
		if (!Manager || ContextId.IsEmpty())
		{
			return;
		}

		const TArray<FRshipRenderContextState> ExistingContexts = Manager->GetRenderContexts();
		for (const FRshipRenderContextState& ExistingContext : ExistingContexts)
		{
			if (ExistingContext.Id != ContextId)
			{
				continue;
			}

			FRshipRenderContextState UpdatedContext = ExistingContext;
			bool bNeedsUpdate = false;

			if (UpdatedContext.SourceType.IsEmpty())
			{
				UpdatedContext.SourceType = UpdatedContext.AssetId.IsEmpty() ? TEXT("camera") : TEXT("asset-store");
				bNeedsUpdate = true;
			}
			if (UpdatedContext.Width <= 0)
			{
				UpdatedContext.Width = 1920;
				bNeedsUpdate = true;
			}
			if (UpdatedContext.Height <= 0)
			{
				UpdatedContext.Height = 1080;
				bNeedsUpdate = true;
			}
			if (UpdatedContext.CaptureMode.IsEmpty())
			{
				UpdatedContext.CaptureMode = TEXT("FinalColorLDR");
				bNeedsUpdate = true;
			}
			if (!UpdatedContext.bEnabled)
			{
				UpdatedContext.bEnabled = true;
				bNeedsUpdate = true;
			}

			if (bNeedsUpdate)
			{
				Manager->UpdateRenderContext(UpdatedContext);
			}
			return;
		}
	}

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
		if (Config->HasTypedField<EJson::Object>(TEXT("feedRect"))
			|| Config->HasTypedField<EJson::Array>(TEXT("feedRects"))
			|| Config->HasTypedField<EJson::Object>(TEXT("feedV2")))
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

	const TArray<FString>& GetMappingModeOptions()
	{
		static const TArray<FString> Modes = {
			MapModeDirect,
			MapModeFeed,
			MapModePerspective,
			MapModeCustomMatrix,
			MapModeCylindrical,
			MapModeSpherical,
			MapModeParallel,
			MapModeRadial,
			MapModeMesh,
			MapModeFisheye,
			MapModeCameraPlate,
			MapModeSpatial,
			MapModeDepthMap
		};
		return Modes;
	}

	FText GetMappingModeOptionLabel(const FString& Mode)
	{
		const FString Normalized = NormalizeMapMode(Mode, MapModeDirect);
		if (Normalized == MapModeFeed) return LOCTEXT("MapModeOptFeedLabel", "Feed");
		if (Normalized == MapModeDirect) return LOCTEXT("MapModeOptDirectLabel", "Direct");
		if (Normalized == MapModeCustomMatrix) return LOCTEXT("MapModeOptCustomMatrixLabel", "Custom Matrix");
		if (Normalized == MapModeCylindrical) return LOCTEXT("MapModeOptCylLabel", "Cylindrical");
		if (Normalized == MapModeSpherical) return LOCTEXT("MapModeOptSphericalLabel", "Spherical");
		if (Normalized == MapModeParallel) return LOCTEXT("MapModeOptParallelLabel", "Parallel");
		if (Normalized == MapModeRadial) return LOCTEXT("MapModeOptRadialLabel", "Radial");
		if (Normalized == MapModeMesh) return LOCTEXT("MapModeOptMeshLabel", "Mesh");
		if (Normalized == MapModeFisheye) return LOCTEXT("MapModeOptFisheyeLabel", "Fisheye");
		if (Normalized == MapModeCameraPlate) return LOCTEXT("MapModeOptCameraPlateLabel", "Camera Plate");
		if (Normalized == MapModeSpatial) return LOCTEXT("MapModeOptSpatialLabel", "Spatial");
		if (Normalized == MapModeDepthMap) return LOCTEXT("MapModeOptDepthMapLabel", "Depth Map");
		return LOCTEXT("MapModeOptPerspectiveLabel", "Perspective");
	}

	void ApplyModeToMappingState(FRshipContentMappingState& Mapping, const FString& RequestedMode)
	{
		const FString Mode = NormalizeMapMode(RequestedMode, MapModeDirect);
		if (!Mapping.Config.IsValid())
		{
			Mapping.Config = MakeShared<FJsonObject>();
		}

		auto EnsureUvTransformDefaults = [&]()
		{
			if (!Mapping.Config->HasTypedField<EJson::Object>(TEXT("uvTransform")))
			{
				TSharedPtr<FJsonObject> Uv = MakeShared<FJsonObject>();
				Uv->SetNumberField(TEXT("scaleU"), 1.0);
				Uv->SetNumberField(TEXT("scaleV"), 1.0);
				Uv->SetNumberField(TEXT("offsetU"), 0.0);
				Uv->SetNumberField(TEXT("offsetV"), 0.0);
				Uv->SetNumberField(TEXT("rotationDeg"), 0.0);
				Mapping.Config->SetObjectField(TEXT("uvTransform"), Uv);
			}
		};

		auto EnsureProjectionDefaults = [&]()
		{
			if (!Mapping.Config->HasTypedField<EJson::Object>(TEXT("projectorPosition")))
			{
				TSharedPtr<FJsonObject> Pos = MakeShared<FJsonObject>();
				Pos->SetNumberField(TEXT("x"), 0.0);
				Pos->SetNumberField(TEXT("y"), 0.0);
				Pos->SetNumberField(TEXT("z"), 0.0);
				Mapping.Config->SetObjectField(TEXT("projectorPosition"), Pos);
			}
			if (!Mapping.Config->HasTypedField<EJson::Object>(TEXT("projectorRotation")))
			{
				TSharedPtr<FJsonObject> Rot = MakeShared<FJsonObject>();
				Rot->SetNumberField(TEXT("x"), 0.0);
				Rot->SetNumberField(TEXT("y"), 0.0);
				Rot->SetNumberField(TEXT("z"), 0.0);
				Mapping.Config->SetObjectField(TEXT("projectorRotation"), Rot);
			}
			if (!Mapping.Config->HasTypedField<EJson::Number>(TEXT("fov")))
			{
				Mapping.Config->SetNumberField(TEXT("fov"), 60.0);
			}
			if (!Mapping.Config->HasTypedField<EJson::Number>(TEXT("aspectRatio")))
			{
				Mapping.Config->SetNumberField(TEXT("aspectRatio"), 1.7778);
			}
			if (!Mapping.Config->HasTypedField<EJson::Number>(TEXT("near")))
			{
				Mapping.Config->SetNumberField(TEXT("near"), 10.0);
			}
			if (!Mapping.Config->HasTypedField<EJson::Number>(TEXT("far")))
			{
				Mapping.Config->SetNumberField(TEXT("far"), 10000.0);
			}
		};

		if (Mode == MapModeDirect || Mode == MapModeFeed)
		{
			Mapping.Type = TEXT("surface-uv");
			Mapping.Config->SetStringField(TEXT("uvMode"), (Mode == MapModeFeed) ? MapModeFeed : MapModeDirect);
			Mapping.Config->RemoveField(TEXT("projectionType"));
			EnsureUvTransformDefaults();
			if (Mode == MapModeFeed && !Mapping.Config->HasTypedField<EJson::Object>(TEXT("feedRect")))
			{
				TSharedPtr<FJsonObject> FeedRect = MakeShared<FJsonObject>();
				FeedRect->SetNumberField(TEXT("u"), 0.0);
				FeedRect->SetNumberField(TEXT("v"), 0.0);
				FeedRect->SetNumberField(TEXT("width"), 1.0);
				FeedRect->SetNumberField(TEXT("height"), 1.0);
				Mapping.Config->SetObjectField(TEXT("feedRect"), FeedRect);
			}
			if (Mode == MapModeFeed && !Mapping.Config->HasTypedField<EJson::Object>(TEXT("feedV2")))
			{
				TSharedPtr<FJsonObject> FeedV2 = MakeShared<FJsonObject>();
				FeedV2->SetStringField(TEXT("coordinateSpace"), TEXT("pixel"));
				FeedV2->SetArrayField(TEXT("sources"), TArray<TSharedPtr<FJsonValue>>());
				FeedV2->SetArrayField(TEXT("destinations"), TArray<TSharedPtr<FJsonValue>>());
				FeedV2->SetArrayField(TEXT("routes"), TArray<TSharedPtr<FJsonValue>>());
				Mapping.Config->SetObjectField(TEXT("feedV2"), FeedV2);
			}
			return;
		}

		Mapping.Type = TEXT("surface-projection");
		Mapping.Config->SetStringField(TEXT("projectionType"), Mode);
		Mapping.Config->RemoveField(TEXT("uvMode"));
		EnsureProjectionDefaults();

		if ((Mode == MapModeCylindrical || Mode == MapModeRadial) && !Mapping.Config->HasTypedField<EJson::Object>(TEXT("cylindrical")))
		{
			TSharedPtr<FJsonObject> Cyl = MakeShared<FJsonObject>();
			Cyl->SetStringField(TEXT("axis"), TEXT("y"));
			Cyl->SetNumberField(TEXT("radius"), 100.0);
			Cyl->SetNumberField(TEXT("height"), 1000.0);
			Cyl->SetNumberField(TEXT("startAngle"), 0.0);
			Cyl->SetNumberField(TEXT("endAngle"), 90.0);
			Mapping.Config->SetObjectField(TEXT("cylindrical"), Cyl);
		}

		if (Mode == MapModeSpherical)
		{
			if (!Mapping.Config->HasTypedField<EJson::Number>(TEXT("sphereRadius")))
			{
				Mapping.Config->SetNumberField(TEXT("sphereRadius"), 500.0);
			}
			if (!Mapping.Config->HasTypedField<EJson::Number>(TEXT("horizontalArc")))
			{
				Mapping.Config->SetNumberField(TEXT("horizontalArc"), 360.0);
			}
			if (!Mapping.Config->HasTypedField<EJson::Number>(TEXT("verticalArc")))
			{
				Mapping.Config->SetNumberField(TEXT("verticalArc"), 180.0);
			}
		}

		if (Mode == MapModeParallel)
		{
			if (!Mapping.Config->HasTypedField<EJson::Number>(TEXT("sizeW")))
			{
				Mapping.Config->SetNumberField(TEXT("sizeW"), 1000.0);
			}
			if (!Mapping.Config->HasTypedField<EJson::Number>(TEXT("sizeH")))
			{
				Mapping.Config->SetNumberField(TEXT("sizeH"), 1000.0);
			}
		}

		if (Mode == MapModeMesh && !Mapping.Config->HasTypedField<EJson::Object>(TEXT("eyepoint")))
		{
			TSharedPtr<FJsonObject> EpObj = MakeShared<FJsonObject>();
			EpObj->SetNumberField(TEXT("x"), 0.0);
			EpObj->SetNumberField(TEXT("y"), 0.0);
			EpObj->SetNumberField(TEXT("z"), 0.0);
			Mapping.Config->SetObjectField(TEXT("eyepoint"), EpObj);
		}

		if (Mode == MapModeFisheye)
		{
			if (!Mapping.Config->HasTypedField<EJson::Number>(TEXT("fisheyeFov")))
			{
				Mapping.Config->SetNumberField(TEXT("fisheyeFov"), 180.0);
			}
			if (!Mapping.Config->HasTypedField<EJson::String>(TEXT("lensType")))
			{
				Mapping.Config->SetStringField(TEXT("lensType"), TEXT("equidistant"));
			}
		}

		if (Mode == MapModeCustomMatrix
			&& !Mapping.Config->HasTypedField<EJson::Object>(TEXT("customProjectionMatrix"))
			&& !Mapping.Config->HasTypedField<EJson::Object>(TEXT("matrix")))
		{
			TSharedPtr<FJsonObject> MatrixObj = MakeShared<FJsonObject>();
			for (int32 Row = 0; Row < 4; ++Row)
			{
				for (int32 Col = 0; Col < 4; ++Col)
				{
					const FString FieldName = FString::Printf(TEXT("m%d%d"), Row, Col);
					MatrixObj->SetNumberField(FieldName, Row == Col ? 1.0 : 0.0);
				}
			}
			Mapping.Config->SetObjectField(TEXT("customProjectionMatrix"), MatrixObj);
		}
	}

	bool IsProjectionMode(const FString& Mode);

	bool ActorIsValidScreenCandidate(AActor* Actor)
	{
		if (!Actor)
		{
			return false;
		}

		if (Actor->IsA<ACameraActor>() || Actor->IsA<ARshipCameraActor>())
		{
			return false;
		}

		if (Actor->FindComponentByClass<UCameraComponent>())
		{
			return false;
		}

		TArray<UMeshComponent*> MeshComponents;
		Actor->GetComponents(MeshComponents);
		return MeshComponents.Num() > 0;
	}

	const FSlateBrush* GetMappingTypeIcon(const FString& MappingMode)
	{
		if (MappingMode == MapModeDirect)
		{
			return FAppStyle::GetBrush(TEXT("Icons.Check"));
		}
		if (MappingMode == MapModeFeed)
		{
			return FAppStyle::GetBrush(TEXT("Icons.Import"));
		}
		if (MappingMode == MapModeCameraPlate)
		{
			return FAppStyle::GetBrush(TEXT("Icons.Export"));
		}
		return FAppStyle::GetBrush(TEXT("Icons.FilledCircle"));
	}

	FLinearColor GetMappingTypeColor(const FString& MappingMode)
	{
		if (MappingMode == MapModeDirect) return FLinearColor(0.25f, 0.88f, 0.25f);
		if (MappingMode == MapModeFeed) return FLinearColor(0.25f, 0.6f, 1.0f);
		if (MappingMode == MapModePerspective) return FLinearColor(0.9f, 0.7f, 0.2f);
		if (MappingMode == MapModeCustomMatrix) return FLinearColor(0.86f, 0.24f, 0.96f);
		if (MappingMode == MapModeCylindrical) return FLinearColor(0.82f, 0.44f, 0.17f);
		if (MappingMode == MapModeSpherical) return FLinearColor(0.52f, 0.42f, 1.0f);
		if (MappingMode == MapModeParallel) return FLinearColor(0.2f, 0.85f, 0.85f);
		if (MappingMode == MapModeRadial) return FLinearColor(0.95f, 0.4f, 0.3f);
		if (MappingMode == MapModeMesh) return FLinearColor(0.9f, 0.9f, 0.28f);
		if (MappingMode == MapModeFisheye) return FLinearColor(0.62f, 0.62f, 0.62f);
		if (MappingMode == MapModeCameraPlate) return FLinearColor(0.2f, 0.8f, 0.36f);
		if (MappingMode == MapModeSpatial) return FLinearColor(0.78f, 0.36f, 0.18f);
		if (MappingMode == MapModeDepthMap) return FLinearColor(0.32f, 0.82f, 1.0f);
		return FLinearColor::White;
	}

	FString DisplayTextOrDefault(const FString& Source, const FString& Fallback)
	{
		return Source.IsEmpty() ? Fallback : Source;
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
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(8.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				BuildMappingsSection()
			]
		]
	];

	ResetForms();
	ApplyStoredQuickCreateDefaults();
	RefreshStatus();
}

SRshipContentMappingPanel::~SRshipContentMappingPanel()
{
	CloseMappingEditorWindow();
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
			if (!Option->ResolvedId.IsEmpty())
			{
				return Option->ResolvedId;
			}
			if (Option->Actor.IsValid())
			{
				if (URshipTargetComponent* CreatedTarget = EnsureTargetComponentForActor(Option->Actor.Get()))
				{
					if (CreatedTarget->targetName.IsEmpty())
					{
						return Option->Id;
					}
					URshipSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URshipSubsystem>() : nullptr;
					if (Subsystem)
					{
						const FString ServiceId = Subsystem->GetServiceId();
						return ServiceId.IsEmpty() ? CreatedTarget->targetName : ServiceId + TEXT(":") + CreatedTarget->targetName;
					}
					return CreatedTarget->targetName;
				}
			}
			return Option->Id;
		}

		if (Option->Actor.IsValid())
		{
			const FString ActorLabel = Option->Actor->GetActorLabel();
			if (!ActorLabel.IsEmpty() && ActorLabel.Equals(Trimmed, ESearchCase::IgnoreCase))
			{
				if (!Option->ResolvedId.IsEmpty())
				{
					return Option->ResolvedId;
				}
				if (URshipTargetComponent* CreatedTarget = EnsureTargetComponentForActor(Option->Actor.Get()))
				{
					URshipSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<URshipSubsystem>() : nullptr;
					if (Subsystem)
					{
						const FString ServiceId = Subsystem->GetServiceId();
						return ServiceId.IsEmpty() ? CreatedTarget->targetName : ServiceId + TEXT(":") + CreatedTarget->targetName;
					}
					return CreatedTarget->targetName;
				}
				return Option->Id;
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

URshipTargetComponent* SRshipContentMappingPanel::EnsureTargetComponentForActor(AActor* Actor) const
{
#if WITH_EDITOR
	if (!Actor)
	{
		return nullptr;
	}

	if (URshipTargetComponent* Existing = Actor->FindComponentByClass<URshipTargetComponent>())
	{
		return Existing;
	}

	TArray<UMeshComponent*> MeshComponents;
	Actor->GetComponents(MeshComponents);
	if (MeshComponents.Num() == 0)
	{
		return nullptr;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddRshipTargetComponentTxn", "Add Rship Target Component"));
	Actor->Modify();

	URshipTargetComponent* NewComponent = NewObject<URshipTargetComponent>(Actor, URshipTargetComponent::StaticClass(), NAME_None, RF_Transactional);
	if (!NewComponent)
	{
		return nullptr;
	}

	if (NewComponent->targetName.IsEmpty())
	{
		const FString ActorLabel = Actor->GetActorLabel();
		NewComponent->targetName = ActorLabel.IsEmpty() ? Actor->GetName() : ActorLabel;
	}

	Actor->AddInstanceComponent(NewComponent);
	NewComponent->OnComponentCreated();
	NewComponent->RegisterComponent();
	return NewComponent;
#else
	return nullptr;
#endif
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
			return Option->ResolvedId.IsEmpty() ? Option->Id : Option->ResolvedId;
		}
	}

	URshipTargetComponent* TargetComp = Actor->FindComponentByClass<URshipTargetComponent>();
	if (!TargetComp)
	{
		TargetComp = EnsureTargetComponentForActor(Actor);
	}

	if (TargetComp)
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
			if (Subsystem)
			{
				const FString ServiceId = Subsystem->GetServiceId();
				if (!ServiceId.IsEmpty())
				{
					return ServiceId + TEXT(":") + TargetComp->targetName;
				}
			}
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

	const bool bScreenMode = (QuickTargetIdInput.IsValid() && TargetInput == QuickTargetIdInput);
	TArray<FRshipMappingSurfaceState> KnownSurfaces;
	if (bScreenMode && GEngine)
	{
		if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
		{
			if (URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager())
			{
				KnownSurfaces = Manager->GetMappingSurfaces();
			}
		}
	}

	TArray<FString> ResolvedIds;
	for (FSelectionIterator It(*Selection); It; ++It)
	{
		AActor* Actor = Cast<AActor>(*It);
		if (!ActorIsValidScreenCandidate(Actor))
		{
			continue;
		}

		FString ResolvedId;
		if (bScreenMode)
		{
			for (const TSharedPtr<FRshipIdOption>& Option : SurfaceOptions)
			{
				if (Option.IsValid() && Option->Actor.Get() == Actor)
				{
					ResolvedId = Option->Id;
					break;
				}
			}

			if (ResolvedId.IsEmpty())
			{
				const FString ActorPath = Actor->GetPathName();
				if (!ActorPath.IsEmpty())
				{
					for (const FRshipMappingSurfaceState& Surface : KnownSurfaces)
					{
						if (Surface.ActorPath.Equals(ActorPath, ESearchCase::CaseSensitive))
						{
							ResolvedId = Surface.Id;
							break;
						}
					}
				}

				if (ResolvedId.IsEmpty())
				{
					TArray<UMeshComponent*> MeshComponents;
					Actor->GetComponents(MeshComponents);
					const FString MeshName = (MeshComponents.Num() > 0 && MeshComponents[0]) ? MeshComponents[0]->GetName() : TEXT("");
					if (!MeshName.IsEmpty())
					{
						FString UniqueMatch;
						int32 MatchCount = 0;
						for (const FRshipMappingSurfaceState& Surface : KnownSurfaces)
						{
							if (Surface.MeshComponentName.Equals(MeshName, ESearchCase::IgnoreCase))
							{
								++MatchCount;
								UniqueMatch = Surface.Id;
								if (MatchCount > 1)
								{
									UniqueMatch.Reset();
									break;
								}
							}
						}
						if (MatchCount == 1 && !UniqueMatch.IsEmpty())
						{
							ResolvedId = UniqueMatch;
						}
					}
				}
			}
		}
		else
		{
			ResolvedId = Actor->GetPathName();
		}

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

int32 SRshipContentMappingPanel::CreateScreensFromSelectedActors()
{
#if WITH_EDITOR
	if (!GEditor || !GEngine)
	{
		return 0;
	}

	USelection* Selection = GEditor->GetSelectedActors();

	URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	if (!Subsystem)
	{
		return 0;
	}

	URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
	if (!Manager)
	{
		return 0;
	}

	TArray<FRshipMappingSurfaceState> ExistingSurfaces = Manager->GetMappingSurfaces();
	TArray<AActor*> CandidateActors;
	TSet<AActor*> SeenActors;
	int32 CreatedCount = 0;
	bool bSelectionChanged = false;

	auto AddCandidateActor = [&CandidateActors, &SeenActors](AActor* Actor)
	{
		if (Actor && !SeenActors.Contains(Actor))
		{
			SeenActors.Add(Actor);
			CandidateActors.Add(Actor);
		}
	};

	int32 ActorSelectionCount = 0;
	if (Selection)
	{
		for (FSelectionIterator It(*Selection); It; ++It)
		{
			if (AActor* SelectedActor = Cast<AActor>(*It))
			{
				++ActorSelectionCount;
				AddCandidateActor(SelectedActor);
			}
		}
	}
	if (ActorSelectionCount == 0)
	{
		if (USelection* ComponentSelection = GEditor->GetSelectedComponents())
		{
			for (FSelectionIterator It(*ComponentSelection); It; ++It)
			{
				if (UActorComponent* SelectedComponent = Cast<UActorComponent>(*It))
				{
					AddCandidateActor(SelectedComponent->GetOwner());
				}
			}
		}
	}

	if (CandidateActors.Num() == 0)
	{
		return 0;
	}

	const FString ProjectFilter = SurfProjectInput.IsValid() ? SurfProjectInput->GetText().ToString().TrimStartAndEnd() : TEXT("");

	auto MatchesProject = [&ProjectFilter](const FRshipMappingSurfaceState& Surface) -> bool
	{
		if (ProjectFilter.IsEmpty())
		{
			return Surface.ProjectId.IsEmpty();
		}
		return Surface.ProjectId == ProjectFilter;
	};

	auto FindExistingSurfaceId = [&ExistingSurfaces, &MatchesProject](const FString& ActorPath, const FString& MeshName) -> FString
	{
		if (!ActorPath.IsEmpty())
		{
			for (const FRshipMappingSurfaceState& Surface : ExistingSurfaces)
			{
				if (!MatchesProject(Surface))
				{
					continue;
				}
				if (Surface.ActorPath.Equals(ActorPath, ESearchCase::CaseSensitive))
				{
					return Surface.Id;
				}
			}
		}

		if (!MeshName.IsEmpty())
		{
			FString UniqueMatchId;
			int32 MatchCount = 0;
			for (const FRshipMappingSurfaceState& Surface : ExistingSurfaces)
			{
				if (!MatchesProject(Surface))
				{
					continue;
				}
				if (Surface.MeshComponentName.Equals(MeshName, ESearchCase::IgnoreCase))
				{
					++MatchCount;
					UniqueMatchId = Surface.Id;
					if (MatchCount > 1)
					{
						return TEXT("");
					}
				}
			}
			if (MatchCount == 1)
			{
				return UniqueMatchId;
			}
		}

		return TEXT("");
	};

	for (AActor* Actor : CandidateActors)
	{
		if (!ActorIsValidScreenCandidate(Actor))
		{
			continue;
		}

		TArray<UMeshComponent*> MeshComponents;
		Actor->GetComponents(MeshComponents);
		if (MeshComponents.Num() == 0)
		{
			continue;
		}

		UMeshComponent* Mesh = MeshComponents[0];
		if (!Mesh)
		{
			continue;
		}

		const FString MeshName = Mesh->GetName();
		if (MeshName.IsEmpty())
		{
			continue;
		}
		const FString ActorLabel = Actor->GetActorLabel();
		const FString BaseName = ActorLabel.IsEmpty() ? Actor->GetName() : ActorLabel;
		const FString ActorPath = Actor->GetPathName();

		const FString ExistingSurfaceId = FindExistingSurfaceId(ActorPath, MeshName);
		if (!ExistingSurfaceId.IsEmpty())
		{
			SelectedSurfaceRows.Add(ExistingSurfaceId);
			SelectedSurfaceId = ExistingSurfaceId;
			bSelectionChanged = true;
			continue;
		}

		FRshipMappingSurfaceState NewSurface;
		NewSurface.Name = (MeshComponents.Num() > 1)
			? FString::Printf(TEXT("%s / %s"), *BaseName, *MeshName)
			: BaseName;
		NewSurface.ProjectId = ProjectFilter;
		NewSurface.TargetId.Reset();
		NewSurface.UVChannel = SurfUVInput.IsValid() ? SurfUVInput->GetValue() : 0;
		NewSurface.MeshComponentName = MeshName;
		NewSurface.ActorPath = ActorPath;
		NewSurface.bEnabled = !SurfEnabledInput.IsValid() || SurfEnabledInput->IsChecked();

		FString NewSurfaceId = Manager->CreateMappingSurface(NewSurface);
		if (!NewSurfaceId.IsEmpty())
		{
			SelectedSurfaceRows.Add(NewSurfaceId);
			SelectedSurfaceId = NewSurfaceId;
			CreatedCount++;
			bSelectionChanged = true;

			FRshipMappingSurfaceState CreatedState = NewSurface;
			CreatedState.Id = NewSurfaceId;
			ExistingSurfaces.Add(CreatedState);
		}
	}

	if (CreatedCount > 0 || bSelectionChanged)
	{
		bHasListHash = false;
		bHasPendingListHash = false;
		RefreshStatus();
	}

	return CreatedCount;
#else
	return 0;
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
		bHasLiveMappingFormHash = false;
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
	bHasLiveMappingFormHash = false;
}

void SRshipContentMappingPanel::OpenMappingEditorWindow(const FRshipContentMappingState& Mapping)
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	if (!MappingEditorWindow.IsValid())
	{
		TSharedRef<SWindow> NewWindow = SNew(SWindow)
			.Title(LOCTEXT("MappingEditorWindowTitle", "Mapping Edit Mode"))
			.ClientSize(FVector2D(920.0f, 780.0f))
			.SupportsMaximize(true)
			.SupportsMinimize(false)
			[
				SNew(SBorder)
				.Padding(8.0f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("MappingEditorWindowHint", "Edit mapping parameters here. List-row fields remain inline editable."))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
						[
							SAssignNew(PreviewBorder, SBorder)
							.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
							.Padding(6.0f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
								[
									SAssignNew(PreviewLabel, STextBlock)
									.Text(LOCTEXT("MappingEditorPreviewDefault", "Preview unavailable"))
									.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									SAssignNew(PreviewImage, SImage)
									.Image(FAppStyle::GetBrush("WhiteBrush"))
								]
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
						[
							BuildContextForm()
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							BuildMappingForm()
						]
					]
				]
			];

		NewWindow->SetOnWindowClosed(FOnWindowClosed::CreateLambda([this](const TSharedRef<SWindow>&)
		{
			MappingEditorWindow.Reset();
			StopProjectionEdit();
		}));

		MappingEditorWindow = NewWindow;
		FSlateApplication::Get().AddWindow(NewWindow);
	}
	else
	{
		MappingEditorWindow->BringToFront();
	}

	FRshipContentMappingState MappingToEdit = Mapping;
	if (GEngine)
	{
		if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
		{
			if (URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager())
			{
				const TArray<FRshipContentMappingState> ExistingMappings = Manager->GetMappings();
				for (const FRshipContentMappingState& ExistingMapping : ExistingMappings)
				{
					if (ExistingMapping.Id == Mapping.Id)
					{
						MappingToEdit = ExistingMapping;
						break;
					}
				}
			}
		}
	}

	SetSelectedMappingId(MappingToEdit.Id);
	LastPreviewMappingId = MappingToEdit.Id;
	PopulateMappingForm(MappingToEdit);
	RefreshStatus();

	if (IsProjectionMode(GetMappingModeFromState(MappingToEdit)))
	{
		StartProjectionEdit(MappingToEdit);
	}
	else
	{
		StopProjectionEdit();
	}
}

void SRshipContentMappingPanel::CloseMappingEditorWindow()
{
	if (MappingEditorWindow.IsValid())
	{
		if (FSlateApplication::IsInitialized())
		{
			MappingEditorWindow->RequestDestroyWindow();
		}
		MappingEditorWindow.Reset();
	}
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
				if (Parts.Contains(SelectedId))
				{
					Parts.RemoveAll([&SelectedId](const FString& Part)
					{
						return Part.Equals(SelectedId, ESearchCase::CaseSensitive);
					});
				}
				else
				{
					Parts.Add(SelectedId);
				}
				TargetInput->SetText(FText::FromString(FString::Join(Parts, TEXT(", "))));
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
				Opt->Label = DisplayName.IsEmpty() ? TEXT("(Unnamed target)") : DisplayName;
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
				Opt->Label = DisplayTextOrDefault(Cam.Name, TEXT("(Unnamed camera)"));
				CameraOptions.Add(Opt);
				ExistingCameraIds.Add(Cam.Id);
			}
		}
	}

	UWorld* World = GetEditorWorld();
	if (World)
	{
		TSet<const AActor*> ExistingTargetActors;
		for (const TSharedPtr<FRshipIdOption>& ExistingTarget : TargetOptions)
		{
			if (ExistingTarget.IsValid() && ExistingTarget->Actor.IsValid())
			{
				ExistingTargetActors.Add(ExistingTarget->Actor.Get());
			}
		}

		const FString ServiceId = Subsystem ? Subsystem->GetServiceId() : TEXT("");
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor || ExistingTargetActors.Contains(Actor))
			{
				continue;
			}

			URshipTargetComponent* TargetComp = Actor->FindComponentByClass<URshipTargetComponent>();
			if (!TargetComp)
			{
				continue;
			}

			if (TargetComp->TargetData && Subsystem && !ServiceId.IsEmpty())
			{
				const FString ExpectedFullId = ServiceId + TEXT(":") + TargetComp->targetName;
				if (!Subsystem->FindTargetComponent(ExpectedFullId))
				{
					Subsystem->RegisterTargetComponent(TargetComp);
				}
			}

			TSharedPtr<FRshipIdOption> DiscoveredOpt = MakeShared<FRshipIdOption>();
			DiscoveredOpt->Actor = Actor;
			DiscoveredOpt->Id = TargetComp->targetName.IsEmpty() ? Actor->GetName() : TargetComp->targetName;
			DiscoveredOpt->ResolvedId = ServiceId.IsEmpty() ? DiscoveredOpt->Id : ServiceId + TEXT(":") + DiscoveredOpt->Id;
			DiscoveredOpt->Label = DisplayTextOrDefault(Actor->GetActorLabel(), TEXT("(Unnamed target)"));
			TargetOptions.Add(DiscoveredOpt);
			ExistingTargetActors.Add(Actor);
		}

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor || ExistingTargetActors.Contains(Actor))
			{
				continue;
			}

			TArray<UMeshComponent*> MeshComponents;
			Actor->GetComponents(MeshComponents);
			if (MeshComponents.Num() == 0)
			{
				continue;
			}

			TSharedPtr<FRshipIdOption> MeshTargetOpt = MakeShared<FRshipIdOption>();
			const FString ActorLabel = Actor->GetActorLabel();
			MeshTargetOpt->Actor = Actor;
			MeshTargetOpt->Id = ActorLabel.IsEmpty() ? Actor->GetName() : ActorLabel;
			MeshTargetOpt->ResolvedId = TEXT("");
			MeshTargetOpt->Label = FString::Printf(TEXT("%s (add target)"), *DisplayTextOrDefault(ActorLabel, TEXT("Mesh Actor")));
			TargetOptions.Add(MeshTargetOpt);
			ExistingTargetActors.Add(Actor);
		}

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
			Opt->Label = DisplayTextOrDefault(Ctx.Name, TEXT("(Unnamed input)"));
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
			const FString LabelSurface = DisplayTextOrDefault(Surface.Name, TEXT("(Unnamed screen)"));
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

	if (!bSuspendLiveMappingSync
		&& !SelectedMappingId.IsEmpty()
		&& MapNameInput.IsValid()
		&& MapModeSelector.IsValid())
	{
		const uint32 CurrentHash = ComputeMappingFormLiveHash();
		if (!bHasLiveMappingFormHash || CurrentHash != LastLiveMappingFormHash)
		{
			if (ApplyCurrentFormToSelectedMapping(false))
			{
				LastLiveMappingFormHash = CurrentHash;
				bHasLiveMappingFormHash = true;
			}
		}
	}
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
	FString SourceId = QuickSourceIdInput.IsValid() ? QuickSourceIdInput->GetText().ToString().TrimStartAndEnd() : TEXT("");
	FString ScreenInput = QuickTargetIdInput.IsValid() ? QuickTargetIdInput->GetText().ToString().TrimStartAndEnd() : TEXT("");
	const TArray<FRshipMappingSurfaceState> Surfaces = Manager->GetMappingSurfaces();
	TMap<FString, const FRshipMappingSurfaceState*> SurfacesById;
	for (const FRshipMappingSurfaceState& Surface : Surfaces)
	{
		if (!Surface.Id.IsEmpty())
		{
			SurfacesById.Add(Surface.Id, &Surface);
		}
	}

	const int32 Width = bQuickAdvanced && QuickWidthInput.IsValid() ? QuickWidthInput->GetValue() : 1920;
	const int32 Height = bQuickAdvanced && QuickHeightInput.IsValid() ? QuickHeightInput->GetValue() : 1080;
	const FString CaptureMode = bQuickAdvanced && QuickCaptureModeInput.IsValid() ? QuickCaptureModeInput->GetText().ToString().TrimStartAndEnd() : TEXT("");
	const float Opacity = QuickOpacityInput.IsValid() ? QuickOpacityInput->GetValue() : 1.0f;

#if WITH_EDITOR
	// If fields are empty, infer from current level selection for the happy-path workflow.
	if (GEditor)
	{
		USelection* Selection = GEditor->GetSelectedActors();
		if (Selection)
		{
			if (SourceId.IsEmpty())
			{
				for (FSelectionIterator It(*Selection); It; ++It)
				{
					AActor* Actor = Cast<AActor>(*It);
					if (!Actor)
					{
						continue;
					}
					const FString SelectedCameraId = ResolveCameraIdForActor(Actor);
					if (!SelectedCameraId.IsEmpty())
					{
						SourceId = SelectedCameraId;
						if (QuickSourceIdInput.IsValid())
						{
							QuickSourceIdInput->SetText(FText::FromString(SourceId));
						}
						break;
					}
				}
			}

				if (ScreenInput.IsEmpty())
				{
					TArray<FString> SelectedScreenIds;
					for (FSelectionIterator It(*Selection); It; ++It)
					{
						AActor* Actor = Cast<AActor>(*It);
						if (!ActorIsValidScreenCandidate(Actor))
						{
							continue;
						}

						FString SelectedScreenId;
						for (const TSharedPtr<FRshipIdOption>& Option : SurfaceOptions)
						{
							if (Option.IsValid() && Option->Actor.Get() == Actor)
							{
								SelectedScreenId = Option->Id;
								break;
							}
						}

						if (SelectedScreenId.IsEmpty())
						{
							const FString SelectedActorPath = Actor->GetPathName();
							if (!SelectedActorPath.IsEmpty())
							{
								for (const FRshipMappingSurfaceState& Surface : Surfaces)
								{
									if (Surface.ActorPath.Equals(SelectedActorPath, ESearchCase::CaseSensitive))
									{
										SelectedScreenId = Surface.Id;
										break;
									}
								}
							}
						}

						if (!SelectedScreenId.IsEmpty())
						{
							SelectedScreenIds.AddUnique(SelectedScreenId);
						}
					}
					if (SelectedScreenIds.Num() > 0)
					{
						ScreenInput = FString::Join(SelectedScreenIds, TEXT(","));
						if (QuickTargetIdInput.IsValid())
					{
						QuickTargetIdInput->SetText(FText::FromString(ScreenInput));
					}
				}
			}
		}
	}
#endif

	if (SourceId.IsEmpty())
	{
		if (PreviewLabel.IsValid())
		{
			PreviewLabel->SetText(LOCTEXT("QuickNeedsSource", "Select an input source first (camera or asset)."));
			PreviewLabel->SetColorAndOpacity(FLinearColor::Yellow);
		}
		return false;
	}

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

	const TArray<FString> ScreenTokens = ParseTokenList(ScreenInput);
	TArray<FString> SurfaceIds;
	TArray<FString> SurfaceLabels;
	SurfaceIds.Reserve(ScreenTokens.Num());
	SurfaceLabels.Reserve(ScreenTokens.Num());
	TSet<FString> AddedSurfaceIds;
	TArray<FString> MissingScreens;

	for (const FString& RawToken : ScreenTokens)
	{
		const FString ResolvedScreenId = ResolveScreenIdInput(RawToken);

		const FRshipMappingSurfaceState* FoundSurface = nullptr;

		if (const FRshipMappingSurfaceState** Surface = SurfacesById.Find(ResolvedScreenId))
		{
			FoundSurface = *Surface;
		}

		if (FoundSurface != nullptr && !FoundSurface->Id.IsEmpty())
		{
			if (!AddedSurfaceIds.Contains(FoundSurface->Id))
			{
				AddedSurfaceIds.Add(FoundSurface->Id);
				SurfaceIds.Add(FoundSurface->Id);
				SurfaceLabels.Add(DisplayTextOrDefault(FoundSurface->Name, TEXT("(Unnamed screen)")));
			}
		}
		else
		{
			MissingScreens.Add(RawToken);
		}
	}

	const TArray<FString> SortedSurfaceIds = NormalizeSurfaceIds(SurfaceIds);
	if (ScreenTokens.Num() == 0 || SortedSurfaceIds.Num() == 0)
	{
		if (PreviewLabel.IsValid())
		{
			if (MissingScreens.Num() > 0)
			{
				PreviewLabel->SetText(FText::FromString(FString::Printf(
					TEXT("Define screens first. Missing: %s"),
					*FString::Join(MissingScreens, TEXT(", ")))));
			}
			else
			{
				PreviewLabel->SetText(LOCTEXT("QuickNeedsScreens", "Define screens first, then select at least one screen."));
			}
			PreviewLabel->SetColorAndOpacity(FLinearColor::Yellow);
		}
		return false;
	}

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

					const int32 FeedWidth = FMath::Max(1, Width);
					const int32 FeedHeight = FMath::Max(1, Height);
					TSharedPtr<FJsonObject> FeedV2 = MakeShared<FJsonObject>();
					FeedV2->SetStringField(TEXT("coordinateSpace"), TEXT("pixel"));

					TArray<TSharedPtr<FJsonValue>> FeedSources;
					{
						TSharedPtr<FJsonObject> SourceObj = MakeShared<FJsonObject>();
						SourceObj->SetStringField(TEXT("id"), TEXT("source-1"));
						SourceObj->SetStringField(TEXT("label"), TEXT("Source 1"));
						SourceObj->SetStringField(TEXT("contextId"), ContextId);
						SourceObj->SetNumberField(TEXT("width"), FeedWidth);
						SourceObj->SetNumberField(TEXT("height"), FeedHeight);
						FeedSources.Add(MakeShared<FJsonValueObject>(SourceObj));
					}
					FeedV2->SetArrayField(TEXT("sources"), FeedSources);

					TArray<TSharedPtr<FJsonValue>> FeedDestinations;
					TArray<TSharedPtr<FJsonValue>> FeedRoutes;
					for (int32 SurfaceIndex = 0; SurfaceIndex < SortedSurfaceIds.Num(); ++SurfaceIndex)
					{
						const FString DestinationId = FString::Printf(TEXT("dest-%d"), SurfaceIndex + 1);
						const FString RouteId = FString::Printf(TEXT("route-%d"), SurfaceIndex + 1);

						TSharedPtr<FJsonObject> DestinationObj = MakeShared<FJsonObject>();
						DestinationObj->SetStringField(TEXT("id"), DestinationId);
						DestinationObj->SetStringField(TEXT("surfaceId"), SortedSurfaceIds[SurfaceIndex]);
						DestinationObj->SetStringField(
							TEXT("label"),
							(SurfaceLabels.IsValidIndex(SurfaceIndex) && !SurfaceLabels[SurfaceIndex].IsEmpty())
								? SurfaceLabels[SurfaceIndex]
								: FString::Printf(TEXT("Destination %d"), SurfaceIndex + 1));
						DestinationObj->SetNumberField(TEXT("width"), FeedWidth);
						DestinationObj->SetNumberField(TEXT("height"), FeedHeight);
						FeedDestinations.Add(MakeShared<FJsonValueObject>(DestinationObj));

						TSharedPtr<FJsonObject> RouteObj = MakeShared<FJsonObject>();
						RouteObj->SetStringField(TEXT("id"), RouteId);
						RouteObj->SetStringField(TEXT("sourceId"), TEXT("source-1"));
						RouteObj->SetStringField(TEXT("destinationId"), DestinationId);
						RouteObj->SetBoolField(TEXT("enabled"), true);
						RouteObj->SetNumberField(TEXT("opacity"), 1.0);

						TSharedPtr<FJsonObject> SrcRect = MakeShared<FJsonObject>();
						SrcRect->SetNumberField(TEXT("x"), 0);
						SrcRect->SetNumberField(TEXT("y"), 0);
						SrcRect->SetNumberField(TEXT("w"), FeedWidth);
						SrcRect->SetNumberField(TEXT("h"), FeedHeight);
						RouteObj->SetObjectField(TEXT("sourceRect"), SrcRect);

						TSharedPtr<FJsonObject> DstRect = MakeShared<FJsonObject>();
						DstRect->SetNumberField(TEXT("x"), 0);
						DstRect->SetNumberField(TEXT("y"), 0);
						DstRect->SetNumberField(TEXT("w"), FeedWidth);
						DstRect->SetNumberField(TEXT("h"), FeedHeight);
						RouteObj->SetObjectField(TEXT("destinationRect"), DstRect);

						FeedRoutes.Add(MakeShared<FJsonValueObject>(RouteObj));
					}
					FeedV2->SetArrayField(TEXT("destinations"), FeedDestinations);
					FeedV2->SetArrayField(TEXT("routes"), FeedRoutes);
					NewMapping.Config->SetObjectField(TEXT("feedV2"), FeedV2);
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
	Manager->Tick(0.0f);

	FString MappingError;
	FString SurfaceError;
	{
		const TArray<FRshipContentMappingState> UpdatedMappings = Manager->GetMappings();
		for (const FRshipContentMappingState& Mapping : UpdatedMappings)
		{
			if (Mapping.Id == MappingId)
			{
				MappingError = Mapping.LastError;
				break;
			}
		}

		if (MappingError.IsEmpty())
		{
			const TArray<FRshipMappingSurfaceState> UpdatedSurfaces = Manager->GetMappingSurfaces();
			TSet<FString> AssignedSurfaceIds;
			for (const FRshipContentMappingState& Mapping : UpdatedMappings)
			{
				if (Mapping.Id == MappingId)
				{
					for (const FString& SurfaceId : Mapping.SurfaceIds)
					{
						AssignedSurfaceIds.Add(SurfaceId);
					}
					break;
				}
			}
			for (const FRshipMappingSurfaceState& Surface : UpdatedSurfaces)
			{
				if (AssignedSurfaceIds.Contains(Surface.Id) && !Surface.LastError.IsEmpty())
				{
					SurfaceError = Surface.LastError;
					break;
				}
			}
		}
	}

	if (PreviewLabel.IsValid())
	{
		if (!MappingError.IsEmpty())
		{
			PreviewLabel->SetText(FText::FromString(FString::Printf(TEXT("Mapping created but not applied: %s"), *MappingError)));
			PreviewLabel->SetColorAndOpacity(FLinearColor::Yellow);
		}
		else if (!SurfaceError.IsEmpty())
		{
			PreviewLabel->SetText(FText::FromString(FString::Printf(TEXT("Screen setup error: %s"), *SurfaceError)));
			PreviewLabel->SetColorAndOpacity(FLinearColor::Yellow);
		}
		else
		{
			PreviewLabel->SetText(LOCTEXT("QuickCreated", "Mapping created and applied."));
			PreviewLabel->SetColorAndOpacity(FLinearColor::White);
		}
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
						.Text(LOCTEXT("QuickNote", "Optional shortcut for one-step create. Primary workflow is blank mappings edited in the Mappings table."))
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
						.HintText(LOCTEXT("QuickTargetHint", "Existing screen ids (comma-separated)"))
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
								return LOCTEXT("QuickPickTarget", "Add Existing Screen");
							})
						]
					]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,6,0)
						[
							SNew(SButton)
							.Text(LOCTEXT("QuickUseSelectedTarget", "Add Selected Screen"))
						.OnClicked_Lambda([this]()
						{
							const bool bOk = TryApplySelectionToTarget(QuickTargetIdInput, true);
							if (!bOk && PreviewLabel.IsValid())
							{
								PreviewLabel->SetText(LOCTEXT("QuickSelectTargetFail", "Select actor(s) that already exist in the Screens list."));
								PreviewLabel->SetColorAndOpacity(FLinearColor::Yellow);
							}
								return FReply::Handled();
							})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SNew(SButton)
						.Text(LOCTEXT("QuickRemoveLastTarget", "Remove Last"))
						.IsEnabled_Lambda([this]()
						{
							if (!QuickTargetIdInput.IsValid())
							{
								return false;
							}
							const FString Current = QuickTargetIdInput->GetText().ToString().TrimStartAndEnd();
							return !Current.IsEmpty();
						})
						.OnClicked_Lambda([this]()
						{
							if (!QuickTargetIdInput.IsValid())
							{
								return FReply::Handled();
							}
							TArray<FString> Parts;
							QuickTargetIdInput->GetText().ToString().ParseIntoArray(Parts, TEXT(","), true);
							for (FString& Part : Parts)
							{
								Part = Part.TrimStartAndEnd();
							}
							Parts.RemoveAll([](const FString& Part) { return Part.IsEmpty(); });
							if (Parts.Num() > 0)
							{
								Parts.RemoveAt(Parts.Num() - 1);
								QuickTargetIdInput->SetText(FText::FromString(FString::Join(Parts, TEXT(","))));
							}
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,6,0)
					[
						SNew(SButton)
						.Text(LOCTEXT("QuickClearTargets", "Clear"))
						.IsEnabled_Lambda([this]()
						{
							if (!QuickTargetIdInput.IsValid())
							{
								return false;
							}
							return !QuickTargetIdInput->GetText().ToString().TrimStartAndEnd().IsEmpty();
						})
						.OnClicked_Lambda([this]()
						{
							if (QuickTargetIdInput.IsValid())
							{
								QuickTargetIdInput->SetText(FText::GetEmpty());
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
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MappingsTitle", "Mappings"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.ContentPadding(CompactMappingButtonPadding)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MapCreateBlankAlways", "Create"))
					.Font(CompactMappingButtonFont)
				]
				.OnClicked_Lambda([this]()
				{
					if (!GEngine) return FReply::Handled();
					URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
					if (!Subsystem) return FReply::Handled();
					URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
					if (!Manager) return FReply::Handled();

					FRshipContentMappingState NewMapping;
					NewMapping.Name = TEXT("Mapping");
					NewMapping.Opacity = 1.0f;
					NewMapping.bEnabled = true;
					ApplyModeToMappingState(NewMapping, MapModeDirect);

					const FString NewId = Manager->CreateMapping(NewMapping);
					if (!NewId.IsEmpty())
					{
						Manager->Tick(0.0f);
						SetSelectedMappingId(NewId);
						SelectedMappingRows.Empty();
						SelectedMappingRows.Add(NewId);
					}
					RefreshStatus();
					return FReply::Handled();
				})
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(MappingList, SVerticalBox)
			]
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
						RefreshMappingCanvasFeedRects();
						ApplyCurrentFormToSelectedMapping(false);
					})
			]

				+ SVerticalBox::Slot().AutoHeight().Padding(0,4,0,2)
				[
					SNew(SVerticalBox)
					.Visibility(EVisibility::Collapsed)
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
							ApplyCurrentFormToSelectedMapping(false);
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
							ApplyCurrentFormToSelectedMapping(false);
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
							ApplyCurrentFormToSelectedMapping(false);
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
							ApplyCurrentFormToSelectedMapping(false);
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
							ApplyCurrentFormToSelectedMapping(false);
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
					SNew(SBox)
					.Visibility(EVisibility::Collapsed)
					[
						SNew(STextBlock).Text(LOCTEXT("MapFeedHeader", "Feed Rect"))
					]
				]
					+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
					[
						SNew(SBox)
						.Visibility(EVisibility::Collapsed)
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
							RefreshMappingCanvasFeedRects();
							ApplyCurrentFormToSelectedMapping(false);
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
							RefreshMappingCanvasFeedRects();
							ApplyCurrentFormToSelectedMapping(false);
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
							RefreshMappingCanvasFeedRects();
							ApplyCurrentFormToSelectedMapping(false);
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
							RefreshMappingCanvasFeedRects();
							ApplyCurrentFormToSelectedMapping(false);
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
								ApplyCurrentFormToSelectedMapping(false);
								return FReply::Handled();
							})
						]
						]
				]
					+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
					[
						SNew(SBox)
						.Visibility(EVisibility::Collapsed)
						[
							SNew(STextBlock).Text(LOCTEXT("MapFeedOverrides", "Screen Overrides"))
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
					[
						SNew(SBox)
						.Visibility(EVisibility::Collapsed)
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
									ApplyCurrentFormToSelectedMapping(false);
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
									ApplyCurrentFormToSelectedMapping(false);
									return FReply::Handled();
								})
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox)
						.Visibility(EVisibility::Collapsed)
						[
							SAssignNew(MapFeedRectList, SVerticalBox)
						]
					]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)
						[
							SNew(SBox)
							.Visibility(EVisibility::Collapsed)
							[
								SAssignNew(MappingCanvas, SRshipMappingCanvas)
						.DesiredHeight(260.0f)
						.OnFeedRectChanged_Lambda([this](const FString& SurfaceId, float U, float V, float W, float H)
						{
							if (!SurfaceId.IsEmpty())
							{
								ActiveFeedSurfaceId = SurfaceId;
								FFeedRect& Rect = MapFeedRectOverrides.FindOrAdd(SurfaceId);
								Rect.U = U;
								Rect.V = V;
								Rect.W = W;
								Rect.H = H;
							}
							else
							{
								if (MapFeedUInput.IsValid()) MapFeedUInput->SetValue(U);
								if (MapFeedVInput.IsValid()) MapFeedVInput->SetValue(V);
								if (MapFeedWInput.IsValid()) MapFeedWInput->SetValue(W);
								if (MapFeedHInput.IsValid()) MapFeedHInput->SetValue(H);
							}
							RebuildFeedRectList();
							ApplyCurrentFormToSelectedMapping(false);
						})
						.OnFeedRectSelectionChanged_Lambda([this](const FString& SurfaceId)
						{
							if (!SurfaceId.IsEmpty())
							{
								ActiveFeedSurfaceId = SurfaceId;
							}
							RebuildFeedRectList();
						})
						.OnUvTransformChanged_Lambda([this](float ScaleU, float ScaleV, float OffsetU, float OffsetV, float RotDeg)
						{
							if (MapUvScaleUInput.IsValid()) MapUvScaleUInput->SetValue(ScaleU);
							if (MapUvScaleVInput.IsValid()) MapUvScaleVInput->SetValue(ScaleV);
							if (MapUvOffsetUInput.IsValid()) MapUvOffsetUInput->SetValue(OffsetU);
							if (MapUvOffsetVInput.IsValid()) MapUvOffsetVInput->SetValue(OffsetV);
							if (MapUvRotInput.IsValid()) MapUvRotInput->SetValue(RotDeg);
							ApplyCurrentFormToSelectedMapping(false);
							})
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,8,0,2)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("FeedV2Header", "Feed Router (Pixel)"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
							[
								SNew(SButton)
								.Text(LOCTEXT("FeedV2AddSource", "Add Source"))
								.OnClicked_Lambda([this]()
								{
									const TArray<FString> SurfaceIds = GetCurrentMappingSurfaceIds();
									FFeedSourceV2 Source;
									Source.Id = FString::Printf(TEXT("source-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
									Source.ContextId = MapContextInput.IsValid() ? MapContextInput->GetText().ToString().TrimStartAndEnd() : TEXT("");
									Source.Width = 1920;
									Source.Height = 1080;
									if (GEngine && !Source.ContextId.IsEmpty())
									{
										if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
										{
											if (URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager())
											{
												const TArray<FRshipRenderContextState> Contexts = Manager->GetRenderContexts();
												for (const FRshipRenderContextState& Context : Contexts)
												{
													if (Context.Id == Source.ContextId)
													{
														if (Context.Width > 0) Source.Width = Context.Width;
														if (Context.Height > 0) Source.Height = Context.Height;
														break;
													}
												}
											}
										}
									}
									MapFeedSources.Add(Source);
									ActiveFeedSourceId = Source.Id;
									if (MapFeedDestinations.Num() == 0)
									{
										EnsureFeedDestinationsBoundToSurfaces(SurfaceIds);
										if (MapFeedDestinations.Num() > 0)
										{
											ActiveFeedDestinationId = MapFeedDestinations[0].Id;
										}
									}
									EnsureFeedRoutesForDestinations(SurfaceIds);
									RebuildFeedV2Lists();
									ApplyCurrentFormToSelectedMapping(false);
									return FReply::Handled();
								})
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
							[
								SNew(SButton)
								.Text(LOCTEXT("FeedV2AddDestination", "Add Destination"))
								.OnClicked_Lambda([this]()
								{
									const TArray<FString> SurfaceIds = GetCurrentMappingSurfaceIds();
									FFeedDestinationV2 Destination;
									Destination.Id = FString::Printf(TEXT("dest-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
									Destination.SurfaceId = ActiveFeedSurfaceId;
									if (Destination.SurfaceId.IsEmpty())
									{
										TSet<FString> UsedSurfaceIds;
										for (const FFeedDestinationV2& Existing : MapFeedDestinations)
										{
											const FString ExistingSurfaceId = Existing.SurfaceId.TrimStartAndEnd();
											if (!ExistingSurfaceId.IsEmpty())
											{
												UsedSurfaceIds.Add(ExistingSurfaceId);
											}
										}
										for (const FString& SurfaceId : SurfaceIds)
										{
											if (!UsedSurfaceIds.Contains(SurfaceId))
											{
												Destination.SurfaceId = SurfaceId;
												break;
											}
										}
										if (Destination.SurfaceId.IsEmpty() && SurfaceIds.Num() > 0)
										{
											Destination.SurfaceId = SurfaceIds[0];
										}
									}
									Destination.Width = 1920;
									Destination.Height = 1080;
									MapFeedDestinations.Add(Destination);
									ActiveFeedDestinationId = Destination.Id;
									EnsureFeedRoutesForDestinations(SurfaceIds);
									RebuildFeedV2Lists();
									ApplyCurrentFormToSelectedMapping(false);
									return FReply::Handled();
								})
							]
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SNew(SButton)
								.Text(LOCTEXT("FeedV2AddRectPair", "Add Rect Pair"))
								.OnClicked_Lambda([this]()
								{
									const TArray<FString> SurfaceIds = GetCurrentMappingSurfaceIds();
									if (MapFeedSources.Num() == 0)
									{
										FFeedSourceV2 Source;
										Source.Id = FString::Printf(TEXT("source-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
										Source.ContextId = MapContextInput.IsValid() ? MapContextInput->GetText().ToString().TrimStartAndEnd() : TEXT("");
										MapFeedSources.Add(Source);
									}
									if (MapFeedDestinations.Num() == 0)
									{
										EnsureFeedDestinationsBoundToSurfaces(SurfaceIds);
										if (MapFeedDestinations.Num() == 0)
										{
											FFeedDestinationV2 Destination;
											Destination.Id = FString::Printf(TEXT("dest-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
											Destination.SurfaceId = SurfaceIds.Num() > 0 ? SurfaceIds[0] : TEXT("");
											MapFeedDestinations.Add(Destination);
										}
									}

									FFeedRouteV2 Route;
									Route.Id = FString::Printf(TEXT("route-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
									Route.SourceId = (!ActiveFeedSourceId.IsEmpty() && FindFeedSourceById(ActiveFeedSourceId))
										? ActiveFeedSourceId
										: MapFeedSources[0].Id;
									Route.DestinationId = (!ActiveFeedDestinationId.IsEmpty() && FindFeedDestinationById(ActiveFeedDestinationId))
										? ActiveFeedDestinationId
										: MapFeedDestinations[0].Id;
									ActiveFeedSourceId = Route.SourceId;
									ActiveFeedDestinationId = Route.DestinationId;

									int32 SourceWidth = 1920;
									int32 SourceHeight = 1080;
									TryGetFeedSourceDimensions(Route.SourceId, SourceWidth, SourceHeight);
									int32 DestinationWidth = 1920;
									int32 DestinationHeight = 1080;
									TryGetFeedDestinationDimensions(Route.DestinationId, DestinationWidth, DestinationHeight);

									const int32 PairWidth = FMath::Max(1, FMath::Min(SourceWidth, DestinationWidth));
									const int32 PairHeight = FMath::Max(1, FMath::Min(SourceHeight, DestinationHeight));
									Route.SourceX = 0;
									Route.SourceY = 0;
									Route.SourceW = PairWidth;
									Route.SourceH = PairHeight;
									Route.DestinationX = 0;
									Route.DestinationY = 0;
									Route.DestinationW = PairWidth;
									Route.DestinationH = PairHeight;
									ClampFeedRouteToCanvas(Route);
									MapFeedRoutes.Add(Route);
									ActiveFeedRouteId = Route.Id;
									RebuildFeedV2Lists();
									ApplyCurrentFormToSelectedMapping(false);
									return FReply::Handled();
								})
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0,0,4,0)
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
								.Padding(2.0f)
								[
									SAssignNew(MapFeedSourceList, SVerticalBox)
								]
							]
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0,0,4,0)
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
								.Padding(2.0f)
								[
									SAssignNew(MapFeedDestinationList, SVerticalBox)
								]
							]
							+ SHorizontalBox::Slot().FillWidth(1.4f)
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
								.Padding(2.0f)
								[
									SAssignNew(MapFeedRouteList, SVerticalBox)
								]
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)
						[
							SNew(STextBlock).Text(LOCTEXT("FeedV2SourceCanvasLabel", "Source Rectangles"))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,2,0,0)
						[
							SNew(SScrollBox)
							.Orientation(Orient_Horizontal)
							+ SScrollBox::Slot()
							[
								SAssignNew(FeedSourceCanvas, SRshipMappingCanvas)
								.OnFeedRectChanged_Lambda([this](const FString& RouteId, float U, float V, float W, float H)
								{
									for (FFeedRouteV2& Route : MapFeedRoutes)
									{
										if (Route.Id != RouteId)
										{
											continue;
										}
										int32 SourceWidth = 1920;
										int32 SourceHeight = 1080;
										for (const FFeedSourceV2& Source : MapFeedSources)
										{
											if (Source.Id == Route.SourceId)
											{
												SourceWidth = FMath::Max(1, Source.Width);
												SourceHeight = FMath::Max(1, Source.Height);
												break;
											}
										}
										Route.SourceX = FMath::Clamp(FMath::RoundToInt(U), 0, SourceWidth - 1);
										Route.SourceY = FMath::Clamp(FMath::RoundToInt(V), 0, SourceHeight - 1);
										Route.SourceW = FMath::Clamp(FMath::RoundToInt(W), 1, SourceWidth - Route.SourceX);
										Route.SourceH = FMath::Clamp(FMath::RoundToInt(H), 1, SourceHeight - Route.SourceY);
										ClampFeedRouteToCanvas(Route);
										ActiveFeedRouteId = Route.Id;
										ActiveFeedSourceId = Route.SourceId;
										ActiveFeedDestinationId = Route.DestinationId;
										break;
									}
									RebuildFeedV2Lists();
									ApplyCurrentFormToSelectedMapping(false);
								})
								.OnFeedRectSelectionChanged_Lambda([this](const FString& RouteId)
								{
									for (const FFeedRouteV2& Route : MapFeedRoutes)
									{
										if (Route.Id == RouteId)
										{
											ActiveFeedRouteId = Route.Id;
											ActiveFeedSourceId = Route.SourceId;
											ActiveFeedDestinationId = Route.DestinationId;
											break;
										}
									}
									RebuildFeedV2Lists();
								})
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0,2,0,0)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
							[
								SNew(STextBlock).Text(LOCTEXT("FeedV2SourceRectEditLabel", "Src Rect (px)"))
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
							[
								SNew(SSpinBox<int32>)
								.MinValue(0).MaxValue(65535)
								.Value_Lambda([this]()
								{
									if (const FFeedRouteV2* Route = FindFeedRouteById(ActiveFeedRouteId))
									{
										return Route->SourceX;
									}
									return 0;
								})
								.OnValueChanged_Lambda([this](int32 NewValue)
								{
									if (FFeedRouteV2* Route = FindFeedRouteById(ActiveFeedRouteId))
									{
										Route->SourceX = NewValue;
										ClampFeedRouteToCanvas(*Route);
										RefreshFeedV2Canvases();
										if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
									}
								})
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
							[
								SNew(SSpinBox<int32>)
								.MinValue(0).MaxValue(65535)
								.Value_Lambda([this]()
								{
									if (const FFeedRouteV2* Route = FindFeedRouteById(ActiveFeedRouteId))
									{
										return Route->SourceY;
									}
									return 0;
								})
								.OnValueChanged_Lambda([this](int32 NewValue)
								{
									if (FFeedRouteV2* Route = FindFeedRouteById(ActiveFeedRouteId))
									{
										Route->SourceY = NewValue;
										ClampFeedRouteToCanvas(*Route);
										RefreshFeedV2Canvases();
										if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
									}
								})
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
							[
								SNew(SSpinBox<int32>)
								.MinValue(1).MaxValue(65535)
								.Value_Lambda([this]()
								{
									if (const FFeedRouteV2* Route = FindFeedRouteById(ActiveFeedRouteId))
									{
										return Route->SourceW;
									}
									return 1;
								})
								.OnValueChanged_Lambda([this](int32 NewValue)
								{
									if (FFeedRouteV2* Route = FindFeedRouteById(ActiveFeedRouteId))
									{
										Route->SourceW = FMath::Max(1, NewValue);
										ClampFeedRouteToCanvas(*Route);
										RefreshFeedV2Canvases();
										if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
									}
								})
							]
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SNew(SSpinBox<int32>)
								.MinValue(1).MaxValue(65535)
								.Value_Lambda([this]()
								{
									if (const FFeedRouteV2* Route = FindFeedRouteById(ActiveFeedRouteId))
									{
										return Route->SourceH;
									}
									return 1;
								})
								.OnValueChanged_Lambda([this](int32 NewValue)
								{
									if (FFeedRouteV2* Route = FindFeedRouteById(ActiveFeedRouteId))
									{
										Route->SourceH = FMath::Max(1, NewValue);
										ClampFeedRouteToCanvas(*Route);
										RefreshFeedV2Canvases();
										if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
									}
								})
							]
						]
							+ SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)
							[
								SNew(STextBlock).Text(LOCTEXT("FeedV2DestinationCanvasLabel", "Destination Rectangles"))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0,2,0,0)
							[
								SAssignNew(FeedDestinationCanvasList, SVerticalBox)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0,2,0,0)
							[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)
							[
								SNew(STextBlock).Text(LOCTEXT("FeedV2DestinationRectEditLabel", "Dst Rect (px)"))
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
							[
								SNew(SSpinBox<int32>)
								.MinValue(0).MaxValue(65535)
								.Value_Lambda([this]()
								{
									if (const FFeedRouteV2* Route = FindFeedRouteById(ActiveFeedRouteId))
									{
										return Route->DestinationX;
									}
									return 0;
								})
								.OnValueChanged_Lambda([this](int32 NewValue)
								{
									if (FFeedRouteV2* Route = FindFeedRouteById(ActiveFeedRouteId))
									{
										Route->DestinationX = NewValue;
										ClampFeedRouteToCanvas(*Route);
										RefreshFeedV2Canvases();
										if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
									}
								})
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
							[
								SNew(SSpinBox<int32>)
								.MinValue(0).MaxValue(65535)
								.Value_Lambda([this]()
								{
									if (const FFeedRouteV2* Route = FindFeedRouteById(ActiveFeedRouteId))
									{
										return Route->DestinationY;
									}
									return 0;
								})
								.OnValueChanged_Lambda([this](int32 NewValue)
								{
									if (FFeedRouteV2* Route = FindFeedRouteById(ActiveFeedRouteId))
									{
										Route->DestinationY = NewValue;
										ClampFeedRouteToCanvas(*Route);
										RefreshFeedV2Canvases();
										if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
									}
								})
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
							[
								SNew(SSpinBox<int32>)
								.MinValue(1).MaxValue(65535)
								.Value_Lambda([this]()
								{
									if (const FFeedRouteV2* Route = FindFeedRouteById(ActiveFeedRouteId))
									{
										return Route->DestinationW;
									}
									return 1;
								})
								.OnValueChanged_Lambda([this](int32 NewValue)
								{
									if (FFeedRouteV2* Route = FindFeedRouteById(ActiveFeedRouteId))
									{
										Route->DestinationW = FMath::Max(1, NewValue);
										ClampFeedRouteToCanvas(*Route);
										RefreshFeedV2Canvases();
										if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
									}
								})
							]
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SNew(SSpinBox<int32>)
								.MinValue(1).MaxValue(65535)
								.Value_Lambda([this]()
								{
									if (const FFeedRouteV2* Route = FindFeedRouteById(ActiveFeedRouteId))
									{
										return Route->DestinationH;
									}
									return 1;
								})
								.OnValueChanged_Lambda([this](int32 NewValue)
								{
									if (FFeedRouteV2* Route = FindFeedRouteById(ActiveFeedRouteId))
									{
										Route->DestinationH = FMath::Max(1, NewValue);
										ClampFeedRouteToCanvas(*Route);
										RefreshFeedV2Canvases();
										if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
									}
								})
							]
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
					.Text(LOCTEXT("SurfCreateFromSelection", "Use Selected As Screen"))
					.OnClicked_Lambda([this]()
					{
						const int32 Created = CreateScreensFromSelectedActors();
						if (PreviewLabel.IsValid())
						{
							if (Created > 0)
							{
								PreviewLabel->SetText(FText::Format(
									LOCTEXT("SurfCreateFromSelectionCreatedFmt", "Created {0} screen(s) from selection."),
									FText::AsNumber(Created)));
							}
							else
							{
								PreviewLabel->SetText(LOCTEXT("SurfCreateFromSelectionNone", "No new screens created. Select mesh actor(s) in the level."));
							}
						}
						return FReply::Handled();
					})
				]
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
							State.TargetId.Reset();
							State.ActorPath = TargetInput.TrimStartAndEnd();
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
						ApplyCurrentFormToSelectedMapping(false);
					})
					.OnTextChanged_Lambda([this](const FText&)
					{
						RebuildFeedRectList();
						ApplyCurrentFormToSelectedMapping(false);
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
						if (ApplyCurrentFormToSelectedMapping(true))
						{
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
	bSuspendLiveMappingSync = true;
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
	ActiveFeedSurfaceId.Reset();
	ResetFeedV2State();
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
	RefreshMappingCanvasFeedRects();
	if (AngleMaskWidget.IsValid()) AngleMaskWidget->SetAngles(0.0f, 360.0f);
	if (ContentModeSelector.IsValid()) ContentModeSelector->SetSelectedMode(TEXT("stretch"));
	bHasLiveMappingFormHash = false;
	bSuspendLiveMappingSync = false;
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
	if (SurfTargetInput.IsValid()) SurfTargetInput->SetText(FText::FromString(State.ActorPath));
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
	bSuspendLiveMappingSync = true;
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
	ResetFeedV2State();
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
	PopulateFeedV2FromMapping(State);
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
	RefreshMappingCanvasFeedRects();
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
	LastLiveMappingFormHash = ComputeMappingFormLiveHash();
	bHasLiveMappingFormHash = true;
	bSuspendLiveMappingSync = false;
}

void SRshipContentMappingPanel::RefreshMappingCanvasFeedRects()
{
	// Legacy single-canvas feed preview has been removed from the active UI.
}

SRshipContentMappingPanel::FFeedSourceV2* SRshipContentMappingPanel::FindFeedSourceById(const FString& Id)
{
	for (FFeedSourceV2& Source : MapFeedSources)
	{
		if (Source.Id == Id)
		{
			return &Source;
		}
	}
	return nullptr;
}

SRshipContentMappingPanel::FFeedDestinationV2* SRshipContentMappingPanel::FindFeedDestinationById(const FString& Id)
{
	for (FFeedDestinationV2& Destination : MapFeedDestinations)
	{
		if (Destination.Id == Id)
		{
			return &Destination;
		}
	}
	return nullptr;
}

SRshipContentMappingPanel::FFeedRouteV2* SRshipContentMappingPanel::FindFeedRouteById(const FString& Id)
{
	for (FFeedRouteV2& Route : MapFeedRoutes)
	{
		if (Route.Id == Id)
		{
			return &Route;
		}
	}
	return nullptr;
}

bool SRshipContentMappingPanel::TryGetFeedSourceDimensions(const FString& SourceId, int32& OutWidth, int32& OutHeight) const
{
	for (const FFeedSourceV2& Source : MapFeedSources)
	{
		if (Source.Id == SourceId)
		{
			OutWidth = FMath::Max(1, Source.Width);
			OutHeight = FMath::Max(1, Source.Height);
			return true;
		}
	}

	OutWidth = 1920;
	OutHeight = 1080;
	return false;
}

bool SRshipContentMappingPanel::TryGetFeedDestinationDimensions(const FString& DestinationId, int32& OutWidth, int32& OutHeight) const
{
	for (const FFeedDestinationV2& Destination : MapFeedDestinations)
	{
		if (Destination.Id == DestinationId)
		{
			OutWidth = FMath::Max(1, Destination.Width);
			OutHeight = FMath::Max(1, Destination.Height);
			return true;
		}
	}

	OutWidth = 1920;
	OutHeight = 1080;
	return false;
}

TArray<FString> SRshipContentMappingPanel::GetCurrentMappingSurfaceIds() const
{
	TArray<FString> SurfaceIds;
	if (MapSurfacesInput.IsValid())
	{
		TArray<FString> Parts;
		MapSurfacesInput->GetText().ToString().ParseIntoArray(Parts, TEXT(","), true);
		for (FString& Part : Parts)
		{
			const FString SurfaceId = Part.TrimStartAndEnd();
			if (!SurfaceId.IsEmpty())
			{
				SurfaceIds.AddUnique(SurfaceId);
			}
		}
	}

	if (SurfaceIds.Num() == 0)
	{
		for (const FFeedDestinationV2& Destination : MapFeedDestinations)
		{
			const FString SurfaceId = Destination.SurfaceId.TrimStartAndEnd();
			if (!SurfaceId.IsEmpty() && !SurfaceId.Equals(TEXT("surface"), ESearchCase::IgnoreCase))
			{
				SurfaceIds.AddUnique(SurfaceId);
			}
		}
	}

	if (SurfaceIds.Num() == 0 && !ActiveFeedSurfaceId.IsEmpty())
	{
		SurfaceIds.Add(ActiveFeedSurfaceId);
	}

	return SurfaceIds;
}

void SRshipContentMappingPanel::ClampFeedRouteToCanvas(FFeedRouteV2& Route)
{
	const FString ContextId = MapContextInput.IsValid() ? MapContextInput->GetText().ToString().TrimStartAndEnd() : TEXT("");
	const TArray<FString> SurfaceIds = GetCurrentMappingSurfaceIds();

	if (Route.SourceId.IsEmpty())
	{
		EnsureFeedSourcesBoundToContext(ContextId);
		if (MapFeedSources.Num() == 0)
		{
			FFeedSourceV2 Source;
			Source.Id = FString::Printf(TEXT("source-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
			Source.ContextId = ContextId;
			Source.Width = 1920;
			Source.Height = 1080;
			MapFeedSources.Add(Source);
		}
		Route.SourceId = MapFeedSources[0].Id;
	}
	else if (!FindFeedSourceById(Route.SourceId) && MapFeedSources.Num() > 0)
	{
		Route.SourceId = MapFeedSources[0].Id;
	}

	if (Route.DestinationId.IsEmpty())
	{
		EnsureFeedDestinationsBoundToSurfaces(SurfaceIds);
		if (MapFeedDestinations.Num() == 0)
		{
			FFeedDestinationV2 Destination;
			Destination.Id = FString::Printf(TEXT("dest-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
			Destination.Width = 1920;
			Destination.Height = 1080;
			Destination.SurfaceId = SurfaceIds.Num() > 0 ? SurfaceIds[0] : TEXT("");
			MapFeedDestinations.Add(Destination);
		}
		Route.DestinationId = MapFeedDestinations[0].Id;
	}
	else if (!FindFeedDestinationById(Route.DestinationId) && MapFeedDestinations.Num() > 0)
	{
		Route.DestinationId = MapFeedDestinations[0].Id;
	}
	else if (!FindFeedDestinationById(Route.DestinationId))
	{
		EnsureFeedDestinationsBoundToSurfaces(SurfaceIds);
		if (MapFeedDestinations.Num() > 0)
		{
			Route.DestinationId = MapFeedDestinations[0].Id;
		}
	}

	int32 SourceWidth = 1920;
	int32 SourceHeight = 1080;
	TryGetFeedSourceDimensions(Route.SourceId, SourceWidth, SourceHeight);

	int32 DestinationWidth = 1920;
	int32 DestinationHeight = 1080;
	TryGetFeedDestinationDimensions(Route.DestinationId, DestinationWidth, DestinationHeight);

	Route.SourceX = FMath::Clamp(Route.SourceX, 0, SourceWidth - 1);
	Route.SourceY = FMath::Clamp(Route.SourceY, 0, SourceHeight - 1);
	Route.SourceW = FMath::Clamp(Route.SourceW, 1, SourceWidth - Route.SourceX);
	Route.SourceH = FMath::Clamp(Route.SourceH, 1, SourceHeight - Route.SourceY);

	Route.DestinationX = FMath::Clamp(Route.DestinationX, 0, DestinationWidth - 1);
	Route.DestinationY = FMath::Clamp(Route.DestinationY, 0, DestinationHeight - 1);
	Route.DestinationW = FMath::Clamp(Route.DestinationW, 1, DestinationWidth - Route.DestinationX);
	Route.DestinationH = FMath::Clamp(Route.DestinationH, 1, DestinationHeight - Route.DestinationY);
}

void SRshipContentMappingPanel::ClampAllFeedRoutesToCanvases()
{
	for (FFeedRouteV2& Route : MapFeedRoutes)
	{
		ClampFeedRouteToCanvas(Route);
	}
}

void SRshipContentMappingPanel::EnsureFeedSourcesBoundToContext(const FString& DefaultContextId)
{
	FString ResolvedDefault = DefaultContextId.TrimStartAndEnd();
	if (ResolvedDefault.IsEmpty() && GEngine)
	{
		if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
		{
			if (URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager())
			{
				const TArray<FRshipRenderContextState> Contexts = Manager->GetRenderContexts();
				for (const FRshipRenderContextState& Context : Contexts)
				{
					if (!Context.Id.IsEmpty() && Context.bEnabled && Context.ResolvedTexture)
					{
						ResolvedDefault = Context.Id;
						break;
					}
				}
				if (ResolvedDefault.IsEmpty())
				{
					for (const FRshipRenderContextState& Context : Contexts)
					{
						if (!Context.Id.IsEmpty() && Context.bEnabled)
						{
							ResolvedDefault = Context.Id;
							break;
						}
					}
				}
				if (ResolvedDefault.IsEmpty())
				{
					for (const FRshipRenderContextState& Context : Contexts)
					{
						if (!Context.Id.IsEmpty())
						{
							ResolvedDefault = Context.Id;
							break;
						}
					}
				}
			}
		}
	}

	if (MapFeedSources.Num() == 0 && !ResolvedDefault.IsEmpty())
	{
		FFeedSourceV2 Source;
		Source.Id = FString::Printf(TEXT("source-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
		Source.ContextId = ResolvedDefault;
		MapFeedSources.Add(Source);
	}

	if (ResolvedDefault.IsEmpty())
	{
		return;
	}

	for (FFeedSourceV2& Source : MapFeedSources)
	{
		if (Source.ContextId.TrimStartAndEnd().IsEmpty())
		{
			Source.ContextId = ResolvedDefault;
		}
	}
}

void SRshipContentMappingPanel::EnsureFeedDestinationsBoundToSurfaces(const TArray<FString>& MappingSurfaceIds)
{
	TArray<FString> ValidSurfaceIds = MappingSurfaceIds;
	for (FString& SurfaceId : ValidSurfaceIds)
	{
		SurfaceId = SurfaceId.TrimStartAndEnd();
	}
	ValidSurfaceIds.RemoveAll([](const FString& SurfaceId) { return SurfaceId.IsEmpty(); });
	if (ValidSurfaceIds.Num() == 0)
	{
		return;
	}
	TSet<FString> SeenSurfaceIds;
	TArray<FString> UniqueSurfaceIds;
	for (const FString& SurfaceId : ValidSurfaceIds)
	{
		if (!SeenSurfaceIds.Contains(SurfaceId))
		{
			SeenSurfaceIds.Add(SurfaceId);
			UniqueSurfaceIds.Add(SurfaceId);
		}
	}
	ValidSurfaceIds = MoveTemp(UniqueSurfaceIds);

	int32 SurfaceIdx = 0;
	for (FFeedDestinationV2& Destination : MapFeedDestinations)
	{
		const FString SurfaceId = Destination.SurfaceId.TrimStartAndEnd();
		const bool bInvalidSurface = SurfaceId.IsEmpty()
			|| SurfaceId.Equals(TEXT("surface"), ESearchCase::IgnoreCase)
			|| !ValidSurfaceIds.Contains(SurfaceId);
		if (bInvalidSurface)
		{
			Destination.SurfaceId = ValidSurfaceIds[SurfaceIdx % ValidSurfaceIds.Num()];
			++SurfaceIdx;
		}
	}

	TSet<FString> ExistingBoundSurfaces;
	for (const FFeedDestinationV2& Destination : MapFeedDestinations)
	{
		const FString SurfaceId = Destination.SurfaceId.TrimStartAndEnd();
		if (ValidSurfaceIds.Contains(SurfaceId))
		{
			ExistingBoundSurfaces.Add(SurfaceId);
		}
	}

	for (const FString& SurfaceId : ValidSurfaceIds)
	{
		if (ExistingBoundSurfaces.Contains(SurfaceId))
		{
			continue;
		}
		FFeedDestinationV2 Destination;
		Destination.Id = FString::Printf(TEXT("dest-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
		Destination.SurfaceId = SurfaceId;
		MapFeedDestinations.Add(Destination);
		ExistingBoundSurfaces.Add(SurfaceId);
	}
}

void SRshipContentMappingPanel::EnsureFeedRoutesForDestinations(const TArray<FString>& MappingSurfaceIds)
{
	if (MapFeedSources.Num() == 0 || MapFeedDestinations.Num() == 0)
	{
		return;
	}

	const FString DefaultSourceId = (!ActiveFeedSourceId.IsEmpty() && FindFeedSourceById(ActiveFeedSourceId))
		? ActiveFeedSourceId
		: MapFeedSources[0].Id;

	int32 SourceWidth = 1920;
	int32 SourceHeight = 1080;
	TryGetFeedSourceDimensions(DefaultSourceId, SourceWidth, SourceHeight);

	for (const FFeedDestinationV2& Destination : MapFeedDestinations)
	{
		const FString SurfaceId = Destination.SurfaceId.TrimStartAndEnd();
		if (MappingSurfaceIds.Num() > 0 && (SurfaceId.IsEmpty() || !MappingSurfaceIds.Contains(SurfaceId)))
		{
			continue;
		}

		const bool bHasRouteForDestination = MapFeedRoutes.ContainsByPredicate([&Destination](const FFeedRouteV2& ExistingRoute)
		{
			return ExistingRoute.DestinationId == Destination.Id;
		});
		if (bHasRouteForDestination)
		{
			continue;
		}

		FFeedRouteV2 Route;
		Route.Id = FString::Printf(TEXT("route-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
		Route.SourceId = DefaultSourceId;
		Route.DestinationId = Destination.Id;
		Route.SourceX = 0;
		Route.SourceY = 0;
		Route.SourceW = FMath::Max(1, SourceWidth);
		Route.SourceH = FMath::Max(1, SourceHeight);
		Route.DestinationX = 0;
		Route.DestinationY = 0;
		Route.DestinationW = FMath::Max(1, Destination.Width);
		Route.DestinationH = FMath::Max(1, Destination.Height);
		ClampFeedRouteToCanvas(Route);
		MapFeedRoutes.Add(Route);
	}

	if (MapFeedRoutes.Num() > 0 && ActiveFeedRouteId.IsEmpty())
	{
		ActiveFeedRouteId = MapFeedRoutes[0].Id;
		ActiveFeedSourceId = MapFeedRoutes[0].SourceId;
		ActiveFeedDestinationId = MapFeedRoutes[0].DestinationId;
	}
}

void SRshipContentMappingPanel::ResetFeedV2State()
{
	MapFeedSources.Empty();
	MapFeedDestinations.Empty();
	MapFeedRoutes.Empty();
	ActiveFeedSourceId.Reset();
	ActiveFeedDestinationId.Reset();
	ActiveFeedRouteId.Reset();
}

void SRshipContentMappingPanel::PopulateFeedV2FromMapping(const FRshipContentMappingState& State)
{
	ResetFeedV2State();
	if (!State.Config.IsValid() || !State.Config->HasTypedField<EJson::Object>(TEXT("feedV2")))
	{
		return;
	}

	const TSharedPtr<FJsonObject> FeedV2 = State.Config->GetObjectField(TEXT("feedV2"));
	if (!FeedV2.IsValid())
	{
		return;
	}

	auto GetNum = [](const TSharedPtr<FJsonObject>& Obj, const FString& Field, double DefaultVal) -> double
	{
		return (Obj.IsValid() && Obj->HasTypedField<EJson::Number>(Field)) ? Obj->GetNumberField(Field) : DefaultVal;
	};

	if (FeedV2->HasTypedField<EJson::Array>(TEXT("sources")))
	{
		const TArray<TSharedPtr<FJsonValue>> Sources = FeedV2->GetArrayField(TEXT("sources"));
		for (const TSharedPtr<FJsonValue>& Value : Sources)
		{
			if (!Value.IsValid() || Value->Type != EJson::Object)
			{
				continue;
			}
			const TSharedPtr<FJsonObject> SourceObj = Value->AsObject();
			if (!SourceObj.IsValid())
			{
				continue;
			}

			FFeedSourceV2 Source;
			Source.Id = SourceObj->HasTypedField<EJson::String>(TEXT("id")) ? SourceObj->GetStringField(TEXT("id")) : TEXT("");
			Source.Label = SourceObj->HasTypedField<EJson::String>(TEXT("label")) ? SourceObj->GetStringField(TEXT("label")) : TEXT("");
			Source.ContextId = SourceObj->HasTypedField<EJson::String>(TEXT("contextId")) ? SourceObj->GetStringField(TEXT("contextId")) : TEXT("");
			Source.Width = FMath::Max(1, static_cast<int32>(GetNum(SourceObj, TEXT("width"), 1920.0)));
			Source.Height = FMath::Max(1, static_cast<int32>(GetNum(SourceObj, TEXT("height"), 1080.0)));
			Source.Id = Source.Id.TrimStartAndEnd();
			Source.ContextId = Source.ContextId.TrimStartAndEnd();
			if (Source.Id.IsEmpty() && !Source.ContextId.IsEmpty())
			{
				Source.Id = Source.ContextId;
			}
			if (Source.ContextId.IsEmpty() && !Source.Id.IsEmpty())
			{
				Source.ContextId = Source.Id;
			}
			if (!Source.Id.IsEmpty())
			{
				MapFeedSources.Add(Source);
			}
		}
	}

	if (FeedV2->HasTypedField<EJson::Array>(TEXT("destinations")))
	{
		const TArray<TSharedPtr<FJsonValue>> Destinations = FeedV2->GetArrayField(TEXT("destinations"));
		for (const TSharedPtr<FJsonValue>& Value : Destinations)
		{
			if (!Value.IsValid() || Value->Type != EJson::Object)
			{
				continue;
			}
			const TSharedPtr<FJsonObject> DestinationObj = Value->AsObject();
			if (!DestinationObj.IsValid())
			{
				continue;
			}

			FFeedDestinationV2 Destination;
			Destination.Id = DestinationObj->HasTypedField<EJson::String>(TEXT("id")) ? DestinationObj->GetStringField(TEXT("id")) : TEXT("");
			Destination.Label = DestinationObj->HasTypedField<EJson::String>(TEXT("label")) ? DestinationObj->GetStringField(TEXT("label")) : TEXT("");
			Destination.SurfaceId = DestinationObj->HasTypedField<EJson::String>(TEXT("surfaceId")) ? DestinationObj->GetStringField(TEXT("surfaceId")) : TEXT("");
			Destination.Width = FMath::Max(1, static_cast<int32>(GetNum(DestinationObj, TEXT("width"), 1920.0)));
			Destination.Height = FMath::Max(1, static_cast<int32>(GetNum(DestinationObj, TEXT("height"), 1080.0)));
			Destination.Id = Destination.Id.TrimStartAndEnd();
			Destination.SurfaceId = Destination.SurfaceId.TrimStartAndEnd();
			if (Destination.Id.IsEmpty() && !Destination.SurfaceId.IsEmpty())
			{
				Destination.Id = Destination.SurfaceId;
			}
			if (Destination.SurfaceId.IsEmpty() && !Destination.Id.IsEmpty())
			{
				Destination.SurfaceId = Destination.Id;
			}
			if (!Destination.Id.IsEmpty())
			{
				MapFeedDestinations.Add(Destination);
			}
		}
	}

	const bool bHasRoutes = FeedV2->HasTypedField<EJson::Array>(TEXT("routes"));
	const bool bHasLinks = FeedV2->HasTypedField<EJson::Array>(TEXT("links"));
	if (bHasRoutes || bHasLinks)
	{
		const TArray<TSharedPtr<FJsonValue>>& Routes = bHasRoutes
			? FeedV2->GetArrayField(TEXT("routes"))
			: FeedV2->GetArrayField(TEXT("links"));
		for (const TSharedPtr<FJsonValue>& Value : Routes)
		{
			if (!Value.IsValid() || Value->Type != EJson::Object)
			{
				continue;
			}
			const TSharedPtr<FJsonObject> RouteObj = Value->AsObject();
			if (!RouteObj.IsValid())
			{
				continue;
			}

			FFeedRouteV2 Route;
			Route.Id = RouteObj->HasTypedField<EJson::String>(TEXT("id")) ? RouteObj->GetStringField(TEXT("id")) : TEXT("");
			Route.Label = RouteObj->HasTypedField<EJson::String>(TEXT("label")) ? RouteObj->GetStringField(TEXT("label")) : TEXT("");
			Route.SourceId = RouteObj->HasTypedField<EJson::String>(TEXT("sourceId")) ? RouteObj->GetStringField(TEXT("sourceId")) : TEXT("");
			Route.DestinationId = RouteObj->HasTypedField<EJson::String>(TEXT("destinationId")) ? RouteObj->GetStringField(TEXT("destinationId")) : TEXT("");
			Route.bEnabled = RouteObj->HasTypedField<EJson::Boolean>(TEXT("enabled")) ? RouteObj->GetBoolField(TEXT("enabled")) : true;
			Route.Opacity = static_cast<float>(FMath::Clamp(GetNum(RouteObj, TEXT("opacity"), 1.0), 0.0, 1.0));

			auto ParseRect = [&GetNum](const TSharedPtr<FJsonObject>& RectObj, int32& OutX, int32& OutY, int32& OutW, int32& OutH, int32 DefaultW, int32 DefaultH)
			{
				if (!RectObj.IsValid())
				{
					return;
				}
				OutX = static_cast<int32>(GetNum(RectObj, TEXT("x"), GetNum(RectObj, TEXT("u"), 0.0)));
				OutY = static_cast<int32>(GetNum(RectObj, TEXT("y"), GetNum(RectObj, TEXT("v"), 0.0)));
				OutW = static_cast<int32>(GetNum(RectObj, TEXT("w"), GetNum(RectObj, TEXT("width"), DefaultW)));
				OutH = static_cast<int32>(GetNum(RectObj, TEXT("h"), GetNum(RectObj, TEXT("height"), DefaultH)));
			};

			if (RouteObj->HasTypedField<EJson::Object>(TEXT("sourceRect")))
			{
				ParseRect(RouteObj->GetObjectField(TEXT("sourceRect")), Route.SourceX, Route.SourceY, Route.SourceW, Route.SourceH, 1920, 1080);
			}
			else if (RouteObj->HasTypedField<EJson::Object>(TEXT("srcRect")))
			{
				ParseRect(RouteObj->GetObjectField(TEXT("srcRect")), Route.SourceX, Route.SourceY, Route.SourceW, Route.SourceH, 1920, 1080);
			}
			else
			{
				Route.SourceX = static_cast<int32>(GetNum(RouteObj, TEXT("sourceX"), GetNum(RouteObj, TEXT("srcX"), 0.0)));
				Route.SourceY = static_cast<int32>(GetNum(RouteObj, TEXT("sourceY"), GetNum(RouteObj, TEXT("srcY"), 0.0)));
				Route.SourceW = static_cast<int32>(GetNum(RouteObj, TEXT("sourceW"), GetNum(RouteObj, TEXT("srcW"), 1920.0)));
				Route.SourceH = static_cast<int32>(GetNum(RouteObj, TEXT("sourceH"), GetNum(RouteObj, TEXT("srcH"), 1080.0)));
			}

			if (RouteObj->HasTypedField<EJson::Object>(TEXT("destinationRect")))
			{
				ParseRect(RouteObj->GetObjectField(TEXT("destinationRect")), Route.DestinationX, Route.DestinationY, Route.DestinationW, Route.DestinationH, 1920, 1080);
			}
			else if (RouteObj->HasTypedField<EJson::Object>(TEXT("dstRect")))
			{
				ParseRect(RouteObj->GetObjectField(TEXT("dstRect")), Route.DestinationX, Route.DestinationY, Route.DestinationW, Route.DestinationH, 1920, 1080);
			}
			else
			{
				Route.DestinationX = static_cast<int32>(GetNum(RouteObj, TEXT("destinationX"), GetNum(RouteObj, TEXT("dstX"), 0.0)));
				Route.DestinationY = static_cast<int32>(GetNum(RouteObj, TEXT("destinationY"), GetNum(RouteObj, TEXT("dstY"), 0.0)));
				Route.DestinationW = static_cast<int32>(GetNum(RouteObj, TEXT("destinationW"), GetNum(RouteObj, TEXT("dstW"), 1920.0)));
				Route.DestinationH = static_cast<int32>(GetNum(RouteObj, TEXT("destinationH"), GetNum(RouteObj, TEXT("dstH"), 1080.0)));
			}

			Route.Id = Route.Id.TrimStartAndEnd();
			Route.SourceId = Route.SourceId.TrimStartAndEnd();
			Route.DestinationId = Route.DestinationId.TrimStartAndEnd();
			Route.SourceW = FMath::Max(1, Route.SourceW);
			Route.SourceH = FMath::Max(1, Route.SourceH);
			Route.DestinationW = FMath::Max(1, Route.DestinationW);
			Route.DestinationH = FMath::Max(1, Route.DestinationH);

			if (Route.Id.IsEmpty())
			{
				Route.Id = FString::Printf(TEXT("route-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
			}
			if (!Route.SourceId.IsEmpty() && !Route.DestinationId.IsEmpty())
			{
				MapFeedRoutes.Add(Route);
			}
		}
	}

	EnsureFeedSourcesBoundToContext(State.ContextId);
	EnsureFeedDestinationsBoundToSurfaces(State.SurfaceIds);
	EnsureFeedRoutesForDestinations(State.SurfaceIds);
	ClampAllFeedRoutesToCanvases();

	if (MapFeedSources.Num() > 0)
	{
		ActiveFeedSourceId = MapFeedSources[0].Id;
	}
	if (MapFeedDestinations.Num() > 0)
	{
		ActiveFeedDestinationId = MapFeedDestinations[0].Id;
	}
	if (MapFeedRoutes.Num() > 0)
	{
		ActiveFeedRouteId = MapFeedRoutes[0].Id;
	}
}

void SRshipContentMappingPanel::WriteFeedV2Config(const TSharedPtr<FJsonObject>& Config) const
{
	if (!Config.IsValid())
	{
		return;
	}

	if (MapMode != TEXT("feed"))
	{
		Config->RemoveField(TEXT("feedV2"));
		return;
	}

	TSharedPtr<FJsonObject> FeedV2 = MakeShared<FJsonObject>();
	FeedV2->SetStringField(TEXT("coordinateSpace"), TEXT("pixel"));

	TArray<TSharedPtr<FJsonValue>> SourceArray;
	for (const FFeedSourceV2& Source : MapFeedSources)
	{
		if (Source.Id.IsEmpty())
		{
			continue;
		}
		TSharedPtr<FJsonObject> SourceObj = MakeShared<FJsonObject>();
		SourceObj->SetStringField(TEXT("id"), Source.Id);
		if (!Source.Label.IsEmpty()) SourceObj->SetStringField(TEXT("label"), Source.Label);
		SourceObj->SetStringField(TEXT("contextId"), Source.ContextId);
		SourceObj->SetNumberField(TEXT("width"), FMath::Max(1, Source.Width));
		SourceObj->SetNumberField(TEXT("height"), FMath::Max(1, Source.Height));
		SourceArray.Add(MakeShared<FJsonValueObject>(SourceObj));
	}
	FeedV2->SetArrayField(TEXT("sources"), SourceArray);

	TArray<TSharedPtr<FJsonValue>> DestinationArray;
	for (const FFeedDestinationV2& Destination : MapFeedDestinations)
	{
		if (Destination.Id.IsEmpty())
		{
			continue;
		}
		TSharedPtr<FJsonObject> DestinationObj = MakeShared<FJsonObject>();
		DestinationObj->SetStringField(TEXT("id"), Destination.Id);
		if (!Destination.Label.IsEmpty()) DestinationObj->SetStringField(TEXT("label"), Destination.Label);
		DestinationObj->SetStringField(TEXT("surfaceId"), Destination.SurfaceId);
		DestinationObj->SetNumberField(TEXT("width"), FMath::Max(1, Destination.Width));
		DestinationObj->SetNumberField(TEXT("height"), FMath::Max(1, Destination.Height));
		DestinationArray.Add(MakeShared<FJsonValueObject>(DestinationObj));
	}
	FeedV2->SetArrayField(TEXT("destinations"), DestinationArray);

	TArray<TSharedPtr<FJsonValue>> RouteArray;
	for (const FFeedRouteV2& Route : MapFeedRoutes)
	{
		if (Route.Id.IsEmpty() || Route.SourceId.IsEmpty() || Route.DestinationId.IsEmpty())
		{
			continue;
		}

		TSharedPtr<FJsonObject> RouteObj = MakeShared<FJsonObject>();
		RouteObj->SetStringField(TEXT("id"), Route.Id);
		if (!Route.Label.IsEmpty()) RouteObj->SetStringField(TEXT("label"), Route.Label);
		RouteObj->SetStringField(TEXT("sourceId"), Route.SourceId);
		RouteObj->SetStringField(TEXT("destinationId"), Route.DestinationId);
		RouteObj->SetBoolField(TEXT("enabled"), Route.bEnabled);
		RouteObj->SetNumberField(TEXT("opacity"), FMath::Clamp(Route.Opacity, 0.0f, 1.0f));

		TSharedPtr<FJsonObject> SourceRect = MakeShared<FJsonObject>();
		SourceRect->SetNumberField(TEXT("x"), Route.SourceX);
		SourceRect->SetNumberField(TEXT("y"), Route.SourceY);
		SourceRect->SetNumberField(TEXT("w"), FMath::Max(1, Route.SourceW));
		SourceRect->SetNumberField(TEXT("h"), FMath::Max(1, Route.SourceH));
		RouteObj->SetObjectField(TEXT("sourceRect"), SourceRect);

		TSharedPtr<FJsonObject> DestinationRect = MakeShared<FJsonObject>();
		DestinationRect->SetNumberField(TEXT("x"), Route.DestinationX);
		DestinationRect->SetNumberField(TEXT("y"), Route.DestinationY);
		DestinationRect->SetNumberField(TEXT("w"), FMath::Max(1, Route.DestinationW));
		DestinationRect->SetNumberField(TEXT("h"), FMath::Max(1, Route.DestinationH));
		RouteObj->SetObjectField(TEXT("destinationRect"), DestinationRect);

		RouteArray.Add(MakeShared<FJsonValueObject>(RouteObj));
	}
	FeedV2->SetArrayField(TEXT("routes"), RouteArray);

	Config->SetObjectField(TEXT("feedV2"), FeedV2);
}

void SRshipContentMappingPanel::RefreshFeedV2Canvases()
{
	if (MapMode != TEXT("feed"))
	{
		if (FeedSourceCanvas.IsValid())
		{
			FeedSourceCanvas->SetFeedRectValueModePixels(false);
			FeedSourceCanvas->SetCanvasResolution(1920, 1080);
			FeedSourceCanvas->SetBackgroundTexture(nullptr);
			FeedSourceCanvas->SetFeedRect(0.0f, 0.0f, 1.0f, 1.0f);
		}
		if (FeedDestinationCanvas.IsValid())
		{
			FeedDestinationCanvas->SetFeedRectValueModePixels(false);
			FeedDestinationCanvas->SetCanvasResolution(1920, 1080);
			FeedDestinationCanvas->SetBackgroundTexture(nullptr);
			FeedDestinationCanvas->SetFeedRect(0.0f, 0.0f, 1.0f, 1.0f);
		}
		FeedDestinationCanvas.Reset();
		if (FeedDestinationCanvasList.IsValid())
		{
			FeedDestinationCanvasList->ClearChildren();
		}
		return;
	}

	ClampAllFeedRoutesToCanvases();

	if (ActiveFeedSourceId.IsEmpty() && MapFeedSources.Num() > 0)
	{
		ActiveFeedSourceId = MapFeedSources[0].Id;
	}
	if (ActiveFeedDestinationId.IsEmpty() && MapFeedDestinations.Num() > 0)
	{
		ActiveFeedDestinationId = MapFeedDestinations[0].Id;
	}
	if (ActiveFeedRouteId.IsEmpty() && MapFeedRoutes.Num() > 0)
	{
		ActiveFeedRouteId = MapFeedRoutes[0].Id;
	}

	if (FFeedRouteV2* ActiveRoute = FindFeedRouteById(ActiveFeedRouteId))
	{
		if (!ActiveRoute->SourceId.IsEmpty())
		{
			ActiveFeedSourceId = ActiveRoute->SourceId;
		}
		if (!ActiveRoute->DestinationId.IsEmpty())
		{
			ActiveFeedDestinationId = ActiveRoute->DestinationId;
		}
	}

	if (FeedSourceCanvas.IsValid())
	{
		FeedSourceCanvas->SetDisplayMode(TEXT("feed"));
		TArray<FRshipCanvasFeedRectEntry> SourceEntries;
		int32 SourceWidth = 1920;
		int32 SourceHeight = 1080;
		UTexture* SourceTexture = nullptr;

		if (FFeedSourceV2* Source = FindFeedSourceById(ActiveFeedSourceId))
		{
			SourceWidth = FMath::Max(1, Source->Width);
			SourceHeight = FMath::Max(1, Source->Height);
			if (GEngine && !Source->ContextId.IsEmpty())
			{
				if (URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>())
				{
					if (URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager())
					{
						const TArray<FRshipRenderContextState> Contexts = Manager->GetRenderContexts();
						for (const FRshipRenderContextState& Context : Contexts)
						{
							if (Context.Id == Source->ContextId)
							{
								SourceTexture = Context.ResolvedTexture;
								if (Context.Width > 0) SourceWidth = Context.Width;
								if (Context.Height > 0) SourceHeight = Context.Height;
								break;
							}
						}
					}
				}
			}
		}
		FeedSourceCanvas->SetFeedRectValueModePixels(true);
		FeedSourceCanvas->SetCanvasResolution(SourceWidth, SourceHeight);

		for (const FFeedRouteV2& Route : MapFeedRoutes)
		{
			if (Route.SourceId != ActiveFeedSourceId)
			{
				continue;
			}
			FRshipCanvasFeedRectEntry Entry;
			Entry.SurfaceId = Route.Id;
			Entry.Label = Route.Label.IsEmpty() ? Route.DestinationId : Route.Label;
			Entry.U = static_cast<float>(Route.SourceX);
			Entry.V = static_cast<float>(Route.SourceY);
			Entry.W = static_cast<float>(FMath::Max(1, Route.SourceW));
			Entry.H = static_cast<float>(FMath::Max(1, Route.SourceH));
			Entry.bActive = (Route.Id == ActiveFeedRouteId);
			SourceEntries.Add(Entry);
		}
		FeedSourceCanvas->SetFeedRects(SourceEntries);
		FeedSourceCanvas->SetBackgroundTexture(SourceTexture);
	}

	FeedDestinationCanvas.Reset();
	if (FeedDestinationCanvasList.IsValid())
	{
		FeedDestinationCanvasList->ClearChildren();

		auto ResolveSourceTextureForDestination = [this](const FString& DestinationId) -> UTexture*
		{
			if (!GEngine)
			{
				return nullptr;
			}
			URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
			if (!Subsystem)
			{
				return nullptr;
			}
			URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
			if (!Manager)
			{
				return nullptr;
			}
			const TArray<FRshipRenderContextState> Contexts = Manager->GetRenderContexts();

			for (const FFeedRouteV2& Route : MapFeedRoutes)
			{
				if (Route.DestinationId != DestinationId || !Route.bEnabled)
				{
					continue;
				}
				const FFeedSourceV2* Source = nullptr;
				for (const FFeedSourceV2& Candidate : MapFeedSources)
				{
					if (Candidate.Id == Route.SourceId)
					{
						Source = &Candidate;
						break;
					}
				}
				if (!Source)
				{
					continue;
				}
				const FString SourceContextId = Source->ContextId.TrimStartAndEnd();
				if (SourceContextId.IsEmpty())
				{
					continue;
				}
				for (const FRshipRenderContextState& Context : Contexts)
				{
					if (Context.Id == SourceContextId && Context.ResolvedTexture)
					{
						return Context.ResolvedTexture;
					}
				}
			}

			for (const FRshipRenderContextState& Context : Contexts)
			{
				if (Context.bEnabled && Context.ResolvedTexture)
				{
					return Context.ResolvedTexture;
				}
			}
			return nullptr;
		};

		for (const FFeedDestinationV2& Destination : MapFeedDestinations)
		{
			const FString DestinationId = Destination.Id;
			const int32 DestinationWidth = FMath::Max(1, Destination.Width);
			const int32 DestinationHeight = FMath::Max(1, Destination.Height);

			TArray<FRshipCanvasFeedRectEntry> DestinationEntries;
			for (const FFeedRouteV2& Route : MapFeedRoutes)
			{
				if (Route.DestinationId != DestinationId)
				{
					continue;
				}
				FRshipCanvasFeedRectEntry Entry;
				Entry.SurfaceId = Route.Id;
				Entry.Label = Route.Label.IsEmpty() ? Route.SourceId : Route.Label;
				Entry.U = static_cast<float>(Route.DestinationX);
				Entry.V = static_cast<float>(Route.DestinationY);
				Entry.W = static_cast<float>(FMath::Max(1, Route.DestinationW));
				Entry.H = static_cast<float>(FMath::Max(1, Route.DestinationH));
				Entry.bActive = (Route.Id == ActiveFeedRouteId);
				DestinationEntries.Add(Entry);
			}

			const FString DestinationLabel = Destination.SurfaceId.TrimStartAndEnd().IsEmpty()
				? DestinationId
				: Destination.SurfaceId;

			TSharedPtr<SRshipMappingCanvas> DestinationCanvasWidget;
			FeedDestinationCanvasList->AddSlot().AutoHeight().Padding(0, 0, 0, 6)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(2.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
					[
						SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%s (%dx%d)"), *DestinationLabel, DestinationWidth, DestinationHeight)))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SScrollBox)
						.Orientation(Orient_Horizontal)
						+ SScrollBox::Slot()
						[
							SAssignNew(DestinationCanvasWidget, SRshipMappingCanvas)
							.OnFeedRectChanged_Lambda([this](const FString& RouteId, float U, float V, float W, float H)
							{
								for (FFeedRouteV2& Route : MapFeedRoutes)
								{
									if (Route.Id != RouteId)
									{
										continue;
									}
									int32 CanvasWidth = 1920;
									int32 CanvasHeight = 1080;
									for (const FFeedDestinationV2& CanvasDestination : MapFeedDestinations)
									{
										if (CanvasDestination.Id == Route.DestinationId)
										{
											CanvasWidth = FMath::Max(1, CanvasDestination.Width);
											CanvasHeight = FMath::Max(1, CanvasDestination.Height);
											break;
										}
									}
									Route.DestinationX = FMath::Clamp(FMath::RoundToInt(U), 0, CanvasWidth - 1);
									Route.DestinationY = FMath::Clamp(FMath::RoundToInt(V), 0, CanvasHeight - 1);
									Route.DestinationW = FMath::Clamp(FMath::RoundToInt(W), 1, CanvasWidth - Route.DestinationX);
									Route.DestinationH = FMath::Clamp(FMath::RoundToInt(H), 1, CanvasHeight - Route.DestinationY);
									ClampFeedRouteToCanvas(Route);
									ActiveFeedRouteId = Route.Id;
									ActiveFeedSourceId = Route.SourceId;
									ActiveFeedDestinationId = Route.DestinationId;
									break;
								}
								RebuildFeedV2Lists();
								ApplyCurrentFormToSelectedMapping(false);
							})
							.OnFeedRectSelectionChanged_Lambda([this](const FString& RouteId)
							{
								for (const FFeedRouteV2& Route : MapFeedRoutes)
								{
									if (Route.Id == RouteId)
									{
										ActiveFeedRouteId = Route.Id;
										ActiveFeedSourceId = Route.SourceId;
										ActiveFeedDestinationId = Route.DestinationId;
										break;
									}
								}
								RebuildFeedV2Lists();
							})
						]
					]
				]
			];

			if (DestinationCanvasWidget.IsValid())
			{
				DestinationCanvasWidget->SetDisplayMode(TEXT("feed"));
				DestinationCanvasWidget->SetFeedRectValueModePixels(true);
				DestinationCanvasWidget->SetCanvasResolution(DestinationWidth, DestinationHeight);
				DestinationCanvasWidget->SetFeedRects(DestinationEntries);
				DestinationCanvasWidget->SetBackgroundTexture(ResolveSourceTextureForDestination(DestinationId));
				if (!FeedDestinationCanvas.IsValid() || DestinationId == ActiveFeedDestinationId)
				{
					FeedDestinationCanvas = DestinationCanvasWidget;
				}
			}
		}
	}
}

void SRshipContentMappingPanel::RebuildFeedV2Lists()
{
	if (!MapFeedSourceList.IsValid() || !MapFeedDestinationList.IsValid() || !MapFeedRouteList.IsValid())
	{
		RefreshFeedV2Canvases();
		return;
	}

	MapFeedSourceList->ClearChildren();
	MapFeedDestinationList->ClearChildren();
	MapFeedRouteList->ClearChildren();

	if (MapMode != TEXT("feed"))
	{
		RefreshFeedV2Canvases();
		return;
	}

	const FString ContextId = MapContextInput.IsValid() ? MapContextInput->GetText().ToString().TrimStartAndEnd() : TEXT("");
	const TArray<FString> MappingSurfaceIds = GetCurrentMappingSurfaceIds();
	EnsureFeedSourcesBoundToContext(ContextId);
	EnsureFeedDestinationsBoundToSurfaces(MappingSurfaceIds);
	EnsureFeedRoutesForDestinations(MappingSurfaceIds);
	ClampAllFeedRoutesToCanvases();

	if (MapFeedSources.Num() == 0)
	{
		MapFeedSourceList->AddSlot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("FeedV2NoSources", "No sources"))];
	}
	for (const FFeedSourceV2& SourceRef : MapFeedSources)
	{
		const FString SourceId = SourceRef.Id;
		MapFeedSourceList->AddSlot().AutoHeight().Padding(0, 1)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
			[
				SNew(SButton)
				.Text(LOCTEXT("FeedV2SelectSource", "Use"))
				.OnClicked_Lambda([this, SourceId]()
				{
					ActiveFeedSourceId = SourceId;
					RefreshFeedV2Canvases();
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot().FillWidth(0.45f).Padding(0,0,4,0)
			[
				SNew(SEditableTextBox)
				.Text_Lambda([this, SourceId]() { if (const FFeedSourceV2* Source = [&]() -> const FFeedSourceV2* { for (const FFeedSourceV2& S : MapFeedSources) { if (S.Id == SourceId) return &S; } return nullptr; }()) return FText::FromString(Source->ContextId); return FText::GetEmpty(); })
				.OnTextCommitted_Lambda([this, SourceId](const FText& NewText, ETextCommit::Type)
				{
					if (FFeedSourceV2* Source = FindFeedSourceById(SourceId))
					{
						Source->ContextId = NewText.ToString().TrimStartAndEnd();
					}
					RefreshFeedV2Canvases();
					if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
			[
				SNew(SSpinBox<int32>)
				.MinValue(1).MaxValue(16384)
				.Value_Lambda([this, SourceId]() { if (const FFeedSourceV2* Source = [&]() -> const FFeedSourceV2* { for (const FFeedSourceV2& S : MapFeedSources) { if (S.Id == SourceId) return &S; } return nullptr; }()) return Source->Width; return 1920; })
				.OnValueChanged_Lambda([this, SourceId](int32 NewValue)
				{
					if (FFeedSourceV2* Source = FindFeedSourceById(SourceId))
					{
						Source->Width = FMath::Max(1, NewValue);
						ClampAllFeedRoutesToCanvases();
					}
					RefreshFeedV2Canvases();
					if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
			[
				SNew(SSpinBox<int32>)
				.MinValue(1).MaxValue(16384)
				.Value_Lambda([this, SourceId]() { if (const FFeedSourceV2* Source = [&]() -> const FFeedSourceV2* { for (const FFeedSourceV2& S : MapFeedSources) { if (S.Id == SourceId) return &S; } return nullptr; }()) return Source->Height; return 1080; })
				.OnValueChanged_Lambda([this, SourceId](int32 NewValue)
				{
					if (FFeedSourceV2* Source = FindFeedSourceById(SourceId))
					{
						Source->Height = FMath::Max(1, NewValue);
						ClampAllFeedRoutesToCanvases();
					}
					RefreshFeedV2Canvases();
					if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
				})
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("FeedV2DelSource", "X"))
				.OnClicked_Lambda([this, SourceId]()
				{
					MapFeedSources.RemoveAll([&SourceId](const FFeedSourceV2& Source) { return Source.Id == SourceId; });
					MapFeedRoutes.RemoveAll([&SourceId](const FFeedRouteV2& Route) { return Route.SourceId == SourceId; });
					if (ActiveFeedSourceId == SourceId) ActiveFeedSourceId.Reset();
					if (ActiveFeedRouteId.IsEmpty() || !MapFeedRoutes.ContainsByPredicate([this](const FFeedRouteV2& Route) { return Route.Id == ActiveFeedRouteId; }))
					{
						ActiveFeedRouteId = MapFeedRoutes.Num() > 0 ? MapFeedRoutes[0].Id : FString();
					}
					RebuildFeedV2Lists();
					if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
					return FReply::Handled();
				})
			]
		];
	}

	if (MapFeedDestinations.Num() == 0)
	{
		MapFeedDestinationList->AddSlot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("FeedV2NoDestinations", "No destinations"))];
	}
	for (const FFeedDestinationV2& DestinationRef : MapFeedDestinations)
	{
		const FString DestinationId = DestinationRef.Id;
		MapFeedDestinationList->AddSlot().AutoHeight().Padding(0, 1)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
			[
				SNew(SButton)
				.Text(LOCTEXT("FeedV2SelectDestination", "Use"))
				.OnClicked_Lambda([this, DestinationId]()
				{
					ActiveFeedDestinationId = DestinationId;
					RefreshFeedV2Canvases();
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot().FillWidth(0.45f).Padding(0,0,4,0)
			[
				SNew(SEditableTextBox)
				.Text_Lambda([this, DestinationId]() { for (const FFeedDestinationV2& Destination : MapFeedDestinations) { if (Destination.Id == DestinationId) return FText::FromString(Destination.SurfaceId); } return FText::GetEmpty(); })
				.OnTextCommitted_Lambda([this, DestinationId](const FText& NewText, ETextCommit::Type)
				{
					if (FFeedDestinationV2* Destination = FindFeedDestinationById(DestinationId))
					{
						Destination->SurfaceId = NewText.ToString().TrimStartAndEnd();
					}
					RefreshFeedV2Canvases();
					if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
			[
				SNew(SSpinBox<int32>)
				.MinValue(1).MaxValue(16384)
				.Value_Lambda([this, DestinationId]() { for (const FFeedDestinationV2& Destination : MapFeedDestinations) { if (Destination.Id == DestinationId) return Destination.Width; } return 1920; })
				.OnValueChanged_Lambda([this, DestinationId](int32 NewValue)
				{
					if (FFeedDestinationV2* Destination = FindFeedDestinationById(DestinationId))
					{
						Destination->Width = FMath::Max(1, NewValue);
						ClampAllFeedRoutesToCanvases();
					}
					RefreshFeedV2Canvases();
					if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
			[
				SNew(SSpinBox<int32>)
				.MinValue(1).MaxValue(16384)
				.Value_Lambda([this, DestinationId]() { for (const FFeedDestinationV2& Destination : MapFeedDestinations) { if (Destination.Id == DestinationId) return Destination.Height; } return 1080; })
				.OnValueChanged_Lambda([this, DestinationId](int32 NewValue)
				{
					if (FFeedDestinationV2* Destination = FindFeedDestinationById(DestinationId))
					{
						Destination->Height = FMath::Max(1, NewValue);
						ClampAllFeedRoutesToCanvases();
					}
					RefreshFeedV2Canvases();
					if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
				})
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("FeedV2DelDestination", "X"))
				.OnClicked_Lambda([this, DestinationId]()
				{
					MapFeedDestinations.RemoveAll([&DestinationId](const FFeedDestinationV2& Destination) { return Destination.Id == DestinationId; });
					MapFeedRoutes.RemoveAll([&DestinationId](const FFeedRouteV2& Route) { return Route.DestinationId == DestinationId; });
					if (ActiveFeedDestinationId == DestinationId) ActiveFeedDestinationId.Reset();
					if (ActiveFeedRouteId.IsEmpty() || !MapFeedRoutes.ContainsByPredicate([this](const FFeedRouteV2& Route) { return Route.Id == ActiveFeedRouteId; }))
					{
						ActiveFeedRouteId = MapFeedRoutes.Num() > 0 ? MapFeedRoutes[0].Id : FString();
					}
					RebuildFeedV2Lists();
					if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
					return FReply::Handled();
				})
			]
		];
	}

	if (MapFeedRoutes.Num() == 0)
	{
		MapFeedRouteList->AddSlot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("FeedV2NoRoutes", "No routes"))];
	}
	for (const FFeedRouteV2& RouteRef : MapFeedRoutes)
	{
		const FString RouteId = RouteRef.Id;
		MapFeedRouteList->AddSlot().AutoHeight().Padding(0, 2)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.BorderBackgroundColor_Lambda([this, RouteId]()
			{
				return RouteId == ActiveFeedRouteId
					? FLinearColor(0.24f, 0.42f, 0.88f, 0.45f)
					: FLinearColor(0.05f, 0.05f, 0.05f, 0.2f);
			})
			.Padding(3.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SNew(SButton)
						.Text(LOCTEXT("FeedV2SelectRoute", "Edit"))
						.OnClicked_Lambda([this, RouteId]()
						{
							ActiveFeedRouteId = RouteId;
							for (const FFeedRouteV2& Route : MapFeedRoutes)
							{
								if (Route.Id == RouteId)
								{
									ActiveFeedSourceId = Route.SourceId;
									ActiveFeedDestinationId = Route.DestinationId;
									break;
								}
							}
							RefreshFeedV2Canvases();
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().FillWidth(0.35f).Padding(0,0,4,0)
					[
						SNew(SEditableTextBox)
						.Text_Lambda([this, RouteId]() { for (const FFeedRouteV2& Route : MapFeedRoutes) { if (Route.Id == RouteId) return FText::FromString(Route.SourceId); } return FText::GetEmpty(); })
						.OnTextCommitted_Lambda([this, RouteId](const FText& NewText, ETextCommit::Type)
						{
							if (FFeedRouteV2* Route = FindFeedRouteById(RouteId))
							{
								Route->SourceId = NewText.ToString().TrimStartAndEnd();
								ClampFeedRouteToCanvas(*Route);
								ActiveFeedSourceId = Route->SourceId;
								ActiveFeedRouteId = RouteId;
							}
							RefreshFeedV2Canvases();
							if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SNew(SComboButton)
						.OnGetMenuContent_Lambda([this, RouteId]()
						{
							FMenuBuilder MenuBuilder(true, nullptr);
							for (const FFeedSourceV2& Source : MapFeedSources)
							{
								const FString SourceId = Source.Id;
								const FString SourceLabel = Source.ContextId.IsEmpty() ? SourceId : FString::Printf(TEXT("%s (%s)"), *SourceId, *Source.ContextId);
								MenuBuilder.AddMenuEntry(
									FText::FromString(SourceLabel),
									FText::GetEmpty(),
									FSlateIcon(),
									FUIAction(FExecuteAction::CreateLambda([this, RouteId, SourceId]()
									{
										if (FFeedRouteV2* Route = FindFeedRouteById(RouteId))
										{
											Route->SourceId = SourceId;
											ClampFeedRouteToCanvas(*Route);
											ActiveFeedSourceId = Route->SourceId;
											ActiveFeedRouteId = RouteId;
										}
										RefreshFeedV2Canvases();
										if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
									})));
							}
							if (MapFeedSources.Num() == 0)
							{
								MenuBuilder.AddWidget(
									SNew(STextBlock).Text(LOCTEXT("FeedV2NoSourceOptions", "No sources")),
									FText::GetEmpty(),
									true,
									false);
							}
							return MenuBuilder.MakeWidget();
						})
						.ButtonContent()
						[
							SNew(STextBlock).Text(LOCTEXT("FeedV2PickSource", "Pick Src"))
						]
					]
					+ SHorizontalBox::Slot().FillWidth(0.35f).Padding(0,0,4,0)
					[
						SNew(SEditableTextBox)
						.Text_Lambda([this, RouteId]() { for (const FFeedRouteV2& Route : MapFeedRoutes) { if (Route.Id == RouteId) return FText::FromString(Route.DestinationId); } return FText::GetEmpty(); })
						.OnTextCommitted_Lambda([this, RouteId](const FText& NewText, ETextCommit::Type)
						{
							if (FFeedRouteV2* Route = FindFeedRouteById(RouteId))
							{
								Route->DestinationId = NewText.ToString().TrimStartAndEnd();
								ClampFeedRouteToCanvas(*Route);
								ActiveFeedDestinationId = Route->DestinationId;
								ActiveFeedRouteId = RouteId;
							}
							RefreshFeedV2Canvases();
							if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SNew(SComboButton)
						.OnGetMenuContent_Lambda([this, RouteId]()
						{
							FMenuBuilder MenuBuilder(true, nullptr);
							for (const FFeedDestinationV2& Destination : MapFeedDestinations)
							{
								const FString DestinationId = Destination.Id;
								const FString DestinationLabel = Destination.SurfaceId.IsEmpty() ? DestinationId : FString::Printf(TEXT("%s (%s)"), *DestinationId, *Destination.SurfaceId);
								MenuBuilder.AddMenuEntry(
									FText::FromString(DestinationLabel),
									FText::GetEmpty(),
									FSlateIcon(),
									FUIAction(FExecuteAction::CreateLambda([this, RouteId, DestinationId]()
									{
										if (FFeedRouteV2* Route = FindFeedRouteById(RouteId))
										{
											Route->DestinationId = DestinationId;
											ClampFeedRouteToCanvas(*Route);
											ActiveFeedDestinationId = Route->DestinationId;
											ActiveFeedRouteId = RouteId;
										}
										RefreshFeedV2Canvases();
										if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
									})));
							}
							if (MapFeedDestinations.Num() == 0)
							{
								MenuBuilder.AddWidget(
									SNew(STextBlock).Text(LOCTEXT("FeedV2NoDestinationOptions", "No destinations")),
									FText::GetEmpty(),
									true,
									false);
							}
							return MenuBuilder.MakeWidget();
						})
						.ButtonContent()
						[
							SNew(STextBlock).Text(LOCTEXT("FeedV2PickDestination", "Pick Dst"))
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([this, RouteId]() { for (const FFeedRouteV2& Route : MapFeedRoutes) { if (Route.Id == RouteId) return Route.bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; } return ECheckBoxState::Checked; })
						.OnCheckStateChanged_Lambda([this, RouteId](ECheckBoxState NewState)
						{
							if (FFeedRouteV2* Route = FindFeedRouteById(RouteId))
							{
								Route->bEnabled = (NewState == ECheckBoxState::Checked);
							}
							RefreshFeedV2Canvases();
							if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)
					[
						SNew(SSpinBox<float>)
						.MinValue(0.0f).MaxValue(1.0f).Delta(0.05f)
						.Value_Lambda([this, RouteId]() { for (const FFeedRouteV2& Route : MapFeedRoutes) { if (Route.Id == RouteId) return Route.Opacity; } return 1.0f; })
						.OnValueChanged_Lambda([this, RouteId](float NewValue)
						{
							if (FFeedRouteV2* Route = FindFeedRouteById(RouteId))
							{
								Route->Opacity = FMath::Clamp(NewValue, 0.0f, 1.0f);
							}
							if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
						})
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("FeedV2DelRoute", "X"))
						.OnClicked_Lambda([this, RouteId]()
						{
							MapFeedRoutes.RemoveAll([&RouteId](const FFeedRouteV2& Route) { return Route.Id == RouteId; });
							if (ActiveFeedRouteId == RouteId)
							{
								ActiveFeedRouteId = MapFeedRoutes.Num() > 0 ? MapFeedRoutes[0].Id : FString();
							}
							RebuildFeedV2Lists();
							if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
							return FReply::Handled();
						})
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0,2,0,0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,4,0)
					[
						SNew(STextBlock).Text(LOCTEXT("FeedV2SrcRectLabel", "Src"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,2,0)
					[
						SNew(SSpinBox<int32>)
						.MinValue(0).MaxValue(65535)
						.Value_Lambda([this, RouteId]() { for (const FFeedRouteV2& Route : MapFeedRoutes) { if (Route.Id == RouteId) return Route.SourceX; } return 0; })
						.OnValueChanged_Lambda([this, RouteId](int32 NewValue)
						{
							if (FFeedRouteV2* Route = FindFeedRouteById(RouteId))
							{
								Route->SourceX = NewValue;
								ClampFeedRouteToCanvas(*Route);
							}
							RefreshFeedV2Canvases();
							if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,2,0)
					[
						SNew(SSpinBox<int32>)
						.MinValue(0).MaxValue(65535)
						.Value_Lambda([this, RouteId]() { for (const FFeedRouteV2& Route : MapFeedRoutes) { if (Route.Id == RouteId) return Route.SourceY; } return 0; })
						.OnValueChanged_Lambda([this, RouteId](int32 NewValue)
						{
							if (FFeedRouteV2* Route = FindFeedRouteById(RouteId))
							{
								Route->SourceY = NewValue;
								ClampFeedRouteToCanvas(*Route);
							}
							RefreshFeedV2Canvases();
							if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,2,0)
					[
						SNew(SSpinBox<int32>)
						.MinValue(1).MaxValue(65535)
						.Value_Lambda([this, RouteId]() { for (const FFeedRouteV2& Route : MapFeedRoutes) { if (Route.Id == RouteId) return Route.SourceW; } return 1920; })
						.OnValueChanged_Lambda([this, RouteId](int32 NewValue)
						{
							if (FFeedRouteV2* Route = FindFeedRouteById(RouteId))
							{
								Route->SourceW = FMath::Max(1, NewValue);
								ClampFeedRouteToCanvas(*Route);
							}
							RefreshFeedV2Canvases();
							if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,6,0)
					[
						SNew(SSpinBox<int32>)
						.MinValue(1).MaxValue(65535)
						.Value_Lambda([this, RouteId]() { for (const FFeedRouteV2& Route : MapFeedRoutes) { if (Route.Id == RouteId) return Route.SourceH; } return 1080; })
						.OnValueChanged_Lambda([this, RouteId](int32 NewValue)
						{
							if (FFeedRouteV2* Route = FindFeedRouteById(RouteId))
							{
								Route->SourceH = FMath::Max(1, NewValue);
								ClampFeedRouteToCanvas(*Route);
							}
							RefreshFeedV2Canvases();
							if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,4,0)
					[
						SNew(STextBlock).Text(LOCTEXT("FeedV2DstRectLabel", "Dst"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,2,0)
					[
						SNew(SSpinBox<int32>)
						.MinValue(0).MaxValue(65535)
						.Value_Lambda([this, RouteId]() { for (const FFeedRouteV2& Route : MapFeedRoutes) { if (Route.Id == RouteId) return Route.DestinationX; } return 0; })
						.OnValueChanged_Lambda([this, RouteId](int32 NewValue)
						{
							if (FFeedRouteV2* Route = FindFeedRouteById(RouteId))
							{
								Route->DestinationX = NewValue;
								ClampFeedRouteToCanvas(*Route);
							}
							RefreshFeedV2Canvases();
							if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,2,0)
					[
						SNew(SSpinBox<int32>)
						.MinValue(0).MaxValue(65535)
						.Value_Lambda([this, RouteId]() { for (const FFeedRouteV2& Route : MapFeedRoutes) { if (Route.Id == RouteId) return Route.DestinationY; } return 0; })
						.OnValueChanged_Lambda([this, RouteId](int32 NewValue)
						{
							if (FFeedRouteV2* Route = FindFeedRouteById(RouteId))
							{
								Route->DestinationY = NewValue;
								ClampFeedRouteToCanvas(*Route);
							}
							RefreshFeedV2Canvases();
							if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0,0,2,0)
					[
						SNew(SSpinBox<int32>)
						.MinValue(1).MaxValue(65535)
						.Value_Lambda([this, RouteId]() { for (const FFeedRouteV2& Route : MapFeedRoutes) { if (Route.Id == RouteId) return Route.DestinationW; } return 1920; })
						.OnValueChanged_Lambda([this, RouteId](int32 NewValue)
						{
							if (FFeedRouteV2* Route = FindFeedRouteById(RouteId))
							{
								Route->DestinationW = FMath::Max(1, NewValue);
								ClampFeedRouteToCanvas(*Route);
							}
							RefreshFeedV2Canvases();
							if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
						})
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SSpinBox<int32>)
						.MinValue(1).MaxValue(65535)
						.Value_Lambda([this, RouteId]() { for (const FFeedRouteV2& Route : MapFeedRoutes) { if (Route.Id == RouteId) return Route.DestinationH; } return 1080; })
						.OnValueChanged_Lambda([this, RouteId](int32 NewValue)
						{
							if (FFeedRouteV2* Route = FindFeedRouteById(RouteId))
							{
								Route->DestinationH = FMath::Max(1, NewValue);
								ClampFeedRouteToCanvas(*Route);
							}
							RefreshFeedV2Canvases();
							if (!bSuspendLiveMappingSync) ApplyCurrentFormToSelectedMapping(false);
						})
					]
				]
			]
		];
	}

	RefreshFeedV2Canvases();
}

bool SRshipContentMappingPanel::ApplyCurrentFormToSelectedMapping(bool bCreateIfMissing)
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
	// Programmatic form population/reset can fire value-change delegates.
	// Suppress implicit writes while live sync is suspended.
	if (bSuspendLiveMappingSync && !bCreateIfMissing)
	{
		return false;
	}

	FRshipContentMappingState State;
	State.Id = SelectedMappingId;
	if (State.Id.IsEmpty() && !bCreateIfMissing)
	{
		return false;
	}
	if (!State.Id.IsEmpty())
	{
		const TArray<FRshipContentMappingState> ExistingMappings = Manager->GetMappings();
		const bool bHasExistingMapping = ExistingMappings.ContainsByPredicate([&State](const FRshipContentMappingState& Existing)
		{
			return Existing.Id == State.Id;
		});
		if (!bHasExistingMapping)
		{
			if (!bCreateIfMissing)
			{
				SelectedMappingRows.Remove(State.Id);
				ClearSelectedMappingId();
				return false;
			}
			State.Id.Reset();
		}
	}
	const bool bCreatingNewMapping = State.Id.IsEmpty();

	State.Name = MapNameInput.IsValid() ? MapNameInput->GetText().ToString() : TEXT("");
	State.ProjectId = MapProjectInput.IsValid() ? MapProjectInput->GetText().ToString() : TEXT("");
	const FString ModeForSave = MapModeSelector.IsValid() ? MapModeSelector->GetSelectedMode() : MapMode;
	const FString NormalizedMode = NormalizeMapMode(ModeForSave, MapModeDirect);
	MapMode = NormalizedMode;
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

	if (NormalizedMode == MapModeFeed)
	{
		if (State.SurfaceIds.Num() == 0)
		{
			for (const FFeedDestinationV2& Destination : MapFeedDestinations)
			{
				const FString SurfaceId = Destination.SurfaceId.TrimStartAndEnd();
				if (SurfaceId.IsEmpty() || SurfaceId.Equals(TEXT("surface"), ESearchCase::IgnoreCase))
				{
					continue;
				}
				State.SurfaceIds.AddUnique(SurfaceId);
			}
		}
		if (State.SurfaceIds.Num() == 0)
		{
			const TArray<FRshipMappingSurfaceState> Surfaces = Manager->GetMappingSurfaces();
			for (const FRshipMappingSurfaceState& Surface : Surfaces)
			{
				if (Surface.Id.IsEmpty() || !Surface.bEnabled)
				{
					continue;
				}
				State.SurfaceIds.AddUnique(Surface.Id);
			}
		}

		EnsureFeedSourcesBoundToContext(State.ContextId);
		if (!State.ContextId.IsEmpty()
			&& !MapFeedSources.ContainsByPredicate([&State](const FFeedSourceV2& Source)
			{
				return Source.ContextId.TrimStartAndEnd().Equals(State.ContextId.TrimStartAndEnd(), ESearchCase::IgnoreCase);
			}))
		{
			FFeedSourceV2 Source;
			Source.Id = FString::Printf(TEXT("source-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
			Source.ContextId = State.ContextId.TrimStartAndEnd();
			MapFeedSources.Add(Source);
			if (ActiveFeedSourceId.IsEmpty())
			{
				ActiveFeedSourceId = Source.Id;
			}
		}
		if (State.ContextId.IsEmpty())
		{
			for (const FFeedSourceV2& Source : MapFeedSources)
			{
				const FString SourceContextId = Source.ContextId.TrimStartAndEnd();
				if (!SourceContextId.IsEmpty())
				{
					State.ContextId = SourceContextId;
					break;
				}
			}
		}

		EnsureFeedDestinationsBoundToSurfaces(State.SurfaceIds);

		EnsureFeedRoutesForDestinations(State.SurfaceIds);
		if (MapFeedRoutes.Num() > 0 && (bCreatingNewMapping || ActiveFeedRouteId.IsEmpty()))
		{
			ActiveFeedRouteId = MapFeedRoutes[0].Id;
			ActiveFeedSourceId = MapFeedRoutes[0].SourceId;
			ActiveFeedDestinationId = MapFeedRoutes[0].DestinationId;
		}
		ClampAllFeedRoutesToCanvases();
	}

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
			// Feed mode is driven by feedV2 only; legacy feedRect/feedRects are intentionally omitted.
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

		Config->SetNumberField(TEXT("angleMaskStart"), MapMaskStartInput.IsValid() ? MapMaskStartInput->GetValue() : 0.0);
		Config->SetNumberField(TEXT("angleMaskEnd"), MapMaskEndInput.IsValid() ? MapMaskEndInput->GetValue() : 360.0);
		Config->SetBoolField(TEXT("clipOutsideRegion"), MapClipOutsideInput.IsValid() && MapClipOutsideInput->IsChecked());
		Config->SetNumberField(TEXT("borderExpansion"), MapBorderExpansionInput.IsValid() ? MapBorderExpansionInput->GetValue() : 0.0);
	}

	WriteFeedV2Config(Config);

	const FString ContentModeStr = MapContentModeInput.IsValid() ? MapContentModeInput->GetText().ToString().TrimStartAndEnd() : TEXT("stretch");
	if (!ContentModeStr.IsEmpty() && !ContentModeStr.Equals(TEXT("stretch"), ESearchCase::IgnoreCase))
	{
		Config->SetStringField(TEXT("contentMode"), ContentModeStr);
	}
	else
	{
		Config->RemoveField(TEXT("contentMode"));
	}

	State.Config = Config;
	bool bSaved = false;

	if (State.Id.IsEmpty())
	{
		const FString NewId = Manager->CreateMapping(State);
		if (NewId.IsEmpty())
		{
			return false;
		}
		bSaved = true;
		State.Id = NewId;
		SetSelectedMappingId(NewId);
		SelectedMappingRows.Empty();
		SelectedMappingRows.Add(NewId);
		bHasLiveMappingFormHash = false;
		RefreshStatus();
	}
	else
	{
		bSaved = Manager->UpdateMapping(State);
		if (!bSaved && bCreateIfMissing)
		{
			State.Id.Reset();
			const FString NewId = Manager->CreateMapping(State);
			if (NewId.IsEmpty())
			{
				return false;
			}
			bSaved = true;
			State.Id = NewId;
			SetSelectedMappingId(NewId);
			SelectedMappingRows.Empty();
			SelectedMappingRows.Add(NewId);
			bHasLiveMappingFormHash = false;
			RefreshStatus();
		}
	}

	if (!bSaved)
	{
		return false;
	}

	EnsureContextDefaultsForMapping(Manager, State.ContextId);
	Manager->Tick(0.0f);
	return true;
}

uint32 SRshipContentMappingPanel::ComputeMappingFormLiveHash() const
{
	uint32 Hash = GetTypeHash(SelectedMappingId);
	auto HashStr = [&Hash](const FString& Value)
	{
		Hash = HashCombine(Hash, GetTypeHash(Value));
	};
	auto HashFloat = [&Hash](float Value)
	{
		Hash = HashCombine(Hash, GetTypeHash(FMath::RoundToInt(Value * 10000.0f)));
	};
	auto HashBool = [&Hash](bool Value)
	{
		Hash = HashCombine(Hash, GetTypeHash(Value ? 1 : 0));
	};

	HashStr(MapNameInput.IsValid() ? MapNameInput->GetText().ToString() : TEXT(""));
	HashStr(MapProjectInput.IsValid() ? MapProjectInput->GetText().ToString() : TEXT(""));
	HashStr(MapContextInput.IsValid() ? MapContextInput->GetText().ToString() : TEXT(""));
	HashStr(MapSurfacesInput.IsValid() ? MapSurfacesInput->GetText().ToString() : TEXT(""));
	HashStr(MapModeSelector.IsValid() ? MapModeSelector->GetSelectedMode() : MapMode);
	HashFloat(MapOpacityInput.IsValid() ? MapOpacityInput->GetValue() : 1.0f);
	HashBool(!MapEnabledInput.IsValid() || MapEnabledInput->IsChecked());

	HashFloat(MapUvScaleUInput.IsValid() ? MapUvScaleUInput->GetValue() : 1.0f);
	HashFloat(MapUvScaleVInput.IsValid() ? MapUvScaleVInput->GetValue() : 1.0f);
	HashFloat(MapUvOffsetUInput.IsValid() ? MapUvOffsetUInput->GetValue() : 0.0f);
	HashFloat(MapUvOffsetVInput.IsValid() ? MapUvOffsetVInput->GetValue() : 0.0f);
	HashFloat(MapUvRotInput.IsValid() ? MapUvRotInput->GetValue() : 0.0f);
	HashFloat(MapFeedUInput.IsValid() ? MapFeedUInput->GetValue() : 0.0f);
	HashFloat(MapFeedVInput.IsValid() ? MapFeedVInput->GetValue() : 0.0f);
	HashFloat(MapFeedWInput.IsValid() ? MapFeedWInput->GetValue() : 1.0f);
	HashFloat(MapFeedHInput.IsValid() ? MapFeedHInput->GetValue() : 1.0f);

	HashFloat(MapProjPosXInput.IsValid() ? MapProjPosXInput->GetValue() : 0.0f);
	HashFloat(MapProjPosYInput.IsValid() ? MapProjPosYInput->GetValue() : 0.0f);
	HashFloat(MapProjPosZInput.IsValid() ? MapProjPosZInput->GetValue() : 0.0f);
	HashFloat(MapProjRotXInput.IsValid() ? MapProjRotXInput->GetValue() : 0.0f);
	HashFloat(MapProjRotYInput.IsValid() ? MapProjRotYInput->GetValue() : 0.0f);
	HashFloat(MapProjRotZInput.IsValid() ? MapProjRotZInput->GetValue() : 0.0f);
	HashFloat(MapProjFovInput.IsValid() ? MapProjFovInput->GetValue() : 60.0f);
	HashFloat(MapProjAspectInput.IsValid() ? MapProjAspectInput->GetValue() : 1.7778f);
	HashFloat(MapProjNearInput.IsValid() ? MapProjNearInput->GetValue() : 10.0f);
	HashFloat(MapProjFarInput.IsValid() ? MapProjFarInput->GetValue() : 10000.0f);
	HashStr(MapCylAxisInput.IsValid() ? MapCylAxisInput->GetText().ToString() : TEXT(""));
	HashFloat(MapCylRadiusInput.IsValid() ? MapCylRadiusInput->GetValue() : 100.0f);
	HashFloat(MapCylHeightInput.IsValid() ? MapCylHeightInput->GetValue() : 1000.0f);
	HashFloat(MapCylStartInput.IsValid() ? MapCylStartInput->GetValue() : 0.0f);
	HashFloat(MapCylEndInput.IsValid() ? MapCylEndInput->GetValue() : 90.0f);
	HashFloat(MapParallelSizeWInput.IsValid() ? MapParallelSizeWInput->GetValue() : 1000.0f);
	HashFloat(MapParallelSizeHInput.IsValid() ? MapParallelSizeHInput->GetValue() : 1000.0f);
	HashFloat(MapSphRadiusInput.IsValid() ? MapSphRadiusInput->GetValue() : 500.0f);
	HashFloat(MapSphHArcInput.IsValid() ? MapSphHArcInput->GetValue() : 360.0f);
	HashFloat(MapSphVArcInput.IsValid() ? MapSphVArcInput->GetValue() : 180.0f);
	HashFloat(MapFisheyeFovInput.IsValid() ? MapFisheyeFovInput->GetValue() : 180.0f);
	HashStr(MapFisheyeLensInput.IsValid() ? MapFisheyeLensInput->GetText().ToString() : TEXT(""));
	HashFloat(MapMeshEyeXInput.IsValid() ? MapMeshEyeXInput->GetValue() : 0.0f);
	HashFloat(MapMeshEyeYInput.IsValid() ? MapMeshEyeYInput->GetValue() : 0.0f);
	HashFloat(MapMeshEyeZInput.IsValid() ? MapMeshEyeZInput->GetValue() : 0.0f);
	HashStr(MapContentModeInput.IsValid() ? MapContentModeInput->GetText().ToString() : TEXT("stretch"));
	HashFloat(MapMaskStartInput.IsValid() ? MapMaskStartInput->GetValue() : 0.0f);
	HashFloat(MapMaskEndInput.IsValid() ? MapMaskEndInput->GetValue() : 360.0f);
	HashBool(MapClipOutsideInput.IsValid() && MapClipOutsideInput->IsChecked());
	HashFloat(MapBorderExpansionInput.IsValid() ? MapBorderExpansionInput->GetValue() : 0.0f);

	TArray<FString> FeedKeys;
	MapFeedRectOverrides.GetKeys(FeedKeys);
	FeedKeys.Sort();
		for (const FString& SurfaceId : FeedKeys)
		{
			HashStr(SurfaceId);
			if (const FFeedRect* Rect = MapFeedRectOverrides.Find(SurfaceId))
			{
			HashFloat(Rect->U);
			HashFloat(Rect->V);
			HashFloat(Rect->W);
			HashFloat(Rect->H);
			}
		}

		for (const FFeedSourceV2& Source : MapFeedSources)
		{
			HashStr(Source.Id);
			HashStr(Source.Label);
			HashStr(Source.ContextId);
			Hash = HashCombine(Hash, GetTypeHash(Source.Width));
			Hash = HashCombine(Hash, GetTypeHash(Source.Height));
		}

		for (const FFeedDestinationV2& Destination : MapFeedDestinations)
		{
			HashStr(Destination.Id);
			HashStr(Destination.Label);
			HashStr(Destination.SurfaceId);
			Hash = HashCombine(Hash, GetTypeHash(Destination.Width));
			Hash = HashCombine(Hash, GetTypeHash(Destination.Height));
		}

		for (const FFeedRouteV2& Route : MapFeedRoutes)
		{
			HashStr(Route.Id);
			HashStr(Route.Label);
			HashStr(Route.SourceId);
			HashStr(Route.DestinationId);
			HashBool(Route.bEnabled);
			HashFloat(Route.Opacity);
			Hash = HashCombine(Hash, GetTypeHash(Route.SourceX));
			Hash = HashCombine(Hash, GetTypeHash(Route.SourceY));
			Hash = HashCombine(Hash, GetTypeHash(Route.SourceW));
			Hash = HashCombine(Hash, GetTypeHash(Route.SourceH));
			Hash = HashCombine(Hash, GetTypeHash(Route.DestinationX));
			Hash = HashCombine(Hash, GetTypeHash(Route.DestinationY));
			Hash = HashCombine(Hash, GetTypeHash(Route.DestinationW));
			Hash = HashCombine(Hash, GetTypeHash(Route.DestinationH));
		}

		return Hash;
	}

void SRshipContentMappingPanel::RebuildFeedRectList()
{
	RebuildFeedV2Lists();
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
				if (Mapping.Config->HasTypedField<EJson::Object>(TEXT("feedV2")))
				{
					const TSharedPtr<FJsonObject> FeedV2Obj = Mapping.Config->GetObjectField(TEXT("feedV2"));
					FString FeedV2Json;
					const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&FeedV2Json);
					if (FeedV2Obj.IsValid())
					{
						FJsonSerializer::Serialize(FeedV2Obj.ToSharedRef(), Writer);
						HashString(SnapshotHash, FeedV2Json);
					}
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
	if (!bHasListHash || SnapshotHash != LastListHash)
	{
		LastListHash = SnapshotHash;
		bHasListHash = true;
		bHasPendingListHash = false;
		bRebuildLists = true;
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
	if (!SelectedMappingId.IsEmpty() && !ValidMappingIds.Contains(SelectedMappingId))
	{
		CloseMappingEditorWindow();
		LastPreviewMappingId.Reset();
		ClearSelectedMappingId();
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
						.ContentPadding(CompactMappingButtonPadding)
						[
							SNew(STextBlock)
							.Font(CompactMappingButtonFont)
							.Text(LOCTEXT("CtxSelectVisible", "Select Visible"))
						]
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
						.ContentPadding(CompactMappingButtonPadding)
						[
							SNew(STextBlock)
							.Font(CompactMappingButtonFont)
							.Text(LOCTEXT("CtxClearSelection", "Clear Selection"))
						]
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
						.ContentPadding(CompactMappingButtonPadding)
						[
							SNew(STextBlock)
							.Font(CompactMappingButtonFont)
							.Text(LOCTEXT("CtxBulkEnable", "Enable"))
						]
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
						.ContentPadding(CompactMappingButtonPadding)
						[
							SNew(STextBlock)
							.Font(CompactMappingButtonFont)
							.Text(LOCTEXT("CtxBulkDisable", "Disable"))
						]
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
							.ContentPadding(CompactMappingButtonPadding)
							.IsEnabled_Lambda([this]()
							{
								return SelectedContextRows.Num() > 0 || !SelectedContextId.IsEmpty();
							})
							[
								SNew(STextBlock)
								.Font(CompactMappingButtonFont)
								.Text(LOCTEXT("CtxBulkDelete", "Delete"))
							]
							.OnClicked_Lambda([this]()
							{
								if (!GEngine) return FReply::Handled();
								URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
								if (!Subsystem) return FReply::Handled();
								URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
								if (!Manager) return FReply::Handled();

								TSet<FString> TargetIds = SelectedContextRows;
								if (TargetIds.Num() == 0 && !SelectedContextId.IsEmpty())
								{
									TargetIds.Add(SelectedContextId);
								}
								if (TargetIds.Num() == 0)
								{
									if (PreviewLabel.IsValid())
									{
										PreviewLabel->SetText(LOCTEXT("CtxDeleteNeedsSelection", "Select input rows to delete."));
									}
									return FReply::Handled();
								}

								for (const FString& ContextId : TargetIds)
								{
									Manager->DeleteRenderContext(ContextId);
									if (SelectedContextId == ContextId)
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
			else
			{
				ContextList->AddSlot()
				.AutoHeight()
				.Padding(0, 0, 0, 4)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 2, 0).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(SGridPanel)
						.FillColumn(0, 1.6f)
						.FillColumn(1, 0.8f)
						.FillColumn(2, 0.8f)
						.FillColumn(3, 1.0f)
						.FillColumn(4, 0.9f)
						.FillColumn(5, 0.8f)
						+ SGridPanel::Slot(0, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("CtxColName", "Input"))
							.Font(MappingListHeaderFont)
							.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
						]
						+ SGridPanel::Slot(1, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("CtxColSource", "Source"))
							.Font(MappingListHeaderFont)
							.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
						]
						+ SGridPanel::Slot(2, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("CtxColRes", "Res"))
							.Font(MappingListHeaderFont)
							.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
						]
						+ SGridPanel::Slot(3, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("CtxColCapture", "Capture"))
							.Font(MappingListHeaderFont)
							.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
						]
						+ SGridPanel::Slot(4, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("CtxColProject", "Project"))
							.Font(MappingListHeaderFont)
							.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
						]
						+ SGridPanel::Slot(5, 0).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("CtxColStatus", "Status"))
							.Font(MappingListHeaderFont)
							.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
						]
					]
				];
			}

			for (const FRshipRenderContextState& Context : VisibleContexts)
			{
				const FString Name = DisplayTextOrDefault(Context.Name, TEXT("(Unnamed input)"));
				const FString SourceType = Context.SourceType.IsEmpty() ? TEXT("camera") : Context.SourceType;
				const FString Resolution = FString::Printf(TEXT("%dx%d"), Context.Width, Context.Height);
				const FString ProjectText = Context.ProjectId.IsEmpty() ? TEXT("(default)") : Context.ProjectId;
				const FString Capture = Context.CaptureMode.IsEmpty() ? TEXT("default") : Context.CaptureMode;
				const FString Status = Context.bEnabled ? TEXT("Enabled") : TEXT("Disabled");

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
							SNew(SGridPanel)
							.FillColumn(0, 1.6f)
							.FillColumn(1, 0.8f)
							.FillColumn(2, 0.8f)
							.FillColumn(3, 1.0f)
							.FillColumn(4, 0.9f)
							.FillColumn(5, 0.8f)
							+ SGridPanel::Slot(0, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
							[
								SNew(STextBlock)
									.Text(FText::FromString(Name))
									.Font(MappingListFont)
									.ColorAndOpacity(bHasError ? FLinearColor(1.f, 0.5f, 0.4f, 1.f) : FLinearColor::White)
									.AutoWrapText(false)
							]
							+ SGridPanel::Slot(1, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
							[
								SNew(STextBlock)
									.Text(FText::FromString(SourceType))
									.Font(MappingListFont)
									.ColorAndOpacity(bHasError ? FLinearColor(1.f, 0.5f, 0.4f, 1.f) : FLinearColor::White)
									.AutoWrapText(false)
							]
							+ SGridPanel::Slot(2, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
							[
								SNew(STextBlock)
									.Text(FText::FromString(Resolution))
									.Font(MappingListFont)
									.ColorAndOpacity(bHasError ? FLinearColor(1.f, 0.5f, 0.4f, 1.f) : FLinearColor::White)
									.AutoWrapText(false)
							]
							+ SGridPanel::Slot(3, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
							[
								SNew(STextBlock)
									.Text(FText::FromString(Capture))
									.Font(MappingListFont)
									.ColorAndOpacity(bHasError ? FLinearColor(1.f, 0.5f, 0.4f, 1.f) : FLinearColor::White)
									.AutoWrapText(false)
							]
							+ SGridPanel::Slot(4, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
							[
								SNew(STextBlock)
									.Text(FText::FromString(ProjectText))
									.Font(MappingListFont)
									.ColorAndOpacity(bHasError ? FLinearColor(1.f, 0.5f, 0.4f, 1.f) : FLinearColor::White)
									.AutoWrapText(false)
							]
							+ SGridPanel::Slot(5, 0).VAlign(VAlign_Center)
							[
								SNew(STextBlock)
									.Text(FText::FromString(Status))
									.Font(MappingListFont)
									.ColorAndOpacity(Context.bEnabled ? FLinearColor(0.45f, 0.9f, 0.45f, 1.f) : FLinearColor(0.85f, 0.85f, 0.85f, 1.f))
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(1,0,0,0)
						[
							SNew(STextBlock)
								.Text(FText::FromString(Context.LastError))
								.Visibility(bHasError ? EVisibility::Visible : EVisibility::Collapsed)
								.ColorAndOpacity(FLinearColor(1.f, 0.35f, 0.25f, 1.f))
								.Font(CompactErrorFont)
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

		SurfaceList->AddSlot()
		.AutoHeight()
		.Padding(0, 0, 0, 6)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.ContentPadding(CompactMappingButtonPadding)
				[
					SNew(STextBlock)
					.Font(CompactMappingButtonFont)
					.Text(LOCTEXT("SurfQuickCreateFromSelection", "Use Selected As Screen"))
				]
				.OnClicked_Lambda([this]()
				{
					const int32 Created = CreateScreensFromSelectedActors();
					if (PreviewLabel.IsValid())
					{
						if (Created > 0)
						{
							PreviewLabel->SetText(FText::Format(
								LOCTEXT("SurfQuickCreateFromSelectionCreatedFmt", "Created {0} screen(s) from selection."),
								FText::AsNumber(Created)));
						}
						else
						{
							PreviewLabel->SetText(LOCTEXT("SurfQuickCreateFromSelectionNone", "No new screens created. Select mesh actor(s) in the level."));
						}
					}
					return FReply::Handled();
				})
			]
		];

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
						.ContentPadding(CompactMappingButtonPadding)
						[
							SNew(STextBlock)
							.Font(CompactMappingButtonFont)
							.Text(LOCTEXT("SurfSelectVisible", "Select Visible"))
						]
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
						.ContentPadding(CompactMappingButtonPadding)
						[
							SNew(STextBlock)
							.Font(CompactMappingButtonFont)
							.Text(LOCTEXT("SurfClearSelection", "Clear Selection"))
						]
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
						.ContentPadding(CompactMappingButtonPadding)
						[
							SNew(STextBlock)
							.Font(CompactMappingButtonFont)
							.Text(LOCTEXT("SurfBulkEnable", "Enable"))
						]
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
							.ContentPadding(CompactMappingButtonPadding)
							[
								SNew(STextBlock)
								.Font(CompactMappingButtonFont)
								.Text(LOCTEXT("SurfBulkDisable", "Disable"))
							]
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
							.ContentPadding(CompactMappingButtonPadding)
							.IsEnabled_Lambda([this]()
							{
								return SelectedSurfaceRows.Num() > 0 || !SelectedSurfaceId.IsEmpty();
							})
							[
								SNew(STextBlock)
								.Font(CompactMappingButtonFont)
								.Text(LOCTEXT("SurfBulkDelete", "Delete"))
							]
							.OnClicked_Lambda([this]()
							{
								if (!GEngine) return FReply::Handled();
								URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
								if (!Subsystem) return FReply::Handled();
								URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
								if (!Manager) return FReply::Handled();

								TSet<FString> TargetIds = SelectedSurfaceRows;
								if (TargetIds.Num() == 0 && !SelectedSurfaceId.IsEmpty())
								{
									TargetIds.Add(SelectedSurfaceId);
								}
								if (TargetIds.Num() == 0)
								{
									if (PreviewLabel.IsValid())
									{
										PreviewLabel->SetText(LOCTEXT("SurfDeleteNeedsSelection", "Select screen rows to delete."));
									}
									return FReply::Handled();
								}

								for (const FString& SurfaceId : TargetIds)
								{
									Manager->DeleteMappingSurface(SurfaceId);
									if (SelectedSurfaceId == SurfaceId)
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
			else
			{
				SurfaceList->AddSlot()
				.AutoHeight()
				.Padding(0, 0, 0, 4)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 2, 0).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(SGridPanel)
						.FillColumn(0, 1.4f)
						.FillColumn(1, 1.4f)
						.FillColumn(2, 0.6f)
						.FillColumn(3, 0.7f)
						.FillColumn(4, 0.8f)
						.FillColumn(5, 0.8f)
						+ SGridPanel::Slot(0, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SurfColName", "Screen"))
							.Font(MappingListHeaderFont)
							.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
						]
						+ SGridPanel::Slot(1, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SurfColMesh", "Mesh"))
							.Font(MappingListHeaderFont)
							.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
						]
						+ SGridPanel::Slot(2, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SurfColUv", "UV"))
							.Font(MappingListHeaderFont)
							.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
						]
						+ SGridPanel::Slot(3, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SurfColSlots", "Slots"))
							.Font(MappingListHeaderFont)
							.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
						]
						+ SGridPanel::Slot(4, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SurfColProject", "Project"))
							.Font(MappingListHeaderFont)
							.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
						]
						+ SGridPanel::Slot(5, 0).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SurfColEnabled", "Enabled"))
							.Font(MappingListHeaderFont)
							.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
						]
					]
				];
			}

			for (const FRshipMappingSurfaceState& Surface : VisibleSurfaces)
			{
				const FString NameText = Surface.Name;
				const FString MeshNameText = Surface.MeshComponentName;
				TArray<FString> SlotValues;
				SlotValues.Reserve(Surface.MaterialSlots.Num());
				for (int32 Slot : Surface.MaterialSlots)
				{
					SlotValues.Add(FString::FromInt(Slot));
				}
				const FString SlotSummary = SlotValues.Num() == 0 ? TEXT("") : FString::Join(SlotValues, TEXT(","));
				const FString ProjectText = Surface.ProjectId;
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
							SNew(SGridPanel)
							.FillColumn(0, 1.4f)
							.FillColumn(1, 1.4f)
							.FillColumn(2, 0.6f)
							.FillColumn(3, 0.7f)
							.FillColumn(4, 0.8f)
							.FillColumn(5, 0.8f)
							+ SGridPanel::Slot(0, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
							[
								SNew(SEditableTextBox)
								.Text(FText::FromString(NameText))
								.Font(MappingListFont)
								.HintText(LOCTEXT("SurfRowNameHint", "Screen name"))
								.SelectAllTextWhenFocused(true)
								.OnTextCommitted_Lambda([this, Surface](const FText& NewText, ETextCommit::Type CommitType)
								{
									if (CommitType == ETextCommit::Default || !GEngine)
									{
										return;
									}

									URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
									if (!Subsystem)
									{
										return;
									}

									URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
									if (!Manager)
									{
										return;
									}

									FRshipMappingSurfaceState Updated = Surface;
									Updated.Name = NewText.ToString().TrimStartAndEnd();
									Manager->UpdateMappingSurface(Updated);
									RefreshStatus();
								})
							]
							+ SGridPanel::Slot(1, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
							[
								SNew(SEditableTextBox)
								.Text(FText::FromString(MeshNameText))
								.Font(MappingListFont)
								.HintText(LOCTEXT("SurfRowMeshHint", "Mesh component"))
								.SelectAllTextWhenFocused(true)
								.OnTextCommitted_Lambda([this, Surface](const FText& NewText, ETextCommit::Type CommitType)
								{
									if (CommitType == ETextCommit::Default || !GEngine)
									{
										return;
									}

									URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
									if (!Subsystem)
									{
										return;
									}

									URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
									if (!Manager)
									{
										return;
									}

									FRshipMappingSurfaceState Updated = Surface;
									Updated.MeshComponentName = NewText.ToString().TrimStartAndEnd();
									Manager->UpdateMappingSurface(Updated);
									RefreshStatus();
								})
							]
							+ SGridPanel::Slot(2, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
							[
								SNew(SSpinBox<int32>)
								.MinValue(0)
								.MaxValue(7)
								.Value(Surface.UVChannel)
								.OnValueCommitted_Lambda([this, Surface](int32 NewValue, ETextCommit::Type CommitType)
								{
									if (CommitType == ETextCommit::Default || !GEngine)
									{
										return;
									}

									URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
									if (!Subsystem)
									{
										return;
									}

									URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
									if (!Manager)
									{
										return;
									}

									FRshipMappingSurfaceState Updated = Surface;
									Updated.UVChannel = FMath::Clamp(NewValue, 0, 7);
									Manager->UpdateMappingSurface(Updated);
									RefreshStatus();
								})
							]
							+ SGridPanel::Slot(3, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
							[
								SNew(SEditableTextBox)
								.Text(FText::FromString(SlotSummary))
								.Font(MappingListFont)
								.HintText(LOCTEXT("SurfRowSlotsHint", "all slots"))
								.SelectAllTextWhenFocused(true)
								.OnTextCommitted_Lambda([this, Surface](const FText& NewText, ETextCommit::Type CommitType)
								{
									if (CommitType == ETextCommit::Default || !GEngine)
									{
										return;
									}

									URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
									if (!Subsystem)
									{
										return;
									}

									URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
									if (!Manager)
									{
										return;
									}

									FRshipMappingSurfaceState Updated = Surface;
									Updated.MaterialSlots.Empty();

									const FString SlotsText = NewText.ToString().TrimStartAndEnd();
									if (!SlotsText.IsEmpty() && !SlotsText.Equals(TEXT("all"), ESearchCase::IgnoreCase))
									{
										TArray<FString> Parts;
										SlotsText.ParseIntoArray(Parts, TEXT(","), true);
										for (FString Part : Parts)
										{
											Part = Part.TrimStartAndEnd();
											if (!Part.IsEmpty())
											{
												Updated.MaterialSlots.Add(FCString::Atoi(*Part));
											}
										}
									}

									Manager->UpdateMappingSurface(Updated);
									RefreshStatus();
								})
							]
							+ SGridPanel::Slot(4, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
							[
								SNew(SEditableTextBox)
								.Text(FText::FromString(ProjectText))
								.Font(MappingListFont)
								.HintText(LOCTEXT("SurfRowProjectHint", "(default)"))
								.SelectAllTextWhenFocused(true)
								.OnTextCommitted_Lambda([this, Surface](const FText& NewText, ETextCommit::Type CommitType)
								{
									if (CommitType == ETextCommit::Default || !GEngine)
									{
										return;
									}

									URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
									if (!Subsystem)
									{
										return;
									}

									URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
									if (!Manager)
									{
										return;
									}

									FRshipMappingSurfaceState Updated = Surface;
									Updated.ProjectId = NewText.ToString().TrimStartAndEnd();
									Manager->UpdateMappingSurface(Updated);
									RefreshStatus();
								})
							]
							+ SGridPanel::Slot(5, 0).VAlign(VAlign_Center)
							[
								SNew(SCheckBox)
								.IsChecked(Surface.bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
								.OnCheckStateChanged_Lambda([this, Surface](ECheckBoxState NewState)
								{
									if (!GEngine)
									{
										return;
									}

									URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
									if (!Subsystem)
									{
										return;
									}

									URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
									if (!Manager)
									{
										return;
									}

									FRshipMappingSurfaceState Updated = Surface;
									Updated.bEnabled = (NewState == ECheckBoxState::Checked);
									Manager->UpdateMappingSurface(Updated);
									RefreshStatus();
								})
								[
									SNew(STextBlock)
									.Text(LOCTEXT("SurfRowEnabled", "Enabled"))
									.Font(MappingListFont)
								]
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(1,0,0,0)
						[
							SNew(STextBlock)
								.Text(FText::FromString(Surface.LastError))
								.Visibility(bHasError ? EVisibility::Visible : EVisibility::Collapsed)
								.ColorAndOpacity(FLinearColor(1.f, 0.35f, 0.25f, 1.f))
								.Font(CompactErrorFont)
								.AutoWrapText(true)
						]
					]
				];
			}
		}
	}


		if (MappingList.IsValid() && bRebuildLists)
		{
			TMap<FString, FString> SurfaceIdToLabel;
			for (const FRshipMappingSurfaceState& Surface : SortedSurfaces)
			{
				const FString SurfaceLabelBase = DisplayTextOrDefault(Surface.Name, TEXT("Screen"));
				const FString SurfaceLabel = Surface.MeshComponentName.IsEmpty()
					? SurfaceLabelBase
					: FString::Printf(TEXT("%s [%s]"), *SurfaceLabelBase, *Surface.MeshComponentName);
				SurfaceIdToLabel.Add(Surface.Id, SurfaceLabel);

			}

			TMap<FString, FString> ContextIdToLabel;
			TMap<FString, FString> ContextTokenToId;
			for (const FRshipRenderContextState& Context : SortedContexts)
			{
				const FString FallbackLabel = Context.SourceType.Equals(TEXT("camera"), ESearchCase::IgnoreCase)
					? TEXT("Camera Input")
					: TEXT("Input");
				const FString ContextLabel = DisplayTextOrDefault(Context.Name, FallbackLabel);
				ContextIdToLabel.Add(Context.Id, ContextLabel);

				auto AddContextToken = [&ContextTokenToId, &Context](const FString& Token)
				{
					const FString Key = Token.TrimStartAndEnd().ToLower();
					if (!Key.IsEmpty())
					{
						ContextTokenToId.Add(Key, Context.Id);
					}
				};
				AddContextToken(Context.Id);
				AddContextToken(ContextLabel);
				AddContextToken(Context.CameraId);
				AddContextToken(Context.AssetId);
			}

			TMap<FString, FString> CameraTokenToId;
			TMap<FString, FString> CameraIdToLabel;
			for (const TSharedPtr<FRshipIdOption>& CameraOpt : CameraOptions)
			{
				if (!CameraOpt.IsValid())
				{
					continue;
				}

				const FString CameraId = CameraOpt->ResolvedId.IsEmpty() ? CameraOpt->Id : CameraOpt->ResolvedId;
				const FString CameraLabel = CameraOpt->Actor.IsValid()
					? DisplayTextOrDefault(CameraOpt->Actor->GetActorLabel(), TEXT("Camera"))
					: DisplayTextOrDefault(CameraOpt->Label, TEXT("Camera"));
				if (!CameraId.IsEmpty())
				{
					CameraIdToLabel.Add(CameraId, CameraLabel);
				}

				auto AddCameraToken = [&CameraTokenToId, &CameraId](const FString& Token)
				{
					const FString Key = Token.TrimStartAndEnd().ToLower();
					if (!Key.IsEmpty() && !CameraId.IsEmpty())
					{
						CameraTokenToId.Add(Key, CameraId);
					}
				};
				AddCameraToken(CameraOpt->Id);
				AddCameraToken(CameraOpt->ResolvedId);
				AddCameraToken(CameraOpt->Label);
				if (CameraOpt->Actor.IsValid())
				{
					AddCameraToken(CameraOpt->Actor->GetActorLabel());
				}
			}

			TMap<FString, FString> AssetTokenToId;
			TMap<FString, FString> AssetIdToLabel;
			for (const TSharedPtr<FRshipIdOption>& AssetOpt : AssetOptions)
			{
				if (!AssetOpt.IsValid())
				{
					continue;
				}

				const FString AssetId = AssetOpt->Id;
				const FString AssetLabel = DisplayTextOrDefault(AssetOpt->Label, TEXT("Asset"));
				if (!AssetId.IsEmpty())
				{
					AssetIdToLabel.Add(AssetId, AssetLabel);
				}

				auto AddAssetToken = [&AssetTokenToId, &AssetId](const FString& Token)
				{
					const FString Key = Token.TrimStartAndEnd().ToLower();
					if (!Key.IsEmpty() && !AssetId.IsEmpty())
					{
						AssetTokenToId.Add(Key, AssetId);
					}
				};
				AddAssetToken(AssetOpt->Id);
				AddAssetToken(AssetOpt->Label);
				AddAssetToken(AssetOpt->ResolvedId);
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
					.Padding(0, 0, 0, 4)
					[
						SNew(SGridPanel)
						.FillColumn(0, 0.8f)
						.FillColumn(1, 2.1f)
						.FillColumn(2, 0.8f)
						.FillColumn(3, 1.9f)
						.FillColumn(4, 0.65f)
						.FillColumn(5, 0.75f)
						.FillColumn(6, 0.45f)
						+ SGridPanel::Slot(0, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("MapColType", "Type"))
							.Font(MappingListHeaderFont)
							.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
						]
						+ SGridPanel::Slot(1, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("MapColSource", "Source"))
							.Font(MappingListHeaderFont)
							.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
						]
						+ SGridPanel::Slot(2, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("MapColResolution", "Resolution"))
							.Font(MappingListHeaderFont)
							.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
						]
						+ SGridPanel::Slot(3, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("MapColScreens", "Screens"))
							.Font(MappingListHeaderFont)
							.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
						]
						+ SGridPanel::Slot(4, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("MapColOpacity", "Opacity"))
							.Font(MappingListHeaderFont)
							.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
						]
						+ SGridPanel::Slot(5, 0).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("MapColEnabled", "Enabled"))
							.Font(MappingListHeaderFont)
							.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
						]
						+ SGridPanel::Slot(6, 0).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("MapColActions", "Actions"))
							.Font(MappingListHeaderFont)
							.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
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
						const FString MappingMode = GetMappingModeFromState(Mapping);
						const FString SourceText = [&]()
						{
							if (Mapping.ContextId.IsEmpty())
							{
								return FString();
							}
							if (const FString* Label = ContextIdToLabel.Find(Mapping.ContextId))
							{
								return *Label;
							}
							return FString(TEXT("(Input missing)"));
						}();
						const int32 ResolutionWidth = [&]()
						{
							if (Mapping.ContextId.IsEmpty())
							{
								return 0;
							}

							for (const FRshipRenderContextState& Context : SortedContexts)
							{
								if (Context.Id == Mapping.ContextId)
								{
									return Context.Width;
								}
							}
							return 0;
						}();
						const int32 ResolutionHeight = [&]()
						{
							if (Mapping.ContextId.IsEmpty())
							{
								return 0;
							}

							for (const FRshipRenderContextState& Context : SortedContexts)
							{
								if (Context.Id == Mapping.ContextId)
								{
									return Context.Height;
								}
							}
							return 0;
						}();
						const bool bHasError = !Mapping.LastError.IsEmpty();
						const FString LastError = bHasError ? Mapping.LastError : TEXT("");
						TSharedRef<SWrapBox> AssignedScreensWrap = SNew(SWrapBox)
							.UseAllottedSize(true)
							.InnerSlotPadding(FVector2D(2.0f, 1.0f));
						if (Mapping.SurfaceIds.Num() == 0)
						{
							AssignedScreensWrap->AddSlot()
							[
								SNew(STextBlock)
								.Text(LOCTEXT("MapRowNoAssignedScreens", "(No screens)"))
								.Font(MappingListFont)
							];
						}
						else
						{
							for (const FString& SurfaceId : Mapping.SurfaceIds)
							{
								const FString SurfaceLabel = SurfaceIdToLabel.Contains(SurfaceId)
									? SurfaceIdToLabel[SurfaceId]
									: TEXT("(Screen missing)");
								AssignedScreensWrap->AddSlot()
								[
									SNew(SBorder)
									.BorderImage(FAppStyle::GetBrush("NoBorder"))
									.Padding(FMargin(2.0f, 1.0f))
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 2, 0)
										[
											SNew(STextBlock)
											.Text(FText::FromString(SurfaceLabel))
											.Font(MappingListFont)
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
										[
											SNew(SButton)
											.ContentPadding(CompactMappingButtonPadding)
											[
												SNew(STextBlock)
												.Text(LOCTEXT("MapRowRemoveScreen", "x"))
												.Font(CompactMappingButtonFont)
											]
											.OnClicked_Lambda([this, Mapping, SurfaceId]()
											{
												if (!GEngine) return FReply::Handled();
												URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
												if (!Subsystem) return FReply::Handled();
												URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
												if (!Manager) return FReply::Handled();

												FRshipContentMappingState Updated = Mapping;
												for (const FRshipContentMappingState& ExistingMapping : Manager->GetMappings())
												{
													if (ExistingMapping.Id == Mapping.Id)
													{
														Updated = ExistingMapping;
														break;
													}
												}

												if (Updated.SurfaceIds.Remove(SurfaceId) > 0 && Manager->UpdateMapping(Updated))
												{
													Manager->Tick(0.0f);
													RefreshStatus();
												}
												return FReply::Handled();
											})
										]
									]
								];
							}
						}

				MappingList->AddSlot()
				.AutoHeight()
				.Padding(0, 0, 0, 2)
				[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 0, 0)
							[
								SNew(SGridPanel)
								.FillColumn(0, 0.8f)
								.FillColumn(1, 2.1f)
								.FillColumn(2, 0.8f)
								.FillColumn(3, 1.9f)
								.FillColumn(4, 0.65f)
								.FillColumn(5, 0.75f)
								.FillColumn(6, 0.45f)
								+ SGridPanel::Slot(0, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
								[
									SNew(SComboButton)
									.ContentPadding(CompactMappingButtonPadding)
									.OnGetMenuContent_Lambda([this, Mapping]()
									{
										FMenuBuilder MenuBuilder(true, nullptr);
										for (const FString& CandidateMode : GetMappingModeOptions())
										{
											MenuBuilder.AddMenuEntry(
												GetMappingModeOptionLabel(CandidateMode),
												FText::GetEmpty(),
												FSlateIcon(),
												FUIAction(FExecuteAction::CreateLambda([this, Mapping, CandidateMode]()
												{
													if (!GEngine) return;
													URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
													if (!Subsystem) return;
													URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
													if (!Manager) return;

													FRshipContentMappingState Updated = Mapping;
													ApplyModeToMappingState(Updated, CandidateMode);
													Manager->UpdateMapping(Updated);
													Manager->Tick(0.0f);
													RefreshStatus();
												})));
										}
										return MenuBuilder.MakeWidget();
									})
									.ButtonContent()
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 2, 0)
										[
											SNew(SImage)
											.Image(GetMappingTypeIcon(MappingMode))
											.ColorAndOpacity(GetMappingTypeColor(MappingMode))
											.DesiredSizeOverride(MappingTypeIconSize)
										]
										+ SHorizontalBox::Slot().FillWidth(1.0f)
										[
											SNew(STextBlock)
											.Text(GetMappingModeOptionLabel(MappingMode))
											.Font(MappingListFont)
										]
									]
								]
									+ SGridPanel::Slot(1, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 4, 0)
										[
											SNew(SEditableTextBox)
											.Text(FText::FromString(SourceText))
											.Font(MappingListFont)
											.HintText(LOCTEXT("MapRowSourceHint", "Input source (optional)"))
											.SelectAllTextWhenFocused(true)
											.OnTextCommitted_Lambda([this, Mapping, ContextTokenToId, CameraTokenToId, CameraIdToLabel, AssetTokenToId, AssetIdToLabel](const FText& NewText, ETextCommit::Type CommitType)
											{
												if (CommitType == ETextCommit::Default || !GEngine)
												{
													return;
												}

												URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
												if (!Subsystem)
												{
													return;
												}
												URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
												if (!Manager)
												{
													return;
												}

												FRshipContentMappingState Updated = Mapping;
												const FString SourceToken = NewText.ToString().TrimStartAndEnd();
												if (SourceToken.IsEmpty())
												{
													Updated.ContextId.Reset();
												}
												else
												{
													const FString SourceKey = SourceToken.ToLower();
													if (const FString* ExistingContextId = ContextTokenToId.Find(SourceKey))
													{
														Updated.ContextId = *ExistingContextId;
													}
													else if (const FString* CameraId = CameraTokenToId.Find(SourceKey))
													{
														FString ContextIdForCamera;
														const TArray<FRshipRenderContextState> ExistingContexts = Manager->GetRenderContexts();
														for (const FRshipRenderContextState& Context : ExistingContexts)
														{
															const FString ContextSourceType = Context.SourceType.IsEmpty() ? TEXT("camera") : Context.SourceType;
															if (!ContextSourceType.Equals(TEXT("camera"), ESearchCase::IgnoreCase))
															{
																continue;
															}
															if (!Context.CameraId.Equals(*CameraId, ESearchCase::IgnoreCase))
															{
																continue;
															}
															if (!Mapping.ProjectId.IsEmpty() && Context.ProjectId != Mapping.ProjectId)
															{
																continue;
															}
															if (Mapping.ProjectId.IsEmpty() && !Context.ProjectId.IsEmpty())
															{
																continue;
															}
															ContextIdForCamera = Context.Id;
															break;
														}

														if (ContextIdForCamera.IsEmpty())
														{
															FRshipRenderContextState NewCtx;
															NewCtx.Name = CameraIdToLabel.Contains(*CameraId) ? CameraIdToLabel[*CameraId] : TEXT("Camera Input");
															NewCtx.ProjectId = Mapping.ProjectId;
															NewCtx.SourceType = TEXT("camera");
															NewCtx.CameraId = *CameraId;
															NewCtx.Width = 1920;
															NewCtx.Height = 1080;
															NewCtx.CaptureMode = TEXT("FinalColorLDR");
															NewCtx.bEnabled = true;
															ContextIdForCamera = Manager->CreateRenderContext(NewCtx);
														}

														Updated.ContextId = ContextIdForCamera;
													}
													else if (const FString* AssetId = AssetTokenToId.Find(SourceKey))
													{
														FString ContextIdForAsset;
														const TArray<FRshipRenderContextState> ExistingContexts = Manager->GetRenderContexts();
														for (const FRshipRenderContextState& Context : ExistingContexts)
														{
															const FString ContextSourceType = Context.SourceType.IsEmpty() ? TEXT("camera") : Context.SourceType;
															if (!ContextSourceType.Equals(TEXT("asset-store"), ESearchCase::IgnoreCase))
															{
																continue;
															}
															if (!Context.AssetId.Equals(*AssetId, ESearchCase::IgnoreCase))
															{
																continue;
															}
															if (!Mapping.ProjectId.IsEmpty() && Context.ProjectId != Mapping.ProjectId)
															{
																continue;
															}
															if (Mapping.ProjectId.IsEmpty() && !Context.ProjectId.IsEmpty())
															{
																continue;
															}
															ContextIdForAsset = Context.Id;
															break;
														}

														if (ContextIdForAsset.IsEmpty())
														{
															FRshipRenderContextState NewCtx;
															NewCtx.Name = AssetIdToLabel.Contains(*AssetId) ? AssetIdToLabel[*AssetId] : TEXT("Asset Input");
															NewCtx.ProjectId = Mapping.ProjectId;
															NewCtx.SourceType = TEXT("asset-store");
															NewCtx.AssetId = *AssetId;
															NewCtx.Width = 1920;
															NewCtx.Height = 1080;
															NewCtx.CaptureMode = TEXT("FinalColorLDR");
															NewCtx.bEnabled = true;
															ContextIdForAsset = Manager->CreateRenderContext(NewCtx);
														}

														Updated.ContextId = ContextIdForAsset;
													}
													else
													{
														Updated.ContextId = SourceToken;
													}
												}
												EnsureContextDefaultsForMapping(Manager, Updated.ContextId);
												Manager->UpdateMapping(Updated);
												Manager->Tick(0.0f);
												RefreshStatus();
											})
										]
										+ SHorizontalBox::Slot().AutoWidth()
										[
											SNew(SComboButton)
											.ContentPadding(CompactMappingButtonPadding)
											.OnGetMenuContent_Lambda([this, Mapping]()
											{
												FMenuBuilder MenuBuilder(true, nullptr);
												MenuBuilder.AddMenuEntry(
													LOCTEXT("MapRowSourceClear", "Clear Source"),
													FText::GetEmpty(),
													FSlateIcon(),
													FUIAction(FExecuteAction::CreateLambda([this, Mapping]()
													{
														if (!GEngine) return;
														URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
														if (!Subsystem) return;
														URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
														if (!Manager) return;

														FRshipContentMappingState Updated = Mapping;
														Updated.ContextId.Reset();
														Manager->UpdateMapping(Updated);
														Manager->Tick(0.0f);
														RefreshStatus();
													})));
												MenuBuilder.AddSeparator();

												if (ContextOptions.Num() == 0 && CameraOptions.Num() == 0 && AssetOptions.Num() == 0)
												{
													MenuBuilder.AddWidget(
														SNew(STextBlock).Text(LOCTEXT("MapRowNoSources", "No inputs, cameras, or assets found")),
														FText::GetEmpty(),
														true);
												}

												if (ContextOptions.Num() > 0)
												{
													for (const TSharedPtr<FRshipIdOption>& Option : ContextOptions)
													{
														if (!Option.IsValid())
														{
															continue;
														}
														const FString ContextId = Option->Id;
														const FString ContextLabel = Option->Label.IsEmpty() ? Option->Id : Option->Label;
														MenuBuilder.AddMenuEntry(
															FText::FromString(ContextLabel),
															FText::GetEmpty(),
															FSlateIcon(),
															FUIAction(FExecuteAction::CreateLambda([this, Mapping, ContextId]()
															{
																if (!GEngine) return;
																URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
																if (!Subsystem) return;
																URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
																if (!Manager) return;

																FRshipContentMappingState Updated = Mapping;
																Updated.ContextId = ContextId;
																EnsureContextDefaultsForMapping(Manager, Updated.ContextId);
																Manager->UpdateMapping(Updated);
																Manager->Tick(0.0f);
																RefreshStatus();
															})));
													}
												}

												if (CameraOptions.Num() > 0)
												{
													MenuBuilder.AddSeparator();
													for (const TSharedPtr<FRshipIdOption>& CameraOpt : CameraOptions)
													{
														if (!CameraOpt.IsValid())
														{
															continue;
														}
														const FString CameraId = CameraOpt->ResolvedId.IsEmpty() ? CameraOpt->Id : CameraOpt->ResolvedId;
														if (CameraId.IsEmpty())
														{
															continue;
														}
														const FString CameraLabel = CameraOpt->Actor.IsValid()
															? DisplayTextOrDefault(CameraOpt->Actor->GetActorLabel(), TEXT("Camera"))
															: DisplayTextOrDefault(CameraOpt->Label, TEXT("Camera"));
														MenuBuilder.AddMenuEntry(
															FText::FromString(FString::Printf(TEXT("Camera: %s"), *CameraLabel)),
															FText::GetEmpty(),
															FSlateIcon(),
															FUIAction(FExecuteAction::CreateLambda([this, Mapping, CameraId, CameraLabel]()
															{
																if (!GEngine) return;
																URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
																if (!Subsystem) return;
																URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
																if (!Manager) return;

																FString ContextIdForCamera;
																const TArray<FRshipRenderContextState> ExistingContexts = Manager->GetRenderContexts();
																for (const FRshipRenderContextState& Context : ExistingContexts)
																{
																	const FString ContextSourceType = Context.SourceType.IsEmpty() ? TEXT("camera") : Context.SourceType;
																	if (!ContextSourceType.Equals(TEXT("camera"), ESearchCase::IgnoreCase))
																	{
																		continue;
																	}
																	if (!Context.CameraId.Equals(CameraId, ESearchCase::IgnoreCase))
																	{
																		continue;
																	}
																	if (!Mapping.ProjectId.IsEmpty() && Context.ProjectId != Mapping.ProjectId)
																	{
																		continue;
																	}
																	if (Mapping.ProjectId.IsEmpty() && !Context.ProjectId.IsEmpty())
																	{
																		continue;
																	}
																	ContextIdForCamera = Context.Id;
																	break;
																}

																if (ContextIdForCamera.IsEmpty())
																{
																	FRshipRenderContextState NewCtx;
																	NewCtx.Name = CameraLabel;
																	NewCtx.ProjectId = Mapping.ProjectId;
																	NewCtx.SourceType = TEXT("camera");
																	NewCtx.CameraId = CameraId;
																	NewCtx.Width = 1920;
																	NewCtx.Height = 1080;
																	NewCtx.CaptureMode = TEXT("FinalColorLDR");
																	NewCtx.bEnabled = true;
																	ContextIdForCamera = Manager->CreateRenderContext(NewCtx);
																}

																FRshipContentMappingState Updated = Mapping;
																Updated.ContextId = ContextIdForCamera;
																EnsureContextDefaultsForMapping(Manager, Updated.ContextId);
																Manager->UpdateMapping(Updated);
																Manager->Tick(0.0f);
																RefreshStatus();
															})));
													}
												}

												if (AssetOptions.Num() > 0)
												{
													MenuBuilder.AddSeparator();
													for (const TSharedPtr<FRshipIdOption>& AssetOpt : AssetOptions)
													{
														if (!AssetOpt.IsValid() || AssetOpt->Id.IsEmpty())
														{
															continue;
														}

														const FString AssetId = AssetOpt->Id;
														const FString AssetLabel = DisplayTextOrDefault(AssetOpt->Label, TEXT("Asset"));
														MenuBuilder.AddMenuEntry(
															FText::FromString(FString::Printf(TEXT("Asset: %s"), *AssetLabel)),
															FText::GetEmpty(),
															FSlateIcon(),
															FUIAction(FExecuteAction::CreateLambda([this, Mapping, AssetId, AssetLabel]()
															{
																if (!GEngine) return;
																URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
																if (!Subsystem) return;
																URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
																if (!Manager) return;

																FString ContextIdForAsset;
																const TArray<FRshipRenderContextState> ExistingContexts = Manager->GetRenderContexts();
																for (const FRshipRenderContextState& Context : ExistingContexts)
																{
																	const FString ContextSourceType = Context.SourceType.IsEmpty() ? TEXT("camera") : Context.SourceType;
																	if (!ContextSourceType.Equals(TEXT("asset-store"), ESearchCase::IgnoreCase))
																	{
																		continue;
																	}
																	if (!Context.AssetId.Equals(AssetId, ESearchCase::IgnoreCase))
																	{
																		continue;
																	}
																	if (!Mapping.ProjectId.IsEmpty() && Context.ProjectId != Mapping.ProjectId)
																	{
																		continue;
																	}
																	if (Mapping.ProjectId.IsEmpty() && !Context.ProjectId.IsEmpty())
																	{
																		continue;
																	}
																	ContextIdForAsset = Context.Id;
																	break;
																}

																if (ContextIdForAsset.IsEmpty())
																{
																	FRshipRenderContextState NewCtx;
																	NewCtx.Name = AssetLabel;
																	NewCtx.ProjectId = Mapping.ProjectId;
																	NewCtx.SourceType = TEXT("asset-store");
																	NewCtx.AssetId = AssetId;
																	NewCtx.Width = 1920;
																	NewCtx.Height = 1080;
																	NewCtx.CaptureMode = TEXT("FinalColorLDR");
																	NewCtx.bEnabled = true;
																	ContextIdForAsset = Manager->CreateRenderContext(NewCtx);
																}

																FRshipContentMappingState Updated = Mapping;
																Updated.ContextId = ContextIdForAsset;
																EnsureContextDefaultsForMapping(Manager, Updated.ContextId);
																Manager->UpdateMapping(Updated);
																Manager->Tick(0.0f);
																RefreshStatus();
															})));
													}
												}
												return MenuBuilder.MakeWidget();
											})
											.ButtonContent()
											[
												SNew(STextBlock)
												.Text(LOCTEXT("MapRowSourcePick", "Pick"))
												.Font(CompactMappingButtonFont)
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().Padding(2, 0, 0, 0)
										[
											SNew(SButton)
											.ContentPadding(CompactMappingButtonPadding)
											[
												SNew(STextBlock)
												.Text(LOCTEXT("MapRowSourceUseSelected", "Use Selected"))
												.Font(CompactMappingButtonFont)
											]
											.OnClicked_Lambda([this, Mapping, MappingMode]()
											{
												if (!GEngine)
												{
													return FReply::Handled();
												}

												URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
												if (!Subsystem)
												{
													return FReply::Handled();
												}
												URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
												if (!Manager)
												{
													return FReply::Handled();
												}

#if WITH_EDITOR
												if (!GEditor)
												{
													return FReply::Handled();
												}
												USelection* Selection = GEditor->GetSelectedActors();
												if (!Selection)
												{
													return FReply::Handled();
												}

												FString SelectedSourceId;
												FString SelectedSourceLabel;
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
														SelectedSourceId = CameraId;
														SelectedSourceLabel = DisplayTextOrDefault(Actor->GetActorLabel(), TEXT("Camera Input"));
														break;
													}

													// Projection mappings can use selected mesh actors as source anchors.
													if (IsProjectionMode(MappingMode) && ActorIsValidScreenCandidate(Actor))
													{
														SelectedSourceId = ResolveTargetIdForActor(Actor);
														if (SelectedSourceId.IsEmpty())
														{
															SelectedSourceId = Actor->GetActorLabel();
															if (SelectedSourceId.IsEmpty())
															{
																SelectedSourceId = Actor->GetName();
															}
														}
														SelectedSourceLabel = DisplayTextOrDefault(Actor->GetActorLabel(), TEXT("Mesh Input"));
														break;
													}
												}

												if (SelectedSourceId.IsEmpty())
												{
													return FReply::Handled();
												}

												FString ContextIdForSelection;
												const TArray<FRshipRenderContextState> ExistingContexts = Manager->GetRenderContexts();
												for (const FRshipRenderContextState& Context : ExistingContexts)
												{
													const FString ContextSourceType = Context.SourceType.IsEmpty() ? TEXT("camera") : Context.SourceType;
													if (!ContextSourceType.Equals(TEXT("camera"), ESearchCase::IgnoreCase))
													{
														continue;
													}
													if (!Context.CameraId.Equals(SelectedSourceId, ESearchCase::IgnoreCase))
													{
														continue;
													}
													if (!Mapping.ProjectId.IsEmpty() && Context.ProjectId != Mapping.ProjectId)
													{
														continue;
													}
													if (Mapping.ProjectId.IsEmpty() && !Context.ProjectId.IsEmpty())
													{
														continue;
													}
													ContextIdForSelection = Context.Id;
													break;
												}

												if (ContextIdForSelection.IsEmpty())
												{
													FRshipRenderContextState NewCtx;
													NewCtx.Name = SelectedSourceLabel.IsEmpty() ? TEXT("Input Source") : SelectedSourceLabel;
													NewCtx.ProjectId = Mapping.ProjectId;
													NewCtx.SourceType = TEXT("camera");
													NewCtx.CameraId = SelectedSourceId;
													NewCtx.Width = 1920;
													NewCtx.Height = 1080;
													NewCtx.CaptureMode = TEXT("FinalColorLDR");
													NewCtx.bEnabled = true;
													ContextIdForSelection = Manager->CreateRenderContext(NewCtx);
												}

												FRshipContentMappingState Updated = Mapping;
												Updated.ContextId = ContextIdForSelection;
												EnsureContextDefaultsForMapping(Manager, Updated.ContextId);
												Manager->UpdateMapping(Updated);
												Manager->Tick(0.0f);
												RefreshStatus();
#endif
												return FReply::Handled();
											})
										]
										+ SHorizontalBox::Slot().AutoWidth().Padding(2, 0, 0, 0)
										[
											SNew(SButton)
											.ContentPadding(CompactMappingButtonPadding)
											[
												SNew(STextBlock)
												.Text(LOCTEXT("MapRowScreensClear", "Clear"))
												.Font(CompactMappingButtonFont)
											]
											.OnClicked_Lambda([this, Mapping]()
											{
												if (!GEngine) return FReply::Handled();
												URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
												if (!Subsystem) return FReply::Handled();
												URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
												if (!Manager) return FReply::Handled();

												FRshipContentMappingState Updated = Mapping;
												for (const FRshipContentMappingState& ExistingMapping : Manager->GetMappings())
												{
													if (ExistingMapping.Id == Mapping.Id)
													{
														Updated = ExistingMapping;
														break;
													}
												}

												if (Updated.SurfaceIds.Num() == 0)
												{
													return FReply::Handled();
												}

												Updated.SurfaceIds.Empty();
												if (Manager->UpdateMapping(Updated))
												{
													Manager->Tick(0.0f);
													RefreshStatus();
												}
												return FReply::Handled();
											})
										]
									]
									+ SGridPanel::Slot(2, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 2, 0)
										[
										SNew(SSpinBox<int32>)
										.MinValue(1)
										.MaxValue(16384)
										.Delta(1)
										.Value(ResolutionWidth > 0 ? ResolutionWidth : 1920)
										.IsEnabled(!Mapping.ContextId.IsEmpty())
										.OnValueChanged_Lambda([Mapping](int32 NewWidth)
										{
											if (!GEngine || Mapping.ContextId.IsEmpty() || NewWidth <= 0)
											{
												return;
											}

												URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
												if (!Subsystem)
												{
													return;
												}
												URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
												if (!Manager)
												{
													return;
												}

												const TArray<FRshipRenderContextState> ExistingContexts = Manager->GetRenderContexts();
											for (const FRshipRenderContextState& Context : ExistingContexts)
											{
												if (Context.Id != Mapping.ContextId)
												{
													continue;
												}

												const int32 ClampedWidth = FMath::Clamp(NewWidth, 1, 16384);
												const int32 ExistingHeight = Context.Height > 0 ? Context.Height : 1080;
												if (Context.Width == ClampedWidth && Context.Height == ExistingHeight)
												{
													return;
												}

												FRshipRenderContextState UpdatedContext = Context;
												UpdatedContext.Width = ClampedWidth;
												UpdatedContext.Height = ExistingHeight;
												UpdatedContext.CaptureMode = UpdatedContext.CaptureMode.IsEmpty() ? TEXT("FinalColorLDR") : UpdatedContext.CaptureMode;
												UpdatedContext.bEnabled = true;
												Manager->UpdateRenderContext(UpdatedContext);
												return;
											}
										})
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 2, 0)
										[
											SNew(STextBlock)
											.Text(FText::FromString(TEXT("x")))
											.Font(MappingListFont)
										]
										+ SHorizontalBox::Slot().FillWidth(1.0f)
										[
											SNew(SSpinBox<int32>)
											.MinValue(1)
											.MaxValue(16384)
											.Delta(1)
											.Value(ResolutionHeight > 0 ? ResolutionHeight : 1080)
											.IsEnabled(!Mapping.ContextId.IsEmpty())
											.OnValueChanged_Lambda([Mapping](int32 NewHeight)
											{
												if (!GEngine || Mapping.ContextId.IsEmpty() || NewHeight <= 0)
												{
													return;
												}

												URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
												if (!Subsystem)
												{
													return;
												}
												URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
												if (!Manager)
												{
													return;
												}

												const TArray<FRshipRenderContextState> ExistingContexts = Manager->GetRenderContexts();
												for (const FRshipRenderContextState& Context : ExistingContexts)
												{
													if (Context.Id != Mapping.ContextId)
													{
														continue;
													}

													const int32 ClampedHeight = FMath::Clamp(NewHeight, 1, 16384);
													const int32 ExistingWidth = Context.Width > 0 ? Context.Width : 1920;
													if (Context.Height == ClampedHeight && Context.Width == ExistingWidth)
													{
														return;
													}

													FRshipRenderContextState UpdatedContext = Context;
													UpdatedContext.Width = ExistingWidth;
													UpdatedContext.Height = ClampedHeight;
													UpdatedContext.CaptureMode = UpdatedContext.CaptureMode.IsEmpty() ? TEXT("FinalColorLDR") : UpdatedContext.CaptureMode;
													UpdatedContext.bEnabled = true;
													Manager->UpdateRenderContext(UpdatedContext);
													return;
												}
											})
										]
									]
									+ SGridPanel::Slot(3, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 4, 0)
										[
											AssignedScreensWrap
										]
										+ SHorizontalBox::Slot().AutoWidth()
										[
											SNew(SComboButton)
											.ContentPadding(CompactMappingButtonPadding)
											.OnGetMenuContent_Lambda([this, Mapping]()
											{
												FMenuBuilder MenuBuilder(true, nullptr);
												if (SurfaceOptions.Num() == 0)
												{
													MenuBuilder.AddWidget(
														SNew(STextBlock).Text(LOCTEXT("MapRowNoScreens", "No screens found")),
														FText::GetEmpty(),
														true);
												}
												else
												{
													for (const TSharedPtr<FRshipIdOption>& Option : SurfaceOptions)
													{
														if (!Option.IsValid() || Option->Id.IsEmpty())
														{
															continue;
														}
														const FString SurfaceId = Option->Id;
														const FString SurfaceLabel = Option->Label.IsEmpty() ? Option->Id : Option->Label;
														MenuBuilder.AddMenuEntry(
															FText::FromString(SurfaceLabel),
															FText::GetEmpty(),
															FSlateIcon(),
															FUIAction(FExecuteAction::CreateLambda([this, Mapping, SurfaceId]()
															{
																if (!GEngine) return;
																URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
																if (!Subsystem) return;
																URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
																if (!Manager) return;

																FRshipContentMappingState Updated = Mapping;
																for (const FRshipContentMappingState& ExistingMapping : Manager->GetMappings())
																{
																	if (ExistingMapping.Id == Mapping.Id)
																	{
																		Updated = ExistingMapping;
																		break;
																	}
																}

																const int32 PreviousCount = Updated.SurfaceIds.Num();
																Updated.SurfaceIds.AddUnique(SurfaceId);
																if (Updated.SurfaceIds.Num() != PreviousCount && Manager->UpdateMapping(Updated))
																{
																	Manager->Tick(0.0f);
																	RefreshStatus();
																}
															})));
													}
												}
												return MenuBuilder.MakeWidget();
											})
											.ButtonContent()
											[
												SNew(STextBlock)
												.Text(LOCTEXT("MapRowScreensAdd", "Add"))
												.Font(CompactMappingButtonFont)
											]
										]
										+ SHorizontalBox::Slot().AutoWidth().Padding(2, 0, 0, 0)
										[
											SNew(SButton)
											.ContentPadding(CompactMappingButtonPadding)
											[
												SNew(STextBlock)
												.Text(LOCTEXT("MapRowScreensUseSelected", "Use Selected"))
												.Font(CompactMappingButtonFont)
											]
											.OnClicked_Lambda([this, Mapping]()
											{
												if (!GEngine)
												{
													return FReply::Handled();
												}

												URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
												if (!Subsystem)
												{
													return FReply::Handled();
												}
												URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
												if (!Manager)
												{
													return FReply::Handled();
												}

#if WITH_EDITOR
												if (!GEditor)
												{
													return FReply::Handled();
												}
												USelection* Selection = GEditor->GetSelectedActors();

												FRshipContentMappingState WorkingMapping = Mapping;
												for (const FRshipContentMappingState& ExistingMapping : Manager->GetMappings())
												{
													if (ExistingMapping.Id == Mapping.Id)
													{
														WorkingMapping = ExistingMapping;
														break;
													}
												}

												TArray<FString> SelectedSurfaceIds;
												TArray<FRshipMappingSurfaceState> ExistingSurfaces = Manager->GetMappingSurfaces();
												TArray<AActor*> CandidateActors;
												TSet<AActor*> SeenActors;

												auto AddCandidateActor = [&CandidateActors, &SeenActors](AActor* Actor)
												{
													if (Actor && !SeenActors.Contains(Actor))
													{
														SeenActors.Add(Actor);
														CandidateActors.Add(Actor);
													}
												};

												int32 ActorSelectionCount = 0;
												if (Selection)
												{
													for (FSelectionIterator It(*Selection); It; ++It)
													{
														if (AActor* SelectedActor = Cast<AActor>(*It))
														{
															++ActorSelectionCount;
															AddCandidateActor(SelectedActor);
														}
													}
												}

												if (ActorSelectionCount == 0)
												{
													if (USelection* ComponentSelection = GEditor->GetSelectedComponents())
													{
														for (FSelectionIterator It(*ComponentSelection); It; ++It)
														{
															if (UActorComponent* SelectedComponent = Cast<UActorComponent>(*It))
															{
																AddCandidateActor(SelectedComponent->GetOwner());
															}
														}
													}
												}

												if (CandidateActors.Num() == 0)
												{
													if (PreviewLabel.IsValid())
													{
														PreviewLabel->SetText(LOCTEXT("MapRowScreensUseSelectedNoSelection", "No selected actors/components"));
														PreviewLabel->SetColorAndOpacity(FLinearColor::Yellow);
													}
													return FReply::Handled();
												}

												auto MatchesProject = [&WorkingMapping](const FRshipMappingSurfaceState& Surface) -> bool
												{
													if (WorkingMapping.ProjectId.IsEmpty())
													{
														return Surface.ProjectId.IsEmpty();
													}
													return Surface.ProjectId == WorkingMapping.ProjectId;
												};

												for (AActor* Actor : CandidateActors)
												{
													if (!Actor)
													{
														continue;
													}

													TArray<UMeshComponent*> MeshComponents;
													Actor->GetComponents(MeshComponents);
													UMeshComponent* PrimaryMesh = nullptr;
													for (UMeshComponent* MeshComponent : MeshComponents)
													{
														if (MeshComponent)
														{
															PrimaryMesh = MeshComponent;
															break;
														}
													}
													if (!PrimaryMesh)
													{
														continue;
													}

													FString SurfaceId;
													for (const TSharedPtr<FRshipIdOption>& Option : SurfaceOptions)
													{
														if (!Option.IsValid() || !Option->Actor.IsValid())
														{
															continue;
														}
														if (Option->Actor.Get() == Actor)
														{
															SurfaceId = Option->Id;
															break;
														}
													}

													const FString MeshName = PrimaryMesh ? PrimaryMesh->GetName() : TEXT("");
													const FString ActorLabel = Actor->GetActorLabel();
													const FString BaseName = ActorLabel.IsEmpty() ? Actor->GetName() : ActorLabel;
													const FString ActorPath = Actor->GetPathName();

													if (SurfaceId.IsEmpty())
													{
														for (const FRshipMappingSurfaceState& Surface : ExistingSurfaces)
														{
															if (!MatchesProject(Surface))
															{
																continue;
															}
															if (!ActorPath.IsEmpty() && Surface.ActorPath.Equals(ActorPath, ESearchCase::CaseSensitive))
															{
																SurfaceId = Surface.Id;
																break;
															}
														}
													}

													if (SurfaceId.IsEmpty() && !MeshName.IsEmpty())
													{
														FString UniqueMatchId;
														int32 MatchCount = 0;
														for (const FRshipMappingSurfaceState& Surface : ExistingSurfaces)
														{
															if (!MatchesProject(Surface))
															{
																continue;
															}
															if (Surface.MeshComponentName.Equals(MeshName, ESearchCase::IgnoreCase))
															{
																++MatchCount;
																UniqueMatchId = Surface.Id;
																if (MatchCount > 1)
																{
																	UniqueMatchId.Reset();
																	break;
																}
															}
														}
														if (MatchCount == 1 && !UniqueMatchId.IsEmpty())
														{
															SurfaceId = UniqueMatchId;
														}
													}

													if (SurfaceId.IsEmpty() && PrimaryMesh)
													{
														FRshipMappingSurfaceState NewSurface;
														NewSurface.Name = (MeshComponents.Num() > 1)
															? FString::Printf(TEXT("%s / %s"), *BaseName, *MeshName)
															: BaseName;
														NewSurface.ProjectId = WorkingMapping.ProjectId;
														NewSurface.TargetId.Reset();
														NewSurface.UVChannel = 0;
														NewSurface.MeshComponentName = MeshName;
														NewSurface.ActorPath = ActorPath;
														NewSurface.bEnabled = true;
														SurfaceId = Manager->CreateMappingSurface(NewSurface);

														if (!SurfaceId.IsEmpty())
														{
															FRshipMappingSurfaceState CreatedState = NewSurface;
															CreatedState.Id = SurfaceId;
															ExistingSurfaces.Add(CreatedState);
														}
													}

													if (!SurfaceId.IsEmpty())
													{
														SelectedSurfaceIds.AddUnique(SurfaceId);
													}
												}

												if (SelectedSurfaceIds.Num() == 0)
												{
													if (PreviewLabel.IsValid())
													{
														PreviewLabel->SetText(LOCTEXT("MapRowScreensUseSelectedNoValidScreens", "Selection has no valid mesh screens"));
														PreviewLabel->SetColorAndOpacity(FLinearColor::Yellow);
													}
													return FReply::Handled();
												}

												FRshipContentMappingState Updated = WorkingMapping;
												const int32 PreviousCount = Updated.SurfaceIds.Num();
												for (const FString& AddedSurfaceId : SelectedSurfaceIds)
												{
													Updated.SurfaceIds.AddUnique(AddedSurfaceId);
												}
												if (Updated.SurfaceIds.Num() == PreviousCount)
												{
													return FReply::Handled();
												}

												if (Manager->UpdateMapping(Updated))
												{
													Manager->Tick(0.0f);
													RefreshStatus();
													if (PreviewLabel.IsValid())
													{
														PreviewLabel->SetText(FText::Format(
															LOCTEXT("MapRowScreensUseSelectedAddedFmt", "Added {0} screen(s) from selection"),
															FText::AsNumber(Updated.SurfaceIds.Num() - PreviousCount)));
														PreviewLabel->SetColorAndOpacity(FLinearColor::White);
													}
												}
#endif
												return FReply::Handled();
											})
										]
									]
								+ SGridPanel::Slot(4, 0).VAlign(VAlign_Center).Padding(0, 0, 6, 0)
								[
									SNew(SSpinBox<float>)
									.MinValue(0.0f)
									.MaxValue(1.0f)
									.Delta(0.01f)
									.Value(Mapping.Opacity)
									.OnValueChanged_Lambda([Mapping](float NewValue)
									{
										if (!GEngine)
										{
											return;
										}

										URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
										if (!Subsystem)
										{
											return;
										}
										URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
										if (!Manager)
										{
											return;
										}

										const float ClampedOpacity = FMath::Clamp(NewValue, 0.0f, 1.0f);
										const TArray<FRshipContentMappingState> ExistingMappings = Manager->GetMappings();
										for (const FRshipContentMappingState& ExistingMapping : ExistingMappings)
										{
											if (ExistingMapping.Id != Mapping.Id)
											{
												continue;
											}
											if (FMath::IsNearlyEqual(ExistingMapping.Opacity, ClampedOpacity, KINDA_SMALL_NUMBER))
											{
												return;
											}

											FRshipContentMappingState Updated = ExistingMapping;
											Updated.Opacity = ClampedOpacity;
											Manager->UpdateMapping(Updated);
											return;
										}
									})
								]
									+ SGridPanel::Slot(5, 0).VAlign(VAlign_Center)
									[
										SNew(SCheckBox)
										.IsChecked(Mapping.bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
									.OnCheckStateChanged_Lambda([this, MappingId = Mapping.Id](ECheckBoxState NewState)
									{
										if (!GEngine)
										{
											return;
										}
										URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
										if (!Subsystem)
										{
											return;
										}
										URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
										if (!Manager)
										{
											return;
										}

										const TArray<FRshipContentMappingState> ExistingMappings = Manager->GetMappings();
										for (const FRshipContentMappingState& ExistingMapping : ExistingMappings)
										{
											if (ExistingMapping.Id != MappingId)
											{
												continue;
											}

											const bool bEnabled = (NewState == ECheckBoxState::Checked);
											if (ExistingMapping.bEnabled == bEnabled)
											{
												return;
											}

											FRshipContentMappingState Updated = ExistingMapping;
											Updated.bEnabled = bEnabled;
											Manager->UpdateMapping(Updated);
											Manager->Tick(0.0f);
											RefreshStatus();
											return;
										}
									})
									[
										SNew(STextBlock)
											.Text(LOCTEXT("MapRowEnabled", "Enabled"))
											.Font(MappingListFont)
										]
									]
									+ SGridPanel::Slot(6, 0).VAlign(VAlign_Center)
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
										[
											SNew(SButton)
											.ContentPadding(CompactMappingButtonPadding)
											[
												SNew(STextBlock)
												.Text(LOCTEXT("MapRowEdit", "Edit"))
												.Font(CompactMappingButtonFont)
											]
											.OnClicked_Lambda([this, MappingId = Mapping.Id]()
											{
												if (!GEngine) return FReply::Handled();
												URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
												if (!Subsystem) return FReply::Handled();
												URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
												if (!Manager) return FReply::Handled();

												const TArray<FRshipContentMappingState> ExistingMappings = Manager->GetMappings();
												for (const FRshipContentMappingState& ExistingMapping : ExistingMappings)
												{
													if (ExistingMapping.Id != MappingId)
													{
														continue;
													}

													SetSelectedMappingId(MappingId);
													if (!SelectedMappingRows.Contains(MappingId))
													{
														SelectedMappingRows.Empty();
														SelectedMappingRows.Add(MappingId);
													}
													OpenMappingEditorWindow(ExistingMapping);
													return FReply::Handled();
												}

												// Mapping disappeared between list build and click.
												ClearSelectedMappingId();
												SelectedMappingRows.Remove(MappingId);
												RefreshStatus();
												return FReply::Handled();
											})
										]
										+ SHorizontalBox::Slot().AutoWidth()
										[
											SNew(SButton)
											.ContentPadding(CompactMappingButtonPadding)
											[
												SNew(STextBlock)
												.Text(LOCTEXT("MapRowDelete", "X"))
												.Font(CompactMappingButtonFont)
											]
											.OnClicked_Lambda([this, MappingId = Mapping.Id]()
											{
												if (!GEngine) return FReply::Handled();
												URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
												if (!Subsystem) return FReply::Handled();
												URshipContentMappingManager* Manager = Subsystem->GetContentMappingManager();
												if (!Manager) return FReply::Handled();

												const bool bDeletingSelectedMapping = (SelectedMappingId == MappingId);
												const bool bDeletingActiveProjection = (ActiveProjectionMappingId == MappingId);
												if (bDeletingSelectedMapping)
												{
													CloseMappingEditorWindow();
													LastPreviewMappingId.Reset();
												}
												if (bDeletingActiveProjection)
												{
													StopProjectionEdit();
												}

												if (Manager->DeleteMapping(MappingId))
												{
													if (bDeletingSelectedMapping)
													{
														ClearSelectedMappingId();
														ResetForms();
													}
													SelectedMappingRows.Remove(MappingId);
													ExpandedMappingConfigRows.Remove(MappingId);
													ExpandedProjectionPrecisionRows.Remove(MappingId);
													RefreshStatus();
												}
												return FReply::Handled();
											})
										]
									]
								]
							]
						+ SVerticalBox::Slot().AutoHeight().Padding(1, 0, 0, 2)
					[
						SNew(STextBlock)
						.Visibility_Lambda([bHasError]() { return bHasError ? EVisibility::Visible : EVisibility::Collapsed; })
						.Text(FText::FromString(LastError))
						.ColorAndOpacity(FLinearColor(1.f, 0.35f, 0.25f, 1.f))
						.Font(MappingListFont)
						.AutoWrapText(true)
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
