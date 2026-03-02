// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Styling/SlateBrush.h"

struct FRshipCanvasFeedRectEntry
{
	FString SurfaceId;
	FString Label;
	float U = 0.0f;
	float V = 0.0f;
	float W = 1.0f;
	float H = 1.0f;
	bool bActive = false;
};

DECLARE_DELEGATE_FiveParams(FOnFeedRectChanged, const FString& /*SurfaceId*/, float /*U*/, float /*V*/, float /*W*/, float /*H*/);
DECLARE_DELEGATE_OneParam(FOnFeedRectSelectionChanged, const FString& /*SurfaceId*/);
DECLARE_DELEGATE_FiveParams(FOnUvTransformChanged, float /*ScaleU*/, float /*ScaleV*/, float /*OffsetU*/, float /*OffsetV*/, float /*RotDeg*/);

class SRshipMappingCanvas : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRshipMappingCanvas)
		: _DesiredHeight(300.0f)
	{}
		SLATE_ARGUMENT(float, DesiredHeight)
		SLATE_EVENT(FOnFeedRectChanged, OnFeedRectChanged)
		SLATE_EVENT(FOnFeedRectSelectionChanged, OnFeedRectSelectionChanged)
		SLATE_EVENT(FOnUvTransformChanged, OnUvTransformChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

	/** Setters that do NOT fire delegates (prevents loops) */
	void SetFeedRect(float U, float V, float W, float H);
	void SetFeedRects(const TArray<FRshipCanvasFeedRectEntry>& InFeedRects);
	void SetUvTransform(float ScaleU, float ScaleV, float OffsetU, float OffsetV, float RotDeg);
	void SetBackgroundTexture(class UTexture* Texture);
	void SetDisplayMode(const FString& Mode);
	void SetCanvasResolution(int32 WidthPx, int32 HeightPx);
	void SetFeedRectValueModePixels(bool bInPixels);

private:
	enum class EDragMode : uint8
	{
		None,
		MoveRect,
		ResizeTopLeft,
		ResizeTopRight,
		ResizeBottomLeft,
		ResizeBottomRight,
		ResizeLeft,
		ResizeRight,
		ResizeTop,
		ResizeBottom,
		UvOffset,
		UvRotate,
	};

	EDragMode HitTestHandle(const FGeometry& MyGeometry, const FVector2D& LocalPos) const;
	int32 HitTestFeedRectBody(const FGeometry& MyGeometry, const FVector2D& LocalPos) const;
	void PaintCheckerboard(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void PaintUvGrid(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void PaintFeedRect(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void SyncActiveRectFromCachedValues();
	void SyncCachedValuesFromActiveRect();

	float DesiredHeight = 300.0f;

	// Feed rect (normalized 0-1)
	float FeedU = 0.0f;
	float FeedV = 0.0f;
	float FeedW = 1.0f;
	float FeedH = 1.0f;
	TArray<FRshipCanvasFeedRectEntry> FeedRects;
	int32 ActiveFeedRectIndex = INDEX_NONE;

	// UV transform
	float UvScaleU = 1.0f;
	float UvScaleV = 1.0f;
	float UvOffsetU = 0.0f;
	float UvOffsetV = 0.0f;
	float UvRotDeg = 0.0f;

	// Display mode
	FString DisplayMode = TEXT("feed");
	int32 CanvasWidthPx = 1920;
	int32 CanvasHeightPx = 1080;
	bool bFeedRectValuesArePixels = false;

	// Texture
	TWeakObjectPtr<class UTexture> BackgroundTexture;
	FSlateBrush TextureBrush;
	bool bHasTextureBrush = false;

	// Interaction state
	EDragMode ActiveDrag = EDragMode::None;
	FVector2D DragStartMouse = FVector2D::ZeroVector;
	float DragStartFeedU = 0.0f;
	float DragStartFeedV = 0.0f;
	float DragStartFeedW = 0.0f;
	float DragStartFeedH = 0.0f;
	float DragStartUvOffsetU = 0.0f;
	float DragStartUvOffsetV = 0.0f;
	float DragStartUvRotDeg = 0.0f;

	// Delegates
	FOnFeedRectChanged OnFeedRectChanged;
	FOnFeedRectSelectionChanged OnFeedRectSelectionChanged;
	FOnUvTransformChanged OnUvTransformChanged;

	static constexpr float HandleSize = 12.0f;
	static constexpr float HandleHitRadius = 20.0f;
};
