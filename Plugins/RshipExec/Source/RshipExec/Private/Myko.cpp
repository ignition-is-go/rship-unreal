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

TSharedPtr<FJsonObject> MakeDel(FString itemType, TSharedPtr<FJsonObject> data)
{
    // Inner event data object (matches myko MEvent structure)
    TSharedPtr<FJsonObject> eventData = MakeShareable(new FJsonObject);
    eventData->SetStringField(TEXT("changeType"), TEXT("DEL"));
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
