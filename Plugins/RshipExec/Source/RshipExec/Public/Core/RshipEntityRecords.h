#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

struct FRshipMachineRecord
{
	FString Id;
	FString Name;
	FString ExecName;
	FString ClientId;
	FString Hash;
};

struct FRshipInstanceRecord
{
	FString ClientId;
	FString Name;
	FString Id;
	FString ClusterId;
	FString ServiceTypeCode;
	FString ServiceId;
	FString MachineId;
	FString Status;
	FString Color;
	FString Hash;
};

struct FRshipActionRecord
{
	FString Id;
	FString Name;
	FString TargetId;
	FString ServiceId;
	TSharedPtr<FJsonObject> Schema;
	FString Hash;
};

struct FRshipEmitterRecord
{
	FString Id;
	FString Name;
	FString TargetId;
	FString ServiceId;
	TSharedPtr<FJsonObject> Schema;
	FString Hash;
};

struct FRshipTargetRecord
{
	FString Id;
	FString Name;
	FString ServiceId;
	FString Category;
	FString ForegroundColor;
	FString BackgroundColor;
	TArray<FString> ActionIds;
	TArray<FString> EmitterIds;
	TArray<FString> Tags;
	TArray<FString> GroupIds;
	TArray<FString> ParentTargetIds;
	bool bRootLevel = true;
	FString Hash;
};

struct FRshipTargetStatusRecord
{
	FString Id;
	FString TargetId;
	FString InstanceId;
	FString Status;
	FString Hash;
};

struct FRshipPulseRecord
{
	FString Id;
	FString EmitterId;
	TSharedPtr<FJsonObject> Data;
	double TimestampMs = 0.0;
	FString ClientId;
	FString Hash;
};
