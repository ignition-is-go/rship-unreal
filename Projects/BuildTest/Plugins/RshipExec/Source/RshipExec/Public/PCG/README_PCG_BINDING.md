# Rship PCG Auto-Bind System

Automatically bind PCG-spawned actors to rShip for reactive control.

## Overview

The PCG Auto-Bind system enables PCG-spawned actors to automatically become rShip Targets with:
- Stable, deterministic Target IDs (survive PCG regeneration)
- Automatic property binding (Actions for writes, Emitters for reads)
- Clean lifecycle management (register on spawn, deregister on destroy)
- Zero manual setup per instance

## Quick Start

### Method 1: Using the PCG Spawn Actor Node (Requires PCG Plugin)

1. Enable PCG plugin in your `.uproject`
2. Set `bHasPCG = true` in `RshipExec.Build.cs`
3. In your PCG graph, use "Rship Spawn Actor" instead of "Spawn Actor"
4. Configure the node:
   - Set your Blueprint class
   - Set target category and tags
   - Configure naming pattern

### Method 2: Manual Component Attachment (No PCG Plugin Required)

1. Add `URshipPCGAutoBindComponent` to your spawned actors:

```cpp
// In your spawner or Blueprint
URshipPCGAutoBindComponent* BindComp = NewObject<URshipPCGAutoBindComponent>(SpawnedActor);
BindComp->RegisterComponent();
SpawnedActor->AddInstanceComponent(BindComp);
```

2. Set the instance ID from PCG point data:

```cpp
FRshipPCGInstanceId InstanceId = FRshipPCGInstanceId::FromPCGPoint(
    PCGComponentGuid,  // GUID of PCG component
    SourceKey,         // e.g., spline name
    PointIndex,        // Point index
    DistanceAlong,     // Distance along spline
    Alpha,             // 0-1 progress
    Seed,              // PCG seed
    DisplayName);      // Human-readable name

BindComp->SetInstanceId(InstanceId);
```

## Marking Properties for Binding

### Using Metadata (Preferred)

```cpp
UPROPERTY(EditAnywhere, meta=(RShipParam))
float Intensity = 1.0f;

UPROPERTY(EditAnywhere, meta=(RShipParam="CustomName", RShipCategory="Lighting"))
FLinearColor Color = FLinearColor::White;

UPROPERTY(EditAnywhere, meta=(RShipParam, RShipMin="0", RShipMax="100"))
float Brightness = 50.0f;

UPROPERTY(EditAnywhere, meta=(RShipParam, RShipWritable="false", RShipPulseMode="onchange"))
float SensorValue = 0.0f;

UPROPERTY(EditAnywhere, meta=(RShipParam, RShipPulseMode="fixedrate", RShipPulseRate="30"))
FVector Position;
```

### Using RS_ Prefix (Legacy/Blueprint Compatible)

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite)
float RS_Intensity = 1.0f;

UPROPERTY(EditAnywhere, BlueprintReadWrite)
FLinearColor RS_Color = FLinearColor::White;
```

## Metadata Reference

| Key | Description | Values |
|-----|-------------|--------|
| `RShipParam` | Mark for binding (value = custom name) | `""` or `"CustomName"` |
| `RShipWritable` | Allow writes from rShip | `"true"`, `"false"` |
| `RShipReadable` | Allow reads (pulses) | `"true"`, `"false"` |
| `RShipCategory` | UI category | `"Lighting"`, `"Transform"`, etc. |
| `RShipMin` | Minimum value | Numeric string |
| `RShipMax` | Maximum value | Numeric string |
| `RShipPulseMode` | Pulse emission mode | `"off"`, `"onchange"`, `"fixedrate"` |
| `RShipPulseRate` | Pulse rate (Hz) | `"1"` to `"60"` |

## Supported Property Types

| UE Type | rShip Type | Notes |
|---------|------------|-------|
| `bool` | boolean | |
| `int32` | integer | |
| `int64` | integer | |
| `float` | number | |
| `double` | number | |
| `FString` | string | |
| `FName` | string | |
| `FText` | string | |
| `FVector` | object | `{x, y, z}` |
| `FVector2D` | object | `{x, y}` |
| `FRotator` | object | `{pitch, yaw, roll}` |
| `FLinearColor` | object | `{r, g, b, a}` |
| `FColor` | object | Normalized 0-1 |
| `FTransform` | object | `{location, rotation, scale}` |
| `Enum` | string | Name or index |

## Deterministic ID Scheme

Target paths follow this format:
```
/pcg/{PCGComponentGuid}/{SourceKey}/{PointKey}
```

Where:
- `PCGComponentGuid`: GUID of the UPCGComponent
- `SourceKey`: Name of source (spline, volume, etc.)
- `PointKey`: Either `p{index}` or `d{distance}_s{seed}`

IDs remain stable across PCG regeneration if:
- The PCG component hasn't changed
- The source (spline) is the same
- Point indices are stable OR distance+seed are stable

## API Reference

### URshipPCGAutoBindComponent

```cpp
// Set instance identity
void SetInstanceId(const FRshipPCGInstanceId& InInstanceId);

// Check registration status
bool IsRegistered() const;

// Force re-registration
void Reregister();

// Re-scan properties
void RescanProperties();

// Get/set property values
float GetFloatProperty(FName PropertyName, bool& bSuccess) const;
bool SetFloatProperty(FName PropertyName, float Value);
FVector GetVectorProperty(FName PropertyName, bool& bSuccess) const;
bool SetVectorProperty(FName PropertyName, FVector Value);
FLinearColor GetColorProperty(FName PropertyName, bool& bSuccess) const;
bool SetColorProperty(FName PropertyName, FLinearColor Value);

// JSON access
FString GetPropertyValueAsJson(FName PropertyName) const;
bool SetPropertyValueFromJson(FName PropertyName, const FString& JsonValue);

// Pulse emission
void EmitPulse(FName PropertyName);
void EmitAllPulses();
```

### URshipPCGManager

```cpp
// Instance management
void RegisterInstance(URshipPCGAutoBindComponent* Component);
void UnregisterInstance(URshipPCGAutoBindComponent* Component);
URshipPCGAutoBindComponent* FindInstanceByPath(const FString& TargetPath) const;
TArray<URshipPCGAutoBindComponent*> GetAllInstances() const;

// Bulk operations
int32 SetPropertyOnAllInstances(UClass* Class, FName PropertyName, const FString& JsonValue);
int32 SetPropertyOnTaggedInstances(const FString& Tag, FName PropertyName, const FString& JsonValue);
TArray<URshipPCGAutoBindComponent*> GetInstancesWithTag(const FString& Tag) const;

// Debug
void DumpAllTargets();
void DumpTarget(const FString& TargetPath);
bool ValidateAllBindings();
```

## Console Commands

```
rship.pcg.list_targets       - List all registered PCG targets
rship.pcg.dump_target <path> - Dump details of a specific target
rship.pcg.validate           - Validate all bindings
rship.pcg.stats              - Show manager statistics
```

## Events

### URshipPCGAutoBindComponent

```cpp
// Called when bound to rShip
UPROPERTY(BlueprintAssignable)
FOnRshipPCGBound OnRshipBound;

// Called when a parameter changes from rShip
UPROPERTY(BlueprintAssignable)
FOnRshipPCGParamChanged OnRshipParamChanged;

// Called when any action is received
UPROPERTY(BlueprintAssignable)
FOnRshipPCGActionReceived OnRshipActionReceived;
```

### URshipPCGManager

```cpp
// Instance lifecycle
UPROPERTY(BlueprintAssignable)
FOnPCGInstanceRegistered OnInstanceRegistered;

UPROPERTY(BlueprintAssignable)
FOnPCGInstanceUnregistered OnInstanceUnregistered;

// Action execution
UPROPERTY(BlueprintAssignable)
FOnPCGActionExecuted OnActionExecuted;
```

## Performance Considerations

- Class bindings are cached per-class (built once, shared by instances)
- Property states use byte comparison for change detection
- Registration is batched for efficiency
- Pulse emission uses timers, not tick
- Stale instances are cleaned up automatically

## Example Blueprint

Create a Blueprint `BP_PCGControlledFixture`:

1. Add variables with RShip metadata:
   ```
   RS_Intensity (float, 0-1)
   RS_Color (LinearColor)
   RS_Pan (float, -180 to 180)
   RS_Tilt (float, -90 to 90)
   ```

2. Add `URshipPCGAutoBindComponent`

3. In your PCG graph:
   - Add Point Sampler on spline
   - Add "Rship Spawn Actor" node
   - Set Template Actor Class = BP_PCGControlledFixture
   - Set Target Category = "Lighting"
   - Set Default Tags = ["fixture", "moving_head"]

4. Run PCG generation

5. In rShip:
   - Each spawned fixture appears as a Target
   - RS_Intensity, RS_Color, RS_Pan, RS_Tilt are exposed as Actions
   - Set values per-instance from rShip
