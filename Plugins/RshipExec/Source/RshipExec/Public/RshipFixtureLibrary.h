// Rship Fixture Library
// Manages fixture type definitions, GDTF import, and manufacturer profiles

#pragma once

#include "CoreMinimal.h"
#include "RshipCalibrationTypes.h"
#include "RshipFixtureLibrary.generated.h"

class URshipSubsystem;

// ============================================================================
// FIXTURE PROFILE TYPES
// ============================================================================

UENUM(BlueprintType)
enum class ERshipFixtureCategory : uint8
{
    Conventional    UMETA(DisplayName = "Conventional"),
    LED             UMETA(DisplayName = "LED"),
    Moving          UMETA(DisplayName = "Moving Light"),
    Wash            UMETA(DisplayName = "Wash"),
    Spot            UMETA(DisplayName = "Spot"),
    Beam            UMETA(DisplayName = "Beam"),
    Profile         UMETA(DisplayName = "Profile/ERS"),
    Strobe          UMETA(DisplayName = "Strobe"),
    Laser           UMETA(DisplayName = "Laser"),
    Effect          UMETA(DisplayName = "Effect"),
    Atmospheric     UMETA(DisplayName = "Atmospheric (Haze/Fog)"),
    Practical       UMETA(DisplayName = "Practical"),
    Architectural   UMETA(DisplayName = "Architectural"),
    Other           UMETA(DisplayName = "Other")
};

UENUM(BlueprintType)
enum class ERshipColorMixing : uint8
{
    None            UMETA(DisplayName = "None (Single Color)"),
    RGB             UMETA(DisplayName = "RGB"),
    RGBW            UMETA(DisplayName = "RGBW"),
    RGBA            UMETA(DisplayName = "RGBA"),
    RGBAW           UMETA(DisplayName = "RGBAW"),
    CMY             UMETA(DisplayName = "CMY"),
    ColorWheel      UMETA(DisplayName = "Color Wheel"),
    ColorTemp       UMETA(DisplayName = "Color Temperature"),
    Custom          UMETA(DisplayName = "Custom")
};

/**
 * Physical dimensions of a fixture
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipFixtureDimensions
{
    GENERATED_BODY()

    /** Width in mm */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    float Width = 0.0f;

    /** Height in mm */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    float Height = 0.0f;

    /** Depth in mm */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    float Depth = 0.0f;

    /** Weight in kg */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    float Weight = 0.0f;
};

/**
 * Beam/optics specification
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipBeamProfile
{
    GENERATED_BODY()

    /** Minimum beam angle (degrees) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    float BeamAngleMin = 10.0f;

    /** Maximum beam angle (degrees) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    float BeamAngleMax = 10.0f;

    /** Minimum field angle (degrees) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    float FieldAngleMin = 20.0f;

    /** Maximum field angle (degrees) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    float FieldAngleMax = 20.0f;

    /** Has zoom capability */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    bool bHasZoom = false;

    /** Has focus capability */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    bool bHasFocus = false;

    /** Has iris */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    bool bHasIris = false;

    /** Has framing shutters */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    bool bHasFramingShutters = false;

    /** Number of shutters (0-4 typically) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    int32 ShutterCount = 0;
};

/**
 * Color system specification
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipColorSystem
{
    GENERATED_BODY()

    /** Color mixing type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    ERshipColorMixing MixingType = ERshipColorMixing::None;

    /** Minimum color temperature (Kelvin) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    float ColorTempMin = 2700.0f;

    /** Maximum color temperature (Kelvin) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    float ColorTempMax = 6500.0f;

    /** CRI rating */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    int32 CRI = 0;

    /** TLCI rating */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    int32 TLCI = 0;

    /** Has color wheel */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    bool bHasColorWheel = false;

    /** Number of color wheel slots */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    int32 ColorWheelSlots = 0;

    /** Color wheel colors (names) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    TArray<FString> ColorWheelColors;
};

/**
 * Gobo wheel specification
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipGoboWheel
{
    GENERATED_BODY()

    /** Wheel name (e.g., "Gobo 1", "Gobo 2") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    FString Name;

    /** Number of slots */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    int32 SlotCount = 0;

    /** Is rotating gobo wheel */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    bool bRotating = false;

    /** Gobo names/descriptions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    TArray<FString> GoboNames;

    /** Gobo image paths (if available) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    TArray<FString> GoboImagePaths;
};

/**
 * Movement specification for moving lights
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipMovementSpec
{
    GENERATED_BODY()

    /** Has pan movement */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    bool bHasPan = false;

    /** Pan range in degrees */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    float PanRange = 540.0f;

    /** Has tilt movement */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    bool bHasTilt = false;

    /** Tilt range in degrees */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    float TiltRange = 270.0f;

    /** Pan speed (degrees/second) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    float PanSpeed = 0.0f;

    /** Tilt speed (degrees/second) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    float TiltSpeed = 0.0f;

    /** Has 16-bit pan/tilt */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    bool bIs16Bit = false;
};

/**
 * DMX channel definition for fixture library
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipFixtureDMXChannel
{
    GENERATED_BODY()

    /** Channel offset (0-based) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    int32 Offset = 0;

    /** Channel name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    FString Name;

    /** Channel function (e.g., "Dimmer", "Pan", "Red") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    FString Function;

    /** Default value */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    uint8 DefaultValue = 0;

    /** Is 16-bit channel (has fine channel) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    bool bIs16Bit = false;

    /** Fine channel offset (if 16-bit) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    int32 FineOffset = -1;
};

/**
 * DMX mode definition
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipDMXMode
{
    GENERATED_BODY()

    /** Mode name (e.g., "Standard", "Extended", "Basic") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    FString Name;

    /** Total channel count */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    int32 ChannelCount = 0;

    /** Channel definitions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    TArray<FRshipFixtureDMXChannel> Channels;

    /** Is default mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    bool bIsDefault = false;
};

/**
 * Complete fixture type definition
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipFixtureProfile
{
    GENERATED_BODY()

    // ========================================================================
    // IDENTITY
    // ========================================================================

    /** Unique profile ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    FString Id;

    /** GDTF fixture ID (if imported from GDTF) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    FString GDTFId;

    /** Manufacturer name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    FString Manufacturer;

    /** Model name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    FString Model;

    /** Model revision/version */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    FString Revision;

    /** Category */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    ERshipFixtureCategory Category = ERshipFixtureCategory::Other;

    /** User-friendly display name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    FString DisplayName;

    // ========================================================================
    // PHOTOMETRIC DATA
    // ========================================================================

    /** Lamp wattage */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    float Wattage = 0.0f;

    /** Total power draw */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    float PowerDraw = 0.0f;

    /** Luminous flux in lumens */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    float LumensOutput = 0.0f;

    /** Beam profile */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    FRshipBeamProfile BeamProfile;

    /** Path to IES profile file */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    FString IESProfilePath;

    // ========================================================================
    // COLOR
    // ========================================================================

    /** Color system */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    FRshipColorSystem ColorSystem;

    // ========================================================================
    // GOBOS
    // ========================================================================

    /** Gobo wheels */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    TArray<FRshipGoboWheel> GoboWheels;

    // ========================================================================
    // MOVEMENT
    // ========================================================================

    /** Movement specification */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    FRshipMovementSpec Movement;

    // ========================================================================
    // PHYSICAL
    // ========================================================================

    /** Physical dimensions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    FRshipFixtureDimensions Dimensions;

    // ========================================================================
    // DMX
    // ========================================================================

    /** Available DMX modes */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    TArray<FRshipDMXMode> DMXModes;

    // ========================================================================
    // CALIBRATION
    // ========================================================================

    /** Default calibration data */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    FRshipFixtureCalibration DefaultCalibration;

    // ========================================================================
    // METADATA
    // ========================================================================

    /** Tags for searching/filtering */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    TArray<FString> Tags;

    /** Notes/description */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    FString Notes;

    /** Thumbnail image path */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    FString ThumbnailPath;

    /** 3D model path */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    FString Model3DPath;

    /** Source (e.g., "GDTF", "Manual", "rship") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    FString Source;

    /** Last updated timestamp */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Library")
    FDateTime LastUpdated;
};

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProfileLoaded, const FRshipFixtureProfile&, Profile);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLibraryUpdated, int32, ProfileCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGDTFImportComplete, bool, bSuccess, const FString&, ErrorMessage);

// ============================================================================
// FIXTURE LIBRARY SERVICE
// ============================================================================

/**
 * Manages fixture type definitions and profile library.
 * Supports GDTF import and synchronization with rship server.
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipFixtureLibrary : public UObject
{
    GENERATED_BODY()

public:
    /** Initialize with subsystem reference */
    void Initialize(URshipSubsystem* InSubsystem);

    /** Shutdown and cleanup */
    void Shutdown();

    // ========================================================================
    // PROFILE QUERIES
    // ========================================================================

    /** Get all profiles in the library */
    UFUNCTION(BlueprintCallable, Category = "Rship|Library")
    TArray<FRshipFixtureProfile> GetAllProfiles() const;

    /** Get profile by ID */
    UFUNCTION(BlueprintCallable, Category = "Rship|Library")
    bool GetProfile(const FString& ProfileId, FRshipFixtureProfile& OutProfile) const;

    /** Search profiles by manufacturer */
    UFUNCTION(BlueprintCallable, Category = "Rship|Library")
    TArray<FRshipFixtureProfile> GetProfilesByManufacturer(const FString& Manufacturer) const;

    /** Search profiles by category */
    UFUNCTION(BlueprintCallable, Category = "Rship|Library")
    TArray<FRshipFixtureProfile> GetProfilesByCategory(ERshipFixtureCategory Category) const;

    /** Search profiles by tag */
    UFUNCTION(BlueprintCallable, Category = "Rship|Library")
    TArray<FRshipFixtureProfile> GetProfilesByTag(const FString& Tag) const;

    /** Search profiles (text search across name, manufacturer, model) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Library")
    TArray<FRshipFixtureProfile> SearchProfiles(const FString& SearchText) const;

    /** Get all manufacturers in library */
    UFUNCTION(BlueprintCallable, Category = "Rship|Library")
    TArray<FString> GetManufacturers() const;

    /** Get profile count */
    UFUNCTION(BlueprintCallable, Category = "Rship|Library")
    int32 GetProfileCount() const { return Profiles.Num(); }

    // ========================================================================
    // PROFILE MANAGEMENT
    // ========================================================================

    /** Add or update a profile */
    UFUNCTION(BlueprintCallable, Category = "Rship|Library")
    void AddProfile(const FRshipFixtureProfile& Profile);

    /** Remove a profile */
    UFUNCTION(BlueprintCallable, Category = "Rship|Library")
    bool RemoveProfile(const FString& ProfileId);

    /** Create a profile from an existing fixture (reverse engineer) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Library")
    FRshipFixtureProfile CreateProfileFromFixture(const FString& FixtureId);

    // ========================================================================
    // GDTF IMPORT
    // ========================================================================

    /** Import a GDTF file */
    UFUNCTION(BlueprintCallable, Category = "Rship|Library")
    bool ImportGDTF(const FString& FilePath, FRshipFixtureProfile& OutProfile, FString& OutError);

    /** Import multiple GDTF files from a directory */
    UFUNCTION(BlueprintCallable, Category = "Rship|Library")
    int32 ImportGDTFDirectory(const FString& DirectoryPath);

    /** Download GDTF from fixture ID (via GDTF Share) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Library")
    void DownloadGDTF(const FString& FixtureId);

    // ========================================================================
    // SYNC WITH RSHIP
    // ========================================================================

    /** Sync library with rship server */
    UFUNCTION(BlueprintCallable, Category = "Rship|Library")
    void SyncWithServer();

    /** Upload profile to rship server */
    UFUNCTION(BlueprintCallable, Category = "Rship|Library")
    void UploadProfile(const FString& ProfileId);

    /** Download profile from rship server */
    UFUNCTION(BlueprintCallable, Category = "Rship|Library")
    void DownloadProfile(const FString& ProfileId);

    // ========================================================================
    // PERSISTENCE
    // ========================================================================

    /** Save library to file */
    UFUNCTION(BlueprintCallable, Category = "Rship|Library")
    bool SaveLibrary();

    /** Load library from file */
    UFUNCTION(BlueprintCallable, Category = "Rship|Library")
    bool LoadLibrary();

    /** Get library file path */
    UFUNCTION(BlueprintCallable, Category = "Rship|Library")
    FString GetLibraryPath() const;

    // ========================================================================
    // EVENTS
    // ========================================================================

    /** Fired when a profile is loaded/updated */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Library")
    FOnProfileLoaded OnProfileLoaded;

    /** Fired when the library is updated */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Library")
    FOnLibraryUpdated OnLibraryUpdated;

    /** Fired when GDTF import completes */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Library")
    FOnGDTFImportComplete OnGDTFImportComplete;

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    // Profile storage
    UPROPERTY()
    TMap<FString, FRshipFixtureProfile> Profiles;

    // GDTF parsing helpers
    bool ParseGDTFArchive(const FString& FilePath, FRshipFixtureProfile& OutProfile, FString& OutError);
    bool ParseGDTFDescription(const FString& XMLContent, FRshipFixtureProfile& OutProfile);
    void ExtractGDTFResources(const FString& ArchivePath, const FString& ProfileId);

    // Convert to/from JSON for persistence
    TSharedPtr<FJsonObject> ProfileToJson(const FRshipFixtureProfile& Profile) const;
    FRshipFixtureProfile JsonToProfile(const TSharedPtr<FJsonObject>& Json) const;

    // Server sync handlers
    void ProcessProfileEvent(const TSharedPtr<FJsonObject>& Data, bool bIsDelete);
};
