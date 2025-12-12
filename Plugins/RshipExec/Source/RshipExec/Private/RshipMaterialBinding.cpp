// Rship Material Binding Implementation

#include "RshipMaterialBinding.h"
#include "RshipSubsystem.h"
#include "RshipPulseReceiver.h"
#include "Logs.h"
#include "Engine/Engine.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"

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
    BindToPulseReceiver();

    // Enable tick if we have smoothed parameters
    bool bNeedsTick = false;
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
