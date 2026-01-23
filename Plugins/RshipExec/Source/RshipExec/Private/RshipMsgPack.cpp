#include "RshipMsgPack.h"
#include "Logs.h"
#include "Misc/Base64.h"

#include <sstream>

// Unreal's check() macro conflicts with msgpack's internal use
#pragma push_macro("check")
#undef check

#include <msgpack.hpp>

#pragma pop_macro("check")

// Forward declarations for recursive encoding/decoding
static void PackJsonValue(msgpack::packer<msgpack::sbuffer>& Packer, const TSharedPtr<FJsonValue>& Value);
static TSharedPtr<FJsonValue> UnpackToJsonValue(const msgpack::object& Obj);

// ============================================================================
// Encoding: FJsonObject -> msgpack binary
// ============================================================================

static void PackJsonObject(msgpack::packer<msgpack::sbuffer>& Packer, const TSharedPtr<FJsonObject>& Object)
{
    if (!Object.IsValid())
    {
        Packer.pack_nil();
        return;
    }

    const auto& Values = Object->Values;
    Packer.pack_map(static_cast<uint32_t>(Values.Num()));

    for (const auto& Pair : Values)
    {
        // Pack key as string
        std::string Key = TCHAR_TO_UTF8(*Pair.Key);
        Packer.pack(Key);

        // Pack value
        PackJsonValue(Packer, Pair.Value);
    }
}

static void PackJsonArray(msgpack::packer<msgpack::sbuffer>& Packer, const TArray<TSharedPtr<FJsonValue>>& Array)
{
    Packer.pack_array(static_cast<uint32_t>(Array.Num()));

    for (const auto& Value : Array)
    {
        PackJsonValue(Packer, Value);
    }
}

static void PackJsonValue(msgpack::packer<msgpack::sbuffer>& Packer, const TSharedPtr<FJsonValue>& Value)
{
    if (!Value.IsValid())
    {
        Packer.pack_nil();
        return;
    }

    switch (Value->Type)
    {
    case EJson::Null:
        Packer.pack_nil();
        break;

    case EJson::Boolean:
        Packer.pack(Value->AsBool());
        break;

    case EJson::Number:
    {
        double NumValue = Value->AsNumber();
        // Check if it's an integer
        if (NumValue == FMath::FloorToDouble(NumValue) &&
            NumValue >= static_cast<double>(INT64_MIN) &&
            NumValue <= static_cast<double>(INT64_MAX))
        {
            Packer.pack(static_cast<int64_t>(NumValue));
        }
        else
        {
            Packer.pack(NumValue);
        }
        break;
    }

    case EJson::String:
    {
        std::string StrValue = TCHAR_TO_UTF8(*Value->AsString());
        Packer.pack(StrValue);
        break;
    }

    case EJson::Array:
        PackJsonArray(Packer, Value->AsArray());
        break;

    case EJson::Object:
        PackJsonObject(Packer, Value->AsObject());
        break;

    default:
        Packer.pack_nil();
        break;
    }
}

bool FRshipMsgPack::Encode(const TSharedPtr<FJsonObject>& JsonObject, TArray<uint8>& OutData)
{
    if (!JsonObject.IsValid())
    {
        return false;
    }

    try
    {
        msgpack::sbuffer Buffer;
        msgpack::packer<msgpack::sbuffer> Packer(&Buffer);

        PackJsonObject(Packer, JsonObject);

        OutData.Reset();
        OutData.Append(reinterpret_cast<const uint8*>(Buffer.data()), Buffer.size());

        return true;
    }
    catch (const std::exception& e)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("FRshipMsgPack::Encode failed: %s"), UTF8_TO_TCHAR(e.what()));
        return false;
    }
}

// ============================================================================
// Decoding: msgpack binary -> FJsonObject
// ============================================================================

static TSharedPtr<FJsonObject> UnpackToJsonObject(const msgpack::object& Obj)
{
    if (Obj.type != msgpack::type::MAP)
    {
        return nullptr;
    }

    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());

    const msgpack::object_map& Map = Obj.via.map;
    for (uint32_t i = 0; i < Map.size; i++)
    {
        const msgpack::object_kv& KV = Map.ptr[i];

        // Get key as string
        FString Key;
        if (KV.key.type == msgpack::type::STR)
        {
            Key = UTF8_TO_TCHAR(std::string(KV.key.via.str.ptr, KV.key.via.str.size).c_str());
        }
        else
        {
            // Convert non-string keys to string
            std::stringstream ss;
            ss << KV.key;
            Key = UTF8_TO_TCHAR(ss.str().c_str());
        }

        // Unpack value
        TSharedPtr<FJsonValue> Value = UnpackToJsonValue(KV.val);
        if (Value.IsValid())
        {
            JsonObject->SetField(Key, Value);
        }
    }

    return JsonObject;
}

static TArray<TSharedPtr<FJsonValue>> UnpackToJsonArray(const msgpack::object& Obj)
{
    TArray<TSharedPtr<FJsonValue>> Array;

    if (Obj.type != msgpack::type::ARRAY)
    {
        return Array;
    }

    const msgpack::object_array& MsgArray = Obj.via.array;
    Array.Reserve(MsgArray.size);

    for (uint32_t i = 0; i < MsgArray.size; i++)
    {
        TSharedPtr<FJsonValue> Value = UnpackToJsonValue(MsgArray.ptr[i]);
        Array.Add(Value);
    }

    return Array;
}

static TSharedPtr<FJsonValue> UnpackToJsonValue(const msgpack::object& Obj)
{
    switch (Obj.type)
    {
    case msgpack::type::NIL:
        return MakeShareable(new FJsonValueNull());

    case msgpack::type::BOOLEAN:
        return MakeShareable(new FJsonValueBoolean(Obj.via.boolean));

    case msgpack::type::POSITIVE_INTEGER:
        return MakeShareable(new FJsonValueNumber(static_cast<double>(Obj.via.u64)));

    case msgpack::type::NEGATIVE_INTEGER:
        return MakeShareable(new FJsonValueNumber(static_cast<double>(Obj.via.i64)));

    case msgpack::type::FLOAT32:
    case msgpack::type::FLOAT64:
        return MakeShareable(new FJsonValueNumber(Obj.via.f64));

    case msgpack::type::STR:
    {
        FString StrValue = UTF8_TO_TCHAR(std::string(Obj.via.str.ptr, Obj.via.str.size).c_str());
        return MakeShareable(new FJsonValueString(StrValue));
    }

    case msgpack::type::BIN:
    {
        // Convert binary to base64 string for JSON compatibility
        FString Base64 = FBase64::Encode(reinterpret_cast<const uint8*>(Obj.via.bin.ptr), Obj.via.bin.size);
        return MakeShareable(new FJsonValueString(Base64));
    }

    case msgpack::type::ARRAY:
        return MakeShareable(new FJsonValueArray(UnpackToJsonArray(Obj)));

    case msgpack::type::MAP:
        return MakeShareable(new FJsonValueObject(UnpackToJsonObject(Obj)));

    case msgpack::type::EXT:
        // Extension types not supported, return null
        return MakeShareable(new FJsonValueNull());

    default:
        return MakeShareable(new FJsonValueNull());
    }
}

bool FRshipMsgPack::Decode(const TArray<uint8>& Data, TSharedPtr<FJsonObject>& OutObject)
{
    if (Data.Num() == 0)
    {
        return false;
    }

    try
    {
        msgpack::object_handle Handle = msgpack::unpack(
            reinterpret_cast<const char*>(Data.GetData()),
            Data.Num()
        );

        const msgpack::object& Obj = Handle.get();

        if (Obj.type != msgpack::type::MAP)
        {
            UE_LOG(LogRshipExec, Warning, TEXT("FRshipMsgPack::Decode: Root is not a map (type=%d)"), static_cast<int>(Obj.type));
            return false;
        }

        OutObject = UnpackToJsonObject(Obj);
        return OutObject.IsValid();
    }
    catch (const std::exception& e)
    {
        UE_LOG(LogRshipExec, Warning, TEXT("FRshipMsgPack::Decode failed: %s"), UTF8_TO_TCHAR(e.what()));
        return false;
    }
}
