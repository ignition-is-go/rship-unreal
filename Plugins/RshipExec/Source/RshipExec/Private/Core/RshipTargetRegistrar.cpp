#include "Core/RshipTargetRegistrar.h"

#include "RshipSubsystem.h"

FRshipRegisteredTarget::FRshipRegisteredTarget(URshipSubsystem* InSubsystem, const FString& InFullTargetId)
	: Subsystem(InSubsystem)
	, FullTargetId(InFullTargetId)
{
}

bool FRshipRegisteredTarget::IsValid() const
{
	return Subsystem.IsValid() && !FullTargetId.IsEmpty();
}

FRshipRegisteredTarget FRshipRegisteredTarget::AddTarget(const FString& ShortId, const FString& DisplayName)
{
	URshipSubsystem* Sub = Subsystem.Get();
	if (!Sub)
	{
		return FRshipRegisteredTarget();
	}

	return FRshipTargetRegistrar(Sub, FullTargetId).AddTarget(ShortId, DisplayName);
}

FRshipRegisteredTarget& FRshipRegisteredTarget::AddAction(UObject* Owner, const FName& FunctionName, const FString& ExposedActionName)
{
	if (URshipSubsystem* Sub = Subsystem.Get())
	{
		Sub->RegisterFunctionActionForTarget(FullTargetId, Owner, FunctionName, ExposedActionName);
	}
	return *this;
}

FRshipRegisteredTarget& FRshipRegisteredTarget::AddAction(UObject* Owner, UFunction* Function, const FString& ExposedActionName)
{
	if (Function)
	{
		AddAction(Owner, Function->GetFName(), ExposedActionName);
	}
	return *this;
}

FRshipRegisteredTarget& FRshipRegisteredTarget::AddPropertyAction(UObject* Owner, const FName& PropertyName, const FString& ExposedActionName)
{
	if (URshipSubsystem* Sub = Subsystem.Get())
	{
		Sub->RegisterPropertyActionForTarget(FullTargetId, Owner, PropertyName, ExposedActionName);
	}
	return *this;
}

FRshipRegisteredTarget& FRshipRegisteredTarget::AddEmitter(UObject* Owner, const FName& DelegateName, const FString& ExposedEmitterName)
{
	if (URshipSubsystem* Sub = Subsystem.Get())
	{
		Sub->RegisterEmitterForTarget(FullTargetId, Owner, DelegateName, ExposedEmitterName);
	}
	return *this;
}

FRshipTargetRegistrar::FRshipTargetRegistrar(URshipSubsystem* InSubsystem, const FString& InParentFullTargetId)
	: Subsystem(InSubsystem)
	, ParentFullTargetId(InParentFullTargetId)
{
}

bool FRshipTargetRegistrar::IsValid() const
{
	return Subsystem.IsValid() && !ParentFullTargetId.IsEmpty();
}

FRshipRegisteredTarget FRshipTargetRegistrar::AddTarget(const FString& ShortId, const FString& DisplayName)
{
	URshipSubsystem* Sub = Subsystem.Get();
	if (!Sub)
	{
		return FRshipRegisteredTarget();
	}

	const FString TrimmedShortId = ShortId.TrimStartAndEnd();
	if (TrimmedShortId.IsEmpty())
	{
		return FRshipRegisteredTarget();
	}

	const FString FullTargetId = ParentFullTargetId + TEXT(".") + TrimmedShortId;
	const FString Name = DisplayName.IsEmpty() ? TrimmedShortId : DisplayName;
	return Sub->EnsureTargetIdentity(FullTargetId, Name, { ParentFullTargetId });
}
