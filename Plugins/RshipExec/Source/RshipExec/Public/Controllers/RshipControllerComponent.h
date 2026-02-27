#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/RshipBindingContributor.h"
#include "RshipControllerComponent.generated.h"

UCLASS(Abstract)
class RSHIPEXEC_API URshipControllerComponent : public UActorComponent, public IRshipBindingContributor
{
	GENERATED_BODY()

public:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void RegisterRshipBindings() override;

protected:
	virtual void OnBeforeRegisterRshipBindings();
	virtual void RegisterOrRefreshTarget() PURE_VIRTUAL(URshipControllerComponent::RegisterOrRefreshTarget, );

private:
	void ScheduleDeferredRegisterRshipBindings();
	void ScheduleOwnerRegistrationRefresh();
};
