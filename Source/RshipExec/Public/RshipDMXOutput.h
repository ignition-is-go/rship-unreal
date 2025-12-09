// Rship DMX Output
// Send fixture values to DMX universes for controlling real-world fixtures

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "RshipDMXOutput.generated.h"

class URshipSubsystem;
class URshipFixtureManager;

/** DMX Protocol selection */
UENUM(BlueprintType)
enum class ERshipDMXProtocol : uint8
{
    ArtNet      UMETA(DisplayName = "Art-Net"),
    sACN        UMETA(DisplayName = "sACN (E1.31)")
};

// ============================================================================
// DMX CHANNEL TYPES
// ============================================================================

/** Standard DMX channel functions */
UENUM(BlueprintType)
enum class ERshipDMXChannelType : uint8
{
    Dimmer          UMETA(DisplayName = "Dimmer"),
    Red             UMETA(DisplayName = "Red"),
    Green           UMETA(DisplayName = "Green"),
    Blue            UMETA(DisplayName = "Blue"),
    White           UMETA(DisplayName = "White"),
    Amber           UMETA(DisplayName = "Amber"),
    UV              UMETA(DisplayName = "UV"),
    Pan             UMETA(DisplayName = "Pan"),
    PanFine         UMETA(DisplayName = "Pan Fine"),
    Tilt            UMETA(DisplayName = "Tilt"),
    TiltFine        UMETA(DisplayName = "Tilt Fine"),
    ColorWheel      UMETA(DisplayName = "Color Wheel"),
    Gobo            UMETA(DisplayName = "Gobo"),
    Zoom            UMETA(DisplayName = "Zoom"),
    Focus           UMETA(DisplayName = "Focus"),
    Shutter         UMETA(DisplayName = "Shutter"),
    Strobe          UMETA(DisplayName = "Strobe"),
    Frost           UMETA(DisplayName = "Frost"),
    Prism           UMETA(DisplayName = "Prism"),
    Control         UMETA(DisplayName = "Control/Mode"),
    Custom          UMETA(DisplayName = "Custom")
};

// ============================================================================
// DMX CONFIGURATION
// ============================================================================

/** Single DMX channel mapping */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipDMXChannel
{
    GENERATED_BODY()

    /** Channel offset from fixture base address (0-based) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|DMX")
    int32 ChannelOffset = 0;

    /** Channel type/function */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|DMX")
    ERshipDMXChannelType Type = ERshipDMXChannelType::Dimmer;

    /** Custom field name to read from fixture data (for Custom type) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|DMX", meta = (EditCondition = "Type == ERshipDMXChannelType::Custom"))
    FString CustomFieldName;

    /** Default value (0-255) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|DMX", meta = (ClampMin = "0", ClampMax = "255"))
    uint8 DefaultValue = 0;

    /** Whether to invert the value (255 - value) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|DMX")
    bool bInvert = false;

    /** Whether this is a 16-bit channel (combines with next channel for fine) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|DMX")
    bool b16Bit = false;
};

/** DMX fixture profile for channel layout */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipDMXProfile
{
    GENERATED_BODY()

    /** Profile name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|DMX")
    FString Name;

    /** Number of channels this profile uses */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|DMX")
    int32 ChannelCount = 1;

    /** Channel mappings */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|DMX")
    TArray<FRshipDMXChannel> Channels;
};

/** DMX output configuration for a single fixture */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipDMXFixtureOutput
{
    GENERATED_BODY()

    /** rship fixture ID to source data from */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|DMX")
    FString FixtureId;

    /** DMX universe (1-based) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|DMX", meta = (ClampMin = "1", ClampMax = "64000"))
    int32 Universe = 1;

    /** DMX start address (1-512) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|DMX", meta = (ClampMin = "1", ClampMax = "512"))
    int32 StartAddress = 1;

    /** Profile to use for channel mapping */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|DMX")
    FString ProfileName;

    /** Custom profile (if ProfileName is empty) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|DMX")
    FRshipDMXProfile CustomProfile;

    /** Whether this output is enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|DMX")
    bool bEnabled = true;

    /** Master dimmer scale (0-1) applied to all intensity channels */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|DMX", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MasterDimmer = 1.0f;
};

/** DMX universe buffer */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipDMXUniverseBuffer
{
    GENERATED_BODY()

    /** Universe number (1-based) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|DMX")
    int32 Universe = 1;

    /** 512 channel values */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|DMX")
    TArray<uint8> Channels;

    /** Whether this universe has changed since last send */
    bool bDirty = false;

    FRshipDMXUniverseBuffer()
    {
        Channels.SetNumZeroed(512);
    }
};

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDMXUniverseUpdated, int32, Universe, const TArray<uint8>&, Channels);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDMXOutputError, const FString&, ErrorMessage);

// ============================================================================
// DMX OUTPUT SERVICE
// ============================================================================

/**
 * Service for outputting rship fixture data to DMX universes.
 * Supports Art-Net and sACN protocols via platform implementations.
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipDMXOutput : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(URshipSubsystem* InSubsystem);
    void Shutdown();
    void Tick(float DeltaTime);

    // ========================================================================
    // OUTPUT CONFIGURATION
    // ========================================================================

    /** Add a fixture output mapping */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    void AddFixtureOutput(const FRshipDMXFixtureOutput& Output);

    /** Remove a fixture output by fixture ID */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    void RemoveFixtureOutput(const FString& FixtureId);

    /** Get all fixture outputs */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    TArray<FRshipDMXFixtureOutput> GetAllOutputs() const { return FixtureOutputs; }

    /** Clear all fixture outputs */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    void ClearAllOutputs();

    // ========================================================================
    // PROFILE MANAGEMENT
    // ========================================================================

    /** Register a DMX profile */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    void RegisterProfile(const FRshipDMXProfile& Profile);

    /** Get a profile by name */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    bool GetProfile(const FString& Name, FRshipDMXProfile& OutProfile) const;

    /** Get all registered profiles */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    TArray<FRshipDMXProfile> GetAllProfiles() const;

    /** Create common fixture profiles */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    void CreateDefaultProfiles();

    // ========================================================================
    // QUICK SETUP
    // ========================================================================

    /** Auto-map all rship fixtures to DMX, assigning sequential addresses */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    int32 AutoMapAllFixtures(int32 StartUniverse = 1, int32 StartAddress = 1, const FString& DefaultProfile = TEXT("Generic RGB"));

    /** Auto-map fixtures from a specific universe in rship to DMX */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    int32 AutoMapRshipUniverse(int32 RshipUniverse, int32 DMXUniverse);

    // ========================================================================
    // OUTPUT CONTROL
    // ========================================================================

    /** Enable/disable DMX output */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    void SetOutputEnabled(bool bEnabled);

    /** Is output enabled */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|DMX")
    bool IsOutputEnabled() const { return bOutputEnabled; }

    /** Convenience wrapper for IsOutputEnabled */
    bool IsEnabled() const { return IsOutputEnabled(); }

    /** Convenience wrapper for SetOutputEnabled */
    void SetEnabled(bool bEnabled) { SetOutputEnabled(bEnabled); }

    /** Set global master dimmer (affects all outputs) */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    void SetGlobalMaster(float Master);

    /** Get global master dimmer */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|DMX")
    float GetGlobalMaster() const { return GlobalMaster; }

    /** Convenience wrappers for master dimmer */
    void SetMasterDimmer(float Dimmer) { SetGlobalMaster(Dimmer); }
    float GetMasterDimmer() const { return GetGlobalMaster(); }

    /** Blackout all outputs (zero all channels) */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    void Blackout();

    /** Release blackout */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    void ReleaseBlackout();

    /** Is currently in blackout */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|DMX")
    bool IsBlackedOut() const { return bBlackout; }

    /** Convenience wrapper for IsBlackedOut */
    bool IsBlackout() const { return IsBlackedOut(); }

    // ========================================================================
    // DIRECT CHANNEL ACCESS
    // ========================================================================

    /** Set a single DMX channel value directly */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    void SetChannel(int32 Universe, int32 Channel, uint8 Value);

    /** Set multiple consecutive channels */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    void SetChannels(int32 Universe, int32 StartChannel, const TArray<uint8>& Values);

    /** Get a single channel value */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    uint8 GetChannel(int32 Universe, int32 Channel) const;

    /** Get all channels for a universe */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    TArray<uint8> GetUniverseChannels(int32 Universe) const;

    // ========================================================================
    // PROTOCOL SETTINGS
    // ========================================================================

    /** Set output frame rate (Hz) */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    void SetFrameRate(float Hz);

    /** Get output frame rate */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|DMX")
    float GetFrameRate() const { return FrameRate; }

    /** Set Art-Net destination IP */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    void SetArtNetDestination(const FString& IP);

    /** Set sACN multicast enable */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    void SetSACNMulticast(bool bEnable);

    /** Get current protocol */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|DMX")
    ERshipDMXProtocol GetProtocol() const { return CurrentProtocol; }

    /** Set DMX protocol */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    void SetProtocol(ERshipDMXProtocol Protocol) { CurrentProtocol = Protocol; }

    /** Set destination address (IP for Art-Net, or multicast settings for sACN) */
    UFUNCTION(BlueprintCallable, Category = "Rship|DMX")
    void SetDestinationAddress(const FString& IP) { ArtNetDestination = IP; }

    /** Get fixture count */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|DMX")
    int32 GetFixtureCount() const { return FixtureOutputs.Num(); }

    /** Get active universe count */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rship|DMX")
    int32 GetActiveUniverseCount() const { return UniverseBuffers.Num(); }

    // ========================================================================
    // EVENTS
    // ========================================================================

    UPROPERTY(BlueprintAssignable, Category = "Rship|DMX")
    FOnDMXUniverseUpdated OnUniverseUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Rship|DMX")
    FOnDMXOutputError OnOutputError;

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    UPROPERTY()
    URshipFixtureManager* FixtureManager;

    TArray<FRshipDMXFixtureOutput> FixtureOutputs;
    TMap<FString, FRshipDMXProfile> Profiles;
    TMap<int32, FRshipDMXUniverseBuffer> UniverseBuffers;

    bool bOutputEnabled = false;
    bool bBlackout = false;
    float GlobalMaster = 1.0f;
    float FrameRate = 44.0f;  // Standard DMX refresh rate
    double LastSendTime = 0.0;

    ERshipDMXProtocol CurrentProtocol = ERshipDMXProtocol::ArtNet;
    FString ArtNetDestination = TEXT("255.255.255.255");
    bool bSACNMulticast = true;

    void UpdateFixtureToBuffer(const FRshipDMXFixtureOutput& Output);
    void SendDirtyUniverses();
    uint8 MapChannelValue(const FRshipDMXChannel& Channel, float NormalizedValue);
    FRshipDMXUniverseBuffer& GetOrCreateBuffer(int32 Universe);

    // Protocol implementations (platform-specific)
    void SendArtNet(int32 Universe, const TArray<uint8>& Channels);
    void SendSACN(int32 Universe, const TArray<uint8>& Channels);
};
