#include "Controllers/RshipRigController.h"

#include "ControlRig.h"
#include "ControlRigComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Logs.h"
#include "Rigs/RigHierarchy.h"
#include "Rigs/RigHierarchyElements.h"
#include "RshipSubsystem.h"

void URshipRigController::RegisterOrRefreshTarget()
{
	AActor* Owner = GetOwner();
	if (!Owner || !GEngine)
	{
		return;
	}

	URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	FRshipTargetProxy ParentIdentity = Subsystem->EnsureActorIdentity(Owner);
	if (!ParentIdentity.IsValid())
	{
		return;
	}

	ParentIdentity
		.AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipRigController, LogBones), TEXT("LogBones"))
		.AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipRigController, LogSockets), TEXT("LogSockets"));
}

void URshipRigController::LogBones()
{
	if (!ControlRigComponent)
	{
		ControlRigComponent = GetOwner() ? GetOwner()->FindComponentByClass<UControlRigComponent>() : nullptr;
	}

	if (!ControlRigComponent)
	{
		UE_LOG(LogRshipExec, Warning, TEXT("RshipRigController on '%s' has no ControlRigComponent assigned."), *GetNameSafe(GetOwner()));
		return;
	}

	UControlRig* ControlRig = ControlRigComponent->GetControlRig();
	if (!ControlRig)
	{
		UE_LOG(LogRshipExec, Warning, TEXT("ControlRigComponent '%s' has no active Control Rig instance."), *GetNameSafe(ControlRigComponent));
		return;
	}

	URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
	if (!Hierarchy)
	{
		UE_LOG(LogRshipExec, Warning, TEXT("Control Rig '%s' has no hierarchy."), *GetNameSafe(ControlRig));
		return;
	}

	const TArray<FRigBoneElement*> Bones = Hierarchy->GetElementsOfType<FRigBoneElement>();
	UE_LOG(LogRshipExec, Log, TEXT("Control Rig '%s' bone count: %d"), *GetNameSafe(ControlRig), Bones.Num());

	for (const FRigBoneElement* Bone : Bones)
	{
		if (!Bone)
		{
			continue;
		}

		const FRigElementKey BoneKey = Bone->GetKey();
		UE_LOG(LogRshipExec, Log, TEXT("Bone: %s"), *BoneKey.Name.ToString());
	}
}

void URshipRigController::LogSockets()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogRshipExec, Warning, TEXT("RshipRigController has no owner."));
		return;
	}

	TArray<USceneComponent*> SceneComponents;
	Owner->GetComponents(SceneComponents);

	int32 TotalSocketCount = 0;
	for (const USceneComponent* SceneComponent : SceneComponents)
	{
		if (!SceneComponent)
		{
			continue;
		}

		const TArray<FName> SocketNames = SceneComponent->GetAllSocketNames();
		if (SocketNames.Num() == 0)
		{
			continue;
		}

		UE_LOG(LogRshipExec, Log, TEXT("Component '%s' socket count: %d"), *GetNameSafe(SceneComponent), SocketNames.Num());
		TotalSocketCount += SocketNames.Num();

		for (const FName& SocketName : SocketNames)
		{
			UE_LOG(LogRshipExec, Log, TEXT("Socket: %s.%s"), *GetNameSafe(SceneComponent), *SocketName.ToString());
		}
	}

	if (TotalSocketCount == 0)
	{
		UE_LOG(LogRshipExec, Log, TEXT("Actor '%s' has no sockets on its scene components."), *GetNameSafe(Owner));
	}
}
