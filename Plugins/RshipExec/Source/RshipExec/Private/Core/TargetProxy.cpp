#include "Core/TargetProxy.h"

#include "RshipSubsystem.h"

FRshipTargetProxy::FRshipTargetProxy(URshipSubsystem* InSubsystem, const FString& InFullTargetId)
	: Subsystem(InSubsystem)
	, FullTargetId(InFullTargetId)
{
}

bool FRshipTargetProxy::IsValid() const
{
	return Subsystem.IsValid() && !FullTargetId.IsEmpty();
}

FRshipTargetProxy FRshipTargetProxy::AddTarget(const FString& ShortId, const FString& DisplayName)
{
	URshipSubsystem* Sub = Subsystem.Get();
	if (!Sub)
	{
		return FRshipTargetProxy();
	}

	const FString TrimmedShortId = ShortId.TrimStartAndEnd();
	if (TrimmedShortId.IsEmpty())
	{
		return FRshipTargetProxy();
	}

	const FString ChildFullTargetId = FullTargetId + TEXT(".") + TrimmedShortId;
	const FString Name = DisplayName.IsEmpty() ? TrimmedShortId : DisplayName;
	return Sub->EnsureTargetIdentity(ChildFullTargetId, Name, { FullTargetId });
}

FRshipTargetProxy& FRshipTargetProxy::AddAction(UObject* Owner, const FName& FunctionName, const FString& ExposedActionName)
{
	if (URshipSubsystem* Sub = Subsystem.Get())
	{
		Sub->RegisterFunctionActionForTarget(FullTargetId, Owner, FunctionName, ExposedActionName);
	}
	return *this;
}

FRshipTargetProxy& FRshipTargetProxy::AddAction(UObject* Owner, UFunction* Function, const FString& ExposedActionName)
{
	if (Function)
	{
		AddAction(Owner, Function->GetFName(), ExposedActionName);
	}
	return *this;
}

FRshipTargetProxy& FRshipTargetProxy::AddPropertyAction(UObject* Owner, const FName& PropertyName, const FString& ExposedActionName)
{
	if (URshipSubsystem* Sub = Subsystem.Get())
	{
		Sub->RegisterPropertyActionForTarget(FullTargetId, Owner, PropertyName, ExposedActionName);
	}
	return *this;
}

FRshipTargetProxy& FRshipTargetProxy::AddEmitter(UObject* Owner, const FName& DelegateName, const FString& ExposedEmitterName)
{
	if (URshipSubsystem* Sub = Subsystem.Get())
	{
		Sub->RegisterEmitterForTarget(FullTargetId, Owner, DelegateName, ExposedEmitterName);
	}
	return *this;
}
