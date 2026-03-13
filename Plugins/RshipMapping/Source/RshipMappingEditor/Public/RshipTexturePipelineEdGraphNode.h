// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "RshipTexturePipelineEdGraphNode.generated.h"

UCLASS()
class RSHIPMAPPINGEDITOR_API URshipTexturePipelineEdGraphNode : public UEdGraphNode
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString NodeId;

	UPROPERTY()
	FString NodeType;

	UPROPERTY()
	FString DisplayLabel;

	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void PrepareForCopying() override;
};

