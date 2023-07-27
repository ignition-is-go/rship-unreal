#include "Myko.h"
#include "Misc/Guid.h"
#include "HAL/PlatformProcess.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"

using namespace std;

const FString MEVENT_EVENT = "ws:m:event";

TSharedPtr<FJsonObject> MakeSet(FString itemType, TSharedPtr<FJsonObject> data)
{
    TSharedPtr<FJsonObject> payload = MakeShareable(new FJsonObject);
    payload->SetStringField(TEXT("event"), TEXT("ws:m:event"));
    payload->SetStringField(TEXT("changeType"), TEXT("SET"));
    payload->SetStringField(TEXT("itemType"), itemType);
    payload->SetObjectField(TEXT("item"), data);

    FString Hash;
    TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&Hash);
    if (FJsonSerializer::Serialize(payload.ToSharedRef(), JsonWriter))
    {
        // UE_LOG(LogTemp, Warning, TEXT("JSON: %s"), *Hash);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to serialize JSON object."));
    }
    // payload->SetStringField(TEXT("hash"), Hash);

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
