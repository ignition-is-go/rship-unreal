#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RshipBindingContributor.generated.h"

UINTERFACE()
class URshipBindingContributor : public UInterface
{
	GENERATED_BODY()
};

class IRshipBindingContributor
{
	GENERATED_BODY()

public:
	virtual void RegisterRshipBindings() = 0;
};

