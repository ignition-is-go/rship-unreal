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

void FUltimateControlLightingHandler::RegisterMethods(TMap<FString, FJsonRpcMethodHandler>& Methods)
{
	Methods.Add(TEXT("light.list"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleListLights));
	Methods.Add(TEXT("light.get"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetLight));
	Methods.Add(TEXT("light.getIntensity"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetLightIntensity));
	Methods.Add(TEXT("light.setIntensity"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetLightIntensity));
	Methods.Add(TEXT("light.getColor"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetLightColor));
	Methods.Add(TEXT("light.setColor"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetLightColor));
	Methods.Add(TEXT("light.getTemperature"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetLightTemperature));
	Methods.Add(TEXT("light.setTemperature"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetLightTemperature));
	Methods.Add(TEXT("light.getVisibility"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetLightVisibility));
	Methods.Add(TEXT("light.setVisibility"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetLightVisibility));
	Methods.Add(TEXT("light.getEnabled"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetLightEnabled));
	Methods.Add(TEXT("light.setEnabled"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetLightEnabled));
	Methods.Add(TEXT("light.getRadius"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetLightRadius));
	Methods.Add(TEXT("light.setRadius"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetLightRadius));
	Methods.Add(TEXT("light.getSpotAngles"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetSpotlightAngles));
	Methods.Add(TEXT("light.setSpotAngles"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetSpotlightAngles));
	Methods.Add(TEXT("light.getShadowSettings"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetShadowSettings));
	Methods.Add(TEXT("light.setShadowSettings"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetShadowSettings));
	Methods.Add(TEXT("light.getCastShadows"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetCastShadows));
	Methods.Add(TEXT("light.setCastShadows"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetCastShadows));
	Methods.Add(TEXT("light.getSkyLight"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetSkyLight));
	Methods.Add(TEXT("light.setSkyLightIntensity"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetSkyLightIntensity));
	Methods.Add(TEXT("light.recaptureSkyLight"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleRecaptureSkyLight));
	Methods.Add(TEXT("light.getDirectionalLight"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetDirectionalLight));
	Methods.Add(TEXT("light.setSunRotation"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetSunRotation));
	Methods.Add(TEXT("light.getMobility"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetLightMobility));
	Methods.Add(TEXT("light.setMobility"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetLightMobility));
	Methods.Add(TEXT("light.buildLighting"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleBuildLighting));
	Methods.Add(TEXT("light.getBuildStatus"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetLightBuildStatus));
	Methods.Add(TEXT("light.cancelBuild"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleCancelLightBuild));
	Methods.Add(TEXT("light.getIESProfile"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetIESProfile));
	Methods.Add(TEXT("light.setIESProfile"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetIESProfile));
	Methods.Add(TEXT("light.listIESProfiles"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleListIESProfiles));
	Methods.Add(TEXT("light.getLightFunction"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleGetLightFunction));
	Methods.Add(TEXT("light.setLightFunction"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlLightingHandler::HandleSetLightFunction));
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
		Error = CreateError(-32002, TEXT("No world loaded"));
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
		Error = CreateError(-32002, TEXT("No world loaded"));
		return false;
	}

	AActor* Actor = FindActorByName(World, LightName);
	ALight* Light = Cast<ALight>(Actor);

	if (Light)
	{
		Result = MakeShared<FJsonValueObject>(LightToJson(Light));
		return true;
	}

	Error = CreateError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
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
		Error = CreateError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
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
		Error = CreateError(-32602, TEXT("Missing required parameter: intensity"));
		return false;
	}
	float Intensity = Params->GetNumberField(TEXT("intensity"));

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = CreateError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
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
		Error = CreateError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
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
		Error = CreateError(-32602, TEXT("Missing required parameter: color"));
		return false;
	}
	FLinearColor Color = JsonToColor(Params->GetObjectField(TEXT("color")));

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = CreateError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
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
		Error = CreateError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
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
		Error = CreateError(-32602, TEXT("Missing required parameter: temperature"));
		return false;
	}
	float Temperature = Params->GetNumberField(TEXT("temperature"));

	ULightComponent* LightComp = GetLightComponent(LightName);
	if (!LightComp)
	{
		Error = CreateError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
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
		Error = CreateError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
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
		Error = CreateError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
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
		Error = CreateError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
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
		Error = CreateError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	LightComp->SetAffectDynamicIndirectLighting(bEnabled);

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
		Error = CreateError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	UPointLightComponent* PointLight = Cast<UPointLightComponent>(LightComp);
	if (!PointLight)
	{
		Error = CreateError(-32002, TEXT("Light is not a point/spot light"));
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
		Error = CreateError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	UPointLightComponent* PointLight = Cast<UPointLightComponent>(LightComp);
	if (!PointLight)
	{
		Error = CreateError(-32002, TEXT("Light is not a point/spot light"));
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
		Error = CreateError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	USpotLightComponent* SpotLight = Cast<USpotLightComponent>(LightComp);
	if (!SpotLight)
	{
		Error = CreateError(-32002, TEXT("Light is not a spotlight"));
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
		Error = CreateError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
		return false;
	}

	USpotLightComponent* SpotLight = Cast<USpotLightComponent>(LightComp);
	if (!SpotLight)
	{
		Error = CreateError(-32002, TEXT("Light is not a spotlight"));
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
		Error = CreateError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
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
		Error = CreateError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
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
		Error = CreateError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
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
		Error = CreateError(-32002, TEXT("No world loaded"));
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

	Error = CreateError(-32003, TEXT("No sky light found in the level"));
	return false;
}

bool FUltimateControlLightingHandler::HandleSetSkyLightIntensity(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (!Params->HasField(TEXT("intensity")))
	{
		Error = CreateError(-32602, TEXT("Missing required parameter: intensity"));
		return false;
	}
	float Intensity = Params->GetNumberField(TEXT("intensity"));

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = CreateError(-32002, TEXT("No world loaded"));
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

	Error = CreateError(-32003, TEXT("No sky light found in the level"));
	return false;
}

bool FUltimateControlLightingHandler::HandleRecaptureSkyLight(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = CreateError(-32002, TEXT("No world loaded"));
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

	Error = CreateError(-32003, TEXT("No sky light found in the level"));
	return false;
}

bool FUltimateControlLightingHandler::HandleGetDirectionalLight(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = CreateError(-32002, TEXT("No world loaded"));
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

	Error = CreateError(-32003, TEXT("No directional light found in the level"));
	return false;
}

bool FUltimateControlLightingHandler::HandleSetSunRotation(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (!Params->HasField(TEXT("rotation")))
	{
		Error = CreateError(-32602, TEXT("Missing required parameter: rotation"));
		return false;
	}
	FRotator Rotation = JsonToRotator(Params->GetObjectField(TEXT("rotation")));

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = CreateError(-32002, TEXT("No world loaded"));
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

	Error = CreateError(-32003, TEXT("No directional light found in the level"));
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
		Error = CreateError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
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
		Error = CreateError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
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
	// Cancel lighting build
	GEditor->CancelLightingBuild();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
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
		Error = CreateError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
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
		Error = CreateError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
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
		Error = CreateError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
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
		Error = CreateError(-32003, FString::Printf(TEXT("Light not found: %s"), *LightName));
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
