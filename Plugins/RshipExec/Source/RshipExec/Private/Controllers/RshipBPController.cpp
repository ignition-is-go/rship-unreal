#include "Controllers/RshipBPController.h"

#include "GameFramework/Actor.h"
#include "RshipSubsystem.h"
#include "UObject/UnrealType.h"

void URshipBPController::RegisterOrRefreshTarget()
{
	AActor* Owner = GetOwner();
	if (!Owner || !GEngine)
	{
		return;
	}

	URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	FRshipTargetProxy ParentIdentity = Subsystem->EnsureActorIdentity(Owner);
	if (!ParentIdentity.IsValid())
	{
		return;
	}

	const FString Suffix = ChildTargetSuffix.IsEmpty() ? TEXT("bp") : ChildTargetSuffix;
	FRshipTargetProxy Target = ParentIdentity.AddTarget(Suffix, Suffix);
	if (!Target.IsValid())
	{
		return;
	}

	if (bScanOwnerActor)
	{
		RegisterObjectMembers(Target, Owner);
	}

	if (bScanSiblingComponents)
	{
		TArray<UActorComponent*> Components;
		Owner->GetComponents(Components);
		for (UActorComponent* Component : Components)
		{
			if (!Component || Component == this)
			{
				continue;
			}
			RegisterObjectMembers(Target, Component);
		}
	}
}

void URshipBPController::RegisterObjectMembers(FRshipRegisteredTarget& Target, UObject* Object) const
{
	if (!Object)
	{
		return;
	}

	UClass* ObjectClass = Object->GetClass();
	for (TFieldIterator<UFunction> FuncIt(ObjectClass, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
	{
		UFunction* Function = *FuncIt;
		if (!Function)
		{
			continue;
		}

		const FString Name = Function->GetName();
		if (Name.Contains(TEXT("__DelegateSignature")) || !ShouldRegisterMemberName(Name))
		{
			continue;
		}

		Target.AddAction(Object, Function);
	}

	for (TFieldIterator<FProperty> PropIt(ObjectClass, EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!Property)
		{
			continue;
		}

		const FString Name = Property->GetName();
		if (!ShouldRegisterMemberName(Name))
		{
			continue;
		}

		if (CastField<FMulticastInlineDelegateProperty>(Property))
		{
			Target.AddEmitter(Object, *Name);
			continue;
		}

		if (!Property->IsA<FMulticastDelegateProperty>())
		{
			Target.AddPropertyAction(Object, *Name);
		}
	}
}

bool URshipBPController::ShouldRegisterMemberName(const FString& Name) const
{
	return !bRequireRSPrefix || Name.StartsWith(TEXT("RS_"));
}
