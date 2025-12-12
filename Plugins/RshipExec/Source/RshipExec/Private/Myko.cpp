#include "Myko.h"
#include "Misc/Guid.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformProcess.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"

using namespace std;

const FString MEVENT_EVENT = "ws:m:event";

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
    TSharedPtr<FJsonObject> payload = MakeShareable(new FJsonObject);
    payload->SetStringField(TEXT("event"), TEXT("ws:m:event"));
    payload->SetStringField(TEXT("changeType"), TEXT("SET"));
    payload->SetStringField(TEXT("itemType"), itemType);
    payload->SetObjectField(TEXT("item"), data);

    // Add transaction ID for event tracking/deduplication (myko protocol requirement)
    payload->SetStringField(TEXT("tx"), GenerateTransactionId());

    // Add timestamp for event ordering (myko protocol requirement)
    payload->SetStringField(TEXT("createdAt"), GetIso8601Timestamp());

    return payload;
}

TSharedPtr<FJsonObject> MakeDel(FString itemType, TSharedPtr<FJsonObject> data)
{
    TSharedPtr<FJsonObject> payload = MakeShareable(new FJsonObject);
    payload->SetStringField(TEXT("event"), TEXT("ws:m:event"));
    payload->SetStringField(TEXT("changeType"), TEXT("DEL"));
    payload->SetStringField(TEXT("itemType"), itemType);
    payload->SetObjectField(TEXT("item"), data);

    // Add transaction ID for event tracking/deduplication (myko protocol requirement)
    payload->SetStringField(TEXT("tx"), GenerateTransactionId());

    // Add timestamp for event ordering (myko protocol requirement)
    payload->SetStringField(TEXT("createdAt"), GetIso8601Timestamp());

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
