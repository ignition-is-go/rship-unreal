// Rocketship Health Monitor Implementation

#include "RshipHealthMonitor.h"
#include "RshipSubsystem.h"
#include "RshipActorRegistrationComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

URshipHealthMonitor::URshipHealthMonitor()
	: LastSecondRollover(FDateTime::Now())
{
}

void URshipHealthMonitor::Initialize(URshipSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;

	if (!Subsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("RshipHealthMonitor: Cannot initialize without subsystem"));
		return;
	}

	// Start periodic update timer
	if (UWorld* World = Subsystem->GetWorld())
	{
		World->GetTimerManager().SetTimer(
			UpdateTimerHandle,
			this,
			&URshipHealthMonitor::UpdateHealthData,
			UpdateInterval,
			true  // Looping
		);
	}

	UE_LOG(LogTemp, Log, TEXT("RshipHealthMonitor: Initialized with %.1fs update interval"), UpdateInterval);
}

void URshipHealthMonitor::Shutdown()
{
	if (Subsystem)
	{
		if (UWorld* World = Subsystem->GetWorld())
		{
			World->GetTimerManager().ClearTimer(UpdateTimerHandle);
		}
	}

	TargetPulseInfo.Empty();
	Subsystem = nullptr;

	UE_LOG(LogTemp, Log, TEXT("RshipHealthMonitor: Shutdown"));
}

// ============================================================================
// HEALTH QUERIES
// ============================================================================

FRshipHealthStatus URshipHealthMonitor::GetCurrentHealth()
{
	FRshipHealthStatus Status;

	if (!Subsystem)
	{
		Status.StatusSummary = TEXT("Not initialized");
		return Status;
	}

	// Connection status
	Status.bIsConnected = Subsystem->IsConnected();
	Status.ReconnectAttempts = 0; // Would need to expose from subsystem

	// Target counts
	if (Subsystem->TargetComponents)
	{
		Status.TotalTargets = Subsystem->TargetComponents->Num();

		int32 ActiveCount = 0;
		int32 ErrorCount = 0;
		double Now = FPlatformTime::Seconds();

		for (auto& Pair : *Subsystem->TargetComponents)
		{
			URshipActorRegistrationComponent* Comp = Pair.Value;
			if (!Comp) continue;

			FTargetPulseInfo* Info = TargetPulseInfo.Find(Comp->targetName);
			if (Info)
			{
				// Check if active (pulsed recently)
				float SecondsSinceLastPulse = (FDateTime::Now() - Info->LastPulseTime).GetTotalSeconds();
				if (SecondsSinceLastPulse < InactiveThreshold)
				{
					ActiveCount++;
				}

				if (Info->bHasError)
				{
					ErrorCount++;
				}
			}
		}

		Status.ActiveTargets = ActiveCount;
		Status.InactiveTargets = Status.TotalTargets - ActiveCount;
		Status.ErrorTargets = ErrorCount;
	}

	// Throughput metrics
	RolloverPulseCounters();
	Status.PulsesPerSecond = TotalPulsesLastSecond;
	Status.MessagesPerSecond = Subsystem->GetMessagesSentPerSecond();
	Status.BytesPerSecond = Subsystem->GetBytesSentPerSecond();

	// Queue status
	Status.QueueLength = Subsystem->GetQueueLength();
	Status.QueuePressure = Subsystem->GetQueuePressure();
	Status.MessagesDropped = Subsystem->GetMessagesDropped();
	Status.MessagesDroppedLastSecond = Status.MessagesDropped - PreviousMessagesDropped;
	PreviousMessagesDropped = Status.MessagesDropped;

	// Rate limiting status
	Status.bIsBackingOff = Subsystem->IsRateLimiterBackingOff();
	Status.BackoffRemaining = Subsystem->GetBackoffRemaining();
	Status.CurrentRateLimit = Subsystem->GetCurrentRateLimit();

	// Calculate rate limit percentage (assuming max is in settings, use 100 as default)
	// This would need to be exposed from the rate limiter config
	float MaxRate = 100.0f; // Default assumption
	if (Status.CurrentRateLimit > 0)
	{
		Status.RateLimitPercentage = FMath::Clamp((Status.CurrentRateLimit / MaxRate) * 100.0f, 0.0f, 100.0f);
	}

	// Calculate health score and summary
	Status.HealthScore = CalculateHealthScore(Status);
	Status.StatusSummary = GenerateStatusSummary(Status);
	Status.CapturedAt = FDateTime::Now();

	return Status;
}

TArray<FRshipTargetActivity> URshipHealthMonitor::GetHotTargets(int32 TopN)
{
	TArray<FRshipTargetActivity> Activities;

	for (const auto& Pair : TargetPulseInfo)
	{
		FRshipTargetActivity Activity;
		Activity.TargetId = Pair.Key;
		Activity.TargetName = Pair.Key; // Could be enhanced with display name
		Activity.PulsesPerSecond = Pair.Value.PulseCountLastSecond;
		Activity.SecondsSinceLastPulse = (FDateTime::Now() - Pair.Value.LastPulseTime).GetTotalSeconds();
		Activity.bIsActive = Activity.SecondsSinceLastPulse < InactiveThreshold;
		Activity.bHasError = Pair.Value.bHasError;
		Activity.ErrorMessage = Pair.Value.ErrorMessage;

		Activities.Add(Activity);
	}

	// Sort by pulse rate (descending)
	Activities.Sort([](const FRshipTargetActivity& A, const FRshipTargetActivity& B) {
		return A.PulsesPerSecond > B.PulsesPerSecond;
	});

	// Trim to TopN
	if (Activities.Num() > TopN)
	{
		Activities.SetNum(TopN);
	}

	return Activities;
}

TArray<FRshipTargetActivity> URshipHealthMonitor::GetInactiveTargets(float InactiveThresholdSeconds)
{
	TArray<FRshipTargetActivity> Activities;

	if (!Subsystem || !Subsystem->TargetComponents)
	{
		return Activities;
	}

	FDateTime Now = FDateTime::Now();

	for (auto& Pair : *Subsystem->TargetComponents)
	{
		URshipActorRegistrationComponent* Comp = Pair.Value;
		if (!Comp) continue;

		FTargetPulseInfo* Info = TargetPulseInfo.Find(Comp->targetName);
		float SecondsSinceLastPulse = Info ?
			(Now - Info->LastPulseTime).GetTotalSeconds() :
			9999.0f; // Never pulsed

		if (SecondsSinceLastPulse >= InactiveThresholdSeconds)
		{
			FRshipTargetActivity Activity;
			Activity.TargetId = Comp->targetName;
			Activity.TargetName = Comp->targetName;
			Activity.PulsesPerSecond = Info ? Info->PulseCountLastSecond : 0;
			Activity.SecondsSinceLastPulse = SecondsSinceLastPulse;
			Activity.bIsActive = false;
			Activity.bHasError = Info ? Info->bHasError : false;
			Activity.ErrorMessage = Info ? Info->ErrorMessage : TEXT("");

			Activities.Add(Activity);
		}
	}

	// Sort by time since last pulse (descending - longest inactive first)
	Activities.Sort([](const FRshipTargetActivity& A, const FRshipTargetActivity& B) {
		return A.SecondsSinceLastPulse > B.SecondsSinceLastPulse;
	});

	return Activities;
}

TArray<FRshipTargetActivity> URshipHealthMonitor::GetErrorTargets()
{
	TArray<FRshipTargetActivity> Activities;

	for (const auto& Pair : TargetPulseInfo)
	{
		if (Pair.Value.bHasError)
		{
			FRshipTargetActivity Activity;
			Activity.TargetId = Pair.Key;
			Activity.TargetName = Pair.Key;
			Activity.PulsesPerSecond = Pair.Value.PulseCountLastSecond;
			Activity.SecondsSinceLastPulse = (FDateTime::Now() - Pair.Value.LastPulseTime).GetTotalSeconds();
			Activity.bIsActive = Activity.SecondsSinceLastPulse < InactiveThreshold;
			Activity.bHasError = true;
			Activity.ErrorMessage = Pair.Value.ErrorMessage;

			Activities.Add(Activity);
		}
	}

	return Activities;
}

FRshipTargetActivity URshipHealthMonitor::GetTargetActivity(const FString& TargetId)
{
	FRshipTargetActivity Activity;
	Activity.TargetId = TargetId;
	Activity.TargetName = TargetId;

	FTargetPulseInfo* Info = TargetPulseInfo.Find(TargetId);
	if (Info)
	{
		Activity.PulsesPerSecond = Info->PulseCountLastSecond;
		Activity.SecondsSinceLastPulse = (FDateTime::Now() - Info->LastPulseTime).GetTotalSeconds();
		Activity.bIsActive = Activity.SecondsSinceLastPulse < InactiveThreshold;
		Activity.bHasError = Info->bHasError;
		Activity.ErrorMessage = Info->ErrorMessage;
	}
	else
	{
		Activity.SecondsSinceLastPulse = 9999.0f;
		Activity.bIsActive = false;
	}

	return Activity;
}

// ============================================================================
// HEALTH ACTIONS
// ============================================================================

void URshipHealthMonitor::ReconnectAll()
{
	if (Subsystem)
	{
		Subsystem->Reconnect();
		UE_LOG(LogTemp, Log, TEXT("RshipHealthMonitor: Triggered reconnection"));
	}
}

void URshipHealthMonitor::ReregisterAll()
{
	if (Subsystem)
	{
		Subsystem->SendAll();
		UE_LOG(LogTemp, Log, TEXT("RshipHealthMonitor: Triggered re-registration of all targets"));
	}
}

void URshipHealthMonitor::ResetStatistics()
{
	TargetPulseInfo.Empty();
	TotalPulsesLastSecond = 0;
	PreviousMessagesDropped = 0;
	LastSecondRollover = FDateTime::Now();

	if (Subsystem)
	{
		Subsystem->ResetRateLimiterStats();
	}

	UE_LOG(LogTemp, Log, TEXT("RshipHealthMonitor: Statistics reset"));
}

void URshipHealthMonitor::ClearTargetError(const FString& TargetId)
{
	FTargetPulseInfo* Info = TargetPulseInfo.Find(TargetId);
	if (Info)
	{
		Info->bHasError = false;
		Info->ErrorMessage.Empty();
		UE_LOG(LogTemp, Log, TEXT("RshipHealthMonitor: Cleared error for target '%s'"), *TargetId);
	}
}

// ============================================================================
// PULSE TRACKING
// ============================================================================

void URshipHealthMonitor::RecordPulse(const FString& TargetId)
{
	FTargetPulseInfo& Info = TargetPulseInfo.FindOrAdd(TargetId);
	Info.LastPulseTime = FDateTime::Now();
	Info.PulseCountThisSecond++;
}

void URshipHealthMonitor::RecordError(const FString& TargetId, const FString& ErrorMessage)
{
	FTargetPulseInfo& Info = TargetPulseInfo.FindOrAdd(TargetId);
	Info.bHasError = true;
	Info.ErrorMessage = ErrorMessage;

	UE_LOG(LogTemp, Warning, TEXT("RshipHealthMonitor: Error for target '%s': %s"), *TargetId, *ErrorMessage);
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

void URshipHealthMonitor::UpdateHealthData()
{
	FRshipHealthStatus NewStatus = GetCurrentHealth();
	CheckAndFireEvents(NewStatus);
	LastHealth = NewStatus;
}

void URshipHealthMonitor::RolloverPulseCounters()
{
	FDateTime Now = FDateTime::Now();
	float SecondsSinceRollover = (Now - LastSecondRollover).GetTotalSeconds();

	if (SecondsSinceRollover >= 1.0f)
	{
		// Roll over counters
		TotalPulsesLastSecond = 0;

		for (auto& Pair : TargetPulseInfo)
		{
			Pair.Value.PulseCountLastSecond = Pair.Value.PulseCountThisSecond;
			TotalPulsesLastSecond += Pair.Value.PulseCountLastSecond;
			Pair.Value.PulseCountThisSecond = 0;
			Pair.Value.LastSecondStart = Now;
		}

		LastSecondRollover = Now;
	}
}

int32 URshipHealthMonitor::CalculateHealthScore(const FRshipHealthStatus& Status)
{
	int32 Score = 100;

	// Connection penalty
	if (!Status.bIsConnected)
	{
		Score -= 50;
	}

	// Backoff penalty
	if (Status.bIsBackingOff)
	{
		Score -= 20;
	}

	// Queue pressure penalty (progressive)
	if (Status.QueuePressure > 0.9f)
	{
		Score -= 30;
	}
	else if (Status.QueuePressure > 0.7f)
	{
		Score -= 15;
	}
	else if (Status.QueuePressure > 0.5f)
	{
		Score -= 5;
	}

	// Dropped messages penalty
	if (Status.MessagesDroppedLastSecond > 0)
	{
		Score -= FMath::Min(20, Status.MessagesDroppedLastSecond * 2);
	}

	// Error targets penalty
	if (Status.ErrorTargets > 0)
	{
		Score -= FMath::Min(10, Status.ErrorTargets);
	}

	// Inactive targets penalty (minor)
	float InactiveRatio = Status.TotalTargets > 0 ?
		(float)Status.InactiveTargets / Status.TotalTargets : 0.0f;
	if (InactiveRatio > 0.5f)
	{
		Score -= 5;
	}

	return FMath::Clamp(Score, 0, 100);
}

FString URshipHealthMonitor::GenerateStatusSummary(const FRshipHealthStatus& Status)
{
	if (!Status.bIsConnected)
	{
		return TEXT("Disconnected - Attempting to reconnect...");
	}

	if (Status.bIsBackingOff)
	{
		return FString::Printf(TEXT("Rate limited - Backing off for %.1fs"), Status.BackoffRemaining);
	}

	if (Status.QueuePressure > 0.9f)
	{
		return TEXT("Queue nearly full - Messages may be dropped");
	}

	if (Status.QueuePressure > 0.7f)
	{
		return TEXT("High queue pressure - Consider reducing send rate");
	}

	if (Status.ErrorTargets > 0)
	{
		return FString::Printf(TEXT("%d target(s) with errors"), Status.ErrorTargets);
	}

	if (Status.MessagesDroppedLastSecond > 0)
	{
		return FString::Printf(TEXT("Dropping %d msg/s due to backpressure"), Status.MessagesDroppedLastSecond);
	}

	// All good
	if (Status.TotalTargets == 0)
	{
		return TEXT("Connected - No targets registered");
	}

	return FString::Printf(TEXT("Healthy - %d targets, %d active, %d msg/s"),
		Status.TotalTargets, Status.ActiveTargets, Status.MessagesPerSecond);
}

void URshipHealthMonitor::CheckAndFireEvents(const FRshipHealthStatus& NewStatus)
{
	// Connection lost
	if (bWasConnected && !NewStatus.bIsConnected)
	{
		OnConnectionLost.Broadcast();
		UE_LOG(LogTemp, Warning, TEXT("RshipHealthMonitor: Connection lost"));
	}

	// Connection restored
	if (!bWasConnected && NewStatus.bIsConnected)
	{
		OnConnectionRestored.Broadcast();
		UE_LOG(LogTemp, Log, TEXT("RshipHealthMonitor: Connection restored"));
	}

	bWasConnected = NewStatus.bIsConnected;

	// Backpressure warning
	bool bBackpressureWarning = NewStatus.QueuePressure >= BackpressureWarningThreshold;
	if (bBackpressureWarning && !bWasBackpressureWarning)
	{
		OnBackpressureWarning.Broadcast(NewStatus.QueuePressure);
		UE_LOG(LogTemp, Warning, TEXT("RshipHealthMonitor: Backpressure warning (%.0f%%)"),
			NewStatus.QueuePressure * 100.0f);
	}
	bWasBackpressureWarning = bBackpressureWarning;

	// Health changed significantly (score changed by 10+ points)
	if (FMath::Abs(NewStatus.HealthScore - LastHealth.HealthScore) >= 10)
	{
		OnHealthChanged.Broadcast(NewStatus);
	}
}

