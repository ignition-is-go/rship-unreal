// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/UltimateControlSequencerHandler.h"
#include "LevelSequence/LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieSceneSection.h"
#include "MovieSceneSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ISequencer.h"
#include "LevelEditor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Toolkits/AssetEditorManager.h"
#include "EngineUtils.h"
#include "Factories/LevelSequenceFactoryNew.h"
#include "AssetToolsModule.h"

void FUltimateControlSequencerHandler::RegisterMethods(TMap<FString, FJsonRpcMethodHandler>& Methods)
{
	Methods.Add(TEXT("sequencer.list"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandleListSequences));
	Methods.Add(TEXT("sequencer.get"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandleGetSequence));
	Methods.Add(TEXT("sequencer.create"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandleCreateSequence));
	Methods.Add(TEXT("sequencer.play"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandlePlaySequence));
	Methods.Add(TEXT("sequencer.stop"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandleStopSequence));
	Methods.Add(TEXT("sequencer.pause"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandlePauseSequence));
	Methods.Add(TEXT("sequencer.scrub"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandleScrubSequence));
	Methods.Add(TEXT("sequencer.getPlaybackState"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandleGetPlaybackState));
	Methods.Add(TEXT("sequencer.getCurrentTime"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandleGetCurrentTime));
	Methods.Add(TEXT("sequencer.setCurrentTime"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandleSetCurrentTime));
	Methods.Add(TEXT("sequencer.getPlaybackRate"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandleGetPlaybackRate));
	Methods.Add(TEXT("sequencer.setPlaybackRate"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandleSetPlaybackRate));
	Methods.Add(TEXT("sequencer.getLength"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandleGetSequenceLength));
	Methods.Add(TEXT("sequencer.getFrameRate"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandleGetFrameRate));
	Methods.Add(TEXT("sequencer.setFrameRate"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandleSetFrameRate));
	Methods.Add(TEXT("sequencer.getPlaybackRange"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandleGetPlaybackRange));
	Methods.Add(TEXT("sequencer.setPlaybackRange"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandleSetPlaybackRange));
	Methods.Add(TEXT("sequencer.getTracks"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandleGetTracks));
	Methods.Add(TEXT("sequencer.addTrack"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandleAddTrack));
	Methods.Add(TEXT("sequencer.removeTrack"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandleRemoveTrack));
	Methods.Add(TEXT("sequencer.getBindings"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandleGetBindings));
	Methods.Add(TEXT("sequencer.addBinding"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandleAddBinding));
	Methods.Add(TEXT("sequencer.removeBinding"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandleRemoveBinding));
	Methods.Add(TEXT("sequencer.open"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandleOpenSequence));
	Methods.Add(TEXT("sequencer.close"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandleCloseSequence));
	Methods.Add(TEXT("sequencer.getOpen"), FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlSequencerHandler::HandleGetOpenSequence));
}

TSharedPtr<FJsonObject> FUltimateControlSequencerHandler::SequenceToJson(ULevelSequence* Sequence)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	Result->SetStringField(TEXT("name"), Sequence->GetName());
	Result->SetStringField(TEXT("path"), Sequence->GetPathName());

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (MovieScene)
	{
		Result->SetNumberField(TEXT("duration"), MovieScene->GetPlaybackRange().Size<FFrameNumber>().Value / MovieScene->GetTickResolution().AsDecimal());
		Result->SetNumberField(TEXT("frameRate"), MovieScene->GetDisplayRate().AsDecimal());

		TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();
		TSharedPtr<FJsonObject> RangeObj = MakeShared<FJsonObject>();
		RangeObj->SetNumberField(TEXT("start"), PlaybackRange.GetLowerBoundValue().Value / MovieScene->GetTickResolution().AsDecimal());
		RangeObj->SetNumberField(TEXT("end"), PlaybackRange.GetUpperBoundValue().Value / MovieScene->GetTickResolution().AsDecimal());
		Result->SetObjectField(TEXT("playbackRange"), RangeObj);

		Result->SetNumberField(TEXT("trackCount"), MovieScene->GetTracks().Num());
	}

	return Result;
}

TSharedPtr<FJsonObject> FUltimateControlSequencerHandler::TrackToJson(UMovieSceneTrack* Track)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	Result->SetStringField(TEXT("name"), Track->GetDisplayName().ToString());
	Result->SetStringField(TEXT("class"), Track->GetClass()->GetName());
	Result->SetNumberField(TEXT("sectionCount"), Track->GetAllSections().Num());
	Result->SetBoolField(TEXT("isMuted"), Track->IsEvalDisabled());

	return Result;
}

ALevelSequenceActor* FUltimateControlSequencerHandler::FindSequenceActor(ULevelSequence* Sequence)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return nullptr;

	for (TActorIterator<ALevelSequenceActor> It(World); It; ++It)
	{
		ALevelSequenceActor* Actor = *It;
		if (Actor->GetSequence() == Sequence)
		{
			return Actor;
		}
	}

	return nullptr;
}

bool FUltimateControlSequencerHandler::HandleListSequences(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
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
	Filter.ClassPaths.Add(ULevelSequence::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(*Path));
	Filter.bRecursivePaths = true;

	AssetRegistry.GetAssets(Filter, AssetDataList);

	TArray<TSharedPtr<FJsonValue>> SequencesArray;
	for (int32 i = 0; i < FMath::Min(AssetDataList.Num(), Limit); i++)
	{
		const FAssetData& AssetData = AssetDataList[i];
		TSharedPtr<FJsonObject> SeqObj = MakeShared<FJsonObject>();
		SeqObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		SeqObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
		SequencesArray.Add(MakeShared<FJsonValueObject>(SeqObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("sequences"), SequencesArray);
	ResultObj->SetNumberField(TEXT("count"), SequencesArray.Num());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlSequencerHandler::HandleGetSequence(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *Path);
	if (!Sequence)
	{
		Error = CreateError(-32003, FString::Printf(TEXT("Sequence not found: %s"), *Path));
		return false;
	}

	Result = MakeShared<FJsonValueObject>(SequenceToJson(Sequence));
	return true;
}

bool FUltimateControlSequencerHandler::HandleCreateSequence(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	FString PackagePath = FPackageName::GetLongPackagePath(Path);
	FString AssetName = FPackageName::GetShortName(Path);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	ULevelSequenceFactoryNew* Factory = NewObject<ULevelSequenceFactoryNew>();
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, ULevelSequence::StaticClass(), Factory);

	if (!NewAsset)
	{
		Error = CreateError(-32002, FString::Printf(TEXT("Failed to create sequence at: %s"), *Path));
		return false;
	}

	ULevelSequence* NewSequence = Cast<ULevelSequence>(NewAsset);
	Result = MakeShared<FJsonValueObject>(SequenceToJson(NewSequence));
	return true;
}

bool FUltimateControlSequencerHandler::HandlePlaySequence(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *Path);
	if (!Sequence)
	{
		Error = CreateError(-32003, FString::Printf(TEXT("Sequence not found: %s"), *Path));
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Error = CreateError(-32002, TEXT("No world loaded"));
		return false;
	}

	// Find or create a sequence actor
	ALevelSequenceActor* SequenceActor = FindSequenceActor(Sequence);
	if (!SequenceActor)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SequenceActor = World->SpawnActor<ALevelSequenceActor>(SpawnParams);
		SequenceActor->SetSequence(Sequence);
	}

	// Get or create player
	ULevelSequencePlayer* Player = SequenceActor->GetSequencePlayer();
	if (!Player)
	{
		Error = CreateError(-32002, TEXT("Failed to get sequence player"));
		return false;
	}

	float PlayRate = 1.0f;
	if (Params->HasField(TEXT("playRate")))
	{
		PlayRate = Params->GetNumberField(TEXT("playRate"));
		Player->SetPlayRate(PlayRate);
	}

	Player->Play();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlSequencerHandler::HandleStopSequence(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *Path);
	if (!Sequence)
	{
		Error = CreateError(-32003, FString::Printf(TEXT("Sequence not found: %s"), *Path));
		return false;
	}

	ALevelSequenceActor* SequenceActor = FindSequenceActor(Sequence);
	if (SequenceActor)
	{
		ULevelSequencePlayer* Player = SequenceActor->GetSequencePlayer();
		if (Player)
		{
			Player->Stop();
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlSequencerHandler::HandlePauseSequence(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *Path);
	if (!Sequence)
	{
		Error = CreateError(-32003, FString::Printf(TEXT("Sequence not found: %s"), *Path));
		return false;
	}

	ALevelSequenceActor* SequenceActor = FindSequenceActor(Sequence);
	if (SequenceActor)
	{
		ULevelSequencePlayer* Player = SequenceActor->GetSequencePlayer();
		if (Player)
		{
			Player->Pause();
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlSequencerHandler::HandleScrubSequence(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	if (!Params->HasField(TEXT("time")))
	{
		Error = CreateError(-32602, TEXT("Missing required parameter: time"));
		return false;
	}
	float Time = Params->GetNumberField(TEXT("time"));

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *Path);
	if (!Sequence)
	{
		Error = CreateError(-32003, FString::Printf(TEXT("Sequence not found: %s"), *Path));
		return false;
	}

	ALevelSequenceActor* SequenceActor = FindSequenceActor(Sequence);
	if (SequenceActor)
	{
		ULevelSequencePlayer* Player = SequenceActor->GetSequencePlayer();
		if (Player)
		{
			Player->ScrubToTime(FTimespan::FromSeconds(Time));
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlSequencerHandler::HandleGetPlaybackState(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *Path);
	if (!Sequence)
	{
		Error = CreateError(-32003, FString::Printf(TEXT("Sequence not found: %s"), *Path));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();

	ALevelSequenceActor* SequenceActor = FindSequenceActor(Sequence);
	if (SequenceActor)
	{
		ULevelSequencePlayer* Player = SequenceActor->GetSequencePlayer();
		if (Player)
		{
			ResultObj->SetBoolField(TEXT("isPlaying"), Player->IsPlaying());
			ResultObj->SetBoolField(TEXT("isPaused"), Player->IsPaused());
			ResultObj->SetBoolField(TEXT("isReversed"), Player->IsReversed());
			ResultObj->SetNumberField(TEXT("currentTime"), Player->GetCurrentTime().AsSeconds());
			ResultObj->SetNumberField(TEXT("duration"), Player->GetDuration().AsSeconds());
			ResultObj->SetNumberField(TEXT("playRate"), Player->GetPlayRate());
		}
		else
		{
			ResultObj->SetBoolField(TEXT("isPlaying"), false);
		}
	}
	else
	{
		ResultObj->SetBoolField(TEXT("isPlaying"), false);
		ResultObj->SetStringField(TEXT("note"), TEXT("No sequence actor in world"));
	}

	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlSequencerHandler::HandleGetCurrentTime(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *Path);
	if (!Sequence)
	{
		Error = CreateError(-32003, FString::Printf(TEXT("Sequence not found: %s"), *Path));
		return false;
	}

	ALevelSequenceActor* SequenceActor = FindSequenceActor(Sequence);
	if (SequenceActor)
	{
		ULevelSequencePlayer* Player = SequenceActor->GetSequencePlayer();
		if (Player)
		{
			TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
			ResultObj->SetNumberField(TEXT("time"), Player->GetCurrentTime().AsSeconds());
			Result = MakeShared<FJsonValueObject>(ResultObj);
			return true;
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetNumberField(TEXT("time"), 0.0f);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlSequencerHandler::HandleSetCurrentTime(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	return HandleScrubSequence(Params, Result, Error);
}

bool FUltimateControlSequencerHandler::HandleGetPlaybackRate(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *Path);
	if (!Sequence)
	{
		Error = CreateError(-32003, FString::Printf(TEXT("Sequence not found: %s"), *Path));
		return false;
	}

	ALevelSequenceActor* SequenceActor = FindSequenceActor(Sequence);
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();

	if (SequenceActor)
	{
		ULevelSequencePlayer* Player = SequenceActor->GetSequencePlayer();
		if (Player)
		{
			ResultObj->SetNumberField(TEXT("playRate"), Player->GetPlayRate());
			Result = MakeShared<FJsonValueObject>(ResultObj);
			return true;
		}
	}

	ResultObj->SetNumberField(TEXT("playRate"), 1.0f);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlSequencerHandler::HandleSetPlaybackRate(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	if (!Params->HasField(TEXT("rate")))
	{
		Error = CreateError(-32602, TEXT("Missing required parameter: rate"));
		return false;
	}
	float Rate = Params->GetNumberField(TEXT("rate"));

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *Path);
	if (!Sequence)
	{
		Error = CreateError(-32003, FString::Printf(TEXT("Sequence not found: %s"), *Path));
		return false;
	}

	ALevelSequenceActor* SequenceActor = FindSequenceActor(Sequence);
	if (SequenceActor)
	{
		ULevelSequencePlayer* Player = SequenceActor->GetSequencePlayer();
		if (Player)
		{
			Player->SetPlayRate(Rate);
		}
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlSequencerHandler::HandleGetSequenceLength(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *Path);
	if (!Sequence)
	{
		Error = CreateError(-32003, FString::Printf(TEXT("Sequence not found: %s"), *Path));
		return false;
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		Error = CreateError(-32002, TEXT("No movie scene"));
		return false;
	}

	float Duration = MovieScene->GetPlaybackRange().Size<FFrameNumber>().Value / MovieScene->GetTickResolution().AsDecimal();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetNumberField(TEXT("duration"), Duration);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlSequencerHandler::HandleGetFrameRate(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *Path);
	if (!Sequence)
	{
		Error = CreateError(-32003, FString::Printf(TEXT("Sequence not found: %s"), *Path));
		return false;
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		Error = CreateError(-32002, TEXT("No movie scene"));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetNumberField(TEXT("displayRate"), MovieScene->GetDisplayRate().AsDecimal());
	ResultObj->SetNumberField(TEXT("tickResolution"), MovieScene->GetTickResolution().AsDecimal());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlSequencerHandler::HandleSetFrameRate(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	if (!Params->HasField(TEXT("frameRate")))
	{
		Error = CreateError(-32602, TEXT("Missing required parameter: frameRate"));
		return false;
	}
	float FrameRate = Params->GetNumberField(TEXT("frameRate"));

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *Path);
	if (!Sequence)
	{
		Error = CreateError(-32003, FString::Printf(TEXT("Sequence not found: %s"), *Path));
		return false;
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		Error = CreateError(-32002, TEXT("No movie scene"));
		return false;
	}

	MovieScene->SetDisplayRate(FFrameRate(FMath::RoundToInt(FrameRate), 1));
	Sequence->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlSequencerHandler::HandleGetPlaybackRange(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *Path);
	if (!Sequence)
	{
		Error = CreateError(-32003, FString::Printf(TEXT("Sequence not found: %s"), *Path));
		return false;
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		Error = CreateError(-32002, TEXT("No movie scene"));
		return false;
	}

	TRange<FFrameNumber> Range = MovieScene->GetPlaybackRange();
	double TickResolution = MovieScene->GetTickResolution().AsDecimal();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetNumberField(TEXT("start"), Range.GetLowerBoundValue().Value / TickResolution);
	ResultObj->SetNumberField(TEXT("end"), Range.GetUpperBoundValue().Value / TickResolution);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlSequencerHandler::HandleSetPlaybackRange(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	if (!Params->HasField(TEXT("start")) || !Params->HasField(TEXT("end")))
	{
		Error = CreateError(-32602, TEXT("Missing required parameters: start and end"));
		return false;
	}

	float Start = Params->GetNumberField(TEXT("start"));
	float End = Params->GetNumberField(TEXT("end"));

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *Path);
	if (!Sequence)
	{
		Error = CreateError(-32003, FString::Printf(TEXT("Sequence not found: %s"), *Path));
		return false;
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		Error = CreateError(-32002, TEXT("No movie scene"));
		return false;
	}

	double TickResolution = MovieScene->GetTickResolution().AsDecimal();
	FFrameNumber StartFrame = FFrameNumber(FMath::RoundToInt(Start * TickResolution));
	FFrameNumber EndFrame = FFrameNumber(FMath::RoundToInt(End * TickResolution));

	MovieScene->SetPlaybackRange(TRange<FFrameNumber>(StartFrame, EndFrame));
	Sequence->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlSequencerHandler::HandleGetTracks(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *Path);
	if (!Sequence)
	{
		Error = CreateError(-32003, FString::Printf(TEXT("Sequence not found: %s"), *Path));
		return false;
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		Error = CreateError(-32002, TEXT("No movie scene"));
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> TracksArray;
	for (UMovieSceneTrack* Track : MovieScene->GetTracks())
	{
		TracksArray.Add(MakeShared<FJsonValueObject>(TrackToJson(Track)));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("tracks"), TracksArray);
	ResultObj->SetNumberField(TEXT("count"), TracksArray.Num());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlSequencerHandler::HandleAddTrack(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	// Adding tracks requires knowing the track type and potentially the binding
	Error = CreateError(-32002, TEXT("Adding tracks via API requires specific track type. Use the Sequencer editor."));
	return false;
}

bool FUltimateControlSequencerHandler::HandleRemoveTrack(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	Error = CreateError(-32002, TEXT("Removing tracks via API not fully implemented. Use the Sequencer editor."));
	return false;
}

bool FUltimateControlSequencerHandler::HandleGetBindings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *Path);
	if (!Sequence)
	{
		Error = CreateError(-32003, FString::Printf(TEXT("Sequence not found: %s"), *Path));
		return false;
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		Error = CreateError(-32002, TEXT("No movie scene"));
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> BindingsArray;

	for (int32 i = 0; i < MovieScene->GetPossessableCount(); i++)
	{
		const FMovieScenePossessable& Possessable = MovieScene->GetPossessable(i);
		TSharedPtr<FJsonObject> BindingObj = MakeShared<FJsonObject>();
		BindingObj->SetStringField(TEXT("name"), Possessable.GetName());
		BindingObj->SetStringField(TEXT("guid"), Possessable.GetGuid().ToString());
		BindingObj->SetStringField(TEXT("type"), TEXT("Possessable"));
		BindingsArray.Add(MakeShared<FJsonValueObject>(BindingObj));
	}

	for (int32 i = 0; i < MovieScene->GetSpawnableCount(); i++)
	{
		const FMovieSceneSpawnable& Spawnable = MovieScene->GetSpawnable(i);
		TSharedPtr<FJsonObject> BindingObj = MakeShared<FJsonObject>();
		BindingObj->SetStringField(TEXT("name"), Spawnable.GetName());
		BindingObj->SetStringField(TEXT("guid"), Spawnable.GetGuid().ToString());
		BindingObj->SetStringField(TEXT("type"), TEXT("Spawnable"));
		BindingsArray.Add(MakeShared<FJsonValueObject>(BindingObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("bindings"), BindingsArray);
	ResultObj->SetNumberField(TEXT("count"), BindingsArray.Num());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlSequencerHandler::HandleAddBinding(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	Error = CreateError(-32002, TEXT("Adding bindings via API not fully implemented. Use the Sequencer editor."));
	return false;
}

bool FUltimateControlSequencerHandler::HandleRemoveBinding(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	Error = CreateError(-32002, TEXT("Removing bindings via API not fully implemented. Use the Sequencer editor."));
	return false;
}

bool FUltimateControlSequencerHandler::HandleOpenSequence(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *Path);
	if (!Sequence)
	{
		Error = CreateError(-32003, FString::Printf(TEXT("Sequence not found: %s"), *Path));
		return false;
	}

	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Sequence);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlSequencerHandler::HandleCloseSequence(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *Path);
	if (!Sequence)
	{
		Error = CreateError(-32003, FString::Printf(TEXT("Sequence not found: %s"), *Path));
		return false;
	}

	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(Sequence);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlSequencerHandler::HandleGetOpenSequence(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	// Get currently focused sequence in sequencer
	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("note"), TEXT("Getting the currently focused sequence requires ISequencer interface access"));
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}
