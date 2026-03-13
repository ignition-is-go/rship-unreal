// Rship Live Link Source
// Expose rship pulse data as Live Link subjects for real-time streaming

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ILiveLinkSource.h"
#include "LiveLinkTypes.h"
#include "Roles/LiveLinkTransformRole.h"
#include "Roles/LiveLinkAnimationRole.h"
#include "Roles/LiveLinkCameraRole.h"
#include "Roles/LiveLinkLightRole.h"
#include "RshipLiveLinkSource.generated.h"

class URshipSubsystem;
class URshipPulseReceiver;
class FJsonObject;

// ============================================================================
// LIVE LINK MODE
// ============================================================================

/**
 * Mode for LiveLink service data flow direction
 */
UENUM(BlueprintType)
enum class ERshipLiveLinkMode : uint8
{
    Consume         UMETA(DisplayName = "Consume (rship -> LiveLink)"),    // rship pulses become LiveLink subjects (current)
    Publish         UMETA(DisplayName = "Publish (LiveLink -> rship)"),    // LiveLink subjects become rship emitters (new)
    Bidirectional   UMETA(DisplayName = "Bidirectional")                   // Both directions
};

// ============================================================================
// LIVE LINK SUBJECT TYPES
// ============================================================================

/** Type of Live Link subject to create */
UENUM(BlueprintType)
enum class ERshipLiveLinkSubjectType : uint8
{
    Transform       UMETA(DisplayName = "Transform"),       // Position, rotation, scale
    Camera          UMETA(DisplayName = "Camera"),          // Camera with FOV, focus
    Light           UMETA(DisplayName = "Light"),           // Light with intensity, color
    Animation       UMETA(DisplayName = "Animation"),       // Bone animation data
    Custom          UMETA(DisplayName = "Custom")           // Custom property data
};

/** Mapping mode for pulse data to Live Link */
UENUM(BlueprintType)
enum class ERshipLiveLinkMappingMode : uint8
{
    Direct          UMETA(DisplayName = "Direct"),          // Direct value mapping
    Accumulated     UMETA(DisplayName = "Accumulated"),     // Add to previous value
    Velocity        UMETA(DisplayName = "Velocity"),        // Apply as velocity
    Smoothed        UMETA(DisplayName = "Smoothed")         // Apply with smoothing
};

// ============================================================================
// LIVE LINK SUBJECT CONFIGURATION
// ============================================================================

/** Configuration for a single Live Link subject */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipLiveLinkSubjectConfig
{
    GENERATED_BODY()

    /** Unique subject name (visible in Live Link) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink")
    FName SubjectName;

    /** Type of Live Link data to publish */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink")
    ERshipLiveLinkSubjectType SubjectType = ERshipLiveLinkSubjectType::Transform;

    /** Emitter ID pattern to receive data from (supports wildcards) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink")
    FString EmitterPattern;

    /** Mapping mode for data */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink")
    ERshipLiveLinkMappingMode MappingMode = ERshipLiveLinkMappingMode::Direct;

    /** Smoothing factor (0 = instant, 1 = very slow) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink", meta = (ClampMin = "0.0", ClampMax = "0.99"))
    float Smoothing = 0.0f;

    /** Whether this subject is enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink")
    bool bEnabled = true;

    // ========================================================================
    // TRANSFORM MAPPING
    // ========================================================================

    /** Field path for position X (e.g., "position.x" or "values.pan") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink|Transform")
    FString PositionXField;

    /** Field path for position Y */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink|Transform")
    FString PositionYField;

    /** Field path for position Z */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink|Transform")
    FString PositionZField;

    /** Field path for rotation X (pitch) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink|Transform")
    FString RotationXField;

    /** Field path for rotation Y (yaw) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink|Transform")
    FString RotationYField;

    /** Field path for rotation Z (roll) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink|Transform")
    FString RotationZField;

    /** Field path for uniform scale */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink|Transform")
    FString ScaleField;

    /** Position scale factor (multiply incoming position values) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink|Transform")
    float PositionScale = 1.0f;

    /** Rotation scale factor (multiply incoming rotation values, degrees) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink|Transform")
    float RotationScale = 1.0f;

    // ========================================================================
    // CAMERA MAPPING
    // ========================================================================

    /** Field path for field of view */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink|Camera")
    FString FOVField;

    /** Field path for focus distance */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink|Camera")
    FString FocusDistanceField;

    /** Field path for aperture */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink|Camera")
    FString ApertureField;

    // ========================================================================
    // LIGHT MAPPING
    // ========================================================================

    /** Field path for light intensity */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink|Light")
    FString IntensityField = TEXT("intensity");

    /** Field path for light color */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink|Light")
    FString ColorField = TEXT("color");

    /** Field path for light temperature (Kelvin) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink|Light")
    FString TemperatureField;

    // Runtime state
    FTransform CurrentTransform = FTransform::Identity;
    FTransform TargetTransform = FTransform::Identity;
    float CurrentFOV = 90.0f;
    float CurrentIntensity = 1.0f;
    FLinearColor CurrentColor = FLinearColor::White;
};

/** Animation bone mapping for skeletal animation */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipLiveLinkBoneMapping
{
    GENERATED_BODY()

    /** Bone name in the skeleton */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink")
    FName BoneName;

    /** Emitter ID pattern for this bone's data */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink")
    FString EmitterPattern;

    /** Field paths for bone transform */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink")
    FString PositionXField;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink")
    FString PositionYField;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink")
    FString PositionZField;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink")
    FString RotationXField;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink")
    FString RotationYField;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink")
    FString RotationZField;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink")
    FString RotationWField;

    // Runtime state
    FTransform CurrentTransform = FTransform::Identity;
};

/** Animation subject configuration */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipLiveLinkAnimationConfig
{
    GENERATED_BODY()

    /** Subject name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink")
    FName SubjectName;

    /** Skeleton asset reference name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink")
    FString SkeletonName;

    /** Bone mappings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink")
    TArray<FRshipLiveLinkBoneMapping> BoneMappings;

    /** Whether this subject is enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink")
    bool bEnabled = true;
};

// ============================================================================
// EMITTER MAPPING (LiveLink -> rship)
// ============================================================================

/**
 * Configuration for publishing a LiveLink subject to rship as an emitter
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipLiveLinkEmitterMapping
{
    GENERATED_BODY()

    /** LiveLink subject name to subscribe to */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink")
    FName SubjectName;

    /** rship Target ID to publish under */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink")
    FString TargetId = TEXT("UE_LiveLink");

    /** rship Emitter ID (will be SubjectName if empty) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink")
    FString EmitterId;

    /** Publish rate limit (Hz, 0 = every frame) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink", meta = (ClampMin = "0.0", ClampMax = "120.0"))
    float PublishRateHz = 30.0f;

    /** Whether this mapping is enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|LiveLink")
    bool bEnabled = true;

    // Runtime state
    double LastPublishTime = 0.0;

    FString GetEffectiveEmitterId() const
    {
        return EmitterId.IsEmpty() ? SubjectName.ToString() : EmitterId;
    }
};

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLiveLinkSubjectUpdated, FName, SubjectName, FTransform, Transform);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLiveLinkSourceError, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLiveLinkEmitterPublished, FName, SubjectName, const FString&, EmitterId);

// ============================================================================
// LIVE LINK SOURCE (Internal)
// ============================================================================

/**
 * Internal Live Link source implementation.
 * This is the actual source that gets registered with the Live Link client.
 */
class RSHIPEXEC_API FRshipLiveLinkSource : public ILiveLinkSource
{
public:
    FRshipLiveLinkSource();
    virtual ~FRshipLiveLinkSource();

    // ILiveLinkSource interface
    virtual void ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid) override;
    virtual bool IsSourceStillValid() const override;
    virtual bool RequestSourceShutdown() override;
    virtual FText GetSourceType() const override;
    virtual FText GetSourceMachineName() const override;
    virtual FText GetSourceStatus() const override;

    // Subject management
    void UpdateTransformSubject(const FName& SubjectName, const FTransform& Transform, double WorldTime);
    void UpdateCameraSubject(const FName& SubjectName, const FTransform& Transform, float FOV, float FocusDistance, float Aperture, double WorldTime);
    void UpdateLightSubject(const FName& SubjectName, const FTransform& Transform, float Intensity, FLinearColor Color, float Temperature, double WorldTime);
    void UpdateAnimationSubject(const FName& SubjectName, const TArray<FTransform>& BoneTransforms, const TArray<FName>& BoneNames, double WorldTime);

    void RegisterTransformSubject(const FName& SubjectName);
    void RegisterCameraSubject(const FName& SubjectName);
    void RegisterLightSubject(const FName& SubjectName);
    void RegisterAnimationSubject(const FName& SubjectName, const TArray<FName>& BoneNames);
    void UnregisterSubject(const FName& SubjectName);

    bool IsValid() const { return bIsValid; }
    void SetValid(bool bValid) { bIsValid = bValid; }

private:
    ILiveLinkClient* Client = nullptr;
    FGuid SourceGuid;
    bool bIsValid = true;

    TSet<FName> RegisteredSubjects;
    FCriticalSection SubjectLock;
};

// ============================================================================
// LIVE LINK SERVICE
// ============================================================================

/**
 * Service for publishing rship data to Live Link.
 * Manages Live Link source registration and subject updates.
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipLiveLinkService : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(URshipSubsystem* InSubsystem);
    void Shutdown();
    void Tick(float DeltaTime);

    // ========================================================================
    // SOURCE MANAGEMENT
    // ========================================================================

    /** Start the Live Link source */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    bool StartSource();

    /** Stop the Live Link source */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    void StopSource();

    /** Is the source currently active */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|LiveLink")
    bool IsSourceActive() const { return Source.IsValid() && Source->IsValid(); }

    // ========================================================================
    // SUBJECT MANAGEMENT
    // ========================================================================

    /** Add a transform subject */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    void AddTransformSubject(const FRshipLiveLinkSubjectConfig& Config);

    /** Add a camera subject */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    void AddCameraSubject(const FRshipLiveLinkSubjectConfig& Config);

    /** Add a light subject */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    void AddLightSubject(const FRshipLiveLinkSubjectConfig& Config);

    /** Add an animation subject */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    void AddAnimationSubject(const FRshipLiveLinkAnimationConfig& Config);

    /** Remove a subject by name */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    void RemoveSubject(FName SubjectName);

    /** Get all subject names */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    TArray<FName> GetAllSubjectNames() const;

    /** Clear all subjects */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    void ClearAllSubjects();

    // ========================================================================
    // QUICK SETUP
    // ========================================================================

    /** Create subjects from all fixtures */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    int32 CreateSubjectsFromFixtures();

    /** Create a camera tracking subject (pan/tilt to rotation) */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    void CreateCameraTrackingSubject(const FString& EmitterId, FName SubjectName);

    /** Create a light tracking subject */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    void CreateLightTrackingSubject(const FString& EmitterId, FName SubjectName);

    // ========================================================================
    // DIRECT UPDATES (for manual control)
    // ========================================================================

    /** Manually update a transform subject */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    void UpdateTransform(FName SubjectName, FTransform Transform);

    /** Manually update a camera subject */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    void UpdateCamera(FName SubjectName, FTransform Transform, float FOV, float FocusDistance = 0.0f, float Aperture = 2.8f);

    /** Manually update a light subject */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    void UpdateLight(FName SubjectName, FTransform Transform, float Intensity, FLinearColor Color);

    // ========================================================================
    // MODE CONTROL (BIDIRECTIONAL)
    // ========================================================================

    /** Set the LiveLink mode (Consume/Publish/Bidirectional) */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    void SetMode(ERshipLiveLinkMode NewMode);

    /** Get the current mode */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|LiveLink")
    ERshipLiveLinkMode GetMode() const { return CurrentMode; }

    // ========================================================================
    // EMITTER PUBLISHING (LiveLink -> rship)
    // ========================================================================

    /** Add a subject-to-emitter mapping (publish LiveLink subject to rship) */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    void AddEmitterMapping(const FRshipLiveLinkEmitterMapping& Mapping);

    /** Remove an emitter mapping by subject name */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    void RemoveEmitterMapping(FName SubjectName);

    /** Get all emitter mappings */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    TArray<FRshipLiveLinkEmitterMapping> GetAllEmitterMappings() const;

    /** Clear all emitter mappings */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    void ClearAllEmitterMappings();

    /** Get available LiveLink subjects (for UI population) */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    TArray<FName> GetAvailableLiveLinkSubjects() const;

    /** Auto-create emitter mappings for all available subjects */
    UFUNCTION(BlueprintCallable, Category = "Rship|LiveLink")
    int32 CreateEmitterMappingsForAllSubjects();

    // ========================================================================
    // EVENTS
    // ========================================================================

    UPROPERTY(BlueprintAssignable, Category = "Rship|LiveLink")
    FOnLiveLinkSubjectUpdated OnSubjectUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Rship|LiveLink")
    FOnLiveLinkSourceError OnError;

    UPROPERTY(BlueprintAssignable, Category = "Rship|LiveLink")
    FOnLiveLinkEmitterPublished OnEmitterPublished;

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    TSharedPtr<FRshipLiveLinkSource> Source;

    TMap<FName, FRshipLiveLinkSubjectConfig> SubjectConfigs;
    TMap<FName, FRshipLiveLinkAnimationConfig> AnimationConfigs;

    // Mode and emitter publishing
    ERshipLiveLinkMode CurrentMode = ERshipLiveLinkMode::Consume;
    TMap<FName, FRshipLiveLinkEmitterMapping> EmitterMappings;

    FDelegateHandle PulseHandle;

    void BindToPulseReceiver();
    void UnbindFromPulseReceiver();
    void OnPulseReceived(const FString& EmitterId, float Intensity, FLinearColor Color, TSharedPtr<FJsonObject> Data);

    bool MatchesPattern(const FString& EmitterId, const FString& Pattern);
    float ExtractFloat(TSharedPtr<FJsonObject> Data, const FString& FieldPath, float Default = 0.0f);
    FLinearColor ExtractColor(TSharedPtr<FJsonObject> Data, const FString& FieldPath);

    void UpdateSubjectFromPulse(FRshipLiveLinkSubjectConfig& Config, TSharedPtr<FJsonObject> Data);
    void ApplySmoothing(FRshipLiveLinkSubjectConfig& Config, float DeltaTime);

    // Emitter publishing
    void PublishEmitterMappings();
    void PublishSubjectToRship(FRshipLiveLinkEmitterMapping& Mapping);
};
