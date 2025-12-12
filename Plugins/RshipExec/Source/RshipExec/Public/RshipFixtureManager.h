// Rship Fixture Manager
// Manages fixture entities, fixture types, and calibration data from rship server

#pragma once

#include "CoreMinimal.h"
#include "RshipCalibrationTypes.h"
#include "RshipFixtureManager.generated.h"

class URshipSubsystem;

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFixturesUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFixtureAdded, const FRshipFixtureInfo&, Fixture);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFixtureRemoved, const FString&, FixtureId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFixtureTypeAdded, const FRshipFixtureTypeInfo&, FixtureType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCalibrationUpdated, const FRshipFixtureCalibration&, Calibration);

/**
 * Manages fixture entities, fixture types, and calibration profiles.
 * Subscribes to server-side entities and provides O(1) lookups.
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipFixtureManager : public UObject
{
    GENERATED_BODY()

public:
    /**
     * Initialize the manager with reference to the subsystem
     */
    void Initialize(URshipSubsystem* InSubsystem);

    /**
     * Cleanup on shutdown
     */
    void Shutdown();

    // ========================================================================
    // FIXTURE QUERIES
    // ========================================================================

    /** Get all fixtures in the current project */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixtures")
    TArray<FRshipFixtureInfo> GetAllFixtures() const;

    /** Get fixture by ID */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixtures")
    bool GetFixtureById(const FString& FixtureId, FRshipFixtureInfo& OutFixture) const;

    /** Get all fixtures of a specific type */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixtures")
    TArray<FRshipFixtureInfo> GetFixturesByType(const FString& FixtureTypeId) const;

    /** Get fixture count */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixtures")
    int32 GetFixtureCount() const { return Fixtures.Num(); }

    // ========================================================================
    // FIXTURE TYPE QUERIES
    // ========================================================================

    /** Get all fixture types */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixtures")
    TArray<FRshipFixtureTypeInfo> GetAllFixtureTypes() const;

    /** Get fixture type by ID */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixtures")
    bool GetFixtureTypeById(const FString& FixtureTypeId, FRshipFixtureTypeInfo& OutFixtureType) const;

    /** Get fixture type for a fixture */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixtures")
    bool GetFixtureTypeForFixture(const FString& FixtureId, FRshipFixtureTypeInfo& OutFixtureType) const;

    // ========================================================================
    // CALIBRATION QUERIES
    // ========================================================================

    /** Get all fixture calibrations */
    UFUNCTION(BlueprintCallable, Category = "Rship|Calibration")
    TArray<FRshipFixtureCalibration> GetAllCalibrations() const;

    /** Get calibration by ID */
    UFUNCTION(BlueprintCallable, Category = "Rship|Calibration")
    bool GetCalibrationById(const FString& CalibrationId, FRshipFixtureCalibration& OutCalibration) const;

    /** Get calibration for a fixture type */
    UFUNCTION(BlueprintCallable, Category = "Rship|Calibration")
    bool GetCalibrationForFixtureType(const FString& FixtureTypeId, FRshipFixtureCalibration& OutCalibration) const;

    /**
     * Get the effective calibration for a specific fixture
     * Returns per-fixture override if set, otherwise fixture type calibration
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Calibration")
    bool GetCalibrationForFixture(const FString& FixtureId, FRshipFixtureCalibration& OutCalibration) const;

    /** Get calibrations for a fixture type (may have multiple) */
    UFUNCTION(BlueprintCallable, Category = "Rship|Calibration")
    TArray<FRshipFixtureCalibration> GetCalibrationsForFixtureType(const FString& FixtureTypeId) const;

    // ========================================================================
    // CALIBRATION HELPERS
    // ========================================================================

    /**
     * Convert DMX value to output intensity for a fixture
     * Uses calibration if available, otherwise returns linear mapping
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Calibration")
    float DmxToOutputForFixture(const FString& FixtureId, int32 DmxValue) const;

    /**
     * Get color correction for a fixture at a target color temperature
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Calibration")
    FLinearColor GetColorCorrectionForFixture(const FString& FixtureId, float TargetKelvin) const;

    /**
     * Get calibrated beam angle for a fixture
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Calibration")
    float GetCalibratedBeamAngleForFixture(const FString& FixtureId) const;

    /**
     * Get calibrated field angle for a fixture
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Calibration")
    float GetCalibratedFieldAngleForFixture(const FString& FixtureId) const;

    /**
     * Get falloff exponent for a fixture
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Calibration")
    float GetFalloffExponentForFixture(const FString& FixtureId) const;

    // ========================================================================
    // LOCAL REGISTRATION (for scene conversion)
    // ========================================================================

    /**
     * Register a locally-created fixture with the server
     * Used by scene converter to push UE lights as rship fixtures
     * @param FixtureInfo The fixture info to register
     * @return Whether the registration was sent successfully
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixtures")
    bool RegisterLocalFixture(const FRshipFixtureInfo& FixtureInfo);

    /**
     * Update a fixture's position on the server
     * Used to sync UE editor changes back to rship
     * @param FixtureId The fixture to update
     * @param Position New position (in rship units - meters)
     * @param Rotation New rotation
     * @return Whether the update was sent successfully
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixtures")
    bool UpdateFixturePosition(const FString& FixtureId, const FVector& Position, const FRotator& Rotation);

    /**
     * Remove a locally-registered fixture from the server
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Fixtures")
    bool UnregisterFixture(const FString& FixtureId);

    // ========================================================================
    // EVENTS
    // ========================================================================

    /** Fired when fixture list changes (any add/remove/update) */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Fixtures")
    FOnFixturesUpdated OnFixturesUpdated;

    /** Fired when a specific fixture is added */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Fixtures")
    FOnFixtureAdded OnFixtureAdded;

    /** Fired when a specific fixture is removed */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Fixtures")
    FOnFixtureRemoved OnFixtureRemoved;

    /** Fired when a fixture type is added */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Fixtures")
    FOnFixtureTypeAdded OnFixtureTypeAdded;

    /** Fired when calibration data is updated */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Calibration")
    FOnCalibrationUpdated OnCalibrationUpdated;

    // ========================================================================
    // ENTITY PROCESSING (called by subsystem)
    // ========================================================================

    /**
     * Process incoming Fixture entity event
     */
    void ProcessFixtureEvent(const TSharedPtr<FJsonObject>& Data, bool bIsDelete);

    /**
     * Process incoming FixtureType entity event
     */
    void ProcessFixtureTypeEvent(const TSharedPtr<FJsonObject>& Data, bool bIsDelete);

    /**
     * Process incoming FixtureCalibration entity event
     */
    void ProcessCalibrationEvent(const TSharedPtr<FJsonObject>& Data, bool bIsDelete);

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    // Fixtures by ID
    TMap<FString, FRshipFixtureInfo> Fixtures;

    // Fixture types by ID
    TMap<FString, FRshipFixtureTypeInfo> FixtureTypes;

    // Calibrations by ID
    TMap<FString, FRshipFixtureCalibration> Calibrations;

    // Calibrations indexed by fixture type ID (for fast lookup)
    TMultiMap<FString, FString> CalibrationsByFixtureType;

    // Parse fixture info from JSON
    FRshipFixtureInfo ParseFixture(const TSharedPtr<FJsonObject>& Data) const;

    // Parse fixture type info from JSON
    FRshipFixtureTypeInfo ParseFixtureType(const TSharedPtr<FJsonObject>& Data) const;

    // Parse calibration from JSON
    FRshipFixtureCalibration ParseCalibration(const TSharedPtr<FJsonObject>& Data) const;
};
