// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISceneOutlinerColumn.h"

class ISceneOutliner;
struct ISceneOutlinerTreeItem;

class FRshipTargetIdOutlinerColumn : public ISceneOutlinerColumn
{
public:
    explicit FRshipTargetIdOutlinerColumn(ISceneOutliner& Outliner);

    static FName GetID();

    virtual FName GetColumnID() override;
    virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn() override;
    virtual const TSharedRef<SWidget> ConstructRowWidget(FSceneOutlinerTreeItemRef TreeItem, const STableRow<FSceneOutlinerTreeItemPtr>& Row) override;
    virtual void PopulateSearchStrings(const ISceneOutlinerTreeItem& Item, TArray<FString>& OutSearchStrings) const override;
    virtual bool SupportsSorting() const override;
    virtual void SortItems(TArray<FSceneOutlinerTreeItemPtr>& OutItems, const EColumnSortMode::Type SortMode) const override;

private:
    FString GetTargetIdForItem(const ISceneOutlinerTreeItem& Item) const;
    FText GetTextForItem(TWeakPtr<ISceneOutlinerTreeItem> Item) const;

private:
    TWeakPtr<ISceneOutliner> SceneOutlinerWeak;
};

