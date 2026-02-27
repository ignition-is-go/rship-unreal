#pragma once

#include "CoreMinimal.h"
#include "Util.h"

class AActor;
class UFunction;
class FProperty;
class FMulticastInlineDelegateProperty;

struct RSHIPEXEC_API FRshipActionBinding
{
	FString Id;
	FString Name;
	FString FunctionName;
	TObjectPtr<UObject> Owner = nullptr;
	FProperty* Property = nullptr;
	TSharedPtr<TDoubleLinkedList<SchemaNode>> Props = MakeShared<TDoubleLinkedList<SchemaNode>>();

	static FRshipActionBinding FromFunction(const FString& InId, const FString& InName, UFunction* InFunction, UObject* InOwner);
	static FRshipActionBinding FromProperty(const FString& InId, const FString& InName, FProperty* InProperty, UObject* InOwner);

	bool IsValid() const { return !Id.IsEmpty() && Owner != nullptr; }
	UObject* GetOwnerObject() const { return Owner.Get(); }
	TSharedPtr<FJsonObject> GetSchema() const;
	bool Take(AActor* Actor, const TSharedRef<FJsonObject>& Data) const;
};

struct RSHIPEXEC_API FRshipEmitterBinding
{
	FString Id;
	FString Name;
	TSharedPtr<TDoubleLinkedList<SchemaNode>> Props = MakeShared<TDoubleLinkedList<SchemaNode>>();

	static FRshipEmitterBinding FromDelegateProperty(const FString& InId, const FString& InName, FMulticastInlineDelegateProperty* InEmitter);

	bool IsValid() const { return !Id.IsEmpty() && !Name.IsEmpty(); }
	TSharedPtr<FJsonObject> GetSchema() const;
	const TDoubleLinkedList<SchemaNode>& GetProps() const { return *Props; }
};
