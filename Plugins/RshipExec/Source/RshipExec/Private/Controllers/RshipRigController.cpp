#include "Controllers/RshipRigController.h"

#include "ControlRig.h"
#include "ControlRigComponent.h"
#include "GameFramework/Actor.h"
#include "Logs.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
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

void URshipRigController::OnBeforeRegisterRshipTargets()
{
	Super::OnBeforeRegisterRshipTargets();
	ConfigureControlRigComponent();
}

void URshipRigController::ConfigureControlRigComponent()
{
	UControlRigComponent* RigComponent = ResolveControlRigComponent();
	if (!RigComponent)
	{
		UE_LOG(LogRshipExec, Error, TEXT("RshipRigController on '%s' has no ControlRigComponent; cannot configure rig settings."),
			*GetNameSafe(GetOwner()));
		return;
	}

	UControlRig* CurrentRig = RigComponent->GetControlRig();
	if (!CurrentRig)
	{
		UE_LOG(LogRshipExec, Error, TEXT("ControlRigComponent '%s' has no active Control Rig instance."), *GetNameSafe(RigComponent));
		bRigComponentConfigured = false;
		CachedControlRig.Reset();
		return;
	}

	const bool bRigChanged = !CachedControlRig.IsValid() || CachedControlRig.Get() != CurrentRig;
	if (bRigChanged)
	{
		UE_LOG(LogRshipExec, Log, TEXT("ControlRigComponent '%s' rig instance changed; reconfiguring."), *GetNameSafe(RigComponent));
		bRigComponentConfigured = false;
		CachedControlRig = CurrentRig;
	}

	bool bChanged = false;
	if (!RigComponent->bUpdateRigOnTick)
	{
		RigComponent->bUpdateRigOnTick = true;
		bChanged = true;
	}
	if (!RigComponent->bUpdateInEditor)
	{
		RigComponent->bUpdateInEditor = true;
		bChanged = true;
	}
	if (RigComponent->bEnableLazyEvaluation)
	{
		RigComponent->bEnableLazyEvaluation = false;
		bChanged = true;
	}
	if (RigComponent->bResetTransformBeforeTick)
	{
		RigComponent->bResetTransformBeforeTick = false;
		bChanged = true;
	}
	if (RigComponent->bResetInitialsBeforeConstruction)
	{
		RigComponent->bResetInitialsBeforeConstruction = false;
		bChanged = true;
	}

	USkeletalMeshComponent* SkeletalMeshComponent = GetOwner() ? GetOwner()->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	if (!SkeletalMeshComponent)
	{
		UE_LOG(LogRshipExec, Error, TEXT("RshipRigController on '%s' has no SkeletalMeshComponent to bind to."), *GetNameSafe(GetOwner()));
	}
	else
	{
		const bool bBindingChanged = !CachedBoundMesh.IsValid() || CachedBoundMesh.Get() != SkeletalMeshComponent;
		if (bBindingChanged || bRigChanged || !bRigComponentConfigured)
		{
			RigComponent->SetObjectBinding(SkeletalMeshComponent);
			CachedBoundMesh = SkeletalMeshComponent;
		}

		const bool bHasMappedElements = RigComponent->MappedElements.Num() > 0 || RigComponent->UserDefinedElements.Num() > 0;
		const bool bMappedElementsInvalid =
			RigComponent->MappedElements.Num() > 0 && !IsValid(RigComponent->MappedElements[0].SceneComponent);
		if (!bHasMappedElements || bMappedElementsInvalid || bRigChanged || !bRigComponentConfigured)
		{
			RigComponent->ClearMappedElements();
			RigComponent->AddMappedCompleteSkeletalMesh(SkeletalMeshComponent, EControlRigComponentMapDirection::Output);
			UE_LOG(LogRshipExec, Log, TEXT("RshipRigController mapped skeletal mesh '%s' to ControlRigComponent '%s'."),
				*GetNameSafe(SkeletalMeshComponent),
				*GetNameSafe(RigComponent));
		}
	}

	RigComponent->SetComponentTickEnabled(true);

	if (bChanged || !bRigComponentConfigured)
	{
		UE_LOG(LogRshipExec, Log,
			TEXT("ControlRigComponent '%s' configured: UpdateRigOnTick=%d UpdateInEditor=%d LazyEval=%d ResetBeforeTick=%d ResetInitials=%d"),
			*GetNameSafe(RigComponent),
			RigComponent->bUpdateRigOnTick ? 1 : 0,
			RigComponent->bUpdateInEditor ? 1 : 0,
			RigComponent->bEnableLazyEvaluation ? 1 : 0,
			RigComponent->bResetTransformBeforeTick ? 1 : 0,
			RigComponent->bResetInitialsBeforeConstruction ? 1 : 0);
	}

	bRigComponentConfigured = true;

	if (RigComponent->CanExecute())
	{
		RigComponent->Update(0.f);
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
		UE_LOG(LogRshipExec, Error, TEXT("RotateInSocket ignored: rig controller is invalid for bone '%s'."), *BoneName.ToString());
		return;
	}

	UE_LOG(LogRshipExec, Log, TEXT("RotateInSocket: bone='%s' rot=(%0.3f,%0.3f,%0.3f)"), *BoneName.ToString(), X, Y, Z);
	Controller->RotateBoneInSocket(BoneName, FRotator(X, Y, Z));
}

void URshipRigBoneActionProxy::AttachToBone(FString ParentBoneName,
	ERshipRigTransformChoice ParentLocation,
	ERshipRigTransformChoice ParentRotation,
	ERshipRigTransformChoice ChildLocation,
	ERshipRigTransformChoice ChildRotation)
{
	if (!Controller)
	{
		UE_LOG(LogRshipExec, Error, TEXT("AttachToBone ignored: rig controller is invalid for bone '%s'."), *BoneName.ToString());
		return;
	}

	Controller->AttachBoneToParent(BoneName, FName(*ParentBoneName), ParentLocation, ParentRotation, ChildLocation, ChildRotation);
}

void URshipRigBoneActionProxy::ResetToInitialWorld()
{
	if (!Controller)
	{
		UE_LOG(LogRshipExec, Error, TEXT("ResetToInitialWorld ignored: rig controller is invalid for bone '%s'."), *BoneName.ToString());
		return;
	}

	Controller->ResetBoneToInitialWorld(BoneName);
}

void URshipRigController::RegisterOrRefreshTarget()
{
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

	RigTarget.AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipRigController, ResetAllBonesToInitialWorld), TEXT("ResetAllBonesToInitialWorld"));

	URigHierarchy* Hierarchy = ResolveRigHierarchy();
	if (!Hierarchy)
	{
		return;
	}

	FRshipTargetProxy ControlsTarget = RigTarget.AddTarget(TEXT("controls"), TEXT("Controls"));
	FRshipTargetProxy BonesTarget = RigTarget.AddTarget(TEXT("bones"), TEXT("Bones"));
	const bool bControlsTargetValid = ControlsTarget.IsValid();
	const bool bBonesTargetValid = BonesTarget.IsValid();

	BoneSegmentToName.Reset();
	TSet<FString> ControlSegments;

	TMap<FName, URshipRigBoneActionProxy*> ExistingProxiesByBone;
	ExistingProxiesByBone.Reserve(BoneActionProxies.Num());
	for (URshipRigBoneActionProxy* Proxy : BoneActionProxies)
	{
		if (!Proxy)
		{
			continue;
		}

		ExistingProxiesByBone.Add(Proxy->GetBoneName(), Proxy);
	}

	const TArray<FRigControlElement*> Controls = Hierarchy->GetElementsOfType<FRigControlElement>();
	for (const FRigControlElement* Control : Controls)
	{
		if (!Control)
		{
			continue;
		}

		const FName ControlName = Control->GetKey().Name;
		const FString ControlSegment = MakeTargetSegmentFromBoneName(ControlName);
		ControlSegments.Add(ControlSegment);

		FRshipTargetProxy ControlTarget = bControlsTargetValid ? ControlsTarget.AddTarget(ControlSegment, ControlName.ToString()) : FRshipTargetProxy();
		if (!ControlTarget.IsValid())
		{
			continue;
		}

		URshipRigBoneActionProxy* Proxy = ExistingProxiesByBone.FindRef(ControlName);
		if (!Proxy)
		{
			Proxy = NewObject<URshipRigBoneActionProxy>(this);
			if (!Proxy)
			{
				continue;
			}

			Proxy->Initialize(this, ControlName);
			BoneActionProxies.Add(Proxy);
		}

		ControlTarget
			.AddAction(Proxy, GET_FUNCTION_NAME_CHECKED(URshipRigBoneActionProxy, RotateInSocket), TEXT("RotateInSocket"))
			.AddAction(Proxy, GET_FUNCTION_NAME_CHECKED(URshipRigBoneActionProxy, AttachToBone), TEXT("AttachToBone"))
			.AddAction(Proxy, GET_FUNCTION_NAME_CHECKED(URshipRigBoneActionProxy, ResetToInitialWorld), TEXT("ResetToInitialWorld"));
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
		BoneSegmentToName.Add(BoneSegment, BoneName);

		FRshipTargetProxy BoneTarget = bBonesTargetValid ? BonesTarget.AddTarget(BoneSegment, BoneName.ToString()) : FRshipTargetProxy();
		if (!BoneTarget.IsValid())
		{
			continue;
		}

		URshipRigBoneActionProxy* Proxy = ExistingProxiesByBone.FindRef(BoneName);
		if (!Proxy)
		{
			Proxy = NewObject<URshipRigBoneActionProxy>(this);
			if (!Proxy)
			{
				continue;
			}

			Proxy->Initialize(this, BoneName);
			BoneActionProxies.Add(Proxy);
		}

		BoneTarget
			.AddAction(Proxy, GET_FUNCTION_NAME_CHECKED(URshipRigBoneActionProxy, RotateInSocket), TEXT("RotateInSocket"))
			.AddAction(Proxy, GET_FUNCTION_NAME_CHECKED(URshipRigBoneActionProxy, AttachToBone), TEXT("AttachToBone"))
			.AddAction(Proxy, GET_FUNCTION_NAME_CHECKED(URshipRigBoneActionProxy, ResetToInitialWorld), TEXT("ResetToInitialWorld"));
	}
}

void URshipRigController::RotateBoneInSocket(const FName& BoneName, const FRotator& Rotation)
{
	UControlRigComponent* RigComponent = ResolveControlRigComponent();
	if (RigComponent)
	{
		USkeletalMeshComponent* SkeletalMeshComponent = GetOwner() ? GetOwner()->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
		const UObject* SkeletalMeshAsset = SkeletalMeshComponent ? SkeletalMeshComponent->GetSkeletalMeshAsset() : nullptr;
		const UAnimInstance* AnimInstance = SkeletalMeshComponent ? SkeletalMeshComponent->GetAnimInstance() : nullptr;

		UE_LOG(LogRshipExec, Log,
			TEXT("RotateBoneInSocket: rig=%s rigComp=%s skelComp=%s mesh=%s animMode=%d animInstance=%s"),
			*GetNameSafe(RigComponent->GetControlRig()),
			*GetNameSafe(RigComponent),
			*GetNameSafe(SkeletalMeshComponent),
			*GetNameSafe(SkeletalMeshAsset),
			SkeletalMeshComponent ? static_cast<int32>(SkeletalMeshComponent->GetAnimationMode()) : -1,
			*GetNameSafe(AnimInstance));
	}

	if (RigComponent)
	{
		UControlRig* ControlRig = RigComponent->GetControlRig();
		URigHierarchy* Hierarchy = ControlRig ? ControlRig->GetHierarchy() : nullptr;
		const FRigElementKey ControlKey(BoneName, ERigElementType::Control);
		const bool bHasControl = Hierarchy && Hierarchy->Contains(ControlKey);

		if (bHasControl)
		{
			FTransform LocalTransform = RigComponent->GetControlTransform(BoneName, EControlRigComponentSpace::LocalSpace);
			LocalTransform.SetRotation(Rotation.Quaternion());
			RigComponent->SetControlTransform(BoneName, LocalTransform, EControlRigComponentSpace::LocalSpace);

			const FTransform ConfirmLocal = RigComponent->GetControlTransform(BoneName, EControlRigComponentSpace::LocalSpace);
			const FTransform ConfirmGlobal = RigComponent->GetControlTransform(BoneName, EControlRigComponentSpace::RigSpace);
			UE_LOG(LogRshipExec, Log,
				TEXT("RotateInSocket applied via ControlRigComponent: control=%s localRot=%s globalRot=%s"),
				*BoneName.ToString(),
				*ConfirmLocal.GetRotation().Rotator().ToString(),
				*ConfirmGlobal.GetRotation().Rotator().ToString());
		}
		else
		{
			FTransform LocalTransform = RigComponent->GetBoneTransform(BoneName, EControlRigComponentSpace::LocalSpace);
			LocalTransform.SetRotation(Rotation.Quaternion());
			RigComponent->SetBoneTransform(BoneName, LocalTransform, EControlRigComponentSpace::LocalSpace, 1.f, true);

			const FTransform ConfirmLocal = RigComponent->GetBoneTransform(BoneName, EControlRigComponentSpace::LocalSpace);
			const FTransform ConfirmGlobal = RigComponent->GetBoneTransform(BoneName, EControlRigComponentSpace::RigSpace);
			UE_LOG(LogRshipExec, Log,
				TEXT("RotateInSocket applied via ControlRigComponent: bone=%s localRot=%s globalRot=%s"),
				*BoneName.ToString(),
				*ConfirmLocal.GetRotation().Rotator().ToString(),
				*ConfirmGlobal.GetRotation().Rotator().ToString());
		}
		return;
	}

	URigHierarchy* Hierarchy = ResolveRigHierarchy();
	if (!Hierarchy)
	{
		return;
	}

	const FRigElementKey BoneKey(BoneName, ERigElementType::Bone);
	if (!Hierarchy->Contains(BoneKey))
	{
		UE_LOG(LogRshipExec, Error, TEXT("Rig bone not found: %s"), *BoneName.ToString());
		return;
	}

	FTransform LocalTransform = Hierarchy->GetLocalTransform(BoneKey);
	LocalTransform.SetRotation(Rotation.Quaternion());
	Hierarchy->SetLocalTransform(BoneKey, LocalTransform, true);

	const FTransform ConfirmTransform = Hierarchy->GetLocalTransform(BoneKey);
	UE_LOG(LogRshipExec, Log,
		TEXT("RotateBoneInSocket applied: bone=%s localRot=%s"),
		*BoneName.ToString(),
		*ConfirmTransform.GetRotation().Rotator().ToString());
}

void URshipRigController::AttachBoneToParent(const FName& BoneName, const FName& ParentBoneName,
	ERshipRigTransformChoice ParentLocation,
	ERshipRigTransformChoice ParentRotation,
	ERshipRigTransformChoice ChildLocation,
	ERshipRigTransformChoice ChildRotation)
{
	if (ParentBoneName.IsNone())
	{
		UE_LOG(LogRshipExec, Error, TEXT("AttachBoneToParent failed: parent bone name is None for child '%s'."), *BoneName.ToString());
		return;
	}

	URigHierarchy* Hierarchy = ResolveRigHierarchy();
	if (!Hierarchy)
	{
		return;
	}

	const FRigElementKey ChildKey(BoneName, ERigElementType::Bone);
	const FRigElementKey ParentKey(ResolveBoneNameFromInput(ParentBoneName.ToString(), Hierarchy), ERigElementType::Bone);
	if (!Hierarchy->Contains(ChildKey))
	{
		UE_LOG(LogRshipExec, Error, TEXT("Rig child bone not found: %s"), *BoneName.ToString());
		return;
	}
	if (!Hierarchy->Contains(ParentKey))
	{
		UE_LOG(LogRshipExec, Error, TEXT("Rig parent bone not found: %s"), *ParentBoneName.ToString());
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

	// Ensure a clean reattach by removing existing parents first.
	if (!Controller->RemoveAllParents(ChildKey, true, false, false))
	{
		UE_LOG(LogRshipExec, Error, TEXT("AttachBoneToParent failed: could not remove existing parents (child=%s)."),
			*BoneName.ToString());
		return;
	}

	if (!Controller->SetParent(ChildKey, ParentKey, true, false, false))
	{
		UE_LOG(LogRshipExec, Error, TEXT("AttachBoneToParent failed: SetParent returned false (child=%s parent=%s)."),
			*BoneName.ToString(),
			*ParentKey.Name.ToString());
		return;
	}

	Hierarchy->EnsureCacheValidity();

	const bool bUseInitialParent =
		ParentLocation == ERshipRigTransformChoice::Initial ||
		ParentRotation == ERshipRigTransformChoice::Initial;
	const bool bUseInitialChild =
		ChildLocation == ERshipRigTransformChoice::Initial ||
		ChildRotation == ERshipRigTransformChoice::Initial;

	if (bUseInitialParent || bUseInitialChild)
	{
		const FTransform ParentInitialGlobal = Hierarchy->GetGlobalTransform(ParentKey, true);
		const FTransform ParentCurrentGlobal = Hierarchy->GetGlobalTransform(ParentKey, false);
		const FTransform ChildInitialGlobal = Hierarchy->GetGlobalTransform(ChildKey, true);
		const FTransform ChildCurrentGlobal = Hierarchy->GetGlobalTransform(ChildKey, false);

		FTransform ParentDesiredGlobal = ParentCurrentGlobal;
		if (ParentLocation == ERshipRigTransformChoice::Initial)
		{
			ParentDesiredGlobal.SetTranslation(ParentInitialGlobal.GetTranslation());
		}
		if (ParentRotation == ERshipRigTransformChoice::Initial)
		{
			ParentDesiredGlobal.SetRotation(ParentInitialGlobal.GetRotation());
		}

		FTransform ChildDesiredGlobal = ChildCurrentGlobal;
		if (ChildLocation == ERshipRigTransformChoice::Initial)
		{
			ChildDesiredGlobal.SetTranslation(ChildInitialGlobal.GetTranslation());
		}
		if (ChildRotation == ERshipRigTransformChoice::Initial)
		{
			ChildDesiredGlobal.SetRotation(ChildInitialGlobal.GetRotation());
		}

		const FTransform DesiredRelative = ChildDesiredGlobal.GetRelativeTransform(ParentDesiredGlobal);

		FTransform LocalTransform = Hierarchy->GetLocalTransform(ChildKey);
		if (ChildRotation == ERshipRigTransformChoice::Initial || ParentRotation == ERshipRigTransformChoice::Initial)
		{
			LocalTransform.SetRotation(DesiredRelative.GetRotation());
		}
		if (ChildLocation == ERshipRigTransformChoice::Initial || ParentLocation == ERshipRigTransformChoice::Initial)
		{
			LocalTransform.SetTranslation(DesiredRelative.GetTranslation());
		}

		if (UControlRigComponent* RigComponent = ResolveControlRigComponent())
		{
			RigComponent->SetBoneTransform(BoneName, LocalTransform, EControlRigComponentSpace::LocalSpace, 1.f, true);
		}
		else
		{
			Hierarchy->SetLocalTransform(ChildKey, LocalTransform, false, true, false);
		}
	}

	const TArray<FRigElementKey> ParentKeys = Hierarchy->GetParents(ChildKey);
	FString ParentList;
	for (int32 Index = 0; Index < ParentKeys.Num(); ++Index)
	{
		if (Index > 0)
		{
			ParentList.Append(TEXT(", "));
		}
		ParentList.Append(ParentKeys[Index].ToString());
	}

	const TArray<FRigElementKey> Children = Hierarchy->GetChildren(ParentKey, false);

	UE_LOG(LogRshipExec, Log, TEXT("AttachBoneToParent set parent: child=%s parent=%s (parentCount=%d childrenOfParent=%d parents=[%s])"),
		*BoneName.ToString(),
		*ParentKey.Name.ToString(),
		ParentKeys.Num(),
		Children.Num(),
		*ParentList);

	if (UControlRigComponent* RigComponent = ResolveControlRigComponent())
	{
		if (RigComponent->CanExecute())
		{
			RigComponent->Update(0.f);
			UE_LOG(LogRshipExec, Log, TEXT("AttachBoneToParent forced ControlRigComponent update (rig=%s)."),
				*GetNameSafe(RigComponent->GetControlRig()));
		}
	}
}

void URshipRigController::ResetBoneToInitialWorld(const FName& BoneName)
{
	URigHierarchy* Hierarchy = ResolveRigHierarchy();
	if (!Hierarchy)
	{
		return;
	}

	const FRigElementKey BoneKey(BoneName, ERigElementType::Bone);
	if (!Hierarchy->Contains(BoneKey))
	{
		UE_LOG(LogRshipExec, Error, TEXT("ResetBoneToInitialWorld failed: bone not found: %s"), *BoneName.ToString());
		return;
	}

	const FTransform InitialGlobal = Hierarchy->GetGlobalTransform(BoneKey, true);
	Hierarchy->SetGlobalTransform(BoneKey, InitialGlobal, false, true, false);

	if (UControlRigComponent* RigComponent = ResolveControlRigComponent())
	{
		if (RigComponent->CanExecute())
		{
			RigComponent->Update(0.f);
		}
	}

	UE_LOG(LogRshipExec, Log, TEXT("ResetBoneToInitialWorld applied: bone=%s"), *BoneName.ToString());
}

void URshipRigController::ResetAllBonesToInitialWorld()
{
	URigHierarchy* Hierarchy = ResolveRigHierarchy();
	if (!Hierarchy)
	{
		return;
	}

	const TArray<FRigBoneElement*> Bones = Hierarchy->GetElementsOfType<FRigBoneElement>();
	if (Bones.Num() == 0)
	{
		UE_LOG(LogRshipExec, Error, TEXT("ResetAllBonesToInitialWorld failed: no bones in hierarchy."));
		return;
	}

	int32 ResetCount = 0;

	for (const FRigBoneElement* Bone : Bones)
	{
		if (!Bone)
		{
			continue;
		}

		const FRigElementKey BoneKey = Bone->GetKey();
		if (!Hierarchy->Contains(BoneKey))
		{
			continue;
		}

		const FTransform InitialGlobal = Hierarchy->GetGlobalTransform(BoneKey, true);
		Hierarchy->SetGlobalTransform(BoneKey, InitialGlobal, false, true, false);

		++ResetCount;
	}

	if (UControlRigComponent* RigComponent = ResolveControlRigComponent())
	{
		if (RigComponent->CanExecute())
		{
			RigComponent->Update(0.f);
		}
	}

	UE_LOG(LogRshipExec, Log, TEXT("ResetAllBonesToInitialWorld applied: bones=%d"), ResetCount);
}

FName URshipRigController::ResolveBoneNameFromInput(const FString& BoneIdentifier, URigHierarchy* Hierarchy) const
{
	const FString Trimmed = BoneIdentifier.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return NAME_None;
	}

	const FName DirectName(*Trimmed);
	if (Hierarchy && Hierarchy->Contains(FRigElementKey(DirectName, ERigElementType::Bone)))
	{
		return DirectName;
	}

	if (const FName* MappedName = BoneSegmentToName.Find(Trimmed))
	{
		return *MappedName;
	}

	return DirectName;
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
		UE_LOG(LogRshipExec, Error, TEXT("RshipRigController on '%s' has no ControlRigComponent assigned."), *GetNameSafe(GetOwner()));
		return nullptr;
	}

	UControlRig* ControlRig = RigComponent->GetControlRig();
	if (!ControlRig)
	{
		UE_LOG(LogRshipExec, Error, TEXT("ControlRigComponent '%s' has no active Control Rig instance."), *GetNameSafe(RigComponent));
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
		UE_LOG(LogRshipExec, Error, TEXT("Control Rig '%s' has no hierarchy."), *GetNameSafe(ControlRig));
		return nullptr;
	}

	return Hierarchy;
}
