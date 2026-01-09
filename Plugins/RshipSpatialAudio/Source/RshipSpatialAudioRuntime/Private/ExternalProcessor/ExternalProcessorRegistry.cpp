// Copyright Rocketship. All Rights Reserved.

#include "ExternalProcessor/ExternalProcessorRegistry.h"
#include "ExternalProcessor/DS100Processor.h"
#include "Engine/Engine.h"

// Global registry instance
static TWeakObjectPtr<UExternalProcessorRegistry> GProcessorRegistry;

UExternalProcessorRegistry* GetGlobalProcessorRegistry()
{
	if (!GProcessorRegistry.IsValid())
	{
		GProcessorRegistry = NewObject<UExternalProcessorRegistry>(GetTransientPackage(), NAME_None, RF_MarkAsRootSet);
	}
	return GProcessorRegistry.Get();
}

UExternalProcessorRegistry& GetProcessorRegistryChecked()
{
	UExternalProcessorRegistry* Registry = GetGlobalProcessorRegistry();
	check(Registry);
	return *Registry;
}

// ============================================================================
// UExternalProcessorRegistry
// ============================================================================

UExternalProcessorRegistry::UExternalProcessorRegistry()
{
}

UExternalProcessorRegistry::~UExternalProcessorRegistry()
{
	RemoveAllProcessors();
}

IExternalSpatialProcessor* UExternalProcessorRegistry::CreateProcessor(EExternalProcessorType Type)
{
	switch (Type)
	{
	case EExternalProcessorType::DS100:
		return new FDS100Processor();

	case EExternalProcessorType::P1:
		// P1 uses same protocol as DS100
		return new FDS100Processor();

	case EExternalProcessorType::LISA:
		// L-ISA not yet implemented
		UE_LOG(LogTemp, Warning, TEXT("ProcessorRegistry: L-ISA processor not yet implemented"));
		return nullptr;

	case EExternalProcessorType::SpacemapGo:
		// Spacemap Go not yet implemented
		UE_LOG(LogTemp, Warning, TEXT("ProcessorRegistry: Spacemap Go processor not yet implemented"));
		return nullptr;

	case EExternalProcessorType::Custom:
		// Custom OSC - use DS100 base for now
		return new FDS100Processor();

	case EExternalProcessorType::None:
	default:
		return nullptr;
	}
}

TUniquePtr<IExternalSpatialProcessor> UExternalProcessorRegistry::CreateConfiguredProcessor(const FExternalProcessorConfig& Config)
{
	IExternalSpatialProcessor* RawProcessor = CreateProcessor(Config.ProcessorType);
	if (!RawProcessor)
	{
		return nullptr;
	}

	TUniquePtr<IExternalSpatialProcessor> Processor(RawProcessor);

	if (!Processor->Initialize(Config))
	{
		UE_LOG(LogTemp, Error, TEXT("ProcessorRegistry: Failed to initialize processor"));
		return nullptr;
	}

	return Processor;
}

IExternalSpatialProcessor* UExternalProcessorRegistry::GetOrCreateProcessor(const FExternalProcessorConfig& Config)
{
	FString ProcessorId = GenerateProcessorId(Config);

	FScopeLock Lock(&ProcessorsLock);

	// Check if already exists
	if (TUniquePtr<IExternalSpatialProcessor>* Existing = ManagedProcessors.Find(ProcessorId))
	{
		return Existing->Get();
	}

	// Create new processor
	TUniquePtr<IExternalSpatialProcessor> NewProcessor = CreateConfiguredProcessor(Config);
	if (!NewProcessor)
	{
		return nullptr;
	}

	IExternalSpatialProcessor* RawPtr = NewProcessor.Get();

	// Bind events before adding to map
	BindProcessorEvents(RawPtr, ProcessorId);

	ManagedProcessors.Add(ProcessorId, MoveTemp(NewProcessor));

	UE_LOG(LogTemp, Log, TEXT("ProcessorRegistry: Created managed processor '%s' (%s)"),
		*ProcessorId, *Config.DisplayName);

	return RawPtr;
}

IExternalSpatialProcessor* UExternalProcessorRegistry::GetProcessor(const FString& ProcessorId) const
{
	FScopeLock Lock(&ProcessorsLock);

	if (const TUniquePtr<IExternalSpatialProcessor>* Existing = ManagedProcessors.Find(ProcessorId))
	{
		return Existing->Get();
	}

	return nullptr;
}

IExternalSpatialProcessor* UExternalProcessorRegistry::GetProcessorByHost(EExternalProcessorType Type, const FString& Host) const
{
	FScopeLock Lock(&ProcessorsLock);

	for (const auto& Pair : ManagedProcessors)
	{
		if (Pair.Value->GetType() == Type)
		{
			const FExternalProcessorConfig& Config = Pair.Value->GetConfig();
			if (Config.Network.Host == Host)
			{
				return Pair.Value.Get();
			}
		}
	}

	return nullptr;
}

TArray<IExternalSpatialProcessor*> UExternalProcessorRegistry::GetAllProcessors() const
{
	FScopeLock Lock(&ProcessorsLock);

	TArray<IExternalSpatialProcessor*> Result;
	for (const auto& Pair : ManagedProcessors)
	{
		Result.Add(Pair.Value.Get());
	}
	return Result;
}

TArray<IExternalSpatialProcessor*> UExternalProcessorRegistry::GetProcessorsByType(EExternalProcessorType Type) const
{
	FScopeLock Lock(&ProcessorsLock);

	TArray<IExternalSpatialProcessor*> Result;
	for (const auto& Pair : ManagedProcessors)
	{
		if (Pair.Value->GetType() == Type)
		{
			Result.Add(Pair.Value.Get());
		}
	}
	return Result;
}

bool UExternalProcessorRegistry::RemoveProcessor(const FString& ProcessorId)
{
	FScopeLock Lock(&ProcessorsLock);

	if (TUniquePtr<IExternalSpatialProcessor>* Existing = ManagedProcessors.Find(ProcessorId))
	{
		(*Existing)->Shutdown();
		ManagedProcessors.Remove(ProcessorId);

		UE_LOG(LogTemp, Log, TEXT("ProcessorRegistry: Removed processor '%s'"), *ProcessorId);
		return true;
	}

	return false;
}

void UExternalProcessorRegistry::RemoveAllProcessors()
{
	FScopeLock Lock(&ProcessorsLock);

	for (auto& Pair : ManagedProcessors)
	{
		if (Pair.Value)
		{
			Pair.Value->Shutdown();
		}
	}

	ManagedProcessors.Empty();

	UE_LOG(LogTemp, Log, TEXT("ProcessorRegistry: Removed all processors"));
}

bool UExternalProcessorRegistry::HasProcessor(const FString& ProcessorId) const
{
	FScopeLock Lock(&ProcessorsLock);
	return ManagedProcessors.Contains(ProcessorId);
}

int32 UExternalProcessorRegistry::GetProcessorCount() const
{
	FScopeLock Lock(&ProcessorsLock);
	return ManagedProcessors.Num();
}

void UExternalProcessorRegistry::ConnectAll()
{
	FScopeLock Lock(&ProcessorsLock);

	for (auto& Pair : ManagedProcessors)
	{
		if (Pair.Value && !Pair.Value->IsConnected())
		{
			Pair.Value->Connect();
		}
	}
}

void UExternalProcessorRegistry::DisconnectAll()
{
	FScopeLock Lock(&ProcessorsLock);

	for (auto& Pair : ManagedProcessors)
	{
		if (Pair.Value && Pair.Value->IsConnected())
		{
			Pair.Value->Disconnect();
		}
	}
}

TMap<FString, EProcessorConnectionState> UExternalProcessorRegistry::GetAllConnectionStates() const
{
	FScopeLock Lock(&ProcessorsLock);

	TMap<FString, EProcessorConnectionState> Result;
	for (const auto& Pair : ManagedProcessors)
	{
		if (Pair.Value)
		{
			Result.Add(Pair.Key, Pair.Value->GetStatus().ConnectionState);
		}
	}
	return Result;
}

FString UExternalProcessorRegistry::GetProcessorTypeName(EExternalProcessorType Type)
{
	switch (Type)
	{
	case EExternalProcessorType::DS100:
		return TEXT("d&b DS100");
	case EExternalProcessorType::P1:
		return TEXT("d&b P1");
	case EExternalProcessorType::LISA:
		return TEXT("L-Acoustics L-ISA");
	case EExternalProcessorType::SpacemapGo:
		return TEXT("Meyer Spacemap Go");
	case EExternalProcessorType::Custom:
		return TEXT("Custom OSC");
	case EExternalProcessorType::None:
	default:
		return TEXT("None");
	}
}

FString UExternalProcessorRegistry::GetProcessorTypeDescription(EExternalProcessorType Type)
{
	switch (Type)
	{
	case EExternalProcessorType::DS100:
		return TEXT("d&b audiotechnik DS100 Signal Engine - Object-based audio processor with En-Space reverb.");
	case EExternalProcessorType::P1:
		return TEXT("d&b audiotechnik P1 Processor - Compact audio processor using DS100 protocol.");
	case EExternalProcessorType::LISA:
		return TEXT("L-Acoustics L-ISA Processor - Immersive sound art processor for object-based mixing.");
	case EExternalProcessorType::SpacemapGo:
		return TEXT("Meyer Spacemap Go - Real-time spatial sound design and mixing system.");
	case EExternalProcessorType::Custom:
		return TEXT("Custom OSC - Generic OSC-based processor with configurable addresses.");
	case EExternalProcessorType::None:
	default:
		return TEXT("No external processor.");
	}
}

bool UExternalProcessorRegistry::IsProcessorTypeSupported(EExternalProcessorType Type)
{
	switch (Type)
	{
	case EExternalProcessorType::DS100:
		return true;
	case EExternalProcessorType::P1:
		return true;  // Uses DS100 protocol
	case EExternalProcessorType::Custom:
		return true;  // Basic OSC support
	case EExternalProcessorType::LISA:
		return false;  // Not yet implemented
	case EExternalProcessorType::SpacemapGo:
		return false;  // Not yet implemented
	case EExternalProcessorType::None:
	default:
		return false;
	}
}

TArray<EExternalProcessorType> UExternalProcessorRegistry::GetSupportedProcessorTypes()
{
	return TArray<EExternalProcessorType>{
		EExternalProcessorType::DS100,
		EExternalProcessorType::P1,
		EExternalProcessorType::Custom
	};
}

FString UExternalProcessorRegistry::GenerateProcessorId(const FExternalProcessorConfig& Config)
{
	// Generate ID from type and network address
	return FString::Printf(TEXT("%s_%s_%d"),
		*GetProcessorTypeName(Config.ProcessorType).Replace(TEXT(" "), TEXT("")),
		*Config.Network.Host.Replace(TEXT("."), TEXT("_")),
		Config.Network.SendPort);
}

void UExternalProcessorRegistry::BindProcessorEvents(IExternalSpatialProcessor* Processor, const FString& ProcessorId)
{
	if (!Processor)
	{
		return;
	}

	// For now, events are bound via the base class
	// This is where we'd connect the processor's events to the registry's broadcasts

	// Note: We can't easily bind to FExternalSpatialProcessorBase delegates from here
	// because we only have the interface pointer. The derived class events are already
	// set up in Initialize(). For registry-level broadcasting, we'd need to add
	// a registration mechanism or use a different pattern.

	UE_LOG(LogTemp, Verbose, TEXT("ProcessorRegistry: Bound events for processor '%s'"), *ProcessorId);
}
