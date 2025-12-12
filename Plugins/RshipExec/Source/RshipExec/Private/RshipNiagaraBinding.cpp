// Rship Niagara VFX Binding Implementation

#include "RshipNiagaraBinding.h"
#include "RshipSubsystem.h"
#include "RshipPulseReceiver.h"
#include "Logs.h"
#include "Engine/Engine.h"
#include "NiagaraComponent.h"
#include "Dom/JsonObject.h"

// ============================================================================
// NIAGARA BINDING COMPONENT
// ============================================================================

URshipNiagaraBinding::URshipNiagaraBinding()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.033f; // ~30Hz
}

void URshipNiagaraBinding::BeginPlay()
{
    Super::BeginPlay();

    // Get subsystem
    if (GEngine)
    {
        Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
    }

    // Auto-find Niagara component if not set
    if (!NiagaraComponent)
    {
        NiagaraComponent = GetOwner()->FindComponentByClass<UNiagaraComponent>();
    }

    if (!NiagaraComponent)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("RshipNiagaraBinding: No Niagara component found on %s"),
            *GetOwner()->GetName());
        return;
    }

    // Subscribe to pulse events
    if (Subsystem)
    {
        URshipPulseReceiver* Receiver = Subsystem->GetPulseReceiver();
        if (Receiver)
        {
            PulseReceivedHandle = Receiver->OnEmitterPulseReceived.AddLambda(
                [this](const FString& InEmitterId, float Intensity, FLinearColor Color, TSharedPtr<FJsonObject> Data)
                {
                    FString MyEmitterId = GetFullEmitterId();
                    if (InEmitterId == MyEmitterId)
                    {
                        OnPulseReceivedInternal(InEmitterId, Intensity, Color, Data);
                    }
                });
        }
    }

    // Register with manager
    if (Subsystem)
    {
        // Manager registration could go here if needed
    }

    UE_LOG(LogRshipExec, Log, TEXT("RshipNiagaraBinding: Initialized for %s on %s"),
        *GetFullEmitterId(), *GetOwner()->GetName());
}

void URshipNiagaraBinding::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Unsubscribe
    if (Subsystem && PulseReceivedHandle.IsValid())
    {
        URshipPulseReceiver* Receiver = Subsystem->GetPulseReceiver();
        if (Receiver)
        {
            Receiver->OnEmitterPulseReceived.Remove(PulseReceivedHandle);
        }
    }

    Super::EndPlay(EndPlayReason);
}

void URshipNiagaraBinding::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!NiagaraComponent) return;

    // Update smoothed values
    for (FRshipNiagaraParameterBinding& Binding : FloatBindings)
    {
        if (!Binding.bEnabled) continue;

        if (Binding.Smoothing > 0.0f)
        {
            // Apply smoothing
            float Alpha = 1.0f - FMath::Pow(Binding.Smoothing, DeltaTime * 60.0f);
            Binding.SmoothedValue = FMath::Lerp(Binding.SmoothedValue, Binding.LastValue, Alpha);

            // Apply to Niagara
            NiagaraComponent->SetVariableFloat(Binding.NiagaraParameter, Binding.SmoothedValue);
        }
    }

    // Check auto-deactivate
    if (bAutoDeactivate && bIsReceivingPulses)
    {
        double TimeSinceLastPulse = FPlatformTime::Seconds() - LastPulseTime;
        if (TimeSinceLastPulse > 1.0 || CurrentIntensity < DeactivateThreshold)
        {
            if (NiagaraComponent->IsActive())
            {
                NiagaraComponent->Deactivate();
                bIsReceivingPulses = false;
            }
        }
    }
}

void URshipNiagaraBinding::OnPulseReceivedInternal(const FString& InEmitterId, float Intensity, FLinearColor Color, TSharedPtr<FJsonObject> Data)
{
    if (!NiagaraComponent) return;

    CurrentIntensity = Intensity;
    CurrentColor = Color;
    LastPulseTime = FPlatformTime::Seconds();
    bIsReceivingPulses = true;

    // Auto-activate
    if (bAutoActivateOnPulse && !NiagaraComponent->IsActive() && Intensity > DeactivateThreshold)
    {
        NiagaraComponent->Activate(true);
    }

    // Apply bindings
    ApplyBindings(Data);

    // Fire event
    OnPulseReceived.Broadcast(InEmitterId, Intensity);
}

void URshipNiagaraBinding::ApplyBindings(TSharedPtr<FJsonObject> Data)
{
    if (!NiagaraComponent || !Data.IsValid()) return;

    // Apply float bindings
    for (FRshipNiagaraParameterBinding& Binding : FloatBindings)
    {
        if (!Binding.bEnabled) continue;

        float RawValue = GetFloatFromJson(Data, Binding.PulseField);
        float ProcessedValue = ProcessBindingValue(Binding, RawValue, 0.0f);

        Binding.LastValue = ProcessedValue;

        // Apply immediately if no smoothing
        if (Binding.Smoothing <= 0.0f)
        {
            Binding.SmoothedValue = ProcessedValue;
            NiagaraComponent->SetVariableFloat(Binding.NiagaraParameter, ProcessedValue);
        }
    }

    // Apply color bindings
    for (FRshipNiagaraColorBinding& Binding : ColorBindings)
    {
        if (!Binding.bEnabled) continue;

        FLinearColor Color = GetColorFromJson(Data, Binding.ColorFieldPrefix);

        if (Binding.bMultiplyByIntensity)
        {
            float Intensity = GetFloatFromJson(Data, Binding.IntensityField);
            Color *= Intensity;
        }

        NiagaraComponent->SetVariableLinearColor(Binding.NiagaraColorParameter, Color);
    }
}

float URshipNiagaraBinding::ProcessBindingValue(FRshipNiagaraParameterBinding& Binding, float RawValue, float DeltaTime)
{
    float Result = RawValue;

    switch (Binding.Mode)
    {
        case ERshipNiagaraBindingMode::Direct:
            Result = RawValue;
            break;

        case ERshipNiagaraBindingMode::Normalized:
            Result = FMath::Clamp(RawValue, 0.0f, 1.0f);
            break;

        case ERshipNiagaraBindingMode::Scaled:
            Result = RawValue * Binding.ScaleFactor;
            break;

        case ERshipNiagaraBindingMode::Mapped:
            {
                float Normalized = (RawValue - Binding.InputMin) / (Binding.InputMax - Binding.InputMin);
                Normalized = FMath::Clamp(Normalized, 0.0f, 1.0f);
                Result = FMath::Lerp(Binding.OutputMin, Binding.OutputMax, Normalized);
            }
            break;

        case ERshipNiagaraBindingMode::Curve:
            {
                const FRichCurve* Curve = Binding.ResponseCurve.GetRichCurveConst();
                if (Curve)
                {
                    Result = Curve->Eval(RawValue);
                }
            }
            break;

        case ERshipNiagaraBindingMode::Trigger:
            Result = (RawValue >= Binding.TriggerThreshold) ? 1.0f : 0.0f;
            break;
    }

    return Result;
}

float URshipNiagaraBinding::GetFloatFromJson(TSharedPtr<FJsonObject> Data, const FString& FieldPath)
{
    if (!Data.IsValid()) return 0.0f;

    // Handle nested paths like "color.r"
    TArray<FString> Parts;
    FieldPath.ParseIntoArray(Parts, TEXT("."));

    TSharedPtr<FJsonObject> Current = Data;
    for (int32 i = 0; i < Parts.Num() - 1; i++)
    {
        const TSharedPtr<FJsonObject>* NextObj;
        if (!Current->TryGetObjectField(Parts[i], NextObj))
        {
            return 0.0f;
        }
        Current = *NextObj;
    }

    double Value = 0.0;
    Current->TryGetNumberField(Parts.Last(), Value);
    return (float)Value;
}

FLinearColor URshipNiagaraBinding::GetColorFromJson(TSharedPtr<FJsonObject> Data, const FString& Prefix)
{
    if (!Data.IsValid()) return FLinearColor::White;

    const TSharedPtr<FJsonObject>* ColorObj;
    if (Data->TryGetObjectField(Prefix, ColorObj))
    {
        double R = 1.0, G = 1.0, B = 1.0, A = 1.0;
        (*ColorObj)->TryGetNumberField(TEXT("r"), R);
        (*ColorObj)->TryGetNumberField(TEXT("g"), G);
        (*ColorObj)->TryGetNumberField(TEXT("b"), B);
        (*ColorObj)->TryGetNumberField(TEXT("a"), A);
        return FLinearColor(R, G, B, A);
    }

    return FLinearColor::White;
}

FString URshipNiagaraBinding::GetFullEmitterId() const
{
    if (!EmitterId.IsEmpty())
    {
        return EmitterId;
    }

    if (!TargetId.IsEmpty() && !EmitterName.IsEmpty())
    {
        return TargetId + TEXT(":") + EmitterName;
    }

    return TEXT("");
}

void URshipNiagaraBinding::SetFloatParameter(FName ParameterName, float Value)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableFloat(ParameterName, Value);
    }
}

void URshipNiagaraBinding::SetColorParameter(FName ParameterName, FLinearColor Color)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableLinearColor(ParameterName, Color);
    }
}

void URshipNiagaraBinding::ForceUpdate()
{
    // Force-apply all current smoothed values
    if (!NiagaraComponent) return;

    for (FRshipNiagaraParameterBinding& Binding : FloatBindings)
    {
        if (Binding.bEnabled)
        {
            NiagaraComponent->SetVariableFloat(Binding.NiagaraParameter, Binding.SmoothedValue);
        }
    }
}

void URshipNiagaraBinding::SetBindingsEnabled(bool bEnabled)
{
    for (FRshipNiagaraParameterBinding& Binding : FloatBindings)
    {
        Binding.bEnabled = bEnabled;
    }

    for (FRshipNiagaraColorBinding& Binding : ColorBindings)
    {
        Binding.bEnabled = bEnabled;
    }
}

// ============================================================================
// NIAGARA MANAGER
// ============================================================================

void URshipNiagaraManager::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;
    UE_LOG(LogRshipExec, Log, TEXT("NiagaraManager initialized"));
}

void URshipNiagaraManager::Shutdown()
{
    RegisteredBindings.Empty();
    Subsystem = nullptr;
}

void URshipNiagaraManager::Tick(float DeltaTime)
{
    // Stub - bindings update themselves through pulse callbacks
}

void URshipNiagaraManager::RegisterBinding(URshipNiagaraBinding* Binding)
{
    if (Binding && !RegisteredBindings.Contains(Binding))
    {
        RegisteredBindings.Add(Binding);
    }
}

void URshipNiagaraManager::UnregisterBinding(URshipNiagaraBinding* Binding)
{
    RegisteredBindings.Remove(Binding);
}

void URshipNiagaraManager::SetGlobalIntensityMultiplier(float Multiplier)
{
    GlobalIntensityMultiplier = FMath::Max(0.0f, Multiplier);
}
