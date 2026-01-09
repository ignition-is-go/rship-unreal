#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Generate a unique transaction ID (UUID) for myko event tracking
FString GenerateTransactionId();

// Get current UTC timestamp in ISO 8601 format for myko events
FString GetIso8601Timestamp();

// Create a SET event payload with tx and createdAt fields (myko protocol compliant)
TSharedPtr<FJsonObject> MakeSet(FString itemType, TSharedPtr<FJsonObject> data);

// Get unique machine identifier (hostname)
FString GetUniqueMachineId();

// Wrap payload in ws:m:event envelope
TSharedPtr<FJsonObject> WrapWSEvent(TSharedPtr<FJsonObject> payload);

