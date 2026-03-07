#pragma once

#include "CoreMinimal.h"
#include "Controllers/RshipControllerComponent.h"
#include "RshipLightController.generated.h"

class ULightComponent;

UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship Light Controller"))
class RSHIPEXEC_API URshipLightController : public URshipControllerComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Light")
	bool bIncludeCommonProperties = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Light")
	bool bIncludeTypeSpecificProperties = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Light")
	FString ChildTargetSuffix = TEXT("light");

private:
	virtual void RegisterOrRefreshTarget() override;
	ULightComponent* ResolveLightComponent() const;
};
