#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/**
 * Struct for a single batch action item - parsed directly from msgpack
 * without intermediate FJsonObject allocation
 */
struct FRshipBatchActionItem
{
    FString TargetId;
    FString ActionId;
    TSharedPtr<FJsonObject> Data;  // Action data still needs FJsonObject for flexibility
};

/**
 * Struct for batch action command - parsed directly from msgpack
 */
struct FRshipBatchCommand
{
    FString CommandId;
    FString TxId;
    TArray<FRshipBatchActionItem> Actions;
};

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

    /**
     * Try to decode a batch action command directly from msgpack without full FJsonObject conversion.
     * This is an optimized fast path for high-frequency batch commands.
     * @param Data The binary data to decode
     * @param OutCommand Output batch command struct
     * @return true if this was a batch command and was decoded successfully, false to fall back to normal path
     */
    static bool TryDecodeBatchCommand(const TArray<uint8>& Data, FRshipBatchCommand& OutCommand);
};
