#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Generate a unique transaction ID (UUID) for myko event tracking
FString GenerateTransactionId();

// Get current UTC timestamp in ISO 8601 format for myko events
FString GetIso8601Timestamp();

// Create a protocol-compliant event payload envelope for any change type.
TSharedPtr<FJsonObject> MakeEvent(FString itemType, FString changeType, TSharedPtr<FJsonObject> data, const FString& sourceId = TEXT(""));

// Create a SET event payload with tx and createdAt fields (myko protocol compliant)
TSharedPtr<FJsonObject> MakeSet(FString itemType, TSharedPtr<FJsonObject> data);

// Create a DEL event payload with tx and createdAt fields (myko protocol compliant)
TSharedPtr<FJsonObject> MakeDel(FString itemType, TSharedPtr<FJsonObject> data);

// Get unique machine identifier (hostname)
FString GetUniqueMachineId();

// Wrap payload in ws:m:event envelope
TSharedPtr<FJsonObject> WrapWSEvent(TSharedPtr<FJsonObject> payload);

