#include "RshipDisplayFFI.h"
#include "Logs.h"

#if RSHIP_HAS_DISPLAY_RUST
THIRD_PARTY_INCLUDES_START
#include "rship_display.h"
THIRD_PARTY_INCLUDES_END
#endif

namespace
{
#if RSHIP_HAS_DISPLAY_RUST
    FString ConsumeRustString(char* Ptr)
    {
        if (!Ptr)
        {
            return FString();
        }

        FString Result(UTF8_TO_TCHAR(Ptr));
        rship_display_free_string(Ptr);
        return Result;
    }
#endif
}

bool FRshipDisplayFFI::IsAvailable()
{
#if RSHIP_HAS_DISPLAY_RUST
    return true;
#else
    return false;
#endif
}

bool FRshipDisplayFFI::GetVersion(FString& OutVersionJson, FString& OutError)
{
#if RSHIP_HAS_DISPLAY_RUST
    return CallNoArg(&rship_display_version, OutVersionJson, OutError);
#else
    OutError = TEXT("Display Rust library is not available");
    return false;
#endif
}

bool FRshipDisplayFFI::CollectSnapshot(FString& OutEnvelopeJson, FString& OutError)
{
#if RSHIP_HAS_DISPLAY_RUST
    return CallNoArg(&rship_display_collect_snapshot_json, OutEnvelopeJson, OutError);
#else
    OutError = TEXT("Display Rust library is not available");
    return false;
#endif
}

bool FRshipDisplayFFI::BuildKnownFromSnapshot(const FString& SnapshotJson, FString& OutEnvelopeJson, FString& OutError)
{
#if RSHIP_HAS_DISPLAY_RUST
    return CallOneArg(&rship_display_build_known_from_snapshot_json, SnapshotJson, OutEnvelopeJson, OutError);
#else
    OutError = TEXT("Display Rust library is not available");
    return false;
#endif
}

bool FRshipDisplayFFI::ResolveIdentity(
    const FString& KnownJson,
    const FString& SnapshotJson,
    const FString& PinsJson,
    FString& OutEnvelopeJson,
    FString& OutError)
{
#if RSHIP_HAS_DISPLAY_RUST
    return CallThreeArgs(
        &rship_display_resolve_identity_json,
        KnownJson,
        SnapshotJson,
        PinsJson,
        OutEnvelopeJson,
        OutError);
#else
    OutError = TEXT("Display Rust library is not available");
    return false;
#endif
}

bool FRshipDisplayFFI::ValidateProfile(
    const FString& ProfileJson,
    const FString& SnapshotJson,
    FString& OutEnvelopeJson,
    FString& OutError)
{
#if RSHIP_HAS_DISPLAY_RUST
    return CallTwoArgs(&rship_display_validate_profile_json, ProfileJson, SnapshotJson, OutEnvelopeJson, OutError);
#else
    OutError = TEXT("Display Rust library is not available");
    return false;
#endif
}

bool FRshipDisplayFFI::PlanProfile(
    const FString& ProfileJson,
    const FString& SnapshotJson,
    const FString& KnownJson,
    FString& OutEnvelopeJson,
    FString& OutError)
{
#if RSHIP_HAS_DISPLAY_RUST
    return CallThreeArgs(&rship_display_plan_profile_json, ProfileJson, SnapshotJson, KnownJson, OutEnvelopeJson, OutError);
#else
    OutError = TEXT("Display Rust library is not available");
    return false;
#endif
}

bool FRshipDisplayFFI::ApplyPlan(
    const FString& PlanJson,
    bool bDryRun,
    FString& OutEnvelopeJson,
    FString& OutError)
{
#if RSHIP_HAS_DISPLAY_RUST
    FTCHARToUTF8 PlanUtf8(*PlanJson);
    char* Raw = rship_display_apply_plan_json(PlanUtf8.Get(), bDryRun);
    OutEnvelopeJson = ConsumeRustString(Raw);
    if (OutEnvelopeJson.IsEmpty())
    {
        OutError = TEXT("Empty response from rship_display_apply_plan_json");
        return false;
    }
    return true;
#else
    OutError = TEXT("Display Rust library is not available");
    return false;
#endif
}

bool FRshipDisplayFFI::CallNoArg(char* (*Fn)(), FString& OutEnvelopeJson, FString& OutError)
{
#if RSHIP_HAS_DISPLAY_RUST
    if (!Fn)
    {
        OutError = TEXT("Function pointer was null");
        return false;
    }

    char* Raw = Fn();
    OutEnvelopeJson = ConsumeRustString(Raw);
    if (OutEnvelopeJson.IsEmpty())
    {
        OutError = TEXT("FFI function returned an empty response");
        return false;
    }
    return true;
#else
    OutError = TEXT("Display Rust library is not available");
    return false;
#endif
}

bool FRshipDisplayFFI::CallOneArg(char* (*Fn)(const char*), const FString& Arg, FString& OutEnvelopeJson, FString& OutError)
{
#if RSHIP_HAS_DISPLAY_RUST
    if (!Fn)
    {
        OutError = TEXT("Function pointer was null");
        return false;
    }

    FTCHARToUTF8 ArgUtf8(*Arg);
    char* Raw = Fn(ArgUtf8.Get());
    OutEnvelopeJson = ConsumeRustString(Raw);
    if (OutEnvelopeJson.IsEmpty())
    {
        OutError = TEXT("FFI function returned an empty response");
        return false;
    }
    return true;
#else
    OutError = TEXT("Display Rust library is not available");
    return false;
#endif
}

bool FRshipDisplayFFI::CallTwoArgs(
    char* (*Fn)(const char*, const char*),
    const FString& ArgA,
    const FString& ArgB,
    FString& OutEnvelopeJson,
    FString& OutError)
{
#if RSHIP_HAS_DISPLAY_RUST
    if (!Fn)
    {
        OutError = TEXT("Function pointer was null");
        return false;
    }

    FTCHARToUTF8 ArgAUtf8(*ArgA);
    FTCHARToUTF8 ArgBUtf8(*ArgB);
    char* Raw = Fn(ArgAUtf8.Get(), ArgBUtf8.Get());
    OutEnvelopeJson = ConsumeRustString(Raw);
    if (OutEnvelopeJson.IsEmpty())
    {
        OutError = TEXT("FFI function returned an empty response");
        return false;
    }
    return true;
#else
    OutError = TEXT("Display Rust library is not available");
    return false;
#endif
}

bool FRshipDisplayFFI::CallThreeArgs(
    char* (*Fn)(const char*, const char*, const char*),
    const FString& ArgA,
    const FString& ArgB,
    const FString& ArgC,
    FString& OutEnvelopeJson,
    FString& OutError)
{
#if RSHIP_HAS_DISPLAY_RUST
    if (!Fn)
    {
        OutError = TEXT("Function pointer was null");
        return false;
    }

    FTCHARToUTF8 ArgAUtf8(*ArgA);
    FTCHARToUTF8 ArgBUtf8(*ArgB);
    FTCHARToUTF8 ArgCUtf8(*ArgC);
    char* Raw = Fn(ArgAUtf8.Get(), ArgBUtf8.Get(), ArgCUtf8.Get());
    OutEnvelopeJson = ConsumeRustString(Raw);
    if (OutEnvelopeJson.IsEmpty())
    {
        OutError = TEXT("FFI function returned an empty response");
        return false;
    }
    return true;
#else
    OutError = TEXT("Display Rust library is not available");
    return false;
#endif
}
