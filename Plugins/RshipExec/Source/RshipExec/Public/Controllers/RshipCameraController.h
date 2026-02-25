#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RshipActionProvider.h"
#include "RshipCameraController.generated.h"

class UCameraComponent;
class UCineCameraComponent;
class URshipTargetComponent;

UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship Camera Controller"))
class RSHIPEXEC_API URshipCameraController : public UActorComponent, public IRshipActionProvider
{
	GENERATED_BODY()

public:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void RegisterRshipWhitelistedActions(URshipTargetComponent* TargetComponent) override;
	virtual void OnRshipAfterTake(URshipTargetComponent* TargetComponent, const FString& ActionName, UObject* ActionOwner) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera")
	bool bIncludeCommonCameraProperties = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Camera")
	bool bIncludeCineCameraProperties = true;

private:
	UCameraComponent* ResolveCameraComponent() const;
	UCineCameraComponent* ResolveCineCameraComponent() const;
	void NotifyCameraEdited(UCameraComponent* InCamera) const;
};
