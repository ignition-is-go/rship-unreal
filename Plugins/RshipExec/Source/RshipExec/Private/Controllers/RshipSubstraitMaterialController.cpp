// Copyright Rocketship. All Rights Reserved.

#include "Controllers/RshipSubstraitMaterialController.h"
#include "RshipSubsystem.h"
#include "RshipActorRegistrationComponent.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/MeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "Logs.h"

// ============================================================================
// FRshipSubstrateMaterialState
// ============================================================================

FRshipSubstrateMaterialState FRshipSubstrateMaterialState::LerpTo(const FRshipSubstrateMaterialState& Target, float Alpha) const
{
	FRshipSubstrateMaterialState Result;

	// Base
	Result.BaseColor = FMath::Lerp(BaseColor, Target.BaseColor, Alpha);
	Result.Roughness = FMath::Lerp(Roughness, Target.Roughness, Alpha);
	Result.Metallic = FMath::Lerp(Metallic, Target.Metallic, Alpha);
	Result.Specular = FMath::Lerp(Specular, Target.Specular, Alpha);

	// Emissive
	Result.EmissiveColor = FMath::Lerp(EmissiveColor, Target.EmissiveColor, Alpha);
	Result.EmissiveIntensity = FMath::Lerp(EmissiveIntensity, Target.EmissiveIntensity, Alpha);

	// Subsurface
	Result.SubsurfaceColor = FMath::Lerp(SubsurfaceColor, Target.SubsurfaceColor, Alpha);
	Result.SubsurfaceStrength = FMath::Lerp(SubsurfaceStrength, Target.SubsurfaceStrength, Alpha);

	// Clear coat
	Result.ClearCoat = FMath::Lerp(ClearCoat, Target.ClearCoat, Alpha);
	Result.ClearCoatRoughness = FMath::Lerp(ClearCoatRoughness, Target.ClearCoatRoughness, Alpha);

	// Anisotropy
	Result.Anisotropy = FMath::Lerp(Anisotropy, Target.Anisotropy, Alpha);
	Result.AnisotropyRotation = FMath::Lerp(AnisotropyRotation, Target.AnisotropyRotation, Alpha);

	// Opacity
	Result.Opacity = FMath::Lerp(Opacity, Target.Opacity, Alpha);
	Result.OpacityMask = FMath::Lerp(OpacityMask, Target.OpacityMask, Alpha);

	// Fuzz
	Result.FuzzAmount = FMath::Lerp(FuzzAmount, Target.FuzzAmount, Alpha);
	Result.FuzzColor = FMath::Lerp(FuzzColor, Target.FuzzColor, Alpha);

	// Detail
	Result.NormalStrength = FMath::Lerp(NormalStrength, Target.NormalStrength, Alpha);
	Result.DisplacementScale = FMath::Lerp(DisplacementScale, Target.DisplacementScale, Alpha);

	return Result;
}

FRshipSubstrateMaterialState FRshipSubstrateMaterialState::FromJson(const TSharedPtr<FJsonObject>& JsonData)
{
	FRshipSubstrateMaterialState State;

	if (!JsonData.IsValid())
	{
		return State;
	}

	// Helper to extract color from JSON
	auto ExtractColor = [&JsonData](const FString& Prefix, FLinearColor Default) -> FLinearColor
	{
		FLinearColor Color = Default;
		JsonData->TryGetNumberField(Prefix + TEXT("_r"), Color.R);
		JsonData->TryGetNumberField(Prefix + TEXT("_g"), Color.G);
		JsonData->TryGetNumberField(Prefix + TEXT("_b"), Color.B);
		JsonData->TryGetNumberField(Prefix + TEXT("_a"), Color.A);
		return Color;
	};

	// Base
	State.BaseColor = ExtractColor(TEXT("baseColor"), State.BaseColor);
	JsonData->TryGetNumberField(TEXT("roughness"), State.Roughness);
	JsonData->TryGetNumberField(TEXT("metallic"), State.Metallic);
	JsonData->TryGetNumberField(TEXT("specular"), State.Specular);

	// Emissive
	State.EmissiveColor = ExtractColor(TEXT("emissive"), State.EmissiveColor);
	JsonData->TryGetNumberField(TEXT("emissiveIntensity"), State.EmissiveIntensity);

	// Subsurface
	State.SubsurfaceColor = ExtractColor(TEXT("subsurface"), State.SubsurfaceColor);
	JsonData->TryGetNumberField(TEXT("subsurfaceStrength"), State.SubsurfaceStrength);

	// Clear coat
	JsonData->TryGetNumberField(TEXT("clearCoat"), State.ClearCoat);
	JsonData->TryGetNumberField(TEXT("clearCoatRoughness"), State.ClearCoatRoughness);

	// Anisotropy
	JsonData->TryGetNumberField(TEXT("anisotropy"), State.Anisotropy);
	JsonData->TryGetNumberField(TEXT("anisotropyRotation"), State.AnisotropyRotation);

	// Opacity
	JsonData->TryGetNumberField(TEXT("opacity"), State.Opacity);
	JsonData->TryGetNumberField(TEXT("opacityMask"), State.OpacityMask);

	// Fuzz
	State.FuzzColor = ExtractColor(TEXT("fuzz"), State.FuzzColor);
	JsonData->TryGetNumberField(TEXT("fuzzAmount"), State.FuzzAmount);

	// Detail
	JsonData->TryGetNumberField(TEXT("normalStrength"), State.NormalStrength);
	JsonData->TryGetNumberField(TEXT("displacementScale"), State.DisplacementScale);

	return State;
}

TSharedPtr<FJsonObject> FRshipSubstrateMaterialState::ToJson() const
{
	TSharedPtr<FJsonObject> JsonData = MakeShared<FJsonObject>();

	// Base
	JsonData->SetNumberField(TEXT("baseColor_r"), BaseColor.R);
	JsonData->SetNumberField(TEXT("baseColor_g"), BaseColor.G);
	JsonData->SetNumberField(TEXT("baseColor_b"), BaseColor.B);
	JsonData->SetNumberField(TEXT("baseColor_a"), BaseColor.A);
	JsonData->SetNumberField(TEXT("roughness"), Roughness);
	JsonData->SetNumberField(TEXT("metallic"), Metallic);
	JsonData->SetNumberField(TEXT("specular"), Specular);

	// Emissive
	JsonData->SetNumberField(TEXT("emissive_r"), EmissiveColor.R);
	JsonData->SetNumberField(TEXT("emissive_g"), EmissiveColor.G);
	JsonData->SetNumberField(TEXT("emissive_b"), EmissiveColor.B);
	JsonData->SetNumberField(TEXT("emissiveIntensity"), EmissiveIntensity);

	// Subsurface
	JsonData->SetNumberField(TEXT("subsurface_r"), SubsurfaceColor.R);
	JsonData->SetNumberField(TEXT("subsurface_g"), SubsurfaceColor.G);
	JsonData->SetNumberField(TEXT("subsurface_b"), SubsurfaceColor.B);
	JsonData->SetNumberField(TEXT("subsurfaceStrength"), SubsurfaceStrength);

	// Clear coat
	JsonData->SetNumberField(TEXT("clearCoat"), ClearCoat);
	JsonData->SetNumberField(TEXT("clearCoatRoughness"), ClearCoatRoughness);

	// Anisotropy
	JsonData->SetNumberField(TEXT("anisotropy"), Anisotropy);
	JsonData->SetNumberField(TEXT("anisotropyRotation"), AnisotropyRotation);

	// Opacity
	JsonData->SetNumberField(TEXT("opacity"), Opacity);
	JsonData->SetNumberField(TEXT("opacityMask"), OpacityMask);

	// Fuzz
	JsonData->SetNumberField(TEXT("fuzz_r"), FuzzColor.R);
	JsonData->SetNumberField(TEXT("fuzz_g"), FuzzColor.G);
	JsonData->SetNumberField(TEXT("fuzz_b"), FuzzColor.B);
	JsonData->SetNumberField(TEXT("fuzzAmount"), FuzzAmount);

	// Detail
	JsonData->SetNumberField(TEXT("normalStrength"), NormalStrength);
	JsonData->SetNumberField(TEXT("displacementScale"), DisplacementScale);

	return JsonData;
}

// ============================================================================
// URshipSubstraitMaterialController
// ============================================================================

URshipSubstraitMaterialController::URshipSubstraitMaterialController()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void URshipSubstraitMaterialController::BeginPlay()
{
	Super::BeginPlay();

	if (GEngine)
	{
		Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	}

	CurrentState = DefaultState;
	TargetState = DefaultState;

	SetupMaterials();
	BindToPulseReceiver();
	ApplyStateToMaterials(CurrentState);

	// Reflection-based registration for this component is owned by URshipBPController.
}

void URshipSubstraitMaterialController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromPulseReceiver();
	Super::EndPlay(EndPlayReason);
}

void URshipSubstraitMaterialController::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Handle transitions
	if (bIsTransitioning && TransitionDuration > 0.0f)
	{
		TransitionProgress += DeltaTime / TransitionDuration;

		if (TransitionProgress >= 1.0f)
		{
			// Transition complete
			TransitionProgress = 1.0f;
			CurrentState = TargetState;
			bIsTransitioning = false;
			ApplyStateToMaterials(CurrentState);
			OnTransitionComplete.Broadcast();
			OnStateChanged.Broadcast(CurrentState);
		}
		else
		{
			// Apply eased progress
			float EasedProgress = TransitionProgress;
			if (TransitionConfig.EasingCurve)
			{
				EasedProgress = TransitionConfig.EasingCurve->GetFloatValue(TransitionProgress);
			}

			CurrentState = TransitionStartState.LerpTo(TargetState, EasedProgress);
			ApplyStateToMaterials(CurrentState);
			OnTransitionProgress.Broadcast(TransitionProgress, CurrentState);
		}
	}
}

void URshipSubstraitMaterialController::SetupMaterials()
{
	DynamicMaterials.Empty();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<UMeshComponent*> MeshComponents;
	Owner->GetComponents<UMeshComponent>(MeshComponents);

	for (UMeshComponent* MeshComp : MeshComponents)
	{
		// Check if we should affect this component
		if (MeshComponentNames.Num() > 0 && !MeshComponentNames.Contains(MeshComp->GetFName()))
		{
			continue;
		}

		int32 NumMaterials = MeshComp->GetNumMaterials();
		for (int32 i = 0; i < NumMaterials; i++)
		{
			// Check if we should affect this slot
			if (MaterialSlots.Num() > 0 && !MaterialSlots.Contains(i))
			{
				continue;
			}

			UMaterialInterface* Material = MeshComp->GetMaterial(i);
			if (!Material)
			{
				continue;
			}

			// Create dynamic material instance
			UMaterialInstanceDynamic* DynamicMaterial = MeshComp->CreateAndSetMaterialInstanceDynamic(i);
			if (DynamicMaterial)
			{
				DynamicMaterials.Add(DynamicMaterial);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RshipSubstraitMaterialController: Set up %d dynamic materials"), DynamicMaterials.Num());
}

void URshipSubstraitMaterialController::BindToPulseReceiver()
{
	// Pulse receiver removed during hard purge.
}

void URshipSubstraitMaterialController::UnbindFromPulseReceiver()
{
	PulseHandle.Reset();
}

void URshipSubstraitMaterialController::OnPulseReceived(const FString& InEmitterId, float Intensity, FLinearColor Color, TSharedPtr<FJsonObject> Data)
{
	// Check if this pulse is for us
	FString ExpectedEmitterId = FString::Printf(TEXT("%s:%s"), *TargetId, *EmitterId);
	if (InEmitterId != ExpectedEmitterId && InEmitterId != EmitterId)
	{
		return;
	}

	// Parse state from JSON data
	if (Data.IsValid())
	{
		FRshipSubstrateMaterialState NewState = FRshipSubstrateMaterialState::FromJson(Data);

		// Check for explicit transition duration in pulse
		double PulseDuration = -1.0;
		if (Data->TryGetNumberField(TEXT("transitionDuration"), PulseDuration) && PulseDuration >= 0.0)
		{
			TransitionToState(NewState, PulseDuration);
		}
		else if (TransitionConfig.Duration > 0.0f)
		{
			TransitionToState(NewState, TransitionConfig.Duration);
		}
		else
		{
			SetState(NewState);
		}
	}
	else
	{
		// Basic intensity/color update
		FRshipSubstrateMaterialState NewState = CurrentState;
		NewState.EmissiveIntensity = Intensity;
		NewState.EmissiveColor = Color;
		SetState(NewState);
	}
}

void URshipSubstraitMaterialController::ApplyStateToMaterials(const FRshipSubstrateMaterialState& State)
{
	for (UMaterialInstanceDynamic* Material : DynamicMaterials)
	{
		if (!Material)
		{
			continue;
		}

		// Base
		Material->SetVectorParameterValue(GetParamName(BaseColorParam, TEXT("BaseColor")), State.BaseColor);
		Material->SetScalarParameterValue(GetParamName(RoughnessParam, TEXT("Roughness")), State.Roughness);
		Material->SetScalarParameterValue(GetParamName(MetallicParam, TEXT("Metallic")), State.Metallic);
		Material->SetScalarParameterValue(FName(TEXT("Specular")), State.Specular);

		// Emissive - combine color and intensity
		FLinearColor EmissiveValue = State.EmissiveColor * State.EmissiveIntensity;
		Material->SetVectorParameterValue(GetParamName(EmissiveColorParam, TEXT("EmissiveColor")), EmissiveValue);
		Material->SetScalarParameterValue(GetParamName(EmissiveIntensityParam, TEXT("EmissiveIntensity")), State.EmissiveIntensity);

		// Subsurface
		Material->SetVectorParameterValue(FName(TEXT("SubsurfaceColor")), State.SubsurfaceColor);
		Material->SetScalarParameterValue(FName(TEXT("SubsurfaceStrength")), State.SubsurfaceStrength);

		// Clear coat
		Material->SetScalarParameterValue(FName(TEXT("ClearCoat")), State.ClearCoat);
		Material->SetScalarParameterValue(FName(TEXT("ClearCoatRoughness")), State.ClearCoatRoughness);

		// Anisotropy
		Material->SetScalarParameterValue(FName(TEXT("Anisotropy")), State.Anisotropy);
		Material->SetScalarParameterValue(FName(TEXT("AnisotropyRotation")), State.AnisotropyRotation);

		// Opacity
		Material->SetScalarParameterValue(FName(TEXT("Opacity")), State.Opacity);
		Material->SetScalarParameterValue(FName(TEXT("OpacityMask")), State.OpacityMask);

		// Fuzz
		Material->SetVectorParameterValue(FName(TEXT("FuzzColor")), State.FuzzColor);
		Material->SetScalarParameterValue(FName(TEXT("FuzzAmount")), State.FuzzAmount);

		// Detail
		Material->SetScalarParameterValue(FName(TEXT("NormalStrength")), State.NormalStrength);
		Material->SetScalarParameterValue(FName(TEXT("DisplacementScale")), State.DisplacementScale);
	}
}

FName URshipSubstraitMaterialController::GetParamName(FName CustomName, const TCHAR* DefaultName) const
{
	return (CustomName != NAME_None) ? CustomName : FName(DefaultName);
}

void URshipSubstraitMaterialController::SetState(const FRshipSubstrateMaterialState& NewState)
{
	bIsTransitioning = false;
	CurrentState = NewState;
	TargetState = NewState;
	ApplyStateToMaterials(CurrentState);
	OnStateChanged.Broadcast(CurrentState);
}

void URshipSubstraitMaterialController::TransitionToState(const FRshipSubstrateMaterialState& NewState, float Duration)
{
	if (Duration < 0.0f)
	{
		Duration = TransitionConfig.Duration;
	}

	if (Duration <= 0.0f)
	{
		SetState(NewState);
		return;
	}

	TransitionStartState = CurrentState;
	TargetState = NewState;
	TransitionDuration = Duration;
	TransitionProgress = 0.0f;
	bIsTransitioning = true;
}

bool URshipSubstraitMaterialController::TransitionToPreset(const FString& PresetName, float Duration)
{
	FRshipSubstratePreset Preset;
	if (GetPreset(PresetName, Preset))
	{
		TransitionToState(Preset.State, Duration);
		return true;
	}
	return false;
}

bool URshipSubstraitMaterialController::CrossfadePresets(const FString& PresetA, const FString& PresetB, float Alpha)
{
	FRshipSubstratePreset A, B;
	if (!GetPreset(PresetA, A) || !GetPreset(PresetB, B))
	{
		return false;
	}

	FRshipSubstrateMaterialState BlendedState = A.State.LerpTo(B.State, FMath::Clamp(Alpha, 0.0f, 1.0f));
	SetState(BlendedState);
	return true;
}

void URshipSubstraitMaterialController::CancelTransition()
{
	if (bIsTransitioning)
	{
		bIsTransitioning = false;
		TargetState = CurrentState;
	}
}

void URshipSubstraitMaterialController::SaveCurrentAsPreset(const FString& PresetName)
{
	// Check if preset already exists
	for (FRshipSubstratePreset& Preset : Presets)
	{
		if (Preset.PresetName == PresetName)
		{
			Preset.State = CurrentState;
			return;
		}
	}

	// Create new preset
	FRshipSubstratePreset NewPreset;
	NewPreset.PresetName = PresetName;
	NewPreset.State = CurrentState;
	Presets.Add(NewPreset);
}

bool URshipSubstraitMaterialController::DeletePreset(const FString& PresetName)
{
	for (int32 i = 0; i < Presets.Num(); i++)
	{
		if (Presets[i].PresetName == PresetName)
		{
			Presets.RemoveAt(i);
			return true;
		}
	}
	return false;
}

bool URshipSubstraitMaterialController::GetPreset(const FString& PresetName, FRshipSubstratePreset& OutPreset) const
{
	for (const FRshipSubstratePreset& Preset : Presets)
	{
		if (Preset.PresetName == PresetName)
		{
			OutPreset = Preset;
			return true;
		}
	}
	return false;
}

void URshipSubstraitMaterialController::RefreshMaterials()
{
	SetupMaterials();
	ApplyStateToMaterials(CurrentState);
}

// ============================================================================
// RS_ ACTIONS - Base Layer
// ============================================================================

void URshipSubstraitMaterialController::RS_SetBaseColor(float R, float G, float B)
{
	CurrentState.BaseColor = FLinearColor(R, G, B, CurrentState.BaseColor.A);
	ApplyStateToMaterials(CurrentState);
	RS_OnBaseColorChanged.Broadcast(R, G, B);
}

void URshipSubstraitMaterialController::RS_SetBaseColorWithAlpha(float R, float G, float B, float A)
{
	CurrentState.BaseColor = FLinearColor(R, G, B, A);
	ApplyStateToMaterials(CurrentState);
	RS_OnBaseColorChanged.Broadcast(R, G, B);
}

void URshipSubstraitMaterialController::RS_SetRoughness(float Roughness)
{
	CurrentState.Roughness = FMath::Clamp(Roughness, 0.0f, 1.0f);
	ApplyStateToMaterials(CurrentState);
	RS_OnRoughnessChanged.Broadcast(CurrentState.Roughness);
}

void URshipSubstraitMaterialController::RS_SetMetallic(float Metallic)
{
	CurrentState.Metallic = FMath::Clamp(Metallic, 0.0f, 1.0f);
	ApplyStateToMaterials(CurrentState);
	RS_OnMetallicChanged.Broadcast(CurrentState.Metallic);
}

void URshipSubstraitMaterialController::RS_SetSpecular(float Specular)
{
	CurrentState.Specular = FMath::Clamp(Specular, 0.0f, 1.0f);
	ApplyStateToMaterials(CurrentState);
	RS_OnSpecularChanged.Broadcast(CurrentState.Specular);
}

// ============================================================================
// RS_ ACTIONS - Emissive
// ============================================================================

void URshipSubstraitMaterialController::RS_SetEmissiveColor(float R, float G, float B)
{
	CurrentState.EmissiveColor = FLinearColor(R, G, B, 1.0f);
	ApplyStateToMaterials(CurrentState);
	RS_OnEmissiveColorChanged.Broadcast(R, G, B);
}

void URshipSubstraitMaterialController::RS_SetEmissiveIntensity(float Intensity)
{
	CurrentState.EmissiveIntensity = FMath::Max(Intensity, 0.0f);
	ApplyStateToMaterials(CurrentState);
	RS_OnEmissiveIntensityChanged.Broadcast(CurrentState.EmissiveIntensity);
}

void URshipSubstraitMaterialController::RS_SetEmissive(float R, float G, float B, float Intensity)
{
	CurrentState.EmissiveColor = FLinearColor(R, G, B, 1.0f);
	CurrentState.EmissiveIntensity = FMath::Max(Intensity, 0.0f);
	ApplyStateToMaterials(CurrentState);
	RS_OnEmissiveColorChanged.Broadcast(R, G, B);
	RS_OnEmissiveIntensityChanged.Broadcast(CurrentState.EmissiveIntensity);
}

// ============================================================================
// RS_ ACTIONS - Subsurface
// ============================================================================

void URshipSubstraitMaterialController::RS_SetSubsurfaceColor(float R, float G, float B)
{
	CurrentState.SubsurfaceColor = FLinearColor(R, G, B, 1.0f);
	ApplyStateToMaterials(CurrentState);
}

void URshipSubstraitMaterialController::RS_SetSubsurfaceStrength(float Strength)
{
	CurrentState.SubsurfaceStrength = FMath::Clamp(Strength, 0.0f, 1.0f);
	ApplyStateToMaterials(CurrentState);
}

// ============================================================================
// RS_ ACTIONS - Clear Coat
// ============================================================================

void URshipSubstraitMaterialController::RS_SetClearCoat(float Intensity)
{
	CurrentState.ClearCoat = FMath::Clamp(Intensity, 0.0f, 1.0f);
	ApplyStateToMaterials(CurrentState);
}

void URshipSubstraitMaterialController::RS_SetClearCoatRoughness(float Roughness)
{
	CurrentState.ClearCoatRoughness = FMath::Clamp(Roughness, 0.0f, 1.0f);
	ApplyStateToMaterials(CurrentState);
}

// ============================================================================
// RS_ ACTIONS - Anisotropy
// ============================================================================

void URshipSubstraitMaterialController::RS_SetAnisotropy(float Anisotropy)
{
	CurrentState.Anisotropy = FMath::Clamp(Anisotropy, -1.0f, 1.0f);
	ApplyStateToMaterials(CurrentState);
}

void URshipSubstraitMaterialController::RS_SetAnisotropyRotation(float Rotation)
{
	CurrentState.AnisotropyRotation = FMath::Clamp(Rotation, 0.0f, 1.0f);
	ApplyStateToMaterials(CurrentState);
}

// ============================================================================
// RS_ ACTIONS - Opacity
// ============================================================================

void URshipSubstraitMaterialController::RS_SetOpacity(float Opacity)
{
	CurrentState.Opacity = FMath::Clamp(Opacity, 0.0f, 1.0f);
	ApplyStateToMaterials(CurrentState);
	RS_OnOpacityChanged.Broadcast(CurrentState.Opacity);
}

void URshipSubstraitMaterialController::RS_SetOpacityMask(float Threshold)
{
	CurrentState.OpacityMask = FMath::Clamp(Threshold, 0.0f, 1.0f);
	ApplyStateToMaterials(CurrentState);
}

// ============================================================================
// RS_ ACTIONS - Fuzz
// ============================================================================

void URshipSubstraitMaterialController::RS_SetFuzzAmount(float Amount)
{
	CurrentState.FuzzAmount = FMath::Clamp(Amount, 0.0f, 1.0f);
	ApplyStateToMaterials(CurrentState);
}

void URshipSubstraitMaterialController::RS_SetFuzzColor(float R, float G, float B)
{
	CurrentState.FuzzColor = FLinearColor(R, G, B, 1.0f);
	ApplyStateToMaterials(CurrentState);
}

// ============================================================================
// RS_ ACTIONS - Detail
// ============================================================================

void URshipSubstraitMaterialController::RS_SetNormalStrength(float Strength)
{
	CurrentState.NormalStrength = FMath::Clamp(Strength, 0.0f, 2.0f);
	ApplyStateToMaterials(CurrentState);
}

void URshipSubstraitMaterialController::RS_SetDisplacementScale(float Scale)
{
	CurrentState.DisplacementScale = FMath::Clamp(Scale, 0.0f, 10.0f);
	ApplyStateToMaterials(CurrentState);
}

// ============================================================================
// RS_ ACTIONS - Transitions & Presets
// ============================================================================

void URshipSubstraitMaterialController::RS_TransitionToPreset(const FString& PresetName, float Duration)
{
	if (TransitionToPreset(PresetName, Duration))
	{
		RS_OnPresetChanged.Broadcast(PresetName);
	}
}

void URshipSubstraitMaterialController::RS_SetTransitionDuration(float Duration)
{
	TransitionConfig.Duration = FMath::Clamp(Duration, 0.0f, 60.0f);
}

void URshipSubstraitMaterialController::RS_NextPreset()
{
	if (Presets.Num() == 0)
	{
		return;
	}

	// Find current preset index
	int32 CurrentIndex = -1;
	for (int32 i = 0; i < Presets.Num(); i++)
	{
		// Simple heuristic: find the preset closest to current state
		// In practice, we'd track which preset we're on
	}

	// Cycle to next
	int32 NextIndex = (CurrentIndex + 1) % Presets.Num();
	TransitionToPreset(Presets[NextIndex].PresetName, TransitionConfig.Duration);
	RS_OnPresetChanged.Broadcast(Presets[NextIndex].PresetName);
}

void URshipSubstraitMaterialController::RS_PreviousPreset()
{
	if (Presets.Num() == 0)
	{
		return;
	}

	// Find current preset index (simplified - would need to track this properly)
	int32 CurrentIndex = 0;

	// Cycle to previous
	int32 PrevIndex = (CurrentIndex - 1 + Presets.Num()) % Presets.Num();
	TransitionToPreset(Presets[PrevIndex].PresetName, TransitionConfig.Duration);
	RS_OnPresetChanged.Broadcast(Presets[PrevIndex].PresetName);
}

// ============================================================================
// RS_ ACTIONS - Utility
// ============================================================================

void URshipSubstraitMaterialController::RS_ResetToDefault()
{
	TransitionToState(DefaultState, TransitionConfig.Duration);
}

void URshipSubstraitMaterialController::RS_SetGlobalIntensity(float Intensity)
{
	float ClampedIntensity = FMath::Clamp(Intensity, 0.0f, 10.0f);

	// Apply global intensity to emissive
	CurrentState.EmissiveIntensity = DefaultState.EmissiveIntensity * ClampedIntensity;
	ApplyStateToMaterials(CurrentState);
	RS_OnGlobalIntensityChanged.Broadcast(ClampedIntensity);
}

// ============================================================================
// RS_ State Publishing
// ============================================================================

void URshipSubstraitMaterialController::ForcePublish()
{
	// Publish all current state values to emitters
	RS_OnBaseColorChanged.Broadcast(CurrentState.BaseColor.R, CurrentState.BaseColor.G, CurrentState.BaseColor.B);
	RS_OnRoughnessChanged.Broadcast(CurrentState.Roughness);
	RS_OnMetallicChanged.Broadcast(CurrentState.Metallic);
	RS_OnSpecularChanged.Broadcast(CurrentState.Specular);
	RS_OnEmissiveColorChanged.Broadcast(CurrentState.EmissiveColor.R, CurrentState.EmissiveColor.G, CurrentState.EmissiveColor.B);
	RS_OnEmissiveIntensityChanged.Broadcast(CurrentState.EmissiveIntensity);
	RS_OnOpacityChanged.Broadcast(CurrentState.Opacity);
}

FString URshipSubstraitMaterialController::GetSubstrateStateJson() const
{
	TSharedPtr<FJsonObject> JsonObject = CurrentState.ToJson();

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

	return OutputString;
}

bool URshipSubstraitMaterialController::IsSubstrateMaterial(UMaterialInterface* Material)
{
	if (!Material)
	{
		return false;
	}

	UMaterial* BaseMaterial = Material->GetMaterial();
	if (!BaseMaterial)
	{
		return false;
	}

	// Check for Substrate shading models
	// In UE 5.5+, Substrate is the next-gen material system
	// For now, we detect it by checking if certain Substrate-specific features are enabled
	// This is a simplified check - full detection would require deeper material inspection

	// Check blend mode and material domain for hints
	// Substrate materials often use advanced features that aren't in legacy materials

	// For now, we assume all materials could benefit from Substrate-style control
	// The actual parameters available depend on the material setup
	return true;
}

TArray<UMaterialInstanceDynamic*> URshipSubstraitMaterialController::GetSubstrateMaterials() const
{
	TArray<UMaterialInstanceDynamic*> SubstrateMaterials;

	for (UMaterialInstanceDynamic* Material : DynamicMaterials)
	{
		if (Material && IsSubstrateMaterial(Material))
		{
			SubstrateMaterials.Add(Material);
		}
	}

	return SubstrateMaterials;
}

// ============================================================================
// URshipSubstrateMaterialManager
// ============================================================================

void URshipSubstrateMaterialManager::Initialize(URshipSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
}

void URshipSubstrateMaterialManager::Shutdown()
{
	RegisteredBindings.Empty();
	Subsystem = nullptr;
}

void URshipSubstrateMaterialManager::Tick(float DeltaTime)
{
	// Manager tick - could be used for global effects
}

void URshipSubstrateMaterialManager::RegisterBinding(URshipSubstraitMaterialController* Binding)
{
	if (Binding && !RegisteredBindings.Contains(Binding))
	{
		RegisteredBindings.Add(Binding);
	}
}

void URshipSubstrateMaterialManager::UnregisterBinding(URshipSubstraitMaterialController* Binding)
{
	RegisteredBindings.Remove(Binding);
}

void URshipSubstrateMaterialManager::TransitionAllToPreset(const FString& PresetName, float Duration)
{
	for (URshipSubstraitMaterialController* Binding : RegisteredBindings)
	{
		if (Binding)
		{
			Binding->TransitionToPreset(PresetName, Duration);
		}
	}
}

void URshipSubstrateMaterialManager::SetGlobalMasterBrightness(float Brightness)
{
	GlobalMasterBrightness = FMath::Clamp(Brightness, 0.0f, 10.0f);

	// Apply to all bindings by adjusting their emissive intensity
	for (URshipSubstraitMaterialController* Binding : RegisteredBindings)
	{
		if (Binding)
		{
			FRshipSubstrateMaterialState State = Binding->GetCurrentState();
			// Scale emissive by global brightness
			// This is a simple approach - could be more sophisticated
			Binding->SetState(State);
		}
	}
}

void URshipSubstrateMaterialManager::AddPreset(const FRshipSubstratePreset& Preset)
{
	// Check if preset already exists
	for (FRshipSubstratePreset& ExistingPreset : GlobalPresets)
	{
		if (ExistingPreset.PresetName == Preset.PresetName)
		{
			// Update existing preset
			ExistingPreset.State = Preset.State;
			ExistingPreset.Description = Preset.Description;
			return;
		}
	}

	// Add new preset
	GlobalPresets.Add(Preset);
}

bool URshipSubstrateMaterialManager::GetGlobalPreset(const FString& PresetName, FRshipSubstratePreset& OutPreset) const
{
	for (const FRshipSubstratePreset& Preset : GlobalPresets)
	{
		if (Preset.PresetName == PresetName)
		{
			OutPreset = Preset;
			return true;
		}
	}
	return false;
}
