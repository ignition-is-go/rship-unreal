// Copyright Rocketship. All Rights Reserved.

#include "UltimateControlSubsystem.h"
#include "UltimateControlSettings.h"
#include "UltimateControl.h"

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

#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpPath.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Guid.h"
#include "Editor.h"

DEFINE_LOG_CATEGORY(LogUltimateControlServer);

void UUltimateControlSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogUltimateControl, Log, TEXT("UltimateControlSubsystem initializing..."));

	// Initialize handlers
	InitializeHandlers();

	// Register system methods
	RegisterSystemMethods();

	// Auto-start server if configured
	const UUltimateControlSettings* Settings = UUltimateControlSettings::Get();
	if (Settings && Settings->bAutoStartServer)
	{
		StartServer();
	}
}

void UUltimateControlSubsystem::Deinitialize()
{
	StopServer();
	CleanupHandlers();
	RegisteredMethods.Empty();

	Super::Deinitialize();
}

UUltimateControlSubsystem::UUltimateControlSubsystem()
{
	// Default constructor defined here where complete types are available for TUniquePtr
}

UUltimateControlSubsystem::~UUltimateControlSubsystem()
{
	// Destructor defined here where complete types are available for TUniquePtr destruction
}

UUltimateControlSubsystem* UUltimateControlSubsystem::Get()
{
	if (GEditor)
	{
		return GEditor->GetEditorSubsystem<UUltimateControlSubsystem>();
	}
	return nullptr;
}

bool UUltimateControlSubsystem::StartServer()
{
	if (bServerRunning)
	{
		UE_LOG(LogUltimateControlServer, Warning, TEXT("Server is already running"));
		return true;
	}

	const UUltimateControlSettings* Settings = UUltimateControlSettings::Get();
	if (!Settings)
	{
		UE_LOG(LogUltimateControlServer, Error, TEXT("Failed to get settings"));
		return false;
	}

	FHttpServerModule& HttpServerModule = FHttpServerModule::Get();

	// Get or create router for our port
	HttpRouter = HttpServerModule.GetHttpRouter(Settings->ServerPort, /* bFailOnBindFailure */ true);
	if (!HttpRouter.IsValid())
	{
		UE_LOG(LogUltimateControlServer, Error, TEXT("Failed to create HTTP router on port %d"), Settings->ServerPort);
		return false;
	}

	// Bind the JSON-RPC endpoint
	auto Handler = [this](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
	{
		return HandleHttpRequest(Request, OnComplete);
	};

	RouteHandle = HttpRouter->BindRoute(
		FHttpPath(TEXT("/rpc")),
		EHttpServerRequestVerbs::VERB_POST | EHttpServerRequestVerbs::VERB_OPTIONS,
		FHttpRequestHandler::CreateLambda(Handler)
	);

	if (!RouteHandle.IsValid())
	{
		UE_LOG(LogUltimateControlServer, Error, TEXT("Failed to bind route"));
		HttpRouter.Reset();
		return false;
	}

	// Start listeners
	HttpServerModule.StartAllListeners();

	bServerRunning = true;
	UE_LOG(LogUltimateControlServer, Log, TEXT("UltimateControl server started on port %d"), Settings->ServerPort);
	UE_LOG(LogUltimateControlServer, Log, TEXT("Auth token: %s"), *Settings->AuthToken);

	return true;
}

void UUltimateControlSubsystem::StopServer()
{
	if (!bServerRunning)
	{
		return;
	}

	if (HttpRouter.IsValid() && RouteHandle.IsValid())
	{
		HttpRouter->UnbindRoute(RouteHandle);
	}

	HttpRouter.Reset();
	RouteHandle = FHttpRouteHandle();
	bServerRunning = false;

	UE_LOG(LogUltimateControlServer, Log, TEXT("UltimateControl server stopped"));
}

int32 UUltimateControlSubsystem::GetServerPort() const
{
	const UUltimateControlSettings* Settings = UUltimateControlSettings::Get();
	return Settings ? Settings->ServerPort : 7777;
}

FString UUltimateControlSubsystem::GetAuthToken() const
{
	const UUltimateControlSettings* Settings = UUltimateControlSettings::Get();
	return Settings ? Settings->AuthToken : TEXT("");
}

void UUltimateControlSubsystem::RegisterMethod(const FString& MethodName, const FJsonRpcMethodInfo& MethodInfo)
{
	RegisteredMethods.Add(MethodName, MethodInfo);
	UE_LOG(LogUltimateControlServer, Verbose, TEXT("Registered method: %s"), *MethodName);
}

void UUltimateControlSubsystem::UnregisterMethod(const FString& MethodName)
{
	RegisteredMethods.Remove(MethodName);
}

TSharedPtr<FJsonObject> UUltimateControlSubsystem::MakeError(int32 Code, const FString& Message, const TSharedPtr<FJsonValue>& Data)
{
	TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
	ErrorObj->SetNumberField(TEXT("code"), Code);
	ErrorObj->SetStringField(TEXT("message"), Message);
	if (Data.IsValid())
	{
		ErrorObj->SetField(TEXT("data"), Data);
	}
	return ErrorObj;
}

TSharedPtr<FJsonObject> UUltimateControlSubsystem::MakeResult(const TSharedPtr<FJsonValue>& Result, const TSharedPtr<FJsonValue>& Id)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
	Response->SetField(TEXT("result"), Result);
	Response->SetField(TEXT("id"), Id);
	return Response;
}

bool UUltimateControlSubsystem::HandleHttpRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	const UUltimateControlSettings* Settings = UUltimateControlSettings::Get();

	// Handle CORS preflight
	if (Request.Verb == EHttpServerRequestVerbs::VERB_OPTIONS)
	{
		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(FString(TEXT("")), TEXT("text/plain"));
		if (Settings && Settings->bEnableCORS)
		{
			Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), { Settings->CORSAllowedOrigins });
			Response->Headers.Add(TEXT("Access-Control-Allow-Methods"), { TEXT("POST, OPTIONS") });
			Response->Headers.Add(TEXT("Access-Control-Allow-Headers"), { TEXT("Content-Type, X-Ultimate-Control-Token") });
			Response->Headers.Add(TEXT("Access-Control-Max-Age"), { TEXT("86400") });
		}
		OnComplete(MoveTemp(Response));
		return true;
	}

	TotalRequestsHandled++;

	// Log request if enabled
	if (Settings && Settings->bLogRequests)
	{
		FString BodyStr = UTF8_TO_TCHAR(reinterpret_cast<const char*>(Request.Body.GetData()));
		UE_LOG(LogUltimateControlServer, Log, TEXT("Request: %s"), *BodyStr);
	}

	// Validate authentication
	TSharedPtr<FJsonObject> AuthError;
	if (!ValidateAuth(Request, AuthError))
	{
		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		Response->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
		Response->SetObjectField(TEXT("error"), AuthError);
		Response->SetField(TEXT("id"), MakeShared<FJsonValueNull>());

		FString ResponseStr;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseStr);
		FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);

		TUniquePtr<FHttpServerResponse> HttpResponse = FHttpServerResponse::Create(ResponseStr, TEXT("application/json"));
		HttpResponse->Code = EHttpServerResponseCodes::Denied;
		if (Settings && Settings->bEnableCORS)
		{
			HttpResponse->Headers.Add(TEXT("Access-Control-Allow-Origin"), { Settings->CORSAllowedOrigins });
		}
		OnComplete(MoveTemp(HttpResponse));
		TotalErrorsReturned++;
		return true;
	}

	// Parse JSON body
	FString BodyStr = UTF8_TO_TCHAR(reinterpret_cast<const char*>(Request.Body.GetData()));
	TSharedPtr<FJsonObject> RequestObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyStr);

	TSharedPtr<FJsonObject> ResponseObj;

	if (!FJsonSerializer::Deserialize(Reader, RequestObj) || !RequestObj.IsValid())
	{
		ResponseObj = MakeShared<FJsonObject>();
		ResponseObj->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
		ResponseObj->SetObjectField(TEXT("error"), MakeError(EJsonRpcError::ParseError, TEXT("Parse error")));
		ResponseObj->SetField(TEXT("id"), MakeShared<FJsonValueNull>());
		TotalErrorsReturned++;
	}
	else
	{
		// Check if it's a batch request (array)
		// For now, handle single requests
		ResponseObj = ProcessJsonRpcRequest(RequestObj);
	}

	// Serialize response
	FString ResponseStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseStr);
	FJsonSerializer::Serialize(ResponseObj.ToSharedRef(), Writer);

	if (Settings && Settings->bLogResponses)
	{
		UE_LOG(LogUltimateControlServer, Log, TEXT("Response: %s"), *ResponseStr);
	}

	TUniquePtr<FHttpServerResponse> HttpResponse = FHttpServerResponse::Create(ResponseStr, TEXT("application/json"));
	if (Settings && Settings->bEnableCORS)
	{
		HttpResponse->Headers.Add(TEXT("Access-Control-Allow-Origin"), { Settings->CORSAllowedOrigins });
	}
	OnComplete(MoveTemp(HttpResponse));

	return true;
}

TSharedPtr<FJsonObject> UUltimateControlSubsystem::ProcessJsonRpcRequest(const TSharedPtr<FJsonObject>& RequestObj)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));

	// Get request ID
	TSharedPtr<FJsonValue> IdValue = RequestObj->TryGetField(TEXT("id"));
	if (IdValue.IsValid())
	{
		Response->SetField(TEXT("id"), IdValue);
	}
	else
	{
		Response->SetField(TEXT("id"), MakeShared<FJsonValueNull>());
	}

	// Validate jsonrpc version
	FString JsonRpcVersion;
	if (!RequestObj->TryGetStringField(TEXT("jsonrpc"), JsonRpcVersion) || JsonRpcVersion != TEXT("2.0"))
	{
		Response->SetObjectField(TEXT("error"), MakeError(EJsonRpcError::InvalidRequest, TEXT("Invalid JSON-RPC version")));
		TotalErrorsReturned++;
		return Response;
	}

	// Get method name
	FString MethodName;
	if (!RequestObj->TryGetStringField(TEXT("method"), MethodName))
	{
		Response->SetObjectField(TEXT("error"), MakeError(EJsonRpcError::InvalidRequest, TEXT("Missing method")));
		TotalErrorsReturned++;
		return Response;
	}

	// Find method handler
	FJsonRpcMethodInfo* MethodInfo = RegisteredMethods.Find(MethodName);
	if (!MethodInfo)
	{
		Response->SetObjectField(TEXT("error"), MakeError(EJsonRpcError::MethodNotFound, FString::Printf(TEXT("Method not found: %s"), *MethodName)));
		TotalErrorsReturned++;
		return Response;
	}

	// Get params (can be object, array, or omitted)
	TSharedPtr<FJsonObject> Params = RequestObj->GetObjectField(TEXT("params"));
	if (!Params.IsValid())
	{
		Params = MakeShared<FJsonObject>();
	}

	// Execute handler
	TSharedPtr<FJsonValue> Result;
	TSharedPtr<FJsonObject> Error;

	bool bSuccess = MethodInfo->Handler.Execute(Params, Result, Error);

	if (bSuccess)
	{
		if (Result.IsValid())
		{
			Response->SetField(TEXT("result"), Result);
		}
		else
		{
			Response->SetField(TEXT("result"), MakeShared<FJsonValueNull>());
		}
	}
	else
	{
		if (Error.IsValid())
		{
			Response->SetObjectField(TEXT("error"), Error);
		}
		else
		{
			Response->SetObjectField(TEXT("error"), MakeError(EJsonRpcError::InternalError, TEXT("Unknown error")));
		}
		TotalErrorsReturned++;
	}

	return Response;
}

bool UUltimateControlSubsystem::ValidateAuth(const FHttpServerRequest& Request, TSharedPtr<FJsonObject>& OutError)
{
	const UUltimateControlSettings* Settings = UUltimateControlSettings::Get();
	if (!Settings || !Settings->bRequireAuth)
	{
		return true;
	}

	// Check for auth token header
	const TArray<FString>* TokenHeader = Request.Headers.Find(TEXT("X-Ultimate-Control-Token"));
	if (!TokenHeader || TokenHeader->Num() == 0)
	{
		OutError = MakeError(EJsonRpcError::Unauthorized, TEXT("Missing authentication token"));
		return false;
	}

	if ((*TokenHeader)[0] != Settings->AuthToken)
	{
		OutError = MakeError(EJsonRpcError::Unauthorized, TEXT("Invalid authentication token"));
		return false;
	}

	return true;
}

void UUltimateControlSubsystem::InitializeHandlers()
{
	const UUltimateControlSettings* Settings = UUltimateControlSettings::Get();
	if (!Settings)
	{
		return;
	}

	// Always initialize core handlers
	ProjectHandler = MakeUnique<FUltimateControlProjectHandler>(this);
	TransactionHandler = MakeUnique<FUltimateControlTransactionHandler>(this);
	OutlinerHandler = MakeUnique<FUltimateControlOutlinerHandler>(this);
	EditorHandler = MakeUnique<FUltimateControlEditorHandler>(this);

	if (Settings->bEnableAssetTools)
	{
		AssetHandler = MakeUnique<FUltimateControlAssetHandler>(this);
		MaterialHandler = MakeUnique<FUltimateControlMaterialHandler>(this);
	}

	if (Settings->bEnableBlueprintTools)
	{
		BlueprintHandler = MakeUnique<FUltimateControlBlueprintHandler>(this);
	}

	if (Settings->bEnableLevelTools)
	{
		LevelHandler = MakeUnique<FUltimateControlLevelHandler>(this);
		ViewportHandler = MakeUnique<FUltimateControlViewportHandler>(this);
		LightingHandler = MakeUnique<FUltimateControlLightingHandler>(this);
		WorldPartitionHandler = MakeUnique<FUltimateControlWorldPartitionHandler>(this);
		LandscapeHandler = MakeUnique<FUltimateControlLandscapeHandler>(this);
		RenderHandler = MakeUnique<FUltimateControlRenderHandler>(this);
		PhysicsHandler = MakeUnique<FUltimateControlPhysicsHandler>(this);
		AIHandler = MakeUnique<FUltimateControlAIHandler>(this);
	}

	if (Settings->bEnablePIETools)
	{
		PIEHandler = MakeUnique<FUltimateControlPIEHandler>(this);
	}

	if (Settings->bEnableAutomationTools)
	{
		AutomationHandler = MakeUnique<FUltimateControlAutomationHandler>(this);
	}

	if (Settings->bEnableProfilingTools)
	{
		ProfilingHandler = MakeUnique<FUltimateControlProfilingHandler>(this);
	}

	if (Settings->bEnableFileTools)
	{
		FileHandler = MakeUnique<FUltimateControlFileHandler>(this);
	}

	if (Settings->bEnableConsoleCommands)
	{
		ConsoleHandler = MakeUnique<FUltimateControlConsoleHandler>(this);
	}

	// Animation and Sequencer handlers
	AnimationHandler = MakeUnique<FUltimateControlAnimationHandler>(this);
	SequencerHandler = MakeUnique<FUltimateControlSequencerHandler>(this);

	// Audio handler
	AudioHandler = MakeUnique<FUltimateControlAudioHandler>(this);

	// VFX handler
	NiagaraHandler = MakeUnique<FUltimateControlNiagaraHandler>(this);

	// Source control handler
	SourceControlHandler = MakeUnique<FUltimateControlSourceControlHandler>(this);

	// Live coding handler
	LiveCodingHandler = MakeUnique<FUltimateControlLiveCodingHandler>(this);

	// Multi-user session handler
	SessionHandler = MakeUnique<FUltimateControlSessionHandler>(this);

	UE_LOG(LogUltimateControlServer, Log, TEXT("Initialized %d handler categories"), 27);
}

void UUltimateControlSubsystem::CleanupHandlers()
{
	// Reset all handler instances
	AssetHandler.Reset();
	BlueprintHandler.Reset();
	LevelHandler.Reset();
	PIEHandler.Reset();
	AutomationHandler.Reset();
	ProfilingHandler.Reset();
	FileHandler.Reset();
	ConsoleHandler.Reset();
	ProjectHandler.Reset();
	ViewportHandler.Reset();
	TransactionHandler.Reset();
	MaterialHandler.Reset();
	AnimationHandler.Reset();
	SequencerHandler.Reset();
	AudioHandler.Reset();
	PhysicsHandler.Reset();
	LightingHandler.Reset();
	WorldPartitionHandler.Reset();
	NiagaraHandler.Reset();
	LandscapeHandler.Reset();
	AIHandler.Reset();
	RenderHandler.Reset();
	OutlinerHandler.Reset();
	SourceControlHandler.Reset();
	LiveCodingHandler.Reset();
	SessionHandler.Reset();
	EditorHandler.Reset();
}

void UUltimateControlSubsystem::RegisterSystemMethods()
{
	// system.listMethods - List all available methods
	{
		FJsonRpcMethodInfo Info;
		Info.Name = TEXT("system.listMethods");
		Info.Description = TEXT("List all available JSON-RPC methods");
		Info.Category = TEXT("System");

		Info.Handler.BindLambda([this](const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError) -> bool
		{
			TArray<TSharedPtr<FJsonValue>> MethodsArray;

			for (const auto& Pair : RegisteredMethods)
			{
				TSharedPtr<FJsonObject> MethodObj = MakeShared<FJsonObject>();
				MethodObj->SetStringField(TEXT("name"), Pair.Key);
				MethodObj->SetStringField(TEXT("description"), Pair.Value.Description);
				MethodObj->SetStringField(TEXT("category"), Pair.Value.Category);
				MethodObj->SetBoolField(TEXT("dangerous"), Pair.Value.bIsDangerous);
				MethodObj->SetBoolField(TEXT("requiresConfirmation"), Pair.Value.bRequiresConfirmation);

				if (Pair.Value.ParamsSchema.IsValid())
				{
					MethodObj->SetObjectField(TEXT("params"), Pair.Value.ParamsSchema);
				}
				if (Pair.Value.ResultSchema.IsValid())
				{
					MethodObj->SetObjectField(TEXT("result"), Pair.Value.ResultSchema);
				}

				MethodsArray.Add(MakeShared<FJsonValueObject>(MethodObj));
			}

			OutResult = MakeShared<FJsonValueArray>(MethodsArray);
			return true;
		});

		RegisterMethod(TEXT("system.listMethods"), Info);
	}

	// system.getInfo - Get server info
	{
		FJsonRpcMethodInfo Info;
		Info.Name = TEXT("system.getInfo");
		Info.Description = TEXT("Get information about the UltimateControl server and Unreal Engine");
		Info.Category = TEXT("System");

		Info.Handler.BindLambda([this](const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError) -> bool
		{
			TSharedPtr<FJsonObject> InfoObj = MakeShared<FJsonObject>();

			InfoObj->SetStringField(TEXT("serverVersion"), TEXT("1.0.0"));
			InfoObj->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString());
			InfoObj->SetStringField(TEXT("platform"), FPlatformProperties::IniPlatformName());
			InfoObj->SetNumberField(TEXT("port"), GetServerPort());
			InfoObj->SetBoolField(TEXT("isRunning"), IsServerRunning());
			InfoObj->SetNumberField(TEXT("totalRequestsHandled"), TotalRequestsHandled);
			InfoObj->SetNumberField(TEXT("totalErrorsReturned"), TotalErrorsReturned);
			InfoObj->SetNumberField(TEXT("registeredMethods"), RegisteredMethods.Num());

			// Feature flags
			const UUltimateControlSettings* Settings = UUltimateControlSettings::Get();
			if (Settings)
			{
				TSharedPtr<FJsonObject> Features = MakeShared<FJsonObject>();
				Features->SetBoolField(TEXT("assetTools"), Settings->bEnableAssetTools);
				Features->SetBoolField(TEXT("blueprintTools"), Settings->bEnableBlueprintTools);
				Features->SetBoolField(TEXT("levelTools"), Settings->bEnableLevelTools);
				Features->SetBoolField(TEXT("pieTools"), Settings->bEnablePIETools);
				Features->SetBoolField(TEXT("automationTools"), Settings->bEnableAutomationTools);
				Features->SetBoolField(TEXT("profilingTools"), Settings->bEnableProfilingTools);
				Features->SetBoolField(TEXT("fileTools"), Settings->bEnableFileTools);
				Features->SetBoolField(TEXT("consoleCommands"), Settings->bEnableConsoleCommands);
				InfoObj->SetObjectField(TEXT("features"), Features);
			}

			OutResult = MakeShared<FJsonValueObject>(InfoObj);
			return true;
		});

		RegisterMethod(TEXT("system.getInfo"), Info);
	}

	// system.echo - Echo back params (for testing)
	{
		FJsonRpcMethodInfo Info;
		Info.Name = TEXT("system.echo");
		Info.Description = TEXT("Echo back the provided parameters (for testing connectivity)");
		Info.Category = TEXT("System");

		Info.Handler.BindLambda([](const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError) -> bool
		{
			OutResult = MakeShared<FJsonValueObject>(Params);
			return true;
		});

		RegisterMethod(TEXT("system.echo"), Info);
	}
}
