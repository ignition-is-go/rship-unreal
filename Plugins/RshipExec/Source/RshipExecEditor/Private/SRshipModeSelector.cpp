// Copyright Rocketship. All Rights Reserved.

#include "SRshipModeSelector.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SVerticalBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "SlateOptMacros.h"

#define LOCTEXT_NAMESPACE "SRshipModeSelector"

static const FVector2D CardSize(80.0f, 60.0f);
static const float CardPad = 4.0f;

// --- SRshipModeCard ---

void SRshipModeCard::Construct(const FArguments& InArgs)
{
	Mode = InArgs._Mode;
	Label = InArgs._Label;
	TooltipText = InArgs._Tooltip;
	bSelected = InArgs._bSelected;
	OnSelected = InArgs._OnSelected;
	SetToolTipText(TooltipText);
}

FVector2D SRshipModeCard::ComputeDesiredSize(float) const
{
	return CardSize;
}

int32 SRshipModeCard::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
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

	// Label text at bottom
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 7);
	const FVector2D TextPos(4.0f, Size.Y - 14.0f);
	FSlateDrawElement::MakeText(OutDrawElements, LayerId + 3, AllottedGeometry.ToPaintGeometry(FVector2D(Size.X - 8.0f, 14.0f), FSlateLayoutTransform(TextPos)), Label, Font, ESlateDrawEffect::None, LineColor);

	return LayerId + 3;
}

FReply SRshipModeCard::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnSelected.ExecuteIfBound(Mode);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SRshipModeCard::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bHovered = true;
}

void SRshipModeCard::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	bHovered = false;
}

void SRshipModeCard::DrawIllustration(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FLinearColor& LineColor) const
{
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const float CX = Size.X * 0.5f;
	const float CY = (Size.Y - 14.0f) * 0.5f; // Account for label
	const float Scale = FMath::Min(Size.X, Size.Y - 14.0f) * 0.35f;

	if (Mode == TEXT("direct"))
	{
		// Grid icon: rectangle with 3x3 grid
		const float L = CX - Scale;
		const float R = CX + Scale;
		const float T = CY - Scale * 0.6f;
		const float B = CY + Scale * 0.6f;
		TArray<FVector2D> Rect;
		Rect.Add(FVector2D(L, T)); Rect.Add(FVector2D(R, T)); Rect.Add(FVector2D(R, B)); Rect.Add(FVector2D(L, B)); Rect.Add(FVector2D(L, T));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Rect, ESlateDrawEffect::None, LineColor, true, 1.5f);

		// Grid lines
		for (int32 i = 1; i < 3; ++i)
		{
			const float Frac = i / 3.0f;
			TArray<FVector2D> HLine;
			HLine.Add(FVector2D(L, T + (B - T) * Frac));
			HLine.Add(FVector2D(R, T + (B - T) * Frac));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), HLine, ESlateDrawEffect::None, LineColor * FLinearColor(1,1,1,0.4f), true, 1.0f);
			TArray<FVector2D> VLine;
			VLine.Add(FVector2D(L + (R - L) * Frac, T));
			VLine.Add(FVector2D(L + (R - L) * Frac, B));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), VLine, ESlateDrawEffect::None, LineColor * FLinearColor(1,1,1,0.4f), true, 1.0f);
		}
	}
	else if (Mode == TEXT("feed"))
	{
		// Outer rectangle
		const float L = CX - Scale;
		const float R = CX + Scale;
		const float T = CY - Scale * 0.6f;
		const float B = CY + Scale * 0.6f;
		TArray<FVector2D> Outer;
		Outer.Add(FVector2D(L, T)); Outer.Add(FVector2D(R, T)); Outer.Add(FVector2D(R, B)); Outer.Add(FVector2D(L, B)); Outer.Add(FVector2D(L, T));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Outer, ESlateDrawEffect::None, LineColor * FLinearColor(1,1,1,0.4f), true, 1.0f);

		// Inner crop rect (offset)
		const float IL = L + Scale * 0.3f;
		const float IR = R - Scale * 0.15f;
		const float IT = T + Scale * 0.15f;
		const float IB = B - Scale * 0.25f;
		TArray<FVector2D> Inner;
		Inner.Add(FVector2D(IL, IT)); Inner.Add(FVector2D(IR, IT)); Inner.Add(FVector2D(IR, IB)); Inner.Add(FVector2D(IL, IB)); Inner.Add(FVector2D(IL, IT));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Inner, ESlateDrawEffect::None, FLinearColor(1.0f, 0.85f, 0.0f, 1.0f), true, 1.5f);
	}
	else if (Mode == TEXT("perspective"))
	{
		// Frustum: point on left, expanding to plane on right
		const float Apex = CX - Scale;
		const float PlaneX = CX + Scale;
		const float PlaneH = Scale * 0.7f;
		TArray<FVector2D> Top;
		Top.Add(FVector2D(Apex, CY)); Top.Add(FVector2D(PlaneX, CY - PlaneH));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Top, ESlateDrawEffect::None, LineColor, true, 1.5f);
		TArray<FVector2D> Bottom;
		Bottom.Add(FVector2D(Apex, CY)); Bottom.Add(FVector2D(PlaneX, CY + PlaneH));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Bottom, ESlateDrawEffect::None, LineColor, true, 1.5f);
		TArray<FVector2D> Plane;
		Plane.Add(FVector2D(PlaneX, CY - PlaneH)); Plane.Add(FVector2D(PlaneX, CY + PlaneH));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Plane, ESlateDrawEffect::None, LineColor, true, 1.5f);
	}
	else if (Mode == TEXT("cylindrical"))
	{
		// Two ellipses + vertical lines
		const int32 Segments = 16;
		const float EllipseW = Scale * 0.8f;
		const float EllipseH = Scale * 0.25f;
		const float HalfH = Scale * 0.5f;

		for (int32 Row = 0; Row < 2; ++Row)
		{
			const float YOff = (Row == 0) ? (CY - HalfH) : (CY + HalfH);
			TArray<FVector2D> Ellipse;
			for (int32 i = 0; i <= Segments; ++i)
			{
				const float Angle = 2.0f * PI * i / Segments;
				Ellipse.Add(FVector2D(CX + FMath::Cos(Angle) * EllipseW, YOff + FMath::Sin(Angle) * EllipseH));
			}
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Ellipse, ESlateDrawEffect::None, LineColor, true, 1.5f);
		}

		// Vertical lines
		TArray<FVector2D> Left;
		Left.Add(FVector2D(CX - EllipseW, CY - HalfH)); Left.Add(FVector2D(CX - EllipseW, CY + HalfH));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Left, ESlateDrawEffect::None, LineColor, true, 1.5f);
		TArray<FVector2D> Right;
		Right.Add(FVector2D(CX + EllipseW, CY - HalfH)); Right.Add(FVector2D(CX + EllipseW, CY + HalfH));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Right, ESlateDrawEffect::None, LineColor, true, 1.5f);
	}
	else if (Mode == TEXT("spherical"))
	{
		// Circle with lat/lon arcs
		const int32 Segments = 24;
		const float Radius = Scale * 0.8f;

		TArray<FVector2D> Circle;
		for (int32 i = 0; i <= Segments; ++i)
		{
			const float Angle = 2.0f * PI * i / Segments;
			Circle.Add(FVector2D(CX + FMath::Cos(Angle) * Radius, CY + FMath::Sin(Angle) * Radius));
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Circle, ESlateDrawEffect::None, LineColor, true, 1.5f);

		// Horizontal ellipse (equator)
		TArray<FVector2D> Equator;
		for (int32 i = 0; i <= Segments; ++i)
		{
			const float Angle = 2.0f * PI * i / Segments;
			Equator.Add(FVector2D(CX + FMath::Cos(Angle) * Radius, CY + FMath::Sin(Angle) * Radius * 0.3f));
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Equator, ESlateDrawEffect::None, LineColor * FLinearColor(1,1,1,0.4f), true, 1.0f);

		// Vertical ellipse (meridian)
		TArray<FVector2D> Meridian;
		for (int32 i = 0; i <= Segments; ++i)
		{
			const float Angle = 2.0f * PI * i / Segments;
			Meridian.Add(FVector2D(CX + FMath::Cos(Angle) * Radius * 0.3f, CY + FMath::Sin(Angle) * Radius));
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Meridian, ESlateDrawEffect::None, LineColor * FLinearColor(1,1,1,0.4f), true, 1.0f);
	}
	else if (Mode == TEXT("parallel"))
	{
		// Parallel lines toward surface
		const float PlaneX = CX + Scale;
		TArray<FVector2D> Plane;
		Plane.Add(FVector2D(PlaneX, CY - Scale * 0.7f)); Plane.Add(FVector2D(PlaneX, CY + Scale * 0.7f));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Plane, ESlateDrawEffect::None, LineColor, true, 1.5f);

		for (int32 i = -2; i <= 2; ++i)
		{
			const float YOff = CY + i * Scale * 0.28f;
			TArray<FVector2D> Line;
			Line.Add(FVector2D(CX - Scale, YOff)); Line.Add(FVector2D(PlaneX, YOff));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Line, ESlateDrawEffect::None, LineColor * FLinearColor(1,1,1,0.6f), true, 1.0f);
		}
	}
	else if (Mode == TEXT("radial"))
	{
		// Lines radiating from center
		const int32 Rays = 8;
		for (int32 i = 0; i < Rays; ++i)
		{
			const float Angle = 2.0f * PI * i / Rays;
			TArray<FVector2D> Ray;
			Ray.Add(FVector2D(CX, CY));
			Ray.Add(FVector2D(CX + FMath::Cos(Angle) * Scale, CY + FMath::Sin(Angle) * Scale * 0.7f));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Ray, ESlateDrawEffect::None, LineColor, true, 1.5f);
		}
		// Center dot
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(FVector2D(4, 4), FSlateLayoutTransform(FVector2D(CX - 2, CY - 2))), FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, LineColor);
	}
	else if (Mode == TEXT("mesh"))
	{
		// Triangle wireframe + eye dot
		const float TriScale = Scale * 0.75f;
		TArray<FVector2D> Tri;
		Tri.Add(FVector2D(CX, CY - TriScale * 0.7f));
		Tri.Add(FVector2D(CX + TriScale, CY + TriScale * 0.5f));
		Tri.Add(FVector2D(CX - TriScale, CY + TriScale * 0.5f));
		Tri.Add(FVector2D(CX, CY - TriScale * 0.7f));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Tri, ESlateDrawEffect::None, LineColor, true, 1.5f);

		// Inner triangle
		TArray<FVector2D> InnerTri;
		InnerTri.Add(FVector2D(CX, CY - TriScale * 0.2f));
		InnerTri.Add(FVector2D(CX + TriScale * 0.45f, CY + TriScale * 0.35f));
		InnerTri.Add(FVector2D(CX - TriScale * 0.45f, CY + TriScale * 0.35f));
		InnerTri.Add(FVector2D(CX, CY - TriScale * 0.2f));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), InnerTri, ESlateDrawEffect::None, LineColor * FLinearColor(1,1,1,0.4f), true, 1.0f);

		// Eye dot
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(FVector2D(5, 5), FSlateLayoutTransform(FVector2D(CX - 2.5f, CY - TriScale * 0.9f))), FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, LineColor);
	}
	else if (Mode == TEXT("fisheye"))
	{
		// Semicircle dome with radial lines
		const float Radius = Scale * 0.8f;
		const int32 Segments = 16;

		// Dome arc (top half)
		TArray<FVector2D> Arc;
		for (int32 i = 0; i <= Segments; ++i)
		{
			const float Angle = PI + PI * i / Segments;
			Arc.Add(FVector2D(CX + FMath::Cos(Angle) * Radius, CY + FMath::Sin(Angle) * Radius));
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Arc, ESlateDrawEffect::None, LineColor, true, 1.5f);

		// Base line
		TArray<FVector2D> Base;
		Base.Add(FVector2D(CX - Radius, CY)); Base.Add(FVector2D(CX + Radius, CY));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Base, ESlateDrawEffect::None, LineColor, true, 1.5f);

		// Radial lines from center
		for (int32 i = 1; i < 4; ++i)
		{
			const float Angle = PI + PI * i / 4.0f;
			TArray<FVector2D> Ray;
			Ray.Add(FVector2D(CX, CY));
			Ray.Add(FVector2D(CX + FMath::Cos(Angle) * Radius * 0.85f, CY + FMath::Sin(Angle) * Radius * 0.85f));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Ray, ESlateDrawEffect::None, LineColor * FLinearColor(1,1,1,0.5f), true, 1.0f);
		}
	}
}

// --- SRshipModeSelector ---

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SRshipModeSelector::Construct(const FArguments& InArgs)
{
	OnModeSelected = InArgs._OnModeSelected;

	struct FModeInfo
	{
		FString Mode;
		FText Label;
		FText Tooltip;
	};

	const TArray<FModeInfo> UvModes = {
		{ TEXT("direct"), LOCTEXT("Direct", "Direct"), LOCTEXT("DirectTip", "Direct UV mapping: texture coordinates from mesh UV channel") },
		{ TEXT("feed"), LOCTEXT("Feed", "Feed"), LOCTEXT("FeedTip", "Feed rectangle: crop/pan a sub-region of the source texture") },
	};

	const TArray<FModeInfo> ProjectionModes = {
		{ TEXT("perspective"), LOCTEXT("Perspective", "Persp"), LOCTEXT("PerspTip", "Perspective projection from a virtual camera") },
		{ TEXT("cylindrical"), LOCTEXT("Cylindrical", "Cyl"), LOCTEXT("CylTip", "Cylindrical projection wrapping around an axis") },
		{ TEXT("spherical"), LOCTEXT("Spherical", "Sphere"), LOCTEXT("SphereTip", "Spherical projection for dome or full-sphere content") },
		{ TEXT("parallel"), LOCTEXT("Parallel", "Parallel"), LOCTEXT("ParallelTip", "Parallel (orthographic) projection with fixed size") },
		{ TEXT("radial"), LOCTEXT("Radial", "Radial"), LOCTEXT("RadialTip", "Radial projection emanating from center point") },
	};

	const TArray<FModeInfo> SpecialModes = {
		{ TEXT("mesh"), LOCTEXT("Mesh", "Mesh"), LOCTEXT("MeshTip", "Mesh UV mapping from eyepoint direction") },
		{ TEXT("fisheye"), LOCTEXT("Fisheye", "Fisheye"), LOCTEXT("FisheyeTip", "Fisheye lens projection for dome content") },
	};

	auto MakeWrapBox = [this](const TArray<FModeInfo>& Modes) -> TSharedRef<SWrapBox>
	{
		TSharedRef<SWrapBox> WrapBox = SNew(SWrapBox).UseAllottedSize(true);
		for (const FModeInfo& Info : Modes)
		{
			TSharedPtr<SRshipModeCard> Card;
			WrapBox->AddSlot()
			.Padding(CardPad * 0.5f)
			[
				SAssignNew(Card, SRshipModeCard)
				.Mode(Info.Mode)
				.Label(Info.Label)
				.Tooltip(Info.Tooltip)
				.bSelected_Lambda([this, Mode = Info.Mode]() { return SelectedMode == Mode; })
				.OnSelected_Lambda([this](const FString& Mode)
				{
					SelectedMode = Mode;
					OnModeSelected.ExecuteIfBound(Mode);
				})
			];
			Cards.Add(Card);
		}
		return WrapBox;
	};

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("UVHeader", "UV"))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
		[
			MakeWrapBox(UvModes)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ProjectionHeader", "Projection"))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
		[
			MakeWrapBox(ProjectionModes)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SpecialHeader", "Special"))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
			.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
		]

		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeWrapBox(SpecialModes)
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SRshipModeSelector::SetSelectedMode(const FString& InMode)
{
	SelectedMode = InMode;
}

#undef LOCTEXT_NAMESPACE
