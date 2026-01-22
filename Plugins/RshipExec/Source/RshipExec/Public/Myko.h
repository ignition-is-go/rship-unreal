#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Myko WebSocket protocol event types
extern const FString MEVENT_EVENT;           // "ws:m:event"
extern const FString MQUERY_EVENT;           // "ws:m:query"
extern const FString MQUERY_RESPONSE_EVENT;  // "ws:m:query-response"
extern const FString MQUERY_CANCEL_EVENT;    // "ws:m:query-cancel"

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

/**
 * Create a query request payload for the myko protocol.
 * @param QueryId - The query type identifier (e.g., "GetTargetsByServiceId")
 * @param QueryItemType - The entity type being queried (e.g., "Target")
 * @param QueryParams - Additional query parameters as JSON object
 * @param OutTx - Output parameter for the transaction ID (used to match responses)
 * @return The wrapped query message ready to send
 */
TSharedPtr<FJsonObject> MakeQuery(
    const FString& QueryId,
    const FString& QueryItemType,
    TSharedPtr<FJsonObject> QueryParams,
    FString& OutTx
);

/**
 * Create a query cancel message.
 * @param Tx - The transaction ID of the query to cancel
 * @return The wrapped cancel message
 */
TSharedPtr<FJsonObject> MakeQueryCancel(const FString& Tx);

/**
 * Compute a deterministic hash from JSON object data.
 * Used for entity change detection - same data produces same hash.
 * @param Data - The JSON object to hash
 * @return Hex string hash
 */
FString ComputeEntityHash(TSharedPtr<FJsonObject> Data);

