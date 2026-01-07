# Getting Started with RshipExec

Welcome to RshipExec, the Unreal Engine executor plugin for [Rocketship](https://rocketship.io). This guide will help you understand core concepts and get your first actors connected to rship.

## What is RshipExec?

RshipExec connects your Unreal Engine project to the Rocketship platform, enabling:

- **Reactive Control**: Bind actor properties to rship and control them from any connected system
- **Real-time Telemetry**: Stream actor state (position, rotation, custom properties) to rship
- **Multi-system Integration**: Connect UE to lighting consoles, show control systems, media servers, and more

## Core Concepts

### The Rship Model

```
┌─────────────────────────────────────────────────────────────────┐
│                         RSHIP SERVER                            │
│                                                                 │
│  ┌─────────┐    ┌──────────┐    ┌─────────┐                    │
│  │ Binding │───▶│  Scene   │───▶│ Calendar│                    │
│  └─────────┘    └──────────┘    └─────────┘                    │
│       │                                                         │
│       ▼                                                         │
│  When Emitter changes → Trigger Action                         │
└─────────────────────────────────────────────────────────────────┘
        │                                          ▲
        │ Actions (commands)                       │ Pulses (state)
        ▼                                          │
┌─────────────────────────────────────────────────────────────────┐
│                      UNREAL ENGINE                              │
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │                    TARGET (Actor)                         │  │
│  │                                                           │  │
│  │   ┌──────────┐        ┌──────────┐        ┌──────────┐   │  │
│  │   │ Emitter  │        │ Emitter  │        │  Action  │   │  │
│  │   │ Position │        │ Intensity│        │ SetColor │   │  │
│  │   └──────────┘        └──────────┘        └──────────┘   │  │
│  └──────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

| Term | Definition |
|------|------------|
| **Target** | An entity in rship (usually an Actor in UE) |
| **Emitter** | A readable value that sends **Pulses** to rship (e.g., actor position) |
| **Action** | A callable function that rship can invoke (e.g., SetColor, PlayAnimation) |
| **Pulse** | A real-time state update from an Emitter |
| **Binding** | A rule: "when this Emitter changes, trigger this Action" |

## Quick Start

### Step 1: Enable the Plugin

1. Copy the `Plugins/RshipExec` folder to your project's `Plugins/` directory
2. Regenerate project files and build
3. Enable "RshipExec" in Edit → Plugins

### Step 2: Configure Connection

Open **Project Settings → Game → Rocketship Settings**:

| Setting | Description | Default |
|---------|-------------|---------|
| Host | Rship server address | `localhost` |
| Port | WebSocket port | `5155` |
| Auto Connect | Connect on play | `true` |

### Step 3: Add a Target Component

Add `URshipTargetComponent` to any actor you want to expose to rship:

**In Blueprint:**
1. Select your actor
2. Add Component → search "Rship Target"
3. Set the **Target Id** (unique name for this actor in rship)

**In C++:**
```cpp
UPROPERTY(VisibleAnywhere)
URshipTargetComponent* RshipTarget;

// In constructor
RshipTarget = CreateDefaultSubobject<URshipTargetComponent>(TEXT("RshipTarget"));
```

### Step 4: Expose Properties with RS_ Prefix

The simplest way to expose properties is the `RS_` prefix convention:

**Blueprint:**
- Create a variable named `RS_Intensity` (Float)
- It automatically becomes an **Action** (rship can set it) and **Emitter** (changes are sent to rship)

**C++:**
```cpp
// Exposed as both Action and Emitter
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship")
float RS_Intensity = 1.0f;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship")
FLinearColor RS_Color = FLinearColor::White;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship")
FVector RS_Position;
```

**Exposed Functions (Actions only):**
```cpp
// This becomes an Action callable from rship
UFUNCTION(BlueprintCallable, Category = "Rship")
void RS_PlayAnimation(FName AnimationName, float BlendTime);

UFUNCTION(BlueprintCallable, Category = "Rship")
void RS_SetMaterialParameter(FName ParameterName, float Value);
```

### Step 5: Test the Connection

1. Start your rship server
2. Press Play in UE
3. Check the Output Log for: `LogRshipExec: Connected to rship server`
4. Your targets should appear in the rship UI

## The RS_ Naming Convention

Any property or function starting with `RS_` is automatically registered:

| Declaration | Rship Result |
|-------------|--------------|
| `float RS_Brightness` | Action + Emitter named "Brightness" |
| `void RS_TriggerEffect()` | Action named "TriggerEffect" |
| `FVector RS_WorldPosition` | Action + Emitter named "WorldPosition" |
| `DECLARE_DYNAMIC_MULTICAST_DELEGATE RS_OnHit` | Emitter (event delegate) |

The `RS_` prefix is stripped in rship, so `RS_Intensity` appears as just `Intensity`.

## Supported Property Types

| UE Type | Rship Type | Notes |
|---------|------------|-------|
| `bool` | boolean | |
| `int32`, `int64` | integer | |
| `float`, `double` | number | |
| `FString`, `FName`, `FText` | string | |
| `FVector` | object | `{x, y, z}` |
| `FRotator` | object | `{pitch, yaw, roll}` |
| `FTransform` | object | Full transform |
| `FLinearColor`, `FColor` | object | `{r, g, b, a}` |
| `FVector2D` | object | `{x, y}` |
| `Enum` | string | Enum value name |
| `UObject*` | string | Object path |
| `TArray<T>` | array | Arrays of supported types |
| `USTRUCT` | object | Nested structure |

## Organizing Targets

### Categories

```cpp
UPROPERTY(EditAnywhere, Category = "RshipTarget")
FString Category = "lighting";  // Groups targets in rship UI
```

### Tags

```cpp
UPROPERTY(EditAnywhere, Category = "RshipTarget")
TArray<FString> Tags = {"stage-left", "moving-head", "wash"};
```

### Target Groups

Create groups programmatically for bulk operations:

```cpp
URshipSubsystem* Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
URshipTargetGroupManager* Groups = Subsystem->GetGroupManager();

// Create a group
Groups->CreateGroup("all-lights", "All Stage Lights");

// Add targets to group
Groups->AddTargetToGroup("light-01", "all-lights");
Groups->AddTargetToGroup("light-02", "all-lights");

// Bulk operations
Groups->SetGroupEmitterValue("all-lights", "Intensity", 0.5f);
```

## Advanced: Metadata-Based Binding

For more control, use UPROPERTY metadata instead of the RS_ prefix:

```cpp
// Basic binding
UPROPERTY(EditAnywhere, meta=(RShipParam))
float Intensity = 1.0f;

// Custom name and category
UPROPERTY(EditAnywhere, meta=(RShipParam="Brightness", RShipCategory="Dimmer"))
float LightIntensity = 1.0f;

// Read-only emitter (no Action, just Pulse)
UPROPERTY(EditAnywhere, meta=(RShipParam, RShipWritable="false"))
float SensorReading = 0.0f;

// Value constraints
UPROPERTY(EditAnywhere, meta=(RShipParam, RShipMin="0", RShipMax="100"))
float Percentage = 50.0f;

// Pulse rate control
UPROPERTY(EditAnywhere, meta=(RShipParam, RShipPulseMode="fixedrate", RShipPulseRate="30"))
FVector TrackedPosition;
```

| Metadata Key | Description | Values |
|--------------|-------------|--------|
| `RShipParam` | Enable binding (value = custom name) | `""` or `"CustomName"` |
| `RShipWritable` | Allow writes (Action) | `"true"`, `"false"` |
| `RShipReadable` | Allow reads (Emitter) | `"true"`, `"false"` |
| `RShipCategory` | UI grouping | Any string |
| `RShipMin` / `RShipMax` | Value constraints | Numeric |
| `RShipPulseMode` | When to emit | `"off"`, `"onchange"`, `"fixedrate"` |
| `RShipPulseRate` | Fixed rate Hz | `"1"` to `"60"` |

## Blueprint Integration

### Accessing the Subsystem

```
Get Engine Subsystem → URshipSubsystem
```

### Useful Blueprint Nodes

| Node | Description |
|------|-------------|
| `Is Connected` | Check connection status |
| `Get Connection State` | Detailed connection state |
| `Reconnect` | Force reconnection |
| `Get Target Component` | Find target by ID |
| `Get All Targets` | List all registered targets |

### Events

The Target Component exposes:

```
On Rship Data → Called when an Action is invoked on this target
```

## Editor Panels

Access via **Window → Rship**:

| Panel | Purpose |
|-------|---------|
| **Status** | Connection status, target list, diagnostics |
| **Timecode** | SMPTE timecode sync configuration |
| **LiveLink** | LiveLink subject mapping |
| **Materials** | Material parameter bindings |
| **Fixtures** | Lighting fixture management |
| **NDI** | NDI camera streaming |
| **Test** | Offline testing and validation |

## Debugging

### Console Commands

```
rship.status          # Connection status and target count
rship.targets         # List all registered targets
rship.validate        # Validate scene configuration
rship.reconnect       # Force reconnection
rship.timecode        # Timecode sync status
```

### Logging

Enable verbose logging in Project Settings → Rocketship Settings:

- `bEnableVerboseLogging` - Detailed message logging
- `bLogConnectionEvents` - Connection state changes
- `bLogMessageTraffic` - All WebSocket messages

Or via console:
```
Log LogRshipExec Verbose
```

## Common Patterns

### Animated Light Fixture

```cpp
UCLASS()
class AMyLight : public AActor
{
    UPROPERTY(VisibleAnywhere)
    URshipTargetComponent* RshipTarget;

    UPROPERTY(VisibleAnywhere)
    USpotLightComponent* SpotLight;

    // Rship-controllable properties
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RS_Intensity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor RS_Color = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RS_Pan = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RS_Tilt = 0.0f;

    virtual void Tick(float DeltaTime) override
    {
        // Apply rship values to actual light
        SpotLight->SetIntensity(RS_Intensity * 10000.0f);
        SpotLight->SetLightColor(RS_Color);
        SetActorRotation(FRotator(RS_Tilt, RS_Pan, 0));
    }
};
```

### Sensor/Trigger Actor

```cpp
UCLASS()
class AProximitySensor : public AActor
{
    UPROPERTY(VisibleAnywhere)
    URshipTargetComponent* RshipTarget;

    // Read-only sensor value (Emitter only)
    UPROPERTY(BlueprintReadOnly, meta=(RShipParam, RShipWritable="false"))
    float RS_Distance = 0.0f;

    // Trigger event (Emitter)
    UPROPERTY(BlueprintAssignable)
    FOnProximityTrigger RS_OnTrigger;

    void OnOverlapBegin(...)
    {
        RS_OnTrigger.Broadcast();  // Sends pulse to rship
    }
};
```

### Receiving Actions in Blueprint

1. Add Rship Target Component to your actor
2. Bind to the `On Rship Data` event
3. In the event, check which property changed and respond

Or use the metadata approach with `RShipWritable="true"` and handle changes in Tick.

## Next Steps

- [PCG Auto-Bind System](Source/RshipExec/Public/PCG/README_PCG_BINDING.md) - Automatic binding for procedurally spawned actors
- [Rate Limiting Guide](Source/RshipExec/README_RATE_LIMITING.md) - Configure message throttling
- [Upgrade Guide](UPGRADE_GUIDE.md) - Full feature reference

## Troubleshooting

| Issue | Solution |
|-------|----------|
| "Not connected" | Check Host/Port settings, ensure rship server is running |
| Target not appearing | Verify Target Id is set, check for duplicate IDs |
| Actions not working | Ensure property is `BlueprintReadWrite` or has `RShipWritable="true"` |
| Pulses not sending | Check `RShipPulseMode`, ensure property value is actually changing |
| High latency | Enable `bTcpNoDelay` in settings, check rate limiter metrics |

## Support

- GitHub Issues: [github.com/ignition-is-go/rship-unreal](https://github.com/ignition-is-go/rship-unreal)
- Rocketship Docs: [docs.rocketship.io](https://docs.rocketship.io)
