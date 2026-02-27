#include "Core/RshipBindings.h"

#include "GameFramework/Actor.h"
#include "Misc/OutputDeviceNull.h"
#include "SchemaHelpers.h"
#include "Logs.h"

FRshipActionBinding FRshipActionBinding::FromFunction(const FString& InId, const FString& InName, UFunction* InFunction, UObject* InOwner)
{
	FRshipActionBinding Binding;
	Binding.Id = InId;
	Binding.Name = InName;
	Binding.Owner = InOwner;
	Binding.Property = nullptr;
	if (InFunction)
	{
		Binding.FunctionName = InFunction->GetName();
		BuildSchemaPropsFromUFunction(InFunction, *Binding.Props);
	}
	return Binding;
}

FRshipActionBinding FRshipActionBinding::FromProperty(const FString& InId, const FString& InName, FProperty* InProperty, UObject* InOwner)
{
	FRshipActionBinding Binding;
	Binding.Id = InId;
	Binding.Name = InName;
	Binding.Owner = InOwner;
	Binding.Property = InProperty;
	if (InProperty)
	{
		Binding.FunctionName = InProperty->GetName();
		BuildSchemaPropsFromFProperty(InProperty, *Binding.Props);
	}
	return Binding;
}

TSharedPtr<FJsonObject> FRshipActionBinding::GetSchema() const
{
	return Props.IsValid() ? PropsToSchema(Props.Get()) : nullptr;
}

bool FRshipActionBinding::Take(AActor* Actor, const TSharedRef<FJsonObject>& Data) const
{
	if (!Owner)
	{
		return false;
	}

	if (Property)
	{
		const FString ArgList = BuildArgStringFromJson(*Props, Data, false);
		void* PropAddress = Property->ContainerPtrToValuePtr<void>(Owner);
		const TCHAR* Result = Property->ImportText_Direct(*ArgList, PropAddress, Owner, 0);
		return Result == nullptr || FCString::Strlen(Result) == 0;
	}

	const FString ArgList = BuildArgStringFromJson(*Props, Data, true);
	FString Args;
	Args.Append(TEXT("\""));
	Args.Append(FunctionName);
	Args.Append(TEXT("\""));
	if (!ArgList.IsEmpty())
	{
		Args.Append(TEXT(" "));
		Args.Append(ArgList);
	}

	FOutputDeviceNull Out;
	TCHAR* OutStr = Args.GetCharArray().GetData();
	return Owner->CallFunctionByNameWithArguments(OutStr, *GLog, nullptr, true);
}

FRshipEmitterBinding FRshipEmitterBinding::FromDelegateProperty(const FString& InId, const FString& InName, FMulticastInlineDelegateProperty* InEmitter)
{
	FRshipEmitterBinding Binding;
	Binding.Id = InId;
	Binding.Name = InName;
	if (InEmitter && InEmitter->SignatureFunction)
	{
		BuildSchemaPropsFromUFunction(InEmitter->SignatureFunction, *Binding.Props);
	}
	return Binding;
}

TSharedPtr<FJsonObject> FRshipEmitterBinding::GetSchema() const
{
	return Props.IsValid() ? PropsToSchema(Props.Get()) : nullptr;
}
