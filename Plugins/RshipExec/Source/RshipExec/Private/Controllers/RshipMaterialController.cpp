// Rship Material Controller Implementation

#include "Controllers/RshipMaterialController.h"
#include "Logs.h"
#include "GameFramework/Actor.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// ============================================================================
// MATERIAL CONTROLLER COMPONENT
// ============================================================================

URshipMaterialController::URshipMaterialController()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void URshipMaterialController::RegisterOrRefreshTarget()
{
    FRshipTargetProxy Target = ResolveChildTarget(ChildTargetSuffix, TEXT("material"));
    if (!Target.IsValid())
    {
        return;
    }

    Target
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetScalarParameter), TEXT("SetScalarParameter"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetVectorParameter), TEXT("SetVectorParameter"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetTextureIndex), TEXT("SetTextureIndex"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetBaseColor), TEXT("SetBaseColor"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetBaseColorWithAlpha), TEXT("SetBaseColorWithAlpha"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetEmissiveColor), TEXT("SetEmissiveColor"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetEmissiveIntensity), TEXT("SetEmissiveIntensity"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetEmissive), TEXT("SetEmissive"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetRoughness), TEXT("SetRoughness"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetMetallic), TEXT("SetMetallic"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetSpecular), TEXT("SetSpecular"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetOpacity), TEXT("SetOpacity"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetOpacityMask), TEXT("SetOpacityMask"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetAmbientOcclusion), TEXT("SetAmbientOcclusion"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetNormalIntensity), TEXT("SetNormalIntensity"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetUVTiling), TEXT("SetUVTiling"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetUVOffset), TEXT("SetUVOffset"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetUVRotation), TEXT("SetUVRotation"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetUVPivot), TEXT("SetUVPivot"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetSubsurfaceColor), TEXT("SetSubsurfaceColor"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetSubsurfaceIntensity), TEXT("SetSubsurfaceIntensity"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetSheenColor), TEXT("SetSheenColor"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetClearCoat), TEXT("SetClearCoat"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetClearCoatRoughness), TEXT("SetClearCoatRoughness"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, ResetToDefaults), TEXT("ResetToDefaults"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetGlobalIntensity), TEXT("SetGlobalIntensity"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, SetGlobalTint), TEXT("SetGlobalTint"))
        .AddAction(this, GET_FUNCTION_NAME_CHECKED(URshipMaterialController, BlendToDefaults), TEXT("BlendToDefaults"))
        .AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipMaterialController, OnBaseColorChanged), TEXT("onBaseColorChanged"))
        .AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipMaterialController, OnEmissiveColorChanged), TEXT("onEmissiveColorChanged"))
        .AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipMaterialController, OnEmissiveIntensityChanged), TEXT("onEmissiveIntensityChanged"))
        .AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipMaterialController, OnRoughnessChanged), TEXT("onRoughnessChanged"))
        .AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipMaterialController, OnMetallicChanged), TEXT("onMetallicChanged"))
        .AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipMaterialController, OnSpecularChanged), TEXT("onSpecularChanged"))
        .AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipMaterialController, OnOpacityChanged), TEXT("onOpacityChanged"))
        .AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipMaterialController, OnScalarParameterChanged), TEXT("onScalarParameterChanged"))
        .AddEmitter(this, GET_MEMBER_NAME_CHECKED(URshipMaterialController, OnVectorParameterChanged), TEXT("onVectorParameterChanged"));
}

void URshipMaterialController::BeginPlay()
{
    Super::BeginPlay();

    SetupMaterials();
    CacheDefaultValues();

    // Set publish interval from rate
    PublishInterval = 1.0 / FMath::Max(1, PublishRateHz);

    if (bEnableTick)
    {
        SetComponentTickEnabled(true);
    }

    // Registration is handled by URshipControllerComponent::OnRegister through RegisterOrRefreshTarget.
}

void URshipMaterialController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DynamicMaterials.Empty();

    Super::EndPlay(EndPlayReason);
}

void URshipMaterialController::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Publish state at configured rate
    double CurrentTime = FPlatformTime::Seconds();
    if (CurrentTime - LastPublishTime >= PublishInterval)
    {
        ReadAndPublishState();
        LastPublishTime = CurrentTime;
    }
}

void URshipMaterialController::SetupMaterials()
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

    UE_LOG(LogRshipExec, Log, TEXT("MaterialController: Setup %d dynamic materials on %s"),
        DynamicMaterials.Num(), *Owner->GetName());
}

void URshipMaterialController::SetScalarValue(FName ParameterName, float Value)
{
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetScalarParameterValue(ParameterName, Value);
        }
    }
}

void URshipMaterialController::SetVectorValue(FName ParameterName, FLinearColor Value)
{
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetVectorParameterValue(ParameterName, Value);
        }
    }
}

void URshipMaterialController::RefreshMaterials()
{
    SetupMaterials();
}

// ============================================================================
//  ACTIONS - Generic Parameter Control
// ============================================================================

void URshipMaterialController::SetScalarParameter(FName ParameterName, float Value)
{
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetScalarParameterValue(ParameterName, Value);
        }
    }
    OnScalarParameterChanged.Broadcast(ParameterName, Value);
}

void URshipMaterialController::SetVectorParameter(FName ParameterName, float R, float G, float B, float A)
{
    FLinearColor Color(R, G, B, A);
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetVectorParameterValue(ParameterName, Color);
        }
    }
    OnVectorParameterChanged.Broadcast(ParameterName, R, G, B, A);
}

void URshipMaterialController::SetTextureIndex(FName ParameterName, int32 Index)
{
    TArray<UTexture*>* TextureOptions = TextureParameterOptions.Find(ParameterName);
    if (!TextureOptions || !TextureOptions->IsValidIndex(Index))
    {
        return;
    }

    UTexture* Texture = TextureOptions->operator[](Index);
    if (!Texture)
    {
        return;
    }

    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetTextureParameterValue(ParameterName, Texture);
        }
    }
}

// ============================================================================
//  ACTIONS - Common PBR Parameters
// ============================================================================

void URshipMaterialController::SetBaseColor(float R, float G, float B)
{
    SetBaseColorWithAlpha(R, G, B, 1.0f);
}

void URshipMaterialController::SetBaseColorWithAlpha(float R, float G, float B, float A)
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
    OnBaseColorChanged.Broadcast(R, G, B);
}

void URshipMaterialController::SetEmissiveColor(float R, float G, float B)
{
    SetEmissive(R, G, B, LastEmissiveIntensity > 0.0f ? LastEmissiveIntensity : 1.0f);
}

void URshipMaterialController::SetEmissiveIntensity(float Intensity)
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
    OnEmissiveIntensityChanged.Broadcast(Intensity);
}

void URshipMaterialController::SetEmissive(float R, float G, float B, float Intensity)
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
    OnEmissiveColorChanged.Broadcast(R, G, B);
    OnEmissiveIntensityChanged.Broadcast(Intensity);
}

void URshipMaterialController::SetRoughness(float Roughness)
{
    LastRoughness = FMath::Clamp(Roughness, 0.0f, 1.0f);
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetScalarParameterValue(TEXT("Roughness"), LastRoughness);
        }
    }
    OnRoughnessChanged.Broadcast(LastRoughness);
}

void URshipMaterialController::SetMetallic(float Metallic)
{
    LastMetallic = FMath::Clamp(Metallic, 0.0f, 1.0f);
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetScalarParameterValue(TEXT("Metallic"), LastMetallic);
        }
    }
    OnMetallicChanged.Broadcast(LastMetallic);
}

void URshipMaterialController::SetSpecular(float Specular)
{
    LastSpecular = FMath::Clamp(Specular, 0.0f, 1.0f);
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetScalarParameterValue(TEXT("Specular"), LastSpecular);
        }
    }
    OnSpecularChanged.Broadcast(LastSpecular);
}

void URshipMaterialController::SetOpacity(float Opacity)
{
    LastOpacity = FMath::Clamp(Opacity, 0.0f, 1.0f);
    for (UMaterialInstanceDynamic* MID : DynamicMaterials)
    {
        if (MID)
        {
            MID->SetScalarParameterValue(TEXT("Opacity"), LastOpacity);
        }
    }
    OnOpacityChanged.Broadcast(LastOpacity);
}

void URshipMaterialController::SetOpacityMask(float Threshold)
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

void URshipMaterialController::SetAmbientOcclusion(float AO)
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

void URshipMaterialController::SetNormalIntensity(float Intensity)
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
//  ACTIONS - UV/Texture Animation
// ============================================================================

void URshipMaterialController::SetUVTiling(float TileU, float TileV)
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

void URshipMaterialController::SetUVOffset(float OffsetU, float OffsetV)
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

void URshipMaterialController::SetUVRotation(float Degrees)
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

void URshipMaterialController::SetUVPivot(float PivotU, float PivotV)
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
//  ACTIONS - Subsurface/Cloth/Special
// ============================================================================

void URshipMaterialController::SetSubsurfaceColor(float R, float G, float B)
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

void URshipMaterialController::SetSubsurfaceIntensity(float Intensity)
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

void URshipMaterialController::SetSheenColor(float R, float G, float B)
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

void URshipMaterialController::SetClearCoat(float Intensity)
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

void URshipMaterialController::SetClearCoatRoughness(float Roughness)
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
//  ACTIONS - Utility
// ============================================================================

void URshipMaterialController::ResetToDefaults()
{
    for (const auto& Pair : DefaultScalarValues)
    {
        SetScalarParameter(Pair.Key, Pair.Value);
    }
    for (const auto& Pair : DefaultVectorValues)
    {
        SetVectorParameter(Pair.Key, Pair.Value.R, Pair.Value.G, Pair.Value.B, Pair.Value.A);
    }
}

void URshipMaterialController::SetGlobalIntensity(float Intensity)
{
    GlobalIntensityMultiplier = FMath::Max(0.0f, Intensity);
}

void URshipMaterialController::SetGlobalTint(float R, float G, float B)
{
    GlobalTint = FLinearColor(R, G, B);
}

void URshipMaterialController::BlendToDefaults(float Alpha)
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
        SetScalarParameter(Pair.Key, BlendedValue);
    }
    for (const auto& Pair : DefaultVectorValues)
    {
        FLinearColor CurrentColor = FLinearColor::Black;
        if (DynamicMaterials.Num() > 0 && DynamicMaterials[0])
        {
            DynamicMaterials[0]->GetVectorParameterValue(Pair.Key, CurrentColor);
        }
        FLinearColor BlendedColor = FMath::Lerp(CurrentColor, Pair.Value, ClampedAlpha);
        SetVectorParameter(Pair.Key, BlendedColor.R, BlendedColor.G, BlendedColor.B, BlendedColor.A);
    }
}

void URshipMaterialController::ForcePublish()
{
    ReadAndPublishState();
}

FString URshipMaterialController::GetMaterialStateJson() const
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

void URshipMaterialController::ReadAndPublishState()
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
            OnBaseColorChanged.Broadcast(BaseColor.R, BaseColor.G, BaseColor.B);
        }
    }

    FLinearColor EmissiveColor;
    if (MID->GetVectorParameterValue(TEXT("EmissiveColor"), EmissiveColor) ||
        MID->GetVectorParameterValue(TEXT("Emissive Color"), EmissiveColor))
    {
        if (!bOnlyPublishOnChange || HasColorChanged(LastEmissiveColor, EmissiveColor))
        {
            LastEmissiveColor = EmissiveColor;
            OnEmissiveColorChanged.Broadcast(EmissiveColor.R, EmissiveColor.G, EmissiveColor.B);
        }
    }

    float Roughness;
    if (MID->GetScalarParameterValue(TEXT("Roughness"), Roughness))
    {
        if (!bOnlyPublishOnChange || HasValueChanged(LastRoughness, Roughness))
        {
            LastRoughness = Roughness;
            OnRoughnessChanged.Broadcast(Roughness);
        }
    }

    float Metallic;
    if (MID->GetScalarParameterValue(TEXT("Metallic"), Metallic))
    {
        if (!bOnlyPublishOnChange || HasValueChanged(LastMetallic, Metallic))
        {
            LastMetallic = Metallic;
            OnMetallicChanged.Broadcast(Metallic);
        }
    }

    float Specular;
    if (MID->GetScalarParameterValue(TEXT("Specular"), Specular))
    {
        if (!bOnlyPublishOnChange || HasValueChanged(LastSpecular, Specular))
        {
            LastSpecular = Specular;
            OnSpecularChanged.Broadcast(Specular);
        }
    }

    float Opacity;
    if (MID->GetScalarParameterValue(TEXT("Opacity"), Opacity))
    {
        if (!bOnlyPublishOnChange || HasValueChanged(LastOpacity, Opacity))
        {
            LastOpacity = Opacity;
            OnOpacityChanged.Broadcast(Opacity);
        }
    }
}

bool URshipMaterialController::HasColorChanged(const FLinearColor& OldColor, const FLinearColor& NewColor, float Threshold) const
{
    return FMath::Abs(OldColor.R - NewColor.R) > Threshold ||
           FMath::Abs(OldColor.G - NewColor.G) > Threshold ||
           FMath::Abs(OldColor.B - NewColor.B) > Threshold ||
           FMath::Abs(OldColor.A - NewColor.A) > Threshold;
}

bool URshipMaterialController::HasValueChanged(float OldValue, float NewValue, float Threshold) const
{
    return FMath::Abs(OldValue - NewValue) > Threshold;
}

void URshipMaterialController::CacheDefaultValues()
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
