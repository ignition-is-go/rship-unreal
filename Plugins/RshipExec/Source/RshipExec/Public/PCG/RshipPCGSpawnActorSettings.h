// Rship PCG Spawn Actor Settings
// Custom PCG node for spawning actors with automatic rShip binding

#pragma once

#include "CoreMinimal.h"
#include "PCG/RshipPCGTypes.h"

#if RSHIP_HAS_PCG
#include "PCGSettings.h"
#include "PCGElement.h"
#include "Metadata/PCGMetadataAttribute.h"
#endif

#include "RshipPCGSpawnActorSettings.generated.h"

class URshipPCGAutoBindComponent;

// Forward declarations for PCG types when PCG is not available
#if !RSHIP_HAS_PCG
struct FPCGPoint;
class FPCGContext;
#endif

// ============================================================================
// PCG SPAWN ACTOR WITH RSHIP BINDING
// ============================================================================

/**
 * Custom PCG settings node that spawns actors with automatic rShip binding.
 *
 * This node is a drop-in replacement for the standard PCG "Spawn Actor" node.
 * It adds the URshipPCGAutoBindComponent to each spawned actor and configures
 * the deterministic instance ID from PCG point data.
 *
 * Features:
 * - Automatic attachment of URshipPCGAutoBindComponent
 * - Deterministic instance IDs from PCG point metadata
 * - Support for custom target naming patterns
 * - Tag inheritance from PCG attributes
 * - Batch registration for efficiency
 *
 * NOTE: This class requires the PCG plugin to be enabled for full functionality.
 * When PCG is disabled, the class exists but is non-functional.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), DisplayName = "Rship Spawn Actor")
class RSHIPEXEC_API URshipPCGSpawnActorSettings :
#if RSHIP_HAS_PCG
	public UPCGSettings
#else
	public UObject
#endif
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

	// ========================================================================
	// UPCGSETTINGS INTERFACE (PCG-only)
	// ========================================================================

#if RSHIP_HAS_PCG
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return TEXT("Rship Spawn Actor"); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("RshipPCG", "SpawnActorTitle", "Rship Spawn Actor"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
#endif // RSHIP_HAS_PCG
};

// ============================================================================
// PCG ELEMENT (EXECUTION) - Only available when PCG is enabled
// ============================================================================

#if RSHIP_HAS_PCG

/**
 * Execution element for the Rship Spawn Actor node.
 */
class RSHIPEXEC_API FRshipPCGSpawnActorElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

private:
	/** Spawn a single actor from a point */
	AActor* SpawnActorFromPoint(
		FPCGContext* Context,
		const URshipPCGSpawnActorSettings* Settings,
		const FPCGPoint& Point,
		int32 PointIndex,
		const FGuid& PCGComponentGuid,
		const FString& SourceKey) const;

	/** Build instance ID from point data */
	FRshipPCGInstanceId BuildInstanceId(
		const URshipPCGSpawnActorSettings* Settings,
		const FPCGPoint& Point,
		int32 PointIndex,
		const FPCGMetadataAttribute<int32>* SeedAttr,
		const FPCGMetadataAttribute<float>* DistanceAttr,
		const FPCGMetadataAttribute<float>* AlphaAttr,
		const FPCGMetadataAttribute<int32>* IndexAttr,
		const FGuid& PCGComponentGuid,
		const FString& SourceKey) const;

	/** Apply naming pattern to generate display name */
	FString ApplyNamingPattern(
		const URshipPCGSpawnActorSettings* Settings,
		const FPCGPoint& Point,
		int32 PointIndex,
		const FString& SourceKey) const;
};

#endif // RSHIP_HAS_PCG
