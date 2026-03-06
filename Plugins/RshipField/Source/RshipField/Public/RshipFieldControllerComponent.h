#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RshipFieldTypes.h"
#include "RshipFieldControllerComponent.generated.h"

class URshipFieldSubsystem;

UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent))
class RSHIPFIELD_API URshipFieldControllerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    URshipFieldControllerComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    bool bDebugEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    ERshipFieldDebugMode DebugMode = ERshipFieldDebugMode::Off;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rship|Field")
    FString LastPacketError;

    UFUNCTION(BlueprintCallable, Category = "Rship|Field")
    bool RS_ApplyFieldPacket(const FString& PacketJson);

    UFUNCTION(BlueprintCallable, Category = "Rship|Field")
    void RS_FieldSetUpdateHz(float Hz);

    UFUNCTION(BlueprintCallable, Category = "Rship|Field")
    void RS_FieldSetBpm(float Bpm);

    UFUNCTION(BlueprintCallable, Category = "Rship|Field")
    void RS_FieldSetTransport(float BeatPhase, bool bPlaying);

    UFUNCTION(BlueprintCallable, Category = "Rship|Field")
    void RS_FieldSetMasterScalarGain(float Gain);

    UFUNCTION(BlueprintCallable, Category = "Rship|Field")
    void RS_FieldSetMasterVectorGain(float Gain);

    UFUNCTION(BlueprintCallable, Category = "Rship|Field")
    void RS_FieldDebugSetEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "Rship|Field")
    void RS_FieldDebugSetMode(const FString& Mode);

    UFUNCTION(BlueprintCallable, Category = "Rship|Field")
    void RS_FieldDebugSetSlice(const FString& Axis, float Position01);

    UFUNCTION(BlueprintCallable, Category = "Rship|Field")
    void RS_FieldDebugSetSelection(const FString& SelectionId);

private:
    URshipFieldSubsystem* GetFieldSubsystem() const;
};
