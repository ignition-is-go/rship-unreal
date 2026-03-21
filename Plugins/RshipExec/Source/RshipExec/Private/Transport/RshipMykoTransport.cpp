#include "Transport/RshipMykoTransport.h"

#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"
#include "Containers/Array.h"
#include "Misc/Base64.h"
#include "Templates/SharedPointer.h"

extern "C"
{
#include "rship_msgpack.h"
}

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

TSharedPtr<FJsonObject> FRshipMykoTransport::MakeEvent(const FString& ItemType, const FString& ChangeType, const TSharedPtr<FJsonObject>& Item, const FString& SourceId)
{
	TSharedPtr<FJsonObject> EventData = MakeShareable(new FJsonObject);
	EventData->SetStringField(TEXT("changeType"), ChangeType);
	EventData->SetStringField(TEXT("itemType"), ItemType);
	EventData->SetObjectField(TEXT("item"), Item);
	EventData->SetStringField(TEXT("tx"), GenerateTransactionId());
	EventData->SetStringField(TEXT("createdAt"), GetIso8601Timestamp());
	EventData->SetStringField(TEXT("sourceId"), SourceId.IsEmpty() ? GetUniqueMachineId() : SourceId);

	TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject);
	Payload->SetStringField(TEXT("event"), RshipMykoEventNames::Event);
	Payload->SetObjectField(TEXT("data"), EventData);
	return Payload;
}

FString FRshipMykoTransport::GenerateTransactionId()
{
	return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
}

FString FRshipMykoTransport::GetIso8601Timestamp()
{
	return FDateTime::UtcNow().ToIso8601();
}

FString FRshipMykoTransport::GetUniqueMachineId()
{
	FString Hostname;

#if PLATFORM_WINDOWS
	// Use DNS hostname on Windows to avoid 15-char NetBIOS truncation.
	TCHAR DnsBuffer[256];
	DWORD DnsSize = UE_ARRAY_COUNT(DnsBuffer);
	if (GetComputerNameExW(ComputerNameDnsHostname, DnsBuffer, &DnsSize))
	{
		Hostname = DnsBuffer;
	}
#endif

	if (Hostname.IsEmpty())
	{
		Hostname = FPlatformProcess::ComputerName();
	}

	if (Hostname.IsEmpty())
	{
		Hostname = FPlatformMisc::GetEnvironmentVariable(TEXT("COMPUTERNAME"));
	}

	if (Hostname.IsEmpty())
	{
		Hostname = FPlatformMisc::GetEnvironmentVariable(TEXT("HOSTNAME"));
	}

	if (Hostname.IsEmpty())
	{
		// Keep sourceId non-empty even in unusual runtime environments.
		Hostname = TEXT("unknown-host");
	}

	return Hostname;
}

TSharedPtr<FJsonObject> FRshipMykoTransport::MakeSet(const FString& ItemType, const TSharedPtr<FJsonObject>& Item, const FString& SourceId)
{
	return MakeEvent(ItemType, TEXT("SET"), Item, SourceId);
}

TSharedPtr<FJsonObject> FRshipMykoTransport::MakeDel(const FString& ItemType, const TSharedPtr<FJsonObject>& Item, const FString& SourceId)
{
	return MakeEvent(ItemType, TEXT("DEL"), Item, SourceId);
}

bool FRshipMykoTransport::TryGetMykoEventData(const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FJsonObject>& OutEventData)
{
	OutEventData.Reset();

	if (!Payload.IsValid())
	{
		return false;
	}

	FString EventType;
	if (!Payload->TryGetStringField(TEXT("event"), EventType) || EventType != RshipMykoEventNames::Event)
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* DataPtr = nullptr;
	if (!Payload->TryGetObjectField(TEXT("data"), DataPtr) || !DataPtr || !DataPtr->IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>& Data = *DataPtr;

	FString ChangeType;
	FString ItemType;
	const TSharedPtr<FJsonObject>* ItemPtr = nullptr;

	if (!Data->TryGetStringField(TEXT("changeType"), ChangeType) ||
		!Data->TryGetStringField(TEXT("itemType"), ItemType) ||
		!Data->TryGetObjectField(TEXT("item"), ItemPtr) ||
		!ItemPtr ||
		!ItemPtr->IsValid())
	{
		return false;
	}

	OutEventData = Data;
	return true;
}

bool FRshipMykoTransport::IsMykoEventEnvelope(const TSharedPtr<FJsonObject>& Payload)
{
	TSharedPtr<FJsonObject> EventData;
	return TryGetMykoEventData(Payload, EventData);
}

namespace
{
	static bool EncodeJsonValueToMsgPack(const TSharedPtr<FJsonValue>& Value, rship_msgpack_writer_t& Writer);
	static FString JsonValueKeyToString(const TSharedPtr<FJsonValue>& Value);

	static FString MsgPackUtf8ToFString(const uint8* Ptr, size_t Len)
	{
		if (Ptr == nullptr || Len == 0)
		{
			return FString();
		}

		const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Ptr), static_cast<int32>(Len));
		return FString(Converted.Length(), Converted.Get());
	}

	static bool DecodeMsgPackValue(rship_msgpack_reader_t* Reader, TSharedPtr<FJsonValue>& OutValue)
	{
		msgpack_value_t Value;
		if (!msgpack_read(Reader, &Value))
		{
			return false;
		}

		switch (Value.type)
		{
		case MSGPACK_TYPE_NIL:
			OutValue = MakeShared<FJsonValueNull>();
			return true;
		case MSGPACK_TYPE_BOOL:
			OutValue = MakeShared<FJsonValueBoolean>(Value.v.boolean);
			return true;
		case MSGPACK_TYPE_UINT:
			OutValue = MakeShared<FJsonValueNumber>(static_cast<double>(Value.v.u64));
			return true;
		case MSGPACK_TYPE_INT:
			OutValue = MakeShared<FJsonValueNumber>(static_cast<double>(Value.v.i64));
			return true;
		case MSGPACK_TYPE_FLOAT:
			OutValue = MakeShared<FJsonValueNumber>(static_cast<double>(Value.v.f32));
			return true;
		case MSGPACK_TYPE_DOUBLE:
			OutValue = MakeShared<FJsonValueNumber>(Value.v.f64);
			return true;
		case MSGPACK_TYPE_STR:
			OutValue = MakeShared<FJsonValueString>(MsgPackUtf8ToFString(Value.v.data.ptr, Value.v.data.len));
			return true;
		case MSGPACK_TYPE_BIN:
		{
			const FString Encoded = FBase64::Encode(Value.v.data.ptr, static_cast<uint32>(Value.v.data.len));
			OutValue = MakeShared<FJsonValueString>(Encoded);
			return true;
		}
		case MSGPACK_TYPE_ARRAY:
		{
			TArray<TSharedPtr<FJsonValue>> ArrayValues;
			ArrayValues.Reserve(static_cast<int32>(Value.v.container.count));
			for (size_t Index = 0; Index < Value.v.container.count; ++Index)
			{
				TSharedPtr<FJsonValue> ElementValue;
				if (!DecodeMsgPackValue(Reader, ElementValue))
				{
					return false;
				}
				ArrayValues.Add(ElementValue);
			}
			OutValue = MakeShared<FJsonValueArray>(ArrayValues);
			return true;
		}
		case MSGPACK_TYPE_MAP:
		{
			TSharedPtr<FJsonObject> ObjectValue = MakeShared<FJsonObject>();
			for (size_t Index = 0; Index < Value.v.container.count; ++Index)
			{
				TSharedPtr<FJsonValue> KeyValue;
				TSharedPtr<FJsonValue> EntryValue;
				if (!DecodeMsgPackValue(Reader, KeyValue) || !DecodeMsgPackValue(Reader, EntryValue))
				{
					return false;
				}

				const FString KeyString = JsonValueKeyToString(KeyValue);
				if (KeyString.IsEmpty())
				{
					return false;
				}

				ObjectValue->SetField(KeyString, EntryValue);
			}
			OutValue = MakeShared<FJsonValueObject>(ObjectValue);
			return true;
		}
		case MSGPACK_TYPE_EXT:
		default:
			return false;
		}
	}

	static bool EncodeJsonObjectToMsgPack(const TSharedPtr<FJsonObject>& Object, rship_msgpack_writer_t& Writer)
	{
		if (!Object.IsValid() || !msgpack_write_map(&Writer, static_cast<size_t>(Object->Values.Num())))
		{
			return false;
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
		{
			FTCHARToUTF8 KeyUtf8(*Pair.Key);
			if (!msgpack_write_str_len(&Writer, KeyUtf8.Get(), static_cast<size_t>(KeyUtf8.Length())))
			{
				return false;
			}
			if (!EncodeJsonValueToMsgPack(Pair.Value, Writer))
			{
				return false;
			}
		}

		return !msgpack_writer_overflow(&Writer);
	}

	static bool EncodeJsonValueToMsgPack(const TSharedPtr<FJsonValue>& Value, rship_msgpack_writer_t& Writer)
	{
		if (!Value.IsValid())
		{
			return msgpack_write_nil(&Writer);
		}

		switch (Value->Type)
		{
		case EJson::Null:
			return msgpack_write_nil(&Writer);
		case EJson::String:
		{
			FString StringValue;
			Value->TryGetString(StringValue);
			FTCHARToUTF8 Utf8(*StringValue);
			return msgpack_write_str_len(&Writer, Utf8.Get(), static_cast<size_t>(Utf8.Length()));
		}
		case EJson::Number:
		{
			const double NumberValue = Value->AsNumber();
			const double IntegralValue = FMath::RoundToDouble(NumberValue);
			if (FMath::IsNearlyEqual(NumberValue, IntegralValue))
			{
				return IntegralValue >= 0.0
					? msgpack_write_uint(&Writer, static_cast<uint64>(IntegralValue))
					: msgpack_write_int(&Writer, static_cast<int64>(IntegralValue));
			}
			return msgpack_write_float64(&Writer, NumberValue);
		}
		case EJson::Boolean:
			return msgpack_write_bool(&Writer, Value->AsBool());
		case EJson::Array:
		{
			const TArray<TSharedPtr<FJsonValue>>& ArrayValue = Value->AsArray();
			if (!msgpack_write_array(&Writer, static_cast<size_t>(ArrayValue.Num())))
			{
				return false;
			}
			for (const TSharedPtr<FJsonValue>& Entry : ArrayValue)
			{
				if (!EncodeJsonValueToMsgPack(Entry, Writer))
				{
					return false;
				}
			}
			return !msgpack_writer_overflow(&Writer);
		}
		case EJson::Object:
			return EncodeJsonObjectToMsgPack(Value->AsObject(), Writer);
		case EJson::None:
		default:
			return false;
		}
	}

	static FString JsonValueKeyToString(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid())
		{
			return TEXT("null");
		}

		FString StringValue;
		if (Value->TryGetString(StringValue))
		{
			return StringValue;
		}
		if (Value->Type == EJson::Number)
		{
			return FString::Printf(TEXT("%.0f"), Value->AsNumber());
		}
		if (Value->Type == EJson::Boolean)
		{
			return Value->AsBool() ? TEXT("true") : TEXT("false");
		}
		return TEXT("");
	}

	static TSharedPtr<FJsonObject> MakeObjectFromArray(const TArray<TSharedPtr<FJsonValue>>& Values, const TArray<FString>& FieldNames)
	{
		TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
		const int32 Count = FMath::Min(Values.Num(), FieldNames.Num());
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (Values[Index].IsValid() && Values[Index]->Type != EJson::Null)
			{
				Object->SetField(FieldNames[Index], Values[Index]);
			}
		}
		return Object;
	}

	static FString ResolveMykoEventName(const FString& VariantTag)
	{
		if (VariantTag == TEXT("0")) return TEXT("ws:m:query");
		if (VariantTag == TEXT("1")) return TEXT("ws:m:query-response");
		if (VariantTag == TEXT("2")) return TEXT("ws:m:query-cancel");
		if (VariantTag == TEXT("3")) return TEXT("ws:m:query-window");
		if (VariantTag == TEXT("4")) return TEXT("ws:m:view");
		if (VariantTag == TEXT("5")) return TEXT("ws:m:view-response");
		if (VariantTag == TEXT("6")) return TEXT("ws:m:view-cancel");
		if (VariantTag == TEXT("7")) return TEXT("ws:m:view-window");
		if (VariantTag == TEXT("8")) return TEXT("ws:m:report");
		if (VariantTag == TEXT("9")) return TEXT("ws:m:report-response");
		if (VariantTag == TEXT("10")) return TEXT("ws:m:report-cancel");
		if (VariantTag == TEXT("11")) return TEXT("ws:m:report-error");
		if (VariantTag == TEXT("12")) return TEXT("ws:m:query-error");
		if (VariantTag == TEXT("13")) return TEXT("ws:m:view-error");
		if (VariantTag == TEXT("14")) return TEXT("ws:m:event");
		if (VariantTag == TEXT("15")) return TEXT("ws:m:event-batch");
		if (VariantTag == TEXT("16")) return TEXT("ws:m:command");
		if (VariantTag == TEXT("17")) return TEXT("ws:m:command-response");
		if (VariantTag == TEXT("18")) return TEXT("ws:m:command-error");
		if (VariantTag == TEXT("19")) return TEXT("ws:m:ping");
		if (VariantTag == TEXT("20")) return TEXT("ws:m:protocol-switch");
		return VariantTag;
	}

	static TSharedPtr<FJsonValue> NormalizeWrappedItemValue(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid() || Value->Type != EJson::Array)
		{
			return Value;
		}

		const TArray<TSharedPtr<FJsonValue>>& Values = Value->AsArray();
		if (Values.Num() < 2)
		{
			return Value;
		}

		return MakeShared<FJsonValueObject>(MakeObjectFromArray(Values, { TEXT("item"), TEXT("itemType") }));
	}

	static TSharedPtr<FJsonValue> NormalizeMykoPayload(const FString& EventName, const TSharedPtr<FJsonValue>& PayloadValue)
	{
		if (!PayloadValue.IsValid())
		{
			return PayloadValue;
		}

		if (EventName == TEXT("ws:m:query-response") || EventName == TEXT("ws:m:view-response"))
		{
			if (PayloadValue->Type != EJson::Array)
			{
				return PayloadValue;
			}

			TArray<TSharedPtr<FJsonValue>> Values = PayloadValue->AsArray();
			TSharedPtr<FJsonObject> Object = MakeObjectFromArray(
				Values,
				{ TEXT("changes"), TEXT("deletes"), TEXT("upserts"), TEXT("sequence"), TEXT("tx"), TEXT("totalCount"), TEXT("window") });

			const TArray<TSharedPtr<FJsonValue>>* Upserts = nullptr;
			if (Object->TryGetArrayField(TEXT("upserts"), Upserts) && Upserts != nullptr)
			{
				TArray<TSharedPtr<FJsonValue>> NormalizedUpserts;
				NormalizedUpserts.Reserve(Upserts->Num());
				for (const TSharedPtr<FJsonValue>& Upsert : *Upserts)
				{
					NormalizedUpserts.Add(NormalizeWrappedItemValue(Upsert));
				}
				Object->SetArrayField(TEXT("upserts"), NormalizedUpserts);
			}

			return MakeShared<FJsonValueObject>(Object);
		}

		if (EventName == TEXT("ws:m:query-error"))
		{
			return PayloadValue->Type == EJson::Array
				? MakeShared<FJsonValueObject>(MakeObjectFromArray(PayloadValue->AsArray(), { TEXT("tx"), TEXT("queryId"), TEXT("message") }))
				: PayloadValue;
		}

		if (EventName == TEXT("ws:m:view-error"))
		{
			return PayloadValue->Type == EJson::Array
				? MakeShared<FJsonValueObject>(MakeObjectFromArray(PayloadValue->AsArray(), { TEXT("tx"), TEXT("viewId"), TEXT("message") }))
				: PayloadValue;
		}

		if (EventName == TEXT("ws:m:command-response") || EventName == TEXT("ws:m:report-response"))
		{
			return PayloadValue->Type == EJson::Array
				? MakeShared<FJsonValueObject>(MakeObjectFromArray(PayloadValue->AsArray(), { TEXT("response"), TEXT("tx") }))
				: PayloadValue;
		}

		if (EventName == TEXT("ws:m:command-error"))
		{
			return PayloadValue->Type == EJson::Array
				? MakeShared<FJsonValueObject>(MakeObjectFromArray(PayloadValue->AsArray(), { TEXT("tx"), TEXT("commandId"), TEXT("message") }))
				: PayloadValue;
		}

		if (EventName == TEXT("ws:m:report-error"))
		{
			return PayloadValue->Type == EJson::Array
				? MakeShared<FJsonValueObject>(MakeObjectFromArray(PayloadValue->AsArray(), { TEXT("tx"), TEXT("reportId"), TEXT("message") }))
				: PayloadValue;
		}

		if (EventName == TEXT("ws:m:protocol-switch"))
		{
			return PayloadValue->Type == EJson::Array
				? MakeShared<FJsonValueObject>(MakeObjectFromArray(PayloadValue->AsArray(), { TEXT("protocol") }))
				: PayloadValue;
		}

		if (EventName == TEXT("ws:m:topology-chunk"))
		{
			return PayloadValue->Type == EJson::Array
				? MakeShared<FJsonValueObject>(MakeObjectFromArray(PayloadValue->AsArray(), { TEXT("syncId"), TEXT("chunkIndex"), TEXT("isFinal"), TEXT("data") }))
				: PayloadValue;
		}

		if (EventName == TEXT("ws:m:topology-ack"))
		{
			return PayloadValue->Type == EJson::Array
				? MakeShared<FJsonValueObject>(MakeObjectFromArray(PayloadValue->AsArray(), { TEXT("syncId"), TEXT("chunkIndex") }))
				: PayloadValue;
		}

		if (EventName == TEXT("ws:m:ping"))
		{
			return PayloadValue->Type == EJson::Array
				? MakeShared<FJsonValueObject>(MakeObjectFromArray(PayloadValue->AsArray(), { TEXT("id"), TEXT("timestamp") }))
				: PayloadValue;
		}

		return PayloadValue;
	}

	static bool NormalizeMykoEnvelope(const TSharedPtr<FJsonValue>& RootValue, TSharedPtr<FJsonObject>& OutEnvelope)
	{
		if (!RootValue.IsValid())
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* RootObject = nullptr;
		if (RootValue->TryGetObject(RootObject) && RootObject && RootObject->IsValid())
		{
			FString ExistingEvent;
			if ((*RootObject)->TryGetStringField(TEXT("event"), ExistingEvent))
			{
				OutEnvelope = *RootObject;
				return true;
			}
		}

		TSharedPtr<FJsonValue> VariantValue;
		TSharedPtr<FJsonValue> PayloadValue;

		if (RootValue->Type == EJson::Object)
		{
			const TSharedPtr<FJsonObject> Object = RootValue->AsObject();
			if (!Object.IsValid() || Object->Values.Num() != 1)
			{
				return false;
			}

			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
			{
				VariantValue = MakeShared<FJsonValueString>(Pair.Key);
				PayloadValue = Pair.Value;
				break;
			}
		}
		else if (RootValue->Type == EJson::Array)
		{
			const TArray<TSharedPtr<FJsonValue>>& Values = RootValue->AsArray();
			if (Values.Num() != 2)
			{
				return false;
			}
			VariantValue = Values[0];
			PayloadValue = Values[1];
		}
		else
		{
			return false;
		}

		const FString EventName = ResolveMykoEventName(JsonValueKeyToString(VariantValue));
		if (EventName.IsEmpty())
		{
			return false;
		}

		if (PayloadValue.IsValid() && PayloadValue->Type == EJson::Array)
		{
			const TArray<TSharedPtr<FJsonValue>>& PayloadArray = PayloadValue->AsArray();
			if (PayloadArray.Num() == 1)
			{
				PayloadValue = PayloadArray[0];
			}
		}

		OutEnvelope = MakeShared<FJsonObject>();
		OutEnvelope->SetStringField(TEXT("event"), EventName);
		OutEnvelope->SetField(TEXT("data"), NormalizeMykoPayload(EventName, PayloadValue));
		return true;
	}
}

bool FRshipMykoTransport::DecodeMsgPackToJsonString(const TArray<uint8>& MessageBytes, FString& OutJsonString)
{
	OutJsonString.Reset();

	if (MessageBytes.Num() == 0)
	{
		return false;
	}

	rship_msgpack_reader_t Reader;
	msgpack_reader_init(&Reader, MessageBytes.GetData(), static_cast<size_t>(MessageBytes.Num()));

	TSharedPtr<FJsonValue> RootValue;
	if (!DecodeMsgPackValue(&Reader, RootValue) || !RootValue.IsValid())
	{
		return false;
	}

	TSharedPtr<FJsonObject> Envelope;
	if (!NormalizeMykoEnvelope(RootValue, Envelope) || !Envelope.IsValid())
	{
		return false;
	}

	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJsonString);
	return FJsonSerializer::Serialize(Envelope.ToSharedRef(), Writer);
}

bool FRshipMykoTransport::EncodeJsonStringToMsgPack(const FString& JsonString, TArray<uint8>& OutBytes)
{
	OutBytes.Reset();

	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return false;
	}

	int32 BufferSize = FMath::Max(JsonString.Len() * 4, 1024);
	for (int32 Attempt = 0; Attempt < 4; ++Attempt)
	{
		OutBytes.SetNumUninitialized(BufferSize);
		rship_msgpack_writer_t Writer;
		msgpack_writer_init(&Writer, OutBytes.GetData(), static_cast<size_t>(OutBytes.Num()));

		if (EncodeJsonObjectToMsgPack(RootObject, Writer) && !msgpack_writer_overflow(&Writer))
		{
			OutBytes.SetNum(static_cast<int32>(msgpack_writer_len(&Writer)));
			return true;
		}

		BufferSize *= 2;
	}

	OutBytes.Reset();
	return false;
}
