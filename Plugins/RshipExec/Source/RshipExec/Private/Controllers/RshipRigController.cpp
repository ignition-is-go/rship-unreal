#include "Controllers/RshipRigController.h"

#include "ControlRig.h"
#include "ControlRigComponent.h"
#include "IControlRigObjectBinding.h"
#include "GameFramework/Actor.h"
#include "Logs.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "AnimNode_ControlRigBase.h"
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

FString MakeProxyKey(const FName& Name, const ERigElementType Type)
{
	return FString::FromInt(static_cast<int32>(Type)) + TEXT(":") + Name.ToString();
}

UControlRig* FindRunningControlRigInAnimInstance(const UAnimInstance* AnimInstance)
{
	if (!AnimInstance)
	{
		return nullptr;
	}

	for (TFieldIterator<FStructProperty> It(AnimInstance->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		const FStructProperty* StructProperty = *It;
		if (!StructProperty || !StructProperty->Struct)
		{
			continue;
		}

		if (!StructProperty->Struct->IsChildOf(FAnimNode_ControlRigBase::StaticStruct()))
		{
			continue;
		}

		const uint8* NodeMemory = StructProperty->ContainerPtrToValuePtr<uint8>(AnimInstance);
		if (!NodeMemory)
		{
			continue;
		}

		const FAnimNode_ControlRigBase* ControlRigNode =
			reinterpret_cast<const FAnimNode_ControlRigBase*>(NodeMemory);
		if (!ControlRigNode)
		{
			continue;
		}

		if (UControlRig* RunningRig = ControlRigNode->GetControlRig())
		{
			UE_LOG(
				LogRshipExec,
				Log,
				TEXT(
					"ResolveControlRig found AnimBP-hosted rig '%s' via node property '%s' "
					"on anim instance '%s'"),
				*GetNameSafe(RunningRig),
				*StructProperty->GetName(),
				*GetNameSafe(AnimInstance));
			return RunningRig;
		}
	}

	return nullptr;
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
	LastTickDeltaTime = DeltaTime;
}

void URshipRigController::OnBeforeRegisterRshipTargets()
{
	Super::OnBeforeRegisterRshipTargets();
	ConfigureControlRigComponent();
}

void URshipRigController::ConfigureControlRigComponent()
{
	UControlRigComponent* RigComponent = ResolveControlRigComponent();
	UControlRig* CurrentRig = ResolveControlRig();
	USkeletalMeshComponent* SkeletalMeshComponent = GetOwner() ? GetOwner()->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	const UAnimInstance* AnimInstance = SkeletalMeshComponent ? SkeletalMeshComponent->GetAnimInstance() : nullptr;

	UE_LOG(LogRshipExec, Log,
		TEXT("ConfigureControlRigComponent owner='%s' rigComp='%s' resolvedRig='%s' skelComp='%s' animMode=%d animInstance='%s'"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(RigComponent),
		*GetNameSafe(CurrentRig),
		*GetNameSafe(SkeletalMeshComponent),
		SkeletalMeshComponent ? static_cast<int32>(SkeletalMeshComponent->GetAnimationMode()) : -1,
		*GetNameSafe(AnimInstance));
	if (!CurrentRig)
	{
		bRigComponentConfigured = false;
		CachedControlRig.Reset();
		return;
	}

	const bool bRigChanged = !CachedControlRig.IsValid() || CachedControlRig.Get() != CurrentRig;
	if (bRigChanged)
	{
		UE_LOG(LogRshipExec, Log, TEXT("Rig instance changed; reconfiguring '%s'."), *GetNameSafe(CurrentRig));
		bRigComponentConfigured = false;
		CachedControlRig = CurrentRig;
		ControlRig = CurrentRig;
	}

	if (!RigComponent)
	{
		bRigComponentConfigured = true;
		return;
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
			RigComponent->ClearMappedElements();
			RigComponent->AddMappedCompleteSkeletalMesh(SkeletalMeshComponent);
			CachedBoundMesh = SkeletalMeshComponent;
			UE_LOG(
				LogRshipExec,
				Warning,
				TEXT("ConfigureControlRigComponent restored mapped skeletal mesh binding for '%s'."),
				*GetNameSafe(SkeletalMeshComponent));
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

void URshipRigBoneActionProxy::Initialize(URshipRigController* InController, const FName& InBoneName, ERigElementType InTargetType)
{
	Controller = InController;
	BoneName = InBoneName;
	TargetType = InTargetType;
}

void URshipRigBoneActionProxy::RotateInSocket(float X, float Y, float Z)
{
	if (!Controller)
	{
		UE_LOG(LogRshipExec, Error, TEXT("RotateInSocket ignored: rig controller is invalid for bone '%s'."), *BoneName.ToString());
		return;
	}

	Controller->RotateElementInSocket(BoneName, TargetType, FRotator(X, Y, Z));
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

void URshipRigBoneActionProxy::RemoveConstraints(float BlendSeconds)
{
	if (!Controller)
	{
		UE_LOG(LogRshipExec, Error, TEXT("RemoveConstraints ignored: rig controller is invalid for bone '%s'."), *BoneName.ToString());
		return;
	}

	Controller->RemoveBoneConstraints(BoneName, BlendSeconds);
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

	TMap<FString, URshipRigBoneActionProxy*> ExistingProxiesByKey;
	ExistingProxiesByKey.Reserve(BoneActionProxies.Num());
	for (URshipRigBoneActionProxy* Proxy : BoneActionProxies)
	{
		if (!Proxy)
		{
			continue;
		}

		const FString ProxyKey = MakeProxyKey(Proxy->GetBoneName(), Proxy->GetTargetType());
		ExistingProxiesByKey.Add(ProxyKey, Proxy);
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

		const FString ProxyKey = MakeProxyKey(ControlName, ERigElementType::Control);
		URshipRigBoneActionProxy* Proxy = ExistingProxiesByKey.FindRef(ProxyKey);
		if (!Proxy)
		{
			Proxy = NewObject<URshipRigBoneActionProxy>(this);
			if (!Proxy)
			{
				continue;
			}

			Proxy->Initialize(this, ControlName, ERigElementType::Control);
			BoneActionProxies.Add(Proxy);
		}

		ControlTarget
			.AddAction(Proxy, GET_FUNCTION_NAME_CHECKED(URshipRigBoneActionProxy, RotateInSocket), TEXT("RotateInSocket"))
			.AddAction(Proxy, GET_FUNCTION_NAME_CHECKED(URshipRigBoneActionProxy, AttachToBone), TEXT("AttachToBone"))
			.AddAction(Proxy, GET_FUNCTION_NAME_CHECKED(URshipRigBoneActionProxy, ResetToInitialWorld), TEXT("ResetToInitialWorld"))
			.AddAction(Proxy, GET_FUNCTION_NAME_CHECKED(URshipRigBoneActionProxy, RemoveConstraints), TEXT("RemoveConstraints"));
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

		const FString ProxyKey = MakeProxyKey(BoneName, ERigElementType::Bone);
		URshipRigBoneActionProxy* Proxy = ExistingProxiesByKey.FindRef(ProxyKey);
		if (!Proxy)
		{
			Proxy = NewObject<URshipRigBoneActionProxy>(this);
			if (!Proxy)
			{
				continue;
			}

			Proxy->Initialize(this, BoneName, ERigElementType::Bone);
			BoneActionProxies.Add(Proxy);
		}

		BoneTarget
			.AddAction(Proxy, GET_FUNCTION_NAME_CHECKED(URshipRigBoneActionProxy, RotateInSocket), TEXT("RotateInSocket"))
			.AddAction(Proxy, GET_FUNCTION_NAME_CHECKED(URshipRigBoneActionProxy, AttachToBone), TEXT("AttachToBone"))
			.AddAction(Proxy, GET_FUNCTION_NAME_CHECKED(URshipRigBoneActionProxy, ResetToInitialWorld), TEXT("ResetToInitialWorld"))
			.AddAction(Proxy, GET_FUNCTION_NAME_CHECKED(URshipRigBoneActionProxy, RemoveConstraints), TEXT("RemoveConstraints"));
	}
}

void URshipRigController::RotateElementInSocket(const FName& ElementName, ERigElementType ElementType, const FRotator& Rotation)
{
	FScopeLock Lock(&RigStateMutex);
	if (ElementType == ERigElementType::Control)
	{
		ControlRotationOverrides.Add(ElementName, Rotation.Quaternion());
	}
	else
	{
		BoneRotationOverrides.Add(ElementName, Rotation.Quaternion());
	}
	UE_LOG(LogRshipExec, Verbose, TEXT("RotateBoneInSocket queued: element=%s type=%d rot=%s"),
		*ElementName.ToString(),
		static_cast<int32>(ElementType),
		*Rotation.ToString());
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
	NewTarget.Mode = FRshipRigAttachTarget::EMode::ParentRelative;
	NewTarget.ParentElement = ParentKey;
	NewTarget.LocalOffset = ChildDesiredGlobal.GetRelativeTransform(ParentDesiredGlobal);

	{
		FScopeLock Lock(&RigStateMutex);
		FRshipRigAttachState& State = BoneAttachStates.FindOrAdd(BoneName);
		const bool bAlreadyAttachedToParent =
			!State.bBlendActive &&
			State.Active.Mode == FRshipRigAttachTarget::EMode::ParentRelative &&
			State.Active.ParentElement == ParentKey;
		const bool bAlreadyBlendingToParent =
			State.bBlendActive &&
			State.BlendTo.Mode == FRshipRigAttachTarget::EMode::ParentRelative &&
			State.BlendTo.ParentElement == ParentKey;
		if (bAlreadyAttachedToParent || bAlreadyBlendingToParent)
		{
			return;
		}

		if (State.Active.ParentElement.IsValid() && State.Active.ParentElement != ParentKey)
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
}

void URshipRigController::RemoveBoneConstraints(const FName& BoneName, float BlendSeconds)
{
	FScopeLock Lock(&RigStateMutex);
	FRshipRigAttachState* ExistingState = BoneAttachStates.Find(BoneName);
	if (!ExistingState)
	{
		return;
	}

	const float EffectiveBlendSeconds = FMath::Max(BlendSeconds, 0.0f);
	if (EffectiveBlendSeconds <= KINDA_SMALL_NUMBER)
	{
		BoneAttachStates.Remove(BoneName);
		return;
	}

	URigHierarchy* Hierarchy = ResolveRigHierarchy();
	if (!Hierarchy)
	{
		return;
	}

	const FRigElementKey ChildKey(BoneName, ERigElementType::Bone);
	if (!Hierarchy->Contains(ChildKey))
	{
		BoneAttachStates.Remove(BoneName);
		return;
	}

	auto EvaluateTargetAtRemoval = [Hierarchy, ChildKey](const FRshipRigAttachTarget& Target, FTransform& OutChildGlobal) -> bool
	{
		if (Target.Mode == FRshipRigAttachTarget::EMode::WorldSpace)
		{
			OutChildGlobal = Target.WorldTransform;
			return true;
		}

		if (Target.Mode == FRshipRigAttachTarget::EMode::UnconstrainedPose)
		{
			OutChildGlobal = Hierarchy->GetGlobalTransform(ChildKey, false);
			return true;
		}

		if (!Target.ParentElement.IsValid())
		{
			return false;
		}

		const FRigElementKey ParentKey = Target.ParentElement;
		if (!Hierarchy->Contains(ParentKey))
		{
			return false;
		}

		const FTransform ParentGlobal = Hierarchy->GetGlobalTransform(ParentKey, false);
		OutChildGlobal = Target.LocalOffset * ParentGlobal;
		return true;
	};

	FTransform CurrentConstrainedGlobal = Hierarchy->GetGlobalTransform(ChildKey, false);
	if (ExistingState->bBlendActive)
	{
		FTransform FromGlobal = FTransform::Identity;
		FTransform ToGlobal = FTransform::Identity;
		if (EvaluateTargetAtRemoval(ExistingState->BlendFrom, FromGlobal) && EvaluateTargetAtRemoval(ExistingState->BlendTo, ToGlobal))
		{
			CurrentConstrainedGlobal = BlendTransforms(FromGlobal, ToGlobal, ExistingState->BlendAlpha);
		}
	}
	else
	{
		FTransform ActiveGlobal = FTransform::Identity;
		if (EvaluateTargetAtRemoval(ExistingState->Active, ActiveGlobal))
		{
			CurrentConstrainedGlobal = ActiveGlobal;
		}
	}

	FRshipRigAttachTarget BlendFromTarget;
	BlendFromTarget.Mode = FRshipRigAttachTarget::EMode::WorldSpace;
	BlendFromTarget.WorldTransform = CurrentConstrainedGlobal;

	FRshipRigAttachTarget BlendToTarget;
	BlendToTarget.Mode = FRshipRigAttachTarget::EMode::UnconstrainedPose;

	ExistingState->BlendFrom = BlendFromTarget;
	ExistingState->BlendTo = BlendToTarget;
	ExistingState->BlendAlpha = 0.0f;
	ExistingState->BlendDuration = EffectiveBlendSeconds;
	ExistingState->bBlendActive = true;
	ExistingState->Active = BlendToTarget;
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

	{
		FScopeLock Lock(&RigStateMutex);
		BoneRotationOverrides.Remove(BoneName);
		ControlRotationOverrides.Remove(BoneName);
		BoneAttachStates.Remove(BoneName);
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

	{
		FScopeLock Lock(&RigStateMutex);
		BoneRotationOverrides.Reset();
		ControlRotationOverrides.Reset();
		BoneAttachStates.Reset();
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
		UE_LOG(LogRshipExec, Log,
			TEXT("ResolveControlRigComponent owner='%s' result='%s'"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(ControlRigComponent));
	}
	return ControlRigComponent;
}

UControlRig* URshipRigController::ResolveControlRig()
{
	UControlRigComponent* RigComponent = ResolveControlRigComponent();
	USkeletalMeshComponent* SkeletalMeshComponent = GetOwner() ? GetOwner()->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	UAnimInstance* AnimInstance = SkeletalMeshComponent ? SkeletalMeshComponent->GetAnimInstance() : nullptr;

	if (AnimInstance)
	{
		if (UControlRig* AnimInstanceRig = FindRunningControlRigInAnimInstance(AnimInstance))
		{
			ControlRig = AnimInstanceRig;
			CachedControlRig = AnimInstanceRig;
			return AnimInstanceRig;
		}
	}

	if (RigComponent)
	{
		UControlRig* CurrentRig = RigComponent->GetControlRig();
		UE_LOG(LogRshipExec, Log,
			TEXT("ResolveControlRig componentRig='%s' explicitRig='%s' cachedRig='%s' component='%s' animInstance='%s'"),
			*GetNameSafe(CurrentRig),
			*GetNameSafe(ControlRig),
			*GetNameSafe(CachedControlRig.Get()),
			*GetNameSafe(RigComponent),
			*GetNameSafe(AnimInstance));

		if (CurrentRig)
		{
			ControlRig = CurrentRig;
			CachedControlRig = CurrentRig;
			return CurrentRig;
		}

		UE_LOG(LogRshipExec, Warning, TEXT("ControlRigComponent '%s' has no active Control Rig instance."), *GetNameSafe(RigComponent));
	}

	if (CachedControlRig.IsValid())
	{
		UE_LOG(LogRshipExec, Log, TEXT("ResolveControlRig falling back to cached rig '%s'"), *GetNameSafe(CachedControlRig.Get()));
		return CachedControlRig.Get();
	}

	if (ControlRig)
	{
		CachedControlRig = ControlRig;
		UE_LOG(LogRshipExec, Log, TEXT("ResolveControlRig falling back to explicit rig '%s'"), *GetNameSafe(ControlRig));
		return ControlRig;
	}

	UE_LOG(LogRshipExec, Error, TEXT("RshipRigController on '%s' has no running Control Rig instance."), *GetNameSafe(GetOwner()));
	return nullptr;
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

void URshipRigController::LogElementDiagnostics(URigHierarchy* Hierarchy, const FName& ElementName)
{
}

URshipRigController* URshipRigController::FindForControlRig(const UControlRig* InControlRig)
{
	if (!InControlRig)
	{
		return nullptr;
	}

	if (AActor* HostingActor = InControlRig->GetHostingActor())
	{
		if (URshipRigController* Controller = HostingActor->FindComponentByClass<URshipRigController>())
		{
			return Controller;
		}
	}

	const TSharedPtr<IControlRigObjectBinding> ObjectBinding = InControlRig->GetObjectBinding();
	if (!ObjectBinding.IsValid())
	{
		return nullptr;
	}

	if (UControlRigComponent* RigComponent = Cast<UControlRigComponent>(ObjectBinding->GetBoundObject()))
	{
		return RigComponent->GetOwner() ? RigComponent->GetOwner()->FindComponentByClass<URshipRigController>() : nullptr;
	}

	if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(ObjectBinding->GetBoundObject()))
	{
		return SkeletalMeshComponent->GetOwner() ? SkeletalMeshComponent->GetOwner()->FindComponentByClass<URshipRigController>() : nullptr;
	}

	return nullptr;
}

void URshipRigController::CopyPendingRigState(
	TMap<FName, FQuat>& OutBoneRotations,
	TMap<FName, FQuat>& OutControlRotations,
	TMap<FName, FRshipRigAttachState>& OutAttachStates,
	float& OutDeltaTime)
{
	FScopeLock Lock(&RigStateMutex);
	OutBoneRotations = BoneRotationOverrides;
	OutControlRotations = ControlRotationOverrides;
	OutAttachStates = BoneAttachStates;
	OutDeltaTime = LastTickDeltaTime;
}

void URshipRigController::CommitPendingAttachStates(
	const TMap<FName, FRshipRigAttachState>& UpdatedStates,
	const TArray<FName>& ChildrenToRemove,
	const TArray<FName>& ChildrenToPersist)
{
	FScopeLock Lock(&RigStateMutex);
	for (const FName Child : ChildrenToRemove)
	{
		BoneAttachStates.Remove(Child);
	}
	for (const FName Child : ChildrenToPersist)
	{
		if (const FRshipRigAttachState* UpdatedState = UpdatedStates.Find(Child))
		{
			BoneAttachStates.FindOrAdd(Child) = *UpdatedState;
		}
	}
}

void URshipRigController::ApplyPendingRigStateToHierarchy(URigHierarchy* Hierarchy)
{
	if (!Hierarchy)
	{
		return;
	}

	const float DeltaTime = LastTickDeltaTime;

	TMap<FName, FQuat> RotationOverridesCopy;
	TMap<FName, FQuat> ControlRotationOverridesCopy;
	TMap<FName, FRshipRigAttachState> AttachStatesCopy;
	{
		FScopeLock Lock(&RigStateMutex);
		RotationOverridesCopy = BoneRotationOverrides;
		ControlRotationOverridesCopy = ControlRotationOverrides;
		AttachStatesCopy = BoneAttachStates;
	}

	UE_LOG(LogRshipExec, Log, TEXT("ApplyPendingRigState boneRotations=%d controlRotations=%d attachments=%d delta=%.4f"),
		RotationOverridesCopy.Num(),
		ControlRotationOverridesCopy.Num(),
		AttachStatesCopy.Num(),
		DeltaTime);

	for (const TPair<FName, FQuat>& Pair : RotationOverridesCopy)
	{
		const FRigElementKey BoneKey(Pair.Key, ERigElementType::Bone);
		if (!Hierarchy->Contains(BoneKey))
		{
			UE_LOG(LogRshipExec, Warning, TEXT("ApplyPendingRigState missing bone rotation target: bone=%s"), *Pair.Key.ToString());
			continue;
		}

		const FTransform InitialLocal = Hierarchy->GetLocalTransform(BoneKey, true);
		const FTransform InitialGlobal = Hierarchy->GetGlobalTransform(BoneKey, true);
		const FTransform BeforeLocal = Hierarchy->GetLocalTransform(BoneKey);
		const FTransform BeforeGlobal = Hierarchy->GetGlobalTransform(BoneKey);
		FTransform DesiredGlobal = InitialGlobal;
		DesiredGlobal.SetRotation((Pair.Value * InitialLocal.GetRotation()).GetNormalized());
		Hierarchy->SetGlobalTransform(BoneKey, DesiredGlobal, true);
		const FTransform AfterLocal = Hierarchy->GetLocalTransform(BoneKey);
		const FTransform AfterGlobal = Hierarchy->GetGlobalTransform(BoneKey);
		UE_LOG(LogRshipExec, Log,
			TEXT("ApplyPendingRigState bone rotation: bone=%s initialLocalRot=%s beforeLocalRot=%s afterLocalRot=%s beforeGlobalRot=%s afterGlobalRot=%s"),
			*Pair.Key.ToString(),
			*InitialLocal.Rotator().ToString(),
			*BeforeLocal.Rotator().ToString(),
			*AfterLocal.Rotator().ToString(),
			*BeforeGlobal.Rotator().ToString(),
			*AfterGlobal.Rotator().ToString());
	}

	for (const TPair<FName, FQuat>& Pair : ControlRotationOverridesCopy)
	{
		const FRigElementKey ControlKey(Pair.Key, ERigElementType::Control);
		if (!Hierarchy->Contains(ControlKey))
		{
			UE_LOG(LogRshipExec, Warning, TEXT("ApplyPendingRigState missing control rotation target: control=%s"), *Pair.Key.ToString());
			continue;
		}

		const FTransform BeforeLocal = Hierarchy->GetLocalTransform(ControlKey);
		FTransform LocalTransform = BeforeLocal;
		LocalTransform.SetRotation(Pair.Value);
		if (UControlRig* ActiveControlRig = ResolveControlRig())
		{
			ActiveControlRig->SetControlLocalTransform(Pair.Key, LocalTransform, true, FRigControlModifiedContext(), false, true);
		}
		else
		{
			Hierarchy->SetLocalTransform(ControlKey, LocalTransform, true);
		}
		const FTransform AfterLocal = Hierarchy->GetLocalTransform(ControlKey);
		UE_LOG(LogRshipExec, Log,
			TEXT("ApplyPendingRigState control rotation: control=%s beforeLocalRot=%s afterLocalRot=%s"),
			*Pair.Key.ToString(),
			*BeforeLocal.Rotator().ToString(),
			*AfterLocal.Rotator().ToString());
	}

	if (AttachStatesCopy.Num() == 0)
	{
		return;
	}

	TArray<FName> ChildrenToRemove;
	TArray<FName> ChildrenToPersist;
	ChildrenToRemove.Reserve(AttachStatesCopy.Num());
	ChildrenToPersist.Reserve(AttachStatesCopy.Num());

	for (TPair<FName, FRshipRigAttachState>& Pair : AttachStatesCopy)
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

		auto EvaluateTarget = [Hierarchy, ChildKey](const FRshipRigAttachTarget& Target, FTransform& OutChildGlobal) -> bool
		{
			if (Target.Mode == FRshipRigAttachTarget::EMode::WorldSpace)
			{
				OutChildGlobal = Target.WorldTransform;
				return true;
			}

			if (Target.Mode == FRshipRigAttachTarget::EMode::UnconstrainedPose)
			{
				OutChildGlobal = Hierarchy->GetGlobalTransform(ChildKey, false);
				return true;
			}

		if (!Target.ParentElement.IsValid())
		{
			return false;
		}

		const FRigElementKey ParentKey = Target.ParentElement;
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
				if (State.BlendTo.Mode == FRshipRigAttachTarget::EMode::UnconstrainedPose)
				{
					ChildrenToRemove.Add(ChildBone);
				}
				else
				{
					State.Active = State.BlendTo;
					State.bBlendActive = false;
				}
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

		Hierarchy->SetGlobalTransform(ChildKey, DesiredGlobal, false, true, false);
		const FTransform AppliedGlobal = Hierarchy->GetGlobalTransform(ChildKey);
		UE_LOG(LogRshipExec, Log,
			TEXT("ApplyPendingRigState attachment: child=%s appliedGlobalLoc=%s appliedGlobalRot=%s"),
			*ChildBone.ToString(),
			*AppliedGlobal.GetTranslation().ToString(),
			*AppliedGlobal.Rotator().ToString());
		ChildrenToPersist.Add(ChildBone);
	}

	{
		FScopeLock Lock(&RigStateMutex);
		for (const FName Child : ChildrenToRemove)
		{
			BoneAttachStates.Remove(Child);
		}
		for (const FName Child : ChildrenToPersist)
		{
			if (const FRshipRigAttachState* UpdatedState = AttachStatesCopy.Find(Child))
			{
				BoneAttachStates.FindOrAdd(Child) = *UpdatedState;
			}
		}
	}
}
