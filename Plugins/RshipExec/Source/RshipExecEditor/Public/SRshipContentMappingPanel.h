// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;
class STextBlock;
template<typename NumericType> class SSpinBox;

class SRshipContentMappingPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRshipContentMappingPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SRshipContentMappingPanel();

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	TSharedRef<SWidget> BuildHeaderSection();
	TSharedRef<SWidget> BuildQuickMappingSection();
	TSharedRef<SWidget> BuildContextsSection();
	TSharedRef<SWidget> BuildSurfacesSection();
	TSharedRef<SWidget> BuildMappingsSection();
	TSharedRef<SWidget> BuildContextForm();
	TSharedRef<SWidget> BuildSurfaceForm();
	TSharedRef<SWidget> BuildMappingForm();

	void RefreshStatus();
	void PopulateContextForm(const struct FRshipRenderContextState& State);
	void PopulateSurfaceForm(const struct FRshipMappingSurfaceState& State);
	void PopulateMappingForm(const struct FRshipContentMappingState& State);
	void ResetForms();
	class UWorld* GetEditorWorld() const;

	TSharedPtr<STextBlock> ConnectionText;
	TSharedPtr<STextBlock> CountsText;
	TSharedPtr<class SEditableTextBox> QuickProjectIdInput;
	TSharedPtr<class SEditableTextBox> QuickSourceIdInput;
	TSharedPtr<class SEditableTextBox> QuickTargetIdInput;
	TSharedPtr<class SSpinBox<int32>> QuickWidthInput;
	TSharedPtr<class SSpinBox<int32>> QuickHeightInput;
	TSharedPtr<class SEditableTextBox> QuickCaptureModeInput;
	TSharedPtr<class SSpinBox<int32>> QuickUvChannelInput;
	TSharedPtr<class SEditableTextBox> QuickMaterialSlotsInput;
	TSharedPtr<class SEditableTextBox> QuickMeshNameInput;
	TSharedPtr<class SSpinBox<float>> QuickOpacityInput;
	TSharedPtr<SVerticalBox> ContextList;
	TSharedPtr<SVerticalBox> SurfaceList;
	TSharedPtr<SVerticalBox> MappingList;
	TSharedPtr<class SEditableTextBox> CtxNameInput;
	TSharedPtr<class SEditableTextBox> CtxProjectInput;
	TSharedPtr<class SEditableTextBox> CtxSourceTypeInput;
	TSharedPtr<class SEditableTextBox> CtxCameraInput;
	TSharedPtr<class SEditableTextBox> CtxAssetInput;
	TSharedPtr<class SSpinBox<int32>> CtxWidthInput;
	TSharedPtr<class SSpinBox<int32>> CtxHeightInput;
	TSharedPtr<class SEditableTextBox> CtxCaptureInput;
	TSharedPtr<class SCheckBox> CtxEnabledInput;

	TSharedPtr<class SEditableTextBox> SurfNameInput;
	TSharedPtr<class SEditableTextBox> SurfProjectInput;
	TSharedPtr<class SEditableTextBox> SurfTargetInput;
	TSharedPtr<class SSpinBox<int32>> SurfUVInput;
	TSharedPtr<class SEditableTextBox> SurfSlotsInput;
	TSharedPtr<class SEditableTextBox> SurfMeshInput;
	TSharedPtr<class SCheckBox> SurfEnabledInput;

	TSharedPtr<class SEditableTextBox> MapNameInput;
	TSharedPtr<class SEditableTextBox> MapProjectInput;
	TSharedPtr<class SEditableTextBox> MapTypeInput;
	TSharedPtr<class SEditableTextBox> MapContextInput;
	TSharedPtr<class SEditableTextBox> MapSurfacesInput;
	TSharedPtr<class SSpinBox<float>> MapOpacityInput;
	TSharedPtr<class SCheckBox> MapEnabledInput;
	TSharedPtr<class SEditableTextBox> MapProjectionTypeInput;
	TSharedPtr<class SSpinBox<float>> MapProjPosXInput;
	TSharedPtr<class SSpinBox<float>> MapProjPosYInput;
	TSharedPtr<class SSpinBox<float>> MapProjPosZInput;
	TSharedPtr<class SSpinBox<float>> MapProjRotXInput;
	TSharedPtr<class SSpinBox<float>> MapProjRotYInput;
	TSharedPtr<class SSpinBox<float>> MapProjRotZInput;
	TSharedPtr<class SSpinBox<float>> MapProjFovInput;
	TSharedPtr<class SSpinBox<float>> MapProjAspectInput;
	TSharedPtr<class SSpinBox<float>> MapProjNearInput;
	TSharedPtr<class SSpinBox<float>> MapProjFarInput;
	TSharedPtr<class SEditableTextBox> MapCylAxisInput;
	TSharedPtr<class SSpinBox<float>> MapCylRadiusInput;
	TSharedPtr<class SSpinBox<float>> MapCylHeightInput;
	TSharedPtr<class SSpinBox<float>> MapCylStartInput;
	TSharedPtr<class SSpinBox<float>> MapCylEndInput;
	TSharedPtr<class SSpinBox<float>> MapUvScaleUInput;
	TSharedPtr<class SSpinBox<float>> MapUvScaleVInput;
	TSharedPtr<class SSpinBox<float>> MapUvOffsetUInput;
	TSharedPtr<class SSpinBox<float>> MapUvOffsetVInput;
	TSharedPtr<class SSpinBox<float>> MapUvRotInput;

	FString SelectedContextId;
	FString SelectedSurfaceId;
	FString SelectedMappingId;
	FString QuickSourceType = TEXT("camera");
	FString QuickMappingType = TEXT("surface-uv");
	bool bQuickAdvanced = false;

	// Preview helpers
	TSharedPtr<class SBorder> PreviewBorder;
	TSharedPtr<class SImage> PreviewImage;
	TSharedPtr<class STextBlock> PreviewLabel;
	class FSlateDynamicImageBrush* ActivePreviewBrush = nullptr;
	const class UTexture* LastPreviewTexture = nullptr;
	FString LastPreviewMappingId;
	void UpdatePreviewImage(const class UTexture* Texture, const struct FRshipContentMappingState& Mapping);

	float TimeSinceLastRefresh = 0.0f;
	float RefreshInterval = 1.0f;
};
