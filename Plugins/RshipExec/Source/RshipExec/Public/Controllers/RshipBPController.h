#pragma once

#include "CoreMinimal.h"
#include "Controllers/RshipControllerComponent.h"
#include "RshipBPController.generated.h"

class FRshipRegisteredTarget;
class UObject;

UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship BP Controller"))
class RSHIPEXEC_API URshipBPController : public URshipControllerComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|BP")
	bool bScanOwnerActor = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|BP")
	bool bScanSiblingComponents = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|BP")
	bool bRequireRSPrefix = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|BP")
	FString ChildTargetSuffix = TEXT("bp");

private:
	virtual void RegisterOrRefreshTarget() override;
	void RegisterObjectMembers(FRshipRegisteredTarget& Target, UObject* Object) const;
	bool ShouldRegisterMemberName(const FString& Name) const;
};
