// Rocketship Health Monitor
// Provides at-a-glance status monitoring for artists in "Operate Mode"

#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "RshipHealthMonitor.generated.h"

// Forward declarations
class URshipSubsystem;
class URshipTargetComponent;

/**
 * Aggregated health status for the rship connection and targets
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipHealthStatus
{
	GENERATED_BODY()

	// ========================================================================
	// CONNECTION STATUS
	// ========================================================================

	/** Whether the WebSocket is connected */
	UPROPERTY(BlueprintReadOnly, Category = "Connection")
	bool bIsConnected = false;

	/** Estimated latency to server in milliseconds */
	UPROPERTY(BlueprintReadOnly, Category = "Connection")
	float ConnectionLatencyMs = 0.0f;

	/** Number of reconnection attempts since last successful connect */
	UPROPERTY(BlueprintReadOnly, Category = "Connection")
	int32 ReconnectAttempts = 0;

	/** Time since last successful connection */
	UPROPERTY(BlueprintReadOnly, Category = "Connection")
	float SecondsSinceConnect = 0.0f;

	// ========================================================================
	// TARGET STATUS
	// ========================================================================

	/** Total number of registered targets */
	UPROPERTY(BlueprintReadOnly, Category = "Targets")
	int32 TotalTargets = 0;

	/** Targets that have sent a pulse in the last few seconds */
	UPROPERTY(BlueprintReadOnly, Category = "Targets")
	int32 ActiveTargets = 0;

	/** Targets that haven't pulsed recently (may be stale) */
	UPROPERTY(BlueprintReadOnly, Category = "Targets")
	int32 InactiveTargets = 0;

	/** Targets that failed to register or have errors */
	UPROPERTY(BlueprintReadOnly, Category = "Targets")
	int32 ErrorTargets = 0;

	// ========================================================================
	// THROUGHPUT METRICS
	// ========================================================================

	/** Emitter pulses sent per second */
	UPROPERTY(BlueprintReadOnly, Category = "Throughput")
	int32 PulsesPerSecond = 0;

	/** Messages sent per second (after batching) */
	UPROPERTY(BlueprintReadOnly, Category = "Throughput")
	int32 MessagesPerSecond = 0;

	/** Bytes sent per second */
	UPROPERTY(BlueprintReadOnly, Category = "Throughput")
	int32 BytesPerSecond = 0;

	// ========================================================================
	// QUEUE STATUS
	// ========================================================================

	/** Number of messages currently in queue */
	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 QueueLength = 0;

	/** Queue fullness as a percentage (0.0 - 1.0) */
	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	float QueuePressure = 0.0f;

	/** Total messages dropped since startup */
	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 MessagesDropped = 0;

	/** Messages dropped in the last second */
	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 MessagesDroppedLastSecond = 0;

	// ========================================================================
	// RATE LIMITING STATUS
	// ========================================================================

	/** Whether the rate limiter is currently backing off */
	UPROPERTY(BlueprintReadOnly, Category = "RateLimiting")
	bool bIsBackingOff = false;

	/** Seconds remaining in backoff period */
	UPROPERTY(BlueprintReadOnly, Category = "RateLimiting")
	float BackoffRemaining = 0.0f;

	/** Current effective rate limit (may be reduced by adaptive rate control) */
	UPROPERTY(BlueprintReadOnly, Category = "RateLimiting")
	float CurrentRateLimit = 0.0f;

	/** Percentage of configured max rate (for adaptive rate display) */
	UPROPERTY(BlueprintReadOnly, Category = "RateLimiting")
	float RateLimitPercentage = 100.0f;

	// ========================================================================
	// OVERALL HEALTH
	// ========================================================================

	/** Overall health score (0-100, higher is better) */
	UPROPERTY(BlueprintReadOnly, Category = "Health")
	int32 HealthScore = 100;

	/** Human-readable status summary */
	UPROPERTY(BlueprintReadOnly, Category = "Health")
	FString StatusSummary = TEXT("Initializing...");

	/** Timestamp when this status was captured */
	UPROPERTY(BlueprintReadOnly, Category = "Health")
	FDateTime CapturedAt;

	FRshipHealthStatus()
		: CapturedAt(FDateTime::Now())
	{
	}
};

/**
 * Information about a single target's activity
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipTargetActivity
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Activity")
	FString TargetId;

	UPROPERTY(BlueprintReadOnly, Category = "Activity")
	FString TargetName;

	UPROPERTY(BlueprintReadOnly, Category = "Activity")
	int32 PulsesPerSecond = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Activity")
	float SecondsSinceLastPulse = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Activity")
	bool bIsActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Activity")
	bool bHasError = false;

	UPROPERTY(BlueprintReadOnly, Category = "Activity")
	FString ErrorMessage;
};

/**
 * Delegate types for health events
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRshipConnectionLost);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRshipConnectionRestored);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRshipBackpressureWarning, float, QueuePressure);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRshipHealthChanged, const FRshipHealthStatus&, NewStatus);

/**
 * Monitors rship connection and target health, providing aggregated status for dashboards.
 * Access via URshipSubsystem::GetHealthMonitor()
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipHealthMonitor : public UObject
{
	GENERATED_BODY()

public:
	URshipHealthMonitor();

	/** Initialize the health monitor with a reference to the subsystem */
	void Initialize(URshipSubsystem* InSubsystem);

	/** Shutdown and cleanup */
	void Shutdown();

	// ========================================================================
	// HEALTH QUERIES
	// ========================================================================

	/** Get the current aggregated health status */
	UFUNCTION(BlueprintCallable, Category = "Rship|Health")
	FRshipHealthStatus GetCurrentHealth();

	/** Get the N most active targets (highest pulse rate) */
	UFUNCTION(BlueprintCallable, Category = "Rship|Health")
	TArray<FRshipTargetActivity> GetHotTargets(int32 TopN = 10);

	/** Get targets that haven't pulsed in the given time */
	UFUNCTION(BlueprintCallable, Category = "Rship|Health")
	TArray<FRshipTargetActivity> GetInactiveTargets(float InactiveThresholdSeconds = 5.0f);

	/** Get targets with errors */
	UFUNCTION(BlueprintCallable, Category = "Rship|Health")
	TArray<FRshipTargetActivity> GetErrorTargets();

	/** Get activity info for a specific target */
	UFUNCTION(BlueprintCallable, Category = "Rship|Health")
	FRshipTargetActivity GetTargetActivity(const FString& TargetId);

	// ========================================================================
	// HEALTH ACTIONS
	// ========================================================================

	/** Force reconnection to the server */
	UFUNCTION(BlueprintCallable, Category = "Rship|Health")
	void ReconnectAll();

	/** Re-register all targets with the server */
	UFUNCTION(BlueprintCallable, Category = "Rship|Health")
	void ReregisterAll();

	/** Reset all statistics and error states */
	UFUNCTION(BlueprintCallable, Category = "Rship|Health")
	void ResetStatistics();

	/** Clear error state for a specific target */
	UFUNCTION(BlueprintCallable, Category = "Rship|Health")
	void ClearTargetError(const FString& TargetId);

	// ========================================================================
	// CONFIGURATION
	// ========================================================================

	/** How often to update health data (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	float UpdateInterval = 0.5f;

	/** Threshold for considering a target inactive (seconds since last pulse) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	float InactiveThreshold = 5.0f;

	/** Queue pressure threshold for warning (0.0-1.0) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	float BackpressureWarningThreshold = 0.7f;

	// ========================================================================
	// EVENTS
	// ========================================================================

	/** Fired when connection to server is lost */
	UPROPERTY(BlueprintAssignable, Category = "Rship|Health|Events")
	FOnRshipConnectionLost OnConnectionLost;

	/** Fired when connection to server is restored */
	UPROPERTY(BlueprintAssignable, Category = "Rship|Health|Events")
	FOnRshipConnectionRestored OnConnectionRestored;

	/** Fired when queue pressure exceeds warning threshold */
	UPROPERTY(BlueprintAssignable, Category = "Rship|Health|Events")
	FOnRshipBackpressureWarning OnBackpressureWarning;

	/** Fired when health status changes significantly */
	UPROPERTY(BlueprintAssignable, Category = "Rship|Health|Events")
	FOnRshipHealthChanged OnHealthChanged;

	// ========================================================================
	// PULSE TRACKING (called internally by subsystem)
	// ========================================================================

	/** Record a pulse from a target (called by subsystem when pulse is sent) */
	void RecordPulse(const FString& TargetId);

	/** Record an error for a target */
	void RecordError(const FString& TargetId, const FString& ErrorMessage);

private:
	/** Update health data (called on timer) */
	void UpdateHealthData();

	/** Calculate overall health score */
	int32 CalculateHealthScore(const FRshipHealthStatus& Status);

	/** Generate human-readable status summary */
	FString GenerateStatusSummary(const FRshipHealthStatus& Status);

	/** Check for and fire health events */
	void CheckAndFireEvents(const FRshipHealthStatus& NewStatus);

	/** Reference to the subsystem */
	UPROPERTY()
	URshipSubsystem* Subsystem;

	/** Timer for periodic updates */
	FTimerHandle UpdateTimerHandle;

	/** Last captured health status */
	FRshipHealthStatus LastHealth;

	/** Was connected in the previous update? (for detecting connection changes) */
	bool bWasConnected = false;

	/** Was backpressure warning active? */
	bool bWasBackpressureWarning = false;

	// ========================================================================
	// ACTIVITY TRACKING
	// ========================================================================

	/** Per-target pulse tracking */
	struct FTargetPulseInfo
	{
		FDateTime LastPulseTime;
		int32 PulseCountThisSecond = 0;
		int32 PulseCountLastSecond = 0;
		FDateTime LastSecondStart;
		bool bHasError = false;
		FString ErrorMessage;
	};

	TMap<FString, FTargetPulseInfo> TargetPulseInfo;

	/** Track when we last rolled over the per-second counters */
	FDateTime LastSecondRollover;

	/** Roll over per-second pulse counters */
	void RolloverPulseCounters();

	/** Total pulses in the last second (across all targets) */
	int32 TotalPulsesLastSecond = 0;

	/** Previous messages dropped count (for calculating per-second) */
	int32 PreviousMessagesDropped = 0;
};
