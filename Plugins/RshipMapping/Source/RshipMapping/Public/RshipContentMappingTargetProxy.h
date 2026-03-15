#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RshipContentMappingTargetProxy.generated.h"

class URshipContentMappingManager;
class FJsonObject;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRshipContentMappingTargetPulse);

UCLASS()
class RSHIPMAPPING_API URshipContentMappingTargetProxy : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(URshipContentMappingManager* InManager, const FString& InTargetId);

    UPROPERTY(BlueprintAssignable)
    FRshipContentMappingTargetPulse OnState;

    UPROPERTY(BlueprintAssignable)
    FRshipContentMappingTargetPulse OnStatus;

    UFUNCTION()
    void SetEnabled(bool enabled);

    UFUNCTION()
    void SetSourceType(const FString& sourceType);

    UFUNCTION()
    void SetCameraId(const FString& cameraId);

    UFUNCTION()
    void SetAssetId(const FString& assetId);

    UFUNCTION()
    void SetExternalSourceId(const FString& externalSourceId);

    UFUNCTION()
    void SetDepthAssetId(const FString& depthAssetId);

    UFUNCTION()
    void SetDepthCaptureEnabled(bool depthCaptureEnabled);

    UFUNCTION()
    void SetDepthCaptureMode(const FString& depthCaptureMode);

    UFUNCTION()
    void SetResolution(int32 width, int32 height);

    UFUNCTION()
    void SetCaptureMode(const FString& captureMode);

    UFUNCTION()
    void SetActorPath(const FString& actorPath);

    UFUNCTION()
    void SetUvChannel(int32 uvChannel);

    UFUNCTION()
    void SetMaterialSlots(const FString& materialSlots);

    UFUNCTION()
    void SetMeshComponentName(const FString& meshComponentName);

    UFUNCTION()
    void SetOpacity(float opacity);

    UFUNCTION()
    void SetContextId(const FString& contextId);

    UFUNCTION()
    void SetSurfaceIds(const FString& surfaceIds);

    UFUNCTION()
    void SetProjection(const FString& config);

    UFUNCTION()
    void SetUVTransform(const FString& uvMode, const FString& uvTransform);

    UFUNCTION()
    void SetType(const FString& type);

    UFUNCTION()
    void SetConfig(const FString& config);

    UFUNCTION()
    void SetFeedV2(const FString& feedV2);

    UFUNCTION()
    void UpsertFeedSource(const FString& source);

    UFUNCTION()
    void RemoveFeedSource(const FString& sourceId);

    UFUNCTION()
    void UpsertFeedDestination(const FString& destination);

    UFUNCTION()
    void RemoveFeedDestination(const FString& destinationId);

    UFUNCTION()
    void UpsertFeedRoute(const FString& route);

    UFUNCTION()
    void RemoveFeedRoute(const FString& routeId);

private:
    UPROPERTY(Transient)
    TObjectPtr<URshipContentMappingManager> Manager = nullptr;

    UPROPERTY()
    FString TargetId;

    bool RouteAction(const FString& ActionName, const TSharedPtr<FJsonObject>& Data);
    static TSharedPtr<FJsonObject> ParseJsonObject(const FString& JsonString);
    static TArray<int32> ParseIntArray(const FString& JsonString);
    static TArray<FString> ParseStringArray(const FString& JsonString);
};
