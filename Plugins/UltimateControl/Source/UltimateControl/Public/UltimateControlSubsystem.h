// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpRouteHandle.h"
#include "Dom/JsonObject.h"
#include "UltimateControlTypes.h"

// Include all handler headers to ensure complete types are available for TUniquePtr
// This is required because UHT-generated code needs complete types for destructor
#include "Handlers/UltimateControlHandlerBase.h"
#include "Handlers/UltimateControlAssetHandler.h"
#include "Handlers/UltimateControlBlueprintHandler.h"
#include "Handlers/UltimateControlLevelHandler.h"
#include "Handlers/UltimateControlPIEHandler.h"
#include "Handlers/UltimateControlAutomationHandler.h"
#include "Handlers/UltimateControlProfilingHandler.h"
#include "Handlers/UltimateControlFileHandler.h"
#include "Handlers/UltimateControlConsoleHandler.h"
#include "Handlers/UltimateControlProjectHandler.h"
#include "Handlers/UltimateControlViewportHandler.h"
#include "Handlers/UltimateControlTransactionHandler.h"
#include "Handlers/UltimateControlMaterialHandler.h"
#include "Handlers/UltimateControlAnimationHandler.h"
#include "Handlers/UltimateControlSequencerHandler.h"
#include "Handlers/UltimateControlAudioHandler.h"
#include "Handlers/UltimateControlPhysicsHandler.h"
#include "Handlers/UltimateControlLightingHandler.h"
#include "Handlers/UltimateControlWorldPartitionHandler.h"
#include "Handlers/UltimateControlNiagaraHandler.h"
#include "Handlers/UltimateControlLandscapeHandler.h"
#include "Handlers/UltimateControlAIHandler.h"
#include "Handlers/UltimateControlRenderHandler.h"
#include "Handlers/UltimateControlOutlinerHandler.h"
#include "Handlers/UltimateControlSourceControlHandler.h"
#include "Handlers/UltimateControlLiveCodingHandler.h"
#include "Handlers/UltimateControlSessionHandler.h"
#include "Handlers/UltimateControlEditorHandler.h"

#include "UltimateControlSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUltimateControlServer, Log, All);

/**
 * Editor subsystem that provides HTTP JSON-RPC API for controlling Unreal Engine
 */
UCLASS()
class ULTIMATECONTROL_API UUltimateControlSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	/** Default constructor - must be declared in header and defined in cpp for TUniquePtr with forward declarations */
	UUltimateControlSubsystem();

	/** Destructor - must be declared in header but defined in cpp for TUniquePtr with forward declarations */
	virtual ~UUltimateControlSubsystem();

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
