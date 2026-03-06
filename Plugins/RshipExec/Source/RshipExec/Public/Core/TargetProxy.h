#pragma once

#include "CoreMinimal.h"

class UObject;
class UFunction;
class URshipSubsystem;

class RSHIPEXEC_API FRshipTargetProxy
{
public:
	FRshipTargetProxy() = default;
	FRshipTargetProxy(URshipSubsystem* InSubsystem, const FString& InFullTargetId);

	bool IsValid() const;
	const FString& GetId() const { return FullTargetId; }

	FRshipTargetProxy AddTarget(const FString& ShortId, const FString& DisplayName = TEXT(""));
	FRshipTargetProxy& AddAction(UObject* Owner, const FName& FunctionName, const FString& ExposedActionName = TEXT(""));
	FRshipTargetProxy& AddAction(UObject* Owner, UFunction* Function, const FString& ExposedActionName = TEXT(""));
	FRshipTargetProxy& AddPropertyAction(UObject* Owner, const FName& PropertyName, const FString& ExposedActionName = TEXT(""));
	FRshipTargetProxy& AddEmitter(UObject* Owner, const FName& DelegateName, const FString& ExposedEmitterName = TEXT(""));

private:
	TWeakObjectPtr<URshipSubsystem> Subsystem;
	FString FullTargetId;
};
