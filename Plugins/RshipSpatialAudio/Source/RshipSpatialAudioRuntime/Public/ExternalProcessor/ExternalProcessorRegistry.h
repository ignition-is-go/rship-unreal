// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IExternalSpatialProcessor.h"
#include "ExternalProcessorRegistry.generated.h"

// Forward declarations
class FDS100Processor;

/**
 * Factory and registry for external spatial audio processors.
 *
 * Manages processor instances, provides factory methods for creating
 * processors by type, and supports multiple simultaneous processor connections.
 *
 * Features:
 * - Processor creation by type
 * - Centralized processor management
 * - Multi-processor support (e.g., redundant DS100s)
 * - Global accessor for shared instances
 *
 * Usage:
 *   // Get or create a processor
 *   FExternalProcessorConfig Config;
 *   Config.ProcessorType = EExternalProcessorType::DS100;
 *   Config.Network.Host = TEXT("192.168.1.100");
 *
 *   IExternalSpatialProcessor* Processor = GetGlobalProcessorRegistry().GetOrCreateProcessor(Config);
 *   Processor->Connect();
 *
 * Thread Safety:
 * - Factory methods and registration are thread-safe
 * - Individual processor operations follow processor thread-safety rules
 */
UCLASS(BlueprintType)
class RSHIPSPATIALAUDIORUNTIME_API UExternalProcessorRegistry : public UObject
{
	GENERATED_BODY()

public:
	UExternalProcessorRegistry();
	virtual ~UExternalProcessorRegistry();

	// ========================================================================
	// FACTORY METHODS
	// ========================================================================

	/**
	 * Create a new processor of the specified type.
	 * The registry does NOT take ownership - caller owns the returned processor.
	 *
	 * @param Type The processor type to create.
	 * @return New processor instance, or nullptr if type is unsupported.
	 */
	static IExternalSpatialProcessor* CreateProcessor(EExternalProcessorType Type);

	/**
	 * Create and initialize a processor with the given configuration.
	 * The registry does NOT take ownership.
	 *
	 * @param Config Processor configuration.
	 * @return Initialized processor, or nullptr if creation/initialization failed.
	 */
	static TUniquePtr<IExternalSpatialProcessor> CreateConfiguredProcessor(const FExternalProcessorConfig& Config);

	// ========================================================================
	// MANAGED PROCESSORS
	// ========================================================================

	/**
	 * Get or create a managed processor with the given configuration.
	 * The registry owns and manages the returned processor.
	 *
	 * If a processor with the same host:port already exists, returns that instance.
	 *
	 * @param Config Processor configuration.
	 * @return Managed processor pointer (owned by registry).
	 */
	IExternalSpatialProcessor* GetOrCreateProcessor(const FExternalProcessorConfig& Config);

	/**
	 * Get a managed processor by ID.
	 *
	 * @param ProcessorId Processor identifier.
	 * @return Processor pointer, or nullptr if not found.
	 */
	IExternalSpatialProcessor* GetProcessor(const FString& ProcessorId) const;

	/**
	 * Get a managed processor by type and host.
	 *
	 * @param Type Processor type.
	 * @param Host Host address.
	 * @return Processor pointer, or nullptr if not found.
	 */
	IExternalSpatialProcessor* GetProcessorByHost(EExternalProcessorType Type, const FString& Host) const;

	/**
	 * Get all managed processors.
	 */
	TArray<IExternalSpatialProcessor*> GetAllProcessors() const;

	/**
	 * Get all managed processors of a specific type.
	 */
	TArray<IExternalSpatialProcessor*> GetProcessorsByType(EExternalProcessorType Type) const;

	/**
	 * Remove and destroy a managed processor.
	 *
	 * @param ProcessorId Processor identifier.
	 * @return True if processor was found and removed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|ExternalProcessor")
	bool RemoveProcessor(const FString& ProcessorId);

	/**
	 * Remove and destroy all managed processors.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|ExternalProcessor")
	void RemoveAllProcessors();

	/**
	 * Check if a processor exists.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|ExternalProcessor")
	bool HasProcessor(const FString& ProcessorId) const;

	/**
	 * Get the number of managed processors.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|ExternalProcessor")
	int32 GetProcessorCount() const;

	// ========================================================================
	// CONNECTION MANAGEMENT
	// ========================================================================

	/**
	 * Connect all managed processors.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|ExternalProcessor")
	void ConnectAll();

	/**
	 * Disconnect all managed processors.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|ExternalProcessor")
	void DisconnectAll();

	/**
	 * Get the connection status of all processors.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|ExternalProcessor")
	TMap<FString, EProcessorConnectionState> GetAllConnectionStates() const;

	// ========================================================================
	// TYPE INFORMATION
	// ========================================================================

	/**
	 * Get human-readable name for a processor type.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|ExternalProcessor")
	static FString GetProcessorTypeName(EExternalProcessorType Type);

	/**
	 * Get description for a processor type.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|ExternalProcessor")
	static FString GetProcessorTypeDescription(EExternalProcessorType Type);

	/**
	 * Check if a processor type is supported/implemented.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|ExternalProcessor")
	static bool IsProcessorTypeSupported(EExternalProcessorType Type);

	/**
	 * Get list of all supported processor types.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rship|SpatialAudio|ExternalProcessor")
	static TArray<EExternalProcessorType> GetSupportedProcessorTypes();

	// ========================================================================
	// EVENTS
	// ========================================================================

	/** Called when any managed processor's connection state changes */
	UPROPERTY(BlueprintAssignable, Category = "Rship|SpatialAudio|ExternalProcessor")
	FOnProcessorConnectionStateChanged OnProcessorConnectionStateChanged;

	/** Called when any managed processor encounters an error */
	UPROPERTY(BlueprintAssignable, Category = "Rship|SpatialAudio|ExternalProcessor")
	FOnProcessorError OnProcessorError;

private:
	// ========================================================================
	// INTERNAL STATE
	// ========================================================================

	/** Managed processor instances by ID */
	TMap<FString, TUniquePtr<IExternalSpatialProcessor>> ManagedProcessors;

	/** Thread safety */
	mutable FCriticalSection ProcessorsLock;

	/**
	 * Generate a processor ID from configuration.
	 */
	static FString GenerateProcessorId(const FExternalProcessorConfig& Config);

	/**
	 * Bind processor events.
	 */
	void BindProcessorEvents(IExternalSpatialProcessor* Processor, const FString& ProcessorId);
};

/**
 * Global accessor for the default processor registry.
 * Creates the registry if it doesn't exist.
 */
RSHIPSPATIALAUDIORUNTIME_API UExternalProcessorRegistry* GetGlobalProcessorRegistry();

/**
 * Get or create the processor registry singleton.
 * Alternative accessor that ensures the registry is properly initialized.
 */
RSHIPSPATIALAUDIORUNTIME_API UExternalProcessorRegistry& GetProcessorRegistryChecked();
