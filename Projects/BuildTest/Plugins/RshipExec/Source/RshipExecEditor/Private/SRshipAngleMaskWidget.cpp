// Copyright Rocketship. All Rights Reserved.

#include "SRshipAngleMaskWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Styling/AppStyle.h"
#include "SlateOptMacros.h"

#define LOCTEXT_NAMESPACE "SRshipAngleMaskWidget"

void SRshipAngleMaskWidget::Construct(const FArguments& InArgs)
{
	OnAngleMaskChanged = InArgs._OnAngleMaskChanged;
}

FVector2D SRshipAngleMaskWidget::ComputeDesiredSize(float) const
{
	return FVector2D(WidgetSize, WidgetSize);
}

int32 SRshipAngleMaskWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const FVector2D Center = Size * 0.5f;
	const float Radius = FMath::Min(Size.X, Size.Y) * 0.42f;

	const FLinearColor CircleColor(0.4f, 0.4f, 0.4f, 1.0f);
	const FLinearColor ArcColor(1.0f, 0.85f, 0.0f, 0.4f);
	const FLinearColor ArcBorder(1.0f, 0.85f, 0.0f, 1.0f);
	const FLinearColor HandleColor(1.0f, 1.0f, 1.0f, 1.0f);
	const FLinearColor DimColor(0.15f, 0.15f, 0.15f, 0.6f);

	// Background
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor(0.08f, 0.08f, 0.08f, 1.0f));

	// Dimmed region (full circle background)
	{
		const int32 Segments = 32;
		TArray<FVector2D> FullCircle;
		for (int32 i = 0; i <= Segments; ++i)
		{
			const float Angle = 2.0f * PI * i / Segments;
			FullCircle.Add(FVector2D(Center.X + FMath::Cos(Angle) * Radius, Center.Y + FMath::Sin(Angle) * Radius));
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), FullCircle, ESlateDrawEffect::None, CircleColor, true, 1.5f);
	}

	// Active arc wedge (filled with radial lines)
	{
		const float Start = FMath::DegreesToRadians(StartAngle - 90.0f); // -90 so 0 degrees = top
		const float End = FMath::DegreesToRadians(EndAngle - 90.0f);
		float ArcSpan = End - Start;
		if (ArcSpan < 0) ArcSpan += 2.0f * PI;

		// Draw radial lines to fill the wedge
		const int32 FillSteps = FMath::Max(2, FMath::RoundToInt(ArcSpan / FMath::DegreesToRadians(2.0f)));
		for (int32 i = 0; i <= FillSteps; ++i)
		{
			const float Frac = static_cast<float>(i) / FillSteps;
			const float Angle = Start + ArcSpan * Frac;
			TArray<FVector2D> Ray;
			Ray.Add(Center);
			Ray.Add(FVector2D(Center.X + FMath::Cos(Angle) * Radius, Center.Y + FMath::Sin(Angle) * Radius));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(), Ray, ESlateDrawEffect::None, ArcColor, true, 1.0f);
		}

		// Arc border lines
		TArray<FVector2D> ArcBorderPts;
		const int32 ArcSegments = FMath::Max(4, FMath::RoundToInt(FMath::Abs(ArcSpan) / FMath::DegreesToRadians(5.0f)));
		for (int32 i = 0; i <= ArcSegments; ++i)
		{
			const float Frac = static_cast<float>(i) / ArcSegments;
			const float Angle = Start + ArcSpan * Frac;
			ArcBorderPts.Add(FVector2D(Center.X + FMath::Cos(Angle) * Radius, Center.Y + FMath::Sin(Angle) * Radius));
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 3, AllottedGeometry.ToPaintGeometry(), ArcBorderPts, ESlateDrawEffect::None, ArcBorder, true, 2.0f);

		// Radial edges of the wedge
		TArray<FVector2D> StartEdge;
		StartEdge.Add(Center);
		StartEdge.Add(FVector2D(Center.X + FMath::Cos(Start) * Radius, Center.Y + FMath::Sin(Start) * Radius));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 3, AllottedGeometry.ToPaintGeometry(), StartEdge, ESlateDrawEffect::None, ArcBorder, true, 1.5f);

		TArray<FVector2D> EndEdge;
		EndEdge.Add(Center);
		EndEdge.Add(FVector2D(Center.X + FMath::Cos(End) * Radius, Center.Y + FMath::Sin(End) * Radius));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 3, AllottedGeometry.ToPaintGeometry(), EndEdge, ESlateDrawEffect::None, ArcBorder, true, 1.5f);
	}

	// Handle dots
	const FVector2D StartPt = AngleToPoint(StartAngle, Center, Radius);
	const FVector2D EndPt = AngleToPoint(EndAngle, Center, Radius);

	const FLinearColor StartHandleColor = (ActiveDrag == EHandleDrag::Start) ? FLinearColor(1.0f, 0.85f, 0.0f, 1.0f) : HandleColor;
	const FLinearColor EndHandleColor = (ActiveDrag == EHandleDrag::End) ? FLinearColor(1.0f, 0.85f, 0.0f, 1.0f) : HandleColor;

	FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 4, AllottedGeometry.ToPaintGeometry(FVector2D(HandleRadius * 2, HandleRadius * 2), FSlateLayoutTransform(FVector2D(StartPt.X - HandleRadius, StartPt.Y - HandleRadius))), FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, StartHandleColor);
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 4, AllottedGeometry.ToPaintGeometry(FVector2D(HandleRadius * 2, HandleRadius * 2), FSlateLayoutTransform(FVector2D(EndPt.X - HandleRadius, EndPt.Y - HandleRadius))), FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, EndHandleColor);

	// Angle labels
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 8);
	const FString StartLabel = FString::Printf(TEXT("%.0f"), StartAngle);
	const FString EndLabel = FString::Printf(TEXT("%.0f"), EndAngle);

	FSlateDrawElement::MakeText(OutDrawElements, LayerId + 5, AllottedGeometry.ToPaintGeometry(FVector2D(40, 14), FSlateLayoutTransform(FVector2D(2, 2))), FText::FromString(TEXT("S:") + StartLabel), Font, ESlateDrawEffect::None, FLinearColor(0.8f, 0.8f, 0.8f));
	FSlateDrawElement::MakeText(OutDrawElements, LayerId + 5, AllottedGeometry.ToPaintGeometry(FVector2D(40, 14), FSlateLayoutTransform(FVector2D(2, 14))), FText::FromString(TEXT("E:") + EndLabel), Font, ESlateDrawEffect::None, FLinearColor(0.8f, 0.8f, 0.8f));

	return LayerId + 5;
}

FVector2D SRshipAngleMaskWidget::AngleToPoint(float Degrees, const FVector2D& Center, float Radius) const
{
	const float Rad = FMath::DegreesToRadians(Degrees - 90.0f); // -90 so 0 = top
	return FVector2D(Center.X + FMath::Cos(Rad) * Radius, Center.Y + FMath::Sin(Rad) * Radius);
}

float SRshipAngleMaskWidget::PointToAngle(const FVector2D& Point, const FVector2D& Center) const
{
	float Deg = FMath::RadiansToDegrees(FMath::Atan2(Point.Y - Center.Y, Point.X - Center.X)) + 90.0f;
	if (Deg < 0) Deg += 360.0f;
	if (Deg >= 360.0f) Deg -= 360.0f;
	return Deg;
}

SRshipAngleMaskWidget::EHandleDrag SRshipAngleMaskWidget::HitTestHandle(const FGeometry& MyGeometry, const FVector2D& LocalPos) const
{
	const FVector2D Size = MyGeometry.GetLocalSize();
	const FVector2D Center = Size * 0.5f;
	const float Radius = FMath::Min(Size.X, Size.Y) * 0.42f;

	const FVector2D StartPt = AngleToPoint(StartAngle, Center, Radius);
	const FVector2D EndPt = AngleToPoint(EndAngle, Center, Radius);

	if (FVector2D::Distance(LocalPos, StartPt) <= HandleHitRadius)
	{
		return EHandleDrag::Start;
	}
	if (FVector2D::Distance(LocalPos, EndPt) <= HandleHitRadius)
	{
		return EHandleDrag::End;
	}
	return EHandleDrag::None;
}

FReply SRshipAngleMaskWidget::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	const FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const EHandleDrag Hit = HitTestHandle(MyGeometry, LocalPos);

	if (Hit == EHandleDrag::None)
	{
		return FReply::Unhandled();
	}

	ActiveDrag = Hit;
	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SRshipAngleMaskWidget::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (ActiveDrag == EHandleDrag::None)
	{
		return FReply::Unhandled();
	}

	const FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const FVector2D Center = MyGeometry.GetLocalSize() * 0.5f;
	float Angle = PointToAngle(LocalPos, Center);

	// Snap to integer degrees
	Angle = FMath::RoundToFloat(Angle);

	if (ActiveDrag == EHandleDrag::Start)
	{
		StartAngle = FMath::Clamp(Angle, 0.0f, 360.0f);
	}
	else
	{
		EndAngle = FMath::Clamp(Angle, 0.0f, 360.0f);
	}

	OnAngleMaskChanged.ExecuteIfBound(StartAngle, EndAngle);
	return FReply::Handled();
}

FReply SRshipAngleMaskWidget::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (ActiveDrag != EHandleDrag::None && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		ActiveDrag = EHandleDrag::None;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

FCursorReply SRshipAngleMaskWidget::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	if (ActiveDrag != EHandleDrag::None)
	{
		return FCursorReply::Cursor(EMouseCursor::GrabHand);
	}

	const FVector2D LocalPos = MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition());
	const EHandleDrag Hit = HitTestHandle(MyGeometry, LocalPos);
	if (Hit != EHandleDrag::None)
	{
		return FCursorReply::Cursor(EMouseCursor::GrabHand);
	}
	return FCursorReply::Unhandled();
}

void SRshipAngleMaskWidget::SetAngles(float InStartDeg, float InEndDeg)
{
	StartAngle = InStartDeg;
	EndAngle = InEndDeg;
}

#undef LOCTEXT_NAMESPACE
