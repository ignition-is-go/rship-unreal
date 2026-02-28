#include "Controllers/RshipControllerComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "RshipActorRegistrationComponent.h"
#include "RshipSubsystem.h"

void URshipControllerComponent::OnRegister()
{
	Super::OnRegister();
	OnBeforeRegisterRshipBindings();
	RegisterRshipBindings();
	ScheduleDeferredRegisterRshipBindings();
}

void URshipControllerComponent::OnUnregister()
{
	ScheduleOwnerRegistrationRefresh();
	Super::OnUnregister();
}

void URshipControllerComponent::RegisterRshipBindings()
{
	RegisterOrRefreshTarget();
}

void URshipControllerComponent::OnBeforeRegisterRshipBindings()
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

void URshipControllerComponent::ScheduleDeferredRegisterRshipBindings()
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

			StrongThis->RegisterRshipBindings();
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
