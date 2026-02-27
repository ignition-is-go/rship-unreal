#pragma once

#include "CoreMinimal.h"
#include "Controllers/RshipControllerComponent.h"
#include "RshipRigController.generated.h"

class UControlRigComponent;

UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship Rig Controller"))
class RSHIPEXEC_API URshipRigController : public URshipControllerComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Rship|Rig")
	void LogBones();

	UFUNCTION(BlueprintCallable, Category = "Rship|Rig")
	void LogSockets();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Rig")
	TObjectPtr<UControlRigComponent> ControlRigComponent;

private:
	virtual void RegisterOrRefreshTarget() override;
};
