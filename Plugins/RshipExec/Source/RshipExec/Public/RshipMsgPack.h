#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/**
 * MessagePack encoder/decoder for FJsonObject using msgpack-c library.
 *
 * Provides efficient binary serialization over WebSocket to the rship server.
 * The server auto-detects msgpack when it receives binary data.
 */
class RSHIPEXEC_API FRshipMsgPack
{
public:
    /**
     * Encode a JSON object to msgpack binary format.
     * @param JsonObject The JSON object to encode
     * @param OutData Output byte array
     * @return true if encoding succeeded
     */
    static bool Encode(const TSharedPtr<FJsonObject>& JsonObject, TArray<uint8>& OutData);

    /**
     * Decode msgpack binary data to a JSON object.
     * @param Data The binary data to decode
     * @param OutObject Output JSON object
     * @return true if decoding succeeded
     */
    static bool Decode(const TArray<uint8>& Data, TSharedPtr<FJsonObject>& OutObject);
};
