#pragma once

#include "CoreMinimal.h"
#include "Util.h"

class FMulticastInlineDelegateProperty;

struct RSHIPEXEC_API FRshipEmitterProxy
{
	FString Id;
	FString Name;
	TSharedPtr<TDoubleLinkedList<SchemaNode>> Props = MakeShared<TDoubleLinkedList<SchemaNode>>();

	static FRshipEmitterProxy FromDelegateProperty(const FString& InId, const FString& InName, FMulticastInlineDelegateProperty* InEmitter);

	bool IsValid() const { return !Id.IsEmpty() && !Name.IsEmpty(); }
	TSharedPtr<FJsonObject> GetSchema() const;
	const TDoubleLinkedList<SchemaNode>& GetProps() const { return *Props; }
};
