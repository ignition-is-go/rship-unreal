// Rship PCG (Procedural Content Generation) Binding Implementation
//
// NOTE: This file is excluded from compilation when PCG plugin is not enabled.
// See RshipExec.Build.cs for conditional compilation logic.

#include "RshipPCGBinding.h"
#include "RshipSubsystem.h"
#include "RshipPulseReceiver.h"
#include "PCGGraph.h"
#include "PCGSubsystem.h"
#include "Engine/Engine.h"

// ============================================================================
// URshipPCGBinding
// ============================================================================

URshipPCGBinding::URshipPCGBinding()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void URshipPCGBinding::BeginPlay()
{
    Super::BeginPlay();

    Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();

    if (bAutoDiscoverPCGComponent)
    {
        DiscoverPCGComponent();
    }

    BindToPulseReceiver();

    // Register with manager
    if (Subsystem)
    {
        if (URshipPCGManager* Manager = Subsystem->GetPCGManager())
        {
            Manager->RegisterBinding(this);
        }
    }
}

void URshipPCGBinding::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnbindFromPulseReceiver();

    // Unregister from manager
    if (Subsystem)
    {
        if (URshipPCGManager* Manager = Subsystem->GetPCGManager())
        {
            Manager->UnregisterBinding(this);
        }
    }

    Super::EndPlay(EndPlayReason);
}

void URshipPCGBinding::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update smoothed values
    UpdateSmoothing(DeltaTime);

    // Check if we should regenerate
    CheckAndTriggerRegen(DeltaTime);
}

void URshipPCGBinding::DiscoverPCGComponent()
{
    if (AActor* Owner = GetOwner())
    {
        PCGComponent = Owner->FindComponentByClass<UPCGComponent>();
    }
}

void URshipPCGBinding::BindToPulseReceiver()
{
    if (!Subsystem) return;

    URshipPulseReceiver* PulseReceiver = Subsystem->GetPulseReceiver();
    if (!PulseReceiver) return;

    PulseHandle = PulseReceiver->OnPulseReceived.AddLambda(
        [this](const FString& EmitterId, float Intensity, FLinearColor Color, TSharedPtr<FJsonObject> Data)
        {
            OnPulseReceived(EmitterId, Data);
        }
    );
}

void URshipPCGBinding::UnbindFromPulseReceiver()
{
    if (!Subsystem) return;

    URshipPulseReceiver* PulseReceiver = Subsystem->GetPulseReceiver();
    if (PulseReceiver && PulseHandle.IsValid())
    {
        PulseReceiver->OnPulseReceived.Remove(PulseHandle);
        PulseHandle.Reset();
    }
}

void URshipPCGBinding::OnPulseReceived(const FString& EmitterId, TSharedPtr<FJsonObject> Data)
{
    if (!Data.IsValid()) return;

    // Update all binding types
    UpdateScalarBindings(Data, EmitterId, 0.0f); // DeltaTime handled in tick for smoothing
    UpdateVectorBindings(Data, EmitterId, 0.0f);
    UpdateColorBindings(Data, EmitterId, 0.0f);
    UpdateSeedBindings(Data, EmitterId);
}

void URshipPCGBinding::UpdateScalarBindings(TSharedPtr<FJsonObject> Data, const FString& EmitterId, float DeltaTime)
{
    for (FRshipPCGParameterBinding& Binding : ScalarBindings)
    {
        if (!Binding.bEnabled) continue;
        if (!MatchesEmitterId(EmitterId, Binding.EmitterId)) continue;

        float RawValue = ExtractFloatValue(Data, Binding.PulseField);
        float ProcessedValue = ProcessScalarValue(Binding, RawValue);

        // Check if value changed significantly
        float Delta = FMath::Abs(ProcessedValue - Binding.TargetValue);
        if (Delta > Binding.ChangeThreshold)
        {
            Binding.TargetValue = ProcessedValue;
            Binding.bDirty = true;
            bAnyDirty = true;
            TimeSinceLastDirty = 0.0f;
        }

        Binding.LastRawValue = RawValue;
    }
}

void URshipPCGBinding::UpdateVectorBindings(TSharedPtr<FJsonObject> Data, const FString& EmitterId, float DeltaTime)
{
    for (FRshipPCGVectorBinding& Binding : VectorBindings)
    {
        if (!Binding.bEnabled) continue;
        if (!MatchesEmitterId(EmitterId, Binding.EmitterId)) continue;

        FVector RawValue = ExtractVectorValue(Data, Binding.VectorFieldPrefix);

        // Apply scaling and offset
        FVector ProcessedValue;
        ProcessedValue.X = RawValue.X * Binding.ScaleX + Binding.Offset.X;
        ProcessedValue.Y = RawValue.Y * Binding.ScaleY + Binding.Offset.Y;
        ProcessedValue.Z = RawValue.Z * Binding.ScaleZ + Binding.Offset.Z;

        // Check if value changed significantly
        float Distance = FVector::Dist(ProcessedValue, Binding.TargetValue);
        if (Distance > Binding.ChangeThreshold)
        {
            Binding.TargetValue = ProcessedValue;
            Binding.bDirty = true;
            bAnyDirty = true;
            TimeSinceLastDirty = 0.0f;
        }

        Binding.LastRawValue = RawValue;
    }
}

void URshipPCGBinding::UpdateColorBindings(TSharedPtr<FJsonObject> Data, const FString& EmitterId, float DeltaTime)
{
    for (FRshipPCGColorBinding& Binding : ColorBindings)
    {
        if (!Binding.bEnabled) continue;
        if (!MatchesEmitterId(EmitterId, Binding.EmitterId)) continue;

        FLinearColor RawColor = ExtractColorValue(Data, Binding.ColorField);

        // Apply intensity multiplier if specified
        if (!Binding.IntensityField.IsEmpty())
        {
            float Intensity = ExtractFloatValue(Data, Binding.IntensityField, 1.0f);
            RawColor *= Intensity;
        }

        // Apply color multiplier
        FLinearColor ProcessedColor = RawColor * Binding.ColorMultiplier;

        // Clamp if not HDR
        if (!Binding.bAllowHDR)
        {
            ProcessedColor.R = FMath::Clamp(ProcessedColor.R, 0.0f, 1.0f);
            ProcessedColor.G = FMath::Clamp(ProcessedColor.G, 0.0f, 1.0f);
            ProcessedColor.B = FMath::Clamp(ProcessedColor.B, 0.0f, 1.0f);
            ProcessedColor.A = FMath::Clamp(ProcessedColor.A, 0.0f, 1.0f);
        }

        // Check if value changed significantly
        float ColorDist = FMath::Sqrt(
            FMath::Square(ProcessedColor.R - Binding.TargetValue.R) +
            FMath::Square(ProcessedColor.G - Binding.TargetValue.G) +
            FMath::Square(ProcessedColor.B - Binding.TargetValue.B)
        );

        if (ColorDist > Binding.ChangeThreshold)
        {
            Binding.TargetValue = ProcessedColor;
            Binding.bDirty = true;
            bAnyDirty = true;
            TimeSinceLastDirty = 0.0f;
        }

        Binding.LastRawValue = RawColor;
    }
}

void URshipPCGBinding::UpdateSeedBindings(TSharedPtr<FJsonObject> Data, const FString& EmitterId)
{
    for (FRshipPCGSeedBinding& Binding : SeedBindings)
    {
        if (!Binding.bEnabled) continue;
        if (!MatchesEmitterId(EmitterId, Binding.EmitterId)) continue;

        float RawValue = ExtractFloatValue(Data, Binding.PulseField);

        // Normalize input to 0-1 range
        float Normalized = 0.0f;
        if (Binding.InputMax != Binding.InputMin)
        {
            Normalized = (RawValue - Binding.InputMin) / (Binding.InputMax - Binding.InputMin);
            Normalized = FMath::Clamp(Normalized, 0.0f, 1.0f);
        }

        // Map to seed range
        int32 NewSeed = FMath::RoundToInt(FMath::Lerp((float)Binding.SeedMin, (float)Binding.SeedMax, Normalized));

        if (NewSeed != Binding.CurrentSeed)
        {
            Binding.CurrentSeed = NewSeed;
            Binding.bDirty = true;
            bAnyDirty = true;
            TimeSinceLastDirty = 0.0f;
        }
    }
}

void URshipPCGBinding::UpdateSmoothing(float DeltaTime)
{
    // Scalar bindings smoothing
    for (FRshipPCGParameterBinding& Binding : ScalarBindings)
    {
        if (Binding.Smoothing > 0.0f)
        {
            float Alpha = FMath::Exp(-DeltaTime / (Binding.Smoothing * 0.1f));
            Binding.SmoothedValue = FMath::Lerp(Binding.TargetValue, Binding.SmoothedValue, Alpha);
        }
        else
        {
            Binding.SmoothedValue = Binding.TargetValue;
        }
    }

    // Vector bindings smoothing
    for (FRshipPCGVectorBinding& Binding : VectorBindings)
    {
        if (Binding.Smoothing > 0.0f)
        {
            float Alpha = FMath::Exp(-DeltaTime / (Binding.Smoothing * 0.1f));
            Binding.SmoothedValue = FMath::Lerp(Binding.TargetValue, Binding.SmoothedValue, Alpha);
        }
        else
        {
            Binding.SmoothedValue = Binding.TargetValue;
        }
    }

    // Color bindings smoothing
    for (FRshipPCGColorBinding& Binding : ColorBindings)
    {
        if (Binding.Smoothing > 0.0f)
        {
            float Alpha = FMath::Exp(-DeltaTime / (Binding.Smoothing * 0.1f));
            Binding.SmoothedValue = FMath::Lerp(Binding.TargetValue, Binding.SmoothedValue, Alpha);
        }
        else
        {
            Binding.SmoothedValue = Binding.TargetValue;
        }
    }
}

void URshipPCGBinding::CheckAndTriggerRegen(float DeltaTime)
{
    TimeSinceLastRegen += DeltaTime;
    TimeSinceLastDirty += DeltaTime;

    if (bRegenPaused)
    {
        return;
    }

    if (!bAnyDirty && !bHasDirectOverrides && !bAllowEmptyRegen)
    {
        return;
    }

    float MinRegenInterval = 1.0f / FMath::Max(MaxRegensPerSecond, 0.1f);

    switch (RegenStrategy)
    {
        case ERshipPCGRegenStrategy::Immediate:
            if (TimeSinceLastRegen >= MinRegenInterval)
            {
                DoRegenerate();
            }
            break;

        case ERshipPCGRegenStrategy::Debounced:
            // Wait for quiet period after last change
            if (TimeSinceLastDirty >= DebounceTime && TimeSinceLastRegen >= MinRegenInterval)
            {
                DoRegenerate();
            }
            break;

        case ERshipPCGRegenStrategy::Threshold:
            // Threshold is already handled in binding update - only marks dirty on significant change
            if (TimeSinceLastRegen >= MinRegenInterval)
            {
                DoRegenerate();
            }
            break;

        case ERshipPCGRegenStrategy::Manual:
            // Only regenerate via ForceRegenerate()
            break;
    }
}

void URshipPCGBinding::DoRegenerate()
{
    if (!PCGComponent)
    {
        OnRegenSkipped.Broadcast(TEXT("No PCG Component"));
        return;
    }

    // Apply all current parameter values to graph
    ApplyParametersToGraph();

    // Trigger regeneration
    if (bCleanupBeforeRegen)
    {
        PCGComponent->Cleanup();
    }

    PCGComponent->Generate();

    // Reset state
    TimeSinceLastRegen = 0.0f;
    bAnyDirty = false;
    bHasDirectOverrides = false;

    // Clear dirty flags
    for (FRshipPCGParameterBinding& Binding : ScalarBindings)
    {
        Binding.bDirty = false;
    }
    for (FRshipPCGVectorBinding& Binding : VectorBindings)
    {
        Binding.bDirty = false;
    }
    for (FRshipPCGColorBinding& Binding : ColorBindings)
    {
        Binding.bDirty = false;
    }
    for (FRshipPCGSeedBinding& Binding : SeedBindings)
    {
        Binding.LastSeed = Binding.CurrentSeed;
        Binding.bDirty = false;
    }

    // Clear direct overrides
    DirectScalarValues.Empty();
    DirectVectorValues.Empty();
    DirectColorValues.Empty();
    DirectSeedValues.Empty();

    RegenCount++;
    OnRegenerated.Broadcast();
}

void URshipPCGBinding::ApplyParametersToGraph()
{
    if (!PCGComponent) return;

    UPCGGraph* Graph = PCGComponent->GetGraph();
    if (!Graph) return;

    // Apply scalar bindings
    for (const FRshipPCGParameterBinding& Binding : ScalarBindings)
    {
        if (Binding.bEnabled)
        {
            // PCG uses FPCGOverrideInstancedPropertyBag for parameters
            // The actual API varies by UE version - using the common approach
            FName ParamName = Binding.ParameterName;
            float Value = Binding.SmoothedValue;

            // Set as instance parameter override
            FPCGOverrideInstancedPropertyBag& Overrides = PCGComponent->GetMutableGraphParametersOverrides();
            // Note: The exact method depends on PCG version
            // This represents the pattern - implementation may need adjustment
        }
    }

    // Apply vector bindings
    for (const FRshipPCGVectorBinding& Binding : VectorBindings)
    {
        if (Binding.bEnabled)
        {
            FName ParamName = Binding.ParameterName;
            FVector Value = Binding.SmoothedValue;
            OnVectorParameterUpdated.Broadcast(ParamName, Value);
        }
    }

    // Apply color bindings
    for (const FRshipPCGColorBinding& Binding : ColorBindings)
    {
        if (Binding.bEnabled)
        {
            FName ParamName = Binding.ParameterName;
            FLinearColor Value = Binding.SmoothedValue;
            OnColorParameterUpdated.Broadcast(ParamName, Value);
        }
    }

    // Apply seed bindings
    for (const FRshipPCGSeedBinding& Binding : SeedBindings)
    {
        if (Binding.bEnabled)
        {
            FName ParamName = Binding.ParameterName;
            int32 Value = Binding.CurrentSeed;
            OnScalarParameterUpdated.Broadcast(ParamName, (float)Value);
        }
    }

    // Apply direct overrides
    for (const auto& Pair : DirectScalarValues)
    {
        OnScalarParameterUpdated.Broadcast(Pair.Key, Pair.Value);
    }
    for (const auto& Pair : DirectVectorValues)
    {
        OnVectorParameterUpdated.Broadcast(Pair.Key, Pair.Value);
    }
    for (const auto& Pair : DirectColorValues)
    {
        OnColorParameterUpdated.Broadcast(Pair.Key, Pair.Value);
    }
    for (const auto& Pair : DirectSeedValues)
    {
        OnScalarParameterUpdated.Broadcast(Pair.Key, (float)Pair.Value);
    }

    // Notify PCG that properties changed
    PCGComponent->NotifyPropertiesChangedFromBlueprint();
}

float URshipPCGBinding::ProcessScalarValue(FRshipPCGParameterBinding& Binding, float RawValue)
{
    float Result = RawValue;

    switch (Binding.Mode)
    {
        case ERshipPCGBindingMode::Direct:
            Result = RawValue;
            break;

        case ERshipPCGBindingMode::Normalized:
            if (Binding.InputMax != Binding.InputMin)
            {
                Result = (RawValue - Binding.InputMin) / (Binding.InputMax - Binding.InputMin);
                Result = FMath::Clamp(Result, 0.0f, 1.0f);
            }
            break;

        case ERshipPCGBindingMode::Scaled:
            Result = RawValue * Binding.ScaleFactor;
            break;

        case ERshipPCGBindingMode::Mapped:
            if (Binding.InputMax != Binding.InputMin)
            {
                float Normalized = (RawValue - Binding.InputMin) / (Binding.InputMax - Binding.InputMin);
                Normalized = FMath::Clamp(Normalized, 0.0f, 1.0f);
                Result = FMath::Lerp(Binding.OutputMin, Binding.OutputMax, Normalized);
            }
            break;

        case ERshipPCGBindingMode::Curve:
            if (Binding.ResponseCurve)
            {
                // Normalize input to 0-1 for curve lookup
                float Normalized = 0.0f;
                if (Binding.InputMax != Binding.InputMin)
                {
                    Normalized = (RawValue - Binding.InputMin) / (Binding.InputMax - Binding.InputMin);
                    Normalized = FMath::Clamp(Normalized, 0.0f, 1.0f);
                }
                Result = Binding.ResponseCurve->GetFloatValue(Normalized);
            }
            break;

        case ERshipPCGBindingMode::Trigger:
            Result = (RawValue >= Binding.TriggerThreshold) ? Binding.OnValue : Binding.OffValue;
            break;
    }

    // Apply offset
    Result += Binding.Offset;

    return Result;
}

float URshipPCGBinding::ExtractFloatValue(TSharedPtr<FJsonObject> Data, const FString& FieldPath, float Default)
{
    if (!Data.IsValid()) return Default;

    // Handle nested paths (e.g., "values.intensity")
    TArray<FString> PathParts;
    FieldPath.ParseIntoArray(PathParts, TEXT("."));

    TSharedPtr<FJsonObject> CurrentObj = Data;

    for (int32 i = 0; i < PathParts.Num() - 1; ++i)
    {
        const TSharedPtr<FJsonObject>* NestedObj;
        if (CurrentObj->TryGetObjectField(PathParts[i], NestedObj))
        {
            CurrentObj = *NestedObj;
        }
        else
        {
            return Default;
        }
    }

    double Value;
    if (CurrentObj->TryGetNumberField(PathParts.Last(), Value))
    {
        return (float)Value;
    }

    return Default;
}

FVector URshipPCGBinding::ExtractVectorValue(TSharedPtr<FJsonObject> Data, const FString& Prefix)
{
    FVector Result = FVector::ZeroVector;

    Result.X = ExtractFloatValue(Data, Prefix + TEXT(".x"), 0.0f);
    Result.Y = ExtractFloatValue(Data, Prefix + TEXT(".y"), 0.0f);
    Result.Z = ExtractFloatValue(Data, Prefix + TEXT(".z"), 0.0f);

    return Result;
}

FLinearColor URshipPCGBinding::ExtractColorValue(TSharedPtr<FJsonObject> Data, const FString& Prefix)
{
    FLinearColor Result = FLinearColor::Black;

    Result.R = ExtractFloatValue(Data, Prefix + TEXT(".r"), 0.0f);
    Result.G = ExtractFloatValue(Data, Prefix + TEXT(".g"), 0.0f);
    Result.B = ExtractFloatValue(Data, Prefix + TEXT(".b"), 0.0f);
    Result.A = ExtractFloatValue(Data, Prefix + TEXT(".a"), 1.0f);

    return Result;
}

bool URshipPCGBinding::MatchesEmitterId(const FString& IncomingId, const FString& Pattern) const
{
    if (Pattern.IsEmpty()) return true;
    if (Pattern == TEXT("*")) return true;

    // Support wildcards
    if (Pattern.Contains(TEXT("*")))
    {
        // Simple prefix match for "foo*"
        if (Pattern.EndsWith(TEXT("*")))
        {
            FString Prefix = Pattern.LeftChop(1);
            return IncomingId.StartsWith(Prefix);
        }
        // Simple suffix match for "*foo"
        if (Pattern.StartsWith(TEXT("*")))
        {
            FString Suffix = Pattern.RightChop(1);
            return IncomingId.EndsWith(Suffix);
        }
    }

    // Exact match
    return IncomingId == Pattern;
}

// ========================================================================
// BINDING MANAGEMENT
// ========================================================================

void URshipPCGBinding::AddScalarBinding(const FRshipPCGParameterBinding& Binding)
{
    ScalarBindings.Add(Binding);
}

void URshipPCGBinding::AddVectorBinding(const FRshipPCGVectorBinding& Binding)
{
    VectorBindings.Add(Binding);
}

void URshipPCGBinding::AddColorBinding(const FRshipPCGColorBinding& Binding)
{
    ColorBindings.Add(Binding);
}

void URshipPCGBinding::AddSeedBinding(const FRshipPCGSeedBinding& Binding)
{
    SeedBindings.Add(Binding);
}

void URshipPCGBinding::RemoveBinding(FName ParameterName)
{
    ScalarBindings.RemoveAll([ParameterName](const FRshipPCGParameterBinding& B) {
        return B.ParameterName == ParameterName;
    });
    VectorBindings.RemoveAll([ParameterName](const FRshipPCGVectorBinding& B) {
        return B.ParameterName == ParameterName;
    });
    ColorBindings.RemoveAll([ParameterName](const FRshipPCGColorBinding& B) {
        return B.ParameterName == ParameterName;
    });
    SeedBindings.RemoveAll([ParameterName](const FRshipPCGSeedBinding& B) {
        return B.ParameterName == ParameterName;
    });
}

void URshipPCGBinding::ClearAllBindings()
{
    ScalarBindings.Empty();
    VectorBindings.Empty();
    ColorBindings.Empty();
    SeedBindings.Empty();
    bAnyDirty = false;
}

void URshipPCGBinding::SetAllBindingsEnabled(bool bEnabled)
{
    for (FRshipPCGParameterBinding& Binding : ScalarBindings)
    {
        Binding.bEnabled = bEnabled;
    }
    for (FRshipPCGVectorBinding& Binding : VectorBindings)
    {
        Binding.bEnabled = bEnabled;
    }
    for (FRshipPCGColorBinding& Binding : ColorBindings)
    {
        Binding.bEnabled = bEnabled;
    }
    for (FRshipPCGSeedBinding& Binding : SeedBindings)
    {
        Binding.bEnabled = bEnabled;
    }
}

// ========================================================================
// RUNTIME CONTROL
// ========================================================================

void URshipPCGBinding::ForceRegenerate()
{
    DoRegenerate();
}

void URshipPCGBinding::MarkAllDirty()
{
    for (FRshipPCGParameterBinding& Binding : ScalarBindings)
    {
        Binding.bDirty = true;
    }
    for (FRshipPCGVectorBinding& Binding : VectorBindings)
    {
        Binding.bDirty = true;
    }
    for (FRshipPCGColorBinding& Binding : ColorBindings)
    {
        Binding.bDirty = true;
    }
    for (FRshipPCGSeedBinding& Binding : SeedBindings)
    {
        Binding.bDirty = true;
    }
    bAnyDirty = true;
    TimeSinceLastDirty = 0.0f;
}

void URshipPCGBinding::SetRegenerationPaused(bool bPaused)
{
    bRegenPaused = bPaused;
}

void URshipPCGBinding::SetScalarParameter(FName Name, float Value)
{
    DirectScalarValues.Add(Name, Value);
    bHasDirectOverrides = true;
    bAnyDirty = true;
    TimeSinceLastDirty = 0.0f;
    OnScalarParameterUpdated.Broadcast(Name, Value);
}

void URshipPCGBinding::SetVectorParameter(FName Name, FVector Value)
{
    DirectVectorValues.Add(Name, Value);
    bHasDirectOverrides = true;
    bAnyDirty = true;
    TimeSinceLastDirty = 0.0f;
    OnVectorParameterUpdated.Broadcast(Name, Value);
}

void URshipPCGBinding::SetColorParameter(FName Name, FLinearColor Value)
{
    DirectColorValues.Add(Name, Value);
    bHasDirectOverrides = true;
    bAnyDirty = true;
    TimeSinceLastDirty = 0.0f;
    OnColorParameterUpdated.Broadcast(Name, Value);
}

void URshipPCGBinding::SetSeedParameter(FName Name, int32 Value)
{
    DirectSeedValues.Add(Name, Value);
    bHasDirectOverrides = true;
    bAnyDirty = true;
    TimeSinceLastDirty = 0.0f;
    OnScalarParameterUpdated.Broadcast(Name, (float)Value);
}

// ========================================================================
// DISCOVERY
// ========================================================================

TArray<FName> URshipPCGBinding::GetAvailableParameters() const
{
    TArray<FName> Parameters;

    if (!PCGComponent) return Parameters;

    UPCGGraph* Graph = PCGComponent->GetGraph();
    if (!Graph) return Parameters;

    // Get graph input parameters
    // Note: PCG parameter access varies by engine version
    // This provides the general pattern

    return Parameters;
}

bool URshipPCGBinding::HasParameter(FName ParameterName) const
{
    TArray<FName> Params = GetAvailableParameters();
    return Params.Contains(ParameterName);
}

// ============================================================================
// URshipPCGManager
// ============================================================================

void URshipPCGManager::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;
    RegisteredBindings.Empty();
    TotalRegenCount = 0;
}

void URshipPCGManager::Shutdown()
{
    RegisteredBindings.Empty();
    Subsystem = nullptr;
}

void URshipPCGManager::Tick(float DeltaTime)
{
    // Track regenerations per frame for budget
    RegensThisFrame = 0;

    // Accumulate regen budget
    RegenBudget = FMath::Min(RegenBudget + GlobalMaxRegensPerSecond * DeltaTime, GlobalMaxRegensPerSecond);
}

void URshipPCGManager::RegisterBinding(URshipPCGBinding* Binding)
{
    if (Binding && !RegisteredBindings.Contains(Binding))
    {
        RegisteredBindings.Add(Binding);
    }
}

void URshipPCGManager::UnregisterBinding(URshipPCGBinding* Binding)
{
    RegisteredBindings.Remove(Binding);
}

void URshipPCGManager::PauseAllRegeneration()
{
    for (URshipPCGBinding* Binding : RegisteredBindings)
    {
        if (Binding)
        {
            Binding->SetRegenerationPaused(true);
        }
    }
}

void URshipPCGManager::ResumeAllRegeneration()
{
    for (URshipPCGBinding* Binding : RegisteredBindings)
    {
        if (Binding)
        {
            Binding->SetRegenerationPaused(false);
        }
    }
}

void URshipPCGManager::ForceRegenerateAll()
{
    for (URshipPCGBinding* Binding : RegisteredBindings)
    {
        if (Binding)
        {
            Binding->ForceRegenerate();
            TotalRegenCount++;
            RegensThisFrame++;
        }
    }
}

void URshipPCGManager::MarkAllDirty()
{
    for (URshipPCGBinding* Binding : RegisteredBindings)
    {
        if (Binding)
        {
            Binding->MarkAllDirty();
        }
    }
}

void URshipPCGManager::SetGlobalMaxRegensPerSecond(float MaxRegen)
{
    GlobalMaxRegensPerSecond = FMath::Clamp(MaxRegen, 0.1f, 120.0f);
}
