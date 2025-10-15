#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace Rship::Sdk
{
    struct FConnectionDetails
    {
        FString Host;
        int32 Port;
        FString Path;

        FConnectionDetails(const FString& InHost = TEXT("localhost"), int32 InPort = 5155, const FString& InPath = TEXT("/myko"))
            : Host(InHost)
            , Port(InPort)
            , Path(InPath)
        {
        }
    };

    FConnectionDetails MakeConnectionDetails(const FString& HostAddress);

    FString BuildWebSocketUrl(const FConnectionDetails& Details);

    TSharedPtr<FJsonObject> MakeSet(const FString& ItemType, const TSharedPtr<FJsonObject>& Data);

    TSharedPtr<FJsonObject> MakePulse(const FString& FullEmitterId, const TSharedPtr<FJsonObject>& Data);

    TSharedPtr<FJsonObject> WrapWSEvent(const TSharedPtr<FJsonObject>& Payload);

    FString Serialize(const TSharedPtr<FJsonObject>& Payload);

    uint32 CalculatePayloadHash(const FString& Payload);

    FString GetUniqueMachineId();
}

