// Rship Blueprint Function Library Implementation

#include "RshipBlueprintLibrary.h"
#include "RshipSubsystem.h"
#include "RshipTargetComponent.h"
#include "RshipFixtureManager.h"
#include "RshipPulseReceiver.h"
#include "RshipSceneConverter.h"
#include "RshipSceneValidator.h"
#include "RshipTimecodeSync.h"
#include "RshipMultiCameraManager.h"
#include "RshipFixtureVisualizer.h"
#include "RshipSequencerSync.h"
#include "RshipDMXOutput.h"
#include "RshipOSCBridge.h"
#include "LevelSequence.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

URshipSubsystem* URshipBlueprintLibrary::GetSubsystem()
{
    if (GEngine)
    {
        return GEngine->GetEngineSubsystem<URshipSubsystem>();
    }
    return nullptr;
}

URshipSubsystem* URshipBlueprintLibrary::GetRshipSubsystem()
{
    return GetSubsystem();
}

// ============================================================================
// CONNECTION
// ============================================================================

bool URshipBlueprintLibrary::IsConnectedToRship()
{
    URshipSubsystem* Subsystem = GetSubsystem();
    return Subsystem && Subsystem->IsConnected();
}

void URshipBlueprintLibrary::ReconnectToRship()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        Subsystem->Reconnect();
    }
}

FString URshipBlueprintLibrary::GetRshipServiceId()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        return Subsystem->GetServiceId();
    }
    return TEXT("");
}

// ============================================================================
// TARGETS
// ============================================================================

TArray<URshipTargetComponent*> URshipBlueprintLibrary::GetAllTargetComponents()
{
    TArray<URshipTargetComponent*> Result;

    URshipSubsystem* Subsystem = GetSubsystem();
    if (Subsystem && Subsystem->TargetComponents)
    {
        for (URshipTargetComponent* Comp : *Subsystem->TargetComponents)
        {
            if (Comp)
            {
                Result.Add(Comp);
            }
        }
    }

    return Result;
}

URshipTargetComponent* URshipBlueprintLibrary::FindTargetById(const FString& TargetId)
{
    URshipSubsystem* Subsystem = GetSubsystem();
    if (Subsystem && Subsystem->TargetComponents)
    {
        for (URshipTargetComponent* Comp : *Subsystem->TargetComponents)
        {
            if (Comp && Comp->TargetData && Comp->TargetData->GetId() == TargetId)
            {
                return Comp;
            }
        }
    }
    return nullptr;
}

void URshipBlueprintLibrary::PulseEmitter(const FString& TargetId, const FString& EmitterId, const TMap<FString, FString>& Data)
{
    URshipSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem) return;

    TSharedPtr<FJsonObject> JsonData = MakeShareable(new FJsonObject);
    for (const auto& Pair : Data)
    {
        JsonData->SetStringField(Pair.Key, Pair.Value);
    }

    Subsystem->PulseEmitter(TargetId, EmitterId, JsonData);
}

// ============================================================================
// FIXTURES
// ============================================================================

TArray<FRshipFixtureInfo> URshipBlueprintLibrary::GetAllFixtures()
{
    TArray<FRshipFixtureInfo> Result;

    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipFixtureManager* FM = Subsystem->GetFixtureManager())
        {
            Result = FM->GetAllFixtures();
        }
    }

    return Result;
}

bool URshipBlueprintLibrary::GetFixtureById(const FString& FixtureId, FRshipFixtureInfo& OutFixture)
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipFixtureManager* FM = Subsystem->GetFixtureManager())
        {
            return FM->GetFixtureById(FixtureId, OutFixture);
        }
    }
    return false;
}

void URshipBlueprintLibrary::SetFixtureIntensity(const FString& FixtureId, float Intensity)
{
    URshipSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem) return;

    URshipFixtureManager* FixtureManager = Subsystem->GetFixtureManager();
    URshipPulseReceiver* PulseReceiver = Subsystem->GetPulseReceiver();
    if (!FixtureManager || !PulseReceiver) return;

    FRshipFixtureInfo FixtureInfo;
    if (!FixtureManager->GetFixtureById(FixtureId, FixtureInfo))
    {
        UE_LOG(LogTemp, Warning, TEXT("SetFixtureIntensity: Fixture '%s' not found"), *FixtureId);
        return;
    }

    // Create pulse data with intensity
    TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject());
    Data->SetNumberField(TEXT("intensity"), FMath::Clamp(Intensity, 0.0f, 1.0f));

    // Route through pulse receiver to update all subscribers
    PulseReceiver->ProcessPulseEvent(FixtureInfo.EmitterId, Data);
}

void URshipBlueprintLibrary::SetFixtureColor(const FString& FixtureId, FLinearColor Color)
{
    URshipSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem) return;

    URshipFixtureManager* FixtureManager = Subsystem->GetFixtureManager();
    URshipPulseReceiver* PulseReceiver = Subsystem->GetPulseReceiver();
    if (!FixtureManager || !PulseReceiver) return;

    FRshipFixtureInfo FixtureInfo;
    if (!FixtureManager->GetFixtureById(FixtureId, FixtureInfo))
    {
        UE_LOG(LogTemp, Warning, TEXT("SetFixtureColor: Fixture '%s' not found"), *FixtureId);
        return;
    }

    // Create pulse data with color
    TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject());
    Data->SetNumberField(TEXT("r"), Color.R);
    Data->SetNumberField(TEXT("g"), Color.G);
    Data->SetNumberField(TEXT("b"), Color.B);

    // Route through pulse receiver to update all subscribers
    PulseReceiver->ProcessPulseEvent(FixtureInfo.EmitterId, Data);
}

void URshipBlueprintLibrary::SetFixtureState(const FString& FixtureId, float Intensity, FLinearColor Color)
{
    URshipSubsystem* Subsystem = GetSubsystem();
    if (!Subsystem) return;

    URshipFixtureManager* FixtureManager = Subsystem->GetFixtureManager();
    URshipPulseReceiver* PulseReceiver = Subsystem->GetPulseReceiver();
    if (!FixtureManager || !PulseReceiver) return;

    FRshipFixtureInfo FixtureInfo;
    if (!FixtureManager->GetFixtureById(FixtureId, FixtureInfo))
    {
        UE_LOG(LogTemp, Warning, TEXT("SetFixtureState: Fixture '%s' not found"), *FixtureId);
        return;
    }

    // Create pulse data with intensity and color
    TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject());
    Data->SetNumberField(TEXT("intensity"), FMath::Clamp(Intensity, 0.0f, 1.0f));
    Data->SetNumberField(TEXT("r"), Color.R);
    Data->SetNumberField(TEXT("g"), Color.G);
    Data->SetNumberField(TEXT("b"), Color.B);

    // Route through pulse receiver to update all subscribers
    PulseReceiver->ProcessPulseEvent(FixtureInfo.EmitterId, Data);
}

// ============================================================================
// CAMERAS
// ============================================================================

TArray<FRshipCameraView> URshipBlueprintLibrary::GetAllCameraViews()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipMultiCameraManager* CM = Subsystem->GetMultiCameraManager())
        {
            return CM->GetAllViews();
        }
    }
    return TArray<FRshipCameraView>();
}

void URshipBlueprintLibrary::SwitchToCameraView(const FString& ViewId, ERshipTransitionType TransitionType, float Duration)
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipMultiCameraManager* CM = Subsystem->GetMultiCameraManager())
        {
            if (TransitionType == ERshipTransitionType::Cut || Duration <= 0.0f)
            {
                CM->SwitchToView(ViewId);
            }
            else
            {
                FRshipCameraTransition Transition;
                Transition.Type = TransitionType;
                Transition.Duration = Duration;
                CM->SwitchWithTransition(ViewId, Transition);
            }
        }
    }
}

void URshipBlueprintLibrary::CutToProgram()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipMultiCameraManager* CM = Subsystem->GetMultiCameraManager())
        {
            CM->Cut();
        }
    }
}

void URshipBlueprintLibrary::AutoTransition()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipMultiCameraManager* CM = Subsystem->GetMultiCameraManager())
        {
            CM->Auto();
        }
    }
}

FRshipCameraView URshipBlueprintLibrary::GetProgramView()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipMultiCameraManager* CM = Subsystem->GetMultiCameraManager())
        {
            return CM->GetProgramView();
        }
    }
    return FRshipCameraView();
}

FRshipCameraView URshipBlueprintLibrary::GetPreviewView()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipMultiCameraManager* CM = Subsystem->GetMultiCameraManager())
        {
            return CM->GetPreviewView();
        }
    }
    return FRshipCameraView();
}

// ============================================================================
// TIMECODE
// ============================================================================

FString URshipBlueprintLibrary::GetCurrentTimecodeString()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipTimecodeSync* TC = Subsystem->GetTimecodeSync())
        {
            FRshipTimecodeStatus Status = TC->GetStatus();
            return FormatTimecode(
                Status.Timecode.Hours,
                Status.Timecode.Minutes,
                Status.Timecode.Seconds,
                Status.Timecode.Frames
            );
        }
    }
    return TEXT("00:00:00:00");
}

FRshipTimecodeStatus URshipBlueprintLibrary::GetTimecodeStatus()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipTimecodeSync* TC = Subsystem->GetTimecodeSync())
        {
            return TC->GetStatus();
        }
    }
    return FRshipTimecodeStatus();
}

float URshipBlueprintLibrary::GetElapsedSeconds()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipTimecodeSync* TC = Subsystem->GetTimecodeSync())
        {
            return (float)TC->GetStatus().ElapsedSeconds;
        }
    }
    return 0.0f;
}

int64 URshipBlueprintLibrary::GetCurrentFrame()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipTimecodeSync* TC = Subsystem->GetTimecodeSync())
        {
            return TC->GetStatus().TotalFrames;
        }
    }
    return 0;
}

void URshipBlueprintLibrary::PlayTimecode()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipTimecodeSync* TC = Subsystem->GetTimecodeSync())
        {
            TC->Play();
        }
    }
}

void URshipBlueprintLibrary::PauseTimecode()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipTimecodeSync* TC = Subsystem->GetTimecodeSync())
        {
            TC->Pause();
        }
    }
}

void URshipBlueprintLibrary::StopTimecode()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipTimecodeSync* TC = Subsystem->GetTimecodeSync())
        {
            TC->Stop();
        }
    }
}

void URshipBlueprintLibrary::SeekToTime(float Seconds)
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipTimecodeSync* TC = Subsystem->GetTimecodeSync())
        {
            TC->SeekToTime((double)Seconds);
        }
    }
}

void URshipBlueprintLibrary::SetPlaybackSpeed(float Speed)
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipTimecodeSync* TC = Subsystem->GetTimecodeSync())
        {
            TC->SetPlaybackSpeed(Speed);
        }
    }
}

// ============================================================================
// SEQUENCER
// ============================================================================

bool URshipBlueprintLibrary::IsSequencerSyncEnabled()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipSequencerSync* SS = Subsystem->GetSequencerSync())
        {
            return SS->IsSyncEnabled();
        }
    }
    return false;
}

void URshipBlueprintLibrary::SetSequencerSyncEnabled(bool bEnabled)
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipSequencerSync* SS = Subsystem->GetSequencerSync())
        {
            SS->SetSyncEnabled(bEnabled);
        }
    }
}

void URshipBlueprintLibrary::PlaySequencer()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipSequencerSync* SS = Subsystem->GetSequencerSync())
        {
            SS->Play();
        }
    }
}

void URshipBlueprintLibrary::StopSequencer()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipSequencerSync* SS = Subsystem->GetSequencerSync())
        {
            SS->Stop();
        }
    }
}

void URshipBlueprintLibrary::ForceSyncSequencer()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipSequencerSync* SS = Subsystem->GetSequencerSync())
        {
            SS->ForceSync();
        }
    }
}

FString URshipBlueprintLibrary::QuickSyncLevelSequence(ULevelSequence* Sequence)
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipSequencerSync* SS = Subsystem->GetSequencerSync())
        {
            return SS->QuickSyncSequence(Sequence);
        }
    }
    return TEXT("");
}

// ============================================================================
// SCENE CONVERSION
// ============================================================================

int32 URshipBlueprintLibrary::DiscoverScene()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipSceneConverter* Converter = Subsystem->GetSceneConverter())
        {
            FRshipDiscoveryOptions Options;
            return Converter->DiscoverScene(Options);
        }
    }
    return 0;
}

FRshipValidationResult URshipBlueprintLibrary::ValidateScene()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipSceneValidator* Validator = Subsystem->GetSceneValidator())
        {
            return Validator->ValidateScene();
        }
    }
    return FRshipValidationResult();
}

int32 URshipBlueprintLibrary::ConvertAllLights()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipSceneConverter* Converter = Subsystem->GetSceneConverter())
        {
            FRshipConversionOptions Options;
            TArray<FRshipConversionResult> Results;
            return Converter->ConvertAllLightsValidated(Options, Results);
        }
    }
    return 0;
}

// ============================================================================
// VISUALIZATION
// ============================================================================

void URshipBlueprintLibrary::SetBeamVisualizationVisible(bool bVisible)
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipVisualizationManager* VM = Subsystem->GetVisualizationManager())
        {
            VM->SetGlobalVisibility(bVisible);
        }
    }
}

void URshipBlueprintLibrary::SetBeamVisualizationMode(ERshipVisualizationMode Mode)
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipVisualizationManager* VM = Subsystem->GetVisualizationManager())
        {
            VM->SetGlobalMode(Mode);
        }
    }
}

void URshipBlueprintLibrary::ApplyProgrammingVisualization()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipVisualizationManager* VM = Subsystem->GetVisualizationManager())
        {
            VM->ApplyProgrammingPreset();
        }
    }
}

void URshipBlueprintLibrary::ApplyShowVisualization()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipVisualizationManager* VM = Subsystem->GetVisualizationManager())
        {
            VM->ApplyShowPreset();
        }
    }
}

// ============================================================================
// UTILITY
// ============================================================================

FString URshipBlueprintLibrary::FormatTimecode(int32 Hours, int32 Minutes, int32 Seconds, int32 Frames)
{
    return FString::Printf(TEXT("%02d:%02d:%02d:%02d"), Hours, Minutes, Seconds, Frames);
}

bool URshipBlueprintLibrary::ParseTimecode(const FString& TimecodeString, int32& Hours, int32& Minutes, int32& Seconds, int32& Frames)
{
    TArray<FString> Parts;
    TimecodeString.ParseIntoArray(Parts, TEXT(":"));

    if (Parts.Num() != 4)
    {
        return false;
    }

    Hours = FCString::Atoi(*Parts[0]);
    Minutes = FCString::Atoi(*Parts[1]);
    Seconds = FCString::Atoi(*Parts[2]);
    Frames = FCString::Atoi(*Parts[3]);

    return true;
}

FString URshipBlueprintLibrary::ColorToHex(FLinearColor Color)
{
    FColor SRGBColor = Color.ToFColor(true);
    return FString::Printf(TEXT("#%02X%02X%02X"), SRGBColor.R, SRGBColor.G, SRGBColor.B);
}

FLinearColor URshipBlueprintLibrary::HexToColor(const FString& HexString)
{
    FColor Color = FColor::FromHex(HexString);
    return FLinearColor(Color);
}

// ============================================================================
// DMX OUTPUT
// ============================================================================

bool URshipBlueprintLibrary::IsDMXOutputEnabled()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipDMXOutput* DMX = Subsystem->GetDMXOutput())
        {
            return DMX->IsEnabled();
        }
    }
    return false;
}

void URshipBlueprintLibrary::SetDMXOutputEnabled(bool bEnabled)
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipDMXOutput* DMX = Subsystem->GetDMXOutput())
        {
            DMX->SetEnabled(bEnabled);
        }
    }
}

ERshipDMXProtocol URshipBlueprintLibrary::GetDMXProtocol()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipDMXOutput* DMX = Subsystem->GetDMXOutput())
        {
            return DMX->GetProtocol();
        }
    }
    return ERshipDMXProtocol::ArtNet;
}

void URshipBlueprintLibrary::SetDMXProtocol(ERshipDMXProtocol Protocol)
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipDMXOutput* DMX = Subsystem->GetDMXOutput())
        {
            DMX->SetProtocol(Protocol);
        }
    }
}

void URshipBlueprintLibrary::SetDMXDestination(const FString& IPAddress)
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipDMXOutput* DMX = Subsystem->GetDMXOutput())
        {
            DMX->SetDestinationAddress(IPAddress);
        }
    }
}

void URshipBlueprintLibrary::DMXBlackout()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipDMXOutput* DMX = Subsystem->GetDMXOutput())
        {
            DMX->Blackout();
        }
    }
}

void URshipBlueprintLibrary::DMXReleaseBlackout()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipDMXOutput* DMX = Subsystem->GetDMXOutput())
        {
            DMX->ReleaseBlackout();
        }
    }
}

bool URshipBlueprintLibrary::IsDMXBlackout()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipDMXOutput* DMX = Subsystem->GetDMXOutput())
        {
            return DMX->IsBlackout();
        }
    }
    return false;
}

void URshipBlueprintLibrary::SetDMXMasterDimmer(float Dimmer)
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipDMXOutput* DMX = Subsystem->GetDMXOutput())
        {
            DMX->SetMasterDimmer(Dimmer);
        }
    }
}

float URshipBlueprintLibrary::GetDMXMasterDimmer()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipDMXOutput* DMX = Subsystem->GetDMXOutput())
        {
            return DMX->GetMasterDimmer();
        }
    }
    return 1.0f;
}

void URshipBlueprintLibrary::SetDMXChannel(int32 Universe, int32 Channel, uint8 Value)
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipDMXOutput* DMX = Subsystem->GetDMXOutput())
        {
            DMX->SetChannel(Universe, Channel, Value);
        }
    }
}

int32 URshipBlueprintLibrary::DMXAutoMapFixtures(int32 StartUniverse, int32 StartAddress)
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipDMXOutput* DMX = Subsystem->GetDMXOutput())
        {
            return DMX->AutoMapAllFixtures(StartUniverse, StartAddress);
        }
    }
    return 0;
}

int32 URshipBlueprintLibrary::GetDMXFixtureCount()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipDMXOutput* DMX = Subsystem->GetDMXOutput())
        {
            return DMX->GetFixtureCount();
        }
    }
    return 0;
}

// ============================================================================
// OSC BRIDGE
// ============================================================================

bool URshipBlueprintLibrary::IsOSCServerRunning()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipOSCBridge* OSC = Subsystem->GetOSCBridge())
        {
            return OSC->IsServerRunning();
        }
    }
    return false;
}

bool URshipBlueprintLibrary::StartOSCServer(int32 Port)
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipOSCBridge* OSC = Subsystem->GetOSCBridge())
        {
            return OSC->StartServer(Port);
        }
    }
    return false;
}

void URshipBlueprintLibrary::StopOSCServer()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipOSCBridge* OSC = Subsystem->GetOSCBridge())
        {
            OSC->StopServer();
        }
    }
}

void URshipBlueprintLibrary::SendOSCFloat(const FString& Address, float Value)
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipOSCBridge* OSC = Subsystem->GetOSCBridge())
        {
            OSC->SendFloat(Address, Value);
        }
    }
}

void URshipBlueprintLibrary::SendOSCColor(const FString& Address, FLinearColor Color)
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipOSCBridge* OSC = Subsystem->GetOSCBridge())
        {
            OSC->SendColor(Address, Color);
        }
    }
}

void URshipBlueprintLibrary::AddOSCDestination(const FString& Name, const FString& IPAddress, int32 Port)
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipOSCBridge* OSC = Subsystem->GetOSCBridge())
        {
            FRshipOSCDestination Dest;
            Dest.Name = Name;
            Dest.IPAddress = IPAddress;
            Dest.Port = Port;
            Dest.bEnabled = true;
            OSC->AddDestination(Dest);
        }
    }
}

void URshipBlueprintLibrary::RemoveOSCDestination(const FString& Name)
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipOSCBridge* OSC = Subsystem->GetOSCBridge())
        {
            OSC->RemoveDestination(Name);
        }
    }
}

void URshipBlueprintLibrary::CreateTouchOSCMappings()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipOSCBridge* OSC = Subsystem->GetOSCBridge())
        {
            OSC->CreateTouchOSCMappings();
        }
    }
}

void URshipBlueprintLibrary::CreateQLabMappings()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipOSCBridge* OSC = Subsystem->GetOSCBridge())
        {
            OSC->CreateQLabMappings();
        }
    }
}

// ============================================================================
// LIVE LINK
// ============================================================================

bool URshipBlueprintLibrary::IsLiveLinkSourceActive()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipLiveLinkService* LL = Subsystem->GetLiveLinkService())
        {
            return LL->IsSourceActive();
        }
    }
    return false;
}

bool URshipBlueprintLibrary::StartLiveLinkSource()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipLiveLinkService* LL = Subsystem->GetLiveLinkService())
        {
            return LL->StartSource();
        }
    }
    return false;
}

void URshipBlueprintLibrary::StopLiveLinkSource()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipLiveLinkService* LL = Subsystem->GetLiveLinkService())
        {
            LL->StopSource();
        }
    }
}

int32 URshipBlueprintLibrary::CreateLiveLinkSubjectsFromFixtures()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipLiveLinkService* LL = Subsystem->GetLiveLinkService())
        {
            return LL->CreateSubjectsFromFixtures();
        }
    }
    return 0;
}

void URshipBlueprintLibrary::CreateLiveLinkCameraSubject(const FString& EmitterId, FName SubjectName)
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipLiveLinkService* LL = Subsystem->GetLiveLinkService())
        {
            LL->CreateCameraTrackingSubject(EmitterId, SubjectName);
        }
    }
}

void URshipBlueprintLibrary::CreateLiveLinkLightSubject(const FString& EmitterId, FName SubjectName)
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipLiveLinkService* LL = Subsystem->GetLiveLinkService())
        {
            LL->CreateLightTrackingSubject(EmitterId, SubjectName);
        }
    }
}

void URshipBlueprintLibrary::UpdateLiveLinkTransform(FName SubjectName, FTransform Transform)
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipLiveLinkService* LL = Subsystem->GetLiveLinkService())
        {
            LL->UpdateTransform(SubjectName, Transform);
        }
    }
}

TArray<FName> URshipBlueprintLibrary::GetLiveLinkSubjectNames()
{
    if (URshipSubsystem* Subsystem = GetSubsystem())
    {
        if (URshipLiveLinkService* LL = Subsystem->GetLiveLinkService())
        {
            return LL->GetAllSubjectNames();
        }
    }
    return TArray<FName>();
}
