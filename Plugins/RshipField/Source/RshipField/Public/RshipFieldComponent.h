#pragma once

#include "CoreMinimal.h"
#include "Controllers/RshipControllerComponent.h"
#include "RshipFieldTypes.h"
#include "RshipFieldComponent.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class URshipFieldSubsystem;
class UTextureRenderTarget2D;

UCLASS(ClassGroup = (Rship), meta = (BlueprintSpawnableComponent, DisplayName = "Rship Field"))
class RSHIPFIELD_API URshipFieldComponent : public URshipControllerComponent
{
    GENERATED_BODY()

public:
    URshipFieldComponent();

    virtual void OnRegister() override;
    virtual void OnUnregister() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FString ChildTargetSuffix = TEXT("field");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FString FieldId = TEXT("default");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    bool bEnabled = true;

    // Global
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float UpdateHz = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    int32 FieldResolution = 128;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float MasterScalarGain = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    float MasterVectorGain = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field")
    FVector DomainCenterCm = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field", meta = (ClampMin = "1.0"))
    float DomainSizeCm = 10000.0f;

    // Transport clock — drives all phase groups in this field.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Transport")
    float Bpm = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Transport")
    float BeatPhase = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Transport")
    bool bPlaying = true;

    // Phase groups sync effectors to the transport clock at different tempo divisions.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Transport")
    TArray<FRshipFieldPhaseGroup> PhaseGroups;

    // Effectors
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Effectors")
    TArray<FRshipFieldWaveEffector> WaveEffectors;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Effectors")
    TArray<FRshipFieldNoiseEffector> NoiseEffectors;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Effectors")
    TArray<FRshipFieldAttractorEffector> AttractorEffectors;

    // Debug
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Debug")
    bool bShowWireframes = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Debug")
    bool bShowDebugText = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Debug")
    bool bShowVisualizer = false;

    // Actions
    UFUNCTION()
    void SetUpdateHzAction(float Hz);

    UFUNCTION()
    void SetFieldResolutionAction(int32 Resolution);

    UFUNCTION()
    void SetMasterScalarGainAction(float Gain);

    UFUNCTION()
    void SetMasterVectorGainAction(float Gain);

    UFUNCTION()
    void SetDomainCenterAction(float X, float Y, float Z);

    UFUNCTION()
    void SetDomainSizeAction(float SizeCm);

    UFUNCTION()
    void SetBpmAction(float InBpm);

    UFUNCTION()
    void SetTransportAction(float Phase, bool Playing);

    UFUNCTION()
    void SetFieldState(const FString& StateJson);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rship|Field|Debug", AdvancedDisplay)
    FString LastFieldStateError;

    // Atlas textures owned by this field. Subsystem dispatches into these.
    UTextureRenderTarget2D* GetScalarAtlas() const { return ScalarAtlas; }
    UTextureRenderTarget2D* GetVectorAtlas() const { return VectorAtlas; }

    bool EnsureAtlasTextures();

    // Simulation state
    float SimulationTimeSeconds = 0.0f;
    int64 SimulationFrame = 0;
    float TickAccumulator = 0.0f;

private:
    virtual void RegisterOrRefreshTarget() override;
    void UpdateVisualizer();

    UPROPERTY(Transient)
    TObjectPtr<UTextureRenderTarget2D> ScalarAtlas = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UTextureRenderTarget2D> VectorAtlas = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UNiagaraComponent> VisualizerComponent = nullptr;
};
