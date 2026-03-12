#pragma once

#include "Units/RigUnit.h"
#include "RigUnit_RshipApplyPendingRigState.generated.h"

USTRUCT(meta = (DisplayName = "Rship Apply Pending Rig State", Category = "Rship", Keywords = "Rocketship,Rship,Rig,Bone,Constraint"))
struct RSHIPEXEC_API FRigUnit_RshipApplyPendingRigState : public FRigUnitMutable
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};
