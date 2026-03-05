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
	(void)Actor;

	UObject* OwnerObject = Owner.Get();
	if (!OwnerObject)
	{
		UE_LOG(LogRshipExec, Error, TEXT("Action '%s' failed: owner is invalid or destroyed."), *Id);
		return false;
	}

	if (Property)
	{
		const FString ArgList = BuildArgStringFromJson(*Props, Data, false);
		void* PropAddress = Property->ContainerPtrToValuePtr<void>(OwnerObject);
		const TCHAR* Result = Property->ImportText_Direct(*ArgList, PropAddress, OwnerObject, 0);
		if (Result != nullptr && FCString::Strlen(Result) > 0)
		{
			UE_LOG(LogRshipExec, Error, TEXT("Action '%s' failed to import property '%s' on '%s'. Parse error: %s"),
				*Id,
				*Property->GetName(),
				*GetNameSafe(OwnerObject),
				Result);
		}
		return Result == nullptr || FCString::Strlen(Result) == 0;
	}

	const FString ArgList = BuildArgStringFromJson(*Props, Data, true);
	FString Args = FunctionName;
	if (!ArgList.IsEmpty())
	{
		Args.Append(TEXT(" "));
		Args.Append(ArgList);
	}

	FOutputDeviceNull Out;
	TCHAR* OutStr = Args.GetCharArray().GetData();
	const bool bCalled = OwnerObject->CallFunctionByNameWithArguments(OutStr, Out, nullptr, true);
	if (!bCalled)
	{
		UE_LOG(LogRshipExec, Error, TEXT("Action '%s' failed to invoke function '%s' on '%s'."),
			*Id,
			*FunctionName,
			*GetNameSafe(OwnerObject));
	}
	return bCalled;
}
