// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

DECLARE_DELEGATE_TwoParams(FOnAngleMaskChanged, float /*StartDeg*/, float /*EndDeg*/);

class SRshipAngleMaskWidget : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRshipAngleMaskWidget) {}
		SLATE_EVENT(FOnAngleMaskChanged, OnAngleMaskChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

	/** Set angles without firing delegate */
	void SetAngles(float StartDeg, float EndDeg);

private:
	enum class EHandleDrag : uint8 { None, Start, End };

	EHandleDrag HitTestHandle(const FGeometry& MyGeometry, const FVector2D& LocalPos) const;
	FVector2D AngleToPoint(float Degrees, const FVector2D& Center, float Radius) const;
	float PointToAngle(const FVector2D& Point, const FVector2D& Center) const;

	float StartAngle = 0.0f;
	float EndAngle = 360.0f;
	EHandleDrag ActiveDrag = EHandleDrag::None;
	FOnAngleMaskChanged OnAngleMaskChanged;

	static constexpr float WidgetSize = 100.0f;
	static constexpr float HandleRadius = 6.0f;
	static constexpr float HandleHitRadius = 12.0f;
};
