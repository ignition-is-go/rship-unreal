#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Controllers/RshipControllerComponent.h"
#include "RshipRigController.generated.h"

class UControlRigComponent;
class UControlRig;
class URigHierarchy;
class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class ERshipRigTransformChoice : uint8
{
	Initial,
	Current
};

UCLASS()
class RSHIPEXEC_API URshipRigBoneActionProxy : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(class URshipRigController* InController, const FName& InBoneName);
	FName GetBoneName() const { return BoneName; }

	UFUNCTION()
	void RotateInSocket(float X, float Y, float Z);

	UFUNCTION()
	void AttachToBone(FString ParentBoneName,
		ERshipRigTransformChoice ParentLocation = ERshipRigTransformChoice::Current,
		ERshipRigTransformChoice ParentRotation = ERshipRigTransformChoice::Current,
		ERshipRigTransformChoice ChildLocation = ERshipRigTransformChoice::Current,
		ERshipRigTransformChoice ChildRotation = ERshipRigTransformChoice::Current,
		float BlendSeconds = -1.0f);

	UFUNCTION()
	void ResetToInitialWorld();

	UFUNCTION()
	void RemoveConstraints(float BlendSeconds = -1.0f);

private:
	UPROPERTY()
	TObjectPtr<class URshipRigController> Controller;

	UPROPERTY()
	FName BoneName;
};

UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship Rig Controller"))
class RSHIPEXEC_API URshipRigController : public URshipControllerComponent
{
	GENERATED_BODY()

public:
	URshipRigController();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Rig")
	TObjectPtr<UControlRigComponent> ControlRigComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Rig")
	TObjectPtr<UControlRig> ControlRig;

	UFUNCTION()
	void ResetAllBonesToInitialWorld();

private:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnBeforeRegisterRshipTargets() override;
	void ConfigureControlRigComponent();
	UFUNCTION()
	void HandleControlRigComponentPreForwardsSolve(UControlRigComponent* InComponent);
	void HandleControlRigPreForwardsSolve(UControlRig* InRig, const FName& InEventName);
	virtual void RegisterOrRefreshTarget() override;
	void RotateBoneInSocket(const FName& BoneName, const FRotator& Rotation);
	void AttachBoneToParent(const FName& BoneName, const FName& ParentBoneName,
		ERshipRigTransformChoice ParentLocation,
		ERshipRigTransformChoice ParentRotation,
		ERshipRigTransformChoice ChildLocation,
		ERshipRigTransformChoice ChildRotation,
		float BlendSeconds);
	void RemoveBoneConstraints(const FName& BoneName, float BlendSeconds);
	void ResetBoneToInitialWorld(const FName& BoneName);
	FName ResolveBoneNameFromInput(const FString& BoneIdentifier, URigHierarchy* Hierarchy) const;
	UControlRigComponent* ResolveControlRigComponent();
	UControlRig* ResolveControlRig();
	URigHierarchy* ResolveRigHierarchy();
	void ApplyPendingRigState(float DeltaTime);

	struct FRshipRigAttachTarget
	{
		enum class EMode : uint8
		{
			ParentRelative,
			WorldSpace,
			UnconstrainedPose
		};

		EMode Mode = EMode::ParentRelative;
		FName ParentBone;
		FTransform LocalOffset = FTransform::Identity;
		FTransform WorldTransform = FTransform::Identity;
	};

	struct FRshipRigAttachState
	{
		FRshipRigAttachTarget Active;
		FRshipRigAttachTarget BlendFrom;
		FRshipRigAttachTarget BlendTo;
		float BlendAlpha = 1.0f;
		float BlendDuration = 0.0f;
		bool bBlendActive = false;
	};

	UPROPERTY(Transient)
	TArray<TObjectPtr<URshipRigBoneActionProxy>> BoneActionProxies;

	UPROPERTY(Transient)
	TMap<FString, FName> BoneSegmentToName;

	UPROPERTY(Transient)
	TWeakObjectPtr<USkeletalMeshComponent> CachedBoundMesh;

	UPROPERTY(Transient)
	TWeakObjectPtr<UControlRig> CachedControlRig;

	UPROPERTY(Transient)
	bool bRigComponentConfigured = false;

	UPROPERTY(Transient)
	TMap<FName, FQuat> BoneRotationOverrides;

	mutable FCriticalSection RigStateMutex;
	TMap<FName, FRshipRigAttachState> BoneAttachStates;
	float LastTickDeltaTime = 0.0f;

	friend class URshipRigBoneActionProxy;
};
