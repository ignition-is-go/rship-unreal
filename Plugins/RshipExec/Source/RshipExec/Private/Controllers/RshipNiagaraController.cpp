// Rship Niagara VFX Controller Implementation

#include "Controllers/RshipNiagaraController.h"
#include "Logs.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// ============================================================================
// NIAGARA CONTROLLER COMPONENT
// ============================================================================

URshipNiagaraController::URshipNiagaraController()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.033f; // ~30Hz
}

void URshipNiagaraController::RegisterOrRefreshTarget()
{
    FRshipTargetProxy Target = ResolveChildTarget(ChildTargetSuffix, TEXT("niagara"));
    if (!Target.IsValid())
    {
        return;
    }

    Target
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetFloatParameter), TEXT("SetFloatParameter"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetVectorParameter), TEXT("SetVectorParameter"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetColorParameter), TEXT("SetColorParameter"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetIntParameter), TEXT("SetIntParameter"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetBoolParameter), TEXT("SetBoolParameter"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetSpawnRate), TEXT("SetSpawnRate"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetSpawnRateAbsolute), TEXT("SetSpawnRateAbsolute"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, TriggerBurst), TEXT("TriggerBurst"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetBurstCount), TEXT("SetBurstCount"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetLifetime), TEXT("SetLifetime"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetSize), TEXT("SetSize"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetSizeXYZ), TEXT("SetSizeXYZ"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetVelocity), TEXT("SetVelocity"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetVelocityXYZ), TEXT("SetVelocityXYZ"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetMass), TEXT("SetMass"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetDrag), TEXT("SetDrag"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetGravity), TEXT("SetGravity"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetColor), TEXT("SetColor"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetColorWithAlpha), TEXT("SetColorWithAlpha"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetEmissive), TEXT("SetEmissive"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetEmissiveColor), TEXT("SetEmissiveColor"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetOpacity), TEXT("SetOpacity"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetSpriteRotation), TEXT("SetSpriteRotation"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetSpriteSize), TEXT("SetSpriteSize"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, ActivateSystem), TEXT("Activate"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, DeactivateSystem), TEXT("Deactivate"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, Reset), TEXT("Reset"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, Pause), TEXT("Pause"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, Resume), TEXT("Resume"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetAge), TEXT("SetAge"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetGlobalIntensity), TEXT("SetGlobalIntensity"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetLocation), TEXT("SetLocation"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetRotation), TEXT("SetRotation"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetScale), TEXT("SetScale"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipNiagaraController, SetScaleXYZ), TEXT("SetScaleXYZ"))
        .AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipNiagaraController, OnActiveChanged), TEXT("onActiveChanged"))
        .AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipNiagaraController, OnParticleCountChanged), TEXT("onParticleCountChanged"))
        .AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipNiagaraController, OnAgeChanged), TEXT("onAgeChanged"))
        .AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipNiagaraController, OnSpawnRateChanged), TEXT("onSpawnRateChanged"))
        .AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipNiagaraController, OnLifetimeChanged), TEXT("onLifetimeChanged"))
        .AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipNiagaraController, OnSizeChanged), TEXT("onSizeChanged"))
        .AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipNiagaraController, OnVelocityChanged), TEXT("onVelocityChanged"))
        .AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipNiagaraController, OnColorChanged), TEXT("onColorChanged"))
        .AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipNiagaraController, OnEmissiveChanged), TEXT("onEmissiveChanged"))
        .AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipNiagaraController, OnGlobalIntensityChanged), TEXT("onGlobalIntensityChanged"))
        .AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipNiagaraController, OnLocationChanged), TEXT("onLocationChanged"))
        .AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipNiagaraController, OnRotationChanged), TEXT("onRotationChanged"));
}

void URshipNiagaraController::BeginPlay()
{
    Super::BeginPlay();

    // Auto-find Niagara component if not set
    if (!NiagaraComponent)
    {
        NiagaraComponent = GetOwner()->FindComponentByClass<UNiagaraComponent>();
    }

    if (!NiagaraComponent)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("RshipNiagaraController: No Niagara component found on %s"),
            *GetOwner()->GetName());
        return;
    }

    PublishInterval = 1.0 / FMath::Max(1, PublishRateHz);
    SetComponentTickEnabled(true);

    UE_LOG(LogRshipExec, Log, TEXT("RshipNiagaraController: Initialized on %s"), *GetOwner()->GetName());

    // Registration is handled by URshipControllerComponent::OnRegister through RegisterOrRefreshTarget.
}

void URshipNiagaraController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
}

void URshipNiagaraController::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!NiagaraComponent) return;

    double CurrentTime = FPlatformTime::Seconds();
    if (CurrentTime - LastPublishTime >= PublishInterval)
    {
        ReadAndPublishState();
        LastPublishTime = CurrentTime;
    }
}

void URshipNiagaraController::SetColorValue(FName ParameterName, FLinearColor Color)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableLinearColor(ParameterName, Color);
    }
}

// ============================================================================
//  ACTIONS - Generic Parameter Control
// ============================================================================

void URshipNiagaraController::SetFloatParameter(FName ParameterName, float Value)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableFloat(ParameterName, Value * GlobalIntensityMultiplier);
    }
}

void URshipNiagaraController::SetVectorParameter(FName ParameterName, float X, float Y, float Z)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableVec3(ParameterName, FVector(X, Y, Z));
    }
}

void URshipNiagaraController::SetColorParameter(FName ParameterName, float R, float G, float B, float A)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableLinearColor(ParameterName, FLinearColor(R, G, B, A));
    }
    LastColor = FLinearColor(R, G, B, A);
    OnColorChanged.Broadcast(R, G, B);
}

void URshipNiagaraController::SetIntParameter(FName ParameterName, int32 Value)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableInt(ParameterName, Value);
    }
}

void URshipNiagaraController::SetBoolParameter(FName ParameterName, bool Value)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableBool(ParameterName, Value);
    }
}

// ============================================================================
//  ACTIONS - Spawn Control
// ============================================================================

void URshipNiagaraController::SetSpawnRate(float Rate)
{
    LastSpawnRate = FMath::Max(0.0f, Rate);
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableFloat(TEXT("SpawnRate"), LastSpawnRate * GlobalIntensityMultiplier);
        NiagaraComponent->SetVariableFloat(TEXT("User.SpawnRate"), LastSpawnRate * GlobalIntensityMultiplier);
    }
    OnSpawnRateChanged.Broadcast(LastSpawnRate);
}

void URshipNiagaraController::SetSpawnRateAbsolute(float ParticlesPerSecond)
{
    SetSpawnRate(ParticlesPerSecond);
}

void URshipNiagaraController::TriggerBurst(int32 Count)
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

void URshipNiagaraController::SetBurstCount(int32 Count)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableInt(TEXT("BurstCount"), Count);
        NiagaraComponent->SetVariableInt(TEXT("User.BurstCount"), Count);
    }
}

// ============================================================================
//  ACTIONS - Particle Properties
// ============================================================================

void URshipNiagaraController::SetLifetime(float Lifetime)
{
    LastLifetime = FMath::Max(0.001f, Lifetime);
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableFloat(TEXT("Lifetime"), LastLifetime);
        NiagaraComponent->SetVariableFloat(TEXT("User.Lifetime"), LastLifetime);
        NiagaraComponent->SetVariableFloat(TEXT("LifetimeMultiplier"), LastLifetime);
    }
    OnLifetimeChanged.Broadcast(LastLifetime);
}

void URshipNiagaraController::SetSize(float Size)
{
    LastSize = FMath::Max(0.0f, Size);
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableFloat(TEXT("Size"), LastSize * GlobalIntensityMultiplier);
        NiagaraComponent->SetVariableFloat(TEXT("User.Size"), LastSize * GlobalIntensityMultiplier);
        NiagaraComponent->SetVariableFloat(TEXT("SizeMultiplier"), LastSize * GlobalIntensityMultiplier);
        NiagaraComponent->SetVariableFloat(TEXT("Scale"), LastSize * GlobalIntensityMultiplier);
    }
    OnSizeChanged.Broadcast(LastSize);
}

void URshipNiagaraController::SetSizeXYZ(float X, float Y, float Z)
{
    if (NiagaraComponent)
    {
        FVector SizeVec(X * GlobalIntensityMultiplier, Y * GlobalIntensityMultiplier, Z * GlobalIntensityMultiplier);
        NiagaraComponent->SetVariableVec3(TEXT("SizeXYZ"), SizeVec);
        NiagaraComponent->SetVariableVec3(TEXT("User.SizeXYZ"), SizeVec);
        NiagaraComponent->SetVariableVec3(TEXT("ScaleXYZ"), SizeVec);
    }
    LastSize = (X + Y + Z) / 3.0f;
    OnSizeChanged.Broadcast(LastSize);
}

void URshipNiagaraController::SetVelocity(float Velocity)
{
    LastVelocity = Velocity;
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableFloat(TEXT("Velocity"), Velocity);
        NiagaraComponent->SetVariableFloat(TEXT("User.Velocity"), Velocity);
        NiagaraComponent->SetVariableFloat(TEXT("VelocityMultiplier"), Velocity);
        NiagaraComponent->SetVariableFloat(TEXT("Speed"), Velocity);
    }
    OnVelocityChanged.Broadcast(LastVelocity);
}

void URshipNiagaraController::SetVelocityXYZ(float X, float Y, float Z)
{
    FVector VelVec(X, Y, Z);
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableVec3(TEXT("VelocityXYZ"), VelVec);
        NiagaraComponent->SetVariableVec3(TEXT("User.VelocityXYZ"), VelVec);
        NiagaraComponent->SetVariableVec3(TEXT("VelocityDirection"), VelVec);
    }
    LastVelocity = VelVec.Size();
    OnVelocityChanged.Broadcast(LastVelocity);
}

void URshipNiagaraController::SetMass(float Mass)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableFloat(TEXT("Mass"), Mass);
        NiagaraComponent->SetVariableFloat(TEXT("User.Mass"), Mass);
    }
}

void URshipNiagaraController::SetDrag(float Drag)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableFloat(TEXT("Drag"), Drag);
        NiagaraComponent->SetVariableFloat(TEXT("User.Drag"), Drag);
        NiagaraComponent->SetVariableFloat(TEXT("DragCoefficient"), Drag);
    }
}

void URshipNiagaraController::SetGravity(float Gravity)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableFloat(TEXT("Gravity"), Gravity);
        NiagaraComponent->SetVariableFloat(TEXT("User.Gravity"), Gravity);
        NiagaraComponent->SetVariableFloat(TEXT("GravityMultiplier"), Gravity);
    }
}

// ============================================================================
//  ACTIONS - Visual Properties
// ============================================================================

void URshipNiagaraController::SetColor(float R, float G, float B)
{
    SetColorWithAlpha(R, G, B, 1.0f);
}

void URshipNiagaraController::SetColorWithAlpha(float R, float G, float B, float A)
{
    LastColor = FLinearColor(R, G, B, A);
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableLinearColor(TEXT("Color"), LastColor);
        NiagaraComponent->SetVariableLinearColor(TEXT("User.Color"), LastColor);
        NiagaraComponent->SetVariableLinearColor(TEXT("ParticleColor"), LastColor);
    }
    OnColorChanged.Broadcast(R, G, B);
}

void URshipNiagaraController::SetEmissive(float Intensity)
{
    LastEmissive = FMath::Max(0.0f, Intensity);
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableFloat(TEXT("Emissive"), LastEmissive * GlobalIntensityMultiplier);
        NiagaraComponent->SetVariableFloat(TEXT("User.Emissive"), LastEmissive * GlobalIntensityMultiplier);
        NiagaraComponent->SetVariableFloat(TEXT("EmissiveIntensity"), LastEmissive * GlobalIntensityMultiplier);
    }
    OnEmissiveChanged.Broadcast(LastEmissive);
}

void URshipNiagaraController::SetEmissiveColor(float R, float G, float B, float Intensity)
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
    OnEmissiveChanged.Broadcast(LastEmissive);
}

void URshipNiagaraController::SetOpacity(float Opacity)
{
    float ClampedOpacity = FMath::Clamp(Opacity, 0.0f, 1.0f);
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableFloat(TEXT("Opacity"), ClampedOpacity);
        NiagaraComponent->SetVariableFloat(TEXT("User.Opacity"), ClampedOpacity);
        NiagaraComponent->SetVariableFloat(TEXT("Alpha"), ClampedOpacity);
    }
}

void URshipNiagaraController::SetSpriteRotation(float Degrees)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableFloat(TEXT("SpriteRotation"), Degrees);
        NiagaraComponent->SetVariableFloat(TEXT("User.SpriteRotation"), Degrees);
        NiagaraComponent->SetVariableFloat(TEXT("Rotation"), Degrees);
    }
}

void URshipNiagaraController::SetSpriteSize(float Width, float Height)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetVariableVec2(TEXT("SpriteSize"), FVector2D(Width, Height));
        NiagaraComponent->SetVariableVec2(TEXT("User.SpriteSize"), FVector2D(Width, Height));
    }
}

// ============================================================================
//  ACTIONS - System Control
// ============================================================================

void URshipNiagaraController::ActivateSystem()
{
    if (NiagaraComponent)
    {
        NiagaraComponent->Activate(true);
        bLastActive = true;
        OnActiveChanged.Broadcast(true);
    }
}

void URshipNiagaraController::DeactivateSystem()
{
    if (NiagaraComponent)
    {
        NiagaraComponent->Deactivate();
        bLastActive = false;
        OnActiveChanged.Broadcast(false);
    }
}

void URshipNiagaraController::Reset()
{
    if (NiagaraComponent)
    {
        NiagaraComponent->ResetSystem();
        bLastActive = NiagaraComponent->IsActive();
        OnActiveChanged.Broadcast(bLastActive);
    }
}

void URshipNiagaraController::Pause()
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetPaused(true);
    }
}

void URshipNiagaraController::Resume()
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetPaused(false);
    }
}

void URshipNiagaraController::SetAge(float Age)
{
    if (NiagaraComponent)
    {
        NiagaraComponent->SetSeekDelta(Age);
        NiagaraComponent->SeekToDesiredAge(Age);
        OnAgeChanged.Broadcast(Age);
    }
}

void URshipNiagaraController::SetGlobalIntensity(float Intensity)
{
    GlobalIntensityMultiplier = FMath::Max(0.0f, Intensity);
    OnGlobalIntensityChanged.Broadcast(GlobalIntensityMultiplier);
}

// ============================================================================
//  ACTIONS - Transform
// ============================================================================

void URshipNiagaraController::SetLocation(float X, float Y, float Z)
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        FVector NewLocation(X, Y, Z);
        Owner->SetActorLocation(NewLocation);
        LastLocation = NewLocation;
        OnLocationChanged.Broadcast(X, Y, Z);
    }
}

void URshipNiagaraController::SetRotation(float Pitch, float Yaw, float Roll)
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        FRotator NewRotation(Pitch, Yaw, Roll);
        Owner->SetActorRotation(NewRotation);
        LastRotation = NewRotation;
        OnRotationChanged.Broadcast(Pitch, Yaw, Roll);
    }
}

void URshipNiagaraController::SetScale(float Scale)
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        Owner->SetActorScale3D(FVector(Scale, Scale, Scale));
    }
}

void URshipNiagaraController::SetScaleXYZ(float X, float Y, float Z)
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

void URshipNiagaraController::ForcePublish()
{
    ReadAndPublishState();
}

FString URshipNiagaraController::GetNiagaraStateJson() const
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

void URshipNiagaraController::ReadAndPublishState()
{
    if (!NiagaraComponent) return;

    // Check active state
    bool bCurrentActive = NiagaraComponent->IsActive();
    if (!bOnlyPublishOnChange || bCurrentActive != bLastActive)
    {
        bLastActive = bCurrentActive;
        OnActiveChanged.Broadcast(bCurrentActive);
    }

    // Get transform from owner
    AActor* Owner = GetOwner();
    if (Owner)
    {
        FVector CurrentLocation = Owner->GetActorLocation();
        if (!bOnlyPublishOnChange || !CurrentLocation.Equals(LastLocation, 0.1f))
        {
            LastLocation = CurrentLocation;
            OnLocationChanged.Broadcast(CurrentLocation.X, CurrentLocation.Y, CurrentLocation.Z);
        }

        FRotator CurrentRotation = Owner->GetActorRotation();
        if (!bOnlyPublishOnChange || !CurrentRotation.Equals(LastRotation, 0.1f))
        {
            LastRotation = CurrentRotation;
            OnRotationChanged.Broadcast(CurrentRotation.Pitch, CurrentRotation.Yaw, CurrentRotation.Roll);
        }
    }
}

bool URshipNiagaraController::HasValueChanged(float OldValue, float NewValue, float Threshold) const
{
    return FMath::Abs(OldValue - NewValue) > Threshold;
}

bool URshipNiagaraController::HasColorChanged(const FLinearColor& OldColor, const FLinearColor& NewColor, float Threshold) const
{
    return FMath::Abs(OldColor.R - NewColor.R) > Threshold ||
           FMath::Abs(OldColor.G - NewColor.G) > Threshold ||
           FMath::Abs(OldColor.B - NewColor.B) > Threshold ||
           FMath::Abs(OldColor.A - NewColor.A) > Threshold;
}
