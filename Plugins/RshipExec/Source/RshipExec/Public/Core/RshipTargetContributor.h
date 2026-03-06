#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RshipTargetContributor.generated.h"

UINTERFACE()
class URshipTargetContributor : public UInterface
{
	GENERATED_BODY()
};

class IRshipTargetContributor
{
	GENERATED_BODY()

public:
	virtual void RegisterRshipTargets() = 0;
};

