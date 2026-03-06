#include "Core/RshipEntitySerializer.h"

TArray<TSharedPtr<FJsonValue>> FRshipEntitySerializer::ToStringArray(const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> Out;
	Out.Reserve(Values.Num());
	for (const FString& Value : Values)
	{
		Out.Add(MakeShared<FJsonValueString>(Value));
	}
	return Out;
}

TSharedPtr<FJsonObject> FRshipEntitySerializer::ToJson(const FRshipMachineRecord& Record)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("id"), Record.Id);
	Json->SetStringField(TEXT("name"), Record.Name);
	Json->SetStringField(TEXT("execName"), Record.ExecName);
	Json->SetStringField(TEXT("clientId"), Record.ClientId);
	Json->SetArrayField(TEXT("addresses"), {});
	Json->SetStringField(TEXT("hash"), Record.Hash);
	return Json;
}

TSharedPtr<FJsonObject> FRshipEntitySerializer::ToJson(const FRshipInstanceRecord& Record)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("clientId"), Record.ClientId);
	Json->SetStringField(TEXT("name"), Record.Name);
	Json->SetStringField(TEXT("id"), Record.Id);
	Json->SetStringField(TEXT("clusterId"), Record.ClusterId);
	Json->SetStringField(TEXT("serviceTypeCode"), Record.ServiceTypeCode);
	Json->SetStringField(TEXT("serviceId"), Record.ServiceId);
	Json->SetStringField(TEXT("machineId"), Record.MachineId);
	Json->SetStringField(TEXT("status"), Record.Status);
	Json->SetStringField(TEXT("color"), Record.Color);
	Json->SetStringField(TEXT("hash"), Record.Hash);
	return Json;
}

TSharedPtr<FJsonObject> FRshipEntitySerializer::ToJson(const FRshipActionRecord& Record)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("id"), Record.Id);
	Json->SetStringField(TEXT("name"), Record.Name);
	Json->SetStringField(TEXT("targetId"), Record.TargetId);
	Json->SetStringField(TEXT("serviceId"), Record.ServiceId);
	if (Record.Schema.IsValid())
	{
		Json->SetObjectField(TEXT("schema"), Record.Schema);
	}
	Json->SetStringField(TEXT("hash"), Record.Hash);
	return Json;
}

TSharedPtr<FJsonObject> FRshipEntitySerializer::ToJson(const FRshipEmitterRecord& Record)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("id"), Record.Id);
	Json->SetStringField(TEXT("name"), Record.Name);
	Json->SetStringField(TEXT("targetId"), Record.TargetId);
	Json->SetStringField(TEXT("serviceId"), Record.ServiceId);
	if (Record.Schema.IsValid())
	{
		Json->SetObjectField(TEXT("schema"), Record.Schema);
	}
	Json->SetStringField(TEXT("hash"), Record.Hash);
	return Json;
}

TSharedPtr<FJsonObject> FRshipEntitySerializer::ToJson(const FRshipTargetRecord& Record)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("id"), Record.Id);
	Json->SetStringField(TEXT("name"), Record.Name);
	Json->SetStringField(TEXT("serviceId"), Record.ServiceId);
	Json->SetStringField(TEXT("category"), Record.Category);
	Json->SetStringField(TEXT("fgColor"), Record.ForegroundColor);
	Json->SetStringField(TEXT("bgColor"), Record.BackgroundColor);
	Json->SetArrayField(TEXT("actionIds"), ToStringArray(Record.ActionIds));
	Json->SetArrayField(TEXT("emitterIds"), ToStringArray(Record.EmitterIds));
	Json->SetArrayField(TEXT("tags"), ToStringArray(Record.Tags));
	Json->SetArrayField(TEXT("groupIds"), ToStringArray(Record.GroupIds));
	Json->SetArrayField(TEXT("parentTargets"), ToStringArray(Record.ParentTargetIds));
	Json->SetBoolField(TEXT("rootLevel"), Record.bRootLevel);
	Json->SetStringField(TEXT("hash"), Record.Hash);
	return Json;
}

TSharedPtr<FJsonObject> FRshipEntitySerializer::ToJson(const FRshipTargetStatusRecord& Record)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("id"), Record.Id);
	Json->SetStringField(TEXT("targetId"), Record.TargetId);
	Json->SetStringField(TEXT("instanceId"), Record.InstanceId);
	Json->SetStringField(TEXT("status"), Record.Status);
	Json->SetStringField(TEXT("hash"), Record.Hash);
	return Json;
}

TSharedPtr<FJsonObject> FRshipEntitySerializer::ToJson(const FRshipPulseRecord& Record)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("id"), Record.Id);
	Json->SetStringField(TEXT("emitterId"), Record.EmitterId);
	Json->SetObjectField(TEXT("data"), Record.Data.IsValid() ? Record.Data : MakeShared<FJsonObject>());
	Json->SetNumberField(TEXT("timestamp"), Record.TimestampMs);
	Json->SetStringField(TEXT("clientId"), Record.ClientId);
	Json->SetStringField(TEXT("hash"), Record.Hash);
	return Json;
}
