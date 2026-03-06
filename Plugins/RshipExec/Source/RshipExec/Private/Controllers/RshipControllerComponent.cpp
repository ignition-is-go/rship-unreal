#include "Controllers/RshipControllerComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "RshipActorRegistrationComponent.h"
#include "RshipSubsystem.h"

void URshipControllerComponent::OnRegister()
{
	Super::OnRegister();
	OnBeforeRegisterRshipTargets();
	RegisterRshipTargets();
	ScheduleDeferredRegisterRshipTargets();
}

void URshipControllerComponent::OnUnregister()
{
	ScheduleOwnerRegistrationRefresh();
	Super::OnUnregister();
}

void URshipControllerComponent::RegisterRshipTargets()
{
	RegisterOrRefreshTarget();
}

void URshipControllerComponent::OnBeforeRegisterRshipTargets()
{
}

URshipSubsystem* URshipControllerComponent::ResolveRshipSubsystem() const
{
	if (!GEngine)
	{
		return nullptr;
	}

	return GEngine->GetEngineSubsystem<URshipSubsystem>();
}

FRshipTargetProxy URshipControllerComponent::ResolveParentTarget() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FRshipTargetProxy();
	}

	URshipSubsystem* Subsystem = ResolveRshipSubsystem();
	if (!Subsystem)
	{
		return FRshipTargetProxy();
	}

	return Subsystem->EnsureActorIdentity(Owner);
}

FRshipTargetProxy URshipControllerComponent::ResolveChildTarget(const FString& RequestedSuffix, const FString& DefaultSuffix) const
{
	FRshipTargetProxy ParentTarget = ResolveParentTarget();
	if (!ParentTarget.IsValid())
	{
		return FRshipTargetProxy();
	}

	const FString Trimmed = RequestedSuffix.TrimStartAndEnd();
	const FString Suffix = Trimmed.IsEmpty() ? DefaultSuffix : Trimmed;
	return ParentTarget.AddTarget(Suffix, Suffix);
}

void URshipControllerComponent::ScheduleDeferredRegisterRshipTargets()
{
	const TWeakObjectPtr<URshipControllerComponent> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([WeakThis](float)
		{
			URshipControllerComponent* StrongThis = WeakThis.Get();
			if (!IsValid(StrongThis))
			{
				return false;
			}

			StrongThis->RegisterRshipTargets();
			return false;
		}),
		0.0f);
}

void URshipControllerComponent::ScheduleOwnerRegistrationRefresh()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || Owner->IsActorBeingDestroyed())
	{
		return;
	}

	const TWeakObjectPtr<AActor> WeakOwner(Owner);
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([WeakOwner](float)
		{
			AActor* StrongOwner = WeakOwner.Get();
			if (!IsValid(StrongOwner) || StrongOwner->IsActorBeingDestroyed())
			{
				return false;
			}

			if (URshipActorRegistrationComponent* Registration = StrongOwner->FindComponentByClass<URshipActorRegistrationComponent>())
			{
				Registration->Register();
			}

			return false;
		}),
		0.0f);
}
