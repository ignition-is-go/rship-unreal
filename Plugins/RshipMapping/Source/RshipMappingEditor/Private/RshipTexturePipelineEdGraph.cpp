// Copyright Rocketship. All Rights Reserved.

#include "RshipTexturePipelineEdGraph.h"
#include "RshipTexturePipelineEdGraphSchema.h"

void URshipTexturePipelineEdGraph::InitializeAsTexturePipelineGraph()
{
	if (!Schema)
	{
		Schema = URshipTexturePipelineEdGraphSchema::StaticClass();
	}
}

