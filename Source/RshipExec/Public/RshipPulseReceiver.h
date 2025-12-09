// Rship Pulse Receiver
// Receives and routes pulse data from rship server to fixture/camera actors

#pragma once

#include "CoreMinimal.h"
#include "RshipPulseReceiver.generated.h"

class URshipSubsystem;

// ============================================================================
// PULSE DATA STRUCTURES
// ============================================================================

/**
 * Parsed pulse data for a fixture
 * Semantic values, not raw DMX
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipFixturePulse
{
    GENERATED_BODY()

    /** The emitter ID this pulse came from */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    FString EmitterId;

    /** Timestamp of the pulse */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    double Timestamp = 0.0;

    // ========================================================================
    // INTENSITY
    // ========================================================================

    /** Master intensity (0.0 - 1.0) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    float Intensity = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    bool bHasIntensity = false;

    // ========================================================================
    // COLOR
    // ========================================================================

    /** RGB color */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    FLinearColor Color = FLinearColor::White;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    bool bHasColor = false;

    /** Color temperature in Kelvin (if fixture supports it) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    float ColorTemperature = 5600.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    bool bHasColorTemperature = false;

    // ========================================================================
    // BEAM CONTROL
    // ========================================================================

    /** Zoom/beam angle (0.0 = narrow, 1.0 = wide) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    float Zoom = 0.5f;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    bool bHasZoom = false;

    /** Focus (0.0 - 1.0) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    float Focus = 0.5f;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    bool bHasFocus = false;

    /** Iris (0.0 = closed, 1.0 = open) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    float Iris = 1.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    bool bHasIris = false;

    // ========================================================================
    // POSITION (for moving heads)
    // ========================================================================

    /** Pan angle in degrees */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    float Pan = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    bool bHasPan = false;

    /** Tilt angle in degrees */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    float Tilt = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    bool bHasTilt = false;

    // ========================================================================
    // EFFECTS
    // ========================================================================

    /** Strobe rate (0.0 = off, 1.0 = max speed) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    float Strobe = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    bool bHasStrobe = false;

    /** Gobo selection (index or normalized) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    float Gobo = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    bool bHasGobo = false;

    /** Gobo rotation speed */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    float GoboRotation = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    bool bHasGoboRotation = false;

    /** Prism enabled */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    bool bPrism = false;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Pulse")
    bool bHasPrism = false;

    // ========================================================================
    // RAW DATA ACCESS
    // ========================================================================

    /** Raw JSON data for custom fields */
    TSharedPtr<FJsonObject> RawData;

    /** Get a custom float value from raw data */
    bool GetCustomFloat(const FString& Key, float& OutValue) const;

    /** Get a custom string value from raw data */
    bool GetCustomString(const FString& Key, FString& OutValue) const;
};

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFixturePulseReceived, const FString&, FixtureId, const FRshipFixturePulse&, Pulse);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEmitterPulseReceived, const FString&, EmitterId, const TSharedPtr<FJsonObject>&, Data);

// ============================================================================
// PULSE RECEIVER SERVICE
// ============================================================================

/**
 * Receives pulse data from rship and routes it to fixture actors.
 * Maintains subscriptions and provides efficient lookup for pulse routing.
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipPulseReceiver : public UObject
{
    GENERATED_BODY()

public:
    /** Initialize with subsystem reference */
    void Initialize(URshipSubsystem* InSubsystem);

    /** Shutdown and cleanup */
    void Shutdown();

    // ========================================================================
    // SUBSCRIPTION MANAGEMENT
    // ========================================================================

    /**
     * Subscribe to pulses for a specific fixture
     * @param FixtureId The fixture to subscribe to
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Pulse")
    void SubscribeToFixture(const FString& FixtureId);

    /**
     * Subscribe to pulses for all fixtures
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Pulse")
    void SubscribeToAllFixtures();

    /**
     * Unsubscribe from a specific fixture
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Pulse")
    void UnsubscribeFromFixture(const FString& FixtureId);

    /**
     * Unsubscribe from all fixtures
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Pulse")
    void UnsubscribeFromAll();

    /**
     * Check if subscribed to a fixture
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Pulse")
    bool IsSubscribedToFixture(const FString& FixtureId) const;

    // ========================================================================
    // PULSE QUERIES
    // ========================================================================

    /**
     * Get the last received pulse for a fixture
     * @param FixtureId The fixture ID
     * @param OutPulse The pulse data
     * @return Whether a pulse was found
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Pulse")
    bool GetLastPulse(const FString& FixtureId, FRshipFixturePulse& OutPulse) const;

    /**
     * Get pulses per second for a fixture (for diagnostics)
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Pulse")
    float GetPulseRate(const FString& FixtureId) const;

    /**
     * Get total pulses received per second
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Pulse")
    float GetTotalPulseRate() const;

    // ========================================================================
    // EVENTS
    // ========================================================================

    /** Fired when a fixture pulse is received */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Pulse")
    FOnFixturePulseReceived OnFixturePulseReceived;

    /** Fired when any emitter pulse is received (raw) */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Pulse")
    FOnEmitterPulseReceived OnEmitterPulseReceived;

    // ========================================================================
    // INTERNAL - Called by subsystem
    // ========================================================================

    /**
     * Process an incoming pulse event from the WebSocket
     * Called by URshipSubsystem when a pulse message is received
     */
    void ProcessPulseEvent(const FString& EmitterId, const TSharedPtr<FJsonObject>& Data);

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    // Fixture ID to Emitter ID mapping
    TMap<FString, FString> FixtureToEmitter;

    // Emitter ID to Fixture ID mapping (reverse lookup)
    TMap<FString, FString> EmitterToFixture;

    // Active subscriptions
    TSet<FString> SubscribedFixtures;
    bool bSubscribedToAll = false;

    // Last received pulse per fixture
    TMap<FString, FRshipFixturePulse> LastPulses;

    // Pulse rate tracking
    struct FPulseRateTracker
    {
        TArray<double> RecentTimestamps;
        float CachedRate = 0.0f;
        double LastRateCalcTime = 0.0;
    };
    TMap<FString, FPulseRateTracker> PulseRates;
    int32 TotalPulsesLastSecond = 0;
    double LastTotalRateCalcTime = 0.0;

    // Parse pulse data from JSON
    FRshipFixturePulse ParsePulse(const FString& EmitterId, const TSharedPtr<FJsonObject>& Data) const;

    // Update pulse rate tracking
    void UpdatePulseRate(const FString& FixtureId);

    // Build fixture <-> emitter mappings from FixtureManager
    void RebuildFixtureEmitterMappings();
};
