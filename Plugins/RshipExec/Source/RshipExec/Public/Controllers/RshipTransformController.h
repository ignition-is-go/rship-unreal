#pragma once

#include "CoreMinimal.h"
#include "Controllers/RshipControllerComponent.h"
#include "RshipTransformController.generated.h"

UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship Transform Controller"))
class RSHIPEXEC_API URshipTransformController : public URshipControllerComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Transform")
	bool bExposeLocation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Transform")
	bool bExposeRotation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Transform")
	bool bExposeScale = true;

	UFUNCTION()
	void SetRelativeLocationAction(float X, float Y, float Z);

	UFUNCTION()
	void SetRelativeRotationAction(float X, float Y, float Z);

	UFUNCTION()
	void SetRelativeScaleAction(float X, float Y, float Z);

private:
	virtual void OnBeforeRegisterRshipBindings() override;
	virtual void RegisterOrRefreshTarget() override;
};
