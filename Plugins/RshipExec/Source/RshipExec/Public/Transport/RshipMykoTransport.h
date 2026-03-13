#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace RshipMykoEventNames
{
	inline constexpr const TCHAR* Event = TEXT("ws:m:event");
	inline constexpr const TCHAR* EventBatch = TEXT("ws:m:event-batch");
	inline constexpr const TCHAR* Query = TEXT("ws:m:query");
	inline constexpr const TCHAR* QueryResponse = TEXT("ws:m:query-response");
	inline constexpr const TCHAR* QueryCancel = TEXT("ws:m:query-cancel");
	inline constexpr const TCHAR* QueryError = TEXT("ws:m:query-error");
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
