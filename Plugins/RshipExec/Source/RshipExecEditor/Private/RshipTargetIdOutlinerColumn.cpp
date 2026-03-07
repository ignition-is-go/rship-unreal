// Copyright Rocketship. All Rights Reserved.

#include "RshipTargetIdOutlinerColumn.h"

#include "RshipActorRegistrationComponent.h"

#include "ActorTreeItem.h"
#include "ISceneOutliner.h"
#include "Styling/AppStyle.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "RshipTargetIdOutlinerColumn"

FRshipTargetIdOutlinerColumn::FRshipTargetIdOutlinerColumn(ISceneOutliner& Outliner)
    : SceneOutlinerWeak(StaticCastSharedRef<ISceneOutliner>(Outliner.AsShared()))
{
}

FName FRshipTargetIdOutlinerColumn::GetID()
{
    static const FName ColumnId(TEXT("RshipTargetId"));
    return ColumnId;
}

FName FRshipTargetIdOutlinerColumn::GetColumnID()
{
    return GetID();
}

SHeaderRow::FColumn::FArguments FRshipTargetIdOutlinerColumn::ConstructHeaderRowColumn()
{
    return SHeaderRow::Column(GetColumnID())
        .FillWidth(1.5f)
        .DefaultLabel(LOCTEXT("RshipTargetIdColumnLabel", "Target Id"))
        .DefaultTooltip(LOCTEXT("RshipTargetIdColumnTooltip", "Rocketship target ID from Rship Actor Registration component"));
}

const TSharedRef<SWidget> FRshipTargetIdOutlinerColumn::ConstructRowWidget(
    FSceneOutlinerTreeItemRef TreeItem,
    const STableRow<FSceneOutlinerTreeItemPtr>& Row)
{
    const TSharedPtr<ISceneOutliner> SceneOutliner = SceneOutlinerWeak.Pin();

    return SNew(STextBlock)
        .Text(this, &FRshipTargetIdOutlinerColumn::GetTextForItem, TWeakPtr<ISceneOutlinerTreeItem>(TreeItem))
        .HighlightText(SceneOutliner.IsValid() ? SceneOutliner->GetFilterHighlightText() : FText::GetEmpty())
        .ColorAndOpacity(FSlateColor::UseSubduedForeground());
}

void FRshipTargetIdOutlinerColumn::PopulateSearchStrings(const ISceneOutlinerTreeItem& Item, TArray<FString>& OutSearchStrings) const
{
    const FString TargetId = GetTargetIdForItem(Item);
    if (!TargetId.IsEmpty())
    {
        OutSearchStrings.Add(TargetId);
    }
}

bool FRshipTargetIdOutlinerColumn::SupportsSorting() const
{
    return true;
}

void FRshipTargetIdOutlinerColumn::SortItems(TArray<FSceneOutlinerTreeItemPtr>& OutItems, const EColumnSortMode::Type SortMode) const
{
    OutItems.Sort([this, SortMode](const FSceneOutlinerTreeItemPtr& A, const FSceneOutlinerTreeItemPtr& B)
    {
        const FString TargetIdA = A.IsValid() ? GetTargetIdForItem(*A) : FString();
        const FString TargetIdB = B.IsValid() ? GetTargetIdForItem(*B) : FString();

        if (SortMode == EColumnSortMode::Descending)
        {
            return TargetIdA > TargetIdB;
        }

        return TargetIdA < TargetIdB;
    });
}

FString FRshipTargetIdOutlinerColumn::GetTargetIdForItem(const ISceneOutlinerTreeItem& Item) const
{
    const FActorTreeItem* ActorItem = Item.CastTo<FActorTreeItem>();
    if (!ActorItem)
    {
        return FString();
    }

    const AActor* Actor = ActorItem->Actor.Get();
    if (!Actor)
    {
        return FString();
    }

    const URshipActorRegistrationComponent* Registration = Actor->FindComponentByClass<URshipActorRegistrationComponent>();
    if (!Registration)
    {
        return FString();
    }

    return Registration->GetTargetId();
}

FText FRshipTargetIdOutlinerColumn::GetTextForItem(TWeakPtr<ISceneOutlinerTreeItem> Item) const
{
    const TSharedPtr<ISceneOutlinerTreeItem> PinnedItem = Item.Pin();
    if (!PinnedItem.IsValid())
    {
        return FText::GetEmpty();
    }

    return FText::FromString(GetTargetIdForItem(*PinnedItem));
}

#undef LOCTEXT_NAMESPACE
