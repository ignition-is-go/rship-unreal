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
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	struct FFeedSourceV2;
	struct FFeedDestinationV2;
	struct FFeedRouteV2;

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
	FString ResolveScreenIdInput(const FString& InText) const;
	class URshipTargetComponent* EnsureTargetComponentForActor(class AActor* Actor) const;
	FString ResolveTargetIdForActor(class AActor* Actor) const;
	FString ResolveScreenIdForActor(class AActor* Actor) const;
	FString ResolveCameraIdForActor(class AActor* Actor) const;
	bool TryApplySelectionToTarget(TSharedPtr<class SEditableTextBox> TargetInput, bool bAppend);
	bool TryApplySelectionToCamera(TSharedPtr<class SEditableTextBox> CameraInput);
	int32 CreateScreensFromSelectedActors();
	static FString ShortTargetLabel(const FString& TargetId);
	void StartProjectionEdit(const struct FRshipContentMappingState& Mapping);
	void StopProjectionEdit();
	void UpdateProjectionFromActor(float DeltaTime);
	void SyncProjectionActorFromMapping(const struct FRshipContentMappingState& Mapping, const struct FRshipRenderContextState* ContextState);
	struct FRshipContentMappingState* FindMappingById(const FString& MappingId, TArray<struct FRshipContentMappingState>& Mappings) const;
	struct FRshipRenderContextState* FindContextById(const FString& ContextId, TArray<struct FRshipRenderContextState>& Contexts) const;
	bool IsProjectionEditActiveFor(const FString& MappingId) const;
	bool IsProjectionPrecisionControlsVisible() const;
	bool IsProjectionPrecisionControlsCollapsed() const;
	EVisibility GetProjectionPrecisionControlsVisibility() const;
	EVisibility GetProjectionPrecisionControlsCollapsedVisibility() const;
	bool IsMappingConfigExpanded(const FString& MappingId) const;
	bool IsInlineProjectionPrecisionExpanded(const FString& MappingId) const;
	bool IsProjectionPrecisionControlsVisibleForInlineMapping(const FString& MappingId, bool bInlineProjection) const;
	bool IsProjectionPrecisionControlsNoticeVisibleForInlineMapping(const FString& MappingId, bool bInlineProjection) const;
	void SetInlineProjectionConfigExpanded(const FString& MappingId, bool bExpanded);
	void ToggleMappingConfigExpanded(const FString& MappingId, bool bInlineProjection);
	void SetMappingConfigExpanded(const FString& MappingId, bool bExpanded);
	void SetSelectedMappingId(const FString& NewSelectedMappingId);
	void ClearSelectedMappingId();
	void OpenMappingEditorWindow(const struct FRshipContentMappingState& Mapping);
	void CloseMappingEditorWindow();
	bool ExecuteQuickCreateMapping();
	void StoreQuickCreateDefaults();
	void ApplyStoredQuickCreateDefaults();
	bool DuplicateSelectedMappings();
	bool ToggleSelectedMappingsEnabled();
	void SetSelectedMappingsConfigExpanded(bool bExpanded);
	void RebuildFeedRectList();
	void RefreshMappingCanvasFeedRects();
	void ResetFeedV2State();
	void PopulateFeedV2FromMapping(const struct FRshipContentMappingState& State);
	void RebuildFeedV2Lists();
	void RefreshFeedV2Canvases();
	void WriteFeedV2Config(const TSharedPtr<class FJsonObject>& Config) const;
	FFeedSourceV2* FindFeedSourceById(const FString& Id);
	FFeedDestinationV2* FindFeedDestinationById(const FString& Id);
	FFeedRouteV2* FindFeedRouteById(const FString& Id);
	bool TryGetFeedSourceDimensions(const FString& SourceId, int32& OutWidth, int32& OutHeight) const;
	bool TryGetFeedDestinationDimensions(const FString& DestinationId, int32& OutWidth, int32& OutHeight) const;
	TArray<FString> GetCurrentMappingSurfaceIds() const;
	void ClampFeedRouteToCanvas(FFeedRouteV2& Route);
	void ClampAllFeedRoutesToCanvases();
	void EnsureFeedSourcesBoundToContext(const FString& DefaultContextId);
	void EnsureFeedDestinationsBoundToSurfaces(const TArray<FString>& MappingSurfaceIds);
	void EnsureFeedRoutesForDestinations(const TArray<FString>& MappingSurfaceIds);
	bool ApplyCurrentFormToSelectedMapping(bool bCreateIfMissing);
	uint32 ComputeMappingFormLiveHash() const;

	struct FFeedRect
	{
		float U = 0.0f;
		float V = 0.0f;
		float W = 1.0f;
		float H = 1.0f;
	};

	struct FFeedSourceV2
	{
		FString Id;
		FString Label;
		FString ContextId;
		int32 Width = 1920;
		int32 Height = 1080;
	};

	struct FFeedDestinationV2
	{
		FString Id;
		FString Label;
		FString SurfaceId;
		int32 Width = 1920;
		int32 Height = 1080;
	};

	struct FFeedRouteV2
	{
		FString Id;
		FString Label;
		FString SourceId;
		FString DestinationId;
		int32 SourceX = 0;
		int32 SourceY = 0;
		int32 SourceW = 1920;
		int32 SourceH = 1080;
		int32 DestinationX = 0;
		int32 DestinationY = 0;
		int32 DestinationW = 1920;
		int32 DestinationH = 1080;
		float Opacity = 1.0f;
		bool bEnabled = true;
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
	TArray<TSharedPtr<SSpinBox<float>>> MapCustomMatrixInputs;
	TSharedPtr<SVerticalBox> MapFeedRectList;
	TSharedPtr<SVerticalBox> MapFeedSourceList;
	TSharedPtr<SVerticalBox> MapFeedDestinationList;
	TSharedPtr<SVerticalBox> MapFeedRouteList;
	TMap<FString, FFeedRect> MapFeedRectOverrides;
	FString ActiveFeedSurfaceId;
	TArray<FFeedSourceV2> MapFeedSources;
	TArray<FFeedDestinationV2> MapFeedDestinations;
	TArray<FFeedRouteV2> MapFeedRoutes;
	FString ActiveFeedSourceId;
	FString ActiveFeedDestinationId;
	FString ActiveFeedRouteId;

	// Graphical widgets
	TSharedPtr<SRshipModeSelector> QuickModeSelector;
	TSharedPtr<SRshipModeSelector> MapModeSelector;
	TSharedPtr<SRshipMappingCanvas> MappingCanvas;
	TSharedPtr<SRshipMappingCanvas> FeedSourceCanvas;
	TSharedPtr<SRshipMappingCanvas> FeedDestinationCanvas;
	TSharedPtr<SVerticalBox> FeedDestinationCanvasList;
	TSharedPtr<SRshipAngleMaskWidget> AngleMaskWidget;
	TSharedPtr<SRshipContentModeSelector> ContentModeSelector;
	TSharedPtr<class SWindow> MappingEditorWindow;

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
	float RefreshInterval = 0.1f;
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
	TSet<FString> ExpandedProjectionPrecisionRows;
	bool bShowProjectionPrecisionControls = false;
	bool bContextErrorsOnly = false;
	bool bSurfaceErrorsOnly = false;
	bool bMappingErrorsOnly = false;
	uint32 LastLiveMappingFormHash = 0;
	bool bHasLiveMappingFormHash = false;
	bool bSuspendLiveMappingSync = false;
};
