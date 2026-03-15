#include "RshipContentMappingTargetProxy.h"

#include "RshipContentMappingManager.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    TSharedPtr<FJsonObject> MakePayload()
    {
        return MakeShared<FJsonObject>();
    }
}

void URshipContentMappingTargetProxy::Initialize(URshipContentMappingManager* InManager, const FString& InTargetId)
{
    Manager = InManager;
    TargetId = InTargetId;
}

void URshipContentMappingTargetProxy::SetEnabled(bool enabled)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    Data->SetBoolField(TEXT("enabled"), enabled);
    RouteAction(TEXT("setEnabled"), Data);
}

void URshipContentMappingTargetProxy::SetSourceType(const FString& sourceType)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    Data->SetStringField(TEXT("sourceType"), sourceType);
    RouteAction(TEXT("setSourceType"), Data);
}

void URshipContentMappingTargetProxy::SetCameraId(const FString& cameraId)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    Data->SetStringField(TEXT("cameraId"), cameraId);
    RouteAction(TEXT("setCameraId"), Data);
}

void URshipContentMappingTargetProxy::SetAssetId(const FString& assetId)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    Data->SetStringField(TEXT("assetId"), assetId);
    RouteAction(TEXT("setAssetId"), Data);
}

void URshipContentMappingTargetProxy::SetExternalSourceId(const FString& externalSourceId)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    Data->SetStringField(TEXT("externalSourceId"), externalSourceId);
    RouteAction(TEXT("setExternalSourceId"), Data);
}

void URshipContentMappingTargetProxy::SetDepthAssetId(const FString& depthAssetId)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    Data->SetStringField(TEXT("depthAssetId"), depthAssetId);
    RouteAction(TEXT("setDepthAssetId"), Data);
}

void URshipContentMappingTargetProxy::SetDepthCaptureEnabled(bool depthCaptureEnabled)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    Data->SetBoolField(TEXT("depthCaptureEnabled"), depthCaptureEnabled);
    RouteAction(TEXT("setDepthCaptureEnabled"), Data);
}

void URshipContentMappingTargetProxy::SetDepthCaptureMode(const FString& depthCaptureMode)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    Data->SetStringField(TEXT("depthCaptureMode"), depthCaptureMode);
    RouteAction(TEXT("setDepthCaptureMode"), Data);
}

void URshipContentMappingTargetProxy::SetResolution(int32 width, int32 height)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    Data->SetNumberField(TEXT("width"), width);
    Data->SetNumberField(TEXT("height"), height);
    RouteAction(TEXT("setResolution"), Data);
}

void URshipContentMappingTargetProxy::SetCaptureMode(const FString& captureMode)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    Data->SetStringField(TEXT("captureMode"), captureMode);
    RouteAction(TEXT("setCaptureMode"), Data);
}

void URshipContentMappingTargetProxy::SetActorPath(const FString& actorPath)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    Data->SetStringField(TEXT("actorPath"), actorPath);
    RouteAction(TEXT("setActorPath"), Data);
}

void URshipContentMappingTargetProxy::SetUvChannel(int32 uvChannel)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    Data->SetNumberField(TEXT("uvChannel"), uvChannel);
    RouteAction(TEXT("setUvChannel"), Data);
}

void URshipContentMappingTargetProxy::SetMaterialSlots(const FString& materialSlots)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    const TArray<int32> Slots = ParseIntArray(materialSlots);
    TArray<TSharedPtr<FJsonValue>> SlotValues;
    SlotValues.Reserve(Slots.Num());
    for (int32 Slot : Slots)
    {
        SlotValues.Add(MakeShared<FJsonValueNumber>(Slot));
    }
    Data->SetArrayField(TEXT("materialSlots"), SlotValues);
    RouteAction(TEXT("setMaterialSlots"), Data);
}

void URshipContentMappingTargetProxy::SetMeshComponentName(const FString& meshComponentName)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    Data->SetStringField(TEXT("meshComponentName"), meshComponentName);
    RouteAction(TEXT("setMeshComponentName"), Data);
}

void URshipContentMappingTargetProxy::SetOpacity(float opacity)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    Data->SetNumberField(TEXT("opacity"), opacity);
    RouteAction(TEXT("setOpacity"), Data);
}

void URshipContentMappingTargetProxy::SetContextId(const FString& contextId)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    Data->SetStringField(TEXT("contextId"), contextId);
    RouteAction(TEXT("setContextId"), Data);
}

void URshipContentMappingTargetProxy::SetSurfaceIds(const FString& surfaceIds)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    const TArray<FString> Surfaces = ParseStringArray(surfaceIds);
    TArray<TSharedPtr<FJsonValue>> SurfaceValues;
    SurfaceValues.Reserve(Surfaces.Num());
    for (const FString& SurfaceId : Surfaces)
    {
        SurfaceValues.Add(MakeShared<FJsonValueString>(SurfaceId));
    }
    Data->SetArrayField(TEXT("surfaceIds"), SurfaceValues);
    RouteAction(TEXT("setSurfaceIds"), Data);
}

void URshipContentMappingTargetProxy::SetProjection(const FString& config)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    if (TSharedPtr<FJsonObject> ConfigObject = ParseJsonObject(config))
    {
        Data->SetObjectField(TEXT("config"), ConfigObject);
    }
    else if (!config.TrimStartAndEnd().IsEmpty())
    {
        Data->SetStringField(TEXT("projectionType"), config);
    }
    RouteAction(TEXT("setProjection"), Data);
}

void URshipContentMappingTargetProxy::SetUVTransform(const FString& uvMode, const FString& uvTransform)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    if (!uvMode.IsEmpty())
    {
        Data->SetStringField(TEXT("uvMode"), uvMode);
    }
    if (TSharedPtr<FJsonObject> TransformObject = ParseJsonObject(uvTransform))
    {
        Data->SetObjectField(TEXT("uvTransform"), TransformObject);
    }
    RouteAction(TEXT("setUVTransform"), Data);
}

void URshipContentMappingTargetProxy::SetType(const FString& type)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    Data->SetStringField(TEXT("type"), type);
    RouteAction(TEXT("setType"), Data);
}

void URshipContentMappingTargetProxy::SetConfig(const FString& config)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    if (TSharedPtr<FJsonObject> ConfigObject = ParseJsonObject(config))
    {
        Data->SetObjectField(TEXT("config"), ConfigObject);
    }
    RouteAction(TEXT("setConfig"), Data);
}

void URshipContentMappingTargetProxy::SetFeedV2(const FString& feedV2)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    if (TSharedPtr<FJsonObject> FeedObject = ParseJsonObject(feedV2))
    {
        Data->SetObjectField(TEXT("feedV2"), FeedObject);
    }
    RouteAction(TEXT("setFeedV2"), Data);
}

void URshipContentMappingTargetProxy::UpsertFeedSource(const FString& source)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    if (TSharedPtr<FJsonObject> SourceObject = ParseJsonObject(source))
    {
        Data->SetObjectField(TEXT("source"), SourceObject);
    }
    RouteAction(TEXT("upsertFeedSource"), Data);
}

void URshipContentMappingTargetProxy::RemoveFeedSource(const FString& sourceId)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    Data->SetStringField(TEXT("sourceId"), sourceId);
    RouteAction(TEXT("removeFeedSource"), Data);
}

void URshipContentMappingTargetProxy::UpsertFeedDestination(const FString& destination)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    if (TSharedPtr<FJsonObject> DestinationObject = ParseJsonObject(destination))
    {
        Data->SetObjectField(TEXT("destination"), DestinationObject);
    }
    RouteAction(TEXT("upsertFeedDestination"), Data);
}

void URshipContentMappingTargetProxy::RemoveFeedDestination(const FString& destinationId)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    Data->SetStringField(TEXT("destinationId"), destinationId);
    RouteAction(TEXT("removeFeedDestination"), Data);
}

void URshipContentMappingTargetProxy::UpsertFeedRoute(const FString& route)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    if (TSharedPtr<FJsonObject> RouteObject = ParseJsonObject(route))
    {
        Data->SetObjectField(TEXT("route"), RouteObject);
    }
    RouteAction(TEXT("upsertFeedRoute"), Data);
}

void URshipContentMappingTargetProxy::RemoveFeedRoute(const FString& routeId)
{
    TSharedPtr<FJsonObject> Data = MakePayload();
    Data->SetStringField(TEXT("routeId"), routeId);
    RouteAction(TEXT("removeFeedRoute"), Data);
}

bool URshipContentMappingTargetProxy::RouteAction(const FString& ActionName, const TSharedPtr<FJsonObject>& Data)
{
    if (!Manager || TargetId.IsEmpty())
    {
        return false;
    }

    const TSharedPtr<FJsonObject> Payload = Data.IsValid() ? Data : MakePayload();
    return Manager->RouteAction(TargetId, TargetId + TEXT(":") + ActionName, Payload.ToSharedRef());
}

TSharedPtr<FJsonObject> URshipContentMappingTargetProxy::ParseJsonObject(const FString& JsonString)
{
    const FString Trimmed = JsonString.TrimStartAndEnd();
    if (Trimmed.IsEmpty())
    {
        return nullptr;
    }

    TSharedPtr<FJsonObject> Parsed;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Trimmed);
    if (FJsonSerializer::Deserialize(Reader, Parsed) && Parsed.IsValid())
    {
        return Parsed;
    }

    return nullptr;
}

TArray<int32> URshipContentMappingTargetProxy::ParseIntArray(const FString& JsonString)
{
    const FString Trimmed = JsonString.TrimStartAndEnd();
    if (Trimmed.IsEmpty())
    {
        return {};
    }

    TArray<TSharedPtr<FJsonValue>> Parsed;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Trimmed);
    if (!FJsonSerializer::Deserialize(Reader, Parsed))
    {
        return {};
    }

    TArray<int32> Result;
    for (const TSharedPtr<FJsonValue>& Value : Parsed)
    {
        if (Value.IsValid() && Value->Type == EJson::Number)
        {
            Result.Add(static_cast<int32>(Value->AsNumber()));
        }
    }
    return Result;
}

TArray<FString> URshipContentMappingTargetProxy::ParseStringArray(const FString& JsonString)
{
    const FString Trimmed = JsonString.TrimStartAndEnd();
    if (Trimmed.IsEmpty())
    {
        return {};
    }

    TArray<TSharedPtr<FJsonValue>> Parsed;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Trimmed);
    if (!FJsonSerializer::Deserialize(Reader, Parsed))
    {
        return {};
    }

    TArray<FString> Result;
    for (const TSharedPtr<FJsonValue>& Value : Parsed)
    {
        if (Value.IsValid() && Value->Type == EJson::String)
        {
            Result.Add(Value->AsString());
        }
    }
    return Result;
}
