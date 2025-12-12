// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UltimateControlSettings.generated.h"

/**
 * Settings for the Ultimate Control HTTP API server
 */
UCLASS(config = EditorPerProjectUserSettings, defaultconfig, meta = (DisplayName = "Ultimate Control"))
class ULTIMATECONTROL_API UUltimateControlSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UUltimateControlSettings();

	//~ Begin UDeveloperSettings Interface
	virtual FName GetCategoryName() const override { return FName(TEXT("Plugins")); }
	virtual FName GetSectionName() const override { return FName(TEXT("UltimateControl")); }
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
	//~ End UDeveloperSettings Interface

	/** Get the singleton settings instance */
	static const UUltimateControlSettings* Get();
	static UUltimateControlSettings* GetMutable();

	// ==================== Server Settings ====================

	/** Whether to automatically start the HTTP server when the editor loads */
	UPROPERTY(config, EditAnywhere, Category = "Server", meta = (DisplayName = "Auto Start Server"))
	bool bAutoStartServer = true;

	/** The port for the HTTP JSON-RPC server */
	UPROPERTY(config, EditAnywhere, Category = "Server", meta = (DisplayName = "Server Port", ClampMin = "1024", ClampMax = "65535"))
	int32 ServerPort = 7777;

	/** Whether to bind only to localhost (127.0.0.1) or all interfaces (0.0.0.0) */
	UPROPERTY(config, EditAnywhere, Category = "Server", meta = (DisplayName = "Localhost Only"))
	bool bLocalhostOnly = true;

	// ==================== Security Settings ====================

	/** Whether to require authentication for API requests */
	UPROPERTY(config, EditAnywhere, Category = "Security", meta = (DisplayName = "Require Authentication"))
	bool bRequireAuth = true;

	/**
	 * The secret token for API authentication.
	 * Clients must send this in the X-Ultimate-Control-Token header.
	 * If empty, a random token will be generated on first run.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Security", meta = (DisplayName = "Auth Token", PasswordField = true))
	FString AuthToken;

	/** Maximum number of concurrent connections allowed */
	UPROPERTY(config, EditAnywhere, Category = "Security", meta = (DisplayName = "Max Connections", ClampMin = "1", ClampMax = "100"))
	int32 MaxConnections = 10;

	// ==================== CORS Settings ====================

	/** Whether to enable CORS headers for browser-based clients */
	UPROPERTY(config, EditAnywhere, Category = "CORS", meta = (DisplayName = "Enable CORS"))
	bool bEnableCORS = true;

	/** Allowed origins for CORS (comma-separated, or * for all) */
	UPROPERTY(config, EditAnywhere, Category = "CORS", meta = (DisplayName = "Allowed Origins"))
	FString CORSAllowedOrigins = TEXT("*");

	// ==================== Timeout Settings ====================

	/** Request timeout in seconds */
	UPROPERTY(config, EditAnywhere, Category = "Timeouts", meta = (DisplayName = "Request Timeout (seconds)", ClampMin = "1", ClampMax = "300"))
	float RequestTimeoutSeconds = 30.0f;

	/** Maximum request body size in bytes (default 10MB) */
	UPROPERTY(config, EditAnywhere, Category = "Timeouts", meta = (DisplayName = "Max Request Body Size", ClampMin = "1024", ClampMax = "104857600"))
	int32 MaxRequestBodySize = 10485760;

	// ==================== Logging Settings ====================

	/** Verbosity level for logging (0=Errors only, 1=Warnings, 2=Info, 3=Verbose) */
	UPROPERTY(config, EditAnywhere, Category = "Logging", meta = (DisplayName = "Log Verbosity", ClampMin = "0", ClampMax = "3"))
	int32 LogVerbosity = 1;

	/** Whether to log all incoming requests */
	UPROPERTY(config, EditAnywhere, Category = "Logging", meta = (DisplayName = "Log Requests"))
	bool bLogRequests = false;

	/** Whether to log all responses */
	UPROPERTY(config, EditAnywhere, Category = "Logging", meta = (DisplayName = "Log Responses"))
	bool bLogResponses = false;

	// ==================== Feature Flags ====================

	/** Enable asset management tools */
	UPROPERTY(config, EditAnywhere, Category = "Features", meta = (DisplayName = "Enable Asset Tools"))
	bool bEnableAssetTools = true;

	/** Enable Blueprint modification tools */
	UPROPERTY(config, EditAnywhere, Category = "Features", meta = (DisplayName = "Enable Blueprint Tools"))
	bool bEnableBlueprintTools = true;

	/** Enable level/actor manipulation tools */
	UPROPERTY(config, EditAnywhere, Category = "Features", meta = (DisplayName = "Enable Level Tools"))
	bool bEnableLevelTools = true;

	/** Enable Play-In-Editor control tools */
	UPROPERTY(config, EditAnywhere, Category = "Features", meta = (DisplayName = "Enable PIE Tools"))
	bool bEnablePIETools = true;

	/** Enable automation/build tools */
	UPROPERTY(config, EditAnywhere, Category = "Features", meta = (DisplayName = "Enable Automation Tools"))
	bool bEnableAutomationTools = true;

	/** Enable profiling/logging tools */
	UPROPERTY(config, EditAnywhere, Category = "Features", meta = (DisplayName = "Enable Profiling Tools"))
	bool bEnableProfilingTools = true;

	/** Enable file system access tools */
	UPROPERTY(config, EditAnywhere, Category = "Features", meta = (DisplayName = "Enable File Tools"))
	bool bEnableFileTools = true;

	/** Enable console command execution */
	UPROPERTY(config, EditAnywhere, Category = "Features", meta = (DisplayName = "Enable Console Commands"))
	bool bEnableConsoleCommands = true;

	// ==================== Safety Settings ====================

	/**
	 * Dangerous operations require explicit confirmation.
	 * This includes: deleting assets, modifying source files, etc.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Safety", meta = (DisplayName = "Require Confirmation for Dangerous Ops"))
	bool bRequireConfirmationForDangerousOps = true;

	/** Create backups before modifying files */
	UPROPERTY(config, EditAnywhere, Category = "Safety", meta = (DisplayName = "Create Backups"))
	bool bCreateBackups = true;

	/** Maximum number of actors that can be modified in a single operation */
	UPROPERTY(config, EditAnywhere, Category = "Safety", meta = (DisplayName = "Max Actors Per Operation", ClampMin = "1", ClampMax = "10000"))
	int32 MaxActorsPerOperation = 1000;
};
