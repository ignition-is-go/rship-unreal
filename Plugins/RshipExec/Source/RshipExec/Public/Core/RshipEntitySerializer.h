#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Core/RshipEntityRecords.h"

class RSHIPEXEC_API FRshipEntitySerializer
{
public:
	static TSharedPtr<FJsonObject> ToJson(const FRshipMachineRecord& Record);
	static TSharedPtr<FJsonObject> ToJson(const FRshipInstanceRecord& Record);
	static TSharedPtr<FJsonObject> ToJson(const FRshipActionRecord& Record);
	static TSharedPtr<FJsonObject> ToJson(const FRshipEmitterRecord& Record);
	static TSharedPtr<FJsonObject> ToJson(const FRshipTargetRecord& Record);
	static TSharedPtr<FJsonObject> ToJson(const FRshipTargetStatusRecord& Record);
	static TSharedPtr<FJsonObject> ToJson(const FRshipPulseRecord& Record);

private:
	static TArray<TSharedPtr<FJsonValue>> ToStringArray(const TArray<FString>& Values);
};
