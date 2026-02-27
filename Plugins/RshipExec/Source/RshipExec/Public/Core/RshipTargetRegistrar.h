#pragma once

#include "CoreMinimal.h"

class UObject;
class UFunction;
class URshipSubsystem;

class RSHIPEXEC_API FRshipRegisteredTarget
{
public:
	FRshipRegisteredTarget() = default;
	FRshipRegisteredTarget(URshipSubsystem* InSubsystem, const FString& InFullTargetId);

	bool IsValid() const;
	const FString& GetId() const { return FullTargetId; }

	FRshipRegisteredTarget AddTarget(const FString& ShortId, const FString& DisplayName = TEXT(""));
	FRshipRegisteredTarget& AddAction(UObject* Owner, const FName& FunctionName, const FString& ExposedActionName = TEXT(""));
	FRshipRegisteredTarget& AddAction(UObject* Owner, UFunction* Function, const FString& ExposedActionName = TEXT(""));
	FRshipRegisteredTarget& AddPropertyAction(UObject* Owner, const FName& PropertyName, const FString& ExposedActionName = TEXT(""));
	FRshipRegisteredTarget& AddEmitter(UObject* Owner, const FName& DelegateName, const FString& ExposedEmitterName = TEXT(""));

private:
	TWeakObjectPtr<URshipSubsystem> Subsystem;
	FString FullTargetId;
};

class RSHIPEXEC_API FRshipTargetRegistrar
{
public:
	FRshipTargetRegistrar() = default;
	FRshipTargetRegistrar(URshipSubsystem* InSubsystem, const FString& InParentFullTargetId);

	bool IsValid() const;
	FRshipRegisteredTarget AddTarget(const FString& ShortId, const FString& DisplayName = TEXT(""));

private:
	TWeakObjectPtr<URshipSubsystem> Subsystem;
	FString ParentFullTargetId;
};

using FRshipTargetProxy = FRshipRegisteredTarget;
