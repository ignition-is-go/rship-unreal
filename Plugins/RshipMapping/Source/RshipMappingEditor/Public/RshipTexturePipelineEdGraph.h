// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "RshipTexturePipelineEdGraph.generated.h"

UCLASS()
class RSHIPMAPPINGEDITOR_API URshipTexturePipelineEdGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	void InitializeAsTexturePipelineGraph();
};

