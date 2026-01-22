#include "Myko.h"
#include "Misc/Guid.h"
#include "Misc/DateTime.h"
#include "Misc/SecureHash.h"
#include "HAL/PlatformProcess.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Serialization/JsonSerializer.h"

using namespace std;

// Myko WebSocket protocol event types
const FString MEVENT_EVENT = TEXT("ws:m:event");
const FString MQUERY_EVENT = TEXT("ws:m:query");
const FString MQUERY_RESPONSE_EVENT = TEXT("ws:m:query-response");
const FString MQUERY_CANCEL_EVENT = TEXT("ws:m:query-cancel");

FString GenerateTransactionId()
{
    return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
}

FString GetIso8601Timestamp()
{
    return FDateTime::UtcNow().ToIso8601();
}

TSharedPtr<FJsonObject> MakeSet(FString itemType, TSharedPtr<FJsonObject> data)
{
    // Inner event data object (matches myko MEvent structure)
    TSharedPtr<FJsonObject> eventData = MakeShareable(new FJsonObject);
    eventData->SetStringField(TEXT("changeType"), TEXT("SET"));
    eventData->SetStringField(TEXT("itemType"), itemType);
    eventData->SetObjectField(TEXT("item"), data);
    eventData->SetStringField(TEXT("tx"), GenerateTransactionId());
    eventData->SetStringField(TEXT("createdAt"), GetIso8601Timestamp());

    // Outer wrapper (matches myko WSMEvent structure: { event: "ws:m:event", data: MEvent })
    TSharedPtr<FJsonObject> payload = MakeShareable(new FJsonObject);
    payload->SetStringField(TEXT("event"), TEXT("ws:m:event"));
    payload->SetObjectField(TEXT("data"), eventData);

    return payload;
}

FString GetUniqueMachineId()
{
    FString HostName = FPlatformProcess::ComputerName();
    FString UniqueMachineId = HostName;
    return UniqueMachineId;
}

TSharedPtr<FJsonObject> WrapWSEvent(TSharedPtr<FJsonObject> payload)
{
    TSharedPtr<FJsonObject> wrapped = MakeShareable(new FJsonObject);
    wrapped->SetStringField(TEXT("event"), MEVENT_EVENT);
    wrapped->SetObjectField(TEXT("data"), payload);
    return wrapped;
}

TSharedPtr<FJsonObject> MakeQuery(
    const FString& QueryId,
    const FString& QueryItemType,
    TSharedPtr<FJsonObject> QueryParams,
    FString& OutTx
)
{
    // Generate transaction ID for tracking the response
    OutTx = GenerateTransactionId();

    // Build query object with tx field
    TSharedPtr<FJsonObject> Query = MakeShareable(new FJsonObject);
    Query->SetStringField(TEXT("tx"), OutTx);

    // Copy all query parameters into the query object
    if (QueryParams.IsValid())
    {
        for (const auto& Field : QueryParams->Values)
        {
            Query->SetField(Field.Key, Field.Value);
        }
    }

    // Build wrapped query data (matches MWrappedQuery structure)
    TSharedPtr<FJsonObject> WrappedData = MakeShareable(new FJsonObject);
    WrappedData->SetObjectField(TEXT("query"), Query);
    WrappedData->SetStringField(TEXT("queryId"), QueryId);
    WrappedData->SetStringField(TEXT("queryItemType"), QueryItemType);

    // Build outer message envelope
    TSharedPtr<FJsonObject> Message = MakeShareable(new FJsonObject);
    Message->SetStringField(TEXT("event"), MQUERY_EVENT);
    Message->SetObjectField(TEXT("data"), WrappedData);

    return Message;
}

TSharedPtr<FJsonObject> MakeQueryCancel(const FString& Tx)
{
    TSharedPtr<FJsonObject> Message = MakeShareable(new FJsonObject);
    Message->SetStringField(TEXT("event"), MQUERY_CANCEL_EVENT);
    Message->SetStringField(TEXT("tx"), Tx);
    return Message;
}

FString ComputeEntityHash(TSharedPtr<FJsonObject> Data)
{
    if (!Data.IsValid())
    {
        return TEXT("");
    }

    // Serialize to JSON string (deterministic since FJsonObject maintains insertion order)
    FString JsonString;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonString);
    FJsonSerializer::Serialize(Data.ToSharedRef(), Writer);

    // Compute MD5 hash
    FMD5 Md5;
    Md5.Update(reinterpret_cast<const uint8*>(TCHAR_TO_UTF8(*JsonString)), JsonString.Len());

    uint8 Digest[16];
    Md5.Final(Digest);

    // Convert to hex string
    FString Hash;
    for (int32 i = 0; i < 16; i++)
    {
        Hash += FString::Printf(TEXT("%02x"), Digest[i]);
    }

    return Hash;
}
