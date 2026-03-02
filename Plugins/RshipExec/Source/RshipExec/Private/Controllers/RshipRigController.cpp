#include "Controllers/RshipRigController.h"

#include "ControlRig.h"
#include "ControlRigComponent.h"
#include "GameFramework/Actor.h"
#include "Logs.h"
#include "Rigs/RigHierarchy.h"
#include "Rigs/RigHierarchyController.h"
#include "Rigs/RigHierarchyElements.h"

namespace
{
FString MakeTargetSegmentFromBoneName(const FName& BoneName)
{
	FString Segment = BoneName.ToString();
	Segment.TrimStartAndEndInline();
	if (Segment.IsEmpty())
	{
		return TEXT("bone");
	}

	Segment.ReplaceInline(TEXT(" "), TEXT("_"));
	Segment.ReplaceInline(TEXT("/"), TEXT("_"));
	Segment.ReplaceInline(TEXT("\\"), TEXT("_"));
	Segment.ReplaceInline(TEXT(":"), TEXT("_"));
	Segment.ReplaceInline(TEXT("."), TEXT("_"));
	return Segment;
}
}

void URshipRigBoneActionProxy::Initialize(URshipRigController* InController, const FName& InBoneName)
{
	Controller = InController;
	BoneName = InBoneName;
}

void URshipRigBoneActionProxy::RotateInSocket(float X, float Y, float Z)
{
	if (!Controller)
	{
		return;
	}

	Controller->RotateBoneInSocket(BoneName, FRotator(X, Y, Z));
}

void URshipRigBoneActionProxy::AttachToBone(FString ParentBoneName)
{
	if (!Controller)
	{
		return;
	}

	Controller->AttachBoneToParent(BoneName, FName(*ParentBoneName));
}

void URshipRigController::RegisterOrRefreshTarget()
{
	BoneActionProxies.Reset();

	FRshipTargetProxy ParentTarget = ResolveParentTarget();
	if (!ParentTarget.IsValid())
	{
		return;
	}

	FRshipTargetProxy RigTarget = ParentTarget.AddTarget(TEXT("rig"), TEXT("Rig"));
	if (!RigTarget.IsValid())
	{
		return;
	}

	URigHierarchy* Hierarchy = ResolveRigHierarchy();
	if (!Hierarchy)
	{
		return;
	}

	const TArray<FRigBoneElement*> Bones = Hierarchy->GetElementsOfType<FRigBoneElement>();
	for (const FRigBoneElement* Bone : Bones)
	{
		if (!Bone)
		{
			continue;
		}

		const FName BoneName = Bone->GetKey().Name;
		const FString BoneSegment = MakeTargetSegmentFromBoneName(BoneName);
		FRshipTargetProxy BoneTarget = RigTarget.AddTarget(BoneSegment, BoneName.ToString());
		if (!BoneTarget.IsValid())
		{
			continue;
		}

		URshipRigBoneActionProxy* Proxy = NewObject<URshipRigBoneActionProxy>(this);
		if (!Proxy)
		{
			continue;
		}

		Proxy->Initialize(this, BoneName);
		BoneActionProxies.Add(Proxy);

		BoneTarget
			.AddAction(Proxy, GET_FUNCTION_NAME_CHECKED(URshipRigBoneActionProxy, RotateInSocket), TEXT("RotateInSocket"))
			.AddAction(Proxy, GET_FUNCTION_NAME_CHECKED(URshipRigBoneActionProxy, AttachToBone), TEXT("AttachToBone"));
	}
}

void URshipRigController::RotateBoneInSocket(const FName& BoneName, const FRotator& Rotation)
{
	URigHierarchy* Hierarchy = ResolveRigHierarchy();
	if (!Hierarchy)
	{
		return;
	}

	const FRigElementKey BoneKey(BoneName, ERigElementType::Bone);
	if (!Hierarchy->Contains(BoneKey))
	{
		UE_LOG(LogRshipExec, Warning, TEXT("Rig bone not found: %s"), *BoneName.ToString());
		return;
	}

	FTransform LocalTransform = Hierarchy->GetLocalTransform(BoneKey);
	LocalTransform.SetRotation(Rotation.Quaternion());
	Hierarchy->SetLocalTransform(BoneKey, LocalTransform, true);
}

void URshipRigController::AttachBoneToParent(const FName& BoneName, const FName& ParentBoneName)
{
	if (ParentBoneName.IsNone())
	{
		return;
	}

	URigHierarchy* Hierarchy = ResolveRigHierarchy();
	if (!Hierarchy)
	{
		return;
	}

	const FRigElementKey ChildKey(BoneName, ERigElementType::Bone);
	const FRigElementKey ParentKey(ParentBoneName, ERigElementType::Bone);
	if (!Hierarchy->Contains(ChildKey))
	{
		UE_LOG(LogRshipExec, Warning, TEXT("Rig child bone not found: %s"), *BoneName.ToString());
		return;
	}
	if (!Hierarchy->Contains(ParentKey))
	{
		UE_LOG(LogRshipExec, Warning, TEXT("Rig parent bone not found: %s"), *ParentBoneName.ToString());
		return;
	}
	if (ChildKey == ParentKey)
	{
		return;
	}

	URigHierarchyController* Controller = Hierarchy->GetController(true);
	if (!Controller)
	{
		return;
	}

	Controller->SetParent(ChildKey, ParentKey, true, false, false);
}

UControlRigComponent* URshipRigController::ResolveControlRigComponent()
{
	if (!ControlRigComponent)
	{
		ControlRigComponent = GetOwner() ? GetOwner()->FindComponentByClass<UControlRigComponent>() : nullptr;
	}
	return ControlRigComponent;
}

UControlRig* URshipRigController::ResolveControlRig()
{
	UControlRigComponent* RigComponent = ResolveControlRigComponent();
	if (!RigComponent)
	{
		UE_LOG(LogRshipExec, Warning, TEXT("RshipRigController on '%s' has no ControlRigComponent assigned."), *GetNameSafe(GetOwner()));
		return nullptr;
	}

	UControlRig* ControlRig = RigComponent->GetControlRig();
	if (!ControlRig)
	{
		UE_LOG(LogRshipExec, Warning, TEXT("ControlRigComponent '%s' has no active Control Rig instance."), *GetNameSafe(RigComponent));
		return nullptr;
	}

	return ControlRig;
}

URigHierarchy* URshipRigController::ResolveRigHierarchy()
{
	UControlRig* ControlRig = ResolveControlRig();
	if (!ControlRig)
	{
		return nullptr;
	}

	URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
	if (!Hierarchy)
	{
		UE_LOG(LogRshipExec, Warning, TEXT("Control Rig '%s' has no hierarchy."), *GetNameSafe(ControlRig));
		return nullptr;
	}

	return Hierarchy;
}
