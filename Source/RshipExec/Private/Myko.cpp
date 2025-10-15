#include "Myko.h"
#include "HAL/PlatformProcess.h"
#include "Containers/StringConv.h"
#include "Misc/Crc.h"
#include "Logging/LogMacros.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    constexpr const TCHAR* MykoEvent = TEXT("ws:m:event");
    constexpr int32 DefaultPort = 5155;
    constexpr const TCHAR* DefaultPath = TEXT("/myko");
}

namespace Rship::Sdk
{
    FConnectionDetails MakeConnectionDetails(const FString& HostAddress)
    {
        FString SanitizedHost = HostAddress;
        if (SanitizedHost.IsEmpty())
        {
            SanitizedHost = TEXT("localhost");
        }

        return {SanitizedHost, DefaultPort, DefaultPath};
    }

    FString BuildWebSocketUrl(const FConnectionDetails& Details)
    {
        FString NormalizedPath = Details.Path;
        if (!NormalizedPath.StartsWith(TEXT("/")))
        {
            NormalizedPath = FString::Printf(TEXT("/%s"), *NormalizedPath);
        }

        return FString::Printf(TEXT("ws://%s:%d%s"), *Details.Host, Details.Port, *NormalizedPath);
    }

    TSharedPtr<FJsonObject> MakeSet(const FString& ItemType, const TSharedPtr<FJsonObject>& Data)
    {
        TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject);
        Payload->SetStringField(TEXT("event"), MykoEvent);
        Payload->SetStringField(TEXT("changeType"), TEXT("SET"));
        Payload->SetStringField(TEXT("itemType"), ItemType);
        Payload->SetObjectField(TEXT("item"), Data);
        return Payload;
    }

    TSharedPtr<FJsonObject> MakePulse(const FString& FullEmitterId, const TSharedPtr<FJsonObject>& Data)
    {
        TSharedPtr<FJsonObject> Pulse = MakeShareable(new FJsonObject);
        Pulse->SetStringField(TEXT("emitterId"), FullEmitterId);
        Pulse->SetStringField(TEXT("id"), FullEmitterId);
        Pulse->SetObjectField(TEXT("data"), Data);
        return Pulse;
    }

    TSharedPtr<FJsonObject> WrapWSEvent(const TSharedPtr<FJsonObject>& Payload)
    {
        TSharedPtr<FJsonObject> Wrapped = MakeShareable(new FJsonObject);
        Wrapped->SetStringField(TEXT("event"), MykoEvent);
        Wrapped->SetObjectField(TEXT("data"), Payload);
        return Wrapped;
    }

    FString Serialize(const TSharedPtr<FJsonObject>& Payload)
    {
        FString JsonString;
        TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
        if (!FJsonSerializer::Serialize(Payload.ToSharedRef(), JsonWriter))
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to serialize JSON object."));
        }

        return JsonString;
    }

    uint32 CalculatePayloadHash(const FString& Payload)
    {
        FTCHARToUTF8 Converter(*Payload);
        return FCrc::MemCrc32(Converter.Get(), Converter.Length());
    }

    FString GetUniqueMachineId()
    {
        return FPlatformProcess::ComputerName();
    }
}

