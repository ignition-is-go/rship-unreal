// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/UltimateControlLightingHandler.h"
#include "Engine/Light.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/SkyLight.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/RectLight.h"
#include "EngineUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/TextureLightProfile.h"
#include "Materials/MaterialInterface.h"
// LightmassCharacterIndirectDetailVolume removed in UE 5.6+ - feature deprecated

FUltimateControlLightingHandler::FUltimateControlLightingHandler(UUltimateControlSubsystem* InSubsystem)
	: FUltimateControlHandlerBase(InSubsystem)
{
	RegisterMethod(TEXT("light.list"), TEXT("List lights"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleListLights));
	RegisterMethod(TEXT("light.get"), TEXT("Get light"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetLight));
	RegisterMethod(TEXT("light.getIntensity"), TEXT("Get light intensity"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetLightIntensity));
	RegisterMethod(TEXT("light.setIntensity"), TEXT("Set light intensity"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetLightIntensity));
	RegisterMethod(TEXT("light.getColor"), TEXT("Get light color"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetLightColor));
	RegisterMethod(TEXT("light.setColor"), TEXT("Set light color"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetLightColor));
	RegisterMethod(TEXT("light.getTemperature"), TEXT("Get light temperature"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetLightTemperature));
	RegisterMethod(TEXT("light.setTemperature"), TEXT("Set light temperature"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetLightTemperature));
	RegisterMethod(TEXT("light.getVisibility"), TEXT("Get light visibility"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetLightVisibility));
	RegisterMethod(TEXT("light.setVisibility"), TEXT("Set light visibility"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetLightVisibility));
	RegisterMethod(TEXT("light.getEnabled"), TEXT("Get light enabled"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetLightEnabled));
	RegisterMethod(TEXT("light.setEnabled"), TEXT("Set light enabled"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetLightEnabled));
	RegisterMethod(TEXT("light.getRadius"), TEXT("Get light radius"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetLightRadius));
	RegisterMethod(TEXT("light.setRadius"), TEXT("Set light radius"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetLightRadius));
	RegisterMethod(TEXT("light.getSpotAngles"), TEXT("Get spotlight angles"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetSpotlightAngles));
	RegisterMethod(TEXT("light.setSpotAngles"), TEXT("Set spotlight angles"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetSpotlightAngles));
	RegisterMethod(TEXT("light.getShadowSettings"), TEXT("Get shadow settings"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetShadowSettings));
	RegisterMethod(TEXT("light.setShadowSettings"), TEXT("Set shadow settings"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetShadowSettings));
	RegisterMethod(TEXT("light.getCastShadows"), TEXT("Get cast shadows"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetCastShadows));
	RegisterMethod(TEXT("light.setCastShadows"), TEXT("Set cast shadows"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetCastShadows));
	RegisterMethod(TEXT("light.getSkyLight"), TEXT("Get sky light"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetSkyLight));
	RegisterMethod(TEXT("light.setSkyLightIntensity"), TEXT("Set sky light intensity"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetSkyLightIntensity));
	RegisterMethod(TEXT("light.recaptureSkyLight"), TEXT("Recapture sky light"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleRecaptureSkyLight));
	RegisterMethod(TEXT("light.getDirectionalLight"), TEXT("Get directional light"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetDirectionalLight));
	RegisterMethod(TEXT("light.setSunRotation"), TEXT("Set sun rotation"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetSunRotation));
	RegisterMethod(TEXT("light.getMobility"), TEXT("Get light mobility"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetLightMobility));
	RegisterMethod(TEXT("light.setMobility"), TEXT("Set light mobility"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetLightMobility));
	RegisterMethod(TEXT("light.buildLighting"), TEXT("Build lighting"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleBuildLighting));
	RegisterMethod(TEXT("light.getBuildStatus"), TEXT("Get light build status"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetLightBuildStatus));
	RegisterMethod(TEXT("light.cancelBuild"), TEXT("Cancel light build"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleCancelLightBuild));
	RegisterMethod(TEXT("light.getIESProfile"), TEXT("Get IES profile"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetIESProfile));
	RegisterMethod(TEXT("light.setIESProfile"), TEXT("Set IES profile"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetIESProfile));
	RegisterMethod(TEXT("light.listIESProfiles"), TEXT("List IES profiles"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleListIESProfiles));
	RegisterMethod(TEXT("light.getLightFunction"), TEXT("Get light function"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetLightFunction));
	RegisterMethod(TEXT("light.setLightFunction"), TEXT("Set light function"), TEXT("Light"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetLightFunction));
}

ULightComponent* FUltimateControlLightingHandler::GetLightComponent(const FString& ActorName)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return nullptr;

	AActor* Actor = FindActorByName(World, ActorName);
	if (!Actor) return nullptr;

	// Try to get light component
	return Actor->FindComponentByClass<ULightComponent>();
}

TSharedPtr<FJsonObject> FUltimateControlLightingHandler::LightToJson(ALight* Light)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	Result->SetStringField(TEXT("name"), Light->GetName());
	Result->SetStringField(TEXT("class"), Light->GetClass()->GetName());

	if (ULightComponent* LightComp = Light->GetLightComponent())
	{
		Result->SetObjectField(TEXT("light"), LightComponentToJson(LightComp));
	}

	Result->SetObjectField(TEXT("location"), VectorToJson(Light->GetActorLocation()));
	Result->SetObjectField(TEXT("rotation"), RotatorToJson(Light->GetActorRotation()));

	return Result;
}

TSharedPtr<FJsonObject> FUltimateControlLightingHandler::LightComponentToJson(ULightComponent* LightComponent)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	Result->SetStringField(TEXT("class"), LightComponent->GetClass()->GetName());
	Result->SetNumberField(TEXT("intensity"), LightComponent->Intensity);
	Result->SetObjectField(TEXT("color"), ColorToJson(LightComponent->GetLightColor()));
	Result->SetBoolField(TEXT("castShadows"), LightComponent->CastShadows);
	Result->SetBoolField(TEXT("castStaticShadows"), LightComponent->CastStaticShadows);
	Result->SetBoolField(TEXT("castDynamicShadows"), LightComponent->CastDynamicShadows);
	Result->SetBoolField(TEXT("affectsWorld"), LightComponent->bAffectsWorld);
	Result->SetBoolField(TEXT("useTemperature"), LightComponent->bUseTemperature);
	Result->SetNumberField(TEXT("temperature"), LightComponent->Temperature);

	FString MobilityStr;
	switch (LightComponent->Mobility)
	{
	case EComponentMobility::Static: MobilityStr = TEXT("Static"); break;
	case EComponentMobility::Stationary: MobilityStr = TEXT("Stationary"); break;
	case EComponentMobility::Movable: MobilityStr = TEXT("Movable"); break;
	}
	Result->SetStringField(TEXT("mobility"), MobilityStr);

	// Type-specific properties
	if (UPointLightComponent* PointLight = Cast<UPointLightComponent>(LightComponent))
	{
		Result->SetNumberField(TEXT("attenuationRadius"), PointLight->AttenuationRadius);
		Result->SetNumberField(TEXT("sourceRadius"), PointLight->SourceRadius);
	}

	if (USpotLightComponent* SpotLight = Cast<USpotLightComponent>(LightComponent))
	{
		Result->SetNumberField(TEXT("innerConeAngle"), SpotLight->InnerConeAngle);
		Result->SetNumberField(TEXT("outerConeAngle"), SpotLight->OuterConeAngle);
	}

	return Result;
}

bool FUltimateControlLightingHandler::HandleListLights(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	FString TypeFilter;
	if (Params->HasField(TEXT("type")))
	{
		TypeFilter = Params->GetStringField(TEXT("type"));
	}

	TArray<TSharedPtr<FJsonValue>> LightsArray;

	for (TActorIterator<ALight> It(World); It; ++It)
	{
		ALight* Light = *It;

		// Filter by type if specified
		if (!TypeFilter.IsEmpty())
		{
			if (TypeFilter == TEXT("Point") && !Light->IsA<APointLight>()) continue;
			if (TypeFilter == TEXT("Spot") && !Light->IsA<ASpotLight>()) continue;
			if (TypeFilter == TEXT("Directional") && !Light->IsA<ADirectionalLight>()) continue;
			if (TypeFilter == TEXT("Rect") && !Light->IsA<ARectLight>()) continue;
		}

		LightsArray.Add(MakeShared<FJsonValueObject>(LightToJson(Light)));
	}

	// Also list sky lights
	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		ASkyLight* SkyLight = *It;
		TSharedPtr<FJsonObject> LightObj = MakeShared<FJsonObject>();
		LightObj->SetStringField(TEXT("name"), SkyLight->GetName());
		LightObj->SetStringField(TEXT("class"), TEXT("SkyLight"));
		if (SkyLight->GetLightComponent())
		{
			LightObj->SetNumberField(TEXT("intensity"), SkyLight->GetLightComponent()->Intensity);
		}
		LightsArray.Add(MakeShared<FJsonValueObject>(LightObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("lights"), LightsArray);
	ResultObj->SetNumberField(TEXT("count"), LightsArray.Num());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleGetLight(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LightName;
	if (!RequireString(Params, TEXT("light"), LightName, Error))
	{
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	AActor* Actor = FindActorByName(World, LightName);
	ALight* Light = Cast<ALight>(Actor);

	if (Light)
	{
		Result = MakeShared<FJsonValueObject>(LightToJson(Light));
		return true;
	}

	Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
	return false;
}

bool FUltimateControlLightingHandler::HandleGetLightIntensity(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LightName;
	if (!RequireString(Params, TEXT("light"), LightName, Error))
	{
		return false;
	}

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetNumberField(TEXT("intensity"), LightComp->Intensity);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleSetLightIntensity(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LightName;
	if (!RequireString(Params, TEXT("light"), LightName, Error))
	{
		return false;
	}

	if (!Params->HasField(TEXT("intensity")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: intensity"));
		return false;
	}
	float Intensity = Params->GetNumberField(TEXT("intensity"));

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	LightComp->SetIntensity(Intensity);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleGetLightColor(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LightName;
	if (!RequireString(Params, TEXT("light"), LightName, Error))
	{
		return false;
	}

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetObjectField(TEXT("color"), ColorToJson(LightComp->GetLightColor()));
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleSetLightColor(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LightName;
	if (!RequireString(Params, TEXT("light"), LightName, Error))
	{
		return false;
	}

	if (!Params->HasField(TEXT("color")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: color"));
		return false;
	}
	FLinearColor Color = JsonToColor(Params->GetObjectField(TEXT("color")));

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	LightComp->SetLightColor(Color);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleGetLightTemperature(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LightName;
	if (!RequireString(Params, TEXT("light"), LightName, Error))
	{
		return false;
	}

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetNumberField(TEXT("temperature"), LightComp->Temperature);
	ResultObj->SetBoolField(TEXT("useTemperature"), LightComp->bUseTemperature);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleSetLightTemperature(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LightName;
	if (!RequireString(Params, TEXT("light"), LightName, Error))
	{
		return false;
	}

	if (!Params->HasField(TEXT("temperature")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: temperature"));
		return false;
	}
	float Temperature = Params->GetNumberField(TEXT("temperature"));

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	LightComp->SetTemperature(Temperature);

	bool bUseTemp = true;
	if (Params->HasField(TEXT("useTemperature")))
	{
		bUseTemp = Params->GetBoolField(TEXT("useTemperature"));
	}
	LightComp->SetUseTemperature(bUseTemp);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleGetLightVisibility(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LightName;
	if (!RequireString(Params, TEXT("light"), LightName, Error))
	{
		return false;
	}

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("visible"), LightComp->IsVisible());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleSetLightVisibility(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LightName;
	if (!RequireString(Params, TEXT("light"), LightName, Error))
	{
		return false;
	}

	bool bVisible = true;
	if (Params->HasField(TEXT("visible")))
	{
		bVisible = Params->GetBoolField(TEXT("visible"));
	}

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	LightComp->SetVisibility(bVisible);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleGetLightEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LightName;
	if (!RequireString(Params, TEXT("light"), LightName, Error))
	{
		return false;
	}

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("enabled"), LightComp->bAffectsWorld);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleSetLightEnabled(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LightName;
	if (!RequireString(Params, TEXT("light"), LightName, Error))
	{
		return false;
	}

	bool bEnabled = true;
	if (Params->HasField(TEXT("enabled")))
	{
		bEnabled = Params->GetBoolField(TEXT("enabled"));
	}

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	// SetAffectDynamicIndirectLighting was removed in UE 5.6
	// Use SetVisibility to enable/disable the light component instead
	LightComp->SetVisibility(bEnabled);
	LightComp->SetActive(bEnabled);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleGetLightRadius(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LightName;
	if (!RequireString(Params, TEXT("light"), LightName, Error))
	{
		return false;
	}

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	UPointLightComponent* PointLight = Cast<UPointLightComponent>(LightComp);
	if (!PointLight)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Light is not a point/spot light"));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetNumberField(TEXT("attenuationRadius"), PointLight->AttenuationRadius);
	ResultObj->SetNumberField(TEXT("sourceRadius"), PointLight->SourceRadius);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleSetLightRadius(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LightName;
	if (!RequireString(Params, TEXT("light"), LightName, Error))
	{
		return false;
	}

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	UPointLightComponent* PointLight = Cast<UPointLightComponent>(LightComp);
	if (!PointLight)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Light is not a point/spot light"));
		return false;
	}

	if (Params->HasField(TEXT("attenuationRadius")))
	{
		PointLight->SetAttenuationRadius(Params->GetNumberField(TEXT("attenuationRadius")));
	}

	if (Params->HasField(TEXT("sourceRadius")))
	{
		PointLight->SetSourceRadius(Params->GetNumberField(TEXT("sourceRadius")));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleGetSpotlightAngles(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LightName;
	if (!RequireString(Params, TEXT("light"), LightName, Error))
	{
		return false;
	}

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	USpotLightComponent* SpotLight = Cast<USpotLightComponent>(LightComp);
	if (!SpotLight)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Light is not a spotlight"));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetNumberField(TEXT("innerConeAngle"), SpotLight->InnerConeAngle);
	ResultObj->SetNumberField(TEXT("outerConeAngle"), SpotLight->OuterConeAngle);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleSetSpotlightAngles(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LightName;
	if (!RequireString(Params, TEXT("light"), LightName, Error))
	{
		return false;
	}

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	USpotLightComponent* SpotLight = Cast<USpotLightComponent>(LightComp);
	if (!SpotLight)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Light is not a spotlight"));
		return false;
	}

	if (Params->HasField(TEXT("innerConeAngle")))
	{
		SpotLight->SetInnerConeAngle(Params->GetNumberField(TEXT("innerConeAngle")));
	}

	if (Params->HasField(TEXT("outerConeAngle")))
	{
		SpotLight->SetOuterConeAngle(Params->GetNumberField(TEXT("outerConeAngle")));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleGetShadowSettings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LightName;
	if (!RequireString(Params, TEXT("light"), LightName, Error))
	{
		return false;
	}

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("castShadows"), LightComp->CastShadows);
	ResultObj->SetBoolField(TEXT("castStaticShadows"), LightComp->CastStaticShadows);
	ResultObj->SetBoolField(TEXT("castDynamicShadows"), LightComp->CastDynamicShadows);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleSetShadowSettings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LightName;
	if (!RequireString(Params, TEXT("light"), LightName, Error))
	{
		return false;
	}

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	if (Params->HasField(TEXT("castShadows")))
	{
		LightComp->SetCastShadows(Params->GetBoolField(TEXT("castShadows")));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleGetCastShadows(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	return HandleGetShadowSettings(Params, Result, Error);
}

bool FUltimateControlLightingHandler::HandleSetCastShadows(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LightName;
	if (!RequireString(Params, TEXT("light"), LightName, Error))
	{
		return false;
	}

	bool bCastShadows = true;
	if (Params->HasField(TEXT("castShadows")))
	{
		bCastShadows = Params->GetBoolField(TEXT("castShadows"));
	}

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	LightComp->SetCastShadows(bCastShadows);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleGetSkyLight(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		ASkyLight* SkyLight = *It;
		USkyLightComponent* SkyLightComp = SkyLight->GetLightComponent();

		TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
		ResultObj->SetStringField(TEXT("name"), SkyLight->GetName());
		if (SkyLightComp)
		{
			ResultObj->SetNumberField(TEXT("intensity"), SkyLightComp->Intensity);
			ResultObj->SetBoolField(TEXT("realTimeCaptureEnabled"), SkyLightComp->bRealTimeCapture);
		}
		Result = MakeShared<FJsonValueObject>(ResultObj);
		return true;
	}

	Error = UUltimateControlSubsystem::MakeError(-32003, TEXT("No sky light found in the level"));
	return false;
}

bool FUltimateControlLightingHandler::HandleSetSkyLightIntensity(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (!Params->HasField(TEXT("intensity")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: intensity"));
		return false;
	}
	float Intensity = Params->GetNumberField(TEXT("intensity"));

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		ASkyLight* SkyLight = *It;
		USkyLightComponent* SkyLightComp = SkyLight->GetLightComponent();
		if (SkyLightComp)
		{
			SkyLightComp->SetIntensity(Intensity);
		}

		TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
		ResultObj->SetBoolField(TEXT("success"), true);
		Result = MakeShared<FJsonValueObject>(ResultObj);
		return true;
	}

	Error = UUltimateControlSubsystem::MakeError(-32003, TEXT("No sky light found in the level"));
	return false;
}

bool FUltimateControlLightingHandler::HandleRecaptureSkyLight(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		ASkyLight* SkyLight = *It;
		USkyLightComponent* SkyLightComp = SkyLight->GetLightComponent();
		if (SkyLightComp)
		{
			SkyLightComp->RecaptureSky();
		}

		TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
		ResultObj->SetBoolField(TEXT("success"), true);
		Result = MakeShared<FJsonValueObject>(ResultObj);
		return true;
	}

	Error = UUltimateControlSubsystem::MakeError(-32003, TEXT("No sky light found in the level"));
	return false;
}

bool FUltimateControlLightingHandler::HandleGetDirectionalLight(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		ADirectionalLight* DirLight = *It;

		TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
		ResultObj->SetStringField(TEXT("name"), DirLight->GetName());
		ResultObj->SetObjectField(TEXT("rotation"), RotatorToJson(DirLight->GetActorRotation()));

		if (ULightComponent* LightComp = DirLight->GetLightComponent())
		{
			ResultObj->SetNumberField(TEXT("intensity"), LightComp->Intensity);
			ResultObj->SetObjectField(TEXT("color"), ColorToJson(LightComp->GetLightColor()));
		}

		Result = MakeShared<FJsonValueObject>(ResultObj);
		return true;
	}

	Error = UUltimateControlSubsystem::MakeError(-32003, TEXT("No directional light found in the level"));
	return false;
}

bool FUltimateControlLightingHandler::HandleSetSunRotation(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (!Params->HasField(TEXT("rotation")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: rotation"));
		return false;
	}
	FRotator Rotation = JsonToRotator(Params->GetObjectField(TEXT("rotation")));

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		ADirectionalLight* DirLight = *It;
		DirLight->SetActorRotation(Rotation);

		TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
		ResultObj->SetBoolField(TEXT("success"), true);
		Result = MakeShared<FJsonValueObject>(ResultObj);
		return true;
	}

	Error = UUltimateControlSubsystem::MakeError(-32003, TEXT("No directional light found in the level"));
	return false;
}

bool FUltimateControlLightingHandler::HandleGetLightMobility(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LightName;
	if (!RequireString(Params, TEXT("light"), LightName, Error))
	{
		return false;
	}

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	FString MobilityStr;
	switch (LightComp->Mobility)
	{
	case EComponentMobility::Static: MobilityStr = TEXT("Static"); break;
	case EComponentMobility::Stationary: MobilityStr = TEXT("Stationary"); break;
	case EComponentMobility::Movable: MobilityStr = TEXT("Movable"); break;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("mobility"), MobilityStr);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleSetLightMobility(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LightName;
	if (!RequireString(Params, TEXT("light"), LightName, Error))
	{
		return false;
	}

	FString MobilityStr;
	if (!RequireString(Params, TEXT("mobility"), MobilityStr, Error))
	{
		return false;
	}

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	EComponentMobility::Type Mobility = EComponentMobility::Movable;
	if (MobilityStr == TEXT("Static")) Mobility = EComponentMobility::Static;
	else if (MobilityStr == TEXT("Stationary")) Mobility = EComponentMobility::Stationary;
	else if (MobilityStr == TEXT("Movable")) Mobility = EComponentMobility::Movable;

	LightComp->SetMobility(Mobility);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleBuildLighting(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	// Build lighting is a long operation
	GEditor->Exec(GEditor->GetEditorWorldContext().World(), TEXT("BUILD LIGHTING"));

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("note"), TEXT("Lighting build started. This may take some time."));
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleGetLightBuildStatus(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("isBuilding"), GEditor->IsLightingBuildCurrentlyRunning());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleCancelLightBuild(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	// CancelLightingBuild was removed in UE 5.6 - Lumen replaced static lighting
	// In UE 5.6+, most lighting is computed in real-time using Lumen
	// For static lighting builds, use the Lightmass subsystem if available

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();

	// Check if any lightmass build is in progress
	if (GEditor->IsLightingBuildCurrentlyRunning())
	{
		// Try to cancel through the editor's lightmass interface
		// Note: Direct cancellation API was removed in UE 5.6
		ResultObj->SetBoolField(TEXT("success"), false);
		ResultObj->SetStringField(TEXT("message"), TEXT("Use the Build menu to cancel lighting builds in UE 5.6+"));
	}
	else
	{
		ResultObj->SetBoolField(TEXT("success"), true);
		ResultObj->SetStringField(TEXT("message"), TEXT("No lighting build in progress"));
	}

	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleGetIESProfile(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LightName;
	if (!RequireString(Params, TEXT("light"), LightName, Error))
	{
		return false;
	}

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("useIES"), LightComp->bUseIESBrightness);
	if (LightComp->IESTexture)
	{
		ResultObj->SetStringField(TEXT("profile"), LightComp->IESTexture->GetPathName());
	}
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleSetIESProfile(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LightName;
	if (!RequireString(Params, TEXT("light"), LightName, Error))
	{
		return false;
	}

	FString ProfilePath;
	if (Params->HasField(TEXT("profile")))
	{
		ProfilePath = Params->GetStringField(TEXT("profile"));
	}

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	if (!ProfilePath.IsEmpty())
	{
		UTextureLightProfile* IESTexture = LoadObject<UTextureLightProfile>(nullptr, *ProfilePath);
		if (IESTexture)
		{
			LightComp->SetIESTexture(IESTexture);
		}
	}
	else
	{
		LightComp->SetIESTexture(nullptr);
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleListIESProfiles(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path = TEXT("/Game");
	if (Params->HasField(TEXT("path")))
	{
		Path = Params->GetStringField(TEXT("path"));
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	FARFilter Filter;
	Filter.ClassPaths.Add(UTextureLightProfile::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(*Path));
	Filter.bRecursivePaths = true;

	AssetRegistry.GetAssets(Filter, AssetDataList);

	TArray<TSharedPtr<FJsonValue>> ProfilesArray;
	for (const FAssetData& AssetData : AssetDataList)
	{
		TSharedPtr<FJsonObject> ProfileObj = MakeShared<FJsonObject>();
		ProfileObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		ProfileObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
		ProfilesArray.Add(MakeShared<FJsonValueObject>(ProfileObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("profiles"), ProfilesArray);
	ResultObj->SetNumberField(TEXT("count"), ProfilesArray.Num());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleGetLightFunction(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LightName;
	if (!RequireString(Params, TEXT("light"), LightName, Error))
	{
		return false;
	}

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	if (LightComp->LightFunctionMaterial)
	{
		ResultObj->SetStringField(TEXT("material"), LightComp->LightFunctionMaterial->GetPathName());
	}
	ResultObj->SetNumberField(TEXT("scale"), LightComp->LightFunctionScale.X);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlLightingHandler::HandleSetLightFunction(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString LightName;
	if (!RequireString(Params, TEXT("light"), LightName, Error))
	{
		return false;
	}

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	if (Params->HasField(TEXT("material")))
	{
		FString MaterialPath = Params->GetStringField(TEXT("material"));
		UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
		LightComp->SetLightFunctionMaterial(Material);
	}

	if (Params->HasField(TEXT("scale")))
	{
		float Scale = Params->GetNumberField(TEXT("scale"));
		LightComp->SetLightFunctionScale(FVector(Scale, Scale, Scale));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}
