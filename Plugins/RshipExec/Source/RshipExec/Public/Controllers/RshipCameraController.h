#pragma once

#include "CoreMinimal.h"
#include "Controllers/RshipControllerComponent.h"
#include "RshipCameraController.generated.h"

class UCameraComponent;
class UCineCameraComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRshipCameraFloatEmitter, float, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FRshipCameraVectorEmitter, float, X, float, Y, float, Z);

UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship Camera Controller"))
class RSHIPEXEC_API URshipCameraController : public URshipControllerComponent
{
	GENERATED_BODY()

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION()
	void SetFieldOfViewAction(float Value);

	UFUNCTION()
	void SetFocalLengthAction(float Value);

	UFUNCTION()
	void SetApertureAction(float Value);

	UFUNCTION()
	void SetFocusDistanceAction(float Value);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera")
	bool bIncludeCommonCameraProperties = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera")
	bool bIncludeCineCameraProperties = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera")
	bool bPublishStateEmitters = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera", meta = (ClampMin = "1", ClampMax = "120"))
	int32 PublishRateHz = 30;

	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera|Emitters")
	FRshipCameraFloatEmitter OnFocalLengthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera|Emitters")
	FRshipCameraFloatEmitter OnApertureChanged;

	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera|Emitters")
	FRshipCameraFloatEmitter OnFocusDistanceChanged;

	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera|Emitters")
	FRshipCameraFloatEmitter OnHorizontalFovChanged;

	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera|Emitters")
	FRshipCameraFloatEmitter OnVerticalFovChanged;

	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera|Emitters")
	FRshipCameraVectorEmitter OnLocationChanged;

	UPROPERTY(BlueprintAssignable, Category = "Rship|Camera|Emitters")
	FRshipCameraVectorEmitter OnRotationChanged;

private:
	virtual void OnBeforeRegisterRshipBindings() override;
	virtual void RegisterOrRefreshTarget() override;
	FString GetTargetId() const;
	UCameraComponent* ResolveCameraComponent() const;
	UCineCameraComponent* ResolveCineCameraComponent() const;
	void PublishState();

	double LastPublishTimeSeconds = 0.0;
};
