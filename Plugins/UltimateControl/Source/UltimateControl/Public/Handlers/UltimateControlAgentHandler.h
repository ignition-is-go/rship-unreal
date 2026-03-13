// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Handlers/UltimateControlHandlerBase.h"
#include "Misc/DateTime.h"

/**
 * Handler for agent-team orchestration.
 *
 * This provides a control plane for multiple AI agents coordinating work:
 * - agent registration/heartbeat
 * - lease-based resource claims
 * - shared task queue
 */
class ULTIMATECONTROL_API FUltimateControlAgentHandler : public FUltimateControlHandlerBase
{
public:
	explicit FUltimateControlAgentHandler(UUltimateControlSubsystem* InSubsystem);
	virtual ~FUltimateControlAgentHandler() override;

private:
	struct FAgentRecord
	{
		FString AgentId;
		FString Role;
		FString SessionId;
		FString Status;
		FString CurrentTaskId;
		TArray<FString> Capabilities;
		TSharedPtr<FJsonObject> Metadata;
		FDateTime RegisteredAt;
		FDateTime LastHeartbeat;
	};

	struct FResourceClaim
	{
		FString LeaseId;
		FString ResourcePath;
		FString AgentId;
		TSharedPtr<FJsonObject> Metadata;
		FDateTime ClaimedAt;
		FDateTime ExpiresAt;
	};

	struct FTaskRecord
	{
		FString TaskId;
		FString Title;
		FString Description;
		FString Status;
		FString Assignee;
		FString CreatedBy;
		FString Error;
		int32 Priority = 50;
		TArray<FString> Tags;
		TSharedPtr<FJsonObject> Payload;
		TSharedPtr<FJsonObject> ResultData;
		FDateTime CreatedAt;
		FDateTime UpdatedAt;
	};

	// Agent lifecycle
	bool HandleRegisterAgent(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleHeartbeat(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleListAgents(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleUnregisterAgent(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Resource claims
	bool HandleClaimResource(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleReleaseResource(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleListClaims(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Task queue
	bool HandleCreateTask(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleAssignTask(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleTakeTask(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleUpdateTask(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleListTasks(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleGetDashboard(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	bool LoadState_NoLock();
	bool SaveState_NoLock();
	bool PersistState_NoLock(bool bForce = false);
	static FString GetStateFilePath();
	static FDateTime ParseIsoDateTimeOrDefault(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, const FDateTime& DefaultValue);

	void CleanupExpiredClaims_NoLock(const FDateTime& Now);
	void ReleaseClaimsForAgent_NoLock(const FString& AgentId);
	bool IsAgentStale_NoLock(const FAgentRecord& Agent, const FDateTime& Now, int32 StaleAfterSeconds) const;

	static TArray<FString> ParseStringArray(const TSharedPtr<FJsonObject>& Params, const FString& FieldName);
	static int32 ParseClampedInt(const TSharedPtr<FJsonObject>& Params, const FString& FieldName, int32 DefaultValue, int32 MinValue, int32 MaxValue);

	TSharedPtr<FJsonObject> AgentToJson_NoLock(const FAgentRecord& Agent, const FDateTime& Now, int32 StaleAfterSeconds) const;
	TSharedPtr<FJsonObject> ClaimToJson_NoLock(const FResourceClaim& Claim, const FDateTime& Now) const;
	TSharedPtr<FJsonObject> TaskToJson_NoLock(const FTaskRecord& Task) const;

	FCriticalSection StateMutex;

	TMap<FString, FAgentRecord> AgentsById;
	TMap<FString, FResourceClaim> ClaimsByResource;
	TMap<FString, FString> ResourceByLeaseId;
	TMap<FString, FTaskRecord> TasksById;
	TArray<FString> TaskOrder;
	FDateTime LastStatePersistedAt = FDateTime::MinValue();
};
