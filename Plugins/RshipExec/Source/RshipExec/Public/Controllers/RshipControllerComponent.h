#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/RshipTargetContributor.h"
#include "Core/TargetProxy.h"
#include "RshipControllerComponent.generated.h"

class URshipSubsystem;

UCLASS(Abstract)
class RSHIPEXEC_API URshipControllerComponent : public UActorComponent, public IRshipTargetContributor
{
	GENERATED_BODY()

public:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void RegisterRshipTargets() override;

protected:
	virtual void OnBeforeRegisterRshipTargets();
	virtual void RegisterOrRefreshTarget() PURE_VIRTUAL(URshipControllerComponent::RegisterOrRefreshTarget, );
	URshipSubsystem* ResolveRshipSubsystem() const;
	FRshipTargetProxy ResolveParentTarget() const;
	FRshipTargetProxy ResolveChildTarget(const FString& RequestedSuffix, const FString& DefaultSuffix) const;

private:
	void ScheduleDeferredRegisterRshipTargets();
	void ScheduleOwnerRegistrationRefresh();
};
