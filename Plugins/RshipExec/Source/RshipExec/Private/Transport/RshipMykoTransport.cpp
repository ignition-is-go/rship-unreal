#include "Transport/RshipMykoTransport.h"

#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "HAL/PlatformProcess.h"

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
	return FPlatformProcess::ComputerName();
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
