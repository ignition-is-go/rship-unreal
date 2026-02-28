#include "Core/ActionProxy.h"

#include "GameFramework/Actor.h"
#include "Logs.h"
#include "Misc/OutputDeviceNull.h"
#include "SchemaHelpers.h"

FRshipActionProxy FRshipActionProxy::FromFunction(const FString& InId, const FString& InName, UFunction* InFunction, UObject* InOwner)
{
	FRshipActionProxy Proxy;
	Proxy.Id = InId;
	Proxy.Name = InName;
	Proxy.Owner = InOwner;
	Proxy.Property = nullptr;
	if (InFunction)
	{
		Proxy.FunctionName = InFunction->GetName();
		BuildSchemaPropsFromUFunction(InFunction, *Proxy.Props);
	}
	return Proxy;
}

FRshipActionProxy FRshipActionProxy::FromProperty(const FString& InId, const FString& InName, FProperty* InProperty, UObject* InOwner)
{
	FRshipActionProxy Proxy;
	Proxy.Id = InId;
	Proxy.Name = InName;
	Proxy.Owner = InOwner;
	Proxy.Property = InProperty;
	if (InProperty)
	{
		Proxy.FunctionName = InProperty->GetName();
		BuildSchemaPropsFromFProperty(InProperty, *Proxy.Props);
	}
	return Proxy;
}

TSharedPtr<FJsonObject> FRshipActionProxy::GetSchema() const
{
	return Props.IsValid() ? PropsToSchema(Props.Get()) : nullptr;
}

bool FRshipActionProxy::Take(AActor* Actor, const TSharedRef<FJsonObject>& Data) const
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
