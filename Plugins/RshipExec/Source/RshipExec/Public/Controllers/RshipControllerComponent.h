#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/RshipBindingContributor.h"
#include "Core/RshipTargetRegistrar.h"
#include "RshipControllerComponent.generated.h"

class URshipSubsystem;

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
	URshipSubsystem* ResolveRshipSubsystem() const;
	FRshipTargetProxy ResolveParentTarget() const;
	FRshipTargetProxy ResolveChildTarget(const FString& RequestedSuffix, const FString& DefaultSuffix) const;

private:
	void ScheduleDeferredRegisterRshipBindings();
	void ScheduleOwnerRegistrationRefresh();
};
