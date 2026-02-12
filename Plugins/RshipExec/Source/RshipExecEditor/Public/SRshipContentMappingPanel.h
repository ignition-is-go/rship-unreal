// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SCompoundWidget.h"

class AActor;
class SVerticalBox;
class STextBlock;
template<typename NumericType> class SSpinBox;
class SRshipModeSelector;
class SRshipMappingCanvas;
class SRshipAngleMaskWidget;
class SRshipContentModeSelector;

class SRshipContentMappingPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRshipContentMappingPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SRshipContentMappingPanel();

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	struct FRshipIdOption
	{
		FString Id;
		FString Label;
		bool bIsSceneCamera = false;
		bool bRequiresConversion = false;
		TWeakObjectPtr<AActor> Actor;
		FString ResolvedId;
	};

	TSharedRef<SWidget> BuildHeaderSection();
	TSharedRef<SWidget> BuildQuickMappingSection();
	TSharedRef<SWidget> BuildContextsSection();
	TSharedRef<SWidget> BuildSurfacesSection();
	TSharedRef<SWidget> BuildMappingsSection();
	TSharedRef<SWidget> BuildContextForm();
	TSharedRef<SWidget> BuildSurfaceForm();
	TSharedRef<SWidget> BuildMappingForm();

	TSharedRef<SWidget> BuildIdPickerMenu(const TArray<TSharedPtr<FRshipIdOption>>& Options, const FText& EmptyText, TSharedPtr<class SEditableTextBox> TargetInput, bool bAppend);
	void RebuildPickerOptions(const TArray<struct FRshipRenderContextState>& Contexts, const TArray<struct FRshipMappingSurfaceState>& Surfaces);
	FString ConvertSceneCamera(AActor* Actor) const;
	void RefreshStatus();
	void PopulateContextForm(const struct FRshipRenderContextState& State);
	void PopulateSurfaceForm(const struct FRshipMappingSurfaceState& State);
	void PopulateMappingForm(const struct FRshipContentMappingState& State);
	void ResetForms();
	class UWorld* GetEditorWorld() const;
	FString ResolveTargetIdInput(const FString& InText) const;
	FString ResolveTargetIdForActor(class AActor* Actor) const;
	FString ResolveCameraIdForActor(class AActor* Actor) const;
	bool TryApplySelectionToTarget(TSharedPtr<class SEditableTextBox> TargetInput, bool bAppend);
	bool TryApplySelectionToCamera(TSharedPtr<class SEditableTextBox> CameraInput);
	static FString ShortTargetLabel(const FString& TargetId);
	void StartProjectionEdit(const struct FRshipContentMappingState& Mapping);
	void StopProjectionEdit();
	void UpdateProjectionFromActor(float DeltaTime);
	void SyncProjectionActorFromMapping(const struct FRshipContentMappingState& Mapping, const struct FRshipRenderContextState* ContextState);
	struct FRshipContentMappingState* FindMappingById(const FString& MappingId, TArray<struct FRshipContentMappingState>& Mappings) const;
	struct FRshipRenderContextState* FindContextById(const FString& ContextId, TArray<struct FRshipRenderContextState>& Contexts) const;
	bool IsProjectionEditActiveFor(const FString& MappingId) const;
	void RebuildFeedRectList();

	struct FFeedRect
	{
		float U = 0.0f;
		float V = 0.0f;
		float W = 1.0f;
		float H = 1.0f;
	};

	TSharedPtr<STextBlock> ConnectionText;
	TSharedPtr<STextBlock> CountsText;
	TSharedPtr<class SEditableTextBox> QuickProjectIdInput;
	TSharedPtr<class SEditableTextBox> QuickSourceIdInput;
	TSharedPtr<class SEditableTextBox> QuickTargetIdInput;
	TSharedPtr<SSpinBox<int32>> QuickWidthInput;
	TSharedPtr<SSpinBox<int32>> QuickHeightInput;
	TSharedPtr<class SEditableTextBox> QuickCaptureModeInput;
	TSharedPtr<SSpinBox<int32>> QuickUvChannelInput;
	TSharedPtr<class SEditableTextBox> QuickMaterialSlotsInput;
	TSharedPtr<class SEditableTextBox> QuickMeshNameInput;
	TSharedPtr<SSpinBox<float>> QuickOpacityInput;
	TSharedPtr<SSpinBox<float>> QuickFeedUInput;
	TSharedPtr<SSpinBox<float>> QuickFeedVInput;
	TSharedPtr<SSpinBox<float>> QuickFeedWInput;
	TSharedPtr<SSpinBox<float>> QuickFeedHInput;
	TSharedPtr<class SEditableTextBox> ContextFilterInput;
	TSharedPtr<SVerticalBox> ContextList;
	TSharedPtr<class SEditableTextBox> SurfaceFilterInput;
	TSharedPtr<SVerticalBox> SurfaceList;
	TSharedPtr<class SEditableTextBox> MappingFilterInput;
	TSharedPtr<SVerticalBox> MappingList;
	TSharedPtr<class SEditableTextBox> CtxNameInput;
	TSharedPtr<class SEditableTextBox> CtxProjectInput;
	TSharedPtr<class SEditableTextBox> CtxSourceTypeInput;
	TSharedPtr<class SEditableTextBox> CtxCameraInput;
	TSharedPtr<class SEditableTextBox> CtxAssetInput;
	TSharedPtr<SSpinBox<int32>> CtxWidthInput;
	TSharedPtr<SSpinBox<int32>> CtxHeightInput;
	TSharedPtr<class SEditableTextBox> CtxCaptureInput;
	TSharedPtr<class SCheckBox> CtxEnabledInput;

	TSharedPtr<class SEditableTextBox> SurfNameInput;
	TSharedPtr<class SEditableTextBox> SurfProjectInput;
	TSharedPtr<class SEditableTextBox> SurfTargetInput;
	TSharedPtr<SSpinBox<int32>> SurfUVInput;
	TSharedPtr<class SEditableTextBox> SurfSlotsInput;
	TSharedPtr<class SEditableTextBox> SurfMeshInput;
	TSharedPtr<class SCheckBox> SurfEnabledInput;

	TSharedPtr<class SEditableTextBox> MapNameInput;
	TSharedPtr<class SEditableTextBox> MapProjectInput;
	TSharedPtr<class SEditableTextBox> MapContextInput;
	TSharedPtr<class SEditableTextBox> MapSurfacesInput;
	TSharedPtr<SSpinBox<float>> MapOpacityInput;
	TSharedPtr<class SCheckBox> MapEnabledInput;
	TSharedPtr<SSpinBox<float>> MapProjPosXInput;
	TSharedPtr<SSpinBox<float>> MapProjPosYInput;
	TSharedPtr<SSpinBox<float>> MapProjPosZInput;
	TSharedPtr<SSpinBox<float>> MapProjRotXInput;
	TSharedPtr<SSpinBox<float>> MapProjRotYInput;
	TSharedPtr<SSpinBox<float>> MapProjRotZInput;
	TSharedPtr<SSpinBox<float>> MapProjFovInput;
	TSharedPtr<SSpinBox<float>> MapProjAspectInput;
	TSharedPtr<SSpinBox<float>> MapProjNearInput;
	TSharedPtr<SSpinBox<float>> MapProjFarInput;
	TSharedPtr<class SEditableTextBox> MapCylAxisInput;
	TSharedPtr<SSpinBox<float>> MapCylRadiusInput;
	TSharedPtr<SSpinBox<float>> MapCylHeightInput;
	TSharedPtr<SSpinBox<float>> MapCylStartInput;
	TSharedPtr<SSpinBox<float>> MapCylEndInput;
	TSharedPtr<SSpinBox<float>> MapUvScaleUInput;
	TSharedPtr<SSpinBox<float>> MapUvScaleVInput;
	TSharedPtr<SSpinBox<float>> MapUvOffsetUInput;
	TSharedPtr<SSpinBox<float>> MapUvOffsetVInput;
	TSharedPtr<SSpinBox<float>> MapUvRotInput;
	TSharedPtr<SSpinBox<float>> MapParallelSizeWInput;
	TSharedPtr<SSpinBox<float>> MapParallelSizeHInput;
	TSharedPtr<SSpinBox<float>> MapSphRadiusInput;
	TSharedPtr<SSpinBox<float>> MapSphHArcInput;
	TSharedPtr<SSpinBox<float>> MapSphVArcInput;
	TSharedPtr<SSpinBox<float>> MapFisheyeFovInput;
	TSharedPtr<class SEditableTextBox> MapFisheyeLensInput;
	TSharedPtr<SSpinBox<float>> MapMeshEyeXInput;
	TSharedPtr<SSpinBox<float>> MapMeshEyeYInput;
	TSharedPtr<SSpinBox<float>> MapMeshEyeZInput;
	TSharedPtr<class SEditableTextBox> MapContentModeInput;
	TSharedPtr<SSpinBox<float>> MapMaskStartInput;
	TSharedPtr<SSpinBox<float>> MapMaskEndInput;
	TSharedPtr<class SCheckBox> MapClipOutsideInput;
	TSharedPtr<SSpinBox<float>> MapBorderExpansionInput;
	TSharedPtr<SSpinBox<float>> MapFeedUInput;
	TSharedPtr<SSpinBox<float>> MapFeedVInput;
	TSharedPtr<SSpinBox<float>> MapFeedWInput;
	TSharedPtr<SSpinBox<float>> MapFeedHInput;
	TSharedPtr<SVerticalBox> MapFeedRectList;
	TMap<FString, FFeedRect> MapFeedRectOverrides;

	// Graphical widgets
	TSharedPtr<SRshipModeSelector> QuickModeSelector;
	TSharedPtr<SRshipModeSelector> MapModeSelector;
	TSharedPtr<SRshipMappingCanvas> MappingCanvas;
	TSharedPtr<SRshipAngleMaskWidget> AngleMaskWidget;
	TSharedPtr<SRshipContentModeSelector> ContentModeSelector;

	TArray<TSharedPtr<FRshipIdOption>> TargetOptions;
	TArray<TSharedPtr<FRshipIdOption>> CameraOptions;
	TArray<TSharedPtr<FRshipIdOption>> AssetOptions;
	TArray<TSharedPtr<FRshipIdOption>> ContextOptions;
	TArray<TSharedPtr<FRshipIdOption>> SurfaceOptions;

	FString SelectedContextId;
	FString SelectedSurfaceId;
	FString SelectedMappingId;
	FString QuickSourceType = TEXT("camera");
	FString QuickMapMode = TEXT("direct");
	FString MapMode = TEXT("direct");
	bool bQuickAdvanced = false;

	// Preview helpers
	TSharedPtr<class SBorder> PreviewBorder;
	TSharedPtr<class SImage> PreviewImage;
	TSharedPtr<class STextBlock> PreviewLabel;
	FSlateBrush ActivePreviewBrush;
	bool bHasActivePreviewBrush = false;
	class UTexture* LastPreviewTexture = nullptr;
	FString LastPreviewMappingId;
	void UpdatePreviewImage(class UTexture* Texture, const struct FRshipContentMappingState& Mapping);

	float TimeSinceLastRefresh = 0.0f;
	float RefreshInterval = 1.0f;
	uint32 LastListHash = 0;
	bool bHasListHash = false;
	uint32 PendingListHash = 0;
	bool bHasPendingListHash = false;

	bool bCoveragePreviewEnabled = false;
	FString ActiveProjectionMappingId;
	TWeakObjectPtr<class ARshipContentMappingPreviewActor> ProjectionActor;
	FTransform LastProjectorTransform = FTransform::Identity;
	float ProjectorUpdateAccumulator = 0.0f;
	FString ContextFilterText;
	FString SurfaceFilterText;
	FString MappingFilterText;
	TSet<FString> SelectedContextRows;
	TSet<FString> SelectedSurfaceRows;
	TSet<FString> SelectedMappingRows;
	TSet<FString> ExpandedMappingConfigRows;
	bool bContextErrorsOnly = false;
	bool bSurfaceErrorsOnly = false;
	bool bMappingErrorsOnly = false;
};
