#include "Controllers/RshipRigController.h"

#include "ControlRig.h"
#include "ControlRigComponent.h"
#include "GameFramework/Actor.h"
#include "Logs.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Rigs/RigHierarchy.h"
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

FTransform BlendTransforms(const FTransform& From, const FTransform& To, const float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	FTransform Result;
	Result.SetTranslation(FMath::Lerp(From.GetTranslation(), To.GetTranslation(), ClampedAlpha));
	Result.SetScale3D(FMath::Lerp(From.GetScale3D(), To.GetScale3D(), ClampedAlpha));
	Result.SetRotation(FQuat::Slerp(From.GetRotation(), To.GetRotation(), ClampedAlpha).GetNormalized());
	return Result;
}

FTransform ComposeDesiredGlobalTransform(
	URigHierarchy* Hierarchy,
	const FRigElementKey& Key,
	const ERshipRigTransformChoice LocationChoice,
	const ERshipRigTransformChoice RotationChoice)
{
	const FTransform InitialGlobal = Hierarchy->GetGlobalTransform(Key, true);
	const FTransform CurrentGlobal = Hierarchy->GetGlobalTransform(Key, false);

	FTransform DesiredGlobal = CurrentGlobal;
	if (LocationChoice == ERshipRigTransformChoice::Initial)
	{
		DesiredGlobal.SetTranslation(InitialGlobal.GetTranslation());
	}
	if (RotationChoice == ERshipRigTransformChoice::Initial)
	{
		DesiredGlobal.SetRotation(InitialGlobal.GetRotation());
	}
	return DesiredGlobal;
}
}

URshipRigController::URshipRigController()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bTickInEditor = true;
}

void URshipRigController::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ApplyActiveAttachments(DeltaTime);
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

	UE_LOG(LogRshipExec, Verbose, TEXT("RotateInSocket: bone='%s' rot=(%0.3f,%0.3f,%0.3f)"), *BoneName.ToString(), X, Y, Z);
	Controller->RotateBoneInSocket(BoneName, FRotator(X, Y, Z));
}

void URshipRigBoneActionProxy::AttachToBone(FString ParentBoneName,
	ERshipRigTransformChoice ParentLocation,
	ERshipRigTransformChoice ParentRotation,
	ERshipRigTransformChoice ChildLocation,
	ERshipRigTransformChoice ChildRotation,
	float BlendSeconds)
{
	if (!Controller)
	{
		UE_LOG(LogRshipExec, Error, TEXT("AttachToBone ignored: rig controller is invalid for bone '%s'."), *BoneName.ToString());
		return;
	}

	Controller->AttachBoneToParent(BoneName, FName(*ParentBoneName), ParentLocation, ParentRotation, ChildLocation, ChildRotation, BlendSeconds);
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

		UE_LOG(LogRshipExec, VeryVerbose,
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
			UE_LOG(LogRshipExec, VeryVerbose,
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
			UE_LOG(LogRshipExec, VeryVerbose,
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
	UE_LOG(LogRshipExec, VeryVerbose,
		TEXT("RotateBoneInSocket applied: bone=%s localRot=%s"),
		*BoneName.ToString(),
		*ConfirmTransform.GetRotation().Rotator().ToString());
}

void URshipRigController::AttachBoneToParent(const FName& BoneName, const FName& ParentBoneName,
	ERshipRigTransformChoice ParentLocation,
	ERshipRigTransformChoice ParentRotation,
	ERshipRigTransformChoice ChildLocation,
	ERshipRigTransformChoice ChildRotation,
	float BlendSeconds)
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

	const FTransform ParentDesiredGlobal = ComposeDesiredGlobalTransform(Hierarchy, ParentKey, ParentLocation, ParentRotation);
	const FTransform ChildDesiredGlobal = ComposeDesiredGlobalTransform(Hierarchy, ChildKey, ChildLocation, ChildRotation);

	const float EffectiveBlendSeconds = FMath::Max(BlendSeconds, 0.0f);

	FRshipRigAttachTarget NewTarget;
	NewTarget.ParentBone = ParentKey.Name;
	NewTarget.LocalOffset = ChildDesiredGlobal.GetRelativeTransform(ParentDesiredGlobal);

	FRshipRigAttachState& State = BoneAttachStates.FindOrAdd(BoneName);
	if (!State.Active.ParentBone.IsNone() && State.Active.ParentBone != ParentKey.Name)
	{
		State.BlendFrom = State.Active;
		State.BlendTo = NewTarget;
		State.BlendAlpha = 0.0f;
		State.BlendDuration = FMath::Max(EffectiveBlendSeconds, 0.0f);
		State.bBlendActive = State.BlendDuration > KINDA_SMALL_NUMBER;

		if (!State.bBlendActive)
		{
			State.Active = NewTarget;
		}
	}
	else
	{
		State.Active = NewTarget;
		State.bBlendActive = false;
	}

	UE_LOG(LogRshipExec, Verbose,
		TEXT("AttachBoneToParent set constraint: child=%s parent=%s blend=%.3fs parentLoc=%s parentRot=%s childLoc=%s childRot=%s"),
		*BoneName.ToString(),
		*ParentKey.Name.ToString(),
		EffectiveBlendSeconds,
		ParentLocation == ERshipRigTransformChoice::Initial ? TEXT("Initial") : TEXT("Current"),
		ParentRotation == ERshipRigTransformChoice::Initial ? TEXT("Initial") : TEXT("Current"),
		ChildLocation == ERshipRigTransformChoice::Initial ? TEXT("Initial") : TEXT("Current"),
		ChildRotation == ERshipRigTransformChoice::Initial ? TEXT("Initial") : TEXT("Current"));

	ApplyActiveAttachments(0.0f);
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

	UE_LOG(LogRshipExec, Verbose, TEXT("ResetBoneToInitialWorld applied: bone=%s"), *BoneName.ToString());
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

	UE_LOG(LogRshipExec, Verbose, TEXT("ResetAllBonesToInitialWorld applied: bones=%d"), ResetCount);
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

void URshipRigController::ApplyActiveAttachments(float DeltaTime)
{
	if (BoneAttachStates.Num() == 0)
	{
		return;
	}

	UControlRigComponent* RigComponent = ResolveControlRigComponent();
	URigHierarchy* Hierarchy = ResolveRigHierarchy();
	if (!Hierarchy)
	{
		return;
	}

	TArray<FName> ChildrenToRemove;
	ChildrenToRemove.Reserve(BoneAttachStates.Num());

	for (TPair<FName, FRshipRigAttachState>& Pair : BoneAttachStates)
	{
		const FName ChildBone = Pair.Key;
		FRshipRigAttachState& State = Pair.Value;

		const FRigElementKey ChildKey(ChildBone, ERigElementType::Bone);
		if (!Hierarchy->Contains(ChildKey))
		{
			UE_LOG(LogRshipExec, Error, TEXT("Attach solver dropped child '%s': bone no longer exists."), *ChildBone.ToString());
			ChildrenToRemove.Add(ChildBone);
			continue;
		}

		auto EvaluateTarget = [Hierarchy](const FRshipRigAttachTarget& Target, FTransform& OutChildGlobal) -> bool
		{
			if (Target.ParentBone.IsNone())
			{
				return false;
			}

			const FRigElementKey ParentKey(Target.ParentBone, ERigElementType::Bone);
			if (!Hierarchy->Contains(ParentKey))
			{
				return false;
			}

			const FTransform ParentGlobal = Hierarchy->GetGlobalTransform(ParentKey, false);
			OutChildGlobal = Target.LocalOffset * ParentGlobal;
			return true;
		};

		FTransform DesiredGlobal = FTransform::Identity;
		if (State.bBlendActive)
		{
			FTransform FromGlobal = FTransform::Identity;
			FTransform ToGlobal = FTransform::Identity;

			if (!EvaluateTarget(State.BlendFrom, FromGlobal) || !EvaluateTarget(State.BlendTo, ToGlobal))
			{
				UE_LOG(LogRshipExec, Error, TEXT("Attach solver dropped blend for child '%s': source parent missing."), *ChildBone.ToString());
				ChildrenToRemove.Add(ChildBone);
				continue;
			}

			if (State.BlendDuration <= KINDA_SMALL_NUMBER)
			{
				State.BlendAlpha = 1.0f;
			}
			else
			{
				State.BlendAlpha = FMath::Clamp(State.BlendAlpha + (DeltaTime / State.BlendDuration), 0.0f, 1.0f);
			}

			DesiredGlobal = BlendTransforms(FromGlobal, ToGlobal, State.BlendAlpha);

			if (State.BlendAlpha >= 1.0f - KINDA_SMALL_NUMBER)
			{
				State.Active = State.BlendTo;
				State.bBlendActive = false;
			}
		}
		else
		{
			if (!EvaluateTarget(State.Active, DesiredGlobal))
			{
				UE_LOG(LogRshipExec, Error, TEXT("Attach solver dropped child '%s': active parent missing."), *ChildBone.ToString());
				ChildrenToRemove.Add(ChildBone);
				continue;
			}
		}

		if (RigComponent)
		{
			RigComponent->SetBoneTransform(ChildBone, DesiredGlobal, EControlRigComponentSpace::RigSpace, 1.f, true);
		}
		else
		{
			Hierarchy->SetGlobalTransform(ChildKey, DesiredGlobal, false, true, false);
		}
	}

	for (const FName Child : ChildrenToRemove)
	{
		BoneAttachStates.Remove(Child);
	}

	if (RigComponent && RigComponent->CanExecute())
	{
		RigComponent->Update(0.0f);
	}
}
