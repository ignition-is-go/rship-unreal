#include "RshipDisplayManager.h"

#include "Logs.h"
#include "RshipDisplayFFI.h"
#include "RshipSettings.h"
#include "RshipSubsystem.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    FString JsonValueToString(const TSharedPtr<FJsonValue>& Value)
    {
        if (!Value.IsValid())
        {
            return TEXT("");
        }

        switch (Value->Type)
        {
        case EJson::Null:
            return TEXT("null");
        case EJson::String:
            return FString::Printf(TEXT("\"%s\""), *Value->AsString().ReplaceCharWithEscapedChar());
        case EJson::Number:
            return LexToString(Value->AsNumber());
        case EJson::Boolean:
            return Value->AsBool() ? TEXT("true") : TEXT("false");
        case EJson::Object:
        {
            FString Output;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
            FJsonSerializer::Serialize(Value->AsObject().ToSharedRef(), Writer);
            return Output;
        }
        case EJson::Array:
        {
            FString Output;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
            FJsonSerializer::Serialize(Value->AsArray(), Writer);
            return Output;
        }
        default:
            return TEXT("");
        }
    }

    TSharedPtr<FJsonObject> ParseObject(const FString& Json)
    {
        if (Json.IsEmpty())
        {
            return nullptr;
        }

        TSharedPtr<FJsonObject> Object;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
        {
            return nullptr;
        }
        return Object;
    }
}

void URshipDisplayManager::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;
    LastError.Empty();
    DebugOverlayAccumulated = 0.0f;

    if (!FRshipDisplayFFI::IsAvailable())
    {
        LastError = TEXT("Display Rust runtime unavailable (RSHIP_HAS_DISPLAY_RUST=0)");
    }

    LoadStateCache();

    const URshipSettings* Settings = GetDefault<URshipSettings>();
    if (Settings)
    {
        bDebugOverlayEnabled = Settings->bDisplayManagementDebugOverlay;

        if (!Settings->DisplayManagementProfilePath.IsEmpty())
        {
            FString Loaded;
            if (FFileHelper::LoadFileToString(Loaded, *Settings->DisplayManagementProfilePath))
            {
                ActiveProfileJson = Loaded;
                SaveStateCache();
            }
        }

        if (Settings->bDisplayManagementCollectOnStartup)
        {
            CollectSnapshot();
            BuildKnownDisplays();
        }
    }
}

void URshipDisplayManager::Shutdown()
{
    SaveStateCache();
    Subsystem = nullptr;
}

void URshipDisplayManager::Tick(float DeltaTime)
{
    if (!Subsystem)
    {
        return;
    }

    const bool bConnected = Subsystem->IsConnected();
    if (bConnected && !bWasConnected)
    {
        RegisterTarget();
        EmitState(TEXT("ready"));
    }
    bWasConnected = bConnected;

    if (bDebugOverlayEnabled && GEngine)
    {
        DebugOverlayAccumulated += DeltaTime;
        if (DebugOverlayAccumulated >= 0.5f)
        {
            DebugOverlayAccumulated = 0.0f;

            const FString StatusText = FString::Printf(
                TEXT("Rship Display Mgmt\nSnapshot: %s  Known: %s  Plan: %s\nLastError: %s"),
                LastSnapshotJson.IsEmpty() ? TEXT("no") : TEXT("yes"),
                LastKnownJson.IsEmpty() ? TEXT("no") : TEXT("yes"),
                LastPlanJson.IsEmpty() ? TEXT("no") : TEXT("yes"),
                LastError.IsEmpty() ? TEXT("none") : *LastError);

            GEngine->AddOnScreenDebugMessage(0xD15A11, 0.6f, FColor::Green, StatusText);
        }
    }
}

void URshipDisplayManager::ProcessProfileEvent(const TSharedPtr<FJsonObject>& Data, bool bIsDelete)
{
    if (bIsDelete)
    {
        ActiveProfileJson.Empty();
        SaveStateCache();
        EmitState(TEXT("profile-deleted"));
        return;
    }

    if (!Data.IsValid())
    {
        return;
    }

    FString Serialized;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
    FJsonSerializer::Serialize(Data.ToSharedRef(), Writer);
    ActiveProfileJson = Serialized;
    SaveStateCache();

    EmitState(TEXT("profile-updated"));
}

bool URshipDisplayManager::RouteAction(const FString& TargetId, const FString& ActionId, const TSharedRef<FJsonObject>& Data)
{
    if (TargetId != GetTargetId())
    {
        return false;
    }

    const FString ActionName = ExtractActionName(ActionId);

    if (ActionName == TEXT("setProfileJson"))
    {
        if (!Data->HasTypedField<EJson::String>(TEXT("profileJson")))
        {
            LastError = TEXT("setProfileJson requires profileJson");
            EmitState(TEXT("profile-failed"));
            return false;
        }
        ActiveProfileJson = Data->GetStringField(TEXT("profileJson"));
        SaveStateCache();
        EmitState(TEXT("profile-set"));
        return true;
    }

    if (ActionName == TEXT("collectSnapshot"))
    {
        const bool bOk = CollectSnapshot();
        EmitState(bOk ? TEXT("snapshot-updated") : TEXT("snapshot-failed"));
        return bOk;
    }

    if (ActionName == TEXT("buildKnown"))
    {
        const bool bOk = BuildKnownDisplays();
        EmitState(bOk ? TEXT("known-updated") : TEXT("known-failed"));
        return bOk;
    }

    if (ActionName == TEXT("resolveIdentity"))
    {
        FString PinsJson;
        if (Data->HasTypedField<EJson::String>(TEXT("pinsJson")))
        {
            PinsJson = Data->GetStringField(TEXT("pinsJson"));
        }
        const bool bOk = ResolveIdentity(PinsJson);
        EmitState(bOk ? TEXT("identity-updated") : TEXT("identity-failed"));
        return bOk;
    }

    if (ActionName == TEXT("validateProfile"))
    {
        FString ProfileJson = ActiveProfileJson;
        if (Data->HasTypedField<EJson::String>(TEXT("profileJson")))
        {
            ProfileJson = Data->GetStringField(TEXT("profileJson"));
        }
        const bool bOk = ValidateProfileJson(ProfileJson);
        EmitState(bOk ? TEXT("validation-updated") : TEXT("validation-failed"));
        return bOk;
    }

    if (ActionName == TEXT("planProfile"))
    {
        FString ProfileJson = ActiveProfileJson;
        if (Data->HasTypedField<EJson::String>(TEXT("profileJson")))
        {
            ProfileJson = Data->GetStringField(TEXT("profileJson"));
        }
        const bool bOk = PlanProfileJson(ProfileJson);
        EmitState(bOk ? TEXT("plan-updated") : TEXT("plan-failed"));
        return bOk;
    }

    if (ActionName == TEXT("applyPlan"))
    {
        const bool bDryRun = Data->HasTypedField<EJson::Boolean>(TEXT("dryRun"))
            ? Data->GetBoolField(TEXT("dryRun"))
            : true;
        const bool bOk = ApplyLastPlan(bDryRun);
        EmitState(bOk ? TEXT("apply-updated") : TEXT("apply-failed"));
        return bOk;
    }

    if (ActionName == TEXT("setDebug"))
    {
        const bool bEnabled = Data->HasTypedField<EJson::Boolean>(TEXT("enabled"))
            ? Data->GetBoolField(TEXT("enabled"))
            : false;
        SetDebugOverlayEnabled(bEnabled);
        EmitState(TEXT("debug-updated"));
        return true;
    }

    return false;
}

bool URshipDisplayManager::CollectSnapshot()
{
    FString Envelope;
    FString Error;
    if (!FRshipDisplayFFI::CollectSnapshot(Envelope, Error))
    {
        LastError = Error;
        return false;
    }

    FString DataJson;
    if (!ParseEnvelope(Envelope, DataJson, Error))
    {
        LastError = Error;
        return false;
    }

    LastSnapshotJson = DataJson;
    LastError.Empty();
    SaveStateCache();
    PulseJsonEmitter(TEXT("snapshot"), LastSnapshotJson);
    return true;
}

bool URshipDisplayManager::BuildKnownDisplays()
{
    if (LastSnapshotJson.IsEmpty())
    {
        if (!CollectSnapshot())
        {
            return false;
        }
    }

    FString Envelope;
    FString Error;
    if (!FRshipDisplayFFI::BuildKnownFromSnapshot(LastSnapshotJson, Envelope, Error))
    {
        LastError = Error;
        return false;
    }

    FString DataJson;
    if (!ParseEnvelope(Envelope, DataJson, Error))
    {
        LastError = Error;
        return false;
    }

    LastKnownJson = DataJson;
    LastError.Empty();
    SaveStateCache();
    PulseJsonEmitter(TEXT("known"), LastKnownJson);
    return true;
}

bool URshipDisplayManager::ResolveIdentity(const FString& PinsJson)
{
    if (LastKnownJson.IsEmpty() && !BuildKnownDisplays())
    {
        return false;
    }
    if (LastSnapshotJson.IsEmpty() && !CollectSnapshot())
    {
        return false;
    }

    FString Envelope;
    FString Error;
    if (!FRshipDisplayFFI::ResolveIdentity(LastKnownJson, LastSnapshotJson, PinsJson, Envelope, Error))
    {
        LastError = Error;
        return false;
    }

    FString DataJson;
    if (!ParseEnvelope(Envelope, DataJson, Error))
    {
        LastError = Error;
        return false;
    }

    LastIdentityJson = DataJson;
    LastError.Empty();
    SaveStateCache();
    PulseJsonEmitter(TEXT("identity"), LastIdentityJson);
    return true;
}

bool URshipDisplayManager::ValidateProfileJson(const FString& ProfileJson)
{
    if (ProfileJson.IsEmpty())
    {
        LastError = TEXT("ProfileJson is empty");
        return false;
    }

    FString Envelope;
    FString Error;
    if (!FRshipDisplayFFI::ValidateProfile(ProfileJson, LastSnapshotJson, Envelope, Error))
    {
        LastError = Error;
        return false;
    }

    FString DataJson;
    if (!ParseEnvelope(Envelope, DataJson, Error))
    {
        LastError = Error;
        return false;
    }

    LastValidationJson = DataJson;
    LastError.Empty();
    SaveStateCache();
    PulseJsonEmitter(TEXT("validation"), LastValidationJson);
    return true;
}

bool URshipDisplayManager::PlanProfileJson(const FString& ProfileJson)
{
    if (ProfileJson.IsEmpty())
    {
        LastError = TEXT("ProfileJson is empty");
        return false;
    }

    if (LastSnapshotJson.IsEmpty() && !CollectSnapshot())
    {
        return false;
    }
    if (LastKnownJson.IsEmpty() && !BuildKnownDisplays())
    {
        return false;
    }

    FString Envelope;
    FString Error;
    if (!FRshipDisplayFFI::PlanProfile(ProfileJson, LastSnapshotJson, LastKnownJson, Envelope, Error))
    {
        LastError = Error;
        return false;
    }

    FString DataJson;
    if (!ParseEnvelope(Envelope, DataJson, Error))
    {
        LastError = Error;
        return false;
    }

    TSharedPtr<FJsonObject> DataObj = ParseObject(DataJson);
    if (!DataObj.IsValid() || !DataObj->HasTypedField<EJson::Object>(TEXT("plan")))
    {
        LastError = TEXT("Plan response is missing the plan field");
        return false;
    }

    const TSharedPtr<FJsonValue> PlanValue = DataObj->TryGetField(TEXT("plan"));
    LastPlanJson = JsonValueToString(PlanValue);
    if (DataObj->HasTypedField<EJson::Object>(TEXT("identity")))
    {
        LastIdentityJson = JsonValueToString(DataObj->TryGetField(TEXT("identity")));
    }
    if (DataObj->HasTypedField<EJson::Object>(TEXT("validation")))
    {
        LastValidationJson = JsonValueToString(DataObj->TryGetField(TEXT("validation")));
    }
    if (DataObj->HasTypedField<EJson::Object>(TEXT("ledger")))
    {
        LastLedgerJson = JsonValueToString(DataObj->TryGetField(TEXT("ledger")));
    }

    LastError.Empty();
    SaveStateCache();
    PulseJsonEmitter(TEXT("plan"), LastPlanJson);
    if (!LastIdentityJson.IsEmpty())
    {
        PulseJsonEmitter(TEXT("identity"), LastIdentityJson);
    }
    if (!LastValidationJson.IsEmpty())
    {
        PulseJsonEmitter(TEXT("validation"), LastValidationJson);
    }
    if (!LastLedgerJson.IsEmpty())
    {
        PulseJsonEmitter(TEXT("ledger"), LastLedgerJson);
    }
    return true;
}

bool URshipDisplayManager::ApplyLastPlan(bool bDryRun)
{
    if (LastPlanJson.IsEmpty())
    {
        LastError = TEXT("No plan available. Run plan first.");
        return false;
    }

    FString Envelope;
    FString Error;
    if (!FRshipDisplayFFI::ApplyPlan(LastPlanJson, bDryRun, Envelope, Error))
    {
        LastError = Error;
        return false;
    }

    FString DataJson;
    if (!ParseEnvelope(Envelope, DataJson, Error))
    {
        LastError = Error;
        return false;
    }

    LastApplyJson = DataJson;
    LastError.Empty();
    SaveStateCache();
    PulseJsonEmitter(TEXT("apply"), LastApplyJson);
    return true;
}

void URshipDisplayManager::SetDebugOverlayEnabled(bool bEnabled)
{
    bDebugOverlayEnabled = bEnabled;
    DebugOverlayAccumulated = 0.0f;
}

bool URshipDisplayManager::IsDebugOverlayEnabled() const
{
    return bDebugOverlayEnabled;
}

FString URshipDisplayManager::GetActiveProfileJson() const
{
    return ActiveProfileJson;
}

FString URshipDisplayManager::GetLastSnapshotJson() const
{
    return LastSnapshotJson;
}

FString URshipDisplayManager::GetLastKnownJson() const
{
    return LastKnownJson;
}

FString URshipDisplayManager::GetLastIdentityJson() const
{
    return LastIdentityJson;
}

FString URshipDisplayManager::GetLastValidationJson() const
{
    return LastValidationJson;
}

FString URshipDisplayManager::GetLastPlanJson() const
{
    return LastPlanJson;
}

FString URshipDisplayManager::GetLastLedgerJson() const
{
    return LastLedgerJson;
}

FString URshipDisplayManager::GetLastApplyJson() const
{
    return LastApplyJson;
}

FString URshipDisplayManager::GetLastError() const
{
    return LastError;
}

FString URshipDisplayManager::GetTargetId()
{
    return TEXT("/display-management/system");
}

FString URshipDisplayManager::ExtractActionName(const FString& ActionId)
{
    int32 Index = INDEX_NONE;
    if (ActionId.FindLastChar(TEXT(':'), Index))
    {
        return ActionId.Mid(Index + 1);
    }
    return ActionId;
}

bool URshipDisplayManager::ParseEnvelope(const FString& EnvelopeJson, FString& OutDataJson, FString& OutError) const
{
    TSharedPtr<FJsonObject> Envelope;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(EnvelopeJson);
    if (!FJsonSerializer::Deserialize(Reader, Envelope) || !Envelope.IsValid())
    {
        OutError = TEXT("Failed to parse Rust envelope JSON");
        return false;
    }

    const bool bOk = Envelope->HasTypedField<EJson::Boolean>(TEXT("ok"))
        ? Envelope->GetBoolField(TEXT("ok"))
        : false;

    if (!bOk)
    {
        OutError = Envelope->HasTypedField<EJson::String>(TEXT("error"))
            ? Envelope->GetStringField(TEXT("error"))
            : TEXT("Rust operation failed");
        return false;
    }

    const TSharedPtr<FJsonValue>* DataValue = Envelope->Values.Find(TEXT("data"));
    if (!DataValue || !DataValue->IsValid())
    {
        OutError = TEXT("Rust envelope is missing data payload");
        return false;
    }

    OutDataJson = JsonValueToString(*DataValue);
    return true;
}

void URshipDisplayManager::RegisterTarget()
{
    if (!Subsystem || !Subsystem->IsConnected())
    {
        return;
    }

    const FString TargetId = GetTargetId();
    const FString ServiceId = Subsystem->GetServiceId();

    TArray<TSharedPtr<FJsonValue>> ActionIds;
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":setProfileJson")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":collectSnapshot")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":buildKnown")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":resolveIdentity")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":validateProfile")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":planProfile")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":applyPlan")));
    ActionIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":setDebug")));

    TArray<TSharedPtr<FJsonValue>> EmitterIds;
    EmitterIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":state")));
    EmitterIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":status")));
    EmitterIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":snapshot")));
    EmitterIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":known")));
    EmitterIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":identity")));
    EmitterIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":validation")));
    EmitterIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":plan")));
    EmitterIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":ledger")));
    EmitterIds.Add(MakeShared<FJsonValueString>(TargetId + TEXT(":apply")));

    TSharedPtr<FJsonObject> TargetJson = MakeShared<FJsonObject>();
    TargetJson->SetStringField(TEXT("id"), TargetId);
    TargetJson->SetStringField(TEXT("name"), TEXT("Display Management"));
    TargetJson->SetStringField(TEXT("serviceId"), ServiceId);
    TargetJson->SetStringField(TEXT("category"), TEXT("display-management"));
    TargetJson->SetArrayField(TEXT("actionIds"), ActionIds);
    TargetJson->SetArrayField(TEXT("emitterIds"), EmitterIds);
    TargetJson->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
    Subsystem->SetItem(TEXT("Target"), TargetJson, ERshipMessagePriority::High, TargetId);

    auto RegisterAction = [&](const FString& Name)
    {
        TSharedPtr<FJsonObject> ActionJson = MakeShared<FJsonObject>();
        ActionJson->SetStringField(TEXT("id"), TargetId + TEXT(":") + Name);
        ActionJson->SetStringField(TEXT("name"), Name);
        ActionJson->SetStringField(TEXT("targetId"), TargetId);
        ActionJson->SetStringField(TEXT("serviceId"), ServiceId);

        TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
        Schema->SetStringField(TEXT("type"), TEXT("object"));
        ActionJson->SetObjectField(TEXT("schema"), Schema);
        ActionJson->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));

        Subsystem->SetItem(
            TEXT("Action"),
            ActionJson,
            ERshipMessagePriority::High,
            ActionJson->GetStringField(TEXT("id")));
    };

    RegisterAction(TEXT("setProfileJson"));
    RegisterAction(TEXT("collectSnapshot"));
    RegisterAction(TEXT("buildKnown"));
    RegisterAction(TEXT("resolveIdentity"));
    RegisterAction(TEXT("validateProfile"));
    RegisterAction(TEXT("planProfile"));
    RegisterAction(TEXT("applyPlan"));
    RegisterAction(TEXT("setDebug"));

    auto RegisterEmitter = [&](const FString& Name)
    {
        TSharedPtr<FJsonObject> EmitterJson = MakeShared<FJsonObject>();
        EmitterJson->SetStringField(TEXT("id"), TargetId + TEXT(":") + Name);
        EmitterJson->SetStringField(TEXT("name"), Name);
        EmitterJson->SetStringField(TEXT("targetId"), TargetId);
        EmitterJson->SetStringField(TEXT("serviceId"), ServiceId);

        TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
        Schema->SetStringField(TEXT("type"), TEXT("object"));
        EmitterJson->SetObjectField(TEXT("schema"), Schema);
        EmitterJson->SetStringField(TEXT("hash"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));

        Subsystem->SetItem(
            TEXT("Emitter"),
            EmitterJson,
            ERshipMessagePriority::High,
            EmitterJson->GetStringField(TEXT("id")));
    };

    RegisterEmitter(TEXT("state"));
    RegisterEmitter(TEXT("status"));
    RegisterEmitter(TEXT("snapshot"));
    RegisterEmitter(TEXT("known"));
    RegisterEmitter(TEXT("identity"));
    RegisterEmitter(TEXT("validation"));
    RegisterEmitter(TEXT("plan"));
    RegisterEmitter(TEXT("ledger"));
    RegisterEmitter(TEXT("apply"));
}

void URshipDisplayManager::EmitState(const FString& Status)
{
    if (!Subsystem)
    {
        return;
    }

    TSharedPtr<FJsonObject> StatePayload = MakeShared<FJsonObject>();
    StatePayload->SetStringField(TEXT("status"), Status);
    StatePayload->SetBoolField(TEXT("hasProfile"), !ActiveProfileJson.IsEmpty());
    StatePayload->SetBoolField(TEXT("hasSnapshot"), !LastSnapshotJson.IsEmpty());
    StatePayload->SetBoolField(TEXT("hasKnown"), !LastKnownJson.IsEmpty());
    StatePayload->SetBoolField(TEXT("hasPlan"), !LastPlanJson.IsEmpty());
    StatePayload->SetBoolField(TEXT("hasLedger"), !LastLedgerJson.IsEmpty());
    StatePayload->SetBoolField(TEXT("debug"), bDebugOverlayEnabled);

    Subsystem->PulseEmitter(GetTargetId(), TEXT("state"), StatePayload);

    TSharedPtr<FJsonObject> StatusPayload = MakeShared<FJsonObject>();
    StatusPayload->SetStringField(TEXT("status"), LastError.IsEmpty() ? TEXT("ok") : TEXT("error"));
    if (!LastError.IsEmpty())
    {
        StatusPayload->SetStringField(TEXT("lastError"), LastError);
    }
    Subsystem->PulseEmitter(GetTargetId(), TEXT("status"), StatusPayload);
}

void URshipDisplayManager::PulseJsonEmitter(const FString& EmitterName, const FString& JsonPayload) const
{
    if (!Subsystem || JsonPayload.IsEmpty())
    {
        return;
    }

    TSharedPtr<FJsonObject> PayloadObj = ParseObject(JsonPayload);
    if (PayloadObj.IsValid())
    {
        Subsystem->PulseEmitter(GetTargetId(), EmitterName, PayloadObj);
        return;
    }

    TSharedPtr<FJsonObject> Raw = MakeShared<FJsonObject>();
    Raw->SetStringField(TEXT("raw"), JsonPayload);
    Subsystem->PulseEmitter(GetTargetId(), EmitterName, Raw);
}

FString URshipDisplayManager::GetStateCachePath() const
{
    const URshipSettings* Settings = GetDefault<URshipSettings>();
    if (Settings && !Settings->DisplayManagementStateCachePath.IsEmpty())
    {
        return Settings->DisplayManagementStateCachePath;
    }

    return FPaths::ProjectSavedDir() / TEXT("Rship/DisplayStateCache.json");
}

void URshipDisplayManager::LoadStateCache()
{
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *GetStateCachePath()))
    {
        return;
    }

    TSharedPtr<FJsonObject> Root = ParseObject(JsonString);
    if (!Root.IsValid())
    {
        return;
    }

    if (Root->HasTypedField<EJson::String>(TEXT("activeProfileJson")))
    {
        ActiveProfileJson = Root->GetStringField(TEXT("activeProfileJson"));
    }
    if (Root->HasTypedField<EJson::String>(TEXT("lastSnapshotJson")))
    {
        LastSnapshotJson = Root->GetStringField(TEXT("lastSnapshotJson"));
    }
    if (Root->HasTypedField<EJson::String>(TEXT("lastKnownJson")))
    {
        LastKnownJson = Root->GetStringField(TEXT("lastKnownJson"));
    }
    if (Root->HasTypedField<EJson::String>(TEXT("lastIdentityJson")))
    {
        LastIdentityJson = Root->GetStringField(TEXT("lastIdentityJson"));
    }
    if (Root->HasTypedField<EJson::String>(TEXT("lastValidationJson")))
    {
        LastValidationJson = Root->GetStringField(TEXT("lastValidationJson"));
    }
    if (Root->HasTypedField<EJson::String>(TEXT("lastPlanJson")))
    {
        LastPlanJson = Root->GetStringField(TEXT("lastPlanJson"));
    }
    if (Root->HasTypedField<EJson::String>(TEXT("lastLedgerJson")))
    {
        LastLedgerJson = Root->GetStringField(TEXT("lastLedgerJson"));
    }
    if (Root->HasTypedField<EJson::String>(TEXT("lastApplyJson")))
    {
        LastApplyJson = Root->GetStringField(TEXT("lastApplyJson"));
    }
}

void URshipDisplayManager::SaveStateCache() const
{
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("activeProfileJson"), ActiveProfileJson);
    Root->SetStringField(TEXT("lastSnapshotJson"), LastSnapshotJson);
    Root->SetStringField(TEXT("lastKnownJson"), LastKnownJson);
    Root->SetStringField(TEXT("lastIdentityJson"), LastIdentityJson);
    Root->SetStringField(TEXT("lastValidationJson"), LastValidationJson);
    Root->SetStringField(TEXT("lastPlanJson"), LastPlanJson);
    Root->SetStringField(TEXT("lastLedgerJson"), LastLedgerJson);
    Root->SetStringField(TEXT("lastApplyJson"), LastApplyJson);

    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
    {
        return;
    }

    const FString CachePath = GetStateCachePath();
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(CachePath), true);
    FFileHelper::SaveStringToFile(Output, *CachePath);
}
