// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpRouteHandle.h"
#include "Dom/JsonObject.h"
#include "UltimateControlSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUltimateControlServer, Log, All);

class FUltimateControlAssetHandler;
class FUltimateControlBlueprintHandler;
class FUltimateControlLevelHandler;
class FUltimateControlPIEHandler;
class FUltimateControlAutomationHandler;
class FUltimateControlProfilingHandler;
class FUltimateControlFileHandler;
class FUltimateControlConsoleHandler;
class FUltimateControlProjectHandler;
class FUltimateControlViewportHandler;
class FUltimateControlTransactionHandler;
class FUltimateControlMaterialHandler;
class FUltimateControlAnimationHandler;
class FUltimateControlSequencerHandler;
class FUltimateControlAudioHandler;
class FUltimateControlPhysicsHandler;
class FUltimateControlLightingHandler;
class FUltimateControlWorldPartitionHandler;
class FUltimateControlNiagaraHandler;
class FUltimateControlLandscapeHandler;
class FUltimateControlAIHandler;
class FUltimateControlRenderHandler;
class FUltimateControlOutlinerHandler;
class FUltimateControlSourceControlHandler;
class FUltimateControlLiveCodingHandler;
class FUltimateControlSessionHandler;
class FUltimateControlEditorHandler;

/**
 * JSON-RPC 2.0 Error Codes
 */
namespace EJsonRpcError
{
	constexpr int32 ParseError = -32700;
	constexpr int32 InvalidRequest = -32600;
	constexpr int32 MethodNotFound = -32601;
	constexpr int32 InvalidParams = -32602;
	constexpr int32 InternalError = -32603;
	// Application-specific errors
	constexpr int32 Unauthorized = -32000;
	constexpr int32 FeatureDisabled = -32001;
	constexpr int32 OperationFailed = -32002;
	constexpr int32 NotFound = -32003;
	constexpr int32 ConfirmationRequired = -32004;
	constexpr int32 Timeout = -32005;
}

/**
 * Delegate for JSON-RPC method handlers
 * @param Params - The params object from the JSON-RPC request
 * @param OutResult - The result to return (set on success)
 * @param OutError - The error object to return (set on failure)
 * @return true if the method succeeded, false if it failed
 */
DECLARE_DELEGATE_RetVal_ThreeParams(bool, FJsonRpcMethodHandler,
	const TSharedPtr<FJsonObject>& /* Params */,
	TSharedPtr<FJsonValue>& /* OutResult */,
	TSharedPtr<FJsonObject>& /* OutError */);

/**
 * Information about a registered JSON-RPC method
 */
struct FJsonRpcMethodInfo
{
	FString Name;
	FString Description;
	FString Category;
	TSharedPtr<FJsonObject> ParamsSchema;
	TSharedPtr<FJsonObject> ResultSchema;
	FJsonRpcMethodHandler Handler;
	bool bRequiresConfirmation = false;
	bool bIsDangerous = false;
};

/**
 * Editor subsystem that provides HTTP JSON-RPC API for controlling Unreal Engine
 */
UCLASS()
class ULTIMATECONTROL_API UUltimateControlSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin UEditorSubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End UEditorSubsystem Interface

	/** Get the subsystem instance */
	static UUltimateControlSubsystem* Get();

	/** Start the HTTP server */
	UFUNCTION(BlueprintCallable, Category = "UltimateControl")
	bool StartServer();

	/** Stop the HTTP server */
	UFUNCTION(BlueprintCallable, Category = "UltimateControl")
	void StopServer();

	/** Check if the server is running */
	UFUNCTION(BlueprintPure, Category = "UltimateControl")
	bool IsServerRunning() const { return bServerRunning; }

	/** Get the current server port */
	UFUNCTION(BlueprintPure, Category = "UltimateControl")
	int32 GetServerPort() const;

	/** Get the auth token (for display/copying) */
	UFUNCTION(BlueprintPure, Category = "UltimateControl")
	FString GetAuthToken() const;

	/**
	 * Register a JSON-RPC method handler
	 * @param MethodName - The method name (e.g., "asset.list")
	 * @param MethodInfo - Information about the method including handler
	 */
	void RegisterMethod(const FString& MethodName, const FJsonRpcMethodInfo& MethodInfo);

	/**
	 * Unregister a JSON-RPC method
	 */
	void UnregisterMethod(const FString& MethodName);

	/**
	 * Get all registered methods (for introspection)
	 */
	const TMap<FString, FJsonRpcMethodInfo>& GetRegisteredMethods() const { return RegisteredMethods; }

	/**
	 * Create a JSON-RPC error object
	 */
	static TSharedPtr<FJsonObject> MakeError(int32 Code, const FString& Message, const TSharedPtr<FJsonValue>& Data = nullptr);

	/**
	 * Create a JSON-RPC success result
	 */
	static TSharedPtr<FJsonObject> MakeResult(const TSharedPtr<FJsonValue>& Result, const TSharedPtr<FJsonValue>& Id);

protected:
	/** Handle incoming HTTP request */
	bool HandleHttpRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	/** Process a JSON-RPC request object */
	TSharedPtr<FJsonObject> ProcessJsonRpcRequest(const TSharedPtr<FJsonObject>& RequestObj);

	/** Validate authentication */
	bool ValidateAuth(const FHttpServerRequest& Request, TSharedPtr<FJsonObject>& OutError);

	/** Initialize all method handlers */
	void InitializeHandlers();

	/** Cleanup all method handlers */
	void CleanupHandlers();

	/** Register built-in system methods */
	void RegisterSystemMethods();

private:
	/** HTTP router handle */
	TSharedPtr<IHttpRouter> HttpRouter;

	/** Route handle for cleanup */
	FHttpRouteHandle RouteHandle;

	/** Whether the server is currently running */
	bool bServerRunning = false;

	/** Map of method name -> handler info */
	TMap<FString, FJsonRpcMethodInfo> RegisteredMethods;

	/** Handler instances */
	TUniquePtr<FUltimateControlAssetHandler> AssetHandler;
	TUniquePtr<FUltimateControlBlueprintHandler> BlueprintHandler;
	TUniquePtr<FUltimateControlLevelHandler> LevelHandler;
	TUniquePtr<FUltimateControlPIEHandler> PIEHandler;
	TUniquePtr<FUltimateControlAutomationHandler> AutomationHandler;
	TUniquePtr<FUltimateControlProfilingHandler> ProfilingHandler;
	TUniquePtr<FUltimateControlFileHandler> FileHandler;
	TUniquePtr<FUltimateControlConsoleHandler> ConsoleHandler;
	TUniquePtr<FUltimateControlProjectHandler> ProjectHandler;
	TUniquePtr<FUltimateControlViewportHandler> ViewportHandler;
	TUniquePtr<FUltimateControlTransactionHandler> TransactionHandler;
	TUniquePtr<FUltimateControlMaterialHandler> MaterialHandler;
	TUniquePtr<FUltimateControlAnimationHandler> AnimationHandler;
	TUniquePtr<FUltimateControlSequencerHandler> SequencerHandler;
	TUniquePtr<FUltimateControlAudioHandler> AudioHandler;
	TUniquePtr<FUltimateControlPhysicsHandler> PhysicsHandler;
	TUniquePtr<FUltimateControlLightingHandler> LightingHandler;
	TUniquePtr<FUltimateControlWorldPartitionHandler> WorldPartitionHandler;
	TUniquePtr<FUltimateControlNiagaraHandler> NiagaraHandler;
	TUniquePtr<FUltimateControlLandscapeHandler> LandscapeHandler;
	TUniquePtr<FUltimateControlAIHandler> AIHandler;
	TUniquePtr<FUltimateControlRenderHandler> RenderHandler;
	TUniquePtr<FUltimateControlOutlinerHandler> OutlinerHandler;
	TUniquePtr<FUltimateControlSourceControlHandler> SourceControlHandler;
	TUniquePtr<FUltimateControlLiveCodingHandler> LiveCodingHandler;
	TUniquePtr<FUltimateControlSessionHandler> SessionHandler;
	TUniquePtr<FUltimateControlEditorHandler> EditorHandler;

	/** Pending confirmations for dangerous operations */
	TMap<FString, TSharedPtr<FJsonObject>> PendingConfirmations;

	/** Request statistics */
	int32 TotalRequestsHandled = 0;
	int32 TotalErrorsReturned = 0;
};
