#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RshipActionProvider.h"
#include "RshipTransformController.generated.h"

class USceneComponent;
class URshipTargetComponent;

UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship Transform Controller"))
class RSHIPEXEC_API URshipTransformController : public UActorComponent, public IRshipActionProvider
{
	GENERATED_BODY()

public:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void RegisterRshipWhitelistedActions(URshipTargetComponent* TargetComponent) override;
	virtual void OnRshipAfterTake(URshipTargetComponent* TargetComponent, const FString& ActionName, UObject* ActionOwner) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Transform")
	bool bExposeLocation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Transform")
	bool bExposeRotation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Transform")
	bool bExposeScale = true;

private:
	void ApplyTransformRuntimeRefresh(USceneComponent* Root, const FString& ActionName) const;
	void NotifyEditorTransformChanged() const;
	bool IsTransformAction(const FString& ActionName) const;
};
