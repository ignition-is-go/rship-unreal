#pragma once

#include "CoreMinimal.h"
#include "Controllers/RshipControllerComponent.h"
#include "RshipRigController.generated.h"

class UControlRigComponent;
class UControlRig;
class URigHierarchy;
class URigHierarchyController;

UCLASS()
class RSHIPEXEC_API URshipRigBoneActionProxy : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(class URshipRigController* InController, const FName& InBoneName);

	UFUNCTION()
	void RotateInSocket(float X, float Y, float Z);

	UFUNCTION()
	void AttachToBone(FString ParentBoneName);

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Rig")
	TObjectPtr<UControlRigComponent> ControlRigComponent;

private:
	virtual void RegisterOrRefreshTarget() override;
	void RotateBoneInSocket(const FName& BoneName, const FRotator& Rotation);
	void AttachBoneToParent(const FName& BoneName, const FName& ParentBoneName);
	UControlRigComponent* ResolveControlRigComponent();
	UControlRig* ResolveControlRig();
	URigHierarchy* ResolveRigHierarchy();

	UPROPERTY(Transient)
	TArray<TObjectPtr<URshipRigBoneActionProxy>> BoneActionProxies;

	friend class URshipRigBoneActionProxy;
};
