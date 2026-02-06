// Copyright Rocketship. All Rights Reserved.

#include "SRshipMappingCanvas.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Styling/AppStyle.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "SlateOptMacros.h"

#define LOCTEXT_NAMESPACE "SRshipMappingCanvas"

void SRshipMappingCanvas::Construct(const FArguments& InArgs)
{
	DesiredHeight = InArgs._DesiredHeight;
	OnFeedRectChanged = InArgs._OnFeedRectChanged;
	OnUvTransformChanged = InArgs._OnUvTransformChanged;
}

FVector2D SRshipMappingCanvas::ComputeDesiredSize(float) const
{
	return FVector2D(DesiredHeight * (16.0f / 9.0f), DesiredHeight);
}

int32 SRshipMappingCanvas::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D Size = AllottedGeometry.GetLocalSize();

	// Layer 1: Background
	if (bHasTextureBrush && BackgroundTexture.IsValid())
	{
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), &TextureBrush, ESlateDrawEffect::None, FLinearColor::White);
	}
	else
	{
		PaintCheckerboard(AllottedGeometry, OutDrawElements, LayerId);
	}

	// Layer 2: UV grid (for direct/UV modes)
	if (DisplayMode != TEXT("feed"))
	{
		PaintUvGrid(AllottedGeometry, OutDrawElements, LayerId + 1);
	}

	// Layer 3: Feed rect overlay (for feed mode)
	if (DisplayMode == TEXT("feed"))
	{
		PaintFeedRect(AllottedGeometry, OutDrawElements, LayerId + 2);
	}

	// Outer border
	TArray<FVector2D> Border;
	Border.Add(FVector2D(0, 0));
	Border.Add(FVector2D(Size.X, 0));
	Border.Add(FVector2D(Size.X, Size.Y));
	Border.Add(FVector2D(0, Size.Y));
	Border.Add(FVector2D(0, 0));
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 4, AllottedGeometry.ToPaintGeometry(), Border, ESlateDrawEffect::None, FLinearColor(0.3f, 0.3f, 0.3f, 1.0f), true, 1.0f);

	return LayerId + 4;
}

void SRshipMappingCanvas::PaintCheckerboard(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const float CellSize = 16.0f;
	const FLinearColor ColorA(0.12f, 0.12f, 0.12f, 1.0f);
	const FLinearColor ColorB(0.18f, 0.18f, 0.18f, 1.0f);

	const int32 Cols = FMath::CeilToInt(Size.X / CellSize);
	const int32 Rows = FMath::CeilToInt(Size.Y / CellSize);

	for (int32 Row = 0; Row < Rows; ++Row)
	{
		for (int32 Col = 0; Col < Cols; ++Col)
		{
			const FLinearColor& Color = ((Row + Col) % 2 == 0) ? ColorA : ColorB;
			const FVector2D CellPos(Col * CellSize, Row * CellSize);
			const FVector2D CellSz(FMath::Min(CellSize, Size.X - CellPos.X), FMath::Min(CellSize, Size.Y - CellPos.Y));
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(CellSz, FSlateLayoutTransform(CellPos)), FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, Color);
		}
	}
}

void SRshipMappingCanvas::PaintUvGrid(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const FLinearColor GridColor(1.0f, 1.0f, 1.0f, 0.15f);

	// Grid lines transformed by UV transform
	const float RotRad = FMath::DegreesToRadians(UvRotDeg);
	const float CosR = FMath::Cos(RotRad);
	const float SinR = FMath::Sin(RotRad);

	auto TransformUV = [&](float U, float V) -> FVector2D
	{
		// Apply scale
		float SU = U * UvScaleU;
		float SV = V * UvScaleV;
		// Apply rotation around center
		const float CU = SU - 0.5f;
		const float CV = SV - 0.5f;
		float RU = CU * CosR - CV * SinR + 0.5f;
		float RV = CU * SinR + CV * CosR + 0.5f;
		// Apply offset
		RU += UvOffsetU;
		RV += UvOffsetV;
		return FVector2D(RU * Size.X, RV * Size.Y);
	};

	// Draw grid lines (10 subdivisions)
	const int32 GridDivisions = 10;
	for (int32 i = 0; i <= GridDivisions; ++i)
	{
		const float Frac = static_cast<float>(i) / GridDivisions;

		// Horizontal line
		TArray<FVector2D> HLine;
		HLine.Add(TransformUV(0.0f, Frac));
		HLine.Add(TransformUV(1.0f, Frac));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), HLine, ESlateDrawEffect::None, GridColor, true, 1.0f);

		// Vertical line
		TArray<FVector2D> VLine;
		VLine.Add(TransformUV(Frac, 0.0f));
		VLine.Add(TransformUV(Frac, 1.0f));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), VLine, ESlateDrawEffect::None, GridColor, true, 1.0f);
	}

	// Highlight outer border of UV space
	TArray<FVector2D> UvBorder;
	UvBorder.Add(TransformUV(0.0f, 0.0f));
	UvBorder.Add(TransformUV(1.0f, 0.0f));
	UvBorder.Add(TransformUV(1.0f, 1.0f));
	UvBorder.Add(TransformUV(0.0f, 1.0f));
	UvBorder.Add(TransformUV(0.0f, 0.0f));
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), UvBorder, ESlateDrawEffect::None, FLinearColor(1.0f, 1.0f, 1.0f, 0.4f), true, 2.0f);
}

void SRshipMappingCanvas::PaintFeedRect(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const FLinearColor YellowAccent(1.0f, 0.85f, 0.0f, 1.0f);
	const FLinearColor YellowFill(1.0f, 0.85f, 0.0f, 0.1f);

	// Convert normalized feed rect to pixel coords
	const float PX = FeedU * Size.X;
	const float PY = FeedV * Size.Y;
	const float PW = FeedW * Size.X;
	const float PH = FeedH * Size.Y;

	// Fill
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(FVector2D(PW, PH), FSlateLayoutTransform(FVector2D(PX, PY))), FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, YellowFill);

	// Border
	TArray<FVector2D> RectLines;
	RectLines.Add(FVector2D(PX, PY));
	RectLines.Add(FVector2D(PX + PW, PY));
	RectLines.Add(FVector2D(PX + PW, PY + PH));
	RectLines.Add(FVector2D(PX, PY + PH));
	RectLines.Add(FVector2D(PX, PY));
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), RectLines, ESlateDrawEffect::None, YellowAccent, true, 2.0f);

	// Handles (corners + edge midpoints)
	auto DrawHandle = [&](float HX, float HY, bool bActive)
	{
		const FLinearColor HandleColor = bActive ? YellowAccent : FLinearColor::White;
		const FVector2D HandlePos(HX - HandleSize * 0.5f, HY - HandleSize * 0.5f);
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(FVector2D(HandleSize, HandleSize), FSlateLayoutTransform(HandlePos)), FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, HandleColor);
	};

	// Corners
	DrawHandle(PX, PY, ActiveDrag == EDragMode::ResizeTopLeft);
	DrawHandle(PX + PW, PY, ActiveDrag == EDragMode::ResizeTopRight);
	DrawHandle(PX, PY + PH, ActiveDrag == EDragMode::ResizeBottomLeft);
	DrawHandle(PX + PW, PY + PH, ActiveDrag == EDragMode::ResizeBottomRight);

	// Edge midpoints
	DrawHandle(PX + PW * 0.5f, PY, ActiveDrag == EDragMode::ResizeTop);
	DrawHandle(PX + PW * 0.5f, PY + PH, ActiveDrag == EDragMode::ResizeBottom);
	DrawHandle(PX, PY + PH * 0.5f, ActiveDrag == EDragMode::ResizeLeft);
	DrawHandle(PX + PW, PY + PH * 0.5f, ActiveDrag == EDragMode::ResizeRight);

	// Value labels
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 8);
	const FLinearColor LabelColor(1.0f, 1.0f, 1.0f, 0.7f);

	const FString ULabel = FString::Printf(TEXT("U:%.2f"), FeedU);
	const FString VLabel = FString::Printf(TEXT("V:%.2f"), FeedV);
	const FString WLabel = FString::Printf(TEXT("W:%.2f"), FeedW);
	const FString HLabel = FString::Printf(TEXT("H:%.2f"), FeedH);

	// U label at top-left
	FSlateDrawElement::MakeText(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(FVector2D(60, 14), FSlateLayoutTransform(FVector2D(PX + 2, PY + 2))), FText::FromString(ULabel), Font, ESlateDrawEffect::None, LabelColor);
	// V label below U
	FSlateDrawElement::MakeText(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(FVector2D(60, 14), FSlateLayoutTransform(FVector2D(PX + 2, PY + 14))), FText::FromString(VLabel), Font, ESlateDrawEffect::None, LabelColor);
	// W label at bottom edge
	FSlateDrawElement::MakeText(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(FVector2D(60, 14), FSlateLayoutTransform(FVector2D(PX + PW * 0.5f - 20, PY + PH - 14))), FText::FromString(WLabel), Font, ESlateDrawEffect::None, LabelColor);
	// H label at right edge
	FSlateDrawElement::MakeText(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(FVector2D(60, 14), FSlateLayoutTransform(FVector2D(PX + PW - 50, PY + PH * 0.5f - 7))), FText::FromString(HLabel), Font, ESlateDrawEffect::None, LabelColor);
}

SRshipMappingCanvas::EDragMode SRshipMappingCanvas::HitTestHandle(const FGeometry& MyGeometry, const FVector2D& LocalPos) const
{
	if (DisplayMode != TEXT("feed"))
	{
		return EDragMode::UvOffset;
	}

	const FVector2D Size = MyGeometry.GetLocalSize();
	const float PX = FeedU * Size.X;
	const float PY = FeedV * Size.Y;
	const float PW = FeedW * Size.X;
	const float PH = FeedH * Size.Y;

	struct FHandleHit { FVector2D Pos; EDragMode Mode; };
	const TArray<FHandleHit> Handles = {
		{ FVector2D(PX, PY), EDragMode::ResizeTopLeft },
		{ FVector2D(PX + PW, PY), EDragMode::ResizeTopRight },
		{ FVector2D(PX, PY + PH), EDragMode::ResizeBottomLeft },
		{ FVector2D(PX + PW, PY + PH), EDragMode::ResizeBottomRight },
		{ FVector2D(PX + PW * 0.5f, PY), EDragMode::ResizeTop },
		{ FVector2D(PX + PW * 0.5f, PY + PH), EDragMode::ResizeBottom },
		{ FVector2D(PX, PY + PH * 0.5f), EDragMode::ResizeLeft },
		{ FVector2D(PX + PW, PY + PH * 0.5f), EDragMode::ResizeRight },
	};

	for (const FHandleHit& Handle : Handles)
	{
		if (FVector2D::Distance(LocalPos, Handle.Pos) <= HandleHitRadius)
		{
			return Handle.Mode;
		}
	}

	// Check if inside rect -> move
	if (LocalPos.X >= PX && LocalPos.X <= PX + PW && LocalPos.Y >= PY && LocalPos.Y <= PY + PH)
	{
		return EDragMode::MoveRect;
	}

	return EDragMode::None;
}

FReply SRshipMappingCanvas::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	const FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	EDragMode HitMode;
	if (DisplayMode == TEXT("feed"))
	{
		HitMode = HitTestHandle(MyGeometry, LocalPos);
	}
	else
	{
		// UV mode: check for alt+drag = rotate, otherwise offset
		HitMode = MouseEvent.IsAltDown() ? EDragMode::UvRotate : EDragMode::UvOffset;
	}

	if (HitMode == EDragMode::None)
	{
		return FReply::Unhandled();
	}

	ActiveDrag = HitMode;
	DragStartMouse = LocalPos;
	DragStartFeedU = FeedU;
	DragStartFeedV = FeedV;
	DragStartFeedW = FeedW;
	DragStartFeedH = FeedH;
	DragStartUvOffsetU = UvOffsetU;
	DragStartUvOffsetV = UvOffsetV;
	DragStartUvRotDeg = UvRotDeg;

	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SRshipMappingCanvas::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (ActiveDrag == EDragMode::None)
	{
		return FReply::Unhandled();
	}

	const FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const FVector2D Size = MyGeometry.GetLocalSize();
	const FVector2D DeltaPx = LocalPos - DragStartMouse;
	const float DeltaU = DeltaPx.X / FMath::Max(Size.X, 1.0f);
	const float DeltaV = DeltaPx.Y / FMath::Max(Size.Y, 1.0f);

	switch (ActiveDrag)
	{
	case EDragMode::MoveRect:
		FeedU = DragStartFeedU + DeltaU;
		FeedV = DragStartFeedV + DeltaV;
		OnFeedRectChanged.ExecuteIfBound(FeedU, FeedV, FeedW, FeedH);
		break;

	case EDragMode::ResizeTopLeft:
		FeedU = DragStartFeedU + DeltaU;
		FeedV = DragStartFeedV + DeltaV;
		FeedW = FMath::Max(DragStartFeedW - DeltaU, 0.01f);
		FeedH = FMath::Max(DragStartFeedH - DeltaV, 0.01f);
		OnFeedRectChanged.ExecuteIfBound(FeedU, FeedV, FeedW, FeedH);
		break;

	case EDragMode::ResizeTopRight:
		FeedV = DragStartFeedV + DeltaV;
		FeedW = FMath::Max(DragStartFeedW + DeltaU, 0.01f);
		FeedH = FMath::Max(DragStartFeedH - DeltaV, 0.01f);
		OnFeedRectChanged.ExecuteIfBound(FeedU, FeedV, FeedW, FeedH);
		break;

	case EDragMode::ResizeBottomLeft:
		FeedU = DragStartFeedU + DeltaU;
		FeedW = FMath::Max(DragStartFeedW - DeltaU, 0.01f);
		FeedH = FMath::Max(DragStartFeedH + DeltaV, 0.01f);
		OnFeedRectChanged.ExecuteIfBound(FeedU, FeedV, FeedW, FeedH);
		break;

	case EDragMode::ResizeBottomRight:
		FeedW = FMath::Max(DragStartFeedW + DeltaU, 0.01f);
		FeedH = FMath::Max(DragStartFeedH + DeltaV, 0.01f);
		OnFeedRectChanged.ExecuteIfBound(FeedU, FeedV, FeedW, FeedH);
		break;

	case EDragMode::ResizeLeft:
		FeedU = DragStartFeedU + DeltaU;
		FeedW = FMath::Max(DragStartFeedW - DeltaU, 0.01f);
		OnFeedRectChanged.ExecuteIfBound(FeedU, FeedV, FeedW, FeedH);
		break;

	case EDragMode::ResizeRight:
		FeedW = FMath::Max(DragStartFeedW + DeltaU, 0.01f);
		OnFeedRectChanged.ExecuteIfBound(FeedU, FeedV, FeedW, FeedH);
		break;

	case EDragMode::ResizeTop:
		FeedV = DragStartFeedV + DeltaV;
		FeedH = FMath::Max(DragStartFeedH - DeltaV, 0.01f);
		OnFeedRectChanged.ExecuteIfBound(FeedU, FeedV, FeedW, FeedH);
		break;

	case EDragMode::ResizeBottom:
		FeedH = FMath::Max(DragStartFeedH + DeltaV, 0.01f);
		OnFeedRectChanged.ExecuteIfBound(FeedU, FeedV, FeedW, FeedH);
		break;

	case EDragMode::UvOffset:
		UvOffsetU = DragStartUvOffsetU + DeltaU;
		UvOffsetV = DragStartUvOffsetV + DeltaV;
		OnUvTransformChanged.ExecuteIfBound(UvScaleU, UvScaleV, UvOffsetU, UvOffsetV, UvRotDeg);
		break;

	case EDragMode::UvRotate:
	{
		const FVector2D Center = Size * 0.5f;
		const float StartAngle = FMath::Atan2(DragStartMouse.Y - Center.Y, DragStartMouse.X - Center.X);
		const float CurAngle = FMath::Atan2(LocalPos.Y - Center.Y, LocalPos.X - Center.X);
		UvRotDeg = DragStartUvRotDeg + FMath::RadiansToDegrees(CurAngle - StartAngle);
		OnUvTransformChanged.ExecuteIfBound(UvScaleU, UvScaleV, UvOffsetU, UvOffsetV, UvRotDeg);
		break;
	}

	default:
		break;
	}

	return FReply::Handled();
}

FReply SRshipMappingCanvas::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (ActiveDrag != EDragMode::None && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		ActiveDrag = EDragMode::None;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

FReply SRshipMappingCanvas::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (DisplayMode == TEXT("feed"))
	{
		return FReply::Unhandled();
	}

	const float Factor = MouseEvent.GetWheelDelta() > 0 ? 1.05f : (1.0f / 1.05f);
	UvScaleU = FMath::Clamp(UvScaleU * Factor, 0.01f, 100.0f);
	UvScaleV = FMath::Clamp(UvScaleV * Factor, 0.01f, 100.0f);
	OnUvTransformChanged.ExecuteIfBound(UvScaleU, UvScaleV, UvOffsetU, UvOffsetV, UvRotDeg);
	return FReply::Handled();
}

FCursorReply SRshipMappingCanvas::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	if (ActiveDrag != EDragMode::None)
	{
		switch (ActiveDrag)
		{
		case EDragMode::MoveRect:
		case EDragMode::UvOffset:
			return FCursorReply::Cursor(EMouseCursor::GrabHand);
		case EDragMode::ResizeTopLeft:
		case EDragMode::ResizeBottomRight:
			return FCursorReply::Cursor(EMouseCursor::ResizeSouthEast);
		case EDragMode::ResizeTopRight:
		case EDragMode::ResizeBottomLeft:
			return FCursorReply::Cursor(EMouseCursor::ResizeSouthWest);
		case EDragMode::ResizeLeft:
		case EDragMode::ResizeRight:
			return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
		case EDragMode::ResizeTop:
		case EDragMode::ResizeBottom:
			return FCursorReply::Cursor(EMouseCursor::ResizeUpDown);
		case EDragMode::UvRotate:
			return FCursorReply::Cursor(EMouseCursor::GrabHand);
		default:
			break;
		}
	}

	// Hover cursor
	const FVector2D LocalPos = MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition());
	const EDragMode HoverMode = HitTestHandle(MyGeometry, LocalPos);

	switch (HoverMode)
	{
	case EDragMode::MoveRect:
		return FCursorReply::Cursor(EMouseCursor::CardinalCross);
	case EDragMode::ResizeTopLeft:
	case EDragMode::ResizeBottomRight:
		return FCursorReply::Cursor(EMouseCursor::ResizeSouthEast);
	case EDragMode::ResizeTopRight:
	case EDragMode::ResizeBottomLeft:
		return FCursorReply::Cursor(EMouseCursor::ResizeSouthWest);
	case EDragMode::ResizeLeft:
	case EDragMode::ResizeRight:
		return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
	case EDragMode::ResizeTop:
	case EDragMode::ResizeBottom:
		return FCursorReply::Cursor(EMouseCursor::ResizeUpDown);
	default:
		return FCursorReply::Cursor(EMouseCursor::Crosshairs);
	}
}

void SRshipMappingCanvas::SetFeedRect(float U, float V, float W, float H)
{
	FeedU = U;
	FeedV = V;
	FeedW = W;
	FeedH = H;
}

void SRshipMappingCanvas::SetUvTransform(float InScaleU, float InScaleV, float InOffsetU, float InOffsetV, float InRotDeg)
{
	UvScaleU = InScaleU;
	UvScaleV = InScaleV;
	UvOffsetU = InOffsetU;
	UvOffsetV = InOffsetV;
	UvRotDeg = InRotDeg;
}

void SRshipMappingCanvas::SetBackgroundTexture(UTexture* Texture)
{
	if (Texture == BackgroundTexture.Get() && bHasTextureBrush)
	{
		return;
	}

	BackgroundTexture = Texture;
	if (Texture)
	{
		TextureBrush = FSlateBrush();
		TextureBrush.SetResourceObject(Texture);
		TextureBrush.DrawAs = ESlateBrushDrawType::Image;
		TextureBrush.ImageSize = FVector2D(Texture->GetSurfaceWidth(), Texture->GetSurfaceHeight());
		bHasTextureBrush = true;
	}
	else
	{
		TextureBrush = FSlateBrush();
		bHasTextureBrush = false;
	}
}

void SRshipMappingCanvas::SetDisplayMode(const FString& Mode)
{
	DisplayMode = Mode;
}

#undef LOCTEXT_NAMESPACE
