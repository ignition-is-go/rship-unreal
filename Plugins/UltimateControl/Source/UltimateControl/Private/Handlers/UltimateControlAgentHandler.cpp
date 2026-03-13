// Copyright Rocketship. All Rights Reserved.

#include "Handlers/UltimateControlAgentHandler.h"
#include "UltimateControlSubsystem.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	constexpr int32 DefaultStaleAfterSeconds = 120;
	constexpr int32 DefaultLeaseSeconds = 300;
	constexpr int32 PersistenceIntervalSeconds = 5;
	constexpr int32 AgentStateSchemaVersion = 1;

	bool IsClosedTaskStatus(const FString& Status)
	{
		const FString Normalized = Status.ToLower();
		return Normalized == TEXT("completed")
			|| Normalized == TEXT("failed")
			|| Normalized == TEXT("cancelled");
	}

	bool IsTaskTagMatch(const TArray<FString>& TaskTags, const TArray<FString>& RequestedTags)
	{
		if (RequestedTags.Num() == 0)
		{
			return true;
		}

		for (const FString& RequestedTag : RequestedTags)
		{
			for (const FString& TaskTag : TaskTags)
			{
				if (TaskTag.Equals(RequestedTag, ESearchCase::IgnoreCase))
				{
					return true;
				}
			}
		}

		return false;
	}
}

FUltimateControlAgentHandler::FUltimateControlAgentHandler(UUltimateControlSubsystem* InSubsystem)
	: FUltimateControlHandlerBase(InSubsystem)
{
	RegisterMethod(TEXT("agent.register"), TEXT("Register or update an agent"), TEXT("Agent"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAgentHandler::HandleRegisterAgent));
	RegisterMethod(TEXT("agent.heartbeat"), TEXT("Update agent heartbeat and status"), TEXT("Agent"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAgentHandler::HandleHeartbeat));
	RegisterMethod(TEXT("agent.list"), TEXT("List all known agents"), TEXT("Agent"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAgentHandler::HandleListAgents));
	RegisterMethod(TEXT("agent.unregister"), TEXT("Unregister an agent and release claims"), TEXT("Agent"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAgentHandler::HandleUnregisterAgent));

	RegisterMethod(TEXT("agent.claimResource"), TEXT("Claim a shared resource using a lease"), TEXT("Agent"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAgentHandler::HandleClaimResource));
	RegisterMethod(TEXT("agent.releaseResource"), TEXT("Release a claimed resource"), TEXT("Agent"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAgentHandler::HandleReleaseResource));
	RegisterMethod(TEXT("agent.listClaims"), TEXT("List active resource claims"), TEXT("Agent"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAgentHandler::HandleListClaims));

	RegisterMethod(TEXT("agent.createTask"), TEXT("Create a task in the shared queue"), TEXT("Agent"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAgentHandler::HandleCreateTask));
	RegisterMethod(TEXT("agent.assignTask"), TEXT("Assign an existing task to an agent"), TEXT("Agent"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAgentHandler::HandleAssignTask));
	RegisterMethod(TEXT("agent.takeTask"), TEXT("Take next matching queued task"), TEXT("Agent"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAgentHandler::HandleTakeTask));
	RegisterMethod(TEXT("agent.updateTask"), TEXT("Update task status, assignee, and results"), TEXT("Agent"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAgentHandler::HandleUpdateTask));
	RegisterMethod(TEXT("agent.listTasks"), TEXT("List tasks with filters"), TEXT("Agent"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAgentHandler::HandleListTasks));

	RegisterMethod(TEXT("agent.getDashboard"), TEXT("Get orchestration dashboard metrics"), TEXT("Agent"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAgentHandler::HandleGetDashboard));

	FScopeLock Lock(&StateMutex);
	LoadState_NoLock();
}

FUltimateControlAgentHandler::~FUltimateControlAgentHandler()
{
	FScopeLock Lock(&StateMutex);
	SaveState_NoLock();
}

bool FUltimateControlAgentHandler::HandleRegisterAgent(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString AgentId;
	if (!RequireString(Params, TEXT("agentId"), AgentId, Error))
	{
		return false;
	}

	const FDateTime Now = FDateTime::UtcNow();
	const int32 StaleAfterSeconds = ParseClampedInt(Params, TEXT("staleAfterSeconds"), DefaultStaleAfterSeconds, 1, 86400);

	FScopeLock Lock(&StateMutex);

	FAgentRecord* ExistingAgent = AgentsById.Find(AgentId);
	const bool bCreated = (ExistingAgent == nullptr);

	FAgentRecord& Agent = ExistingAgent ? *ExistingAgent : AgentsById.Add(AgentId, FAgentRecord());
	if (bCreated)
	{
		Agent.AgentId = AgentId;
		Agent.RegisteredAt = Now;
	}

	Agent.LastHeartbeat = Now;
	Agent.Role = GetOptionalString(Params, TEXT("role"), Agent.Role.IsEmpty() ? TEXT("generalist") : Agent.Role);
	Agent.SessionId = GetOptionalString(Params, TEXT("sessionId"), Agent.SessionId);
	Agent.Status = GetOptionalString(Params, TEXT("status"), Agent.Status.IsEmpty() ? TEXT("idle") : Agent.Status);
	Agent.CurrentTaskId = GetOptionalString(Params, TEXT("currentTaskId"), Agent.CurrentTaskId);

	if (Params->HasField(TEXT("capabilities")))
	{
		Agent.Capabilities = ParseStringArray(Params, TEXT("capabilities"));
	}

	const TSharedPtr<FJsonObject>* MetadataObject = nullptr;
	if (Params->TryGetObjectField(TEXT("metadata"), MetadataObject) && MetadataObject && MetadataObject->IsValid())
	{
		Agent.Metadata = *MetadataObject;
	}

	TSharedPtr<FJsonObject> ResultObject = MakeShared<FJsonObject>();
	ResultObject->SetBoolField(TEXT("created"), bCreated);
	ResultObject->SetObjectField(TEXT("agent"), AgentToJson_NoLock(Agent, Now, StaleAfterSeconds));
	PersistState_NoLock(false);

	Result = MakeShared<FJsonValueObject>(ResultObject);
	return true;
}

bool FUltimateControlAgentHandler::HandleHeartbeat(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString AgentId;
	if (!RequireString(Params, TEXT("agentId"), AgentId, Error))
	{
		return false;
	}

	const FDateTime Now = FDateTime::UtcNow();
	const int32 StaleAfterSeconds = ParseClampedInt(Params, TEXT("staleAfterSeconds"), DefaultStaleAfterSeconds, 1, 86400);

	FScopeLock Lock(&StateMutex);

	FAgentRecord* Agent = AgentsById.Find(AgentId);
	if (!Agent)
	{
		Error = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Agent not registered: %s"), *AgentId));
		return false;
	}

	Agent->LastHeartbeat = Now;
	Agent->Status = GetOptionalString(Params, TEXT("status"), Agent->Status);
	Agent->CurrentTaskId = GetOptionalString(Params, TEXT("currentTaskId"), Agent->CurrentTaskId);
	Agent->SessionId = GetOptionalString(Params, TEXT("sessionId"), Agent->SessionId);

	if (Params->HasField(TEXT("capabilities")))
	{
		Agent->Capabilities = ParseStringArray(Params, TEXT("capabilities"));
	}

	const TSharedPtr<FJsonObject>* MetadataObject = nullptr;
	if (Params->TryGetObjectField(TEXT("metadata"), MetadataObject) && MetadataObject && MetadataObject->IsValid())
	{
		Agent->Metadata = *MetadataObject;
	}
	PersistState_NoLock(false);

	Result = MakeShared<FJsonValueObject>(AgentToJson_NoLock(*Agent, Now, StaleAfterSeconds));
	return true;
}

bool FUltimateControlAgentHandler::HandleListAgents(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	const bool bIncludeOffline = GetOptionalBool(Params, TEXT("includeOffline"), true);
	const int32 StaleAfterSeconds = ParseClampedInt(Params, TEXT("staleAfterSeconds"), DefaultStaleAfterSeconds, 1, 86400);
	const FString RoleFilter = GetOptionalString(Params, TEXT("role"), TEXT(""));

	const FDateTime Now = FDateTime::UtcNow();
	TArray<TSharedPtr<FJsonValue>> AgentsArray;
	int32 OnlineCount = 0;

	FScopeLock Lock(&StateMutex);
	const int32 ClaimsBeforeCleanup = ClaimsByResource.Num();
	CleanupExpiredClaims_NoLock(Now);
	if (ClaimsByResource.Num() != ClaimsBeforeCleanup)
	{
		PersistState_NoLock(false);
	}

	TArray<FString> AgentIds;
	AgentsById.GenerateKeyArray(AgentIds);
	AgentIds.Sort();

	for (const FString& AgentId : AgentIds)
	{
		const FAgentRecord* Agent = AgentsById.Find(AgentId);
		if (!Agent)
		{
			continue;
		}

		if (!RoleFilter.IsEmpty() && !Agent->Role.Equals(RoleFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		const bool bIsStale = IsAgentStale_NoLock(*Agent, Now, StaleAfterSeconds);
		if (!bIncludeOffline && bIsStale)
		{
			continue;
		}

		if (!bIsStale)
		{
			OnlineCount++;
		}

		AgentsArray.Add(MakeShared<FJsonValueObject>(AgentToJson_NoLock(*Agent, Now, StaleAfterSeconds)));
	}

	TSharedPtr<FJsonObject> ResultObject = MakeShared<FJsonObject>();
	ResultObject->SetArrayField(TEXT("agents"), AgentsArray);
	ResultObject->SetNumberField(TEXT("count"), AgentsArray.Num());
	ResultObject->SetNumberField(TEXT("online"), OnlineCount);
	ResultObject->SetNumberField(TEXT("offline"), AgentsArray.Num() - OnlineCount);
	ResultObject->SetNumberField(TEXT("staleAfterSeconds"), StaleAfterSeconds);

	Result = MakeShared<FJsonValueObject>(ResultObject);
	return true;
}

bool FUltimateControlAgentHandler::HandleUnregisterAgent(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString AgentId;
	if (!RequireString(Params, TEXT("agentId"), AgentId, Error))
	{
		return false;
	}

	const FDateTime Now = FDateTime::UtcNow();
	int32 RequeuedTaskCount = 0;

	FScopeLock Lock(&StateMutex);

	if (!AgentsById.Remove(AgentId))
	{
		Error = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Agent not registered: %s"), *AgentId));
		return false;
	}

	for (auto& Pair : TasksById)
	{
		FTaskRecord& Task = Pair.Value;
		if (Task.Assignee == AgentId && !IsClosedTaskStatus(Task.Status))
		{
			Task.Assignee.Empty();
			Task.Status = TEXT("queued");
			Task.UpdatedAt = Now;
			RequeuedTaskCount++;
		}
	}

	int32 ReleasedClaims = 0;
	for (const auto& ClaimPair : ClaimsByResource)
	{
		if (ClaimPair.Value.AgentId == AgentId)
		{
			ReleasedClaims++;
		}
	}
	ReleaseClaimsForAgent_NoLock(AgentId);

	TSharedPtr<FJsonObject> ResultObject = MakeShared<FJsonObject>();
	ResultObject->SetBoolField(TEXT("success"), true);
	ResultObject->SetStringField(TEXT("agentId"), AgentId);
	ResultObject->SetNumberField(TEXT("releasedClaims"), ReleasedClaims);
	ResultObject->SetNumberField(TEXT("requeuedTasks"), RequeuedTaskCount);
	PersistState_NoLock(true);

	Result = MakeShared<FJsonValueObject>(ResultObject);
	return true;
}

bool FUltimateControlAgentHandler::HandleClaimResource(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString AgentId;
	if (!RequireString(Params, TEXT("agentId"), AgentId, Error))
	{
		return false;
	}

	FString ResourcePath;
	if (!RequireString(Params, TEXT("resourcePath"), ResourcePath, Error))
	{
		return false;
	}

	const bool bForce = GetOptionalBool(Params, TEXT("force"), false);
	const int32 LeaseSeconds = ParseClampedInt(Params, TEXT("leaseSeconds"), DefaultLeaseSeconds, 5, 86400);
	const FDateTime Now = FDateTime::UtcNow();

	FScopeLock Lock(&StateMutex);
	CleanupExpiredClaims_NoLock(Now);

	FAgentRecord* Agent = AgentsById.Find(AgentId);
	if (!Agent)
	{
		Error = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Agent not registered: %s"), *AgentId));
		return false;
	}

	if (FResourceClaim* ExistingClaim = ClaimsByResource.Find(ResourcePath))
	{
		if (ExistingClaim->AgentId != AgentId && !bForce)
		{
			TSharedPtr<FJsonObject> ErrorData = MakeShared<FJsonObject>();
			ErrorData->SetStringField(TEXT("resourcePath"), ExistingClaim->ResourcePath);
			ErrorData->SetStringField(TEXT("ownerAgentId"), ExistingClaim->AgentId);
			ErrorData->SetStringField(TEXT("leaseId"), ExistingClaim->LeaseId);
			ErrorData->SetStringField(TEXT("expiresAt"), ExistingClaim->ExpiresAt.ToIso8601());

			Error = UUltimateControlSubsystem::MakeError(
				EJsonRpcError::OperationFailed,
				FString::Printf(TEXT("Resource already claimed by %s"), *ExistingClaim->AgentId),
				MakeShared<FJsonValueObject>(ErrorData));
			return false;
		}

		ResourceByLeaseId.Remove(ExistingClaim->LeaseId);
	}

	FResourceClaim NewClaim;
	NewClaim.LeaseId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	NewClaim.ResourcePath = ResourcePath;
	NewClaim.AgentId = AgentId;
	NewClaim.ClaimedAt = Now;
	NewClaim.ExpiresAt = Now + FTimespan::FromSeconds(LeaseSeconds);

	const TSharedPtr<FJsonObject>* MetadataObject = nullptr;
	if (Params->TryGetObjectField(TEXT("metadata"), MetadataObject) && MetadataObject && MetadataObject->IsValid())
	{
		NewClaim.Metadata = *MetadataObject;
	}

	ClaimsByResource.Add(ResourcePath, NewClaim);
	ResourceByLeaseId.Add(NewClaim.LeaseId, ResourcePath);

	Agent->LastHeartbeat = Now;
	PersistState_NoLock(true);

	Result = MakeShared<FJsonValueObject>(ClaimToJson_NoLock(NewClaim, Now));
	return true;
}

bool FUltimateControlAgentHandler::HandleReleaseResource(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	const FString LeaseId = GetOptionalString(Params, TEXT("leaseId"), TEXT(""));
	FString ResourcePath = GetOptionalString(Params, TEXT("resourcePath"), TEXT(""));
	const FString AgentId = GetOptionalString(Params, TEXT("agentId"), TEXT(""));
	const bool bForce = GetOptionalBool(Params, TEXT("force"), false);

	if (LeaseId.IsEmpty() && ResourcePath.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::InvalidParams,
			TEXT("leaseId or resourcePath parameter required"));
		return false;
	}

	FScopeLock Lock(&StateMutex);
	CleanupExpiredClaims_NoLock(FDateTime::UtcNow());

	if (ResourcePath.IsEmpty())
	{
		const FString* ResourceForLease = ResourceByLeaseId.Find(LeaseId);
		if (!ResourceForLease)
		{
			Error = UUltimateControlSubsystem::MakeError(
				EJsonRpcError::NotFound,
				FString::Printf(TEXT("Lease not found: %s"), *LeaseId));
			return false;
		}
		ResourcePath = *ResourceForLease;
	}

	FResourceClaim* ExistingClaim = ClaimsByResource.Find(ResourcePath);
	if (!ExistingClaim)
	{
		Error = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Resource claim not found: %s"), *ResourcePath));
		return false;
	}

	const bool bLeaseMatches = !LeaseId.IsEmpty() && ExistingClaim->LeaseId == LeaseId;
	const bool bAgentMatches = !AgentId.IsEmpty() && ExistingClaim->AgentId == AgentId;

	if (!bForce && !bLeaseMatches && !bAgentMatches)
	{
		Error = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::OperationFailed,
			FString::Printf(TEXT("Resource %s is owned by %s"), *ResourcePath, *ExistingClaim->AgentId));
		return false;
	}

	const FString RemovedLeaseId = ExistingClaim->LeaseId;
	const FString OwnerAgentId = ExistingClaim->AgentId;

	ResourceByLeaseId.Remove(RemovedLeaseId);
	ClaimsByResource.Remove(ResourcePath);

	TSharedPtr<FJsonObject> ResultObject = MakeShared<FJsonObject>();
	ResultObject->SetBoolField(TEXT("success"), true);
	ResultObject->SetStringField(TEXT("resourcePath"), ResourcePath);
	ResultObject->SetStringField(TEXT("leaseId"), RemovedLeaseId);
	ResultObject->SetStringField(TEXT("ownerAgentId"), OwnerAgentId);
	PersistState_NoLock(true);

	Result = MakeShared<FJsonValueObject>(ResultObject);
	return true;
}

bool FUltimateControlAgentHandler::HandleListClaims(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	const FString AgentFilter = GetOptionalString(Params, TEXT("agentId"), TEXT(""));
	const FString PrefixFilter = GetOptionalString(Params, TEXT("resourcePrefix"), TEXT(""));
	const FDateTime Now = FDateTime::UtcNow();

	TArray<TSharedPtr<FJsonValue>> ClaimsArray;

	FScopeLock Lock(&StateMutex);
	const int32 ClaimsBeforeCleanup = ClaimsByResource.Num();
	CleanupExpiredClaims_NoLock(Now);
	if (ClaimsByResource.Num() != ClaimsBeforeCleanup)
	{
		PersistState_NoLock(false);
	}

	TArray<FString> ResourcePaths;
	ClaimsByResource.GenerateKeyArray(ResourcePaths);
	ResourcePaths.Sort();

	for (const FString& ResourcePath : ResourcePaths)
	{
		const FResourceClaim* Claim = ClaimsByResource.Find(ResourcePath);
		if (!Claim)
		{
			continue;
		}

		if (!AgentFilter.IsEmpty() && Claim->AgentId != AgentFilter)
		{
			continue;
		}

		if (!PrefixFilter.IsEmpty() && !ResourcePath.StartsWith(PrefixFilter))
		{
			continue;
		}

		ClaimsArray.Add(MakeShared<FJsonValueObject>(ClaimToJson_NoLock(*Claim, Now)));
	}

	TSharedPtr<FJsonObject> ResultObject = MakeShared<FJsonObject>();
	ResultObject->SetArrayField(TEXT("claims"), ClaimsArray);
	ResultObject->SetNumberField(TEXT("count"), ClaimsArray.Num());

	Result = MakeShared<FJsonValueObject>(ResultObject);
	return true;
}

bool FUltimateControlAgentHandler::HandleCreateTask(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString Title;
	if (!RequireString(Params, TEXT("title"), Title, Error))
	{
		return false;
	}

	const FString TaskId = GetOptionalString(Params, TEXT("taskId"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	const FString Assignee = GetOptionalString(Params, TEXT("assignee"), TEXT(""));

	FScopeLock Lock(&StateMutex);

	if (TasksById.Contains(TaskId))
	{
		Error = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::InvalidParams,
			FString::Printf(TEXT("Task already exists: %s"), *TaskId));
		return false;
	}

	if (!Assignee.IsEmpty() && !AgentsById.Contains(Assignee))
	{
		Error = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Assignee agent not registered: %s"), *Assignee));
		return false;
	}

	FTaskRecord Task;
	Task.TaskId = TaskId;
	Task.Title = Title;
	Task.Description = GetOptionalString(Params, TEXT("description"), TEXT(""));
	Task.Assignee = Assignee;
	Task.CreatedBy = GetOptionalString(Params, TEXT("createdBy"), TEXT("orchestrator"));
	Task.Priority = ParseClampedInt(Params, TEXT("priority"), 50, 0, 1000);
	Task.Tags = ParseStringArray(Params, TEXT("tags"));
	Task.Status = GetOptionalString(Params, TEXT("status"), Assignee.IsEmpty() ? TEXT("queued") : TEXT("assigned")).ToLower();

	const TSharedPtr<FJsonObject>* PayloadObject = nullptr;
	if (Params->TryGetObjectField(TEXT("payload"), PayloadObject) && PayloadObject && PayloadObject->IsValid())
	{
		Task.Payload = *PayloadObject;
	}

	const FDateTime Now = FDateTime::UtcNow();
	Task.CreatedAt = Now;
	Task.UpdatedAt = Now;

	TasksById.Add(TaskId, Task);
	TaskOrder.Add(TaskId);
	PersistState_NoLock(true);

	Result = MakeShared<FJsonValueObject>(TaskToJson_NoLock(Task));
	return true;
}

bool FUltimateControlAgentHandler::HandleAssignTask(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString TaskId;
	if (!RequireString(Params, TEXT("taskId"), TaskId, Error))
	{
		return false;
	}

	FString AgentId;
	if (!RequireString(Params, TEXT("agentId"), AgentId, Error))
	{
		return false;
	}

	const FString Status = GetOptionalString(Params, TEXT("status"), TEXT("assigned")).ToLower();
	const FDateTime Now = FDateTime::UtcNow();

	FScopeLock Lock(&StateMutex);

	FTaskRecord* Task = TasksById.Find(TaskId);
	if (!Task)
	{
		Error = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Task not found: %s"), *TaskId));
		return false;
	}

	FAgentRecord* Agent = AgentsById.Find(AgentId);
	if (!Agent)
	{
		Error = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Agent not registered: %s"), *AgentId));
		return false;
	}

	Task->Assignee = AgentId;
	Task->Status = Status;
	Task->UpdatedAt = Now;

	if (Status == TEXT("in_progress"))
	{
		Agent->CurrentTaskId = TaskId;
		Agent->Status = TEXT("busy");
		Agent->LastHeartbeat = Now;
	}
	PersistState_NoLock(true);

	Result = MakeShared<FJsonValueObject>(TaskToJson_NoLock(*Task));
	return true;
}

bool FUltimateControlAgentHandler::HandleTakeTask(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString AgentId;
	if (!RequireString(Params, TEXT("agentId"), AgentId, Error))
	{
		return false;
	}

	const TArray<FString> RequestedTags = ParseStringArray(Params, TEXT("tags"));
	const int32 MaxPriority = ParseClampedInt(Params, TEXT("maxPriority"), 1000, 0, 1000);
	const FDateTime Now = FDateTime::UtcNow();

	FScopeLock Lock(&StateMutex);

	FAgentRecord* Agent = AgentsById.Find(AgentId);
	if (!Agent)
	{
		Error = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Agent not registered: %s"), *AgentId));
		return false;
	}

	FTaskRecord* SelectedTask = nullptr;
	FString SelectedTaskId;

	for (const FString& CandidateTaskId : TaskOrder)
	{
		FTaskRecord* CandidateTask = TasksById.Find(CandidateTaskId);
		if (!CandidateTask)
		{
			continue;
		}

		const bool bQueued = CandidateTask->Status == TEXT("queued");
		const bool bAssignedToAgent = CandidateTask->Status == TEXT("assigned") && CandidateTask->Assignee == AgentId;
		if (!bQueued && !bAssignedToAgent)
		{
			continue;
		}

		if (CandidateTask->Priority > MaxPriority)
		{
			continue;
		}

		if (!IsTaskTagMatch(CandidateTask->Tags, RequestedTags))
		{
			continue;
		}

		if (!SelectedTask
			|| CandidateTask->Priority < SelectedTask->Priority
			|| (CandidateTask->Priority == SelectedTask->Priority && CandidateTask->CreatedAt < SelectedTask->CreatedAt))
		{
			SelectedTask = CandidateTask;
			SelectedTaskId = CandidateTaskId;
		}
	}

	if (!SelectedTask)
	{
		TSharedPtr<FJsonObject> EmptyResult = MakeShared<FJsonObject>();
		EmptyResult->SetBoolField(TEXT("found"), false);
		EmptyResult->SetStringField(TEXT("message"), TEXT("No matching task available"));
		Result = MakeShared<FJsonValueObject>(EmptyResult);
		return true;
	}

	SelectedTask->Assignee = AgentId;
	SelectedTask->Status = TEXT("in_progress");
	SelectedTask->UpdatedAt = Now;

	Agent->CurrentTaskId = SelectedTaskId;
	Agent->Status = TEXT("busy");
	Agent->LastHeartbeat = Now;
	PersistState_NoLock(true);

	TSharedPtr<FJsonObject> ResultObject = MakeShared<FJsonObject>();
	ResultObject->SetBoolField(TEXT("found"), true);
	ResultObject->SetObjectField(TEXT("task"), TaskToJson_NoLock(*SelectedTask));
	Result = MakeShared<FJsonValueObject>(ResultObject);
	return true;
}

bool FUltimateControlAgentHandler::HandleUpdateTask(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	FString TaskId;
	if (!RequireString(Params, TEXT("taskId"), TaskId, Error))
	{
		return false;
	}

	const FDateTime Now = FDateTime::UtcNow();

	FScopeLock Lock(&StateMutex);

	FTaskRecord* Task = TasksById.Find(TaskId);
	if (!Task)
	{
		Error = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Task not found: %s"), *TaskId));
		return false;
	}

	const FString PreviousAssignee = Task->Assignee;
	const FString PreviousStatus = Task->Status;

	if (Params->HasField(TEXT("title")))
	{
		Task->Title = Params->GetStringField(TEXT("title"));
	}
	if (Params->HasField(TEXT("description")))
	{
		Task->Description = Params->GetStringField(TEXT("description"));
	}
	if (Params->HasField(TEXT("priority")))
	{
		Task->Priority = ParseClampedInt(Params, TEXT("priority"), Task->Priority, 0, 1000);
	}
	if (Params->HasField(TEXT("status")))
	{
		Task->Status = Params->GetStringField(TEXT("status")).ToLower();
	}
	if (Params->HasField(TEXT("assignee")))
	{
		Task->Assignee = Params->GetStringField(TEXT("assignee"));
	}
	if (Params->HasField(TEXT("error")))
	{
		Task->Error = Params->GetStringField(TEXT("error"));
	}
	if (Params->HasField(TEXT("tags")))
	{
		Task->Tags = ParseStringArray(Params, TEXT("tags"));
	}

	const TSharedPtr<FJsonObject>* PayloadObject = nullptr;
	if (Params->TryGetObjectField(TEXT("payload"), PayloadObject) && PayloadObject && PayloadObject->IsValid())
	{
		Task->Payload = *PayloadObject;
	}

	const TSharedPtr<FJsonObject>* ResultObject = nullptr;
	if (Params->TryGetObjectField(TEXT("result"), ResultObject) && ResultObject && ResultObject->IsValid())
	{
		Task->ResultData = *ResultObject;
	}

	if (!Task->Assignee.IsEmpty() && !AgentsById.Contains(Task->Assignee))
	{
		Error = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Assignee agent not registered: %s"), *Task->Assignee));
		return false;
	}

	if (Task->Status == TEXT("queued"))
	{
		Task->Assignee.Empty();
	}

	if (Task->Status == TEXT("in_progress") && Task->Assignee.IsEmpty())
	{
		Error = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::InvalidParams,
			TEXT("in_progress task requires assignee"));
		return false;
	}

	Task->UpdatedAt = Now;

	if (!PreviousAssignee.IsEmpty() && (PreviousAssignee != Task->Assignee || IsClosedTaskStatus(Task->Status)))
	{
		if (FAgentRecord* PreviousAgent = AgentsById.Find(PreviousAssignee))
		{
			if (PreviousAgent->CurrentTaskId == Task->TaskId)
			{
				PreviousAgent->CurrentTaskId.Empty();
				if (PreviousAgent->Status == TEXT("busy"))
				{
					PreviousAgent->Status = TEXT("idle");
				}
			}
		}
	}

	if (!Task->Assignee.IsEmpty())
	{
		if (FAgentRecord* CurrentAssignee = AgentsById.Find(Task->Assignee))
		{
			if (Task->Status == TEXT("in_progress"))
			{
				CurrentAssignee->CurrentTaskId = Task->TaskId;
				CurrentAssignee->Status = TEXT("busy");
				CurrentAssignee->LastHeartbeat = Now;
			}
			else if (IsClosedTaskStatus(Task->Status) && CurrentAssignee->CurrentTaskId == Task->TaskId)
			{
				CurrentAssignee->CurrentTaskId.Empty();
				if (CurrentAssignee->Status == TEXT("busy"))
				{
					CurrentAssignee->Status = TEXT("idle");
				}
			}
		}
	}

	if (PreviousStatus == TEXT("in_progress") && IsClosedTaskStatus(Task->Status) && Task->Assignee.IsEmpty())
	{
		Task->Assignee = PreviousAssignee;
	}
	PersistState_NoLock(true);

	Result = MakeShared<FJsonValueObject>(TaskToJson_NoLock(*Task));
	return true;
}

bool FUltimateControlAgentHandler::HandleListTasks(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	const FString StatusFilter = GetOptionalString(Params, TEXT("status"), TEXT("")).ToLower();
	const FString AssigneeFilter = GetOptionalString(Params, TEXT("assignee"), TEXT(""));
	const FString TagFilter = GetOptionalString(Params, TEXT("tag"), TEXT(""));
	const bool bIncludeClosed = GetOptionalBool(Params, TEXT("includeClosed"), true);
	const int32 Limit = ParseClampedInt(Params, TEXT("limit"), 500, 1, 5000);

	TArray<TSharedPtr<FJsonValue>> TasksArray;
	TMap<FString, int32> StatusCounts;

	FScopeLock Lock(&StateMutex);

	for (const FString& TaskId : TaskOrder)
	{
		const FTaskRecord* Task = TasksById.Find(TaskId);
		if (!Task)
		{
			continue;
		}

		if (!StatusFilter.IsEmpty() && Task->Status != StatusFilter)
		{
			continue;
		}

		if (!AssigneeFilter.IsEmpty() && Task->Assignee != AssigneeFilter)
		{
			continue;
		}

		if (!TagFilter.IsEmpty())
		{
			bool bFoundTag = false;
			for (const FString& Tag : Task->Tags)
			{
				if (Tag.Equals(TagFilter, ESearchCase::IgnoreCase))
				{
					bFoundTag = true;
					break;
				}
			}
			if (!bFoundTag)
			{
				continue;
			}
		}

		if (!bIncludeClosed && IsClosedTaskStatus(Task->Status))
		{
			continue;
		}

		TasksArray.Add(MakeShared<FJsonValueObject>(TaskToJson_NoLock(*Task)));
		StatusCounts.FindOrAdd(Task->Status)++;

		if (TasksArray.Num() >= Limit)
		{
			break;
		}
	}

	TSharedPtr<FJsonObject> CountsObject = MakeShared<FJsonObject>();
	for (const auto& CountPair : StatusCounts)
	{
		CountsObject->SetNumberField(CountPair.Key, CountPair.Value);
	}

	TSharedPtr<FJsonObject> ResultObject = MakeShared<FJsonObject>();
	ResultObject->SetArrayField(TEXT("tasks"), TasksArray);
	ResultObject->SetNumberField(TEXT("count"), TasksArray.Num());
	ResultObject->SetObjectField(TEXT("statusCounts"), CountsObject);

	Result = MakeShared<FJsonValueObject>(ResultObject);
	return true;
}

bool FUltimateControlAgentHandler::HandleGetDashboard(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error)
{
	const int32 StaleAfterSeconds = ParseClampedInt(Params, TEXT("staleAfterSeconds"), DefaultStaleAfterSeconds, 1, 86400);
	const FDateTime Now = FDateTime::UtcNow();

	int32 OnlineAgents = 0;
	int32 OfflineAgents = 0;
	int32 BusyAgents = 0;

	int32 QueuedTasks = 0;
	int32 AssignedTasks = 0;
	int32 InProgressTasks = 0;
	int32 CompletedTasks = 0;
	int32 FailedTasks = 0;

	TArray<TSharedPtr<FJsonValue>> ClaimsArray;

	FScopeLock Lock(&StateMutex);
	const int32 ClaimsBeforeCleanup = ClaimsByResource.Num();
	CleanupExpiredClaims_NoLock(Now);
	if (ClaimsByResource.Num() != ClaimsBeforeCleanup)
	{
		PersistState_NoLock(false);
	}

	for (const auto& AgentPair : AgentsById)
	{
		const FAgentRecord& Agent = AgentPair.Value;
		const bool bStale = IsAgentStale_NoLock(Agent, Now, StaleAfterSeconds);
		if (bStale)
		{
			OfflineAgents++;
		}
		else
		{
			OnlineAgents++;
			if (Agent.Status.Equals(TEXT("busy"), ESearchCase::IgnoreCase))
			{
				BusyAgents++;
			}
		}
	}

	for (const auto& TaskPair : TasksById)
	{
		const FString Status = TaskPair.Value.Status;
		if (Status == TEXT("queued"))
		{
			QueuedTasks++;
		}
		else if (Status == TEXT("assigned"))
		{
			AssignedTasks++;
		}
		else if (Status == TEXT("in_progress"))
		{
			InProgressTasks++;
		}
		else if (Status == TEXT("completed"))
		{
			CompletedTasks++;
		}
		else if (Status == TEXT("failed"))
		{
			FailedTasks++;
		}
	}

	TArray<FString> ClaimKeys;
	ClaimsByResource.GenerateKeyArray(ClaimKeys);
	ClaimKeys.Sort();
	for (const FString& ClaimKey : ClaimKeys)
	{
		const FResourceClaim* Claim = ClaimsByResource.Find(ClaimKey);
		if (Claim)
		{
			ClaimsArray.Add(MakeShared<FJsonValueObject>(ClaimToJson_NoLock(*Claim, Now)));
		}
	}

	TSharedPtr<FJsonObject> ResultObject = MakeShared<FJsonObject>();
	ResultObject->SetStringField(TEXT("generatedAt"), Now.ToIso8601());
	ResultObject->SetNumberField(TEXT("totalAgents"), AgentsById.Num());
	ResultObject->SetNumberField(TEXT("onlineAgents"), OnlineAgents);
	ResultObject->SetNumberField(TEXT("offlineAgents"), OfflineAgents);
	ResultObject->SetNumberField(TEXT("busyAgents"), BusyAgents);
	ResultObject->SetNumberField(TEXT("activeClaims"), ClaimsByResource.Num());
	ResultObject->SetArrayField(TEXT("claims"), ClaimsArray);

	TSharedPtr<FJsonObject> TasksObject = MakeShared<FJsonObject>();
	TasksObject->SetNumberField(TEXT("total"), TasksById.Num());
	TasksObject->SetNumberField(TEXT("queued"), QueuedTasks);
	TasksObject->SetNumberField(TEXT("assigned"), AssignedTasks);
	TasksObject->SetNumberField(TEXT("inProgress"), InProgressTasks);
	TasksObject->SetNumberField(TEXT("completed"), CompletedTasks);
	TasksObject->SetNumberField(TEXT("failed"), FailedTasks);
	ResultObject->SetObjectField(TEXT("tasks"), TasksObject);

	Result = MakeShared<FJsonValueObject>(ResultObject);
	return true;
}

FString FUltimateControlAgentHandler::GetStateFilePath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UltimateControl"), TEXT("AgentOrchestrationState.json"));
}

FDateTime FUltimateControlAgentHandler::ParseIsoDateTimeOrDefault(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName, const FDateTime& DefaultValue)
{
	if (!JsonObject.IsValid())
	{
		return DefaultValue;
	}

	FString IsoString;
	if (!JsonObject->TryGetStringField(FieldName, IsoString) || IsoString.IsEmpty())
	{
		return DefaultValue;
	}

	FDateTime ParsedDateTime;
	if (FDateTime::ParseIso8601(*IsoString, ParsedDateTime))
	{
		return ParsedDateTime;
	}

	return DefaultValue;
}

bool FUltimateControlAgentHandler::LoadState_NoLock()
{
	const FString StateFilePath = GetStateFilePath();
	if (!FPaths::FileExists(StateFilePath))
	{
		return true;
	}

	FString FileContents;
	if (!FFileHelper::LoadFileToString(FileContents, *StateFilePath))
	{
		UE_LOG(LogUltimateControlServer, Warning, TEXT("Failed to load agent state file: %s"), *StateFilePath);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContents);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogUltimateControlServer, Warning, TEXT("Failed to parse agent state file: %s"), *StateFilePath);
		return false;
	}

	AgentsById.Empty();
	ClaimsByResource.Empty();
	ResourceByLeaseId.Empty();
	TasksById.Empty();
	TaskOrder.Empty();

	const FDateTime Now = FDateTime::UtcNow();

	const TArray<TSharedPtr<FJsonValue>>* AgentsArray = nullptr;
	if (RootObject->TryGetArrayField(TEXT("agents"), AgentsArray) && AgentsArray)
	{
		for (const TSharedPtr<FJsonValue>& AgentValue : *AgentsArray)
		{
			const TSharedPtr<FJsonObject>* AgentObject = nullptr;
			if (!AgentValue.IsValid() || !AgentValue->TryGetObject(AgentObject) || !AgentObject || !(*AgentObject).IsValid())
			{
				continue;
			}

			FAgentRecord Agent;
			if (!(*AgentObject)->TryGetStringField(TEXT("agentId"), Agent.AgentId) || Agent.AgentId.IsEmpty())
			{
				continue;
			}

			(*AgentObject)->TryGetStringField(TEXT("role"), Agent.Role);
			(*AgentObject)->TryGetStringField(TEXT("sessionId"), Agent.SessionId);
			(*AgentObject)->TryGetStringField(TEXT("status"), Agent.Status);
			(*AgentObject)->TryGetStringField(TEXT("currentTaskId"), Agent.CurrentTaskId);

			Agent.RegisteredAt = ParseIsoDateTimeOrDefault(*AgentObject, TEXT("registeredAt"), Now);
			Agent.LastHeartbeat = ParseIsoDateTimeOrDefault(*AgentObject, TEXT("lastHeartbeat"), Agent.RegisteredAt);

			Agent.Capabilities = ParseStringArray(*AgentObject, TEXT("capabilities"));

			const TSharedPtr<FJsonObject>* MetadataObject = nullptr;
			if ((*AgentObject)->TryGetObjectField(TEXT("metadata"), MetadataObject) && MetadataObject && MetadataObject->IsValid())
			{
				Agent.Metadata = *MetadataObject;
			}

			AgentsById.Add(Agent.AgentId, Agent);
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* ClaimsArray = nullptr;
	if (RootObject->TryGetArrayField(TEXT("claims"), ClaimsArray) && ClaimsArray)
	{
		for (const TSharedPtr<FJsonValue>& ClaimValue : *ClaimsArray)
		{
			const TSharedPtr<FJsonObject>* ClaimObject = nullptr;
			if (!ClaimValue.IsValid() || !ClaimValue->TryGetObject(ClaimObject) || !ClaimObject || !(*ClaimObject).IsValid())
			{
				continue;
			}

			FResourceClaim Claim;
			if (!(*ClaimObject)->TryGetStringField(TEXT("leaseId"), Claim.LeaseId) || Claim.LeaseId.IsEmpty())
			{
				continue;
			}
			if (!(*ClaimObject)->TryGetStringField(TEXT("resourcePath"), Claim.ResourcePath) || Claim.ResourcePath.IsEmpty())
			{
				continue;
			}
			if (!(*ClaimObject)->TryGetStringField(TEXT("agentId"), Claim.AgentId) || Claim.AgentId.IsEmpty())
			{
				continue;
			}

			Claim.ClaimedAt = ParseIsoDateTimeOrDefault(*ClaimObject, TEXT("claimedAt"), Now);
			Claim.ExpiresAt = ParseIsoDateTimeOrDefault(*ClaimObject, TEXT("expiresAt"), Claim.ClaimedAt + FTimespan::FromSeconds(DefaultLeaseSeconds));

			if (Claim.ExpiresAt <= Now)
			{
				continue;
			}

			const TSharedPtr<FJsonObject>* MetadataObject = nullptr;
			if ((*ClaimObject)->TryGetObjectField(TEXT("metadata"), MetadataObject) && MetadataObject && MetadataObject->IsValid())
			{
				Claim.Metadata = *MetadataObject;
			}

			ClaimsByResource.Add(Claim.ResourcePath, Claim);
			ResourceByLeaseId.Add(Claim.LeaseId, Claim.ResourcePath);
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* TasksArray = nullptr;
	if (RootObject->TryGetArrayField(TEXT("tasks"), TasksArray) && TasksArray)
	{
		for (const TSharedPtr<FJsonValue>& TaskValue : *TasksArray)
		{
			const TSharedPtr<FJsonObject>* TaskObject = nullptr;
			if (!TaskValue.IsValid() || !TaskValue->TryGetObject(TaskObject) || !TaskObject || !(*TaskObject).IsValid())
			{
				continue;
			}

			FTaskRecord Task;
			if (!(*TaskObject)->TryGetStringField(TEXT("taskId"), Task.TaskId) || Task.TaskId.IsEmpty())
			{
				continue;
			}

			(*TaskObject)->TryGetStringField(TEXT("title"), Task.Title);
			(*TaskObject)->TryGetStringField(TEXT("description"), Task.Description);
			(*TaskObject)->TryGetStringField(TEXT("status"), Task.Status);
			(*TaskObject)->TryGetStringField(TEXT("assignee"), Task.Assignee);
			(*TaskObject)->TryGetStringField(TEXT("createdBy"), Task.CreatedBy);
			(*TaskObject)->TryGetStringField(TEXT("error"), Task.Error);

			double Priority = 50.0;
			if ((*TaskObject)->TryGetNumberField(TEXT("priority"), Priority))
			{
				Task.Priority = FMath::RoundToInt(Priority);
			}

			Task.Tags = ParseStringArray(*TaskObject, TEXT("tags"));

			const TSharedPtr<FJsonObject>* PayloadObject = nullptr;
			if ((*TaskObject)->TryGetObjectField(TEXT("payload"), PayloadObject) && PayloadObject && PayloadObject->IsValid())
			{
				Task.Payload = *PayloadObject;
			}

			const TSharedPtr<FJsonObject>* ResultObject = nullptr;
			if ((*TaskObject)->TryGetObjectField(TEXT("result"), ResultObject) && ResultObject && ResultObject->IsValid())
			{
				Task.ResultData = *ResultObject;
			}

			Task.CreatedAt = ParseIsoDateTimeOrDefault(*TaskObject, TEXT("createdAt"), Now);
			Task.UpdatedAt = ParseIsoDateTimeOrDefault(*TaskObject, TEXT("updatedAt"), Task.CreatedAt);

			TasksById.Add(Task.TaskId, Task);
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* TaskOrderArray = nullptr;
	if (RootObject->TryGetArrayField(TEXT("taskOrder"), TaskOrderArray) && TaskOrderArray)
	{
		for (const TSharedPtr<FJsonValue>& TaskIdValue : *TaskOrderArray)
		{
			FString TaskId;
			if (!TaskIdValue.IsValid() || !TaskIdValue->TryGetString(TaskId))
			{
				continue;
			}

			if (!TaskId.IsEmpty() && TasksById.Contains(TaskId))
			{
				TaskOrder.Add(TaskId);
			}
		}
	}

	TSet<FString> OrderedTaskIds;
	for (const FString& TaskId : TaskOrder)
	{
		OrderedTaskIds.Add(TaskId);
	}
	TArray<FTaskRecord> UnorderedTasks;
	for (const auto& TaskPair : TasksById)
	{
		if (!OrderedTaskIds.Contains(TaskPair.Key))
		{
			UnorderedTasks.Add(TaskPair.Value);
		}
	}

	UnorderedTasks.Sort([](const FTaskRecord& A, const FTaskRecord& B)
	{
		return A.CreatedAt < B.CreatedAt;
	});

	for (const FTaskRecord& Task : UnorderedTasks)
	{
		TaskOrder.Add(Task.TaskId);
	}

	LastStatePersistedAt = Now;

	UE_LOG(LogUltimateControlServer, Log, TEXT("Loaded agent orchestration state (%d agents, %d claims, %d tasks)"),
		AgentsById.Num(), ClaimsByResource.Num(), TasksById.Num());
	return true;
}

bool FUltimateControlAgentHandler::SaveState_NoLock()
{
	const FString StateFilePath = GetStateFilePath();
	const FString StateDirectory = FPaths::GetPath(StateFilePath);
	IFileManager::Get().MakeDirectory(*StateDirectory, true);

	TSharedPtr<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetNumberField(TEXT("schemaVersion"), AgentStateSchemaVersion);
	RootObject->SetStringField(TEXT("savedAt"), FDateTime::UtcNow().ToIso8601());

	TArray<TSharedPtr<FJsonValue>> AgentsArray;
	TArray<FString> AgentIds;
	AgentsById.GenerateKeyArray(AgentIds);
	AgentIds.Sort();
	for (const FString& AgentId : AgentIds)
	{
		const FAgentRecord* Agent = AgentsById.Find(AgentId);
		if (!Agent)
		{
			continue;
		}

		TSharedPtr<FJsonObject> AgentObject = MakeShared<FJsonObject>();
		AgentObject->SetStringField(TEXT("agentId"), Agent->AgentId);
		AgentObject->SetStringField(TEXT("role"), Agent->Role);
		AgentObject->SetStringField(TEXT("sessionId"), Agent->SessionId);
		AgentObject->SetStringField(TEXT("status"), Agent->Status);
		AgentObject->SetStringField(TEXT("currentTaskId"), Agent->CurrentTaskId);
		AgentObject->SetStringField(TEXT("registeredAt"), Agent->RegisteredAt.ToIso8601());
		AgentObject->SetStringField(TEXT("lastHeartbeat"), Agent->LastHeartbeat.ToIso8601());

		TArray<TSharedPtr<FJsonValue>> CapabilitiesArray;
		for (const FString& Capability : Agent->Capabilities)
		{
			CapabilitiesArray.Add(MakeShared<FJsonValueString>(Capability));
		}
		AgentObject->SetArrayField(TEXT("capabilities"), CapabilitiesArray);

		if (Agent->Metadata.IsValid())
		{
			AgentObject->SetObjectField(TEXT("metadata"), Agent->Metadata);
		}

		AgentsArray.Add(MakeShared<FJsonValueObject>(AgentObject));
	}
	RootObject->SetArrayField(TEXT("agents"), AgentsArray);

	TArray<TSharedPtr<FJsonValue>> ClaimsArray;
	TArray<FString> ClaimResourcePaths;
	ClaimsByResource.GenerateKeyArray(ClaimResourcePaths);
	ClaimResourcePaths.Sort();
	for (const FString& ResourcePath : ClaimResourcePaths)
	{
		const FResourceClaim* Claim = ClaimsByResource.Find(ResourcePath);
		if (!Claim)
		{
			continue;
		}

		TSharedPtr<FJsonObject> ClaimObject = MakeShared<FJsonObject>();
		ClaimObject->SetStringField(TEXT("leaseId"), Claim->LeaseId);
		ClaimObject->SetStringField(TEXT("resourcePath"), Claim->ResourcePath);
		ClaimObject->SetStringField(TEXT("agentId"), Claim->AgentId);
		ClaimObject->SetStringField(TEXT("claimedAt"), Claim->ClaimedAt.ToIso8601());
		ClaimObject->SetStringField(TEXT("expiresAt"), Claim->ExpiresAt.ToIso8601());

		if (Claim->Metadata.IsValid())
		{
			ClaimObject->SetObjectField(TEXT("metadata"), Claim->Metadata);
		}

		ClaimsArray.Add(MakeShared<FJsonValueObject>(ClaimObject));
	}
	RootObject->SetArrayField(TEXT("claims"), ClaimsArray);

	TArray<FString> PersistedTaskOrder;
	PersistedTaskOrder.Reserve(TaskOrder.Num());
	for (const FString& TaskId : TaskOrder)
	{
		if (TasksById.Contains(TaskId))
		{
			PersistedTaskOrder.Add(TaskId);
		}
	}

	TSet<FString> OrderedTaskIds;
	for (const FString& TaskId : PersistedTaskOrder)
	{
		OrderedTaskIds.Add(TaskId);
	}
	TArray<FString> RemainingTaskIds;
	TasksById.GenerateKeyArray(RemainingTaskIds);
	RemainingTaskIds.Sort([this](const FString& A, const FString& B)
	{
		const FTaskRecord* TaskA = TasksById.Find(A);
		const FTaskRecord* TaskB = TasksById.Find(B);
		if (!TaskA || !TaskB)
		{
			return A < B;
		}
		return TaskA->CreatedAt < TaskB->CreatedAt;
	});

	for (const FString& TaskId : RemainingTaskIds)
	{
		if (!OrderedTaskIds.Contains(TaskId))
		{
			PersistedTaskOrder.Add(TaskId);
		}
	}

	TaskOrder = PersistedTaskOrder;

	TArray<TSharedPtr<FJsonValue>> TasksArray;
	for (const FString& TaskId : TaskOrder)
	{
		const FTaskRecord* Task = TasksById.Find(TaskId);
		if (!Task)
		{
			continue;
		}

		TSharedPtr<FJsonObject> TaskObject = MakeShared<FJsonObject>();
		TaskObject->SetStringField(TEXT("taskId"), Task->TaskId);
		TaskObject->SetStringField(TEXT("title"), Task->Title);
		TaskObject->SetStringField(TEXT("description"), Task->Description);
		TaskObject->SetStringField(TEXT("status"), Task->Status);
		TaskObject->SetStringField(TEXT("assignee"), Task->Assignee);
		TaskObject->SetStringField(TEXT("createdBy"), Task->CreatedBy);
		TaskObject->SetStringField(TEXT("error"), Task->Error);
		TaskObject->SetNumberField(TEXT("priority"), Task->Priority);
		TaskObject->SetStringField(TEXT("createdAt"), Task->CreatedAt.ToIso8601());
		TaskObject->SetStringField(TEXT("updatedAt"), Task->UpdatedAt.ToIso8601());

		TArray<TSharedPtr<FJsonValue>> TagsArray;
		for (const FString& Tag : Task->Tags)
		{
			TagsArray.Add(MakeShared<FJsonValueString>(Tag));
		}
		TaskObject->SetArrayField(TEXT("tags"), TagsArray);

		if (Task->Payload.IsValid())
		{
			TaskObject->SetObjectField(TEXT("payload"), Task->Payload);
		}
		if (Task->ResultData.IsValid())
		{
			TaskObject->SetObjectField(TEXT("result"), Task->ResultData);
		}

		TasksArray.Add(MakeShared<FJsonValueObject>(TaskObject));
	}
	RootObject->SetArrayField(TEXT("tasks"), TasksArray);

	TArray<TSharedPtr<FJsonValue>> TaskOrderArray;
	for (const FString& TaskId : TaskOrder)
	{
		TaskOrderArray.Add(MakeShared<FJsonValueString>(TaskId));
	}
	RootObject->SetArrayField(TEXT("taskOrder"), TaskOrderArray);

	FString OutputJson;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputJson);
	if (!FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer))
	{
		UE_LOG(LogUltimateControlServer, Warning, TEXT("Failed to serialize agent state file: %s"), *StateFilePath);
		return false;
	}

	if (!FFileHelper::SaveStringToFile(OutputJson, *StateFilePath))
	{
		UE_LOG(LogUltimateControlServer, Warning, TEXT("Failed to save agent state file: %s"), *StateFilePath);
		return false;
	}

	LastStatePersistedAt = FDateTime::UtcNow();
	return true;
}

bool FUltimateControlAgentHandler::PersistState_NoLock(bool bForce)
{
	const FDateTime Now = FDateTime::UtcNow();
	if (!bForce && LastStatePersistedAt != FDateTime::MinValue()
		&& (Now - LastStatePersistedAt).GetTotalSeconds() < PersistenceIntervalSeconds)
	{
		return true;
	}

	return SaveState_NoLock();
}

void FUltimateControlAgentHandler::CleanupExpiredClaims_NoLock(const FDateTime& Now)
{
	for (auto It = ClaimsByResource.CreateIterator(); It; ++It)
	{
		if (It->Value.ExpiresAt <= Now)
		{
			ResourceByLeaseId.Remove(It->Value.LeaseId);
			It.RemoveCurrent();
		}
	}
}

void FUltimateControlAgentHandler::ReleaseClaimsForAgent_NoLock(const FString& AgentId)
{
	TArray<FString> ResourcesToRelease;
	for (const auto& Pair : ClaimsByResource)
	{
		if (Pair.Value.AgentId == AgentId)
		{
			ResourcesToRelease.Add(Pair.Key);
		}
	}

	for (const FString& ResourcePath : ResourcesToRelease)
	{
		if (FResourceClaim* Claim = ClaimsByResource.Find(ResourcePath))
		{
			ResourceByLeaseId.Remove(Claim->LeaseId);
		}
		ClaimsByResource.Remove(ResourcePath);
	}
}

bool FUltimateControlAgentHandler::IsAgentStale_NoLock(const FAgentRecord& Agent, const FDateTime& Now, int32 StaleAfterSeconds) const
{
	return Agent.LastHeartbeat + FTimespan::FromSeconds(StaleAfterSeconds) < Now;
}

TArray<FString> FUltimateControlAgentHandler::ParseStringArray(const TSharedPtr<FJsonObject>& Params, const FString& FieldName)
{
	TArray<FString> Values;
	if (!Params.IsValid())
	{
		return Values;
	}

	const TArray<TSharedPtr<FJsonValue>>* JsonValues = nullptr;
	if (!Params->TryGetArrayField(FieldName, JsonValues) || !JsonValues)
	{
		return Values;
	}

	for (const TSharedPtr<FJsonValue>& Value : *JsonValues)
	{
		FString StringValue;
		if (Value.IsValid() && Value->TryGetString(StringValue))
		{
			Values.Add(StringValue);
		}
	}

	return Values;
}

int32 FUltimateControlAgentHandler::ParseClampedInt(const TSharedPtr<FJsonObject>& Params, const FString& FieldName, int32 DefaultValue, int32 MinValue, int32 MaxValue)
{
	double NumberValue = 0.0;
	if (Params.IsValid() && Params->TryGetNumberField(FieldName, NumberValue))
	{
		return FMath::Clamp(FMath::RoundToInt(NumberValue), MinValue, MaxValue);
	}
	return DefaultValue;
}

TSharedPtr<FJsonObject> FUltimateControlAgentHandler::AgentToJson_NoLock(const FAgentRecord& Agent, const FDateTime& Now, int32 StaleAfterSeconds) const
{
	TSharedPtr<FJsonObject> ResultObject = MakeShared<FJsonObject>();
	const bool bIsStale = IsAgentStale_NoLock(Agent, Now, StaleAfterSeconds);

	int32 ActiveClaims = 0;
	for (const auto& ClaimPair : ClaimsByResource)
	{
		if (ClaimPair.Value.AgentId == Agent.AgentId)
		{
			ActiveClaims++;
		}
	}

	int32 OpenTasks = 0;
	for (const auto& TaskPair : TasksById)
	{
		const FTaskRecord& Task = TaskPair.Value;
		if (Task.Assignee == Agent.AgentId && !IsClosedTaskStatus(Task.Status))
		{
			OpenTasks++;
		}
	}

	ResultObject->SetStringField(TEXT("agentId"), Agent.AgentId);
	ResultObject->SetStringField(TEXT("role"), Agent.Role);
	ResultObject->SetStringField(TEXT("sessionId"), Agent.SessionId);
	ResultObject->SetStringField(TEXT("status"), bIsStale ? TEXT("offline") : Agent.Status);
	ResultObject->SetStringField(TEXT("currentTaskId"), Agent.CurrentTaskId);
	ResultObject->SetBoolField(TEXT("online"), !bIsStale);
	ResultObject->SetStringField(TEXT("registeredAt"), Agent.RegisteredAt.ToIso8601());
	ResultObject->SetStringField(TEXT("lastHeartbeat"), Agent.LastHeartbeat.ToIso8601());
	ResultObject->SetNumberField(TEXT("activeClaims"), ActiveClaims);
	ResultObject->SetNumberField(TEXT("openTasks"), OpenTasks);

	TArray<TSharedPtr<FJsonValue>> CapabilitiesArray;
	for (const FString& Capability : Agent.Capabilities)
	{
		CapabilitiesArray.Add(MakeShared<FJsonValueString>(Capability));
	}
	ResultObject->SetArrayField(TEXT("capabilities"), CapabilitiesArray);

	if (Agent.Metadata.IsValid())
	{
		ResultObject->SetObjectField(TEXT("metadata"), Agent.Metadata);
	}

	return ResultObject;
}

TSharedPtr<FJsonObject> FUltimateControlAgentHandler::ClaimToJson_NoLock(const FResourceClaim& Claim, const FDateTime& Now) const
{
	TSharedPtr<FJsonObject> ResultObject = MakeShared<FJsonObject>();

	ResultObject->SetStringField(TEXT("leaseId"), Claim.LeaseId);
	ResultObject->SetStringField(TEXT("resourcePath"), Claim.ResourcePath);
	ResultObject->SetStringField(TEXT("agentId"), Claim.AgentId);
	ResultObject->SetStringField(TEXT("claimedAt"), Claim.ClaimedAt.ToIso8601());
	ResultObject->SetStringField(TEXT("expiresAt"), Claim.ExpiresAt.ToIso8601());
	ResultObject->SetNumberField(TEXT("secondsRemaining"), FMath::Max(0.0, (Claim.ExpiresAt - Now).GetTotalSeconds()));

	if (Claim.Metadata.IsValid())
	{
		ResultObject->SetObjectField(TEXT("metadata"), Claim.Metadata);
	}

	return ResultObject;
}

TSharedPtr<FJsonObject> FUltimateControlAgentHandler::TaskToJson_NoLock(const FTaskRecord& Task) const
{
	TSharedPtr<FJsonObject> ResultObject = MakeShared<FJsonObject>();

	ResultObject->SetStringField(TEXT("taskId"), Task.TaskId);
	ResultObject->SetStringField(TEXT("title"), Task.Title);
	ResultObject->SetStringField(TEXT("description"), Task.Description);
	ResultObject->SetStringField(TEXT("status"), Task.Status);
	ResultObject->SetStringField(TEXT("assignee"), Task.Assignee);
	ResultObject->SetStringField(TEXT("createdBy"), Task.CreatedBy);
	ResultObject->SetNumberField(TEXT("priority"), Task.Priority);
	ResultObject->SetStringField(TEXT("createdAt"), Task.CreatedAt.ToIso8601());
	ResultObject->SetStringField(TEXT("updatedAt"), Task.UpdatedAt.ToIso8601());
	ResultObject->SetStringField(TEXT("error"), Task.Error);

	TArray<TSharedPtr<FJsonValue>> TagsArray;
	for (const FString& Tag : Task.Tags)
	{
		TagsArray.Add(MakeShared<FJsonValueString>(Tag));
	}
	ResultObject->SetArrayField(TEXT("tags"), TagsArray);

	if (Task.Payload.IsValid())
	{
		ResultObject->SetObjectField(TEXT("payload"), Task.Payload);
	}
	if (Task.ResultData.IsValid())
	{
		ResultObject->SetObjectField(TEXT("result"), Task.ResultData);
	}

	return ResultObject;
}
