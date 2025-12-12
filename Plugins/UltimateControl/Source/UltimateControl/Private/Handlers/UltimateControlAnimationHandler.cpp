// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/UltimateControlAnimationHandler.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GameFramework/Character.h"
#include "EngineUtils.h"

FUltimateControlAnimationHandler::FUltimateControlAnimationHandler(UUltimateControlSubsystem* InSubsystem)
	: FUltimateControlHandlerBase(InSubsystem)
{
	RegisterMethod(TEXT("animation.list"), TEXT("List animation sequences"), TEXT("Animation"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAnimationHandler::HandleListAnimations));
	RegisterMethod(TEXT("animation.get"), TEXT("Get animation details"), TEXT("Animation"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAnimationHandler::HandleGetAnimation));
	RegisterMethod(TEXT("animation.listMontages"), TEXT("List animation montages"), TEXT("Animation"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAnimationHandler::HandleListAnimMontages));
	RegisterMethod(TEXT("animation.listBlueprints"), TEXT("List animation blueprints"), TEXT("Animation"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAnimationHandler::HandleListAnimBlueprints));
	RegisterMethod(TEXT("animation.play"), TEXT("Play animation on actor"), TEXT("Animation"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAnimationHandler::HandlePlayAnimation));
	RegisterMethod(TEXT("animation.stop"), TEXT("Stop animation playback"), TEXT("Animation"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAnimationHandler::HandleStopAnimation));
	RegisterMethod(TEXT("animation.pause"), TEXT("Pause animation playback"), TEXT("Animation"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAnimationHandler::HandlePauseAnimation));
	RegisterMethod(TEXT("animation.resume"), TEXT("Resume animation playback"), TEXT("Animation"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAnimationHandler::HandleResumeAnimation));
	RegisterMethod(TEXT("animation.getPosition"), TEXT("Get playback position"), TEXT("Animation"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAnimationHandler::HandleGetPlaybackPosition));
	RegisterMethod(TEXT("animation.setPosition"), TEXT("Set playback position"), TEXT("Animation"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAnimationHandler::HandleSetPlaybackPosition));
	RegisterMethod(TEXT("animation.getRate"), TEXT("Get playback rate"), TEXT("Animation"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAnimationHandler::HandleGetPlaybackRate));
	RegisterMethod(TEXT("animation.setRate"), TEXT("Set playback rate"), TEXT("Animation"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAnimationHandler::HandleSetPlaybackRate));
	RegisterMethod(TEXT("animation.playMontage"), TEXT("Play animation montage"), TEXT("Animation"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAnimationHandler::HandlePlayMontage));
	RegisterMethod(TEXT("animation.stopMontage"), TEXT("Stop animation montage"), TEXT("Animation"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAnimationHandler::HandleStopMontage));
	RegisterMethod(TEXT("animation.jumpToSection"), TEXT("Jump to montage section"), TEXT("Animation"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAnimationHandler::HandleJumpToMontageSection));
	RegisterMethod(TEXT("animation.getMontagePosition"), TEXT("Get montage playback position"), TEXT("Animation"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAnimationHandler::HandleGetMontagePosition));
	RegisterMethod(TEXT("animation.getAnimBlueprintVariables"), TEXT("Get anim blueprint variables"), TEXT("Animation"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAnimationHandler::HandleGetAnimBlueprintVariables));
	RegisterMethod(TEXT("animation.setAnimBlueprintVariable"), TEXT("Set anim blueprint variable"), TEXT("Animation"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAnimationHandler::HandleSetAnimBlueprintVariable));
	RegisterMethod(TEXT("animation.getSkeleton"), TEXT("Get skeleton bone hierarchy"), TEXT("Animation"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAnimationHandler::HandleGetSkeleton));
	RegisterMethod(TEXT("animation.getBoneTransform"), TEXT("Get bone transform"), TEXT("Animation"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAnimationHandler::HandleGetBoneTransform));
	RegisterMethod(TEXT("animation.setBoneTransform"), TEXT("Set bone transform"), TEXT("Animation"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAnimationHandler::HandleSetBoneTransform));
}

USkeletalMeshComponent* FUltimateControlAnimationHandler::GetSkeletalMeshComponent(const FString& ActorName)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return nullptr;

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
	if (!Actor) return nullptr;

	// Try to get skeletal mesh component
	USkeletalMeshComponent* SkeletalMesh = Actor->FindComponentByClass<USkeletalMeshComponent>();
	if (SkeletalMesh) return SkeletalMesh;

	// For characters, get the mesh
	if (ACharacter* Character = Cast<ACharacter>(Actor))
	{
		return Character->GetMesh();
	}

	return nullptr;
}

TSharedPtr<FJsonObject> FUltimateControlAnimationHandler::AnimationToJson(UAnimationAsset* Animation)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	Result->SetStringField(TEXT("name"), Animation->GetName());
	Result->SetStringField(TEXT("path"), Animation->GetPathName());
	Result->SetStringField(TEXT("class"), Animation->GetClass()->GetName());

	if (Animation->GetSkeleton())
	{
		Result->SetStringField(TEXT("skeleton"), Animation->GetSkeleton()->GetPathName());
	}

	return Result;
}

TSharedPtr<FJsonObject> FUltimateControlAnimationHandler::AnimSequenceToJson(UAnimSequence* AnimSequence)
{
	TSharedPtr<FJsonObject> Result = AnimationToJson(AnimSequence);

	Result->SetNumberField(TEXT("duration"), AnimSequence->GetPlayLength());
	Result->SetNumberField(TEXT("frameRate"), AnimSequence->GetFrameRate().AsDecimal());
	Result->SetNumberField(TEXT("numFrames"), AnimSequence->GetNumberOfSampledKeys());
	Result->SetBoolField(TEXT("isLooping"), AnimSequence->bLoop);

	return Result;
}

TSharedPtr<FJsonObject> FUltimateControlAnimationHandler::AnimMontageToJson(UAnimMontage* Montage)
{
	TSharedPtr<FJsonObject> Result = AnimationToJson(Montage);

	Result->SetNumberField(TEXT("duration"), Montage->GetPlayLength());
	Result->SetNumberField(TEXT("blendInTime"), Montage->BlendIn.GetBlendTime());
	Result->SetNumberField(TEXT("blendOutTime"), Montage->BlendOut.GetBlendTime());

	// List sections
	TArray<TSharedPtr<FJsonValue>> SectionsArray;
	for (int32 i = 0; i < Montage->CompositeSections.Num(); i++)
	{
		const FCompositeSection& Section = Montage->CompositeSections[i];
		TSharedPtr<FJsonObject> SectionObj = MakeShared<FJsonObject>();
		SectionObj->SetStringField(TEXT("name"), Section.SectionName.ToString());
		SectionObj->SetNumberField(TEXT("startTime"), Section.GetTime());
		SectionsArray.Add(MakeShared<FJsonValueObject>(SectionObj));
	}
	Result->SetArrayField(TEXT("sections"), SectionsArray);

	return Result;
}

bool FUltimateControlAnimationHandler::HandleListAnimations(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
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
	Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(*Path));
	Filter.bRecursivePaths = true;

	AssetRegistry.GetAssets(Filter, AssetDataList);

	TArray<TSharedPtr<FJsonValue>> AnimsArray;
	for (int32 i = 0; i < FMath::Min(AssetDataList.Num(), Limit); i++)
	{
		const FAssetData& AssetData = AssetDataList[i];
		TSharedPtr<FJsonObject> AnimObj = MakeShared<FJsonObject>();
		AnimObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		AnimObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
		AnimObj->SetStringField(TEXT("class"), TEXT("AnimSequence"));
		AnimsArray.Add(MakeShared<FJsonValueObject>(AnimObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("animations"), AnimsArray);
	ResultObj->SetNumberField(TEXT("count"), AnimsArray.Num());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAnimationHandler::HandleGetAnimation(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, Error))
	{
		return false;
	}

	UAnimSequence* AnimSequence = LoadObject<UAnimSequence>(nullptr, *Path);
	if (AnimSequence)
	{
		Result = MakeShared<FJsonValueObject>(AnimSequenceToJson(AnimSequence));
		return true;
	}

	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *Path);
	if (Montage)
	{
		Result = MakeShared<FJsonValueObject>(AnimMontageToJson(Montage));
		return true;
	}

	Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Animation not found: %s"), *Path));
	return false;
}

bool FUltimateControlAnimationHandler::HandleListAnimMontages(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
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
	Filter.ClassPaths.Add(UAnimMontage::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(*Path));
	Filter.bRecursivePaths = true;

	AssetRegistry.GetAssets(Filter, AssetDataList);

	TArray<TSharedPtr<FJsonValue>> MontagesArray;
	for (int32 i = 0; i < FMath::Min(AssetDataList.Num(), Limit); i++)
	{
		const FAssetData& AssetData = AssetDataList[i];
		TSharedPtr<FJsonObject> MontageObj = MakeShared<FJsonObject>();
		MontageObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		MontageObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
		MontagesArray.Add(MakeShared<FJsonValueObject>(MontageObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("montages"), MontagesArray);
	ResultObj->SetNumberField(TEXT("count"), MontagesArray.Num());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAnimationHandler::HandleListAnimBlueprints(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
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
	Filter.ClassPaths.Add(UAnimBlueprint::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(*Path));
	Filter.bRecursivePaths = true;

	AssetRegistry.GetAssets(Filter, AssetDataList);

	TArray<TSharedPtr<FJsonValue>> BlueprintsArray;
	for (int32 i = 0; i < FMath::Min(AssetDataList.Num(), Limit); i++)
	{
		const FAssetData& AssetData = AssetDataList[i];
		TSharedPtr<FJsonObject> BPObj = MakeShared<FJsonObject>();
		BPObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
		BPObj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
		BlueprintsArray.Add(MakeShared<FJsonValueObject>(BPObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("animBlueprints"), BlueprintsArray);
	ResultObj->SetNumberField(TEXT("count"), BlueprintsArray.Num());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAnimationHandler::HandlePlayAnimation(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	FString AnimPath;
	if (!RequireString(Params, TEXT("animation"), AnimPath, Error))
	{
		return false;
	}

	USkeletalMeshComponent* SkeletalMesh = GetSkeletalMeshComponent(ActorName);
	if (!SkeletalMesh)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("No skeletal mesh found on actor: %s"), *ActorName));
		return false;
	}

	UAnimSequence* Animation = LoadObject<UAnimSequence>(nullptr, *AnimPath);
	if (!Animation)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Animation not found: %s"), *AnimPath));
		return false;
	}

	bool bLooping = false;
	if (Params->HasField(TEXT("looping")))
	{
		bLooping = Params->GetBoolField(TEXT("looping"));
	}

	float PlayRate = 1.0f;
	if (Params->HasField(TEXT("playRate")))
	{
		PlayRate = Params->GetNumberField(TEXT("playRate"));
	}

	SkeletalMesh->PlayAnimation(Animation, bLooping);
	SkeletalMesh->SetPlayRate(PlayRate);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetNumberField(TEXT("duration"), Animation->GetPlayLength());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAnimationHandler::HandleStopAnimation(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	USkeletalMeshComponent* SkeletalMesh = GetSkeletalMeshComponent(ActorName);
	if (!SkeletalMesh)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("No skeletal mesh found on actor: %s"), *ActorName));
		return false;
	}

	SkeletalMesh->Stop();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAnimationHandler::HandlePauseAnimation(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	USkeletalMeshComponent* SkeletalMesh = GetSkeletalMeshComponent(ActorName);
	if (!SkeletalMesh)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("No skeletal mesh found on actor: %s"), *ActorName));
		return false;
	}

	SkeletalMesh->bPauseAnims = true;

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAnimationHandler::HandleResumeAnimation(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	USkeletalMeshComponent* SkeletalMesh = GetSkeletalMeshComponent(ActorName);
	if (!SkeletalMesh)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("No skeletal mesh found on actor: %s"), *ActorName));
		return false;
	}

	SkeletalMesh->bPauseAnims = false;

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAnimationHandler::HandleGetPlaybackPosition(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	USkeletalMeshComponent* SkeletalMesh = GetSkeletalMeshComponent(ActorName);
	if (!SkeletalMesh)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("No skeletal mesh found on actor: %s"), *ActorName));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetNumberField(TEXT("position"), SkeletalMesh->GetPosition());
	ResultObj->SetNumberField(TEXT("playRate"), SkeletalMesh->GetPlayRate());
	ResultObj->SetBoolField(TEXT("isPlaying"), SkeletalMesh->IsPlaying());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAnimationHandler::HandleSetPlaybackPosition(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	if (!Params->HasField(TEXT("position")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: position"));
		return false;
	}
	float Position = Params->GetNumberField(TEXT("position"));

	USkeletalMeshComponent* SkeletalMesh = GetSkeletalMeshComponent(ActorName);
	if (!SkeletalMesh)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("No skeletal mesh found on actor: %s"), *ActorName));
		return false;
	}

	SkeletalMesh->SetPosition(Position, false);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAnimationHandler::HandleGetPlaybackRate(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	USkeletalMeshComponent* SkeletalMesh = GetSkeletalMeshComponent(ActorName);
	if (!SkeletalMesh)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("No skeletal mesh found on actor: %s"), *ActorName));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetNumberField(TEXT("playRate"), SkeletalMesh->GetPlayRate());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAnimationHandler::HandleSetPlaybackRate(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	if (!Params->HasField(TEXT("rate")))
	{
		Error = UUltimateControlSubsystem::MakeError(-32602, TEXT("Missing required parameter: rate"));
		return false;
	}
	float Rate = Params->GetNumberField(TEXT("rate"));

	USkeletalMeshComponent* SkeletalMesh = GetSkeletalMeshComponent(ActorName);
	if (!SkeletalMesh)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("No skeletal mesh found on actor: %s"), *ActorName));
		return false;
	}

	SkeletalMesh->SetPlayRate(Rate);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAnimationHandler::HandlePlayMontage(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	FString MontagePath;
	if (!RequireString(Params, TEXT("montage"), MontagePath, Error))
	{
		return false;
	}

	USkeletalMeshComponent* SkeletalMesh = GetSkeletalMeshComponent(ActorName);
	if (!SkeletalMesh)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("No skeletal mesh found on actor: %s"), *ActorName));
		return false;
	}

	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
	if (!Montage)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Montage not found: %s"), *MontagePath));
		return false;
	}

	UAnimInstance* AnimInstance = SkeletalMesh->GetAnimInstance();
	if (!AnimInstance)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No anim instance on skeletal mesh"));
		return false;
	}

	float PlayRate = 1.0f;
	if (Params->HasField(TEXT("playRate")))
	{
		PlayRate = Params->GetNumberField(TEXT("playRate"));
	}

	FString StartSection;
	FName StartSectionName = NAME_None;
	if (Params->HasField(TEXT("startSection")))
	{
		StartSection = Params->GetStringField(TEXT("startSection"));
		StartSectionName = FName(*StartSection);
	}

	float Duration = AnimInstance->Montage_Play(Montage, PlayRate, EMontagePlayReturnType::MontageLength, 0.0f, true);

	if (StartSectionName != NAME_None)
	{
		AnimInstance->Montage_JumpToSection(StartSectionName, Montage);
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), Duration > 0.0f);
	ResultObj->SetNumberField(TEXT("duration"), Duration);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAnimationHandler::HandleStopMontage(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	USkeletalMeshComponent* SkeletalMesh = GetSkeletalMeshComponent(ActorName);
	if (!SkeletalMesh)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("No skeletal mesh found on actor: %s"), *ActorName));
		return false;
	}

	UAnimInstance* AnimInstance = SkeletalMesh->GetAnimInstance();
	if (!AnimInstance)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No anim instance on skeletal mesh"));
		return false;
	}

	float BlendOutTime = 0.25f;
	if (Params->HasField(TEXT("blendOutTime")))
	{
		BlendOutTime = Params->GetNumberField(TEXT("blendOutTime"));
	}

	AnimInstance->Montage_Stop(BlendOutTime);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAnimationHandler::HandleJumpToMontageSection(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	FString SectionName;
	if (!RequireString(Params, TEXT("section"), SectionName, Error))
	{
		return false;
	}

	USkeletalMeshComponent* SkeletalMesh = GetSkeletalMeshComponent(ActorName);
	if (!SkeletalMesh)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("No skeletal mesh found on actor: %s"), *ActorName));
		return false;
	}

	UAnimInstance* AnimInstance = SkeletalMesh->GetAnimInstance();
	if (!AnimInstance)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No anim instance on skeletal mesh"));
		return false;
	}

	AnimInstance->Montage_JumpToSection(FName(*SectionName));

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAnimationHandler::HandleGetMontagePosition(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	USkeletalMeshComponent* SkeletalMesh = GetSkeletalMeshComponent(ActorName);
	if (!SkeletalMesh)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("No skeletal mesh found on actor: %s"), *ActorName));
		return false;
	}

	UAnimInstance* AnimInstance = SkeletalMesh->GetAnimInstance();
	if (!AnimInstance)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No anim instance on skeletal mesh"));
		return false;
	}

	UAnimMontage* CurrentMontage = AnimInstance->GetCurrentActiveMontage();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	if (CurrentMontage)
	{
		ResultObj->SetBoolField(TEXT("isPlaying"), true);
		ResultObj->SetStringField(TEXT("montage"), CurrentMontage->GetPathName());
		ResultObj->SetNumberField(TEXT("position"), AnimInstance->Montage_GetPosition(CurrentMontage));
		ResultObj->SetNumberField(TEXT("playRate"), AnimInstance->Montage_GetPlayRate(CurrentMontage));

		FName CurrentSection = AnimInstance->Montage_GetCurrentSection(CurrentMontage);
		ResultObj->SetStringField(TEXT("currentSection"), CurrentSection.ToString());
	}
	else
	{
		ResultObj->SetBoolField(TEXT("isPlaying"), false);
	}

	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAnimationHandler::HandleGetAnimBlueprintVariables(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	USkeletalMeshComponent* SkeletalMesh = GetSkeletalMeshComponent(ActorName);
	if (!SkeletalMesh)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("No skeletal mesh found on actor: %s"), *ActorName));
		return false;
	}

	UAnimInstance* AnimInstance = SkeletalMesh->GetAnimInstance();
	if (!AnimInstance)
	{
		Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("No anim instance on skeletal mesh"));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("animBlueprintClass"), AnimInstance->GetClass()->GetName());

	// Note: Getting/setting anim BP variables requires reflection
	// This is a simplified implementation

	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAnimationHandler::HandleSetAnimBlueprintVariable(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	// This requires runtime reflection to set variables on the anim instance
	Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Setting anim blueprint variables via API requires more specific implementation. Use blueprint function calls instead."));
	return false;
}

bool FUltimateControlAnimationHandler::HandleGetSkeleton(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	USkeletalMeshComponent* SkeletalMesh = GetSkeletalMeshComponent(ActorName);
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("No skeletal mesh found on actor: %s"), *ActorName));
		return false;
	}

	USkeleton* Skeleton = SkeletalMesh->GetSkeletalMeshAsset()->GetSkeleton();
	if (!Skeleton)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, TEXT("No skeleton found"));
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> BonesArray;
	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();

	for (int32 i = 0; i < RefSkeleton.GetNum(); i++)
	{
		TSharedPtr<FJsonObject> BoneObj = MakeShared<FJsonObject>();
		BoneObj->SetNumberField(TEXT("index"), i);
		BoneObj->SetStringField(TEXT("name"), RefSkeleton.GetBoneName(i).ToString());
		BoneObj->SetNumberField(TEXT("parentIndex"), RefSkeleton.GetParentIndex(i));
		BonesArray.Add(MakeShared<FJsonValueObject>(BoneObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("skeleton"), Skeleton->GetPathName());
	ResultObj->SetArrayField(TEXT("bones"), BonesArray);
	ResultObj->SetNumberField(TEXT("boneCount"), BonesArray.Num());
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAnimationHandler::HandleGetBoneTransform(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString ActorName;
	if (!RequireString(Params, TEXT("actor"), ActorName, Error))
	{
		return false;
	}

	FString BoneName;
	if (!RequireString(Params, TEXT("bone"), BoneName, Error))
	{
		return false;
	}

	USkeletalMeshComponent* SkeletalMesh = GetSkeletalMeshComponent(ActorName);
	if (!SkeletalMesh)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("No skeletal mesh found on actor: %s"), *ActorName));
		return false;
	}

	int32 BoneIndex = SkeletalMesh->GetBoneIndex(FName(*BoneName));
	if (BoneIndex == INDEX_NONE)
	{
		Error = UUltimateControlSubsystem::MakeError(-32003, FString::Printf(TEXT("Bone not found: %s"), *BoneName));
		return false;
	}

	FTransform BoneTransform = SkeletalMesh->GetBoneTransform(BoneIndex);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetObjectField(TEXT("location"), VectorToJson(BoneTransform.GetLocation()));
	ResultObj->SetObjectField(TEXT("rotation"), RotatorToJson(BoneTransform.Rotator()));
	ResultObj->SetObjectField(TEXT("scale"), VectorToJson(BoneTransform.GetScale3D()));
	Result = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAnimationHandler::HandleSetBoneTransform(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	// Setting bone transforms at runtime requires specific bone modification techniques
	Error = UUltimateControlSubsystem::MakeError(-32002, TEXT("Setting bone transforms via API requires physics asset or animation modification. Use Modify Bone node in anim blueprints."));
	return false;
}
