#pragma once

#include "CoreMinimal.h"

class FRshipDisplayFFI
{
public:
    static bool IsAvailable();

    static bool GetVersion(FString& OutVersionJson, FString& OutError);
    static bool CollectSnapshot(FString& OutEnvelopeJson, FString& OutError);
    static bool BuildKnownFromSnapshot(const FString& SnapshotJson, FString& OutEnvelopeJson, FString& OutError);
    static bool ResolveIdentity(
        const FString& KnownJson,
        const FString& SnapshotJson,
        const FString& PinsJson,
        FString& OutEnvelopeJson,
        FString& OutError);
    static bool ValidateProfile(
        const FString& ProfileJson,
        const FString& SnapshotJson,
        FString& OutEnvelopeJson,
        FString& OutError);
    static bool PlanProfile(
        const FString& ProfileJson,
        const FString& SnapshotJson,
        const FString& KnownJson,
        FString& OutEnvelopeJson,
        FString& OutError);
    static bool ApplyPlan(
        const FString& PlanJson,
        bool bDryRun,
        FString& OutEnvelopeJson,
        FString& OutError);

private:
    static bool CallNoArg(char* (*Fn)(), FString& OutEnvelopeJson, FString& OutError);
    static bool CallOneArg(char* (*Fn)(const char*), const FString& Arg, FString& OutEnvelopeJson, FString& OutError);
    static bool CallTwoArgs(
        char* (*Fn)(const char*, const char*),
        const FString& ArgA,
        const FString& ArgB,
        FString& OutEnvelopeJson,
        FString& OutError);
    static bool CallThreeArgs(
        char* (*Fn)(const char*, const char*, const char*),
        const FString& ArgA,
        const FString& ArgB,
        const FString& ArgC,
        FString& OutEnvelopeJson,
        FString& OutError);
};
