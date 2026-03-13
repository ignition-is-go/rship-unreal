// Copyright Rocketship. All Rights Reserved.

#include "RshipTexturePipelineEdGraphSchema.h"

#define LOCTEXT_NAMESPACE "RshipTexturePipelineEdGraphSchema"

void URshipTexturePipelineEdGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	// Context menu actions are authored by the host panel for deterministic node presets.
}

const FPinConnectionResponse URshipTexturePipelineEdGraphSchema::CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const
{
	if (!A || !B)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("InvalidPins", "Invalid pins."));
	}
	if (A == B)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("SamePin", "Cannot connect a pin to itself."));
	}
	if (A->GetOwningNode() == B->GetOwningNode())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("SameNode", "Cannot connect pins on the same node."));
	}
	if (A->Direction == B->Direction)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("SameDirection", "Pins must have opposite directions."));
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, LOCTEXT("ConnectPins", "Connect pins."));
}

bool URshipTexturePipelineEdGraphSchema::TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const
{
	const FPinConnectionResponse Response = CanCreateConnection(A, B);
	if (Response.Response == CONNECT_RESPONSE_DISALLOW)
	{
		return false;
	}

	if (A && B)
	{
		A->Modify();
		B->Modify();
		A->MakeLinkTo(B);
		return true;
	}
	return false;
}

FLinearColor URshipTexturePipelineEdGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return FLinearColor(0.22f, 0.62f, 0.90f, 1.0f);
}

FText URshipTexturePipelineEdGraphSchema::GetPinDisplayName(const UEdGraphPin* Pin) const
{
	if (!Pin)
	{
		return FText::GetEmpty();
	}
	return FText::FromName(Pin->PinName);
}

#undef LOCTEXT_NAMESPACE

