// Rship Material Binding Implementation

#include "RshipMaterialBinding.h"
#include "RshipSubsystem.h"
#include "RshipPulseReceiver.h"
#include "RshipActorRegistrationComponent.h"
#include "Logs.h"
#include "Engine/Engine.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// ============================================================================
// MATERIAL BINDING COMPONENT
// ============================================================================

URshipMaterialBinding::URshipMaterialBinding()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void URshipMaterialBinding::BeginPlay()
{
    Super::BeginPlay();

    if (GEngine)
    {
        Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
    }

    SetupMaterials();
    CacheDefaultValues();
    BindToPulseReceiver();

    // Set publish interval from rate
    PublishInterval = 1.0 / FMath::Max(1, PublishRateHz);

    // Enable tick if we have smoothed parameters or need to publish state
    bool bNeedsTick = true; // Always tick to publish state via emitters
    for (const FRshipMaterialScalarBinding& B : ScalarBindings)
    {
        if (B.Smoothing > 0.0f) bNeedsTick = true;
    }
    for (const FRshipMaterialVectorBinding& B : VectorBindings)
    {
        if (B.Smoothing > 0.0f) bNeedsTick = true;
    }

    if (bNeedsTick && bEnableTick)
    {
        SetComponentTickEnabled(true);
    }

    // Register with manager
    if (Subsystem)
    {
        URshipMaterialManager* Manager = Subsystem->GetMaterialManager();
        if (Manager)
        {
            Manager->RegisterBinding(this);
        }
    }

    // Reflection-based registration for this component is owned by URshipBPController.
}

void URshipMaterialBinding::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnbindFromPulseReceiver();

    // Unregister from manager
    if (Subsystem)
    {
        URshipMaterialManager* Manager = Subsystem->GetMaterialManager();
        if (Manager)
        {
            Manager->UnregisterBinding(this);
        }
    }

    DynamicMaterials.Empty();
    Subsystem = nullptr;

    Super::EndPlay(EndPlayReason);
}

void URshipMaterialBinding::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Smooth scalar parameters
    for (FRshipMaterialScalarBinding& Binding : ScalarBindings)
    {
        if (!Binding.bEnabled || Binding.Smoothing <= 0.0f) continue;

        float SmoothFactor = FMath::Pow(Binding.Smoothing, DeltaTime * 60.0f);
        Binding.CurrentValue = FMath::Lerp(Binding.TargetValue, Binding.CurrentValue, SmoothFactor);

        for (UMaterialInstanceDynamic* MID : DynamicMaterials)
        {
            if (MID)
            {
                MID->SetScalarParameterValue(Binding.ParameterName, Binding.CurrentValue);
            }
        }
    }

    // Smooth vector parameters
    for (FRshipMaterialVectorBinding& Binding : VectorBindings)
    {
        if (!Binding.bEnabled || Binding.Smoothing <= 0.0f) continue;

        float SmoothFactor = FMath::Pow(Binding.Smoothing, DeltaTime * 60.0f);
        Binding.CurrentColor = FMath::Lerp(Binding.TargetColor, Binding.CurrentColor, SmoothFactor);

        for (UMaterialInstanceDynamic* MID : DynamicMaterials)
        {
            if (MID)
            {
                MID->SetVectorParameterValue(Binding.ParameterName, Binding.CurrentColor);
            }
        }
    }

    // Publish state at configured rate
    double CurrentTime = FPlatformTime::Seconds();
    if (CurrentTime - LastPublishTime >= PublishInterval)
    {
        ReadAndPublishState();
        LastPublishTime = CurrentTime;
    }
}

void URshipMaterialBinding::SetupMaterials()
{
    AActor* Owner = GetOwner();
    if (!Owner) return;

    DynamicMaterials.Empty();

    // Get all mesh components
    TArray<UMeshComponent*> MeshComponents;

    if (MeshComponentNames.Num() > 0)
    {
        // Only specified components
        for (const FName& CompName : MeshComponentNames)
        {
            if (UMeshComponent* MeshComp = Cast<UMeshComponent>(Owner->GetDefaultSubobjectByName(CompName)))
            {
                MeshComponents.Add(MeshComp);
            }
        }
    }
    else
    {
        // All mesh components
        Owner->GetComponents<UMeshComponent>(MeshComponents);
    }

    // Create dynamic material instances
    for (UMeshComponent* MeshComp : MeshComponents)
    {
        int32 NumMaterials = MeshComp->GetNumMaterials();

        for (int32 i = 0; i < NumMaterials; i++)
        {
            // Skip if not in allowed slots
            if (MaterialSlots.Num() > 0 && !MaterialSlots.Contains(i))
            {
                continue;
            }

            UMaterialInterface* Material = MeshComp->GetMaterial(i);
            if (!Material) continue;

            UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Material);
            if (!MID && bAutoCreateDynamicMaterials)
            {
                MID = MeshComp->CreateAndSetMaterialInstanceDynamic(i);
            }

            if (MID && !DynamicMaterials.Contains(MID))
            {
                DynamicMaterials.Add(MID);
            }
        }
    }

    UE_LOG(LogRshipExec, Log, TEXT("MaterialBinding: Setup %d dynamic materials on %s"),
        DynamicMaterials.Num(), *Owner->GetName());
}

void URshipMaterialBinding::BindToPulseReceiver()
{
    if (!Subsystem || EmitterId.IsEmpty()) return;

    URshipPulseReceiver* Receiver = Subsystem->GetPulseReceiver();
    if (!Receiver) return;

    PulseHandle = Receiver->OnEmitterPulseReceived.AddUObject(this, &URshipMaterialBinding::OnPulseReceived);
}

void URshipMaterialBinding::UnbindFromPulseReceiver()
{
    if (!Subsystem) return;

    URshipPulseReceiver* Receiver = Subsystem->GetPulseReceiver();
    if (Receiver && PulseHandle.IsValid())
    {
        Receiver->OnEmitterPulseReceived.Remove(PulseHandle);
        PulseHandle.Reset();
    }
}

void URshipMaterialBinding::OnPulseReceived(const FString& InEmitterId, float /*Intensity*/, FLinearColor /*Color*/, TSharedPtr<FJsonObject> Data)
{
    if (InEmitterId != EmitterId || !Data.IsValid()) return;

    // Process scalar bindings
    for (FRshipMaterialScalarBinding& Binding : ScalarBindings)
    {
        if (!Binding.bEnabled) continue;

        float InputValue = ExtractFloatValue(Data, Binding.PulseField, 0.0f);
        float OutputValue = ProcessScalarBinding(Binding, InputValue);

        Binding.TargetValue = OutputValue;

        // Apply immediately if no smoothing
        if (Binding.Smoothing <= 0.0f)
        {
            Binding.CurrentValue = OutputValue;

            for (UMaterialInstanceDynamic* MID : DynamicMaterials)
            {
                if (MID)
                {
                    MID->SetScalarParameterValue(Binding.ParameterName, OutputValue);
                }
            }

            OnScalarUpdated.Broadcast(Binding.ParameterName, OutputValue);
        }
    }

    // Process vector bindings
    for (FRshipMaterialVectorBinding& Binding : VectorBindings)
    {
        if (!Binding.bEnabled) continue;

        FLinearColor InputColor = ExtractColorValue(Data, Binding.ColorField);
        float Alpha = 1.0f;
        if (!Binding.AlphaField.IsEmpty())
        {
            Alpha = ExtractFloatValue(Data, Binding.AlphaField, 1.0f);
        }

        FLinearColor OutputColor = ProcessVectorBinding(Binding, InputColor, Alpha);

        Binding.TargetColor = OutputColor;

        // Apply immediately if no smoothing
        if (Binding.Smoothing <= 0.0f)
        {
            Binding.CurrentColor = OutputColor;

            for (UMaterialInstanceDynamic* MID : DynamicMaterials)
            {
                if (MID)
                {
                    MID->SetVectorParameterValue(Binding.ParameterName, OutputColor);
                }
            }

            OnColorUpdated.Broadcast(Binding.ParameterName, OutputColor);
        }
    }

    // Process texture bindings
    for (FRshipMaterialTextureBinding& Binding : TextureBindings)
    {
        if (!Binding.bEnabled || Binding.Textures.Num() == 0) continue;

        int32 Index = FMath::RoundToInt(ExtractFloatValue(Data, Binding.IndexField, 0.0f));
        Index = FMath::Clamp(Index, 0, Binding.Textures.Num() - 1);

        if (Index != Binding.CurrentIndex && Binding.Textures.IsValidIndex(Index))
        {
            Binding.CurrentIndex = Index;
            UTexture* Texture = Binding.Textures[Index];

            if (Texture)
            {
                for (UMaterialInstanceDynamic* MID : DynamicMaterials)
                {
                    if (MID)
                    {
                        MID->SetTextureParameterValue(Binding.ParameterName, Texture);
                    }
                }
            }
        }
    }
}

float URshipMaterialBinding::ProcessScalarBinding(const FRshipMaterialScalarBinding& Binding, float InputValue)
{
    float Output = InputValue;

    switch (Binding.Mode)
    {
        case ERshipMaterialBindingMode::Direct:
            Output = InputValue;
            break;

        case ERshipMaterialBindingMode::Normalized:
            Output = FMath::Clamp(InputValue, 0.0f, 1.0f);
            break;

        case ERshipMaterialBindingMode::Scaled:
            Output = InputValue * Binding.Scale;
            break;

        case ERshipMaterialBindingMode::Mapped:
            if (Binding.InputMax != Binding.InputMin)
            {
                float Normalized = (InputValue - Binding.InputMin) / (Binding.InputMax - Binding.InputMin);
                Output = FMath::Lerp(Binding.OutputMin, Binding.OutputMax, FMath::Clamp(Normalized, 0.0f, 1.0f));
            }
            break;

        case ERshipMaterialBindingMode::Curve:
            if (Binding.ResponseCurve)
            {
                Output = Binding.ResponseCurve->GetFloatValue(InputValue);
            }
            break;

        case ERshipMaterialBindingMode::Trigger:
            Output = (InputValue >= Binding.TriggerThreshold) ? Binding.OnValue : Binding.OffValue;
            break;

        case ERshipMaterialBindingMode::Blend:
            Output = FMath::Lerp(Binding.OffValue, Binding.OnValue, FMath::Clamp(InputValue, 0.0f, 1.0f));
            break;
    }

    return Output + Binding.Offset;
}

FLinearColor URshipMaterialBinding::ProcessVectorBinding(const FRshipMaterialVectorBinding& Binding, const FLinearColor& InputColor, float Alpha)
{
    FLinearColor Output = InputColor * Binding.ColorMultiplier;
    Output.A = Alpha;

    if (!Binding.bHDR)
    {
        Output.R = FMath::Clamp(Output.R, 0.0f, 1.0f);
        Output.G = FMath::Clamp(Output.G, 0.0f, 1.0f);
        Output.B = FMath::Clamp(Output.B, 0.0f, 1.0f);
    }

    return Output;
}

float URshipMaterialBinding::ExtractFloatValue(TSharedPtr<FJsonObject> Data, const FString& FieldPath, float Default)
{
    if (!Data.IsValid() || FieldPath.IsEmpty()) return Default;

    // Handle nested paths (e.g., "values.intensity")
    TArray<FString> PathParts;
    FieldPath.ParseIntoArray(PathParts, TEXT("."));

    TSharedPtr<FJsonObject> Current = Data;
    for (int32 i = 0; i < PathParts.Num() - 1; i++)
    {
        if (!Current->HasTypedField<EJson::Object>(PathParts[i]))
        {
            return Default;
        }
        Current = Current->GetObjectField(PathParts[i]);
    }

    const FString& FinalField = PathParts.Last();
    if (Current->HasTypedField<EJson::Number>(FinalField))
    {
        return Current->GetNumberField(FinalField);
    }

    return Default;
}

FLinearColor URshipMaterialBinding::ExtractColorValue(TSharedPtr<FJsonObject> Data, const FString& FieldPath)
{
    if (!Data.IsValid() || FieldPath.IsEmpty()) return FLinearColor::Black;

    // Handle nested paths
    TArray<FString> PathParts;
    FieldPath.ParseIntoArray(PathParts, TEXT("."));

    TSharedPtr<FJsonObject> Current = Data;
    for (int32 i = 0; i < PathParts.Num() - 1; i++)
    {
        if (!Current->HasTypedField<EJson::Object>(PathParts[i]))
        {
            return FLinearColor::Black;
        }
        Current = Current->GetObjectField(PathParts[i]);
    }

    const FString& FinalField = PathParts.Last();

    // Try hex string
    if (Current->HasTypedField<EJson::String>(FinalField))
    {
        FString HexColor = Current->GetStringField(FinalField);
        FColor Color = FColor::FromHex(HexColor);
        return FLinearColor(Color);
    }

    // Try RGB object
    if (Current->HasTypedField<EJson::Object>(FinalField))
    {
        TSharedPtr<FJsonObject> ColorObj = Current->GetObjectField(FinalField);
        float R = ColorObj->HasField(TEXT("r")) ? ColorObj->GetNumberField(TEXT("r")) : 0.0f;
        float G = ColorObj->HasField(TEXT("g")) ? ColorObj->GetNumberField(TEXT("g")) : 0.0f;
        float B = ColorObj->HasField(TEXT("b")) ? ColorObj->GetNumberField(TEXT("b")) : 0.0f;
        float A = ColorObj->HasField(TEXT("a")) ? ColorObj->GetNumberField(TEXT("a")) : 1.0f;
        return FLinearColor(R, G, B, A);
    }

    // Try array [r, g, b] or [r, g, b, a]
    if (Current->HasTypedField<EJson::Array>(FinalField))
    {
        TArray<TSharedPtr<FJsonValue>> Arr = Current->GetArrayField(FinalField);
        float R = Arr.Num() > 0 ? Arr[0]->AsNumber() : 0.0f;
        float G = Arr.Num() > 1 ? Arr[1]->AsNumber() : 0.0f;
        float B = Arr.Num() > 2 ? Arr[2]->AsNumber() : 0.0f;
        float A = Arr.Num() > 3 ? Arr[3]->AsNumber() : 1.0f;
        return FLinearColor(R, G, B, A);
    }

    return FLinearColor::Black;
}

void URshipMaterialBinding::SetEmitterId(const FString& NewEmitterId)
{
    if (EmitterId != NewEmitterId)
    {
        UnbindFromPulseReceiver();
        EmitterId = NewEmitterId;
        BindToPulseReceiver();
    }
}

void URshipMaterialBinding::SetScalarValue(FName ParameterName, float Value)
{
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetScalarParameterValue(ParameterName, Value);
        }
    }
}

void URshipMaterialBinding::SetVectorValue(FName ParameterName, FLinearColor Value)
{
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetVectorParameterValue(ParameterName, Value);
        }
    }
}

void URshipMaterialBinding::RefreshMaterials()
{
    SetupMaterials();
}

// ============================================================================
// RS_ ACTIONS - Generic Parameter Control
// ============================================================================

void URshipMaterialBinding::RS_SetScalarParameter(FName ParameterName, float Value)
{
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetScalarParameterValue(ParameterName, Value);
        }
    }
    RS_OnScalarParameterChanged.Broadcast(ParameterName, Value);
}

void URshipMaterialBinding::RS_SetVectorParameter(FName ParameterName, float R, float G, float B, float A)
{
    FLinearColor Color(R, G, B, A);
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetVectorParameterValue(ParameterName, Color);
        }
    }
    RS_OnVectorParameterChanged.Broadcast(ParameterName, R, G, B, A);
}

void URshipMaterialBinding::RS_SetTextureIndex(FName ParameterName, int32 Index)
{
    for (FRshipMaterialTextureBinding& Binding : TextureBindings)
    {
        if (Binding.ParameterName == ParameterName && Binding.Textures.IsValidIndex(Index))
        {
            Binding.CurrentIndex = Index;
            UTexture* Texture = Binding.Textures[Index];
            if (Texture)
            {
                for (UMaterialInstanceDynamic* MID : DynamicMaterials)
                {
                    if (MID)
                    {
                        MID->SetTextureParameterValue(ParameterName, Texture);
                    }
                }
            }
        }
    }
}

// ============================================================================
// RS_ ACTIONS - Common PBR Parameters
// ============================================================================

void URshipMaterialBinding::RS_SetBaseColor(float R, float G, float B)
{
    RS_SetBaseColorWithAlpha(R, G, B, 1.0f);
}

void URshipMaterialBinding::RS_SetBaseColorWithAlpha(float R, float G, float B, float A)
{
    FLinearColor Color(R * GlobalTint.R * GlobalIntensityMultiplier,
                       G * GlobalTint.G * GlobalIntensityMultiplier,
                       B * GlobalTint.B * GlobalIntensityMultiplier, A);
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetVectorParameterValue(TEXT("BaseColor"), Color);
            MID->SetVectorParameterValue(TEXT("Base Color"), Color);
        }
    }
    LastBaseColor = FLinearColor(R, G, B, A);
    RS_OnBaseColorChanged.Broadcast(R, G, B);
}

void URshipMaterialBinding::RS_SetEmissiveColor(float R, float G, float B)
{
    RS_SetEmissive(R, G, B, LastEmissiveIntensity > 0.0f ? LastEmissiveIntensity : 1.0f);
}

void URshipMaterialBinding::RS_SetEmissiveIntensity(float Intensity)
{
    LastEmissiveIntensity = Intensity;
    FLinearColor EmissiveColor = LastEmissiveColor * Intensity * GlobalIntensityMultiplier;
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetVectorParameterValue(TEXT("EmissiveColor"), EmissiveColor);
            MID->SetVectorParameterValue(TEXT("Emissive Color"), EmissiveColor);
            MID->SetScalarParameterValue(TEXT("EmissiveIntensity"), Intensity);
            MID->SetScalarParameterValue(TEXT("Emissive Intensity"), Intensity);
        }
    }
    RS_OnEmissiveIntensityChanged.Broadcast(Intensity);
}

void URshipMaterialBinding::RS_SetEmissive(float R, float G, float B, float Intensity)
{
    LastEmissiveColor = FLinearColor(R, G, B);
    LastEmissiveIntensity = Intensity;
    FLinearColor EmissiveColor(R * Intensity * GlobalIntensityMultiplier,
                                G * Intensity * GlobalIntensityMultiplier,
                                B * Intensity * GlobalIntensityMultiplier);
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetVectorParameterValue(TEXT("EmissiveColor"), EmissiveColor);
            MID->SetVectorParameterValue(TEXT("Emissive Color"), EmissiveColor);
            MID->SetScalarParameterValue(TEXT("EmissiveIntensity"), Intensity);
            MID->SetScalarParameterValue(TEXT("Emissive Intensity"), Intensity);
        }
    }
    RS_OnEmissiveColorChanged.Broadcast(R, G, B);
    RS_OnEmissiveIntensityChanged.Broadcast(Intensity);
}

void URshipMaterialBinding::RS_SetRoughness(float Roughness)
{
    LastRoughness = FMath::Clamp(Roughness, 0.0f, 1.0f);
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetScalarParameterValue(TEXT("Roughness"), LastRoughness);
        }
    }
    RS_OnRoughnessChanged.Broadcast(LastRoughness);
}

void URshipMaterialBinding::RS_SetMetallic(float Metallic)
{
    LastMetallic = FMath::Clamp(Metallic, 0.0f, 1.0f);
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetScalarParameterValue(TEXT("Metallic"), LastMetallic);
        }
    }
    RS_OnMetallicChanged.Broadcast(LastMetallic);
}

void URshipMaterialBinding::RS_SetSpecular(float Specular)
{
    LastSpecular = FMath::Clamp(Specular, 0.0f, 1.0f);
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetScalarParameterValue(TEXT("Specular"), LastSpecular);
        }
    }
    RS_OnSpecularChanged.Broadcast(LastSpecular);
}

void URshipMaterialBinding::RS_SetOpacity(float Opacity)
{
    LastOpacity = FMath::Clamp(Opacity, 0.0f, 1.0f);
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetScalarParameterValue(TEXT("Opacity"), LastOpacity);
        }
    }
    RS_OnOpacityChanged.Broadcast(LastOpacity);
}

void URshipMaterialBinding::RS_SetOpacityMask(float Threshold)
{
    float ClampedThreshold = FMath::Clamp(Threshold, 0.0f, 1.0f);
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetScalarParameterValue(TEXT("OpacityMask"), ClampedThreshold);
            MID->SetScalarParameterValue(TEXT("Opacity Mask"), ClampedThreshold);
            MID->SetScalarParameterValue(TEXT("OpacityMaskClipValue"), ClampedThreshold);
        }
    }
}

void URshipMaterialBinding::RS_SetAmbientOcclusion(float AO)
{
    float ClampedAO = FMath::Clamp(AO, 0.0f, 1.0f);
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetScalarParameterValue(TEXT("AmbientOcclusion"), ClampedAO);
            MID->SetScalarParameterValue(TEXT("Ambient Occlusion"), ClampedAO);
            MID->SetScalarParameterValue(TEXT("AO"), ClampedAO);
        }
    }
}

void URshipMaterialBinding::RS_SetNormalIntensity(float Intensity)
{
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetScalarParameterValue(TEXT("NormalIntensity"), Intensity);
            MID->SetScalarParameterValue(TEXT("Normal Intensity"), Intensity);
            MID->SetScalarParameterValue(TEXT("NormalStrength"), Intensity);
        }
    }
}

// ============================================================================
// RS_ ACTIONS - UV/Texture Animation
// ============================================================================

void URshipMaterialBinding::RS_SetUVTiling(float TileU, float TileV)
{
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetScalarParameterValue(TEXT("TilingU"), TileU);
            MID->SetScalarParameterValue(TEXT("TilingV"), TileV);
            MID->SetVectorParameterValue(TEXT("UVTiling"), FLinearColor(TileU, TileV, 0.0f, 0.0f));
            MID->SetVectorParameterValue(TEXT("UV Tiling"), FLinearColor(TileU, TileV, 0.0f, 0.0f));
        }
    }
}

void URshipMaterialBinding::RS_SetUVOffset(float OffsetU, float OffsetV)
{
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetScalarParameterValue(TEXT("OffsetU"), OffsetU);
            MID->SetScalarParameterValue(TEXT("OffsetV"), OffsetV);
            MID->SetVectorParameterValue(TEXT("UVOffset"), FLinearColor(OffsetU, OffsetV, 0.0f, 0.0f));
            MID->SetVectorParameterValue(TEXT("UV Offset"), FLinearColor(OffsetU, OffsetV, 0.0f, 0.0f));
        }
    }
}

void URshipMaterialBinding::RS_SetUVRotation(float Degrees)
{
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetScalarParameterValue(TEXT("UVRotation"), Degrees);
            MID->SetScalarParameterValue(TEXT("UV Rotation"), Degrees);
        }
    }
}

void URshipMaterialBinding::RS_SetUVPivot(float PivotU, float PivotV)
{
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetVectorParameterValue(TEXT("UVPivot"), FLinearColor(PivotU, PivotV, 0.0f, 0.0f));
            MID->SetVectorParameterValue(TEXT("UV Pivot"), FLinearColor(PivotU, PivotV, 0.0f, 0.0f));
        }
    }
}

// ============================================================================
// RS_ ACTIONS - Subsurface/Cloth/Special
// ============================================================================

void URshipMaterialBinding::RS_SetSubsurfaceColor(float R, float G, float B)
{
    FLinearColor Color(R, G, B);
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetVectorParameterValue(TEXT("SubsurfaceColor"), Color);
            MID->SetVectorParameterValue(TEXT("Subsurface Color"), Color);
        }
    }
}

void URshipMaterialBinding::RS_SetSubsurfaceIntensity(float Intensity)
{
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetScalarParameterValue(TEXT("SubsurfaceIntensity"), Intensity);
            MID->SetScalarParameterValue(TEXT("Subsurface Intensity"), Intensity);
            MID->SetScalarParameterValue(TEXT("Subsurface"), Intensity);
        }
    }
}

void URshipMaterialBinding::RS_SetSheenColor(float R, float G, float B)
{
    FLinearColor Color(R, G, B);
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetVectorParameterValue(TEXT("SheenColor"), Color);
            MID->SetVectorParameterValue(TEXT("Sheen Color"), Color);
            MID->SetVectorParameterValue(TEXT("ClothColor"), Color);
            MID->SetVectorParameterValue(TEXT("Fuzz Color"), Color);
        }
    }
}

void URshipMaterialBinding::RS_SetClearCoat(float Intensity)
{
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetScalarParameterValue(TEXT("ClearCoat"), Intensity);
            MID->SetScalarParameterValue(TEXT("Clear Coat"), Intensity);
            MID->SetScalarParameterValue(TEXT("ClearCoatIntensity"), Intensity);
        }
    }
}

void URshipMaterialBinding::RS_SetClearCoatRoughness(float Roughness)
{
    float ClampedRoughness = FMath::Clamp(Roughness, 0.0f, 1.0f);
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetScalarParameterValue(TEXT("ClearCoatRoughness"), ClampedRoughness);
            MID->SetScalarParameterValue(TEXT("Clear Coat Roughness"), ClampedRoughness);
        }
    }
}

// ============================================================================
// RS_ ACTIONS - Utility
// ============================================================================

void URshipMaterialBinding::RS_ResetToDefaults()
{
    for (const auto& Pair : DefaultScalarValues)
    {
        RS_SetScalarParameter(Pair.Key, Pair.Value);
    }
    for (const auto& Pair : DefaultVectorValues)
    {
        RS_SetVectorParameter(Pair.Key, Pair.Value.R, Pair.Value.G, Pair.Value.B, Pair.Value.A);
    }
}

void URshipMaterialBinding::RS_SetGlobalIntensity(float Intensity)
{
    GlobalIntensityMultiplier = FMath::Max(0.0f, Intensity);
}

void URshipMaterialBinding::RS_SetGlobalTint(float R, float G, float B)
{
    GlobalTint = FLinearColor(R, G, B);
}

void URshipMaterialBinding::RS_BlendToDefaults(float Alpha)
{
    float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
    for (const auto& Pair : DefaultScalarValues)
    {
        float CurrentValue = 0.0f;
        if (DynamicMaterials.Num() > 0 && DynamicMaterials[0])
        {
            DynamicMaterials[0]->GetScalarParameterValue(Pair.Key, CurrentValue);
        }
        float BlendedValue = FMath::Lerp(CurrentValue, Pair.Value, ClampedAlpha);
        RS_SetScalarParameter(Pair.Key, BlendedValue);
    }
    for (const auto& Pair : DefaultVectorValues)
    {
        FLinearColor CurrentColor = FLinearColor::Black;
        if (DynamicMaterials.Num() > 0 && DynamicMaterials[0])
        {
            DynamicMaterials[0]->GetVectorParameterValue(Pair.Key, CurrentColor);
        }
        FLinearColor BlendedColor = FMath::Lerp(CurrentColor, Pair.Value, ClampedAlpha);
        RS_SetVectorParameter(Pair.Key, BlendedColor.R, BlendedColor.G, BlendedColor.B, BlendedColor.A);
    }
}

void URshipMaterialBinding::ForcePublish()
{
    ReadAndPublishState();
}

FString URshipMaterialBinding::GetMaterialStateJson() const
{
    TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();

    JsonObj->SetNumberField(TEXT("roughness"), LastRoughness);
    JsonObj->SetNumberField(TEXT("metallic"), LastMetallic);
    JsonObj->SetNumberField(TEXT("specular"), LastSpecular);
    JsonObj->SetNumberField(TEXT("opacity"), LastOpacity);
    JsonObj->SetNumberField(TEXT("emissiveIntensity"), LastEmissiveIntensity);

    TSharedPtr<FJsonObject> BaseColorObj = MakeShared<FJsonObject>();
    BaseColorObj->SetNumberField(TEXT("r"), LastBaseColor.R);
    BaseColorObj->SetNumberField(TEXT("g"), LastBaseColor.G);
    BaseColorObj->SetNumberField(TEXT("b"), LastBaseColor.B);
    BaseColorObj->SetNumberField(TEXT("a"), LastBaseColor.A);
    JsonObj->SetObjectField(TEXT("baseColor"), BaseColorObj);

    TSharedPtr<FJsonObject> EmissiveObj = MakeShared<FJsonObject>();
    EmissiveObj->SetNumberField(TEXT("r"), LastEmissiveColor.R);
    EmissiveObj->SetNumberField(TEXT("g"), LastEmissiveColor.G);
    EmissiveObj->SetNumberField(TEXT("b"), LastEmissiveColor.B);
    JsonObj->SetObjectField(TEXT("emissiveColor"), EmissiveObj);

    JsonObj->SetNumberField(TEXT("materialCount"), DynamicMaterials.Num());

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);
    return OutputString;
}

void URshipMaterialBinding::ReadAndPublishState()
{
    // Try to read current values from the first dynamic material
    if (DynamicMaterials.Num() == 0) return;

    UMaterialInstanceDynamic* MID = DynamicMaterials[0];
    if (!MID) return;

    FLinearColor BaseColor;
    if (MID->GetVectorParameterValue(TEXT("BaseColor"), BaseColor) ||
        MID->GetVectorParameterValue(TEXT("Base Color"), BaseColor))
    {
        if (!bOnlyPublishOnChange || HasColorChanged(LastBaseColor, BaseColor))
        {
            LastBaseColor = BaseColor;
            RS_OnBaseColorChanged.Broadcast(BaseColor.R, BaseColor.G, BaseColor.B);
        }
    }

    FLinearColor EmissiveColor;
    if (MID->GetVectorParameterValue(TEXT("EmissiveColor"), EmissiveColor) ||
        MID->GetVectorParameterValue(TEXT("Emissive Color"), EmissiveColor))
    {
        if (!bOnlyPublishOnChange || HasColorChanged(LastEmissiveColor, EmissiveColor))
        {
            LastEmissiveColor = EmissiveColor;
            RS_OnEmissiveColorChanged.Broadcast(EmissiveColor.R, EmissiveColor.G, EmissiveColor.B);
        }
    }

    float Roughness;
    if (MID->GetScalarParameterValue(TEXT("Roughness"), Roughness))
    {
        if (!bOnlyPublishOnChange || HasValueChanged(LastRoughness, Roughness))
        {
            LastRoughness = Roughness;
            RS_OnRoughnessChanged.Broadcast(Roughness);
        }
    }

    float Metallic;
    if (MID->GetScalarParameterValue(TEXT("Metallic"), Metallic))
    {
        if (!bOnlyPublishOnChange || HasValueChanged(LastMetallic, Metallic))
        {
            LastMetallic = Metallic;
            RS_OnMetallicChanged.Broadcast(Metallic);
        }
    }

    float Specular;
    if (MID->GetScalarParameterValue(TEXT("Specular"), Specular))
    {
        if (!bOnlyPublishOnChange || HasValueChanged(LastSpecular, Specular))
        {
            LastSpecular = Specular;
            RS_OnSpecularChanged.Broadcast(Specular);
        }
    }

    float Opacity;
    if (MID->GetScalarParameterValue(TEXT("Opacity"), Opacity))
    {
        if (!bOnlyPublishOnChange || HasValueChanged(LastOpacity, Opacity))
        {
            LastOpacity = Opacity;
            RS_OnOpacityChanged.Broadcast(Opacity);
        }
    }
}

bool URshipMaterialBinding::HasColorChanged(const FLinearColor& OldColor, const FLinearColor& NewColor, float Threshold) const
{
    return FMath::Abs(OldColor.R - NewColor.R) > Threshold ||
           FMath::Abs(OldColor.G - NewColor.G) > Threshold ||
           FMath::Abs(OldColor.B - NewColor.B) > Threshold ||
           FMath::Abs(OldColor.A - NewColor.A) > Threshold;
}

bool URshipMaterialBinding::HasValueChanged(float OldValue, float NewValue, float Threshold) const
{
    return FMath::Abs(OldValue - NewValue) > Threshold;
}

void URshipMaterialBinding::CacheDefaultValues()
{
    if (DynamicMaterials.Num() == 0) return;

    UMaterialInstanceDynamic* MID = DynamicMaterials[0];
    if (!MID) return;

    // Cache common scalar defaults
    TArray<FName> ScalarParams = {
        TEXT("Roughness"), TEXT("Metallic"), TEXT("Specular"), TEXT("Opacity"),
        TEXT("AmbientOcclusion"), TEXT("NormalIntensity"), TEXT("EmissiveIntensity"),
        TEXT("ClearCoat"), TEXT("ClearCoatRoughness"), TEXT("SubsurfaceIntensity")
    };

    for (const FName& ParamName : ScalarParams)
    {
        float Value = 0.0f;
        if (MID->GetScalarParameterValue(ParamName, Value))
        {
            DefaultScalarValues.Add(ParamName, Value);
        }
    }

    // Cache common vector defaults
    TArray<FName> VectorParams = {
        TEXT("BaseColor"), TEXT("Base Color"), TEXT("EmissiveColor"), TEXT("Emissive Color"),
        TEXT("SubsurfaceColor"), TEXT("Subsurface Color"), TEXT("SheenColor")
    };

    for (const FName& ParamName : VectorParams)
    {
        FLinearColor Color;
        if (MID->GetVectorParameterValue(ParamName, Color))
        {
            DefaultVectorValues.Add(ParamName, Color);
        }
    }
}

// ============================================================================
// MATERIAL MANAGER
// ============================================================================

void URshipMaterialManager::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;
    UE_LOG(LogRshipExec, Log, TEXT("MaterialManager initialized"));
}

void URshipMaterialManager::Shutdown()
{
    RegisteredBindings.Empty();
    Subsystem = nullptr;
    UE_LOG(LogRshipExec, Log, TEXT("MaterialManager shutdown"));
}

void URshipMaterialManager::Tick(float DeltaTime)
{
    // Manager-level tick if needed
}

void URshipMaterialManager::RegisterBinding(URshipMaterialBinding* Binding)
{
    if (Binding && !RegisteredBindings.Contains(Binding))
    {
        RegisteredBindings.Add(Binding);
    }
}

void URshipMaterialManager::UnregisterBinding(URshipMaterialBinding* Binding)
{
    RegisteredBindings.Remove(Binding);
}

void URshipMaterialManager::SetGlobalIntensityMultiplier(float Multiplier)
{
    GlobalIntensityMultiplier = FMath::Max(0.0f, Multiplier);
}

void URshipMaterialManager::SetGlobalColorTint(FLinearColor Tint)
{
    GlobalColorTint = Tint;
}
