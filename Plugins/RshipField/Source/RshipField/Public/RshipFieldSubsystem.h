#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "RshipFieldTypes.h"
#include "RshipFieldSubsystem.generated.h"

class URshipFieldTargetComponent;
class UTextureRenderTarget2D;

UCLASS()
class RSHIPFIELD_API URshipFieldSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override;

    bool EnqueuePacketJson(const FString& PacketJson, FString* OutError = nullptr);

    void RegisterTarget(URshipFieldTargetComponent* Target);
    void UnregisterTarget(URshipFieldTargetComponent* Target);

    void SetUpdateHz(float InHz);
    void SetBpm(float InBpm);
    void SetTransport(float InPhase, bool bInPlaying);
    void SetMasterScalarGain(float InGain);
    void SetMasterVectorGain(float InGain);
    void SetDebugEnabled(bool bEnabled);
    void SetDebugMode(ERshipFieldDebugMode InMode);
    void SetDebugSlice(const FString& InAxis, float InPosition01);
    void SetDebugSelection(const FString& InSelectionId);

    UTextureRenderTarget2D* GetGlobalScalarFieldAtlas() const { return GlobalScalarFieldAtlas; }
    UTextureRenderTarget2D* GetGlobalVectorFieldAtlas() const { return GlobalVectorFieldAtlas; }

    FRshipFieldTargetIdentity BuildIdentityForTarget(const URshipFieldTargetComponent* Target) const;
    int64 GetLastAppliedSequence_Debug() const { return LastAppliedSequence; }
    int64 GetSimulationFrame_Debug() const { return SimulationFrame; }
    int32 GetPendingPacketCount_Debug() const { return PendingPackets.Num(); }

private:
    struct FQueuedPacket
    {
        int64 Sequence = 0;
        int64 ApplyFrame = INDEX_NONE;
        FRshipFieldPacket Packet;
    };

    bool ParsePacket(const FString& PacketJson, FRshipFieldPacket& OutPacket, FString& OutError) const;
    void NormalizePacket(FRshipFieldPacket& Packet) const;
    void ApplyPacketState(const FRshipFieldPacket& Packet);
    void ApplyQueuedPackets();
    void RebuildCompiledSplineEmitters();

    bool EnsureGlobalAtlasTextures();
    void DispatchFieldPasses();
    int32 NormalizeResolution(int32 RequestedResolution) const;
    const FRshipFieldTargetDesc* FindTargetOverride(const URshipFieldTargetComponent* Target) const;
    FRshipFieldTargetDesc BuildResolvedTargetDesc(const URshipFieldTargetComponent* Target) const;

private:
    UPROPERTY(Transient)
    TObjectPtr<UTextureRenderTarget2D> GlobalScalarFieldAtlas = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UTextureRenderTarget2D> GlobalVectorFieldAtlas = nullptr;

    UPROPERTY(Transient)
    TArray<TObjectPtr<URshipFieldTargetComponent>> RegisteredTargets;

    FRshipFieldGlobalParams GlobalParams;
    FRshipFieldTransportState TransportState;

    TArray<FRshipFieldLayerDesc> ActiveLayers;
    TArray<FRshipFieldPhaseGroupDesc> ActivePhaseGroups;
    TArray<FRshipFieldEmitterDesc> ActiveEmitters;
    TArray<FRshipFieldSplineEmitterDesc> ActiveSplines;
    TArray<FRshipFieldEmitterDesc> ActiveSplineCompiledEmitters;
    TArray<FRshipFieldTargetDesc> ActiveTargetOverrides;

    TArray<FQueuedPacket> PendingPackets;

    int64 LastAppliedSequence = 0;
    int64 SimulationFrame = 0;
    float SimulationTimeSeconds = 0.0f;
    float TickAccumulator = 0.0f;

    bool bDebugEnabled = false;
    ERshipFieldDebugMode DebugMode = ERshipFieldDebugMode::Off;
    FString DebugSliceAxis = TEXT("z");
    float DebugSlicePosition01 = 0.5f;
    FString DebugSelection;

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rship|Field|Perf")
    int32 LastDispatchedTargetCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rship|Field|Perf")
    int32 LastActiveEmitterCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rship|Field|Perf")
    int32 LastQueuedPacketCount = 0;
};
