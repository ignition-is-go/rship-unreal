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
	OnFeedRectSelectionChanged = InArgs._OnFeedRectSelectionChanged;
	OnUvTransformChanged = InArgs._OnUvTransformChanged;
}

FVector2D SRshipMappingCanvas::ComputeDesiredSize(float) const
{
	if (DisplayMode == TEXT("feed") && bFeedRectValuesArePixels)
	{
		return FVector2D(
			static_cast<float>(FMath::Max(1, CanvasWidthPx)),
			static_cast<float>(FMath::Max(1, CanvasHeightPx)));
	}

	const float SafeHeight = FMath::Max(1.0f, static_cast<float>(CanvasHeightPx));
	const float Aspect = FMath::Max(0.01f, static_cast<float>(CanvasWidthPx) / SafeHeight);
	return FVector2D(DesiredHeight * Aspect, DesiredHeight);
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
	const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Regular", 8);
	const float SafeCanvasW = static_cast<float>(FMath::Max(1, CanvasWidthPx));
	const float SafeCanvasH = static_cast<float>(FMath::Max(1, CanvasHeightPx));
	auto ToNormX = [this, SafeCanvasW](float Value) -> float
	{
		return bFeedRectValuesArePixels ? (Value / SafeCanvasW) : Value;
	};
	auto ToNormY = [this, SafeCanvasH](float Value) -> float
	{
		return bFeedRectValuesArePixels ? (Value / SafeCanvasH) : Value;
	};

	for (int32 Index = 0; Index < FeedRects.Num(); ++Index)
	{
		const FRshipCanvasFeedRectEntry& Rect = FeedRects[Index];
		const bool bIsActive = Index == ActiveFeedRectIndex;
		const uint8 Hue = static_cast<uint8>((Index * 43) % 255);
		const FLinearColor Accent = bIsActive
			? FLinearColor::MakeFromHSV8(Hue, 180, 255)
			: FLinearColor::MakeFromHSV8(Hue, 120, 220).CopyWithNewOpacity(0.85f);
		const FLinearColor Fill = Accent.CopyWithNewOpacity(bIsActive ? 0.17f : 0.08f);

		const float PX = ToNormX(Rect.U) * Size.X;
		const float PY = ToNormY(Rect.V) * Size.Y;
		const float PW = ToNormX(Rect.W) * Size.X;
		const float PH = ToNormY(Rect.H) * Size.Y;

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(FVector2D(PW, PH), FSlateLayoutTransform(FVector2D(PX, PY))),
			FAppStyle::GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			Fill);

		TArray<FVector2D> RectLines;
		RectLines.Add(FVector2D(PX, PY));
		RectLines.Add(FVector2D(PX + PW, PY));
		RectLines.Add(FVector2D(PX + PW, PY + PH));
		RectLines.Add(FVector2D(PX, PY + PH));
		RectLines.Add(FVector2D(PX, PY));
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(),
			RectLines,
			ESlateDrawEffect::None,
			Accent,
			true,
			bIsActive ? 2.0f : 1.0f);

		const FString LabelText = Rect.Label.IsEmpty() ? Rect.SurfaceId : Rect.Label;
		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId + 2,
			AllottedGeometry.ToPaintGeometry(FVector2D(200.0f, 12.0f), FSlateLayoutTransform(FVector2D(PX + 2.0f, PY + 2.0f))),
			FText::FromString(LabelText),
			LabelFont,
			ESlateDrawEffect::None,
			Accent.CopyWithNewOpacity(0.95f));
	}

	if (ActiveFeedRectIndex == INDEX_NONE || !FeedRects.IsValidIndex(ActiveFeedRectIndex))
	{
		return;
	}

	const FLinearColor ActiveAccent(1.0f, 0.85f, 0.0f, 1.0f);
	const float PX = ToNormX(FeedU) * Size.X;
	const float PY = ToNormY(FeedV) * Size.Y;
	const float PW = ToNormX(FeedW) * Size.X;
	const float PH = ToNormY(FeedH) * Size.Y;

	auto DrawHandle = [&](float HX, float HY, bool bActiveHandle)
	{
		const FLinearColor HandleColor = bActiveHandle ? ActiveAccent : FLinearColor::White;
		const FVector2D HandlePos(HX - HandleSize * 0.5f, HY - HandleSize * 0.5f);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 3,
			AllottedGeometry.ToPaintGeometry(FVector2D(HandleSize, HandleSize), FSlateLayoutTransform(HandlePos)),
			FAppStyle::GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			HandleColor);
	};

	DrawHandle(PX, PY, ActiveDrag == EDragMode::ResizeTopLeft);
	DrawHandle(PX + PW, PY, ActiveDrag == EDragMode::ResizeTopRight);
	DrawHandle(PX, PY + PH, ActiveDrag == EDragMode::ResizeBottomLeft);
	DrawHandle(PX + PW, PY + PH, ActiveDrag == EDragMode::ResizeBottomRight);
	DrawHandle(PX + PW * 0.5f, PY, ActiveDrag == EDragMode::ResizeTop);
	DrawHandle(PX + PW * 0.5f, PY + PH, ActiveDrag == EDragMode::ResizeBottom);
	DrawHandle(PX, PY + PH * 0.5f, ActiveDrag == EDragMode::ResizeLeft);
	DrawHandle(PX + PW, PY + PH * 0.5f, ActiveDrag == EDragMode::ResizeRight);

	const FString ULabel = bFeedRectValuesArePixels
		? FString::Printf(TEXT("X:%dpx"), FMath::RoundToInt(FeedU))
		: FString::Printf(TEXT("U:%.3f"), FeedU);
	const FString VLabel = bFeedRectValuesArePixels
		? FString::Printf(TEXT("Y:%dpx"), FMath::RoundToInt(FeedV))
		: FString::Printf(TEXT("V:%.3f"), FeedV);
	const FString WLabel = bFeedRectValuesArePixels
		? FString::Printf(TEXT("W:%dpx"), FMath::RoundToInt(FeedW))
		: FString::Printf(TEXT("W:%.3f"), FeedW);
	const FString HLabel = bFeedRectValuesArePixels
		? FString::Printf(TEXT("H:%dpx"), FMath::RoundToInt(FeedH))
		: FString::Printf(TEXT("H:%.3f"), FeedH);
	const FLinearColor LabelColor(1.0f, 1.0f, 1.0f, 0.85f);

	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId + 4,
		AllottedGeometry.ToPaintGeometry(FVector2D(72.0f, 12.0f), FSlateLayoutTransform(FVector2D(PX + 2.0f, PY + 14.0f))),
		FText::FromString(ULabel),
		LabelFont,
		ESlateDrawEffect::None,
		LabelColor);
	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId + 4,
		AllottedGeometry.ToPaintGeometry(FVector2D(72.0f, 12.0f), FSlateLayoutTransform(FVector2D(PX + 2.0f, PY + 26.0f))),
		FText::FromString(VLabel),
		LabelFont,
		ESlateDrawEffect::None,
		LabelColor);
	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId + 4,
		AllottedGeometry.ToPaintGeometry(FVector2D(72.0f, 12.0f), FSlateLayoutTransform(FVector2D(PX + PW * 0.5f - 22.0f, PY + PH - 14.0f))),
		FText::FromString(WLabel),
		LabelFont,
		ESlateDrawEffect::None,
		LabelColor);
	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId + 4,
		AllottedGeometry.ToPaintGeometry(FVector2D(72.0f, 12.0f), FSlateLayoutTransform(FVector2D(PX + PW - 54.0f, PY + PH * 0.5f - 6.0f))),
		FText::FromString(HLabel),
		LabelFont,
		ESlateDrawEffect::None,
		LabelColor);
}

SRshipMappingCanvas::EDragMode SRshipMappingCanvas::HitTestHandle(const FGeometry& MyGeometry, const FVector2D& LocalPos) const
{
	if (DisplayMode != TEXT("feed"))
	{
		return EDragMode::UvOffset;
	}

	if (ActiveFeedRectIndex == INDEX_NONE || !FeedRects.IsValidIndex(ActiveFeedRectIndex))
	{
		return EDragMode::None;
	}

	const FVector2D Size = MyGeometry.GetLocalSize();
	const float SafeCanvasW = static_cast<float>(FMath::Max(1, CanvasWidthPx));
	const float SafeCanvasH = static_cast<float>(FMath::Max(1, CanvasHeightPx));
	const float U = bFeedRectValuesArePixels ? (FeedU / SafeCanvasW) : FeedU;
	const float V = bFeedRectValuesArePixels ? (FeedV / SafeCanvasH) : FeedV;
	const float W = bFeedRectValuesArePixels ? (FeedW / SafeCanvasW) : FeedW;
	const float H = bFeedRectValuesArePixels ? (FeedH / SafeCanvasH) : FeedH;
	const float PX = U * Size.X;
	const float PY = V * Size.Y;
	const float PW = W * Size.X;
	const float PH = H * Size.Y;

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

int32 SRshipMappingCanvas::HitTestFeedRectBody(const FGeometry& MyGeometry, const FVector2D& LocalPos) const
{
	if (DisplayMode != TEXT("feed"))
	{
		return INDEX_NONE;
	}

	const FVector2D Size = MyGeometry.GetLocalSize();
	const float SafeCanvasW = static_cast<float>(FMath::Max(1, CanvasWidthPx));
	const float SafeCanvasH = static_cast<float>(FMath::Max(1, CanvasHeightPx));
	for (int32 Index = FeedRects.Num() - 1; Index >= 0; --Index)
	{
		const FRshipCanvasFeedRectEntry& Rect = FeedRects[Index];
		const float U = bFeedRectValuesArePixels ? (Rect.U / SafeCanvasW) : Rect.U;
		const float V = bFeedRectValuesArePixels ? (Rect.V / SafeCanvasH) : Rect.V;
		const float W = bFeedRectValuesArePixels ? (Rect.W / SafeCanvasW) : Rect.W;
		const float H = bFeedRectValuesArePixels ? (Rect.H / SafeCanvasH) : Rect.H;
		const float PX = U * Size.X;
		const float PY = V * Size.Y;
		const float PW = W * Size.X;
		const float PH = H * Size.Y;
		if (LocalPos.X >= PX && LocalPos.X <= PX + PW && LocalPos.Y >= PY && LocalPos.Y <= PY + PH)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void SRshipMappingCanvas::SyncActiveRectFromCachedValues()
{
	if (FeedRects.IsValidIndex(ActiveFeedRectIndex))
	{
		FRshipCanvasFeedRectEntry& ActiveRect = FeedRects[ActiveFeedRectIndex];
		ActiveRect.U = FeedU;
		ActiveRect.V = FeedV;
		ActiveRect.W = FeedW;
		ActiveRect.H = FeedH;
	}
}

void SRshipMappingCanvas::SyncCachedValuesFromActiveRect()
{
	if (FeedRects.IsValidIndex(ActiveFeedRectIndex))
	{
		const FRshipCanvasFeedRectEntry& ActiveRect = FeedRects[ActiveFeedRectIndex];
		FeedU = ActiveRect.U;
		FeedV = ActiveRect.V;
		FeedW = ActiveRect.W;
		FeedH = ActiveRect.H;
	}
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
		const int32 HitRectIndex = HitTestFeedRectBody(MyGeometry, LocalPos);
		if (HitRectIndex == INDEX_NONE)
		{
			return FReply::Unhandled();
		}

		if (HitRectIndex != ActiveFeedRectIndex)
		{
			ActiveFeedRectIndex = HitRectIndex;
			SyncCachedValuesFromActiveRect();
			if (FeedRects.IsValidIndex(ActiveFeedRectIndex))
			{
				OnFeedRectSelectionChanged.ExecuteIfBound(FeedRects[ActiveFeedRectIndex].SurfaceId);
			}
		}

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
	const float DeltaX = bFeedRectValuesArePixels
		? (DeltaPx.X * static_cast<float>(FMath::Max(1, CanvasWidthPx)) / FMath::Max(Size.X, 1.0f))
		: DeltaU;
	const float DeltaY = bFeedRectValuesArePixels
		? (DeltaPx.Y * static_cast<float>(FMath::Max(1, CanvasHeightPx)) / FMath::Max(Size.Y, 1.0f))
		: DeltaV;

	auto EmitFeedRect = [this]()
	{
		SyncActiveRectFromCachedValues();
		OnFeedRectChanged.ExecuteIfBound(
			FeedRects.IsValidIndex(ActiveFeedRectIndex) ? FeedRects[ActiveFeedRectIndex].SurfaceId : FString(),
			FeedU, FeedV, FeedW, FeedH);
	};

	auto ClampToPixelCanvas = [this]()
	{
		if (!bFeedRectValuesArePixels)
		{
			FeedW = FMath::Max(0.01f, FeedW);
			FeedH = FMath::Max(0.01f, FeedH);
			return;
		}

		const float MaxX = static_cast<float>(FMath::Max(0, CanvasWidthPx - 1));
		const float MaxY = static_cast<float>(FMath::Max(0, CanvasHeightPx - 1));
		FeedU = FMath::Clamp(FMath::RoundToFloat(FeedU), 0.0f, MaxX);
		FeedV = FMath::Clamp(FMath::RoundToFloat(FeedV), 0.0f, MaxY);
		FeedW = FMath::Max(1.0f, FMath::RoundToFloat(FeedW));
		FeedH = FMath::Max(1.0f, FMath::RoundToFloat(FeedH));
		FeedW = FMath::Min(FeedW, static_cast<float>(FMath::Max(1, CanvasWidthPx)) - FeedU);
		FeedH = FMath::Min(FeedH, static_cast<float>(FMath::Max(1, CanvasHeightPx)) - FeedV);
		FeedW = FMath::Max(1.0f, FeedW);
		FeedH = FMath::Max(1.0f, FeedH);
	};

	switch (ActiveDrag)
	{
	case EDragMode::MoveRect:
		FeedU = DragStartFeedU + DeltaX;
		FeedV = DragStartFeedV + DeltaY;
		ClampToPixelCanvas();
		EmitFeedRect();
		break;

	case EDragMode::ResizeTopLeft:
		FeedU = DragStartFeedU + DeltaX;
		FeedV = DragStartFeedV + DeltaY;
		FeedW = DragStartFeedW - DeltaX;
		FeedH = DragStartFeedH - DeltaY;
		ClampToPixelCanvas();
		EmitFeedRect();
		break;

	case EDragMode::ResizeTopRight:
		FeedV = DragStartFeedV + DeltaY;
		FeedW = DragStartFeedW + DeltaX;
		FeedH = DragStartFeedH - DeltaY;
		ClampToPixelCanvas();
		EmitFeedRect();
		break;

	case EDragMode::ResizeBottomLeft:
		FeedU = DragStartFeedU + DeltaX;
		FeedW = DragStartFeedW - DeltaX;
		FeedH = DragStartFeedH + DeltaY;
		ClampToPixelCanvas();
		EmitFeedRect();
		break;

	case EDragMode::ResizeBottomRight:
		FeedW = DragStartFeedW + DeltaX;
		FeedH = DragStartFeedH + DeltaY;
		ClampToPixelCanvas();
		EmitFeedRect();
		break;

	case EDragMode::ResizeLeft:
		FeedU = DragStartFeedU + DeltaX;
		FeedW = DragStartFeedW - DeltaX;
		ClampToPixelCanvas();
		EmitFeedRect();
		break;

	case EDragMode::ResizeRight:
		FeedW = DragStartFeedW + DeltaX;
		ClampToPixelCanvas();
		EmitFeedRect();
		break;

	case EDragMode::ResizeTop:
		FeedV = DragStartFeedV + DeltaY;
		FeedH = DragStartFeedH - DeltaY;
		ClampToPixelCanvas();
		EmitFeedRect();
		break;

	case EDragMode::ResizeBottom:
		FeedH = DragStartFeedH + DeltaY;
		ClampToPixelCanvas();
		EmitFeedRect();
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
		if (DisplayMode == TEXT("feed") && HitTestFeedRectBody(MyGeometry, LocalPos) != INDEX_NONE)
		{
			return FCursorReply::Cursor(EMouseCursor::CardinalCross);
		}
		return FCursorReply::Cursor(EMouseCursor::Crosshairs);
	}
}

void SRshipMappingCanvas::SetFeedRect(float U, float V, float W, float H)
{
	FeedU = U;
	FeedV = V;
	FeedW = W;
	FeedH = H;
	FeedRects.Reset();
	FRshipCanvasFeedRectEntry DefaultEntry;
	DefaultEntry.SurfaceId = TEXT("");
	DefaultEntry.Label = TEXT("Default");
	DefaultEntry.U = FeedU;
	DefaultEntry.V = FeedV;
	DefaultEntry.W = FeedW;
	DefaultEntry.H = FeedH;
	DefaultEntry.bActive = true;
	FeedRects.Add(DefaultEntry);
	ActiveFeedRectIndex = 0;
}

void SRshipMappingCanvas::SetFeedRects(const TArray<FRshipCanvasFeedRectEntry>& InFeedRects)
{
	FeedRects = InFeedRects;
	ActiveFeedRectIndex = INDEX_NONE;
	for (int32 Index = 0; Index < FeedRects.Num(); ++Index)
	{
		if (FeedRects[Index].bActive)
		{
			ActiveFeedRectIndex = Index;
			break;
		}
	}

	if (ActiveFeedRectIndex == INDEX_NONE && FeedRects.Num() > 0)
	{
		ActiveFeedRectIndex = 0;
	}

	if (ActiveFeedRectIndex != INDEX_NONE)
	{
		SyncCachedValuesFromActiveRect();
	}
	else
	{
		FeedU = 0.0f;
		FeedV = 0.0f;
		FeedW = 1.0f;
		FeedH = 1.0f;
	}
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

void SRshipMappingCanvas::SetCanvasResolution(int32 WidthPx, int32 HeightPx)
{
	const int32 NewWidth = FMath::Max(1, WidthPx);
	const int32 NewHeight = FMath::Max(1, HeightPx);
	if (CanvasWidthPx == NewWidth && CanvasHeightPx == NewHeight)
	{
		return;
	}

	CanvasWidthPx = NewWidth;
	CanvasHeightPx = NewHeight;
	Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

void SRshipMappingCanvas::SetFeedRectValueModePixels(bool bInPixels)
{
	if (bFeedRectValuesArePixels == bInPixels)
	{
		return;
	}
	bFeedRectValuesArePixels = bInPixels;
	Invalidate(EInvalidateWidgetReason::Paint);
}

#undef LOCTEXT_NAMESPACE
