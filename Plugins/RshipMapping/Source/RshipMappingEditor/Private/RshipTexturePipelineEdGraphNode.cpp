// Copyright Rocketship. All Rights Reserved.

#include "RshipTexturePipelineEdGraphNode.h"

#include "EdGraph/EdGraphPin.h"

void URshipTexturePipelineEdGraphNode::AllocateDefaultPins()
{
	const FEdGraphPinType PinType;
	CreatePin(EGPD_Input, PinType, FName(TEXT("In")));
	CreatePin(EGPD_Output, PinType, FName(TEXT("Out")));
}

FText URshipTexturePipelineEdGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (!DisplayLabel.TrimStartAndEnd().IsEmpty())
	{
		return FText::FromString(DisplayLabel);
	}
	if (!NodeType.TrimStartAndEnd().IsEmpty())
	{
		return FText::FromString(NodeType);
	}
	return FText::FromString(TEXT("Pipeline Node"));
}

FText URshipTexturePipelineEdGraphNode::GetTooltipText() const
{
	return FText::FromString(FString::Printf(TEXT("%s (%s)"), *DisplayLabel, *NodeType));
}

void URshipTexturePipelineEdGraphNode::PrepareForCopying()
{
	Super::PrepareForCopying();
}

