#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RshipFieldBindingRegistryAsset.h"
#include "RshipFieldSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRshipFieldRegistryIdentityTest,
    "Rship.Field.Registry.IdentityLookupAndUpsert",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRshipFieldRegistryIdentityTest::RunTest(const FString& Parameters)
{
    URshipFieldBindingRegistryAsset* Registry = NewObject<URshipFieldBindingRegistryAsset>();
    TestNotNull(TEXT("Registry object"), Registry);

    FRshipFieldTargetIdentity Identity;
    Identity.VisibleTargetPath = TEXT("/field/TestActor/TestComp");
    Identity.StableGuid = FGuid(1, 2, 3, 4);
    Identity.ActorPath = TEXT("/Game/TestActor");
    Identity.ComponentName = TEXT("TestComp");
    Identity.MeshPath = TEXT("/Game/TestMesh");

    Registry->UpsertIdentity(Identity);
    TestEqual(TEXT("Registry has one entry"), Registry->RegisteredTargets.Num(), 1);

    const FRshipFieldTargetIdentity* ByGuid = Registry->FindByStableGuid(Identity.StableGuid);
    TestNotNull(TEXT("Find by stable guid"), ByGuid);

    const FRshipFieldTargetIdentity* ByPath = Registry->FindByVisibleTargetPath(Identity.VisibleTargetPath);
    TestNotNull(TEXT("Find by visible target path"), ByPath);

    const FRshipFieldTargetIdentity* ByFingerprint = Registry->FindByFingerprint(Identity.ActorPath, Identity.ComponentName, Identity.MeshPath);
    TestNotNull(TEXT("Find by fingerprint"), ByFingerprint);

    FRshipFieldTargetIdentity Updated = Identity;
    Updated.MeshPath = TEXT("/Game/TestMesh_Updated");
    Registry->UpsertIdentity(Updated);

    TestEqual(TEXT("Upsert keeps single entry"), Registry->RegisteredTargets.Num(), 1);
    const FRshipFieldTargetIdentity* UpdatedByGuid = Registry->FindByStableGuid(Identity.StableGuid);
    TestNotNull(TEXT("Updated entry exists"), UpdatedByGuid);
    if (UpdatedByGuid)
    {
        TestEqual(TEXT("Updated mesh path stored"), UpdatedByGuid->MeshPath, FString(TEXT("/Game/TestMesh_Updated")));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRshipFieldPacketIngestTest,
    "Rship.Field.Packet.IngestValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRshipFieldPacketIngestTest::RunTest(const FString& Parameters)
{
    URshipFieldSubsystem* Subsystem = NewObject<URshipFieldSubsystem>();
    TestNotNull(TEXT("Subsystem object"), Subsystem);

    FString Error;

    const bool bInvalidJsonAccepted = Subsystem->EnqueuePacketJson(TEXT("{not json}"), &Error);
    TestFalse(TEXT("Invalid JSON packet is rejected"), bInvalidJsonAccepted);
    TestTrue(TEXT("Invalid packet populates error"), !Error.IsEmpty());

    Error.Reset();
    const bool bInvalidSchemaAccepted = Subsystem->EnqueuePacketJson(TEXT("{\"schemaVersion\":2,\"sequence\":1}"), &Error);
    TestFalse(TEXT("Unsupported schemaVersion packet is rejected"), bInvalidSchemaAccepted);

    Error.Reset();
    const bool bValidPacketAccepted = Subsystem->EnqueuePacketJson(
        TEXT("{\"schemaVersion\":1,\"sequence\":1,\"globals\":{\"updateHz\":60,\"fieldResolution\":256}}"),
        &Error);
    TestTrue(TEXT("Valid packet is accepted"), bValidPacketAccepted);
    TestEqual(TEXT("Queued packet count updated"), Subsystem->LastQueuedPacketCount, 1);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRshipFieldDeterministicApplyFrameTest,
    "Rship.Field.Packet.DeterministicApplyFrameOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRshipFieldDeterministicApplyFrameTest::RunTest(const FString& Parameters)
{
    URshipFieldSubsystem* Subsystem = NewObject<URshipFieldSubsystem>();
    TestNotNull(TEXT("Subsystem object"), Subsystem);

    FString Error;
    const bool bPacketBLateAccepted = Subsystem->EnqueuePacketJson(
        TEXT("{\"schemaVersion\":1,\"sequence\":11,\"applyFrame\":5}"),
        &Error);
    TestTrue(TEXT("Packet B accepted"), bPacketBLateAccepted);

    Error.Reset();
    const bool bPacketAEarlyAccepted = Subsystem->EnqueuePacketJson(
        TEXT("{\"schemaVersion\":1,\"sequence\":10,\"applyFrame\":3}"),
        &Error);
    TestTrue(TEXT("Packet A accepted"), bPacketAEarlyAccepted);

    const float Step = 1.0f / 60.0f;
    Subsystem->Tick(Step); // frame 1
    Subsystem->Tick(Step); // frame 2
    TestEqual(TEXT("No packet applied before frame 3"), Subsystem->GetLastAppliedSequence_Debug(), int64(0));

    Subsystem->Tick(Step); // frame 3
    TestEqual(TEXT("Sequence 10 applied at frame 3"), Subsystem->GetLastAppliedSequence_Debug(), int64(10));

    Subsystem->Tick(Step); // frame 4
    TestEqual(TEXT("Sequence remains 10 at frame 4"), Subsystem->GetLastAppliedSequence_Debug(), int64(10));

    Subsystem->Tick(Step); // frame 5
    TestEqual(TEXT("Sequence 11 applied at frame 5"), Subsystem->GetLastAppliedSequence_Debug(), int64(11));
    TestEqual(TEXT("Queue fully consumed"), Subsystem->GetPendingPacketCount_Debug(), 0);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
