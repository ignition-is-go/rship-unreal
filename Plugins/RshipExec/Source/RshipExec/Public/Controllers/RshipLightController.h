#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RshipActionProvider.h"
#include "RshipLightController.generated.h"

class ULightComponent;
class URshipTargetComponent;

UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship Light Controller"))
class RSHIPEXEC_API URshipLightController : public UActorComponent, public IRshipActionProvider
{
	GENERATED_BODY()

public:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void RegisterRshipWhitelistedActions(URshipTargetComponent* TargetComponent) override;
	virtual void OnRshipAfterTake(URshipTargetComponent* TargetComponent, const FString& ActionName, UObject* ActionOwner) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Light")
	bool bIncludeCommonProperties = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Light")
	bool bIncludeTypeSpecificProperties = true;

private:
	ULightComponent* ResolveLightComponent() const;
	void NotifyLightEdited(ULightComponent* InLight) const;
};
