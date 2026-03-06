#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace RshipMykoEventNames
{
	inline constexpr const TCHAR* Event = TEXT("ws:m:event");
	inline constexpr const TCHAR* EventBatch = TEXT("ws:m:event-batch");
}

class RSHIPEXEC_API FRshipMykoTransport
{
public:
	static FString GenerateTransactionId();
	static FString GetIso8601Timestamp();
	static FString GetUniqueMachineId();

	static TSharedPtr<FJsonObject> MakeSet(const FString& ItemType, const TSharedPtr<FJsonObject>& Item, const FString& SourceId = TEXT(""));
	static TSharedPtr<FJsonObject> MakeDel(const FString& ItemType, const TSharedPtr<FJsonObject>& Item, const FString& SourceId = TEXT(""));

	static bool IsMykoEventEnvelope(const TSharedPtr<FJsonObject>& Payload);
	static bool TryGetMykoEventData(const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FJsonObject>& OutEventData);

private:
	static TSharedPtr<FJsonObject> MakeEvent(const FString& ItemType, const FString& ChangeType, const TSharedPtr<FJsonObject>& Item, const FString& SourceId);
};
