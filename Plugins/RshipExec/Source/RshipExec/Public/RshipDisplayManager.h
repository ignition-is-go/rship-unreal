#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RshipDisplayManager.generated.h"

class URshipSubsystem;
class FJsonObject;

UCLASS(BlueprintType)
class RSHIPEXEC_API URshipDisplayManager : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(URshipSubsystem* InSubsystem);
    void Shutdown();
    void Tick(float DeltaTime);

    void ProcessProfileEvent(const TSharedPtr<FJsonObject>& Data, bool bIsDelete);
    bool RouteAction(const FString& TargetId, const FString& ActionId, const TSharedRef<FJsonObject>& Data);

    UFUNCTION(BlueprintCallable, Category = "Rship|Display")
    bool CollectSnapshot();

    UFUNCTION(BlueprintCallable, Category = "Rship|Display")
    bool BuildKnownDisplays();

    UFUNCTION(BlueprintCallable, Category = "Rship|Display")
    bool ResolveIdentity(const FString& PinsJson = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "Rship|Display")
    bool ValidateProfileJson(const FString& ProfileJson);

    UFUNCTION(BlueprintCallable, Category = "Rship|Display")
    bool PlanProfileJson(const FString& ProfileJson);

    UFUNCTION(BlueprintCallable, Category = "Rship|Display")
    bool ApplyLastPlan(bool bDryRun);

    UFUNCTION(BlueprintCallable, Category = "Rship|Display")
    void SetDebugOverlayEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "Rship|Display")
    bool IsDebugOverlayEnabled() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Display")
    FString GetActiveProfileJson() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Display")
    FString GetLastSnapshotJson() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Display")
    FString GetLastKnownJson() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Display")
    FString GetLastIdentityJson() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Display")
    FString GetLastValidationJson() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Display")
    FString GetLastPlanJson() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Display")
    FString GetLastLedgerJson() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Display")
    FString GetLastApplyJson() const;

    UFUNCTION(BlueprintCallable, Category = "Rship|Display")
    FString GetLastError() const;

private:
    UPROPERTY()
    URshipSubsystem* Subsystem = nullptr;

    FString ActiveProfileJson;
    FString LastSnapshotJson;
    FString LastKnownJson;
    FString LastIdentityJson;
    FString LastValidationJson;
    FString LastPlanJson;
    FString LastLedgerJson;
    FString LastApplyJson;
    FString LastError;

    bool bWasConnected = false;
    bool bDebugOverlayEnabled = false;
    float DebugOverlayAccumulated = 0.0f;

    static FString GetTargetId();
    static FString ExtractActionName(const FString& ActionId);

    bool ParseEnvelope(const FString& EnvelopeJson, FString& OutDataJson, FString& OutError) const;
    void RegisterTarget();
    void EmitState(const FString& Status);
    void PulseJsonEmitter(const FString& EmitterName, const FString& JsonPayload) const;
    FString GetStateCachePath() const;
    void LoadStateCache();
    void SaveStateCache() const;
};
