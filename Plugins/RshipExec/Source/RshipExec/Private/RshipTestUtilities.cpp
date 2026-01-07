// Copyright Rocketship. All Rights Reserved.

#include "RshipTestUtilities.h"
#include "RshipSubsystem.h"
#include "RshipTargetComponent.h"
#include "RshipMaterialBinding.h"
#include "RshipLiveLinkSource.h"
#include "RshipTimecodeSync.h"
#include "RshipPulseReceiver.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

URshipTestUtilities::URshipTestUtilities()
{
}

URshipSubsystem* URshipTestUtilities::GetSubsystem() const
{
	if (GEngine)
	{
		return GEngine->GetEngineSubsystem<URshipSubsystem>();
	}
	return nullptr;
}

TArray<URshipTargetComponent*> URshipTestUtilities::GetAllTargetComponents() const
{
	TArray<URshipTargetComponent*> Result;

	UWorld* World = nullptr;

#if WITH_EDITOR
	if (GEditor)
	{
		World = GEditor->GetEditorWorldContext().World();
	}
#endif

	if (!World && GEngine)
	{
		World = GEngine->GetCurrentPlayWorld();
	}

	if (!World)
	{
		return Result;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (URshipTargetComponent* Target = Actor->FindComponentByClass<URshipTargetComponent>())
		{
			Result.Add(Target);
		}
	}

	return Result;
}

URshipTargetComponent* URshipTestUtilities::FindTargetById(const FString& TargetId) const
{
	TArray<URshipTargetComponent*> Targets = GetAllTargetComponents();
	for (URshipTargetComponent* Target : Targets)
	{
		if (Target && Target->targetName == TargetId)
		{
			return Target;
		}
	}
	return nullptr;
}

bool URshipTestUtilities::InjectPulseToTarget(URshipTargetComponent* Target, const FString& EmitterId, TSharedPtr<FJsonObject> Data)
{
	if (!Target || !Data.IsValid())
	{
		return false;
	}

	// Inject via the pulse receiver to properly broadcast to all listeners
	URshipSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		return false;
	}

	URshipPulseReceiver* PulseReceiver = Subsystem->GetPulseReceiver();
	if (!PulseReceiver)
	{
		return false;
	}

	// Build the full emitter ID: target.emitter
	FString FullEmitterId = FString::Printf(TEXT("%s:%s"), *Target->targetName, *EmitterId);

	// Process the pulse through the receiver (broadcasts to all listeners)
	PulseReceiver->ProcessPulseEvent(FullEmitterId, Data);

	// Also trigger the target's data callback
	Target->OnDataReceived();

	return true;
}

// ============================================================================
// MOCK PULSE INJECTION
// ============================================================================

bool URshipTestUtilities::InjectMockPulse(const FString& TargetId, const FString& EmitterId, const FString& JsonData)
{
	URshipTargetComponent* Target = FindTargetById(TargetId);
	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("RshipTestUtilities: Target '%s' not found"), *TargetId);
		return false;
	}

	// Parse JSON data
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		// If not valid JSON, wrap the raw value
		JsonObject = MakeShared<FJsonObject>();
		JsonObject->SetStringField(TEXT("value"), JsonData);
	}

	return InjectPulseToTarget(Target, EmitterId, JsonObject);
}

bool URshipTestUtilities::InjectMockPulseFloat(const FString& TargetId, const FString& EmitterId, float Value)
{
	URshipTargetComponent* Target = FindTargetById(TargetId);
	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("RshipTestUtilities: Target '%s' not found"), *TargetId);
		return false;
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("value"), Value);

	return InjectPulseToTarget(Target, EmitterId, Data);
}

bool URshipTestUtilities::InjectMockPulseColor(const FString& TargetId, const FString& EmitterId, FLinearColor Color)
{
	URshipTargetComponent* Target = FindTargetById(TargetId);
	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("RshipTestUtilities: Target '%s' not found"), *TargetId);
		return false;
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("r"), Color.R);
	Data->SetNumberField(TEXT("g"), Color.G);
	Data->SetNumberField(TEXT("b"), Color.B);
	Data->SetNumberField(TEXT("a"), Color.A);

	return InjectPulseToTarget(Target, EmitterId, Data);
}

bool URshipTestUtilities::InjectMockPulseTransform(const FString& TargetId, const FString& EmitterId, const FTransform& Transform)
{
	URshipTargetComponent* Target = FindTargetById(TargetId);
	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("RshipTestUtilities: Target '%s' not found"), *TargetId);
		return false;
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();

	// Position
	FVector Location = Transform.GetLocation();
	Data->SetNumberField(TEXT("px"), Location.X);
	Data->SetNumberField(TEXT("py"), Location.Y);
	Data->SetNumberField(TEXT("pz"), Location.Z);

	// Rotation (as Euler angles in degrees)
	FRotator Rotation = Transform.GetRotation().Rotator();
	Data->SetNumberField(TEXT("rx"), Rotation.Roll);
	Data->SetNumberField(TEXT("ry"), Rotation.Pitch);
	Data->SetNumberField(TEXT("rz"), Rotation.Yaw);

	// Scale
	FVector Scale = Transform.GetScale3D();
	Data->SetNumberField(TEXT("sx"), Scale.X);
	Data->SetNumberField(TEXT("sy"), Scale.Y);
	Data->SetNumberField(TEXT("sz"), Scale.Z);

	return InjectPulseToTarget(Target, EmitterId, Data);
}

int32 URshipTestUtilities::InjectRandomPulsesToAllTargets()
{
	int32 PulsesInjected = 0;
	TArray<URshipTargetComponent*> Targets = GetAllTargetComponents();

	for (URshipTargetComponent* Target : Targets)
	{
		if (!Target)
		{
			continue;
		}

		// Inject random intensity value
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetNumberField(TEXT("value"), FMath::FRand());

		if (InjectPulseToTarget(Target, TEXT("intensity"), Data))
		{
			PulsesInjected++;
		}

		// Also inject random color
		TSharedPtr<FJsonObject> ColorData = MakeShared<FJsonObject>();
		ColorData->SetNumberField(TEXT("r"), FMath::FRand());
		ColorData->SetNumberField(TEXT("g"), FMath::FRand());
		ColorData->SetNumberField(TEXT("b"), FMath::FRand());
		ColorData->SetNumberField(TEXT("a"), 1.0f);

		if (InjectPulseToTarget(Target, TEXT("color"), ColorData))
		{
			PulsesInjected++;
		}
	}

	return PulsesInjected;
}

// ============================================================================
// SETUP VALIDATION
// ============================================================================

TArray<FRshipTestIssue> URshipTestUtilities::ValidateAll()
{
	TArray<FRshipTestIssue> Results;

	Results.Append(ValidateTargets());
	Results.Append(ValidateMaterialBindings());
	Results.Append(ValidateLiveLinkSetup());
	Results.Append(ValidateTimecodeSetup());

	return Results;
}

TArray<FRshipTestIssue> URshipTestUtilities::ValidateTargets()
{
	TArray<FRshipTestIssue> Results;
	TArray<URshipTargetComponent*> Targets = GetAllTargetComponents();
	TMap<FString, URshipTargetComponent*> TargetIdMap;

	if (Targets.Num() == 0)
	{
		FRshipTestIssue Result;
		Result.Severity = ERshipTestSeverity::Info;
		Result.Category = TEXT("Target");
		Result.Message = TEXT("No RshipTargetComponents found in level");
		Result.Details = TEXT("Add RshipTargetComponent to actors you want to control via rship");
		Results.Add(Result);
		return Results;
	}

	for (URshipTargetComponent* Target : Targets)
	{
		if (!Target)
		{
			continue;
		}

		AActor* Owner = Target->GetOwner();
		FString OwnerName = Owner ? Owner->GetName() : TEXT("Unknown");
		FString OwnerPath = Owner ? Owner->GetPathName() : TEXT("");

		// Check for empty Target ID
		if (Target->targetName.IsEmpty())
		{
			FRshipTestIssue Result;
			Result.Severity = ERshipTestSeverity::Error;
			Result.Category = TEXT("Target");
			Result.Message = FString::Printf(TEXT("Target on '%s' has no Target ID"), *OwnerName);
			Result.Details = TEXT("Target ID is required for rship to identify this target");
			Result.SuggestedFix = TEXT("Set a unique Target ID in the component properties");
			Result.EntityPath = OwnerPath;
			Results.Add(Result);
			continue;
		}

		// Check for duplicate Target IDs
		if (URshipTargetComponent** ExistingTarget = TargetIdMap.Find(Target->targetName))
		{
			AActor* ExistingOwner = (*ExistingTarget)->GetOwner();
			FString ExistingName = ExistingOwner ? ExistingOwner->GetName() : TEXT("Unknown");

			FRshipTestIssue Result;
			Result.Severity = ERshipTestSeverity::Error;
			Result.Category = TEXT("Target");
			Result.Message = FString::Printf(TEXT("Duplicate Target ID '%s'"), *Target->targetName);
			Result.Details = FString::Printf(TEXT("Both '%s' and '%s' use the same Target ID"), *OwnerName, *ExistingName);
			Result.SuggestedFix = TEXT("Ensure each target has a unique Target ID");
			Result.EntityPath = OwnerPath;
			Results.Add(Result);
		}
		else
		{
			TargetIdMap.Add(Target->targetName, Target);
		}

		// Validate individual target
		TArray<FRshipTestIssue> TargetResults = ValidateTargetComponent(Target);
		Results.Append(TargetResults);
	}

	// Summary
	FRshipTestIssue Summary;
	Summary.Severity = ERshipTestSeverity::Info;
	Summary.Category = TEXT("Target");
	Summary.Message = FString::Printf(TEXT("Found %d target(s)"), Targets.Num());
	Results.Add(Summary);

	return Results;
}

TArray<FRshipTestIssue> URshipTestUtilities::ValidateTargetComponent(URshipTargetComponent* Target)
{
	TArray<FRshipTestIssue> Results;

	if (!Target)
	{
		return Results;
	}

	AActor* Owner = Target->GetOwner();
	FString OwnerName = Owner ? Owner->GetName() : TEXT("Unknown");
	FString OwnerPath = Owner ? Owner->GetPathName() : TEXT("");

	// Check if target is registered with subsystem
	URshipSubsystem* Subsystem = GetSubsystem();
	if (Subsystem && Target->TargetData)
	{
		// Use O(1) lookup by target ID
		if (!Subsystem->FindTargetComponent(Target->TargetData->GetId()))
		{
			FRshipTestIssue Result;
			Result.Severity = ERshipTestSeverity::Warning;
			Result.Category = TEXT("Target");
			Result.Message = FString::Printf(TEXT("Target '%s' not registered with subsystem"), *Target->targetName);
			Result.Details = TEXT("Target may not have been initialized yet or was never registered");
			Result.SuggestedFix = TEXT("Ensure the target is in a loaded level and properly initialized");
			Result.EntityPath = OwnerPath;
			Results.Add(Result);
		}
	}

	// Check for very long Target IDs (potential typo/copy-paste error)
	if (Target->targetName.Len() > 64)
	{
		FRshipTestIssue Result;
		Result.Severity = ERshipTestSeverity::Warning;
		Result.Category = TEXT("Target");
		Result.Message = FString::Printf(TEXT("Target ID is very long (%d chars)"), Target->targetName.Len());
		Result.Details = TEXT("Long Target IDs may indicate a copy-paste error");
		Result.SuggestedFix = TEXT("Consider using a shorter, more descriptive Target ID");
		Result.EntityPath = OwnerPath;
		Results.Add(Result);
	}

	// Check for special characters in Target ID
	for (TCHAR Char : Target->targetName)
	{
		if (!FChar::IsAlnum(Char) && Char != '_' && Char != '-')
		{
			FRshipTestIssue Result;
			Result.Severity = ERshipTestSeverity::Warning;
			Result.Category = TEXT("Target");
			Result.Message = FString::Printf(TEXT("Target ID '%s' contains special characters"), *Target->targetName);
			Result.Details = TEXT("Special characters may cause issues with some rship integrations");
			Result.SuggestedFix = TEXT("Use only alphanumeric characters, underscores, and hyphens");
			Result.EntityPath = OwnerPath;
			Results.Add(Result);
			break;
		}
	}

	return Results;
}

TArray<FRshipTestIssue> URshipTestUtilities::ValidateMaterialBindings()
{
	TArray<FRshipTestIssue> Results;

	URshipSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		FRshipTestIssue Result;
		Result.Severity = ERshipTestSeverity::Warning;
		Result.Category = TEXT("Material");
		Result.Message = TEXT("Subsystem not available for material validation");
		Results.Add(Result);
		return Results;
	}

	URshipMaterialManager* MaterialManager = Subsystem->GetMaterialManager();
	if (!MaterialManager)
	{
		FRshipTestIssue Result;
		Result.Severity = ERshipTestSeverity::Info;
		Result.Category = TEXT("Material");
		Result.Message = TEXT("Material manager not initialized");
		Results.Add(Result);
		return Results;
	}

	// Get bindings and validate
	TArray<URshipMaterialBinding*> Bindings = MaterialManager->GetAllBindings();

	if (Bindings.Num() == 0)
	{
		FRshipTestIssue Result;
		Result.Severity = ERshipTestSeverity::Info;
		Result.Category = TEXT("Material");
		Result.Message = TEXT("No material bindings configured");
		Results.Add(Result);
		return Results;
	}

	for (URshipMaterialBinding* Binding : Bindings)
	{
		if (!Binding)
		{
			continue;
		}

		// Check if material instances are available
		TArray<UMaterialInstanceDynamic*> Materials = Binding->GetDynamicMaterials();
		if (Materials.Num() == 0)
		{
			FRshipTestIssue Result;
			Result.Severity = ERshipTestSeverity::Warning;
			Result.Category = TEXT("Material");
			Result.Message = TEXT("Material binding has no dynamic material instances");
			Result.Details = TEXT("Materials may not be set up yet (happens on BeginPlay)");
			Result.SuggestedFix = TEXT("Ensure the binding is on an actor with mesh components");
			Results.Add(Result);
		}

		// Check if emitter ID is set
		if (Binding->EmitterId.IsEmpty())
		{
			FRshipTestIssue Result;
			Result.Severity = ERshipTestSeverity::Warning;
			Result.Category = TEXT("Material");
			Result.Message = TEXT("Material binding has no emitter ID");
			Result.SuggestedFix = TEXT("Set an emitter ID for the material binding");
			Results.Add(Result);
		}

		// Check if any bindings are configured
		if (Binding->ScalarBindings.Num() == 0 && Binding->VectorBindings.Num() == 0 && Binding->TextureBindings.Num() == 0)
		{
			FRshipTestIssue Result;
			Result.Severity = ERshipTestSeverity::Warning;
			Result.Category = TEXT("Material");
			Result.Message = TEXT("Material binding has no parameter bindings configured");
			Result.SuggestedFix = TEXT("Add scalar, vector, or texture bindings");
			Results.Add(Result);
		}
	}

	FRshipTestIssue Summary;
	Summary.Severity = ERshipTestSeverity::Info;
	Summary.Category = TEXT("Material");
	Summary.Message = FString::Printf(TEXT("Found %d material binding(s)"), Bindings.Num());
	Results.Add(Summary);

	return Results;
}

TArray<FRshipTestIssue> URshipTestUtilities::ValidateLiveLinkSetup()
{
	TArray<FRshipTestIssue> Results;

	URshipSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		FRshipTestIssue Result;
		Result.Severity = ERshipTestSeverity::Warning;
		Result.Category = TEXT("LiveLink");
		Result.Message = TEXT("Subsystem not available for LiveLink validation");
		Results.Add(Result);
		return Results;
	}

	URshipLiveLinkService* LiveLinkService = Subsystem->GetLiveLinkService();
	if (!LiveLinkService)
	{
		FRshipTestIssue Result;
		Result.Severity = ERshipTestSeverity::Info;
		Result.Category = TEXT("LiveLink");
		Result.Message = TEXT("LiveLink service not initialized");
		Results.Add(Result);
		return Results;
	}

	// Check mode
	ERshipLiveLinkMode Mode = LiveLinkService->GetMode();
	FRshipTestIssue ModeResult;
	ModeResult.Severity = ERshipTestSeverity::Info;
	ModeResult.Category = TEXT("LiveLink");
	switch (Mode)
	{
	case ERshipLiveLinkMode::Consume:
		ModeResult.Message = TEXT("LiveLink mode: Consume (rship -> LiveLink)");
		break;
	case ERshipLiveLinkMode::Publish:
		ModeResult.Message = TEXT("LiveLink mode: Publish (LiveLink -> rship)");
		break;
	case ERshipLiveLinkMode::Bidirectional:
		ModeResult.Message = TEXT("LiveLink mode: Bidirectional");
		break;
	}
	Results.Add(ModeResult);

	// If publishing, check emitter mappings
	if (Mode == ERshipLiveLinkMode::Publish || Mode == ERshipLiveLinkMode::Bidirectional)
	{
		TArray<FRshipLiveLinkEmitterMapping> Mappings = LiveLinkService->GetAllEmitterMappings();
		if (Mappings.Num() == 0)
		{
			FRshipTestIssue Result;
			Result.Severity = ERshipTestSeverity::Warning;
			Result.Category = TEXT("LiveLink");
			Result.Message = TEXT("No LiveLink subjects configured for publishing");
			Result.SuggestedFix = TEXT("Add subjects to publish in the LiveLink panel");
			Results.Add(Result);
		}
		else
		{
			FRshipTestIssue Result;
			Result.Severity = ERshipTestSeverity::Info;
			Result.Category = TEXT("LiveLink");
			Result.Message = FString::Printf(TEXT("%d LiveLink subject(s) publishing to rship"), Mappings.Num());
			Results.Add(Result);
		}
	}

	return Results;
}

TArray<FRshipTestIssue> URshipTestUtilities::ValidateTimecodeSetup()
{
	TArray<FRshipTestIssue> Results;

	URshipSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem)
	{
		FRshipTestIssue Result;
		Result.Severity = ERshipTestSeverity::Warning;
		Result.Category = TEXT("Timecode");
		Result.Message = TEXT("Subsystem not available for timecode validation");
		Results.Add(Result);
		return Results;
	}

	URshipTimecodeSync* TimecodeSync = Subsystem->GetTimecodeSync();
	if (!TimecodeSync)
	{
		FRshipTestIssue Result;
		Result.Severity = ERshipTestSeverity::Info;
		Result.Category = TEXT("Timecode");
		Result.Message = TEXT("Timecode sync not initialized");
		Results.Add(Result);
		return Results;
	}

	// Check mode
	ERshipTimecodeMode Mode = TimecodeSync->GetTimecodeMode();
	FRshipTestIssue ModeResult;
	ModeResult.Severity = ERshipTestSeverity::Info;
	ModeResult.Category = TEXT("Timecode");
	switch (Mode)
	{
	case ERshipTimecodeMode::Receive:
		ModeResult.Message = TEXT("Timecode mode: Receive (UE follows rship)");
		break;
	case ERshipTimecodeMode::Publish:
		ModeResult.Message = TEXT("Timecode mode: Publish (UE is master)");
		break;
	case ERshipTimecodeMode::Bidirectional:
		ModeResult.Message = TEXT("Timecode mode: Bidirectional");
		break;
	}
	Results.Add(ModeResult);

	// Check current timecode status
	FRshipTimecodeStatus Status = TimecodeSync->GetStatus();
	FRshipTestIssue StatusResult;
	StatusResult.Severity = ERshipTestSeverity::Info;
	StatusResult.Category = TEXT("Timecode");
	StatusResult.Message = FString::Printf(TEXT("Current timecode: %02d:%02d:%02d:%02d @ %.2f fps"),
		Status.Timecode.Hours, Status.Timecode.Minutes, Status.Timecode.Seconds, Status.Timecode.Frames,
		Status.FrameRate.AsDecimal());
	Results.Add(StatusResult);

	return Results;
}

// ============================================================================
// STRESS TESTING
// ============================================================================

void URshipTestUtilities::StartStressTest(const FRshipStressTestConfig& Config)
{
	if (bStressTestRunning)
	{
		UE_LOG(LogTemp, Warning, TEXT("RshipTestUtilities: Stress test already running"));
		return;
	}

	StressTestConfig = Config;
	StressTestResults = FRshipStressTestResults();
	StressTestElapsed = 0.0f;
	AccumulatedPulseTime = 0.0f;
	bStressTestRunning = true;

	// Cache target IDs matching pattern
	StressTestTargetIds.Empty();
	TArray<URshipTargetComponent*> Targets = GetAllTargetComponents();

	for (URshipTargetComponent* Target : Targets)
	{
		if (!Target || Target->targetName.IsEmpty())
		{
			continue;
		}

		if (Config.TargetIdPattern.IsEmpty() || Target->targetName.Contains(Config.TargetIdPattern))
		{
			StressTestTargetIds.Add(Target->targetName);
		}
	}

	if (StressTestTargetIds.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("RshipTestUtilities: No targets match stress test pattern"));
		bStressTestRunning = false;
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("RshipTestUtilities: Starting stress test - %d pulses/sec for %.1fs to %d targets"),
		Config.PulsesPerSecond, Config.DurationSeconds, StressTestTargetIds.Num());
}

void URshipTestUtilities::StopStressTest()
{
	if (!bStressTestRunning)
	{
		return;
	}

	bStressTestRunning = false;
	StressTestResults.bCompleted = false;
	StressTestResults.ActualDuration = StressTestElapsed;

	if (StressTestElapsed > 0)
	{
		StressTestResults.EffectivePulsesPerSecond = StressTestResults.TotalPulsesSent / StressTestElapsed;
	}

	UE_LOG(LogTemp, Log, TEXT("RshipTestUtilities: Stress test stopped - %d pulses sent in %.2fs"),
		StressTestResults.TotalPulsesSent, StressTestElapsed);
}

float URshipTestUtilities::GetStressTestProgress() const
{
	if (!bStressTestRunning || StressTestConfig.DurationSeconds <= 0)
	{
		return 0.0f;
	}

	return FMath::Clamp(StressTestElapsed / StressTestConfig.DurationSeconds, 0.0f, 1.0f);
}

void URshipTestUtilities::Tick(float DeltaTime)
{
	if (!bStressTestRunning)
	{
		return;
	}

	StressTestElapsed += DeltaTime;

	// Check if test is complete
	if (StressTestElapsed >= StressTestConfig.DurationSeconds)
	{
		bStressTestRunning = false;
		StressTestResults.bCompleted = true;
		StressTestResults.ActualDuration = StressTestElapsed;

		if (StressTestElapsed > 0)
		{
			StressTestResults.EffectivePulsesPerSecond = StressTestResults.TotalPulsesSent / StressTestElapsed;
		}

		UE_LOG(LogTemp, Log, TEXT("RshipTestUtilities: Stress test complete - %d pulses sent (%.1f/sec)"),
			StressTestResults.TotalPulsesSent, StressTestResults.EffectivePulsesPerSecond);

		OnStressTestCompleted.Broadcast(StressTestResults);
		return;
	}

	// Calculate pulses to send this frame
	float SecondsPerPulse = 1.0f / StressTestConfig.PulsesPerSecond;
	AccumulatedPulseTime += DeltaTime;

	int32 PulsesToSend = FMath::FloorToInt(AccumulatedPulseTime / SecondsPerPulse);
	AccumulatedPulseTime -= PulsesToSend * SecondsPerPulse;

	// Cap to prevent frame spikes
	PulsesToSend = FMath::Min(PulsesToSend, 1000);

	// Send pulses
	for (int32 i = 0; i < PulsesToSend; i++)
	{
		if (StressTestTargetIds.Num() == 0)
		{
			break;
		}

		// Pick a target
		int32 TargetIndex = FMath::RandRange(0, StressTestTargetIds.Num() - 1);
		const FString& TargetId = StressTestTargetIds[TargetIndex];

		// Determine emitter
		FString EmitterId = StressTestConfig.EmitterId.IsEmpty() ? TEXT("intensity") : StressTestConfig.EmitterId;

		// Create pulse data
		float Value = StressTestConfig.bRandomizeValues ? FMath::FRand() : 0.5f;

		if (InjectMockPulseFloat(TargetId, EmitterId, Value))
		{
			StressTestResults.TotalPulsesSent++;
		}
		else
		{
			StressTestResults.PulsesDropped++;
		}
	}

	// Broadcast progress periodically
	static float LastProgressBroadcast = 0.0f;
	if (StressTestElapsed - LastProgressBroadcast > 0.5f)
	{
		LastProgressBroadcast = StressTestElapsed;
		OnStressTestProgress.Broadcast(GetStressTestProgress(), StressTestResults.TotalPulsesSent);
	}
}

// ============================================================================
// CONNECTION SIMULATION
// ============================================================================

void URshipTestUtilities::SimulateDisconnect()
{
	bSimulatingDisconnect = true;

	// Note: We don't actually disconnect - that would affect the real connection.
	// This is for testing UI/logic that responds to disconnect states.
	UE_LOG(LogTemp, Log, TEXT("RshipTestUtilities: Simulating disconnect"));
}

void URshipTestUtilities::SimulateReconnect()
{
	if (!bSimulatingDisconnect)
	{
		return;
	}

	bSimulatingDisconnect = false;

	// Trigger actual reconnect if requested
	URshipSubsystem* Subsystem = GetSubsystem();
	if (Subsystem)
	{
		Subsystem->Reconnect();
	}

	UE_LOG(LogTemp, Log, TEXT("RshipTestUtilities: Simulating reconnect"));
}

void URshipTestUtilities::SetSimulatedLatency(float LatencyMs)
{
	SimulatedLatencyMs = FMath::Max(0.0f, LatencyMs);

	// Note: Actually implementing latency simulation would require modifying
	// the WebSocket layer, which is complex. For now, this is just state tracking.
	UE_LOG(LogTemp, Log, TEXT("RshipTestUtilities: Simulated latency set to %.1fms"), SimulatedLatencyMs);
}

void URshipTestUtilities::ResetConnectionSimulation()
{
	bSimulatingDisconnect = false;
	SimulatedLatencyMs = 0.0f;

	UE_LOG(LogTemp, Log, TEXT("RshipTestUtilities: Connection simulation reset"));
}
