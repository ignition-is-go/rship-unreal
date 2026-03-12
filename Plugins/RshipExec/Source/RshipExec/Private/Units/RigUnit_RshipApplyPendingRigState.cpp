#include "Units/RigUnit_RshipApplyPendingRigState.h"

#include "ControlRig.h"
#include "Controllers/RshipRigController.h"
#include "Logs.h"
#include "Rigs/RigHierarchy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_RshipApplyPendingRigState)

FRigUnit_RshipApplyPendingRigState_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	if (ExecuteContext.GetEventName().IsNone())
	{
		return;
	}

	UControlRig* CurrentControlRig = ExecuteContext.ControlRig;
	URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;
	if (!CurrentControlRig || !Hierarchy)
	{
		return;
	}

	if (URshipRigController* Controller = URshipRigController::FindForControlRig(CurrentControlRig))
	{
		TMap<FName, FQuat> BoneRotations;
		TMap<FName, FQuat> ControlRotations;
		TMap<FName, URshipRigController::FRshipRigAttachState> AttachStates;
		float DeltaTime = 0.0f;
		Controller->CopyPendingRigState(BoneRotations, ControlRotations, AttachStates, DeltaTime);

		for (const TPair<FName, FQuat>& Pair : BoneRotations)
		{
			const FRigElementKey BoneKey(Pair.Key, ERigElementType::Bone);
			FCachedRigElement CachedBone;
			if (!CachedBone.UpdateCache(BoneKey, Hierarchy))
			{
				UE_LOG(LogRshipExec, Warning, TEXT("ApplyPendingRigState missing bone rotation target: bone=%s"), *Pair.Key.ToString());
				continue;
			}

			const FTransform InitialLocal = Hierarchy->GetLocalTransform(BoneKey, true);
			const FTransform BeforeLocal = Hierarchy->GetLocalTransform(BoneKey, false);
			const FTransform BeforeGlobal = Hierarchy->GetGlobalTransform(BoneKey, false);
			FTransform DesiredLocal = InitialLocal;
			DesiredLocal.SetRotation((Pair.Value * InitialLocal.GetRotation()).GetNormalized());
			Hierarchy->SetLocalTransform(CachedBone, DesiredLocal, true);
			const FTransform AfterLocal = Hierarchy->GetLocalTransform(BoneKey, false);
			const FTransform AfterGlobal = Hierarchy->GetGlobalTransform(BoneKey, false);
		}

		for (const TPair<FName, FQuat>& Pair : ControlRotations)
		{
			const FRigElementKey ControlKey(Pair.Key, ERigElementType::Control);
			FCachedRigElement CachedControl;
			if (!CachedControl.UpdateCache(ControlKey, Hierarchy))
			{
				UE_LOG(LogRshipExec, Warning, TEXT("ApplyPendingRigState missing control rotation target: control=%s"), *Pair.Key.ToString());
				continue;
			}

			const FTransform BeforeLocal = Hierarchy->GetLocalTransform(ControlKey, false);
			FTransform LocalTransform = BeforeLocal;
			LocalTransform.SetRotation(Pair.Value);
			CurrentControlRig->SetControlLocalTransform(Pair.Key, LocalTransform, true, FRigControlModifiedContext(), false, true);
			const FTransform AfterLocal = Hierarchy->GetLocalTransform(ControlKey, false);
		}

		TArray<FName> ChildrenToRemove;
		TArray<FName> ChildrenToPersist;
		ChildrenToRemove.Reserve(AttachStates.Num());
		ChildrenToPersist.Reserve(AttachStates.Num());

		for (TPair<FName, URshipRigController::FRshipRigAttachState>& Pair : AttachStates)
		{
			const FName ChildBone = Pair.Key;
			URshipRigController::FRshipRigAttachState& State = Pair.Value;
			const FRigElementKey ChildKey(ChildBone, ERigElementType::Bone);
			if (!Hierarchy->Contains(ChildKey))
			{
				UE_LOG(LogRshipExec, Error, TEXT("Attach solver dropped child '%s': bone no longer exists."), *ChildBone.ToString());
				ChildrenToRemove.Add(ChildBone);
				continue;
			}

			auto EvaluateTarget = [Hierarchy, ChildKey](const URshipRigController::FRshipRigAttachTarget& Target, FTransform& OutChildGlobal) -> bool
			{
				if (Target.Mode == URshipRigController::FRshipRigAttachTarget::EMode::WorldSpace)
				{
					OutChildGlobal = Target.WorldTransform;
					return true;
				}

				if (Target.Mode == URshipRigController::FRshipRigAttachTarget::EMode::UnconstrainedPose)
				{
					OutChildGlobal = Hierarchy->GetGlobalTransform(ChildKey, false);
					return true;
				}

				if (!Target.ParentElement.IsValid())
				{
					return false;
				}

				if (!Hierarchy->Contains(Target.ParentElement))
				{
					return false;
				}

				const FTransform ParentGlobal = Hierarchy->GetGlobalTransform(Target.ParentElement, false);
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

				DesiredGlobal = FTransform::Identity;
				DesiredGlobal.SetTranslation(FMath::Lerp(FromGlobal.GetTranslation(), ToGlobal.GetTranslation(), State.BlendAlpha));
				DesiredGlobal.SetScale3D(FMath::Lerp(FromGlobal.GetScale3D(), ToGlobal.GetScale3D(), State.BlendAlpha));
				DesiredGlobal.SetRotation(FQuat::Slerp(FromGlobal.GetRotation(), ToGlobal.GetRotation(), State.BlendAlpha).GetNormalized());

				if (State.BlendAlpha >= 1.0f - KINDA_SMALL_NUMBER)
				{
					if (State.BlendTo.Mode == URshipRigController::FRshipRigAttachTarget::EMode::UnconstrainedPose)
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
			const FTransform AppliedGlobal = Hierarchy->GetGlobalTransform(ChildKey, false);
			ChildrenToPersist.Add(ChildBone);
		}

		Controller->CommitPendingAttachStates(AttachStates, ChildrenToRemove, ChildrenToPersist);
	}
}
