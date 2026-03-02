// Copyright Rocketship. All Rights Reserved.

#include "SRshipContentModeSelector.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "SlateOptMacros.h"

#define LOCTEXT_NAMESPACE "SRshipContentModeSelector"

static const FVector2D ContentCardSize(65.0f, 50.0f);

// --- SContentModeCard ---

void SContentModeCard::Construct(const FArguments& InArgs)
{
	Mode = InArgs._Mode;
	Label = InArgs._Label;
	TooltipText = InArgs._Tooltip;
	bSelected = InArgs._bSelected;
	OnSelected = InArgs._OnSelected;
	SetToolTipText(TooltipText);
}

FVector2D SContentModeCard::ComputeDesiredSize(float) const
{
	return ContentCardSize;
}

int32 SContentModeCard::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const bool bIsSelected = bSelected.Get(false);
	const FVector2D Size = AllottedGeometry.GetLocalSize();

	// Background
	const FLinearColor BgColor = bIsSelected ? FLinearColor(0.15f, 0.12f, 0.05f, 1.0f) : (bHovered ? FLinearColor(0.12f, 0.12f, 0.12f, 1.0f) : FLinearColor(0.08f, 0.08f, 0.08f, 1.0f));
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, BgColor);

	// Border
	const FLinearColor BorderColor = bIsSelected ? FLinearColor(1.0f, 0.85f, 0.0f, 1.0f) : (bHovered ? FLinearColor(0.5f, 0.5f, 0.5f, 1.0f) : FLinearColor(0.25f, 0.25f, 0.25f, 1.0f));
	const float BorderWidth = bIsSelected ? 2.0f : 1.0f;

	TArray<FVector2D> BorderPts;
	BorderPts.Add(FVector2D(0, 0));
	BorderPts.Add(FVector2D(Size.X, 0));
	BorderPts.Add(FVector2D(Size.X, Size.Y));
	BorderPts.Add(FVector2D(0, Size.Y));
	BorderPts.Add(FVector2D(0, 0));
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), BorderPts, ESlateDrawEffect::None, BorderColor, true, BorderWidth);

	// Illustration
	const FLinearColor LineColor = bIsSelected ? FLinearColor(1.0f, 0.85f, 0.0f, 1.0f) : (bHovered ? FLinearColor::White : FLinearColor(0.6f, 0.6f, 0.6f, 1.0f));
	DrawIllustration(AllottedGeometry, OutDrawElements, LayerId + 2, LineColor);

	// Label at bottom
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 7);
	const FVector2D TextPos(3.0f, Size.Y - 12.0f);
	FSlateDrawElement::MakeText(OutDrawElements, LayerId + 3, AllottedGeometry.ToPaintGeometry(FVector2D(Size.X - 6.0f, 12.0f), FSlateLayoutTransform(TextPos)), Label, Font, ESlateDrawEffect::None, LineColor);

	return LayerId + 3;
}

FReply SContentModeCard::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnSelected.ExecuteIfBound(Mode);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SContentModeCard::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bHovered = true;
}

void SContentModeCard::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	bHovered = false;
}

void SContentModeCard::DrawIllustration(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FLinearColor& LineColor) const
{
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const float CX = Size.X * 0.5f;
	const float CY = (Size.Y - 12.0f) * 0.5f;

	// Frame rect (always the same)
	const float FrameW = 24.0f;
	const float FrameH = 16.0f;
	const float FL = CX - FrameW * 0.5f;
	const float FR = CX + FrameW * 0.5f;
	const float FT = CY - FrameH * 0.5f;
	const float FB = CY + FrameH * 0.5f;

	TArray<FVector2D> Frame;
	Frame.Add(FVector2D(FL, FT)); Frame.Add(FVector2D(FR, FT)); Frame.Add(FVector2D(FR, FB)); Frame.Add(FVector2D(FL, FB)); Frame.Add(FVector2D(FL, FT));
	FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Frame, ESlateDrawEffect::None, LineColor, true, 1.5f);

	if (Mode == TEXT("stretch"))
	{
		// Content fills frame exactly - show distortion arrows
		// Horizontal stretch arrows
		TArray<FVector2D> HArrow1;
		HArrow1.Add(FVector2D(FL + 3, CY)); HArrow1.Add(FVector2D(FR - 3, CY));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), HArrow1, ESlateDrawEffect::None, LineColor * FLinearColor(1,1,1,0.6f), true, 1.0f);

		// Arrow heads
		TArray<FVector2D> LHead;
		LHead.Add(FVector2D(FL + 6, CY - 3)); LHead.Add(FVector2D(FL + 3, CY)); LHead.Add(FVector2D(FL + 6, CY + 3));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), LHead, ESlateDrawEffect::None, LineColor * FLinearColor(1,1,1,0.6f), true, 1.0f);
		TArray<FVector2D> RHead;
		RHead.Add(FVector2D(FR - 6, CY - 3)); RHead.Add(FVector2D(FR - 3, CY)); RHead.Add(FVector2D(FR - 6, CY + 3));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), RHead, ESlateDrawEffect::None, LineColor * FLinearColor(1,1,1,0.6f), true, 1.0f);

		// Fill indicator
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(FVector2D(FrameW - 4, FrameH - 4), FSlateLayoutTransform(FVector2D(FL + 2, FT + 2))), FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, LineColor * FLinearColor(1,1,1,0.08f));
	}
	else if (Mode == TEXT("crop"))
	{
		// Content larger than frame - show overflow
		const float ContentW = FrameW + 10;
		const float ContentH = FrameH + 6;
		const float CL = CX - ContentW * 0.5f;
		const float CR = CX + ContentW * 0.5f;
		const float CT = CY - ContentH * 0.5f;
		const float CB = CY + ContentH * 0.5f;

		// Content rect (partially visible, dashed appearance)
		TArray<FVector2D> Content;
		Content.Add(FVector2D(CL, CT)); Content.Add(FVector2D(CR, CT)); Content.Add(FVector2D(CR, CB)); Content.Add(FVector2D(CL, CB)); Content.Add(FVector2D(CL, CT));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Content, ESlateDrawEffect::None, LineColor * FLinearColor(1,1,1,0.3f), true, 1.0f);

		// Fill inside frame
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(FVector2D(FrameW - 2, FrameH - 2), FSlateLayoutTransform(FVector2D(FL + 1, FT + 1))), FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, LineColor * FLinearColor(1,1,1,0.12f));
	}
	else if (Mode == TEXT("fit"))
	{
		// Content inside frame with letterbox bars
		const float ContentW = FrameW - 2;
		const float ContentH = FrameH - 8; // Letterboxed
		const float CT = CY - ContentH * 0.5f;

		// Letterbox bars (top and bottom)
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(FVector2D(FrameW - 2, 3), FSlateLayoutTransform(FVector2D(FL + 1, FT + 1))), FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor(0.0f, 0.0f, 0.0f, 0.4f));
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(FVector2D(FrameW - 2, 3), FSlateLayoutTransform(FVector2D(FL + 1, FB - 4))), FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor(0.0f, 0.0f, 0.0f, 0.4f));

		// Content in middle
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(FVector2D(ContentW, ContentH), FSlateLayoutTransform(FVector2D(FL + 1, CT))), FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, LineColor * FLinearColor(1,1,1,0.12f));
	}
	else if (Mode == TEXT("pixel-perfect"))
	{
		// Small content rect with pixel grid, 1:1 label
		const float ContentW = 14.0f;
		const float ContentH = 10.0f;
		const float CL = CX - ContentW * 0.5f;
		const float CT = CY - ContentH * 0.5f;

		// Content block
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(FVector2D(ContentW, ContentH), FSlateLayoutTransform(FVector2D(CL, CT))), FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, LineColor * FLinearColor(1,1,1,0.15f));

		// Pixel grid
		const float GridCell = 3.5f;
		for (int32 i = 1; i < 4; ++i)
		{
			TArray<FVector2D> VLine;
			VLine.Add(FVector2D(CL + i * GridCell, CT)); VLine.Add(FVector2D(CL + i * GridCell, CT + ContentH));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), VLine, ESlateDrawEffect::None, LineColor * FLinearColor(1,1,1,0.3f), true, 0.5f);
		}
		for (int32 i = 1; i < 3; ++i)
		{
			TArray<FVector2D> HLine;
			HLine.Add(FVector2D(CL, CT + i * GridCell)); HLine.Add(FVector2D(CL + ContentW, CT + i * GridCell));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), HLine, ESlateDrawEffect::None, LineColor * FLinearColor(1,1,1,0.3f), true, 0.5f);
		}

		// Content border
		TArray<FVector2D> ContentRect;
		ContentRect.Add(FVector2D(CL, CT)); ContentRect.Add(FVector2D(CL + ContentW, CT)); ContentRect.Add(FVector2D(CL + ContentW, CT + ContentH)); ContentRect.Add(FVector2D(CL, CT + ContentH)); ContentRect.Add(FVector2D(CL, CT));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), ContentRect, ESlateDrawEffect::None, LineColor * FLinearColor(1,1,1,0.5f), true, 1.0f);
	}
}

// --- SRshipContentModeSelector ---

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SRshipContentModeSelector::Construct(const FArguments& InArgs)
{
	OnContentModeSelected = InArgs._OnContentModeSelected;

	TSharedRef<SWrapBox> WrapBox = SNew(SWrapBox).UseAllottedSize(true);

	struct FModeInfo { FString Mode; FText Label; FText Tooltip; };
	const TArray<FModeInfo> Modes = {
		{ TEXT("stretch"), LOCTEXT("Stretch", "Stretch"), LOCTEXT("StretchTip", "Content fills frame, may be distorted") },
		{ TEXT("crop"), LOCTEXT("Crop", "Crop"), LOCTEXT("CropTip", "Content overflows frame, center visible") },
		{ TEXT("fit"), LOCTEXT("Fit", "Fit"), LOCTEXT("FitTip", "Content inside frame with letterbox bars") },
		{ TEXT("pixel-perfect"), LOCTEXT("PixelPerfect", "1:1"), LOCTEXT("PixelPerfectTip", "1:1 pixel mapping, may not fill frame") },
	};

	for (const FModeInfo& Info : Modes)
	{
		WrapBox->AddSlot()
		.Padding(2.0f)
		[
			SNew(SContentModeCard)
			.Mode(Info.Mode)
			.Label(Info.Label)
			.Tooltip(Info.Tooltip)
			.bSelected_Lambda([this, Mode = Info.Mode]() { return SelectedMode == Mode; })
			.OnSelected_Lambda([this](const FString& Mode)
			{
				SelectedMode = Mode;
				OnContentModeSelected.ExecuteIfBound(Mode);
			})
		];
	}

	ChildSlot
	[
		WrapBox
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SRshipContentModeSelector::SetSelectedMode(const FString& InMode)
{
	SelectedMode = InMode;
}

#undef LOCTEXT_NAMESPACE
