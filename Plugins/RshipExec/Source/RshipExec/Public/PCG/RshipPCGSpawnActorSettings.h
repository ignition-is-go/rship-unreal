// Rship PCG Spawn Actor Settings
// Configuration object for PCG spawn actor binding
// NOTE: This is NOT a PCG graph node - use URshipPCGAutoBindComponent on actors instead

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "PCG/RshipPCGTypes.h"
#include "RshipPCGSpawnActorSettings.generated.h"

class URshipPCGAutoBindComponent;

// ============================================================================
// PCG SPAWN ACTOR CONFIGURATION
// ============================================================================

/**
 * Configuration settings for rShip PCG actor spawning.
 *
 * NOTE: Due to UHT limitations with conditional base classes, this is a UObject
 * rather than a UPCGSettings node. For PCG integration:
 *
 * 1. Use Unreal's standard PCG "Spawn Actor" node
 * 2. Add URshipPCGAutoBindComponent to your actor Blueprint
 * 3. The component will automatically register with rShip
 *
 * This class can be used as a configuration asset if needed.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), DisplayName = "Rship PCG Spawn Config")
class RSHIPEXEC_API URshipPCGSpawnActorSettings : public UObject
{
	GENERATED_BODY()

public:
	URshipPCGSpawnActorSettings();

	// ========================================================================
	// SETTINGS
	// ========================================================================

	/** The actor class to spawn */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Rship|Spawning")
	TSubclassOf<AActor> TemplateActorClass;

	/** Spawn collision handling */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Rship|Spawning")
	ESpawnActorCollisionHandlingMethod CollisionHandling = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	/** Attach spawned actors to this component */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Rship|Spawning")
	bool bAttachToComponent = false;

	// ========================================================================
	// RSHIP BINDING SETTINGS
	// ========================================================================

	/** Enable automatic rShip binding */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Rship|Binding")
	bool bEnableRshipBinding = true;

	/** Category for targets (appears in rShip UI) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Rship|Binding")
	FString TargetCategory = TEXT("PCG");

	/** Tags to add to all spawned targets */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Rship|Binding")
	TArray<FString> DefaultTags;

	/**
	 * Target naming pattern. Use placeholders:
	 * {class} - Actor class name
	 * {index} - Point index
	 * {source} - Source spline/volume name
	 * {x}, {y}, {z} - World position
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Rship|Binding")
	FString TargetNamePattern = TEXT("{class}_{index}");

	/** Include properties from sibling components */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Rship|Binding")
	bool bIncludeSiblingComponents = true;

	/** Include inherited properties (not just class-specific) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Rship|Binding")
	bool bIncludeInheritedProperties = false;

	/** Default pulse mode for readable properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Rship|Binding")
	ERshipPCGPulseMode DefaultPulseMode = ERshipPCGPulseMode::Off;

	/** Default pulse rate in Hz */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Rship|Binding", meta = (ClampMin = "0.1", ClampMax = "60.0"))
	float DefaultPulseRateHz = 10.0f;

	// ========================================================================
	// IDENTITY SETTINGS
	// ========================================================================

	/**
	 * PCG attribute to use as source identifier.
	 * If empty, uses the generating spline/volume actor name.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Rship|Identity")
	FName SourceKeyAttribute;

	/**
	 * PCG attribute containing point seed.
	 * If empty, uses PCG's internal seed.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Rship|Identity")
	FName SeedAttribute = TEXT("Seed");

	/**
	 * PCG attribute containing point index.
	 * If empty, uses iteration order.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Rship|Identity")
	FName PointIndexAttribute;

	/**
	 * PCG attribute containing distance along source (for splines).
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Rship|Identity")
	FName DistanceAttribute = TEXT("Distance");

	/**
	 * PCG attribute containing alpha/progress along source.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Rship|Identity")
	FName AlphaAttribute = TEXT("Alpha");
};
