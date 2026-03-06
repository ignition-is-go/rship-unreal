// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "RshipTestUtilities.generated.h"

class URshipSubsystem;
class URshipActorRegistrationComponent;

/**
 * Validation issue severity levels
 */
UENUM(BlueprintType)
enum class ERshipTestSeverity : uint8
{
	Info		UMETA(DisplayName = "Info"),
	Warning		UMETA(DisplayName = "Warning"),
	Error		UMETA(DisplayName = "Error")
};

/**
 * Test issue detected during setup checks
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipTestIssue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Rship|Testing")
	ERshipTestSeverity Severity = ERshipTestSeverity::Info;

	UPROPERTY(BlueprintReadOnly, Category = "Rship|Testing")
	FString Category;

	UPROPERTY(BlueprintReadOnly, Category = "Rship|Testing")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "Rship|Testing")
	FString Details;

	UPROPERTY(BlueprintReadOnly, Category = "Rship|Testing")
	FString SuggestedFix;

	UPROPERTY(BlueprintReadOnly, Category = "Rship|Testing")
	FString EntityPath; // Path to affected actor/component

	FRshipTestIssue() = default;

	FRshipTestIssue(ERshipTestSeverity InSeverity, const FString& InCategory, const FString& InMessage)
		: Severity(InSeverity)
		, Category(InCategory)
		, Message(InMessage)
	{}
};

/**
 * Stress test configuration
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipStressTestConfig
{
	GENERATED_BODY()

	/** Number of pulses to send per second */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Testing", meta = (ClampMin = "1", ClampMax = "10000"))
	int32 PulsesPerSecond = 100;

	/** How long to run the stress test in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Testing", meta = (ClampMin = "1", ClampMax = "300"))
	float DurationSeconds = 10.0f;

	/** Target ID pattern to send pulses to (empty = all targets) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Testing")
	FString TargetIdPattern;

	/** Emitter ID to pulse (empty = random) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Testing")
	FString EmitterId;

	/** Whether to vary pulse values randomly */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Testing")
	bool bRandomizeValues = true;
};

/**
 * Stress test results
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipStressTestResults
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Rship|Testing")
	int32 TotalPulsesSent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Rship|Testing")
	int32 PulsesDropped = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Rship|Testing")
	float ActualDuration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Rship|Testing")
	float AverageLatencyMs = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Rship|Testing")
	float MaxLatencyMs = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Rship|Testing")
	float EffectivePulsesPerSecond = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Rship|Testing")
	bool bCompleted = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStressTestCompleted, const FRshipStressTestResults&, Results);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStressTestProgress, float, Progress, int32, PulsesSent);

/**
 * Test utilities for validating rship setups and testing without a server connection
 *
 * Features:
 * - Mock pulse injection for offline testing
 * - Setup validation to detect configuration issues
 * - Stress testing to measure performance under load
 * - Connection simulation for resilience testing
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipTestUtilities : public UObject
{
	GENERATED_BODY()

public:
	URshipTestUtilities();

	// ========================================================================
	// MOCK PULSE INJECTION
	// ========================================================================

	/**
	 * Inject a mock pulse to a target without going through the server.
	 * Useful for testing target responses offline.
	 *
	 * @param TargetId The target to send the pulse to
	 * @param EmitterId The emitter ID to pulse
	 * @param JsonData JSON string of the pulse data (e.g., {"value": 0.5})
	 * @return True if the pulse was injected successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Testing")
	bool InjectMockPulse(const FString& TargetId, const FString& EmitterId, const FString& JsonData);

	/**
	 * Inject a mock pulse with typed data.
	 *
	 * @param TargetId The target to send the pulse to
	 * @param EmitterId The emitter ID to pulse
	 * @param Value Float value to send (creates {"value": X})
	 * @return True if the pulse was injected successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Testing")
	bool InjectMockPulseFloat(const FString& TargetId, const FString& EmitterId, float Value);

	/**
	 * Inject a mock color pulse.
	 *
	 * @param TargetId The target to send the pulse to
	 * @param EmitterId The emitter ID to pulse
	 * @param Color Linear color to send
	 * @return True if the pulse was injected successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Testing")
	bool InjectMockPulseColor(const FString& TargetId, const FString& EmitterId, FLinearColor Color);

	/**
	 * Inject a mock transform pulse.
	 *
	 * @param TargetId The target to send the pulse to
	 * @param EmitterId The emitter ID to pulse
	 * @param Transform Transform to send
	 * @return True if the pulse was injected successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Testing")
	bool InjectMockPulseTransform(const FString& TargetId, const FString& EmitterId, const FTransform& Transform);

	/**
	 * Inject random pulses to all registered targets.
	 *
	 * @return Number of pulses injected
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Testing")
	int32 InjectRandomPulsesToAllTargets();

	// ========================================================================
	// SETUP VALIDATION
	// ========================================================================

	/**
	 * Validate all rship setup in the current world.
	 *
	 * @return Array of test issues found
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Testing")
	TArray<FRshipTestIssue> ValidateAll();

	/**
	 * Validate target component configurations.
	 *
	 * @return Array of test issues for targets
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Testing")
	TArray<FRshipTestIssue> ValidateTargets();

	/**
	 * Validate material bindings.
	 *
	 * @return Array of test issues for materials
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Testing")
	TArray<FRshipTestIssue> ValidateMaterialBindings();

	/**
	 * Validate LiveLink subject mappings.
	 *
	 * @return Array of test issues for LiveLink
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Testing")
	TArray<FRshipTestIssue> ValidateLiveLinkSetup();

	/**
	 * Validate timecode configuration.
	 *
	 * @return Array of test issues for timecode
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Testing")
	TArray<FRshipTestIssue> ValidateTimecodeSetup();

	/**
	 * Validate a specific target component.
	 *
	 * @param Target The target component to validate
	 * @return Array of test issues for this target
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Testing")
	TArray<FRshipTestIssue> ValidateTargetComponent(URshipActorRegistrationComponent* Target);

	// ========================================================================
	// STRESS TESTING
	// ========================================================================

	/**
	 * Start a stress test with the given configuration.
	 *
	 * @param Config Stress test configuration
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Testing")
	void StartStressTest(const FRshipStressTestConfig& Config);

	/**
	 * Stop the currently running stress test.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Testing")
	void StopStressTest();

	/**
	 * Check if a stress test is currently running.
	 */
	UFUNCTION(BlueprintPure, Category = "Rship|Testing")
	bool IsStressTestRunning() const { return bStressTestRunning; }

	/**
	 * Get the current stress test progress (0-1).
	 */
	UFUNCTION(BlueprintPure, Category = "Rship|Testing")
	float GetStressTestProgress() const;

	/**
	 * Get the current stress test results (may be incomplete if still running).
	 */
	UFUNCTION(BlueprintPure, Category = "Rship|Testing")
	FRshipStressTestResults GetStressTestResults() const { return StressTestResults; }

	/** Fired when a stress test completes */
	UPROPERTY(BlueprintAssignable, Category = "Rship|Testing")
	FOnStressTestCompleted OnStressTestCompleted;

	/** Fired periodically during stress test with progress */
	UPROPERTY(BlueprintAssignable, Category = "Rship|Testing")
	FOnStressTestProgress OnStressTestProgress;

	// ========================================================================
	// CONNECTION SIMULATION
	// ========================================================================

	/**
	 * Simulate a server disconnect.
	 * The subsystem will enter reconnecting state.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Testing")
	void SimulateDisconnect();

	/**
	 * Trigger a reconnection after simulated disconnect.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Testing")
	void SimulateReconnect();

	/**
	 * Simulate network latency by adding delay to outgoing messages.
	 *
	 * @param LatencyMs Latency to add in milliseconds (0 to disable)
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Testing")
	void SetSimulatedLatency(float LatencyMs);

	/**
	 * Get the currently simulated latency.
	 */
	UFUNCTION(BlueprintPure, Category = "Rship|Testing")
	float GetSimulatedLatency() const { return SimulatedLatencyMs; }

	/**
	 * Check if we're currently simulating a disconnect.
	 */
	UFUNCTION(BlueprintPure, Category = "Rship|Testing")
	bool IsSimulatingDisconnect() const { return bSimulatingDisconnect; }

	/**
	 * Reset all connection simulations back to normal.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|Testing")
	void ResetConnectionSimulation();

	// ========================================================================
	// INTERNAL - Called by tick
	// ========================================================================

	/** Called every frame to update stress test */
	void Tick(float DeltaTime);

private:
	// Helper to get subsystem
	URshipSubsystem* GetSubsystem() const;

	// Helper to find target components in world
	TArray<URshipActorRegistrationComponent*> GetAllTargetComponents() const;

	// Helper to find target by ID
	URshipActorRegistrationComponent* FindTargetById(const FString& TargetId) const;

	// Inject pulse to a specific target component
	bool InjectPulseToTarget(URshipActorRegistrationComponent* Target, const FString& EmitterId, TSharedPtr<FJsonObject> Data);

	// Stress test state
	bool bStressTestRunning = false;
	FRshipStressTestConfig StressTestConfig;
	FRshipStressTestResults StressTestResults;
	float StressTestElapsed = 0.0f;
	float AccumulatedPulseTime = 0.0f;
	TArray<FString> StressTestTargetIds;

	// Connection simulation state
	bool bSimulatingDisconnect = false;
	float SimulatedLatencyMs = 0.0f;

	// Cached world reference
	TWeakObjectPtr<UWorld> CachedWorld;
};

