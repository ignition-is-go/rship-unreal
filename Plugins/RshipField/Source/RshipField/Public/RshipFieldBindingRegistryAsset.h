#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RshipFieldTypes.h"
#include "RshipFieldBindingRegistryAsset.generated.h"

UCLASS(BlueprintType)
class RSHIPFIELD_API URshipFieldBindingRegistryAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|Field")
    TArray<FRshipFieldTargetIdentity> RegisteredTargets;

    const FRshipFieldTargetIdentity* FindByStableGuid(const FGuid& StableGuid) const
    {
        if (!StableGuid.IsValid())
        {
            return nullptr;
        }

        for (const FRshipFieldTargetIdentity& Entry : RegisteredTargets)
        {
            if (Entry.StableGuid == StableGuid)
            {
                return &Entry;
            }
        }
        return nullptr;
    }

    const FRshipFieldTargetIdentity* FindByVisibleTargetPath(const FString& VisibleTargetPath) const
    {
        if (VisibleTargetPath.IsEmpty())
        {
            return nullptr;
        }

        for (const FRshipFieldTargetIdentity& Entry : RegisteredTargets)
        {
            if (Entry.VisibleTargetPath == VisibleTargetPath)
            {
                return &Entry;
            }
        }
        return nullptr;
    }

    const FRshipFieldTargetIdentity* FindByFingerprint(const FString& ActorPath, const FString& ComponentName, const FString& MeshPath) const
    {
        for (const FRshipFieldTargetIdentity& Entry : RegisteredTargets)
        {
            if (Entry.ActorPath == ActorPath && Entry.ComponentName == ComponentName && Entry.MeshPath == MeshPath)
            {
                return &Entry;
            }
        }
        return nullptr;
    }

    void UpsertIdentity(const FRshipFieldTargetIdentity& Identity)
    {
        if (!Identity.StableGuid.IsValid())
        {
            return;
        }

        for (FRshipFieldTargetIdentity& Entry : RegisteredTargets)
        {
            if (Entry.StableGuid == Identity.StableGuid)
            {
                Entry = Identity;
                return;
            }
        }

        RegisteredTargets.Add(Identity);
    }
};
