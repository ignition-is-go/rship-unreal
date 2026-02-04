// Editor-only projector/frustum visualization for content mapping

#include "RshipContentMappingPreviewActor.h"
#include "DrawDebugHelpers.h"

ARshipContentMappingPreviewActor::ARshipContentMappingPreviewActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    SetActorHiddenInGame(true);
}

void ARshipContentMappingPreviewActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UWorld* World = GetWorld();
    if (!World) return;

    const FVector Origin = ProjectorPosition;
    const FRotationMatrix Rot(ProjectorRotation);
    const FVector Forward = Rot.GetUnitAxis(EAxis::X);
    const FVector Right = Rot.GetUnitAxis(EAxis::Y);
    const FVector Up = Rot.GetUnitAxis(EAxis::Z);

    const float NearDist = NearClip;
    const float FarDist = FarClip;
    const float HalfFovRad = FMath::DegreesToRadians(FOV * 0.5f);
    const float NearHeight = 2.f * FMath::Tan(HalfFovRad) * NearDist;
    const float NearWidth = NearHeight * Aspect;
    const float FarHeight = 2.f * FMath::Tan(HalfFovRad) * FarDist;
    const float FarWidth = FarHeight * Aspect;

    auto BuildCorners = [&](float Dist, float W, float H)
    {
        const FVector Center = Origin + Forward * Dist;
        const FVector UpVec = Up * (H * 0.5f);
        const FVector RightVec = Right * (W * 0.5f);
        FVector TL = Center + UpVec - RightVec;
        FVector TR = Center + UpVec + RightVec;
        FVector BL = Center - UpVec - RightVec;
        FVector BR = Center - UpVec + RightVec;
        return TArray<FVector>{TL, TR, BL, BR};
    };

    TArray<FVector> NearCorners = BuildCorners(NearDist, NearWidth, NearHeight);
    TArray<FVector> FarCorners = BuildCorners(FarDist, FarWidth, FarHeight);

    auto DrawQuad = [&](const TArray<FVector>& C)
    {
        DrawDebugLine(World, C[0], C[1], LineColor, false, -1.f, 0, 1.5f);
        DrawDebugLine(World, C[1], C[3], LineColor, false, -1.f, 0, 1.5f);
        DrawDebugLine(World, C[3], C[2], LineColor, false, -1.f, 0, 1.5f);
        DrawDebugLine(World, C[2], C[0], LineColor, false, -1.f, 0, 1.5f);
    };

    DrawQuad(NearCorners);
    DrawQuad(FarCorners);

    for (int32 i = 0; i < 4; ++i)
    {
        DrawDebugLine(World, NearCorners[i], FarCorners[i], LineColor, false, -1.f, 0, 1.0f);
    }
}
