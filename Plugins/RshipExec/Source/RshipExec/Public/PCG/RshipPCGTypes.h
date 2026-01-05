// Rship PCG Auto-Bind Types
// Core types for PCG-spawned actor binding to rShip

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "RshipPCGTypes.generated.h"

// ============================================================================
// PCG INSTANCE IDENTITY
// ============================================================================

/**
 * Deterministic identity for a PCG-spawned actor instance.
 * Provides stable identification across PCG regeneration cycles.
 *
 * ID Scheme:
 *   TargetPath = "/pcg/{PCGComponentGuid}/{SourceKey}/{PointKey}"
 *   PointKey = Hash(PointIndex, QuantizedDistance, Seed)
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipPCGInstanceId
{
	GENERATED_BODY()

	/** Full path for rShip target identification (e.g., "/pcg/{guid}/{source}/{pointkey}") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|PCG")
	FString TargetPath;

	/** Stable GUID derived from PCG point data for identity tracking */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|PCG")
	FGuid StableGuid;

	/** GUID of the PCG component that spawned this actor */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|PCG")
	FGuid PCGComponentGuid;

	/** Identifier for the source (spline, volume, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|PCG")
	FString SourceKey;

	/** Point index within the source (if available) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|PCG")
	int32 PointIndex = -1;

	/** Distance along spline/path (quantized to avoid float precision issues) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|PCG")
	int64 QuantizedDistance = 0;

	/** Alpha/progress along spline (0-1, quantized) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|PCG")
	int32 QuantizedAlpha = 0;

	/** PCG seed value if available */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|PCG")
	int32 Seed = 0;

	/** User-friendly display name for the target */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|PCG")
	FString DisplayName;

	FRshipPCGInstanceId() = default;

	/** Generate stable GUID from point data */
	void GenerateStableGuid();

	/** Build the target path from components */
	void BuildTargetPath();

	/** Check if this is a valid identity */
	bool IsValid() const;

	/** Comparison operators */
	bool operator==(const FRshipPCGInstanceId& Other) const;
	bool operator!=(const FRshipPCGInstanceId& Other) const { return !(*this == Other); }

	/** Hash function for TMap/TSet */
	friend uint32 GetTypeHash(const FRshipPCGInstanceId& Id);

	/** Create from PCG point metadata */
	static FRshipPCGInstanceId FromPCGPoint(
		const FGuid& PCGComponentGuid,
		const FString& SourceKey,
		int32 PointIndex,
		double DistanceAlong,
		double Alpha,
		int32 Seed,
		const FString& OptionalDisplayName = TEXT(""));
};

// ============================================================================
// PROPERTY BINDING TYPES
// ============================================================================

/** Access mode for a bound property */
UENUM(BlueprintType)
enum class ERshipPCGPropertyAccess : uint8
{
	/** Property can only be read (pulse emission) */
	ReadOnly		UMETA(DisplayName = "Read Only"),

	/** Property can only be written (action reception) */
	WriteOnly		UMETA(DisplayName = "Write Only"),

	/** Property can be both read and written */
	ReadWrite		UMETA(DisplayName = "Read/Write")
};

/** Pulse emission mode for readable properties */
UENUM(BlueprintType)
enum class ERshipPCGPulseMode : uint8
{
	/** No automatic pulse emission */
	Off				UMETA(DisplayName = "Off"),

	/** Emit pulse only when value changes */
	OnChange		UMETA(DisplayName = "On Change"),

	/** Emit pulse at a fixed rate */
	FixedRate		UMETA(DisplayName = "Fixed Rate")
};

/** Type category for bound properties */
UENUM(BlueprintType)
enum class ERshipPCGPropertyType : uint8
{
	Unknown,
	Bool,
	Int32,
	Int64,
	Float,
	Double,
	Vector,
	Vector2D,
	Vector4,
	Rotator,
	Transform,
	Quat,
	LinearColor,
	Color,
	String,
	Name,
	Text,
	Enum,
	Struct,
	Object
};

/**
 * Descriptor for a single bound property.
 * Cached per-class for efficient per-instance binding.
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipPCGPropertyDescriptor
{
	GENERATED_BODY()

	/** Property name (as defined in UPROPERTY) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|PCG")
	FName PropertyName;

	/** Display name for rShip UI (from metadata or generated) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|PCG")
	FString DisplayName;

	/** Category for organizing in rShip UI */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|PCG")
	FString Category;

	/** Property type */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|PCG")
	ERshipPCGPropertyType PropertyType = ERshipPCGPropertyType::Unknown;

	/** Access mode (read/write/both) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|PCG")
	ERshipPCGPropertyAccess Access = ERshipPCGPropertyAccess::ReadWrite;

	/** Pulse mode for readable properties */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|PCG")
	ERshipPCGPulseMode PulseMode = ERshipPCGPulseMode::Off;

	/** Pulse rate in Hz (for FixedRate mode) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|PCG", meta = (ClampMin = "0.1", ClampMax = "60.0"))
	float PulseRateHz = 10.0f;

	/** Minimum value (optional, for UI and clamping) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|PCG")
	float MinValue = 0.0f;

	/** Maximum value (optional, for UI and clamping) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|PCG")
	float MaxValue = 1.0f;

	/** Whether min/max are specified */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|PCG")
	bool bHasRange = false;

	/** Description for rShip UI */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|PCG")
	FString Description;

	/** UE type name (for complex types) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|PCG")
	FString UnrealTypeName;

	/** Enum class path (for enum types) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|PCG")
	FString EnumPath;

	/** Offset of property in container (cached for fast access) */
	int32 PropertyOffset = 0;

	/** Pointer to the FProperty (not serialized, rebuilt on load) */
	FProperty* CachedProperty = nullptr;

	FRshipPCGPropertyDescriptor() = default;

	/** Create from FProperty with metadata parsing */
	static FRshipPCGPropertyDescriptor FromProperty(FProperty* Property);

	/** Determine property type from FProperty */
	static ERshipPCGPropertyType DeterminePropertyType(FProperty* Property);

	/** Parse metadata from property */
	void ParseMetadata(FProperty* Property);

	/** Check if this binding is valid */
	bool IsValid() const { return CachedProperty != nullptr && PropertyType != ERshipPCGPropertyType::Unknown; }

	/** Get JSON schema type string */
	FString GetJsonSchemaType() const;
};

/**
 * Cached property bindings for a UClass.
 * Built once per class type, shared by all instances.
 */
USTRUCT()
struct RSHIPEXEC_API FRshipPCGClassBindings
{
	GENERATED_BODY()

	/** Class these bindings are for */
	UPROPERTY()
	TWeakObjectPtr<UClass> BoundClass;

	/** All bindable properties */
	UPROPERTY()
	TArray<FRshipPCGPropertyDescriptor> Properties;

	/** Timestamp of last rebuild (for cache invalidation) */
	double LastBuildTime = 0.0;

	/** Whether bindings are valid */
	bool bIsValid = false;

	FRshipPCGClassBindings() = default;

	/** Build bindings from a class */
	void BuildFromClass(UClass* InClass);

	/** Rebuild property pointers after load */
	void RebuildPropertyPointers();

	/** Find property by name */
	FRshipPCGPropertyDescriptor* FindProperty(FName PropertyName);
	const FRshipPCGPropertyDescriptor* FindProperty(FName PropertyName) const;

	/** Get all readable properties */
	TArray<FRshipPCGPropertyDescriptor*> GetReadableProperties();

	/** Get all writable properties */
	TArray<FRshipPCGPropertyDescriptor*> GetWritableProperties();
};

// ============================================================================
// RUNTIME INSTANCE STATE
// ============================================================================

/**
 * Runtime state for a single bound property on an instance.
 * Tracks values for change detection and pulse timing.
 */
struct RSHIPEXEC_API FRshipPCGPropertyState
{
	/** Index into class bindings array */
	int32 DescriptorIndex = -1;

	/** Last known value (for change detection) */
	TArray<uint8> LastValueBytes;

	/** Time of last pulse emission */
	double LastPulseTime = 0.0;

	/** Time of next scheduled pulse (for FixedRate) */
	double NextPulseTime = 0.0;

	/** Has the value changed since last pulse? */
	bool bValueChanged = false;

	FRshipPCGPropertyState() = default;

	/** Check if value has changed */
	bool HasValueChanged(const void* CurrentValue, int32 ValueSize) const;

	/** Update stored value */
	void UpdateValue(const void* CurrentValue, int32 ValueSize);
};

/**
 * Complete runtime state for a bound instance.
 */
struct RSHIPEXEC_API FRshipPCGInstanceState
{
	/** Instance identity */
	FRshipPCGInstanceId InstanceId;

	/** Weak pointer to the actor */
	TWeakObjectPtr<AActor> Actor;

	/** Weak pointer to the binding component */
	TWeakObjectPtr<class URshipPCGAutoBindComponent> BindingComponent;

	/** Per-property state */
	TArray<FRshipPCGPropertyState> PropertyStates;

	/** Whether this instance is registered with rShip */
	bool bIsRegistered = false;

	/** Whether this instance is online (actor valid and world playing) */
	bool bIsOnline = false;

	/** Time of registration */
	double RegistrationTime = 0.0;

	FRshipPCGInstanceState() = default;

	/** Check if instance is valid */
	bool IsValid() const;
};

// ============================================================================
// PCG METADATA KEYS
// ============================================================================

/**
 * Standard metadata keys for rShip PCG bindings.
 * Use these in UPROPERTY meta specifiers.
 *
 * Examples:
 *   UPROPERTY(EditAnywhere, meta=(RShipParam))
 *   UPROPERTY(EditAnywhere, meta=(RShipParam="MyCustomName", RShipWritable="true"))
 *   UPROPERTY(EditAnywhere, meta=(RShipParam, RShipCategory="Lighting", RShipMin="0", RShipMax="1"))
 */
namespace RshipPCGMetaKeys
{
	/** Mark property for rShip binding (value = optional custom name) */
	static const FName Param = TEXT("RShipParam");

	/** Whether property is writable from rShip (default: true) */
	static const FName Writable = TEXT("RShipWritable");

	/** Whether property is readable from rShip (default: true) */
	static const FName Readable = TEXT("RShipReadable");

	/** Category for UI organization */
	static const FName Category = TEXT("RShipCategory");

	/** Minimum value for numeric types */
	static const FName Min = TEXT("RShipMin");

	/** Maximum value for numeric types */
	static const FName Max = TEXT("RShipMax");

	/** Description for UI tooltip */
	static const FName Description = TEXT("RShipDescription");

	/** Pulse mode: "off", "onchange", "fixedrate" */
	static const FName PulseMode = TEXT("RShipPulseMode");

	/** Pulse rate in Hz (for fixedrate mode) */
	static const FName PulseRate = TEXT("RShipPulseRate");
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

namespace RshipPCGUtils
{
	/** Convert FProperty value to JSON */
	RSHIPEXEC_API TSharedPtr<FJsonValue> PropertyToJson(
		FProperty* Property,
		const void* ContainerPtr);

	/** Convert JSON to FProperty value */
	RSHIPEXEC_API bool JsonToProperty(
		FProperty* Property,
		void* ContainerPtr,
		const TSharedPtr<FJsonValue>& JsonValue);

	/** Quantize distance to avoid floating point precision issues (mm resolution) */
	RSHIPEXEC_API int64 QuantizeDistance(double Distance);

	/** Quantize alpha (0-1) to integer (0-10000) */
	RSHIPEXEC_API int32 QuantizeAlpha(double Alpha);

	/** Generate deterministic hash from PCG point data */
	RSHIPEXEC_API uint32 HashPCGPoint(
		const FGuid& PCGComponentGuid,
		const FString& SourceKey,
		int32 PointIndex,
		int64 QuantizedDistance,
		int32 Seed);

	/** Check if a property has rShip metadata */
	RSHIPEXEC_API bool HasRshipMetadata(FProperty* Property);

	/** Parse rShip metadata from property */
	RSHIPEXEC_API void ParseRshipMetadata(
		FProperty* Property,
		FString& OutName,
		bool& bOutReadable,
		bool& bOutWritable,
		FString& OutCategory,
		float& OutMin,
		float& OutMax,
		bool& bOutHasRange,
		ERshipPCGPulseMode& OutPulseMode,
		float& OutPulseRate);
}
