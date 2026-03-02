// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"

DECLARE_DELEGATE_OneParam(FOnContentModeSelected, const FString& /*Mode*/);

class SContentModeCard : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SContentModeCard)
		: _Mode()
		, _Label()
		, _bSelected(false)
	{}
		SLATE_ARGUMENT(FString, Mode)
		SLATE_ARGUMENT(FText, Label)
		SLATE_ARGUMENT(FText, Tooltip)
		SLATE_ATTRIBUTE(bool, bSelected)
		SLATE_EVENT(FOnContentModeSelected, OnSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

private:
	void DrawIllustration(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FLinearColor& LineColor) const;

	FString Mode;
	FText Label;
	FText TooltipText;
	TAttribute<bool> bSelected;
	FOnContentModeSelected OnSelected;
	bool bHovered = false;
};

class SRshipContentModeSelector : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRshipContentModeSelector) {}
		SLATE_EVENT(FOnContentModeSelected, OnContentModeSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetSelectedMode(const FString& InMode);
	FString GetSelectedMode() const { return SelectedMode; }

private:
	FString SelectedMode = TEXT("stretch");
	FOnContentModeSelected OnContentModeSelected;
};
