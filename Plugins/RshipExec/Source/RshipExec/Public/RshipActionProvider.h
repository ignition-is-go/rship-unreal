#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RshipActionProvider.generated.h"

class URshipTargetComponent;

UINTERFACE(MinimalAPI)
class URshipActionProvider : public UInterface
{
	GENERATED_BODY()
};

class RSHIPEXEC_API IRshipActionProvider
{
	GENERATED_BODY()

public:
	virtual void RegisterRshipWhitelistedActions(URshipTargetComponent* TargetComponent) = 0;
	virtual void OnRshipAfterTake(URshipTargetComponent* TargetComponent, const FString& ActionName, UObject* ActionOwner) {}
};
