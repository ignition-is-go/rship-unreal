#pragma once

#include "CoreMinimal.h"
#include "Util.h"

class AActor;
class UFunction;
class FProperty;

struct RSHIPEXEC_API FRshipActionProxy
{
	FString Id;
	FString Name;
	FString FunctionName;
	TWeakObjectPtr<UObject> Owner;
	FProperty* Property = nullptr;
	TSharedPtr<TDoubleLinkedList<SchemaNode>> Props = MakeShared<TDoubleLinkedList<SchemaNode>>();

	static FRshipActionProxy FromFunction(const FString& InId, const FString& InName, UFunction* InFunction, UObject* InOwner);
	static FRshipActionProxy FromProperty(const FString& InId, const FString& InName, FProperty* InProperty, UObject* InOwner);

	bool IsValid() const { return !Id.IsEmpty() && Owner.IsValid(); }
	UObject* GetOwnerObject() const { return Owner.Get(); }
	TSharedPtr<FJsonObject> GetSchema() const;
	bool Take(AActor* Actor, const TSharedRef<FJsonObject>& Data) const;
};
