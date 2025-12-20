// Rship Niagara VFX Binding Implementation

#include "RshipNiagaraBinding.h"
#include "RshipSubsystem.h"
#include "RshipPulseReceiver.h"
#include "Logs.h"
#include "Engine/Engine.h"
#include "NiagaraComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

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
// RS_ ACTIONS - Generic Parameter Control
// ============================================================================

void URshipNiagaraBinding::RS_SetFloatParameter(FName ParameterName, float Value)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableFloat(ParameterName, Value * GlobalIntensityMultiplier);
    }
}

void URshipNiagaraBinding::RS_SetVectorParameter(FName ParameterName, float X, float Y, float Z)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableVec3(ParameterName, FVector(X, Y, Z));
    }
}

void URshipNiagaraBinding::RS_SetColorParameter(FName ParameterName, float R, float G, float B, float A)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableLinearColor(ParameterName, FLinearColor(R, G, B, A));
    }
    LastColor = FLinearColor(R, G, B, A);
    RS_OnColorChanged.Broadcast(R, G, B);
}

void URshipNiagaraBinding::RS_SetIntParameter(FName ParameterName, int32 Value)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableInt(ParameterName, Value);
    }
}

void URshipNiagaraBinding::RS_SetBoolParameter(FName ParameterName, bool Value)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableBool(ParameterName, Value);
    }
}

// ============================================================================
// RS_ ACTIONS - Spawn Control
// ============================================================================

void URshipNiagaraBinding::RS_SetSpawnRate(float Rate)
{
    LastSpawnRate = FMath::Max(0.0f, Rate);
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableFloat(TEXT("SpawnRate"), LastSpawnRate * GlobalIntensityMultiplier);
        NiagaraComponent->SetVariableFloat(TEXT("User.SpawnRate"), LastSpawnRate * GlobalIntensityMultiplier);
    }
    RS_OnSpawnRateChanged.Broadcast(LastSpawnRate);
}

void URshipNiagaraBinding::RS_SetSpawnRateAbsolute(float ParticlesPerSecond)
{
    RS_SetSpawnRate(ParticlesPerSecond);
}

void URshipNiagaraBinding::RS_TriggerBurst(int32 Count)
{
    if (NiagaraComponent)
    {
        // Set burst count then trigger
        NiagaraComponent->SetVariableInt(TEXT("BurstCount"), Count);
        NiagaraComponent->SetVariableInt(TEXT("User.BurstCount"), Count);
        // Many Niagara systems use a trigger variable
        NiagaraComponent->SetVariableBool(TEXT("TriggerBurst"), true);
        NiagaraComponent->SetVariableBool(TEXT("User.TriggerBurst"), true);
    }
}

void URshipNiagaraBinding::RS_SetBurstCount(int32 Count)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableInt(TEXT("BurstCount"), Count);
        NiagaraComponent->SetVariableInt(TEXT("User.BurstCount"), Count);
    }
}

// ============================================================================
// RS_ ACTIONS - Particle Properties
// ============================================================================

void URshipNiagaraBinding::RS_SetLifetime(float Lifetime)
{
    LastLifetime = FMath::Max(0.001f, Lifetime);
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableFloat(TEXT("Lifetime"), LastLifetime);
        NiagaraComponent->SetVariableFloat(TEXT("User.Lifetime"), LastLifetime);
        NiagaraComponent->SetVariableFloat(TEXT("LifetimeMultiplier"), LastLifetime);
    }
    RS_OnLifetimeChanged.Broadcast(LastLifetime);
}

void URshipNiagaraBinding::RS_SetSize(float Size)
{
    LastSize = FMath::Max(0.0f, Size);
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableFloat(TEXT("Size"), LastSize * GlobalIntensityMultiplier);
        NiagaraComponent->SetVariableFloat(TEXT("User.Size"), LastSize * GlobalIntensityMultiplier);
        NiagaraComponent->SetVariableFloat(TEXT("SizeMultiplier"), LastSize * GlobalIntensityMultiplier);
        NiagaraComponent->SetVariableFloat(TEXT("Scale"), LastSize * GlobalIntensityMultiplier);
    }
    RS_OnSizeChanged.Broadcast(LastSize);
}

void URshipNiagaraBinding::RS_SetSizeXYZ(float X, float Y, float Z)
{
    if (NiagaraComponent)
    {
        FVector SizeVec(X * GlobalIntensityMultiplier, Y * GlobalIntensityMultiplier, Z * GlobalIntensityMultiplier);
        NiagaraComponent->SetVariableVec3(TEXT("SizeXYZ"), SizeVec);
        NiagaraComponent->SetVariableVec3(TEXT("User.SizeXYZ"), SizeVec);
        NiagaraComponent->SetVariableVec3(TEXT("ScaleXYZ"), SizeVec);
    }
    LastSize = (X + Y + Z) / 3.0f;
    RS_OnSizeChanged.Broadcast(LastSize);
}

void URshipNiagaraBinding::RS_SetVelocity(float Velocity)
{
    LastVelocity = Velocity;
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableFloat(TEXT("Velocity"), Velocity);
        NiagaraComponent->SetVariableFloat(TEXT("User.Velocity"), Velocity);
        NiagaraComponent->SetVariableFloat(TEXT("VelocityMultiplier"), Velocity);
        NiagaraComponent->SetVariableFloat(TEXT("Speed"), Velocity);
    }
    RS_OnVelocityChanged.Broadcast(LastVelocity);
}

void URshipNiagaraBinding::RS_SetVelocityXYZ(float X, float Y, float Z)
{
    FVector VelVec(X, Y, Z);
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableVec3(TEXT("VelocityXYZ"), VelVec);
        NiagaraComponent->SetVariableVec3(TEXT("User.VelocityXYZ"), VelVec);
        NiagaraComponent->SetVariableVec3(TEXT("VelocityDirection"), VelVec);
    }
    LastVelocity = VelVec.Size();
    RS_OnVelocityChanged.Broadcast(LastVelocity);
}

void URshipNiagaraBinding::RS_SetMass(float Mass)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableFloat(TEXT("Mass"), Mass);
        NiagaraComponent->SetVariableFloat(TEXT("User.Mass"), Mass);
    }
}

void URshipNiagaraBinding::RS_SetDrag(float Drag)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableFloat(TEXT("Drag"), Drag);
        NiagaraComponent->SetVariableFloat(TEXT("User.Drag"), Drag);
        NiagaraComponent->SetVariableFloat(TEXT("DragCoefficient"), Drag);
    }
}

void URshipNiagaraBinding::RS_SetGravity(float Gravity)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableFloat(TEXT("Gravity"), Gravity);
        NiagaraComponent->SetVariableFloat(TEXT("User.Gravity"), Gravity);
        NiagaraComponent->SetVariableFloat(TEXT("GravityMultiplier"), Gravity);
    }
}

// ============================================================================
// RS_ ACTIONS - Visual Properties
// ============================================================================

void URshipNiagaraBinding::RS_SetColor(float R, float G, float B)
{
    RS_SetColorWithAlpha(R, G, B, 1.0f);
}

void URshipNiagaraBinding::RS_SetColorWithAlpha(float R, float G, float B, float A)
{
    LastColor = FLinearColor(R, G, B, A);
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableLinearColor(TEXT("Color"), LastColor);
        NiagaraComponent->SetVariableLinearColor(TEXT("User.Color"), LastColor);
        NiagaraComponent->SetVariableLinearColor(TEXT("ParticleColor"), LastColor);
    }
    RS_OnColorChanged.Broadcast(R, G, B);
}

void URshipNiagaraBinding::RS_SetEmissive(float Intensity)
{
    LastEmissive = FMath::Max(0.0f, Intensity);
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableFloat(TEXT("Emissive"), LastEmissive * GlobalIntensityMultiplier);
        NiagaraComponent->SetVariableFloat(TEXT("User.Emissive"), LastEmissive * GlobalIntensityMultiplier);
        NiagaraComponent->SetVariableFloat(TEXT("EmissiveIntensity"), LastEmissive * GlobalIntensityMultiplier);
    }
    RS_OnEmissiveChanged.Broadcast(LastEmissive);
}

void URshipNiagaraBinding::RS_SetEmissiveColor(float R, float G, float B, float Intensity)
{
    FLinearColor EmissiveColor(R * Intensity * GlobalIntensityMultiplier,
                                G * Intensity * GlobalIntensityMultiplier,
                                B * Intensity * GlobalIntensityMultiplier);
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableLinearColor(TEXT("EmissiveColor"), EmissiveColor);
        NiagaraComponent->SetVariableLinearColor(TEXT("User.EmissiveColor"), EmissiveColor);
    }
    LastEmissive = Intensity;
    RS_OnEmissiveChanged.Broadcast(LastEmissive);
}

void URshipNiagaraBinding::RS_SetOpacity(float Opacity)
{
    float ClampedOpacity = FMath::Clamp(Opacity, 0.0f, 1.0f);
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableFloat(TEXT("Opacity"), ClampedOpacity);
        NiagaraComponent->SetVariableFloat(TEXT("User.Opacity"), ClampedOpacity);
        NiagaraComponent->SetVariableFloat(TEXT("Alpha"), ClampedOpacity);
    }
}

void URshipNiagaraBinding::RS_SetSpriteRotation(float Degrees)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableFloat(TEXT("SpriteRotation"), Degrees);
        NiagaraComponent->SetVariableFloat(TEXT("User.SpriteRotation"), Degrees);
        NiagaraComponent->SetVariableFloat(TEXT("Rotation"), Degrees);
    }
}

void URshipNiagaraBinding::RS_SetSpriteSize(float Width, float Height)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableVec2(TEXT("SpriteSize"), FVector2D(Width, Height));
        NiagaraComponent->SetVariableVec2(TEXT("User.SpriteSize"), FVector2D(Width, Height));
    }
}

// ============================================================================
// RS_ ACTIONS - System Control
// ============================================================================

void URshipNiagaraBinding::RS_Activate()
{
    if (NiagaraComponent)
    {
        NiagaraComponent->Activate(true);
        bLastActive = true;
        RS_OnActiveChanged.Broadcast(true);
    }
}

void URshipNiagaraBinding::RS_Deactivate()
{
    if (NiagaraComponent)
    {
        NiagaraComponent->Deactivate();
        bLastActive = false;
        RS_OnActiveChanged.Broadcast(false);
    }
}

void URshipNiagaraBinding::RS_Reset()
{
    if (NiagaraComponent)
    {
        NiagaraComponent->ResetSystem();
        bLastActive = NiagaraComponent->IsActive();
        RS_OnActiveChanged.Broadcast(bLastActive);
    }
}

void URshipNiagaraBinding::RS_Pause()
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetPaused(true);
    }
}

void URshipNiagaraBinding::RS_Resume()
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetPaused(false);
    }
}

void URshipNiagaraBinding::RS_SetAge(float Age)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetSeekDelta(Age);
        NiagaraComponent->SeekToDesiredAge(Age);
        RS_OnAgeChanged.Broadcast(Age);
    }
}

void URshipNiagaraBinding::RS_SetGlobalIntensity(float Intensity)
{
    GlobalIntensityMultiplier = FMath::Max(0.0f, Intensity);
    RS_OnGlobalIntensityChanged.Broadcast(GlobalIntensityMultiplier);
}

// ============================================================================
// RS_ ACTIONS - Transform
// ============================================================================

void URshipNiagaraBinding::RS_SetLocation(float X, float Y, float Z)
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        FVector NewLocation(X, Y, Z);
        Owner->SetActorLocation(NewLocation);
        LastLocation = NewLocation;
        RS_OnLocationChanged.Broadcast(X, Y, Z);
    }
}

void URshipNiagaraBinding::RS_SetRotation(float Pitch, float Yaw, float Roll)
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        FRotator NewRotation(Pitch, Yaw, Roll);
        Owner->SetActorRotation(NewRotation);
        LastRotation = NewRotation;
        RS_OnRotationChanged.Broadcast(Pitch, Yaw, Roll);
    }
}

void URshipNiagaraBinding::RS_SetScale(float Scale)
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        Owner->SetActorScale3D(FVector(Scale, Scale, Scale));
    }
}

void URshipNiagaraBinding::RS_SetScaleXYZ(float X, float Y, float Z)
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        Owner->SetActorScale3D(FVector(X, Y, Z));
    }
}

// ============================================================================
// State Publishing
// ============================================================================

void URshipNiagaraBinding::ForcePublish()
{
    ReadAndPublishState();
}

FString URshipNiagaraBinding::GetNiagaraStateJson() const
{
    TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();

    JsonObj->SetBoolField(TEXT("active"), NiagaraComponent ? NiagaraComponent->IsActive() : false);
    JsonObj->SetNumberField(TEXT("spawnRate"), LastSpawnRate);
    JsonObj->SetNumberField(TEXT("lifetime"), LastLifetime);
    JsonObj->SetNumberField(TEXT("size"), LastSize);
    JsonObj->SetNumberField(TEXT("velocity"), LastVelocity);
    JsonObj->SetNumberField(TEXT("emissive"), LastEmissive);
    JsonObj->SetNumberField(TEXT("globalIntensity"), GlobalIntensityMultiplier);

    TSharedPtr<FJsonObject> ColorObj = MakeShared<FJsonObject>();
    ColorObj->SetNumberField(TEXT("r"), LastColor.R);
    ColorObj->SetNumberField(TEXT("g"), LastColor.G);
    ColorObj->SetNumberField(TEXT("b"), LastColor.B);
    ColorObj->SetNumberField(TEXT("a"), LastColor.A);
    JsonObj->SetObjectField(TEXT("color"), ColorObj);

    TSharedPtr<FJsonObject> LocationObj = MakeShared<FJsonObject>();
    LocationObj->SetNumberField(TEXT("x"), LastLocation.X);
    LocationObj->SetNumberField(TEXT("y"), LastLocation.Y);
    LocationObj->SetNumberField(TEXT("z"), LastLocation.Z);
    JsonObj->SetObjectField(TEXT("location"), LocationObj);

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);
    return OutputString;
}

void URshipNiagaraBinding::ReadAndPublishState()
{
    if (!NiagaraComponent) return;

    // Check active state
    bool bCurrentActive = NiagaraComponent->IsActive();
    if (!bOnlyPublishOnChange || bCurrentActive != bLastActive)
    {
        bLastActive = bCurrentActive;
        RS_OnActiveChanged.Broadcast(bCurrentActive);
    }

    // Get transform from owner
    AActor* Owner = GetOwner();
    if (Owner)
    {
        FVector CurrentLocation = Owner->GetActorLocation();
        if (!bOnlyPublishOnChange || !CurrentLocation.Equals(LastLocation, 0.1f))
        {
            LastLocation = CurrentLocation;
            RS_OnLocationChanged.Broadcast(CurrentLocation.X, CurrentLocation.Y, CurrentLocation.Z);
        }

        FRotator CurrentRotation = Owner->GetActorRotation();
        if (!bOnlyPublishOnChange || !CurrentRotation.Equals(LastRotation, 0.1f))
        {
            LastRotation = CurrentRotation;
            RS_OnRotationChanged.Broadcast(CurrentRotation.Pitch, CurrentRotation.Yaw, CurrentRotation.Roll);
        }
    }
}

bool URshipNiagaraBinding::HasValueChanged(float OldValue, float NewValue, float Threshold) const
{
    return FMath::Abs(OldValue - NewValue) > Threshold;
}

bool URshipNiagaraBinding::HasColorChanged(const FLinearColor& OldColor, const FLinearColor& NewColor, float Threshold) const
{
    return FMath::Abs(OldColor.R - NewColor.R) > Threshold ||
           FMath::Abs(OldColor.G - NewColor.G) > Threshold ||
           FMath::Abs(OldColor.B - NewColor.B) > Threshold ||
           FMath::Abs(OldColor.A - NewColor.A) > Threshold;
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
