// Rship Live Link Source Implementation

#include "RshipLiveLinkSource.h"
#include "RshipSubsystem.h"
#include "RshipPulseReceiver.h"
#include "RshipFixtureManager.h"
#include "ILiveLinkClient.h"
#include "LiveLinkClient.h"
#include "Roles/LiveLinkTransformRole.h"
#include "Roles/LiveLinkCameraRole.h"
#include "Roles/LiveLinkLightRole.h"
#include "Roles/LiveLinkAnimationRole.h"
#include "Modules/ModuleManager.h"
#include "Features/IModularFeatures.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogRshipLiveLink, Log, All);

// ============================================================================
// LIVE LINK SOURCE IMPLEMENTATION
// ============================================================================

FRshipLiveLinkSource::FRshipLiveLinkSource()
{
    UE_LOG(LogRshipLiveLink, Log, TEXT("Rship Live Link source created"));
}

FRshipLiveLinkSource::~FRshipLiveLinkSource()
{
    UE_LOG(LogRshipLiveLink, Log, TEXT("Rship Live Link source destroyed"));
}

void FRshipLiveLinkSource::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
    Client = InClient;
    SourceGuid = InSourceGuid;
    UE_LOG(LogRshipLiveLink, Log, TEXT("Rship Live Link source received client"));
}

bool FRshipLiveLinkSource::IsSourceStillValid() const
{
    return bIsValid;
}

bool FRshipLiveLinkSource::RequestSourceShutdown()
{
    bIsValid = false;
    return true;
}

FText FRshipLiveLinkSource::GetSourceType() const
{
    return FText::FromString(TEXT("Rship"));
}

FText FRshipLiveLinkSource::GetSourceMachineName() const
{
    return FText::FromString(FPlatformProcess::ComputerName());
}

FText FRshipLiveLinkSource::GetSourceStatus() const
{
    return bIsValid ? FText::FromString(TEXT("Active")) : FText::FromString(TEXT("Inactive"));
}

void FRshipLiveLinkSource::RegisterTransformSubject(const FName& SubjectName)
{
    if (!Client) return;

    FScopeLock Lock(&SubjectLock);

    FLiveLinkStaticDataStruct StaticData(FLiveLinkTransformStaticData::StaticStruct());
    FLiveLinkTransformStaticData* TransformData = StaticData.Cast<FLiveLinkTransformStaticData>();

    Client->PushSubjectStaticData_AnyThread({SourceGuid, SubjectName}, ULiveLinkTransformRole::StaticClass(), MoveTemp(StaticData));
    RegisteredSubjects.Add(SubjectName);

    UE_LOG(LogRshipLiveLink, Log, TEXT("Registered transform subject: %s"), *SubjectName.ToString());
}

void FRshipLiveLinkSource::RegisterCameraSubject(const FName& SubjectName)
{
    if (!Client) return;

    FScopeLock Lock(&SubjectLock);

    FLiveLinkStaticDataStruct StaticData(FLiveLinkCameraStaticData::StaticStruct());
    FLiveLinkCameraStaticData* CameraData = StaticData.Cast<FLiveLinkCameraStaticData>();
    CameraData->bIsFieldOfViewSupported = true;
    CameraData->bIsFocusDistanceSupported = true;
    CameraData->bIsApertureSupported = true;

    Client->PushSubjectStaticData_AnyThread({SourceGuid, SubjectName}, ULiveLinkCameraRole::StaticClass(), MoveTemp(StaticData));
    RegisteredSubjects.Add(SubjectName);

    UE_LOG(LogRshipLiveLink, Log, TEXT("Registered camera subject: %s"), *SubjectName.ToString());
}

void FRshipLiveLinkSource::RegisterLightSubject(const FName& SubjectName)
{
    if (!Client) return;

    FScopeLock Lock(&SubjectLock);

    FLiveLinkStaticDataStruct StaticData(FLiveLinkLightStaticData::StaticStruct());
    FLiveLinkLightStaticData* LightData = StaticData.Cast<FLiveLinkLightStaticData>();
    LightData->bIsIntensitySupported = true;
    LightData->bIsLightColorSupported = true;
    LightData->bIsTemperatureSupported = true;

    Client->PushSubjectStaticData_AnyThread({SourceGuid, SubjectName}, ULiveLinkLightRole::StaticClass(), MoveTemp(StaticData));
    RegisteredSubjects.Add(SubjectName);

    UE_LOG(LogRshipLiveLink, Log, TEXT("Registered light subject: %s"), *SubjectName.ToString());
}

void FRshipLiveLinkSource::RegisterAnimationSubject(const FName& SubjectName, const TArray<FName>& BoneNames)
{
    if (!Client) return;

    FScopeLock Lock(&SubjectLock);

    FLiveLinkStaticDataStruct StaticData(FLiveLinkSkeletonStaticData::StaticStruct());
    FLiveLinkSkeletonStaticData* SkeletonData = StaticData.Cast<FLiveLinkSkeletonStaticData>();
    SkeletonData->BoneNames = BoneNames;

    // Set parent indices (flat hierarchy for now)
    SkeletonData->BoneParents.SetNum(BoneNames.Num());
    for (int32 i = 0; i < BoneNames.Num(); i++)
    {
        SkeletonData->BoneParents[i] = i == 0 ? INDEX_NONE : 0;  // All parented to root
    }

    Client->PushSubjectStaticData_AnyThread({SourceGuid, SubjectName}, ULiveLinkAnimationRole::StaticClass(), MoveTemp(StaticData));
    RegisteredSubjects.Add(SubjectName);

    UE_LOG(LogRshipLiveLink, Log, TEXT("Registered animation subject: %s with %d bones"), *SubjectName.ToString(), BoneNames.Num());
}

void FRshipLiveLinkSource::UnregisterSubject(const FName& SubjectName)
{
    FScopeLock Lock(&SubjectLock);

    if (RegisteredSubjects.Contains(SubjectName))
    {
        if (Client)
        {
            Client->RemoveSubject_AnyThread({SourceGuid, SubjectName});
        }
        RegisteredSubjects.Remove(SubjectName);
        UE_LOG(LogRshipLiveLink, Log, TEXT("Unregistered subject: %s"), *SubjectName.ToString());
    }
}

void FRshipLiveLinkSource::UpdateTransformSubject(const FName& SubjectName, const FTransform& Transform, double WorldTime)
{
    if (!Client || !bIsValid) return;

    FLiveLinkFrameDataStruct FrameData(FLiveLinkTransformFrameData::StaticStruct());
    FLiveLinkTransformFrameData* TransformData = FrameData.Cast<FLiveLinkTransformFrameData>();
    TransformData->Transform = Transform;
    TransformData->WorldTime = FLiveLinkWorldTime(WorldTime);

    Client->PushSubjectFrameData_AnyThread({SourceGuid, SubjectName}, MoveTemp(FrameData));
}

void FRshipLiveLinkSource::UpdateCameraSubject(const FName& SubjectName, const FTransform& Transform, float FOV, float FocusDistance, float Aperture, double WorldTime)
{
    if (!Client || !bIsValid) return;

    FLiveLinkFrameDataStruct FrameData(FLiveLinkCameraFrameData::StaticStruct());
    FLiveLinkCameraFrameData* CameraData = FrameData.Cast<FLiveLinkCameraFrameData>();
    CameraData->Transform = Transform;
    CameraData->FieldOfView = FOV;
    CameraData->FocusDistance = FocusDistance;
    CameraData->Aperture = Aperture;
    CameraData->WorldTime = FLiveLinkWorldTime(WorldTime);

    Client->PushSubjectFrameData_AnyThread({SourceGuid, SubjectName}, MoveTemp(FrameData));
}

void FRshipLiveLinkSource::UpdateLightSubject(const FName& SubjectName, const FTransform& Transform, float Intensity, FLinearColor Color, float Temperature, double WorldTime)
{
    if (!Client || !bIsValid) return;

    FLiveLinkFrameDataStruct FrameData(FLiveLinkLightFrameData::StaticStruct());
    FLiveLinkLightFrameData* LightData = FrameData.Cast<FLiveLinkLightFrameData>();
    LightData->Transform = Transform;
    LightData->Intensity = Intensity;
    LightData->LightColor = Color.ToFColor(true);
    LightData->Temperature = Temperature;
    LightData->WorldTime = FLiveLinkWorldTime(WorldTime);

    Client->PushSubjectFrameData_AnyThread({SourceGuid, SubjectName}, MoveTemp(FrameData));
}

void FRshipLiveLinkSource::UpdateAnimationSubject(const FName& SubjectName, const TArray<FTransform>& BoneTransforms, const TArray<FName>& BoneNames, double WorldTime)
{
    if (!Client || !bIsValid) return;

    FLiveLinkFrameDataStruct FrameData(FLiveLinkAnimationFrameData::StaticStruct());
    FLiveLinkAnimationFrameData* AnimData = FrameData.Cast<FLiveLinkAnimationFrameData>();
    AnimData->Transforms = BoneTransforms;
    AnimData->WorldTime = FLiveLinkWorldTime(WorldTime);

    Client->PushSubjectFrameData_AnyThread({SourceGuid, SubjectName}, MoveTemp(FrameData));
}

// ============================================================================
// LIVE LINK SERVICE IMPLEMENTATION
// ============================================================================

void URshipLiveLinkService::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;
    UE_LOG(LogRshipLiveLink, Log, TEXT("RshipLiveLinkService initialized"));
}

void URshipLiveLinkService::Shutdown()
{
    UnbindFromPulseReceiver();
    StopSource();

    SubjectConfigs.Empty();
    AnimationConfigs.Empty();

    UE_LOG(LogRshipLiveLink, Log, TEXT("RshipLiveLinkService shut down"));
}

void URshipLiveLinkService::Tick(float DeltaTime)
{
    // Apply smoothing to all subjects
    for (auto& Pair : SubjectConfigs)
    {
        if (Pair.Value.bEnabled && Pair.Value.Smoothing > 0.0f)
        {
            ApplySmoothing(Pair.Value, DeltaTime);
        }
    }
}

bool URshipLiveLinkService::StartSource()
{
    if (Source.IsValid() && Source->IsValid())
    {
        UE_LOG(LogRshipLiveLink, Warning, TEXT("Live Link source already active"));
        return true;
    }

    // Create the source
    Source = MakeShared<FRshipLiveLinkSource>();

    // Get the Live Link client
    IModularFeatures& ModularFeatures = IModularFeatures::Get();
    if (!ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
    {
        UE_LOG(LogRshipLiveLink, Error, TEXT("Live Link client not available"));
        OnError.Broadcast(TEXT("Live Link client not available"));
        Source.Reset();
        return false;
    }

    ILiveLinkClient* Client = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);

    // Register the source
    FGuid SourceGuid = Client->AddSource(Source);
    if (!SourceGuid.IsValid())
    {
        UE_LOG(LogRshipLiveLink, Error, TEXT("Failed to add Live Link source"));
        OnError.Broadcast(TEXT("Failed to add Live Link source"));
        Source.Reset();
        return false;
    }

    // Bind to pulse receiver for real-time updates
    BindToPulseReceiver();

    // Re-register all configured subjects
    for (const auto& Pair : SubjectConfigs)
    {
        const FRshipLiveLinkSubjectConfig& Config = Pair.Value;
        if (!Config.bEnabled) continue;

        switch (Config.SubjectType)
        {
            case ERshipLiveLinkSubjectType::Transform:
                Source->RegisterTransformSubject(Config.SubjectName);
                break;
            case ERshipLiveLinkSubjectType::Camera:
                Source->RegisterCameraSubject(Config.SubjectName);
                break;
            case ERshipLiveLinkSubjectType::Light:
                Source->RegisterLightSubject(Config.SubjectName);
                break;
            default:
                Source->RegisterTransformSubject(Config.SubjectName);
                break;
        }
    }

    for (const auto& Pair : AnimationConfigs)
    {
        const FRshipLiveLinkAnimationConfig& Config = Pair.Value;
        if (!Config.bEnabled) continue;

        TArray<FName> BoneNames;
        for (const auto& Bone : Config.BoneMappings)
        {
            BoneNames.Add(Bone.BoneName);
        }
        Source->RegisterAnimationSubject(Config.SubjectName, BoneNames);
    }

    UE_LOG(LogRshipLiveLink, Log, TEXT("Live Link source started"));
    return true;
}

void URshipLiveLinkService::StopSource()
{
    UnbindFromPulseReceiver();

    if (Source.IsValid())
    {
        Source->SetValid(false);
        Source.Reset();
        UE_LOG(LogRshipLiveLink, Log, TEXT("Live Link source stopped"));
    }
}

void URshipLiveLinkService::BindToPulseReceiver()
{
    if (!Subsystem) return;

    URshipPulseReceiver* Receiver = Subsystem->GetPulseReceiver();
    if (!Receiver) return;

    PulseHandle = Receiver->OnPulseReceived.AddUObject(this, &URshipLiveLinkService::OnPulseReceived);
}

void URshipLiveLinkService::UnbindFromPulseReceiver()
{
    if (!Subsystem) return;

    URshipPulseReceiver* Receiver = Subsystem->GetPulseReceiver();
    if (Receiver && PulseHandle.IsValid())
    {
        Receiver->OnPulseReceived.Remove(PulseHandle);
        PulseHandle.Reset();
    }
}

void URshipLiveLinkService::OnPulseReceived(const FString& EmitterId, TSharedPtr<FJsonObject> Data)
{
    if (!Source.IsValid() || !Source->IsValid()) return;

    // Check each subject config for matching emitter pattern
    for (auto& Pair : SubjectConfigs)
    {
        FRshipLiveLinkSubjectConfig& Config = Pair.Value;
        if (!Config.bEnabled) continue;

        if (MatchesPattern(EmitterId, Config.EmitterPattern))
        {
            UpdateSubjectFromPulse(Config, Data);
        }
    }
}

bool URshipLiveLinkService::MatchesPattern(const FString& EmitterId, const FString& Pattern)
{
    if (Pattern.IsEmpty()) return false;

    // Simple wildcard matching
    if (Pattern == TEXT("*")) return true;

    if (Pattern.Contains(TEXT("*")))
    {
        // Handle prefix wildcard (e.g., "fixture:*")
        FString Prefix = Pattern.Replace(TEXT("*"), TEXT(""));
        return EmitterId.StartsWith(Prefix);
    }

    return EmitterId == Pattern;
}

float URshipLiveLinkService::ExtractFloat(TSharedPtr<FJsonObject> Data, const FString& FieldPath, float Default)
{
    if (!Data.IsValid() || FieldPath.IsEmpty()) return Default;

    // Handle nested field paths (e.g., "values.pan")
    TArray<FString> Parts;
    FieldPath.ParseIntoArray(Parts, TEXT("."));

    TSharedPtr<FJsonObject> Current = Data;
    for (int32 i = 0; i < Parts.Num() - 1; i++)
    {
        if (!Current->HasField(Parts[i])) return Default;
        Current = Current->GetObjectField(Parts[i]);
        if (!Current.IsValid()) return Default;
    }

    const FString& FinalField = Parts.Last();
    if (Current->HasField(FinalField))
    {
        return Current->GetNumberField(FinalField);
    }

    return Default;
}

FLinearColor URshipLiveLinkService::ExtractColor(TSharedPtr<FJsonObject> Data, const FString& FieldPath)
{
    if (!Data.IsValid() || FieldPath.IsEmpty()) return FLinearColor::White;

    TSharedPtr<FJsonObject> ColorObj;

    // Handle nested field paths
    TArray<FString> Parts;
    FieldPath.ParseIntoArray(Parts, TEXT("."));

    TSharedPtr<FJsonObject> Current = Data;
    for (const FString& Part : Parts)
    {
        if (!Current->HasField(Part)) return FLinearColor::White;

        if (Part == Parts.Last())
        {
            ColorObj = Current->GetObjectField(Part);
        }
        else
        {
            Current = Current->GetObjectField(Part);
        }
    }

    if (!ColorObj.IsValid())
    {
        // Try as hex string
        FString HexStr;
        if (Data->TryGetStringField(FieldPath, HexStr))
        {
            FColor Color = FColor::FromHex(HexStr);
            return FLinearColor(Color);
        }
        return FLinearColor::White;
    }

    float R = ColorObj->HasField(TEXT("r")) ? ColorObj->GetNumberField(TEXT("r")) : 1.0f;
    float G = ColorObj->HasField(TEXT("g")) ? ColorObj->GetNumberField(TEXT("g")) : 1.0f;
    float B = ColorObj->HasField(TEXT("b")) ? ColorObj->GetNumberField(TEXT("b")) : 1.0f;
    float A = ColorObj->HasField(TEXT("a")) ? ColorObj->GetNumberField(TEXT("a")) : 1.0f;

    return FLinearColor(R, G, B, A);
}

void URshipLiveLinkService::UpdateSubjectFromPulse(FRshipLiveLinkSubjectConfig& Config, TSharedPtr<FJsonObject> Data)
{
    double WorldTime = FPlatformTime::Seconds();

    // Extract transform data
    FVector Position = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator;
    FVector Scale = FVector::OneVector;

    if (!Config.PositionXField.IsEmpty()) Position.X = ExtractFloat(Data, Config.PositionXField) * Config.PositionScale;
    if (!Config.PositionYField.IsEmpty()) Position.Y = ExtractFloat(Data, Config.PositionYField) * Config.PositionScale;
    if (!Config.PositionZField.IsEmpty()) Position.Z = ExtractFloat(Data, Config.PositionZField) * Config.PositionScale;

    if (!Config.RotationXField.IsEmpty()) Rotation.Pitch = ExtractFloat(Data, Config.RotationXField) * Config.RotationScale;
    if (!Config.RotationYField.IsEmpty()) Rotation.Yaw = ExtractFloat(Data, Config.RotationYField) * Config.RotationScale;
    if (!Config.RotationZField.IsEmpty()) Rotation.Roll = ExtractFloat(Data, Config.RotationZField) * Config.RotationScale;

    if (!Config.ScaleField.IsEmpty())
    {
        float UniformScale = ExtractFloat(Data, Config.ScaleField, 1.0f);
        Scale = FVector(UniformScale);
    }

    Config.TargetTransform = FTransform(Rotation, Position, Scale);

    // Apply based on mapping mode
    switch (Config.MappingMode)
    {
        case ERshipLiveLinkMappingMode::Direct:
            Config.CurrentTransform = Config.TargetTransform;
            break;

        case ERshipLiveLinkMappingMode::Accumulated:
            Config.CurrentTransform.SetLocation(Config.CurrentTransform.GetLocation() + Position);
            Config.CurrentTransform.SetRotation(Config.CurrentTransform.GetRotation() * FQuat(Rotation));
            break;

        case ERshipLiveLinkMappingMode::Smoothed:
            // Smoothing applied in Tick
            break;

        default:
            Config.CurrentTransform = Config.TargetTransform;
            break;
    }

    // Update based on subject type
    switch (Config.SubjectType)
    {
        case ERshipLiveLinkSubjectType::Transform:
            Source->UpdateTransformSubject(Config.SubjectName, Config.CurrentTransform, WorldTime);
            break;

        case ERshipLiveLinkSubjectType::Camera:
        {
            float FOV = ExtractFloat(Data, Config.FOVField, 90.0f);
            float FocusDist = ExtractFloat(Data, Config.FocusDistanceField, 0.0f);
            float Aperture = ExtractFloat(Data, Config.ApertureField, 2.8f);
            Source->UpdateCameraSubject(Config.SubjectName, Config.CurrentTransform, FOV, FocusDist, Aperture, WorldTime);
        }
        break;

        case ERshipLiveLinkSubjectType::Light:
        {
            float Intensity = ExtractFloat(Data, Config.IntensityField, 1.0f);
            FLinearColor Color = ExtractColor(Data, Config.ColorField);
            float Temperature = ExtractFloat(Data, Config.TemperatureField, 6500.0f);
            Config.CurrentIntensity = Intensity;
            Config.CurrentColor = Color;
            Source->UpdateLightSubject(Config.SubjectName, Config.CurrentTransform, Intensity, Color, Temperature, WorldTime);
        }
        break;

        default:
            Source->UpdateTransformSubject(Config.SubjectName, Config.CurrentTransform, WorldTime);
            break;
    }

    OnSubjectUpdated.Broadcast(Config.SubjectName, Config.CurrentTransform);
}

void URshipLiveLinkService::ApplySmoothing(FRshipLiveLinkSubjectConfig& Config, float DeltaTime)
{
    if (Config.Smoothing <= 0.0f) return;

    float Alpha = 1.0f - FMath::Pow(Config.Smoothing, DeltaTime * 60.0f);

    // Interpolate transform
    FVector CurrentLoc = Config.CurrentTransform.GetLocation();
    FVector TargetLoc = Config.TargetTransform.GetLocation();
    FVector NewLoc = FMath::Lerp(CurrentLoc, TargetLoc, Alpha);

    FQuat CurrentRot = Config.CurrentTransform.GetRotation();
    FQuat TargetRot = Config.TargetTransform.GetRotation();
    FQuat NewRot = FQuat::Slerp(CurrentRot, TargetRot, Alpha);

    Config.CurrentTransform.SetLocation(NewLoc);
    Config.CurrentTransform.SetRotation(NewRot);

    // Push smoothed data
    double WorldTime = FPlatformTime::Seconds();

    switch (Config.SubjectType)
    {
        case ERshipLiveLinkSubjectType::Transform:
            Source->UpdateTransformSubject(Config.SubjectName, Config.CurrentTransform, WorldTime);
            break;

        case ERshipLiveLinkSubjectType::Camera:
            Source->UpdateCameraSubject(Config.SubjectName, Config.CurrentTransform, Config.CurrentFOV, 0.0f, 2.8f, WorldTime);
            break;

        case ERshipLiveLinkSubjectType::Light:
            Source->UpdateLightSubject(Config.SubjectName, Config.CurrentTransform, Config.CurrentIntensity, Config.CurrentColor, 6500.0f, WorldTime);
            break;

        default:
            break;
    }
}

void URshipLiveLinkService::AddTransformSubject(const FRshipLiveLinkSubjectConfig& Config)
{
    FRshipLiveLinkSubjectConfig NewConfig = Config;
    NewConfig.SubjectType = ERshipLiveLinkSubjectType::Transform;
    SubjectConfigs.Add(Config.SubjectName, NewConfig);

    if (Source.IsValid() && Source->IsValid())
    {
        Source->RegisterTransformSubject(Config.SubjectName);
    }

    UE_LOG(LogRshipLiveLink, Log, TEXT("Added transform subject: %s"), *Config.SubjectName.ToString());
}

void URshipLiveLinkService::AddCameraSubject(const FRshipLiveLinkSubjectConfig& Config)
{
    FRshipLiveLinkSubjectConfig NewConfig = Config;
    NewConfig.SubjectType = ERshipLiveLinkSubjectType::Camera;
    SubjectConfigs.Add(Config.SubjectName, NewConfig);

    if (Source.IsValid() && Source->IsValid())
    {
        Source->RegisterCameraSubject(Config.SubjectName);
    }

    UE_LOG(LogRshipLiveLink, Log, TEXT("Added camera subject: %s"), *Config.SubjectName.ToString());
}

void URshipLiveLinkService::AddLightSubject(const FRshipLiveLinkSubjectConfig& Config)
{
    FRshipLiveLinkSubjectConfig NewConfig = Config;
    NewConfig.SubjectType = ERshipLiveLinkSubjectType::Light;
    SubjectConfigs.Add(Config.SubjectName, NewConfig);

    if (Source.IsValid() && Source->IsValid())
    {
        Source->RegisterLightSubject(Config.SubjectName);
    }

    UE_LOG(LogRshipLiveLink, Log, TEXT("Added light subject: %s"), *Config.SubjectName.ToString());
}

void URshipLiveLinkService::AddAnimationSubject(const FRshipLiveLinkAnimationConfig& Config)
{
    AnimationConfigs.Add(Config.SubjectName, Config);

    if (Source.IsValid() && Source->IsValid())
    {
        TArray<FName> BoneNames;
        for (const auto& Bone : Config.BoneMappings)
        {
            BoneNames.Add(Bone.BoneName);
        }
        Source->RegisterAnimationSubject(Config.SubjectName, BoneNames);
    }

    UE_LOG(LogRshipLiveLink, Log, TEXT("Added animation subject: %s"), *Config.SubjectName.ToString());
}

void URshipLiveLinkService::RemoveSubject(FName SubjectName)
{
    SubjectConfigs.Remove(SubjectName);
    AnimationConfigs.Remove(SubjectName);

    if (Source.IsValid())
    {
        Source->UnregisterSubject(SubjectName);
    }

    UE_LOG(LogRshipLiveLink, Log, TEXT("Removed subject: %s"), *SubjectName.ToString());
}

TArray<FName> URshipLiveLinkService::GetAllSubjectNames() const
{
    TArray<FName> Names;

    for (const auto& Pair : SubjectConfigs)
    {
        Names.Add(Pair.Key);
    }
    for (const auto& Pair : AnimationConfigs)
    {
        Names.Add(Pair.Key);
    }

    return Names;
}

void URshipLiveLinkService::ClearAllSubjects()
{
    for (const auto& Pair : SubjectConfigs)
    {
        if (Source.IsValid())
        {
            Source->UnregisterSubject(Pair.Key);
        }
    }
    for (const auto& Pair : AnimationConfigs)
    {
        if (Source.IsValid())
        {
            Source->UnregisterSubject(Pair.Key);
        }
    }

    SubjectConfigs.Empty();
    AnimationConfigs.Empty();

    UE_LOG(LogRshipLiveLink, Log, TEXT("Cleared all subjects"));
}

int32 URshipLiveLinkService::CreateSubjectsFromFixtures()
{
    if (!Subsystem) return 0;

    URshipFixtureManager* FM = Subsystem->GetFixtureManager();
    if (!FM) return 0;

    int32 Count = 0;
    TArray<FRshipFixtureInfo> Fixtures = FM->GetAllFixtures();

    for (const FRshipFixtureInfo& Fixture : Fixtures)
    {
        FRshipLiveLinkSubjectConfig Config;
        Config.SubjectName = FName(*Fixture.Name);
        Config.SubjectType = ERshipLiveLinkSubjectType::Light;
        Config.EmitterPattern = FString::Printf(TEXT("fixture:%s:*"), *Fixture.Id);
        Config.IntensityField = TEXT("intensity");
        Config.ColorField = TEXT("color");

        AddLightSubject(Config);
        Count++;
    }

    UE_LOG(LogRshipLiveLink, Log, TEXT("Created %d subjects from fixtures"), Count);
    return Count;
}

void URshipLiveLinkService::CreateCameraTrackingSubject(const FString& EmitterId, FName SubjectName)
{
    FRshipLiveLinkSubjectConfig Config;
    Config.SubjectName = SubjectName;
    Config.SubjectType = ERshipLiveLinkSubjectType::Camera;
    Config.EmitterPattern = EmitterId;
    Config.RotationYField = TEXT("values.pan");
    Config.RotationXField = TEXT("values.tilt");
    Config.FOVField = TEXT("values.zoom");
    Config.RotationScale = 1.0f;
    Config.Smoothing = 0.3f;

    AddCameraSubject(Config);
}

void URshipLiveLinkService::CreateLightTrackingSubject(const FString& EmitterId, FName SubjectName)
{
    FRshipLiveLinkSubjectConfig Config;
    Config.SubjectName = SubjectName;
    Config.SubjectType = ERshipLiveLinkSubjectType::Light;
    Config.EmitterPattern = EmitterId;
    Config.IntensityField = TEXT("intensity");
    Config.ColorField = TEXT("color");
    Config.RotationYField = TEXT("values.pan");
    Config.RotationXField = TEXT("values.tilt");

    AddLightSubject(Config);
}

void URshipLiveLinkService::UpdateTransform(FName SubjectName, FTransform Transform)
{
    if (Source.IsValid() && Source->IsValid())
    {
        Source->UpdateTransformSubject(SubjectName, Transform, FPlatformTime::Seconds());
    }
}

void URshipLiveLinkService::UpdateCamera(FName SubjectName, FTransform Transform, float FOV, float FocusDistance, float Aperture)
{
    if (Source.IsValid() && Source->IsValid())
    {
        Source->UpdateCameraSubject(SubjectName, Transform, FOV, FocusDistance, Aperture, FPlatformTime::Seconds());
    }
}

void URshipLiveLinkService::UpdateLight(FName SubjectName, FTransform Transform, float Intensity, FLinearColor Color)
{
    if (Source.IsValid() && Source->IsValid())
    {
        Source->UpdateLightSubject(SubjectName, Transform, Intensity, Color, 6500.0f, FPlatformTime::Seconds());
    }
}
