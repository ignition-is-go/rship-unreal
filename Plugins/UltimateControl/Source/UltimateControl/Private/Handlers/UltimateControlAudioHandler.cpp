// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/UltimateControlAudioHandler.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundClass.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "AudioDevice.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"
#include "EngineUtils.h"

FUltimateControlAudioHandler::FUltimateControlAudioHandler(UUltimateControlSubsystem* InSubsystem)
	: FUltimateControlHandlerBase(InSubsystem)
{
	RegisterMethod(TEXT("audio.listSounds"), TEXT("List sounds"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandleListSounds));
	RegisterMethod(TEXT("audio.getSound"), TEXT("Get sound"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandleGetSound));
	RegisterMethod(TEXT("audio.listCues"), TEXT("List sound cues"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandleListSoundCues));
	RegisterMethod(TEXT("audio.listMixes"), TEXT("List sound mixes"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandleListSoundMixes));
	RegisterMethod(TEXT("audio.listClasses"), TEXT("List sound classes"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandleListSoundClasses));
	RegisterMethod(TEXT("audio.play2D"), TEXT("Play sound 2D"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandlePlaySound2D));
	RegisterMethod(TEXT("audio.playAtLocation"), TEXT("Play sound at location"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandlePlaySoundAtLocation));
	RegisterMethod(TEXT("audio.playAttached"), TEXT("Play sound attached"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandlePlaySoundAttached));
	RegisterMethod(TEXT("audio.stop"), TEXT("Stop sound"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandleStopSound));
	RegisterMethod(TEXT("audio.stopAll"), TEXT("Stop all sounds"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandleStopAllSounds));
	RegisterMethod(TEXT("audio.getComponents"), TEXT("Get audio components"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandleGetAudioComponents));
	RegisterMethod(TEXT("audio.setVolume"), TEXT("Set volume"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandleSetAudioComponentVolume));
	RegisterMethod(TEXT("audio.setPitch"), TEXT("Set pitch"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandleSetAudioComponentPitch));
	RegisterMethod(TEXT("audio.fade"), TEXT("Fade audio"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandleFadeAudioComponent));
	RegisterMethod(TEXT("audio.getMasterVolume"), TEXT("Get master volume"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandleGetMasterVolume));
	RegisterMethod(TEXT("audio.setMasterVolume"), TEXT("Set master volume"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandleSetMasterVolume));
	RegisterMethod(TEXT("audio.muteAll"), TEXT("Mute all"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandleMuteAll));
	RegisterMethod(TEXT("audio.unmuteAll"), TEXT("Unmute all"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandleUnmuteAll));
	RegisterMethod(TEXT("audio.pushMix"), TEXT("Push sound mix"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandlePushSoundMix));
	RegisterMethod(TEXT("audio.popMix"), TEXT("Pop sound mix"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandlePopSoundMix));
	RegisterMethod(TEXT("audio.clearMixes"), TEXT("Clear sound mixes"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandleClearSoundMixes));
	RegisterMethod(TEXT("audio.setClassOverride"), TEXT("Set sound class override"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandleSetSoundMixClassOverride));
	RegisterMethod(TEXT("audio.getClassVolume"), TEXT("Get sound class volume"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandleGetSoundClassVolume));
	RegisterMethod(TEXT("audio.setClassVolume"), TEXT("Set sound class volume"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandleSetSoundClassVolume));
	RegisterMethod(TEXT("audio.getDevices"), TEXT("Get active audio devices"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandleGetActiveAudioDevices));
	RegisterMethod(TEXT("audio.getStats"), TEXT("Get audio stats"), TEXT("Audio"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAudioHandler::HandleGetAudioStats));
}

TSharedPtr<FJsonObject> FUltimateControlAudioHandler::SoundToJson(USoundBase* Sound)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	Result->SetStringField(TEXT("name"), Sound->GetName());
	Result->SetStringField(TEXT("path"), Sound->GetPathName());
	Result->SetStringField(TEXT("class"), Sound->GetClass()->GetName());
	Result->SetNumberField(TEXT("duration"), Sound->Duration);
	Result->SetNumberField(TEXT("maxDistance"), Sound->MaxDistance);

	if (Sound->SoundClassObject)
	{
		Result->SetStringField(TEXT("soundClass"), Sound->SoundClassObject->GetName());
	}

	return Result;
}

TSharedPtr<FJsonObject> FUltimateControlAudioHandler::AudioComponentToJson(UAudioComponent* AudioComponent)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	Result->SetStringField(TEXT("name"), AudioComponent->GetName());
	Result->SetBoolField(TEXT("isPlaying"), AudioComponent->IsPlaying());
	Result->SetNumberField(TEXT("volumeMultiplier"), AudioComponent->VolumeMultiplier);
	Result->SetNumberField(TEXT("pitchMultiplier"), AudioComponent->PitchMultiplier);

	if (AudioComponent->Sound)
	{
		Result->SetStringField(TEXT("sound"), AudioComponent->Sound->GetPathName());
	}

	if (AudioComponent->GetOwner())
	{
		Result->SetStringField(TEXT("owner"), AudioComponent->GetOwner()->GetName());
	}

	return Result;
}

bool FUltimateControlAudioHandler::HandleListSounds(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path = TEXT("/Game");
	if (Params->HasField(TEXT("path")))
	{
		Path = Params->GetStringField(TEXT("path"));
	}

	int32 Limit = 500;
	if (Params->HasField(TEXT("limit")))
	{
		Limit = FMath::Clamp(FMath::RoundToInt(Params->GetNumberField(TEXT("limit"))), 1, 10000);
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	FARFilter Filter;
	Filter.ClassPaths.Add(USoundWave::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(*Path));
	Filter.bRecursivePaths = true;

	AssetRegistry.GetAssets(Filter, AssetDataList);

	TArray<TSharedPtr<FJsonValue>> SoundsArray;
	for (int32 i = 0; i < FMath::Min(AssetDataList.Num(), Limit); i++)
	{
		const FAssetData& AssetData = AssetDataList[i];
		TSharedPtr<FJsonObject> SoundObj = MakeShared<FJsonObject>();
		SoundObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		SoundObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
		SoundObj->SetStringField(TEXT("class"), TEXT("SoundWave"));
		SoundsArray.Add(MakeShared<FJsonValueObject>(SoundObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("sounds"), SoundsArray);
	ResultObj->SetNumberField(TEXT("count"), SoundsArray.Num());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAudioHandler::HandleGetSound(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path);
	if (!Sound)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Sound not found: %s"), *Path));
		return false;
	}

	Result = MakeShared<FJsonValueObject>(SoundToJson(Sound));
	return true;
}

bool FUltimateControlAudioHandler::HandleListSoundCues(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path = TEXT("/Game");
	if (Params->HasField(TEXT("path")))
	{
		Path = Params->GetStringField(TEXT("path"));
	}

	int32 Limit = 500;
	if (Params->HasField(TEXT("limit")))
	{
		Limit = FMath::Clamp(FMath::RoundToInt(Params->GetNumberField(TEXT("limit"))), 1, 10000);
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> AssetDataList;
	FARFilter Filter;
	Filter.ClassPaths.Add(USoundCue::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(*Path));
	Filter.bRecursivePaths = true;

	AssetRegistry.GetAssets(Filter, AssetDataList);

	TArray<TSharedPtr<FJsonValue>> CuesArray;
	for (int32 i = 0; i < FMath::Min(AssetDataList.Num(), Limit); i++)
	{
		const FAssetData& AssetData = AssetDataList[i];
		TSharedPtr<FJsonObject> CueObj = MakeShared<FJsonObject>();
		CueObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		CueObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
		CuesArray.Add(MakeShared<FJsonValueObject>(CueObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("soundCues"), CuesArray);
	ResultObj->SetNumberField(TEXT("count"), CuesArray.Num());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAudioHandler::HandleListSoundMixes(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
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
	Filter.ClassPaths.Add(USoundMix::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(*Path));
	Filter.bRecursivePaths = true;

	AssetRegistry.GetAssets(Filter, AssetDataList);

	TArray<TSharedPtr<FJsonValue>> MixesArray;
	for (const FAssetData& AssetData : AssetDataList)
	{
		TSharedPtr<FJsonObject> MixObj = MakeShared<FJsonObject>();
		MixObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		MixObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
		MixesArray.Add(MakeShared<FJsonValueObject>(MixObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("soundMixes"), MixesArray);
	ResultObj->SetNumberField(TEXT("count"), MixesArray.Num());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAudioHandler::HandleListSoundClasses(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
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
	Filter.ClassPaths.Add(USoundClass::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(*Path));
	Filter.bRecursivePaths = true;

	AssetRegistry.GetAssets(Filter, AssetDataList);

	TArray<TSharedPtr<FJsonValue>> ClassesArray;
	for (const FAssetData& AssetData : AssetDataList)
	{
		TSharedPtr<FJsonObject> ClassObj = MakeShared<FJsonObject>();
		ClassObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		ClassObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
		ClassesArray.Add(MakeShared<FJsonValueObject>(ClassObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("soundClasses"), ClassesArray);
	ResultObj->SetNumberField(TEXT("count"), ClassesArray.Num());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAudioHandler::HandlePlaySound2D(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString SoundPath;
	if (!RequireString(Params, TEXT("sound"), SoundPath, Error))
	{
		return false;
	}

	USoundBase* Sound = LoadObject<USoundBase>(nullptr, *SoundPath);
	if (!Sound)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Sound not found: %s"), *SoundPath));
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	float VolumeMultiplier = 1.0f;
	if (Params->HasField(TEXT("volume")))
	{
		VolumeMultiplier = Params->GetNumberField(TEXT("volume"));
	}

	float PitchMultiplier = 1.0f;
	if (Params->HasField(TEXT("pitch")))
	{
		PitchMultiplier = Params->GetNumberField(TEXT("pitch"));
	}

	UAudioComponent* AudioComponent = UGameplayStatics::SpawnSound2D(World, Sound, VolumeMultiplier, PitchMultiplier);

	if (AudioComponent)
	{
		int32 ComponentId = NextAudioComponentId++;
		ActiveAudioComponents.Add(ComponentId, AudioComponent);

		TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
		ResultObj->SetBoolField(TEXT("success"), true);
		ResultObj->SetNumberField(TEXT("componentId"), ComponentId);
		ResultObj->SetNumberField(TEXT("duration"), Sound->Duration);
		Result = MakeShared<FJsonValueObject>(ResultObj);
		return true;
	}

	Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Failed to play sound"));
	return false;
}

bool FUltimateControlAudioHandler::HandlePlaySoundAtLocation(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString SoundPath;
	if (!RequireString(Params, TEXT("sound"), SoundPath, Error))
	{
		return false;
	}

	if (!Params->HasField(TEXT("location")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: location"));
		return false;
	}
	FVector Location = JsonToVector(Params->GetObjectField(TEXT("location")));

	USoundBase* Sound = LoadObject<USoundBase>(nullptr, *SoundPath);
	if (!Sound)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Sound not found: %s"), *SoundPath));
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	float VolumeMultiplier = 1.0f;
	if (Params->HasField(TEXT("volume")))
	{
		VolumeMultiplier = Params->GetNumberField(TEXT("volume"));
	}

	float PitchMultiplier = 1.0f;
	if (Params->HasField(TEXT("pitch")))
	{
		PitchMultiplier = Params->GetNumberField(TEXT("pitch"));
	}

	UAudioComponent* AudioComponent = UGameplayStatics::SpawnSoundAtLocation(World, Sound, Location, FRotator::ZeroRotator, VolumeMultiplier, PitchMultiplier);

	if (AudioComponent)
	{
		int32 ComponentId = NextAudioComponentId++;
		ActiveAudioComponents.Add(ComponentId, AudioComponent);

		TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
		ResultObj->SetBoolField(TEXT("success"), true);
		ResultObj->SetNumberField(TEXT("componentId"), ComponentId);
		Result = MakeShared<FJsonValueObject>(ResultObj);
		return true;
	}

	Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Failed to play sound"));
	return false;
}

bool FUltimateControlAudioHandler::HandlePlaySoundAttached(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString SoundPath;
	if (!RequireString(Params, TEXT("sound"), SoundPath, Error))
	{
		return false;
	}

	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	USoundBase* Sound = LoadObject<USoundBase>(nullptr, *SoundPath);
	if (!Sound)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Sound not found: %s"), *SoundPath));
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	// Find actor by label using TActorIterator
	AActor* Actor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetActorLabel() == ActorName || It->GetName() == ActorName)
		{
			Actor = *It;
			break;
		}
	}
	if (!Actor)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Actor not found: %s"), *ActorName));
		return false;
	}

	float VolumeMultiplier = 1.0f;
	if (Params->HasField(TEXT("volume")))
	{
		VolumeMultiplier = Params->GetNumberField(TEXT("volume"));
	}

	float PitchMultiplier = 1.0f;
	if (Params->HasField(TEXT("pitch")))
	{
		PitchMultiplier = Params->GetNumberField(TEXT("pitch"));
	}

	UAudioComponent* AudioComponent = UGameplayStatics::SpawnSoundAttached(Sound, Actor->GetRootComponent(), NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, false, VolumeMultiplier, PitchMultiplier);

	if (AudioComponent)
	{
		int32 ComponentId = NextAudioComponentId++;
		ActiveAudioComponents.Add(ComponentId, AudioComponent);

		TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
		ResultObj->SetBoolField(TEXT("success"), true);
		ResultObj->SetNumberField(TEXT("componentId"), ComponentId);
		Result = MakeShared<FJsonValueObject>(ResultObj);
		return true;
	}

	Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Failed to play sound"));
	return false;
}

bool FUltimateControlAudioHandler::HandleStopSound(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (!Params->HasField(TEXT("componentId")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: componentId"));
		return false;
	}
	int32 ComponentId = FMath::RoundToInt(Params->GetNumberField(TEXT("componentId")));

	TWeakObjectPtr<UAudioComponent>* ComponentPtr = ActiveAudioComponents.Find(ComponentId);
	if (!ComponentPtr || !ComponentPtr->IsValid())
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, TEXT("Audio component not found or expired"));
		return false;
	}

	UAudioComponent* AudioComponent = ComponentPtr->Get();
	AudioComponent->Stop();
	ActiveAudioComponents.Remove(ComponentId);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAudioHandler::HandleStopAllSounds(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	int32 StoppedCount = 0;
	for (auto& Pair : ActiveAudioComponents)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->Stop();
			StoppedCount++;
		}
	}
	ActiveAudioComponents.Empty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetNumberField(TEXT("stoppedCount"), StoppedCount);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAudioHandler::HandleGetAudioComponents(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TArray<TSharedPtr<FJsonValue>> ComponentsArray;

	for (auto& Pair : ActiveAudioComponents)
	{
		if (Pair.Value.IsValid())
		{
			TSharedPtr<FJsonObject> CompObj = AudioComponentToJson(Pair.Value.Get());
			CompObj->SetNumberField(TEXT("componentId"), Pair.Key);
			ComponentsArray.Add(MakeShared<FJsonValueObject>(CompObj));
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("components"), ComponentsArray);
	ResultObj->SetNumberField(TEXT("count"), ComponentsArray.Num());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAudioHandler::HandleSetAudioComponentVolume(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (!Params->HasField(TEXT("componentId")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: componentId"));
		return false;
	}
	int32 ComponentId = FMath::RoundToInt(Params->GetNumberField(TEXT("componentId")));

	if (!Params->HasField(TEXT("volume")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: volume"));
		return false;
	}
	float Volume = Params->GetNumberField(TEXT("volume"));

	TWeakObjectPtr<UAudioComponent>* ComponentPtr = ActiveAudioComponents.Find(ComponentId);
	if (!ComponentPtr || !ComponentPtr->IsValid())
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, TEXT("Audio component not found or expired"));
		return false;
	}

	ComponentPtr->Get()->SetVolumeMultiplier(Volume);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAudioHandler::HandleSetAudioComponentPitch(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (!Params->HasField(TEXT("componentId")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: componentId"));
		return false;
	}
	int32 ComponentId = FMath::RoundToInt(Params->GetNumberField(TEXT("componentId")));

	if (!Params->HasField(TEXT("pitch")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: pitch"));
		return false;
	}
	float Pitch = Params->GetNumberField(TEXT("pitch"));

	TWeakObjectPtr<UAudioComponent>* ComponentPtr = ActiveAudioComponents.Find(ComponentId);
	if (!ComponentPtr || !ComponentPtr->IsValid())
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, TEXT("Audio component not found or expired"));
		return false;
	}

	ComponentPtr->Get()->SetPitchMultiplier(Pitch);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAudioHandler::HandleFadeAudioComponent(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	if (!Params->HasField(TEXT("componentId")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: componentId"));
		return false;
	}
	int32 ComponentId = FMath::RoundToInt(Params->GetNumberField(TEXT("componentId")));

	if (!Params->HasField(TEXT("targetVolume")) || !Params->HasField(TEXT("duration")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameters: targetVolume, duration"));
		return false;
	}
	float TargetVolume = Params->GetNumberField(TEXT("targetVolume"));
	float Duration = Params->GetNumberField(TEXT("duration"));

	TWeakObjectPtr<UAudioComponent>* ComponentPtr = ActiveAudioComponents.Find(ComponentId);
	if (!ComponentPtr || !ComponentPtr->IsValid())
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, TEXT("Audio component not found or expired"));
		return false;
	}

	EAudioFaderCurve FadeCurve = EAudioFaderCurve::Linear;
	ComponentPtr->Get()->FadeIn(Duration, TargetVolume, 0.0f, FadeCurve);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAudioHandler::HandleGetMasterVolume(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	// Master volume is typically controlled via the audio device
	// This is a simplified implementation
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetNumberField(TEXT("masterVolume"), 1.0f);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAudioHandler::HandleSetMasterVolume(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	// Setting master volume typically requires platform-specific audio API
	Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Setting master volume via API not fully supported. Use Sound Mix instead."));
	return false;
}

bool FUltimateControlAudioHandler::HandleMuteAll(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	// This would use the audio device to mute
	// Simplified implementation - stop all active components
	for (auto& Pair : ActiveAudioComponents)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->SetPaused(true);
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAudioHandler::HandleUnmuteAll(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	for (auto& Pair : ActiveAudioComponents)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->SetPaused(false);
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAudioHandler::HandlePushSoundMix(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString MixPath;
	if (!RequireString(Params, TEXT("mix"), MixPath, Error))
	{
		return false;
	}

	USoundMix* SoundMix = LoadObject<USoundMix>(nullptr, *MixPath);
	if (!SoundMix)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Sound mix not found: %s"), *MixPath));
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	UGameplayStatics::PushSoundMixModifier(World, SoundMix);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAudioHandler::HandlePopSoundMix(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString MixPath;
	if (!RequireString(Params, TEXT("mix"), MixPath, Error))
	{
		return false;
	}

	USoundMix* SoundMix = LoadObject<USoundMix>(nullptr, *MixPath);
	if (!SoundMix)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Sound mix not found: %s"), *MixPath));
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	UGameplayStatics::PopSoundMixModifier(World, SoundMix);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAudioHandler::HandleClearSoundMixes(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	UGameplayStatics::ClearSoundMixModifiers(World);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAudioHandler::HandleSetSoundMixClassOverride(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString MixPath;
	if (!RequireString(Params, TEXT("mix"), MixPath, Error))
	{
		return false;
	}

	FString ClassPath;
	if (!RequireString(Params, TEXT("soundClass"), ClassPath, Error))
	{
		return false;
	}

	USoundMix* SoundMix = LoadObject<USoundMix>(nullptr, *MixPath);
	if (!SoundMix)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Sound mix not found: %s"), *MixPath));
		return false;
	}

	USoundClass* SoundClass = LoadObject<USoundClass>(nullptr, *ClassPath);
	if (!SoundClass)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Sound class not found: %s"), *ClassPath));
		return false;
	}

	float Volume = 1.0f;
	if (Params->HasField(TEXT("volume")))
	{
		Volume = Params->GetNumberField(TEXT("volume"));
	}

	float Pitch = 1.0f;
	if (Params->HasField(TEXT("pitch")))
	{
		Pitch = Params->GetNumberField(TEXT("pitch"));
	}

	float FadeTime = 0.0f;
	if (Params->HasField(TEXT("fadeTime")))
	{
		FadeTime = Params->GetNumberField(TEXT("fadeTime"));
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No world loaded"));
		return false;
	}

	UGameplayStatics::SetSoundMixClassOverride(World, SoundMix, SoundClass, Volume, Pitch, FadeTime, true);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAudioHandler::HandleGetSoundClassVolume(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ClassPath;
	if (!RequireString(Params, TEXT("soundClass"), ClassPath, Error))
	{
		return false;
	}

	USoundClass* SoundClass = LoadObject<USoundClass>(nullptr, *ClassPath);
	if (!SoundClass)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Sound class not found: %s"), *ClassPath));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetNumberField(TEXT("volume"), SoundClass->Properties.Volume);
	ResultObj->SetNumberField(TEXT("pitch"), SoundClass->Properties.Pitch);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAudioHandler::HandleSetSoundClassVolume(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	// Setting sound class volume requires modifying the asset which may not be desired
	Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Modifying sound class properties directly not recommended. Use Sound Mix instead."));
	return false;
}

bool FUltimateControlAudioHandler::HandleGetActiveAudioDevices(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TArray<TSharedPtr<FJsonValue>> DevicesArray;

	// Get audio device info
	if (FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get())
	{
		TSharedPtr<FJsonObject> DeviceObj = MakeShared<FJsonObject>();
		DeviceObj->SetNumberField(TEXT("numActiveAudioDevices"), AudioDeviceManager->GetNumActiveAudioDevices());
		DevicesArray.Add(MakeShared<FJsonValueObject>(DeviceObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("devices"), DevicesArray);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAudioHandler::HandleGetAudioStats(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetNumberField(TEXT("activeComponentCount"), ActiveAudioComponents.Num());

	// Additional audio stats could be gathered from the audio device
	if (FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get())
	{
		ResultObj->SetNumberField(TEXT("numActiveAudioDevices"), AudioDeviceManager->GetNumActiveAudioDevices());
	}

	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}
