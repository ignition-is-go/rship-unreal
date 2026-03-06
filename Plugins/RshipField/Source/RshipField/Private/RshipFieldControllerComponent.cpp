#include "RshipFieldControllerComponent.h"

#include "RshipFieldSubsystem.h"

#include "Engine/World.h"

URshipFieldControllerComponent::URshipFieldControllerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

URshipFieldSubsystem* URshipFieldControllerComponent::GetFieldSubsystem() const
{
    if (UWorld* World = GetWorld())
    {
        return World->GetSubsystem<URshipFieldSubsystem>();
    }
    return nullptr;
}

bool URshipFieldControllerComponent::RS_ApplyFieldPacket(const FString& PacketJson)
{
    LastPacketError.Reset();

    URshipFieldSubsystem* Subsystem = GetFieldSubsystem();
    if (!Subsystem)
    {
        LastPacketError = TEXT("RshipFieldSubsystem unavailable.");
        return false;
    }

    if (!Subsystem->EnqueuePacketJson(PacketJson, &LastPacketError))
    {
        if (LastPacketError.IsEmpty())
        {
            LastPacketError = TEXT("Failed to enqueue field packet.");
        }
        return false;
    }

    return true;
}

void URshipFieldControllerComponent::RS_FieldSetUpdateHz(float Hz)
{
    if (URshipFieldSubsystem* Subsystem = GetFieldSubsystem())
    {
        Subsystem->SetUpdateHz(Hz);
    }
}

void URshipFieldControllerComponent::RS_FieldSetBpm(float Bpm)
{
    if (URshipFieldSubsystem* Subsystem = GetFieldSubsystem())
    {
        Subsystem->SetBpm(Bpm);
    }
}

void URshipFieldControllerComponent::RS_FieldSetTransport(float BeatPhase, bool bPlaying)
{
    if (URshipFieldSubsystem* Subsystem = GetFieldSubsystem())
    {
        Subsystem->SetTransport(BeatPhase, bPlaying);
    }
}

void URshipFieldControllerComponent::RS_FieldSetMasterScalarGain(float Gain)
{
    if (URshipFieldSubsystem* Subsystem = GetFieldSubsystem())
    {
        Subsystem->SetMasterScalarGain(Gain);
    }
}

void URshipFieldControllerComponent::RS_FieldSetMasterVectorGain(float Gain)
{
    if (URshipFieldSubsystem* Subsystem = GetFieldSubsystem())
    {
        Subsystem->SetMasterVectorGain(Gain);
    }
}

void URshipFieldControllerComponent::RS_FieldDebugSetEnabled(bool bEnabled)
{
    bDebugEnabled = bEnabled;
    if (URshipFieldSubsystem* Subsystem = GetFieldSubsystem())
    {
        Subsystem->SetDebugEnabled(bEnabled);
    }
}

void URshipFieldControllerComponent::RS_FieldDebugSetMode(const FString& Mode)
{
    if (URshipFieldSubsystem* Subsystem = GetFieldSubsystem())
    {
        const FString Token = Mode.TrimStartAndEnd().ToLower();
        ERshipFieldDebugMode Parsed = ERshipFieldDebugMode::Off;
        if (Token == TEXT("heatmap"))
        {
            Parsed = ERshipFieldDebugMode::Heatmap;
        }
        else if (Token == TEXT("slice_scalar") || Token == TEXT("slice-scalar"))
        {
            Parsed = ERshipFieldDebugMode::SliceScalar;
        }
        else if (Token == TEXT("slice_vector") || Token == TEXT("slice-vector"))
        {
            Parsed = ERshipFieldDebugMode::SliceVector;
        }
        else if (Token == TEXT("isolate_emitter") || Token == TEXT("isolate-emitter"))
        {
            Parsed = ERshipFieldDebugMode::IsolateEmitter;
        }
        else if (Token == TEXT("isolate_layer") || Token == TEXT("isolate-layer"))
        {
            Parsed = ERshipFieldDebugMode::IsolateLayer;
        }
        else if (Token == TEXT("contrib_scalar") || Token == TEXT("contrib-scalar"))
        {
            Parsed = ERshipFieldDebugMode::ContribScalar;
        }
        else if (Token == TEXT("contrib_vector") || Token == TEXT("contrib-vector"))
        {
            Parsed = ERshipFieldDebugMode::ContribVector;
        }

        DebugMode = Parsed;
        Subsystem->SetDebugMode(Parsed);
    }
}

void URshipFieldControllerComponent::RS_FieldDebugSetSlice(const FString& Axis, float Position01)
{
    if (URshipFieldSubsystem* Subsystem = GetFieldSubsystem())
    {
        Subsystem->SetDebugSlice(Axis, Position01);
    }
}

void URshipFieldControllerComponent::RS_FieldDebugSetSelection(const FString& SelectionId)
{
    if (URshipFieldSubsystem* Subsystem = GetFieldSubsystem())
    {
        Subsystem->SetDebugSelection(SelectionId);
    }
}
