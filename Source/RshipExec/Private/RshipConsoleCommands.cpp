// Rship Console Commands
// Debug commands for testing and monitoring rship systems

#include "RshipSubsystem.h"
#include "RshipSceneValidator.h"
#include "RshipSceneConverter.h"
#include "RshipTimecodeSync.h"
#include "RshipFixtureLibrary.h"
#include "RshipMultiCameraManager.h"
#include "RshipFixtureManager.h"
#include "RshipNiagaraBinding.h"
#include "RshipSequencerSync.h"
#include "RshipMaterialBinding.h"
#include "RshipDMXOutput.h"
#include "Logs.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"

// ============================================================================
// SCENE VALIDATION
// ============================================================================

static FAutoConsoleCommand CmdRshipValidateScene(
    TEXT("rship.validate"),
    TEXT("Validate the current scene for rship conversion issues"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!GEngine) return;
        URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
        if (!Subsystem)
        {
            UE_LOG(LogRshipExec, Warning, TEXT("RshipSubsystem not available"));
            return;
        }

        URshipSceneValidator* Validator = Subsystem->GetSceneValidator();
        if (!Validator)
        {
            UE_LOG(LogRshipExec, Warning, TEXT("SceneValidator not available"));
            return;
        }

        FRshipValidationResult Result = Validator->ValidateScene();

        UE_LOG(LogRshipExec, Log, TEXT("========================================"));
        UE_LOG(LogRshipExec, Log, TEXT("SCENE VALIDATION RESULTS"));
        UE_LOG(LogRshipExec, Log, TEXT("========================================"));
        UE_LOG(LogRshipExec, Log, TEXT("Total actors scanned: %d"), Result.TotalActorsScanned);
        UE_LOG(LogRshipExec, Log, TEXT("Convertible actors: %d"), Result.ConvertibleActors);
        UE_LOG(LogRshipExec, Log, TEXT("Issues found:"));
        UE_LOG(LogRshipExec, Log, TEXT("  Critical: %d"), Result.CriticalCount);
        UE_LOG(LogRshipExec, Log, TEXT("  Errors: %d"), Result.ErrorCount);
        UE_LOG(LogRshipExec, Log, TEXT("  Warnings: %d"), Result.WarningCount);
        UE_LOG(LogRshipExec, Log, TEXT("  Info: %d"), Result.InfoCount);
        UE_LOG(LogRshipExec, Log, TEXT("Validation time: %.2fms"), Result.ValidationTimeSeconds * 1000.0f);
        UE_LOG(LogRshipExec, Log, TEXT("Overall: %s"), Result.bIsValid ? TEXT("VALID") : TEXT("ISSUES FOUND"));

        if (Result.ErrorCount > 0 || Result.CriticalCount > 0)
        {
            UE_LOG(LogRshipExec, Log, TEXT(""));
            UE_LOG(LogRshipExec, Log, TEXT("ERRORS:"));
            for (const FRshipValidationIssue& Issue : Result.Issues)
            {
                if (Issue.Severity == ERshipValidationSeverity::Error ||
                    Issue.Severity == ERshipValidationSeverity::Critical)
                {
                    FString ActorName = Issue.AffectedActor.IsValid() ? Issue.AffectedActor->GetActorLabel() : TEXT("Unknown");
                    UE_LOG(LogRshipExec, Warning, TEXT("  [%s] %s: %s"),
                        Issue.Severity == ERshipValidationSeverity::Critical ? TEXT("CRIT") : TEXT("ERR"),
                        *ActorName, *Issue.Message);
                }
            }
        }
    })
);

// ============================================================================
// TIMECODE
// ============================================================================

static FAutoConsoleCommand CmdRshipTimecode(
    TEXT("rship.timecode"),
    TEXT("Show current timecode status"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!GEngine) return;
        URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
        if (!Subsystem) return;

        URshipTimecodeSync* Timecode = Subsystem->GetTimecodeSync();
        if (!Timecode)
        {
            UE_LOG(LogRshipExec, Warning, TEXT("TimecodeSync not available"));
            return;
        }

        FRshipTimecodeStatus Status = Timecode->GetStatus();

        FString StateStr;
        switch (Status.State)
        {
            case ERshipTimecodeState::Stopped: StateStr = TEXT("STOPPED"); break;
            case ERshipTimecodeState::Playing: StateStr = TEXT("PLAYING"); break;
            case ERshipTimecodeState::Paused: StateStr = TEXT("PAUSED"); break;
            default: StateStr = TEXT("UNKNOWN"); break;
        }

        UE_LOG(LogRshipExec, Log, TEXT("========================================"));
        UE_LOG(LogRshipExec, Log, TEXT("TIMECODE STATUS"));
        UE_LOG(LogRshipExec, Log, TEXT("========================================"));
        UE_LOG(LogRshipExec, Log, TEXT("State: %s"), *StateStr);
        UE_LOG(LogRshipExec, Log, TEXT("Timecode: %02d:%02d:%02d:%02d"),
            Status.Timecode.Hours, Status.Timecode.Minutes,
            Status.Timecode.Seconds, Status.Timecode.Frames);
        UE_LOG(LogRshipExec, Log, TEXT("Frame: %lld"), Status.TotalFrames);
        UE_LOG(LogRshipExec, Log, TEXT("Elapsed: %.2fs"), Status.ElapsedSeconds);
        UE_LOG(LogRshipExec, Log, TEXT("Speed: %.2fx"), Status.PlaybackSpeed);
        UE_LOG(LogRshipExec, Log, TEXT("Synced: %s (offset: %.1fms)"),
            Status.bIsSynchronized ? TEXT("Yes") : TEXT("No"), Status.SyncOffsetMs);
    })
);

static FAutoConsoleCommand CmdRshipTimecodePlay(
    TEXT("rship.timecode.play"),
    TEXT("Start timecode playback"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!GEngine) return;
        URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
        if (!Subsystem) return;
        URshipTimecodeSync* Timecode = Subsystem->GetTimecodeSync();
        if (Timecode) Timecode->Play();
    })
);

static FAutoConsoleCommand CmdRshipTimecodeStop(
    TEXT("rship.timecode.stop"),
    TEXT("Stop timecode playback"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!GEngine) return;
        URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
        if (!Subsystem) return;
        URshipTimecodeSync* Timecode = Subsystem->GetTimecodeSync();
        if (Timecode) Timecode->Stop();
    })
);

// ============================================================================
// FIXTURE LIBRARY
// ============================================================================

static FAutoConsoleCommand CmdRshipFixtures(
    TEXT("rship.fixtures"),
    TEXT("List all fixtures in the library"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!GEngine) return;
        URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
        if (!Subsystem) return;

        URshipFixtureLibrary* Library = Subsystem->GetFixtureLibrary();
        if (!Library)
        {
            UE_LOG(LogRshipExec, Warning, TEXT("FixtureLibrary not available"));
            return;
        }

        TArray<FRshipFixtureProfile> Profiles = Library->GetAllProfiles();

        UE_LOG(LogRshipExec, Log, TEXT("========================================"));
        UE_LOG(LogRshipExec, Log, TEXT("FIXTURE LIBRARY (%d profiles)"), Profiles.Num());
        UE_LOG(LogRshipExec, Log, TEXT("========================================"));

        for (const FRshipFixtureProfile& P : Profiles)
        {
            UE_LOG(LogRshipExec, Log, TEXT("  [%s] %s - %s (%s)"),
                *P.Id, *P.Manufacturer, *P.Model, *P.Source);
        }

        TArray<FString> Manufacturers = Library->GetManufacturers();
        UE_LOG(LogRshipExec, Log, TEXT(""));
        UE_LOG(LogRshipExec, Log, TEXT("Manufacturers: %d"), Manufacturers.Num());
        for (const FString& M : Manufacturers)
        {
            UE_LOG(LogRshipExec, Log, TEXT("  - %s"), *M);
        }
    })
);

// ============================================================================
// CAMERAS
// ============================================================================

static FAutoConsoleCommand CmdRshipCameras(
    TEXT("rship.cameras"),
    TEXT("List all camera views"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!GEngine) return;
        URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
        if (!Subsystem) return;

        URshipMultiCameraManager* CamMgr = Subsystem->GetMultiCameraManager();
        if (!CamMgr)
        {
            UE_LOG(LogRshipExec, Warning, TEXT("MultiCameraManager not available"));
            return;
        }

        TArray<FRshipCameraView> Views = CamMgr->GetAllViews();

        UE_LOG(LogRshipExec, Log, TEXT("========================================"));
        UE_LOG(LogRshipExec, Log, TEXT("CAMERA VIEWS (%d)"), Views.Num());
        UE_LOG(LogRshipExec, Log, TEXT("========================================"));

        for (const FRshipCameraView& V : Views)
        {
            FString TallyStr;
            switch (V.TallyState)
            {
                case ERshipCameraTallyState::Off: TallyStr = TEXT("OFF"); break;
                case ERshipCameraTallyState::Preview: TallyStr = TEXT("PVW"); break;
                case ERshipCameraTallyState::Program: TallyStr = TEXT("PGM"); break;
                case ERshipCameraTallyState::Recording: TallyStr = TEXT("REC"); break;
            }

            UE_LOG(LogRshipExec, Log, TEXT("  [%s] %s (%s)"), *V.Id, *V.Name, *TallyStr);
        }
    })
);

// ============================================================================
// CONNECTION STATUS
// ============================================================================

static FAutoConsoleCommand CmdRshipStatus(
    TEXT("rship.status"),
    TEXT("Show rship connection and queue status"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!GEngine) return;
        URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
        if (!Subsystem)
        {
            UE_LOG(LogRshipExec, Warning, TEXT("RshipSubsystem not available"));
            return;
        }

        UE_LOG(LogRshipExec, Log, TEXT("========================================"));
        UE_LOG(LogRshipExec, Log, TEXT("RSHIP STATUS"));
        UE_LOG(LogRshipExec, Log, TEXT("========================================"));
        UE_LOG(LogRshipExec, Log, TEXT("Connected: %s"), Subsystem->IsConnected() ? TEXT("Yes") : TEXT("No"));
        UE_LOG(LogRshipExec, Log, TEXT("Queue length: %d"), Subsystem->GetQueueLength());
        UE_LOG(LogRshipExec, Log, TEXT("Queue bytes: %d"), Subsystem->GetQueueBytes());
        UE_LOG(LogRshipExec, Log, TEXT("Queue pressure: %.1f%%"), Subsystem->GetQueuePressure() * 100.0f);
        UE_LOG(LogRshipExec, Log, TEXT("Messages/sec: %d"), Subsystem->GetMessagesSentPerSecond());
        UE_LOG(LogRshipExec, Log, TEXT("Bytes/sec: %d"), Subsystem->GetBytesSentPerSecond());
        UE_LOG(LogRshipExec, Log, TEXT("Dropped: %d"), Subsystem->GetMessagesDropped());
        UE_LOG(LogRshipExec, Log, TEXT("Backing off: %s (%.1fs remaining)"),
            Subsystem->IsRateLimiterBackingOff() ? TEXT("Yes") : TEXT("No"),
            Subsystem->GetBackoffRemaining());
        UE_LOG(LogRshipExec, Log, TEXT("Current rate limit: %.1f msg/s"), Subsystem->GetCurrentRateLimit());
    })
);

// ============================================================================
// SCENE DISCOVERY
// ============================================================================

static FAutoConsoleCommand CmdRshipDiscover(
    TEXT("rship.discover"),
    TEXT("Discover convertible lights and cameras in the scene"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!GEngine) return;
        URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
        if (!Subsystem) return;

        URshipSceneConverter* Converter = Subsystem->GetSceneConverter();
        if (!Converter)
        {
            UE_LOG(LogRshipExec, Warning, TEXT("SceneConverter not available"));
            return;
        }

        FRshipDiscoveryOptions Options;
        int32 Count = Converter->DiscoverScene(Options);

        TArray<FRshipDiscoveredLight> Lights = Converter->GetDiscoveredLights();
        TArray<FRshipDiscoveredCamera> Cameras = Converter->GetDiscoveredCameras();

        UE_LOG(LogRshipExec, Log, TEXT("========================================"));
        UE_LOG(LogRshipExec, Log, TEXT("SCENE DISCOVERY"));
        UE_LOG(LogRshipExec, Log, TEXT("========================================"));
        UE_LOG(LogRshipExec, Log, TEXT("Found %d items total"), Count);
        UE_LOG(LogRshipExec, Log, TEXT(""));
        UE_LOG(LogRshipExec, Log, TEXT("LIGHTS (%d):"), Lights.Num());

        for (int32 i = 0; i < Lights.Num(); i++)
        {
            const FRshipDiscoveredLight& L = Lights[i];
            UE_LOG(LogRshipExec, Log, TEXT("  [%d] %s (%s) - Intensity: %.0f%s"),
                i, *L.SuggestedName, *L.LightType, L.Intensity,
                L.bAlreadyConverted ? TEXT(" [CONVERTED]") : TEXT(""));
        }

        UE_LOG(LogRshipExec, Log, TEXT(""));
        UE_LOG(LogRshipExec, Log, TEXT("CAMERAS (%d):"), Cameras.Num());

        for (int32 i = 0; i < Cameras.Num(); i++)
        {
            const FRshipDiscoveredCamera& C = Cameras[i];
            UE_LOG(LogRshipExec, Log, TEXT("  [%d] %s - FOV: %.1f%s"),
                i, *C.SuggestedName, C.FOV,
                C.bAlreadyConverted ? TEXT(" [CONVERTED]") : TEXT(""));
        }
    })
);

// ============================================================================
// NIAGARA
// ============================================================================

static FAutoConsoleCommand CmdRshipNiagara(
    TEXT("rship.niagara"),
    TEXT("Show Niagara binding status"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!GEngine) return;
        URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
        if (!Subsystem) return;

        URshipNiagaraManager* NiagaraMgr = Subsystem->GetNiagaraManager();
        if (!NiagaraMgr)
        {
            UE_LOG(LogRshipExec, Warning, TEXT("NiagaraManager not available"));
            return;
        }

        TArray<URshipNiagaraBinding*> Bindings = NiagaraMgr->GetAllBindings();

        UE_LOG(LogRshipExec, Log, TEXT("========================================"));
        UE_LOG(LogRshipExec, Log, TEXT("NIAGARA BINDINGS (%d)"), Bindings.Num());
        UE_LOG(LogRshipExec, Log, TEXT("========================================"));

        for (URshipNiagaraBinding* B : Bindings)
        {
            if (B)
            {
                AActor* Owner = B->GetOwner();
                FString OwnerName = Owner ? Owner->GetActorLabel() : TEXT("Unknown");
                UE_LOG(LogRshipExec, Log, TEXT("  %s - Emitter: %s (%d params, %d colors)"),
                    *OwnerName, *B->EmitterId,
                    B->ParameterBindings.Num(), B->ColorBindings.Num());
            }
        }
    })
);

// ============================================================================
// SEQUENCER
// ============================================================================

static FAutoConsoleCommand CmdRshipSequencer(
    TEXT("rship.sequencer"),
    TEXT("Show sequencer sync status"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!GEngine) return;
        URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
        if (!Subsystem) return;

        URshipSequencerSync* SeqSync = Subsystem->GetSequencerSync();
        if (!SeqSync)
        {
            UE_LOG(LogRshipExec, Warning, TEXT("SequencerSync not available"));
            return;
        }

        TArray<FString> ActiveMappings = SeqSync->GetActiveMappings();

        UE_LOG(LogRshipExec, Log, TEXT("========================================"));
        UE_LOG(LogRshipExec, Log, TEXT("SEQUENCER SYNC STATUS"));
        UE_LOG(LogRshipExec, Log, TEXT("========================================"));
        UE_LOG(LogRshipExec, Log, TEXT("Sync enabled: %s"), SeqSync->IsSyncEnabled() ? TEXT("Yes") : TEXT("No"));
        UE_LOG(LogRshipExec, Log, TEXT("Playing: %s"), SeqSync->IsPlaying() ? TEXT("Yes") : TEXT("No"));
        UE_LOG(LogRshipExec, Log, TEXT("Active mappings: %d"), ActiveMappings.Num());

        for (const FString& Id : ActiveMappings)
        {
            UE_LOG(LogRshipExec, Log, TEXT("  - %s"), *Id);
        }
    })
);

static FAutoConsoleCommand CmdRshipSequencerPlay(
    TEXT("rship.sequencer.play"),
    TEXT("Start sequencer playback"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!GEngine) return;
        URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
        if (!Subsystem) return;
        URshipSequencerSync* SeqSync = Subsystem->GetSequencerSync();
        if (SeqSync) SeqSync->Play();
    })
);

static FAutoConsoleCommand CmdRshipSequencerStop(
    TEXT("rship.sequencer.stop"),
    TEXT("Stop sequencer playback"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!GEngine) return;
        URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
        if (!Subsystem) return;
        URshipSequencerSync* SeqSync = Subsystem->GetSequencerSync();
        if (SeqSync) SeqSync->Stop();
    })
);

static FAutoConsoleCommand CmdRshipSequencerSync(
    TEXT("rship.sequencer.sync"),
    TEXT("Force sync sequencer to current timecode"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!GEngine) return;
        URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
        if (!Subsystem) return;
        URshipSequencerSync* SeqSync = Subsystem->GetSequencerSync();
        if (SeqSync) SeqSync->ForceSync();
    })
);

// ============================================================================
// MATERIALS
// ============================================================================

static FAutoConsoleCommand CmdRshipMaterials(
    TEXT("rship.materials"),
    TEXT("Show material binding status"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!GEngine) return;
        URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
        if (!Subsystem) return;

        URshipMaterialManager* MatMgr = Subsystem->GetMaterialManager();
        if (!MatMgr)
        {
            UE_LOG(LogRshipExec, Warning, TEXT("MaterialManager not available"));
            return;
        }

        TArray<URshipMaterialBinding*> Bindings = MatMgr->GetAllBindings();

        UE_LOG(LogRshipExec, Log, TEXT("========================================"));
        UE_LOG(LogRshipExec, Log, TEXT("MATERIAL BINDINGS (%d)"), Bindings.Num());
        UE_LOG(LogRshipExec, Log, TEXT("========================================"));
        UE_LOG(LogRshipExec, Log, TEXT("Global intensity: %.2f"), MatMgr->GetGlobalIntensityMultiplier());

        for (URshipMaterialBinding* B : Bindings)
        {
            if (B)
            {
                AActor* Owner = B->GetOwner();
                FString OwnerName = Owner ? Owner->GetActorLabel() : TEXT("Unknown");
                UE_LOG(LogRshipExec, Log, TEXT("  %s - Emitter: %s (%d scalar, %d vector, %d texture)"),
                    *OwnerName, *B->EmitterId,
                    B->ScalarBindings.Num(), B->VectorBindings.Num(), B->TextureBindings.Num());
            }
        }
    })
);

// ============================================================================
// DMX OUTPUT
// ============================================================================

static FAutoConsoleCommand CmdRshipDMX(
    TEXT("rship.dmx"),
    TEXT("Show DMX output status"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!GEngine) return;
        URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
        if (!Subsystem) return;

        URshipDMXOutput* DMX = Subsystem->GetDMXOutput();
        if (!DMX)
        {
            UE_LOG(LogRshipExec, Warning, TEXT("DMXOutput not available"));
            return;
        }

        UE_LOG(LogRshipExec, Log, TEXT("========================================"));
        UE_LOG(LogRshipExec, Log, TEXT("DMX OUTPUT STATUS"));
        UE_LOG(LogRshipExec, Log, TEXT("========================================"));
        UE_LOG(LogRshipExec, Log, TEXT("Enabled: %s"), DMX->IsEnabled() ? TEXT("Yes") : TEXT("No"));
        UE_LOG(LogRshipExec, Log, TEXT("Protocol: %s"), DMX->GetProtocol() == ERshipDMXProtocol::ArtNet ? TEXT("Art-Net") : TEXT("sACN"));
        UE_LOG(LogRshipExec, Log, TEXT("Fixtures: %d"), DMX->GetFixtureCount());
        UE_LOG(LogRshipExec, Log, TEXT("Active universes: %d"), DMX->GetActiveUniverseCount());
        UE_LOG(LogRshipExec, Log, TEXT("Master dimmer: %.0f%%"), DMX->GetMasterDimmer() * 100.0f);
        UE_LOG(LogRshipExec, Log, TEXT("Blackout: %s"), DMX->IsBlackout() ? TEXT("Yes") : TEXT("No"));
    })
);

static FAutoConsoleCommand CmdRshipDMXEnable(
    TEXT("rship.dmx.enable"),
    TEXT("Enable DMX output"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!GEngine) return;
        URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
        if (!Subsystem) return;
        URshipDMXOutput* DMX = Subsystem->GetDMXOutput();
        if (DMX) DMX->SetEnabled(true);
        UE_LOG(LogRshipExec, Log, TEXT("DMX output enabled"));
    })
);

static FAutoConsoleCommand CmdRshipDMXDisable(
    TEXT("rship.dmx.disable"),
    TEXT("Disable DMX output"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!GEngine) return;
        URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
        if (!Subsystem) return;
        URshipDMXOutput* DMX = Subsystem->GetDMXOutput();
        if (DMX) DMX->SetEnabled(false);
        UE_LOG(LogRshipExec, Log, TEXT("DMX output disabled"));
    })
);

static FAutoConsoleCommand CmdRshipDMXBlackout(
    TEXT("rship.dmx.blackout"),
    TEXT("Toggle DMX blackout"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!GEngine) return;
        URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
        if (!Subsystem) return;
        URshipDMXOutput* DMX = Subsystem->GetDMXOutput();
        if (DMX)
        {
            if (DMX->IsBlackout())
            {
                DMX->ReleaseBlackout();
                UE_LOG(LogRshipExec, Log, TEXT("DMX blackout released"));
            }
            else
            {
                DMX->Blackout();
                UE_LOG(LogRshipExec, Log, TEXT("DMX blackout engaged"));
            }
        }
    })
);

static FAutoConsoleCommand CmdRshipDMXAutoMap(
    TEXT("rship.dmx.automap"),
    TEXT("Auto-map all fixtures to DMX (starts at universe 1, address 1)"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!GEngine) return;
        URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
        if (!Subsystem) return;
        URshipDMXOutput* DMX = Subsystem->GetDMXOutput();
        if (DMX)
        {
            int32 Count = DMX->AutoMapAllFixtures();
            UE_LOG(LogRshipExec, Log, TEXT("Auto-mapped %d fixtures to DMX"), Count);
        }
    })
);

// ============================================================================
// HELP
// ============================================================================

static FAutoConsoleCommand CmdRshipHelp(
    TEXT("rship.help"),
    TEXT("Show available rship console commands"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        UE_LOG(LogRshipExec, Log, TEXT("========================================"));
        UE_LOG(LogRshipExec, Log, TEXT("RSHIP CONSOLE COMMANDS"));
        UE_LOG(LogRshipExec, Log, TEXT("========================================"));
        UE_LOG(LogRshipExec, Log, TEXT(""));
        UE_LOG(LogRshipExec, Log, TEXT("Connection:"));
        UE_LOG(LogRshipExec, Log, TEXT("  rship.status         - Show connection and queue status"));
        UE_LOG(LogRshipExec, Log, TEXT(""));
        UE_LOG(LogRshipExec, Log, TEXT("Scene:"));
        UE_LOG(LogRshipExec, Log, TEXT("  rship.validate       - Validate scene for conversion"));
        UE_LOG(LogRshipExec, Log, TEXT("  rship.discover       - Discover convertible items"));
        UE_LOG(LogRshipExec, Log, TEXT(""));
        UE_LOG(LogRshipExec, Log, TEXT("Timecode:"));
        UE_LOG(LogRshipExec, Log, TEXT("  rship.timecode       - Show timecode status"));
        UE_LOG(LogRshipExec, Log, TEXT("  rship.timecode.play  - Start playback"));
        UE_LOG(LogRshipExec, Log, TEXT("  rship.timecode.stop  - Stop playback"));
        UE_LOG(LogRshipExec, Log, TEXT(""));
        UE_LOG(LogRshipExec, Log, TEXT("Sequencer:"));
        UE_LOG(LogRshipExec, Log, TEXT("  rship.sequencer      - Show sequencer sync status"));
        UE_LOG(LogRshipExec, Log, TEXT("  rship.sequencer.play - Start sequencer playback"));
        UE_LOG(LogRshipExec, Log, TEXT("  rship.sequencer.stop - Stop sequencer playback"));
        UE_LOG(LogRshipExec, Log, TEXT("  rship.sequencer.sync - Force sync to timecode"));
        UE_LOG(LogRshipExec, Log, TEXT(""));
        UE_LOG(LogRshipExec, Log, TEXT("Niagara:"));
        UE_LOG(LogRshipExec, Log, TEXT("  rship.niagara        - Show Niagara binding status"));
        UE_LOG(LogRshipExec, Log, TEXT(""));
        UE_LOG(LogRshipExec, Log, TEXT("Materials:"));
        UE_LOG(LogRshipExec, Log, TEXT("  rship.materials      - Show material binding status"));
        UE_LOG(LogRshipExec, Log, TEXT(""));
        UE_LOG(LogRshipExec, Log, TEXT("DMX Output:"));
        UE_LOG(LogRshipExec, Log, TEXT("  rship.dmx            - Show DMX output status"));
        UE_LOG(LogRshipExec, Log, TEXT("  rship.dmx.enable     - Enable DMX output"));
        UE_LOG(LogRshipExec, Log, TEXT("  rship.dmx.disable    - Disable DMX output"));
        UE_LOG(LogRshipExec, Log, TEXT("  rship.dmx.blackout   - Toggle blackout"));
        UE_LOG(LogRshipExec, Log, TEXT("  rship.dmx.automap    - Auto-map fixtures to DMX"));
        UE_LOG(LogRshipExec, Log, TEXT(""));
        UE_LOG(LogRshipExec, Log, TEXT("Library:"));
        UE_LOG(LogRshipExec, Log, TEXT("  rship.fixtures       - List fixture profiles"));
        UE_LOG(LogRshipExec, Log, TEXT("  rship.cameras        - List camera views"));
    })
);
