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

/**
 * Base class for Myko queries.
 * Subclasses define the query structure matching the server-side TypeScript types.
 */
class RSHIPEXEC_API FMQuery
{
public:
    virtual ~FMQuery() = default;

    /** Get the query type identifier (e.g., "GetTargetsByServiceId") */
    virtual FString GetQueryId() const = 0;

    /** Get the entity type being queried (e.g., "Target") */
    virtual FString GetQueryItemType() const = 0;

    /** Build the query parameters as JSON */
    virtual TSharedPtr<FJsonObject> ToJson() const = 0;

    /** Create the full query message with transaction ID */
    TSharedPtr<FJsonObject> MakeMessage(FString& OutTx) const;
};

/** Query targets by service ID - expects { serviceId: "xxx" } */
class RSHIPEXEC_API FGetTargetsByServiceId : public FMQuery
{
public:
    FString ServiceId;

    FGetTargetsByServiceId(const FString& InServiceId) : ServiceId(InServiceId) {}

    virtual FString GetQueryId() const override { return TEXT("GetTargetsByServiceId"); }
    virtual FString GetQueryItemType() const override { return TEXT("Target"); }
    virtual TSharedPtr<FJsonObject> ToJson() const override;
};

/** Query actions by partial match - expects { query: { ...partial } } */
class RSHIPEXEC_API FGetActionsByQuery : public FMQuery
{
public:
    TSharedPtr<FJsonObject> Query;

    FGetActionsByQuery(TSharedPtr<FJsonObject> InQuery) : Query(InQuery) {}

    // Convenience constructor for serviceId filter
    static TSharedPtr<FGetActionsByQuery> ByServiceId(const FString& ServiceId);

    virtual FString GetQueryId() const override { return TEXT("GetActionsByQuery"); }
    virtual FString GetQueryItemType() const override { return TEXT("Action"); }
    virtual TSharedPtr<FJsonObject> ToJson() const override;
};

/** Query emitters by partial match - expects { query: { ...partial } } */
class RSHIPEXEC_API FGetEmittersByQuery : public FMQuery
{
public:
    TSharedPtr<FJsonObject> Query;

    FGetEmittersByQuery(TSharedPtr<FJsonObject> InQuery) : Query(InQuery) {}

    // Convenience constructor for serviceId filter
    static TSharedPtr<FGetEmittersByQuery> ByServiceId(const FString& ServiceId);

    virtual FString GetQueryId() const override { return TEXT("GetEmittersByQuery"); }
    virtual FString GetQueryItemType() const override { return TEXT("Emitter"); }
    virtual TSharedPtr<FJsonObject> ToJson() const override;
};

