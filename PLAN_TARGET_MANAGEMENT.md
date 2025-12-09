# Target Management Feature Plan

## Overview

This document outlines the implementation plan for 7 major features to help UE artists manage thousands of targets efficiently. Each feature is designed with the artist persona in mind: technology should augment creativity, not be an obstacle.

**Scale Targets:**
- 10,000+ targets in a single project
- 100+ targets selected simultaneously
- 60Hz emitter pulse rates
- Multiple UE instances connected to same rship server

---

## Feature 1: Smart Target Groups & Tagging

### User Value
Artists think in creative groupings ("stage-left lights", "hero fixtures", "background elements"), not individual components. This feature lets them organize, filter, and operate on targets as logical groups.

### Technical Design

#### New Data Structures

```cpp
// RshipTargetGroup.h
USTRUCT(BlueprintType)
struct FRshipTargetGroup
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString GroupId;  // Unique identifier

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DisplayName;  // User-facing name

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor Color;  // Visual identification

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> TargetIds;  // Members (by target ID)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Tags;  // Associated tags

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAutoPopulate;  // Auto-add matching targets

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AutoPopulatePattern;  // Wildcard pattern for auto-population
};

// Add to URshipTargetComponent
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Organization")
TArray<FString> Tags;  // User-defined tags

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Organization")
TArray<FString> GroupIds;  // Groups this target belongs to
```

#### New Component: URshipTargetGroupManager

```cpp
// Singleton-style manager accessible from subsystem
UCLASS(BlueprintType)
class URshipTargetGroupManager : public UObject
{
    GENERATED_BODY()

public:
    // Group CRUD
    UFUNCTION(BlueprintCallable)
    FRshipTargetGroup CreateGroup(FString DisplayName, FLinearColor Color);

    UFUNCTION(BlueprintCallable)
    void DeleteGroup(FString GroupId);

    UFUNCTION(BlueprintCallable)
    void AddTargetToGroup(FString TargetId, FString GroupId);

    UFUNCTION(BlueprintCallable)
    void RemoveTargetFromGroup(FString TargetId, FString GroupId);

    // Tag operations
    UFUNCTION(BlueprintCallable)
    void AddTagToTarget(URshipTargetComponent* Target, FString Tag);

    UFUNCTION(BlueprintCallable)
    void RemoveTagFromTarget(URshipTargetComponent* Target, FString Tag);

    // Query operations
    UFUNCTION(BlueprintCallable)
    TArray<URshipTargetComponent*> GetTargetsByTag(FString Tag);

    UFUNCTION(BlueprintCallable)
    TArray<URshipTargetComponent*> GetTargetsByGroup(FString GroupId);

    UFUNCTION(BlueprintCallable)
    TArray<URshipTargetComponent*> GetTargetsByPattern(FString WildcardPattern);

    UFUNCTION(BlueprintCallable)
    TArray<FString> GetAllTags();

    UFUNCTION(BlueprintCallable)
    TArray<FRshipTargetGroup> GetAllGroups();

    // Auto-grouping
    UFUNCTION(BlueprintCallable)
    FRshipTargetGroup CreateGroupFromActorClass(TSubclassOf<AActor> ActorClass);

    UFUNCTION(BlueprintCallable)
    FRshipTargetGroup CreateGroupFromFolder(FString FolderPath);

    UFUNCTION(BlueprintCallable)
    FRshipTargetGroup CreateGroupFromProximity(FVector Center, float Radius);

private:
    TMap<FString, FRshipTargetGroup> Groups;
    TMap<FString, TSet<FString>> TagToTargets;  // Reverse index
};
```

#### Server Sync

Tags and groups are sent as part of target registration:
```cpp
void URshipSubsystem::SendTarget(Target* target)
{
    // ... existing code ...

    // Add tags and groups to target metadata
    Target->SetArrayField(TEXT("tags"), TagsJsonArray);
    Target->SetArrayField(TEXT("groupIds"), GroupIdsJsonArray);
}
```

### Implementation Steps

1. **Create FRshipTargetGroup struct** in new `RshipTargetGroup.h`
2. **Add Tags/GroupIds properties** to URshipTargetComponent
3. **Create URshipTargetGroupManager** class with all Blueprint-callable methods
4. **Integrate manager into URshipSubsystem** (lazy initialization)
5. **Update SendTarget()** to include tags/groups in metadata
6. **Add wildcard matching utility** for pattern-based queries
7. **Implement auto-grouping algorithms** (by class, folder, proximity)
8. **Create Editor Utility Widget** for visual group management
9. **Add persistence** via SaveGame or project settings

### Blueprint API

```
// Example usage in Blueprint
Get Rship Subsystem → Get Group Manager → Get Targets By Tag("stage-left")
                                        → Create Group From Actor Class(BP_Light)
                                        → Add Tag To Target(Target, "hero")
```

### Testing Strategy

| Test Case | Expected Behavior |
|-----------|-------------------|
| Create group with 1000 targets | < 100ms, no frame hitches |
| Query by tag with 10,000 targets | < 10ms |
| Wildcard pattern `stage-*-lights` | Returns exact matches only |
| Auto-group by actor class | All instances of class added |
| Remove target from group | Target still exists, just unlinked |
| Delete group | Targets persist, group references cleared |
| Circular group membership | Allowed (target in multiple groups) |
| Empty tag/group name | Rejected with warning |
| Persistence across PIE restart | Groups/tags restored |

### Edge Cases

- Target destroyed while in group → auto-remove from group
- Group deleted while targets exist → clear group references
- Duplicate tag assignment → no-op (idempotent)
- Very long tag names → truncate to 64 chars
- Unicode in tags → supported
- Case sensitivity → case-insensitive matching, preserve original case

---

## Feature 2: Bulk Property Editing

### User Value
Nobody wants to configure 1000 lights individually. Artists need to select multiple targets and edit shared properties at once.

### Technical Design

#### Bulk Operation Commands

```cpp
// RshipBulkOperations.h
UCLASS(BlueprintType)
class URshipBulkOperations : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Selection management
    UFUNCTION(BlueprintCallable, Category = "Rship|Bulk")
    static void SelectTargets(const TArray<URshipTargetComponent*>& Targets);

    UFUNCTION(BlueprintCallable, Category = "Rship|Bulk")
    static void SelectTargetsByTag(const FString& Tag);

    UFUNCTION(BlueprintCallable, Category = "Rship|Bulk")
    static void SelectTargetsByGroup(const FString& GroupId);

    UFUNCTION(BlueprintCallable, Category = "Rship|Bulk")
    static TArray<URshipTargetComponent*> GetSelectedTargets();

    UFUNCTION(BlueprintCallable, Category = "Rship|Bulk")
    static void ClearSelection();

    // Bulk modifications
    UFUNCTION(BlueprintCallable, Category = "Rship|Bulk")
    static void BulkAddTag(const TArray<URshipTargetComponent*>& Targets, const FString& Tag);

    UFUNCTION(BlueprintCallable, Category = "Rship|Bulk")
    static void BulkRemoveTag(const TArray<URshipTargetComponent*>& Targets, const FString& Tag);

    UFUNCTION(BlueprintCallable, Category = "Rship|Bulk")
    static void BulkSetEnabled(const TArray<URshipTargetComponent*>& Targets, bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "Rship|Bulk")
    static void BulkReregister(const TArray<URshipTargetComponent*>& Targets);

    // Copy/paste configurations
    UFUNCTION(BlueprintCallable, Category = "Rship|Bulk")
    static FRshipTargetConfig CopyTargetConfig(URshipTargetComponent* Source);

    UFUNCTION(BlueprintCallable, Category = "Rship|Bulk")
    static void PasteTargetConfig(const TArray<URshipTargetComponent*>& Targets,
                                   const FRshipTargetConfig& Config,
                                   bool bPasteTags, bool bPasteGroups);

    // Find and replace
    UFUNCTION(BlueprintCallable, Category = "Rship|Bulk")
    static int32 FindAndReplaceInTargetNames(const FString& Find,
                                              const FString& Replace,
                                              bool bCaseSensitive);
};

USTRUCT(BlueprintType)
struct FRshipTargetConfig
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FString TargetName;

    UPROPERTY(BlueprintReadWrite)
    TArray<FString> Tags;

    UPROPERTY(BlueprintReadWrite)
    TArray<FString> GroupIds;

    // Future: emitter configurations, action schemas, etc.
};
```

#### Selection State

```cpp
// In URshipSubsystem or separate selection manager
UPROPERTY()
TSet<URshipTargetComponent*> SelectedTargets;

// Selection changed delegate for UI binding
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRshipSelectionChanged);

UPROPERTY(BlueprintAssignable)
FOnRshipSelectionChanged OnSelectionChanged;
```

### Implementation Steps

1. **Create selection state** in URshipSubsystem
2. **Implement URshipBulkOperations** function library
3. **Add FRshipTargetConfig** struct for copy/paste
4. **Create selection changed delegate** for UI updates
5. **Implement find/replace** with regex support
6. **Add undo/redo support** via transaction system
7. **Create Editor Utility Widget** for bulk operations UI
8. **Batch network updates** (coalesce bulk changes into single send)

### Blueprint API

```
// Select all targets with tag, then bulk add another tag
Select Targets By Tag("stage-left")
  → Get Selected Targets
  → Bulk Add Tag("hero-section")
```

### Testing Strategy

| Test Case | Expected Behavior |
|-----------|-------------------|
| Select 1000 targets | < 50ms |
| Bulk add tag to 1000 targets | < 200ms, single network batch |
| Copy/paste config | All specified fields copied |
| Find/replace in names | Regex patterns work |
| Undo bulk operation | All changes reverted atomically |
| Selection persistence | Survives PIE restart (optional) |

### Edge Cases

- Select destroyed target → auto-remove from selection
- Bulk edit during network reconnect → queue changes
- Paste config with missing fields → skip missing, apply rest
- Find/replace creates duplicate names → append suffix

---

## Feature 3: In-Viewport Selection Sync

### User Value
Bridge the gap between abstract targets (in rship UI) and physical actors (in UE viewport). Click a target in rship → see it highlighted in UE. Select actors in UE → filter the target list.

### Technical Design

#### Editor Integration (Editor Module)

```cpp
// RshipEditorIntegration.h (Editor module only)
UCLASS()
class URshipEditorIntegration : public UEditorSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // Selection sync
    UFUNCTION(BlueprintCallable, Category = "Rship|Editor")
    void FocusOnTarget(URshipTargetComponent* Target);

    UFUNCTION(BlueprintCallable, Category = "Rship|Editor")
    void SelectActorForTarget(URshipTargetComponent* Target);

    UFUNCTION(BlueprintCallable, Category = "Rship|Editor")
    TArray<URshipTargetComponent*> GetTargetsForSelectedActors();

    // Visual indicators
    UFUNCTION(BlueprintCallable, Category = "Rship|Editor")
    void HighlightTargets(const TArray<URshipTargetComponent*>& Targets, FLinearColor Color);

    UFUNCTION(BlueprintCallable, Category = "Rship|Editor")
    void ClearHighlights();

    // Connection status indicators
    UFUNCTION(BlueprintCallable, Category = "Rship|Editor")
    void ShowConnectionStatusOnActors(bool bShow);

private:
    void OnEditorSelectionChanged(UObject* Object);
    void OnTargetSelectionChanged();

    // Component visualizer for drawing status icons
    TSharedPtr<FRshipTargetVisualizer> Visualizer;
};

// Component visualizer for drawing in-editor
class FRshipTargetVisualizer : public FComponentVisualizer
{
public:
    virtual void DrawVisualization(const UActorComponent* Component,
                                    const FSceneView* View,
                                    FPrimitiveDrawInterface* PDI) override;

    // Draw connection status icon above actor
    // Draw pulse activity indicator
    // Draw group color outline
};
```

#### Runtime Highlighting (Game Module)

```cpp
// For PIE and runtime, use debug drawing
UFUNCTION(BlueprintCallable, Category = "Rship|Debug")
void DrawTargetDebugInfo(URshipTargetComponent* Target, float Duration);

// Shows:
// - Target name above actor
// - Connection status (green/red/yellow)
// - Pulse rate indicator
// - Group color highlight
```

### Implementation Steps

1. **Create RshipEditor module** (Editor-only)
2. **Implement URshipEditorIntegration** subsystem
3. **Create FRshipTargetVisualizer** component visualizer
4. **Hook into editor selection changed** events
5. **Implement bidirectional sync** (editor ↔ rship selection)
6. **Add connection status icons** (connected/disconnected/error)
7. **Create debug draw commands** for runtime
8. **Add "Find in Level" context menu** for rship UI

### Blueprint API

```
// From rship UI (via remote command or editor widget)
Focus On Target(Target) → Viewport centers on actor, selects it

// From UE selection
Get Targets For Selected Actors → Returns array of target components
```

### Testing Strategy

| Test Case | Expected Behavior |
|-----------|-------------------|
| Focus on target in sublevel | Sublevel loaded, actor selected |
| Select 100 actors | All corresponding targets highlighted |
| Focus on destroyed target | Error handled gracefully |
| Connection status updates | Icons refresh on connect/disconnect |
| Multiple viewports | All viewports sync |

### Edge Cases

- Target's actor in unloaded sublevel → load sublevel first
- Actor has multiple target components → show all
- Target component on hidden actor → make visible temporarily
- Very distant actor → camera movement animated

---

## Feature 4: Presets & Snapshots

### User Value
Artists want "looks" they can recall instantly. Save the current state of all emitters, recall it later with optional interpolation.

### Technical Design

#### Preset Data Structures

```cpp
// RshipPreset.h
USTRUCT(BlueprintType)
struct FRshipEmitterSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FString EmitterId;  // Full emitter ID

    UPROPERTY(BlueprintReadWrite)
    TSharedPtr<FJsonObject> Values;  // Captured values

    UPROPERTY(BlueprintReadWrite)
    FDateTime CapturedAt;
};

USTRUCT(BlueprintType)
struct FRshipPreset
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PresetId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FRshipEmitterSnapshot> Snapshots;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Tags;  // For organization

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime CreatedAt;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime ModifiedAt;
};

UCLASS(BlueprintType)
class URshipPresetManager : public UObject
{
    GENERATED_BODY()

public:
    // Capture
    UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
    FRshipPreset CapturePreset(const FString& Name,
                                const TArray<URshipTargetComponent*>& Targets);

    UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
    FRshipPreset CapturePresetByTag(const FString& Name, const FString& Tag);

    UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
    FRshipPreset CapturePresetByGroup(const FString& Name, const FString& GroupId);

    // Recall
    UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
    void RecallPreset(const FRshipPreset& Preset, float InterpolationTime = 0.0f);

    UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
    void RecallPresetById(const FString& PresetId, float InterpolationTime = 0.0f);

    // Interpolation
    UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
    void CrossfadePresets(const FRshipPreset& From,
                          const FRshipPreset& To,
                          float Duration);

    // Management
    UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
    void SavePreset(const FRshipPreset& Preset);

    UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
    void DeletePreset(const FString& PresetId);

    UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
    TArray<FRshipPreset> GetAllPresets();

    UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
    TArray<FRshipPreset> GetPresetsByTag(const FString& Tag);

    // Import/Export
    UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
    FString ExportPresetToJson(const FRshipPreset& Preset);

    UFUNCTION(BlueprintCallable, Category = "Rship|Presets")
    FRshipPreset ImportPresetFromJson(const FString& JsonString);

private:
    TMap<FString, FRshipPreset> Presets;

    // Interpolation state
    FRshipPreset InterpolationFrom;
    FRshipPreset InterpolationTo;
    float InterpolationProgress;
    FTimerHandle InterpolationTimer;

    void TickInterpolation();
    TSharedPtr<FJsonObject> LerpJsonObjects(TSharedPtr<FJsonObject> A,
                                             TSharedPtr<FJsonObject> B,
                                             float Alpha);
};
```

#### Emitter Value Capture

To capture current emitter values, we need to store them when pulses are sent:

```cpp
// In AEmitterHandler or URshipSubsystem
TMap<FString, TSharedPtr<FJsonObject>> LastEmitterValues;

void URshipSubsystem::PulseEmitter(FString targetId, FString emitterId, TSharedPtr<FJsonObject> data)
{
    // Store for preset capture
    FString fullId = targetId + TEXT(":") + emitterId;
    LastEmitterValues.Add(fullId, data);

    // ... existing pulse code ...
}
```

### Implementation Steps

1. **Create preset data structures** (FRshipPreset, FRshipEmitterSnapshot)
2. **Implement emitter value caching** in subsystem
3. **Create URshipPresetManager** class
4. **Implement capture functionality** (snapshot all cached values)
5. **Implement recall functionality** (push values back to emitters)
6. **Add interpolation system** (lerp between presets over time)
7. **Implement persistence** (save to project settings or external file)
8. **Create preset browser widget** (list, preview, organize)
9. **Add import/export** for sharing presets

### Blueprint API

```
// Capture current state
Capture Preset By Tag("all-lights", "sunset-look") → FRshipPreset

// Recall with 2-second fade
Recall Preset(SunsetPreset, 2.0)

// Crossfade between presets
Crossfade Presets(SunsetPreset, NightPreset, 5.0)
```

### Testing Strategy

| Test Case | Expected Behavior |
|-----------|-------------------|
| Capture 1000 emitter values | < 100ms |
| Recall preset | All values pushed via actions |
| Interpolate numeric values | Smooth linear interpolation |
| Interpolate colors | HSV or RGB lerp |
| Interpolate non-numeric | Snap at 50% |
| Missing emitter on recall | Skip with warning |
| Extra emitters on recall | Ignored |
| Persistence across sessions | Presets survive project reload |

### Edge Cases

- Emitter schema changed since capture → warn, skip incompatible fields
- Preset for deleted targets → partial recall, log warnings
- Concurrent interpolations → cancel previous, start new
- Very fast recall rate → throttle to prevent flood
- Large presets (10000+ values) → async capture/recall

---

## Feature 5: Health Dashboard Widget

### User Value
"Operate Mode" needs at-a-glance status. Artists need to know: Is everything connected? What's active? Any problems?

### Technical Design

#### Health Data Aggregation

```cpp
// RshipHealthMonitor.h
USTRUCT(BlueprintType)
struct FRshipHealthStatus
{
    GENERATED_BODY()

    // Connection
    UPROPERTY(BlueprintReadOnly)
    bool bIsConnected;

    UPROPERTY(BlueprintReadOnly)
    float ConnectionLatencyMs;

    UPROPERTY(BlueprintReadOnly)
    int32 ReconnectAttempts;

    // Targets
    UPROPERTY(BlueprintReadOnly)
    int32 TotalTargets;

    UPROPERTY(BlueprintReadOnly)
    int32 ActiveTargets;  // Sent pulse in last N seconds

    UPROPERTY(BlueprintReadOnly)
    int32 ErrorTargets;   // Failed to register or communicate

    // Throughput
    UPROPERTY(BlueprintReadOnly)
    int32 PulsesPerSecond;

    UPROPERTY(BlueprintReadOnly)
    int32 MessagesPerSecond;

    UPROPERTY(BlueprintReadOnly)
    int32 BytesPerSecond;

    // Queue status
    UPROPERTY(BlueprintReadOnly)
    int32 QueueLength;

    UPROPERTY(BlueprintReadOnly)
    float QueuePressure;  // 0.0 - 1.0

    UPROPERTY(BlueprintReadOnly)
    int32 MessagesDropped;

    UPROPERTY(BlueprintReadOnly)
    bool bIsBackingOff;

    UPROPERTY(BlueprintReadOnly)
    float BackoffRemaining;

    // Per-target activity (for "hot" targets display)
    UPROPERTY(BlueprintReadOnly)
    TMap<FString, int32> TargetPulseRates;  // Target ID → pulses/sec
};

UCLASS(BlueprintType)
class URshipHealthMonitor : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Rship|Health")
    FRshipHealthStatus GetCurrentHealth();

    UFUNCTION(BlueprintCallable, Category = "Rship|Health")
    TArray<FString> GetHotTargets(int32 TopN = 10);  // Highest pulse rate

    UFUNCTION(BlueprintCallable, Category = "Rship|Health")
    TArray<FString> GetErrorTargets();

    UFUNCTION(BlueprintCallable, Category = "Rship|Health")
    TArray<FString> GetInactiveTargets(float InactiveThresholdSeconds = 5.0f);

    // Actions
    UFUNCTION(BlueprintCallable, Category = "Rship|Health")
    void ReconnectAll();

    UFUNCTION(BlueprintCallable, Category = "Rship|Health")
    void ReregisterAll();

    UFUNCTION(BlueprintCallable, Category = "Rship|Health")
    void ResetStatistics();

    // Events
    UPROPERTY(BlueprintAssignable)
    FOnRshipConnectionLost OnConnectionLost;

    UPROPERTY(BlueprintAssignable)
    FOnRshipConnectionRestored OnConnectionRestored;

    UPROPERTY(BlueprintAssignable)
    FOnRshipBackpressureWarning OnBackpressureWarning;

private:
    void UpdateHealthData();
    FTimerHandle UpdateTimer;

    // Activity tracking
    TMap<FString, FDateTime> LastPulseTime;
    TMap<FString, int32> PulseCountsThisSecond;
};
```

#### Editor Widget

```cpp
// Create as Editor Utility Widget (UMG) or Slate widget
// Displays:
// - Large connection status indicator (green/yellow/red)
// - Throughput graphs (pulses/sec, bytes/sec)
// - Queue pressure meter
// - "Hot" targets list (most active)
// - Error targets list with retry buttons
// - One-click "Reconnect All" button
```

### Implementation Steps

1. **Create FRshipHealthStatus** struct
2. **Create URshipHealthMonitor** class
3. **Add activity tracking** (per-target pulse timestamps)
4. **Implement health aggregation** (poll every 100ms)
5. **Add health events** (connection lost, backpressure warning)
6. **Create Dashboard UMG Widget** (visual layout)
7. **Add graphs/charts** (using UMG or third-party charting)
8. **Implement "hot targets" tracking** (rolling window)
9. **Add error tracking** (failed registrations, timeouts)

### Blueprint API

```
// In Dashboard widget tick
Get Current Health → Update all indicators

// Event bindings
On Connection Lost → Show red banner
On Backpressure Warning → Show yellow warning
```

### Testing Strategy

| Test Case | Expected Behavior |
|-----------|-------------------|
| 1000 targets, 60Hz pulses | Dashboard updates smoothly |
| Connection lost | Status turns red immediately |
| Reconnection | Status turns green, counters reset |
| Queue pressure > 70% | Yellow warning indicator |
| Messages dropped | Counter increments, warning shown |
| No activity for 5 seconds | Target marked inactive |

### Edge Cases

- Dashboard opened before connection → show "Connecting..." state
- Very high pulse rate → throttle dashboard updates (10Hz max)
- Multiple UE instances → each has own dashboard
- PIE restart → reset statistics

---

## Feature 6: Template System

### User Value
Reduce repetitive configuration. Define a template for a light actor class, and all instances automatically get the same target structure.

### Technical Design

#### Template Data Structures

```cpp
// RshipTargetTemplate.h
USTRUCT(BlueprintType)
struct FRshipEmitterTemplate
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString EmitterName;  // Without RS_ prefix

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PropertyPath;  // Path to property on actor

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PulseRate;  // How often to auto-pulse (0 = manual only)
};

USTRUCT(BlueprintType)
struct FRshipActionTemplate
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ActionName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString FunctionName;  // Or property path for property actions

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsProperty;
};

USTRUCT(BlueprintType)
struct FRshipTargetTemplate
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TemplateId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSubclassOf<AActor> TargetActorClass;  // Optional: auto-apply to this class

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TargetNamePattern;  // e.g., "{ActorLabel}" or "{ClassName}_{Index}"

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> DefaultTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FRshipEmitterTemplate> Emitters;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FRshipActionTemplate> Actions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAutoRegister;  // Auto-register when actor spawns
};

UCLASS(BlueprintType, Blueprintable)
class URshipTemplateDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FRshipTargetTemplate> Templates;
};

UCLASS(BlueprintType)
class URshipTemplateManager : public UObject
{
    GENERATED_BODY()

public:
    // Template management
    UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
    void RegisterTemplate(const FRshipTargetTemplate& Template);

    UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
    FRshipTargetTemplate GetTemplateForClass(TSubclassOf<AActor> ActorClass);

    // Apply templates
    UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
    URshipTargetComponent* ApplyTemplate(AActor* Actor, const FRshipTargetTemplate& Template);

    UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
    void ApplyTemplateToAll(TSubclassOf<AActor> ActorClass, const FRshipTargetTemplate& Template);

    // Auto-registration hooks
    void OnActorSpawned(AActor* Actor);

    // Template creation helpers
    UFUNCTION(BlueprintCallable, Category = "Rship|Templates")
    FRshipTargetTemplate CreateTemplateFromComponent(URshipTargetComponent* Source);

private:
    TMap<UClass*, FRshipTargetTemplate> ClassToTemplate;

    void SetupActorSpawnedHook();
};
```

#### Auto-Registration Integration

```cpp
// In URshipSubsystem::Initialize or URshipTemplateManager
void SetupActorSpawnedHook()
{
    if (GEngine)
    {
        GEngine->OnLevelActorAdded().AddUObject(this, &URshipTemplateManager::OnActorSpawned);
    }
}

void URshipTemplateManager::OnActorSpawned(AActor* Actor)
{
    if (FRshipTargetTemplate* Template = ClassToTemplate.Find(Actor->GetClass()))
    {
        if (Template->bAutoRegister)
        {
            ApplyTemplate(Actor, *Template);
        }
    }
}
```

### Implementation Steps

1. **Create template data structures** (FRshipTargetTemplate, etc.)
2. **Create URshipTemplateDataAsset** for storing templates
3. **Implement URshipTemplateManager** class
4. **Add actor spawned hook** for auto-registration
5. **Implement template application** (add component, configure)
6. **Create template editor** (custom Details panel or widget)
7. **Add "Create Template From" context menu** on components
8. **Implement template inheritance** (base template + overrides)
9. **Add template browser widget** for managing templates

### Blueprint API

```
// Apply template to actor
Apply Template(MyLightActor, LightTargetTemplate) → URshipTargetComponent

// Create template from existing
Create Template From Component(ExistingComponent) → FRshipTargetTemplate

// Bulk apply
Apply Template To All(BP_Light, LightTargetTemplate)
```

### Testing Strategy

| Test Case | Expected Behavior |
|-----------|-------------------|
| Apply template to actor | Component added, configured correctly |
| Auto-register on spawn | New actors get template applied |
| Template with emitters | Emitters auto-generated |
| Template with actions | Actions auto-generated |
| Missing function in template | Warning logged, action skipped |
| Template inheritance | Child overrides parent fields |
| Export/import template | JSON round-trip preserves all data |

### Edge Cases

- Actor already has target component → warn, skip or merge
- Template references non-existent property → warn, skip field
- Circular template inheritance → detect and error
- Template applied to wrong actor class → warn but allow
- Hot-reload changes template → existing components unchanged

---

## Feature 7: Level/Streaming Awareness

### User Value
Large scenes use level streaming and World Partition. Targets should automatically register/unregister as levels load/unload.

### Technical Design

#### Level Streaming Integration

```cpp
// RshipLevelStreamingManager.h
UCLASS(BlueprintType)
class URshipLevelStreamingManager : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(URshipSubsystem* Subsystem);
    void Shutdown();

    // Manual level operations
    UFUNCTION(BlueprintCallable, Category = "Rship|Levels")
    void RegisterLevel(ULevel* Level);

    UFUNCTION(BlueprintCallable, Category = "Rship|Levels")
    void UnregisterLevel(ULevel* Level);

    UFUNCTION(BlueprintCallable, Category = "Rship|Levels")
    void RegisterAllLoadedLevels();

    // Query
    UFUNCTION(BlueprintCallable, Category = "Rship|Levels")
    TArray<URshipTargetComponent*> GetTargetsInLevel(ULevel* Level);

    UFUNCTION(BlueprintCallable, Category = "Rship|Levels")
    TArray<FString> GetLoadedLevelsWithTargets();

    // Configuration
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAutoRegisterOnLevelLoad = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAutoUnregisterOnLevelUnload = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> ExcludedLevelPatterns;  // Levels to ignore

private:
    void OnLevelAdded(ULevel* Level, UWorld* World);
    void OnLevelRemoved(ULevel* Level, UWorld* World);
    void OnWorldPartitionCellLoaded(const FWorldPartitionStreamingSource& Source);
    void OnWorldPartitionCellUnloaded(const FWorldPartitionStreamingSource& Source);

    URshipSubsystem* Subsystem;
    TMap<ULevel*, TArray<URshipTargetComponent*>> LevelToTargets;
};
```

#### World Partition Support

```cpp
// For UE5 World Partition
void URshipLevelStreamingManager::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;

    if (UWorld* World = GEngine->GetWorldFromContextObject(Subsystem, EGetWorldErrorMode::LogAndReturnNull))
    {
        // Level streaming callbacks
        FWorldDelegates::LevelAddedToWorld.AddUObject(this, &URshipLevelStreamingManager::OnLevelAdded);
        FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &URshipLevelStreamingManager::OnLevelRemoved);

        // World Partition callbacks (UE5+)
        #if ENGINE_MAJOR_VERSION >= 5
        if (World->GetWorldPartition())
        {
            // Hook into World Partition streaming events
            // Note: API varies by UE5 version
        }
        #endif
    }
}

void URshipLevelStreamingManager::OnLevelAdded(ULevel* Level, UWorld* World)
{
    if (!bAutoRegisterOnLevelLoad)
        return;

    // Check exclusion patterns
    FString LevelName = Level->GetOuter()->GetName();
    for (const FString& Pattern : ExcludedLevelPatterns)
    {
        if (LevelName.MatchesWildcard(Pattern))
            return;
    }

    RegisterLevel(Level);
}

void URshipLevelStreamingManager::RegisterLevel(ULevel* Level)
{
    TArray<URshipTargetComponent*> LevelTargets;

    for (AActor* Actor : Level->Actors)
    {
        if (Actor)
        {
            if (URshipTargetComponent* Comp = Actor->FindComponentByClass<URshipTargetComponent>())
            {
                Comp->Register();
                LevelTargets.Add(Comp);
            }
        }
    }

    LevelToTargets.Add(Level, LevelTargets);

    UE_LOG(LogRshipExec, Log, TEXT("Registered %d targets from level %s"),
           LevelTargets.Num(), *Level->GetOuter()->GetName());
}

void URshipLevelStreamingManager::UnregisterLevel(ULevel* Level)
{
    if (TArray<URshipTargetComponent*>* Targets = LevelToTargets.Find(Level))
    {
        for (URshipTargetComponent* Comp : *Targets)
        {
            if (IsValid(Comp))
            {
                // Send "offline" status before unregistering
                // Mark target as unavailable in rship
            }
        }

        LevelToTargets.Remove(Level);
    }
}
```

### Implementation Steps

1. **Create URshipLevelStreamingManager** class
2. **Hook into level add/remove delegates**
3. **Implement auto-registration on level load**
4. **Implement auto-unregistration on level unload**
5. **Add World Partition support** (UE5)
6. **Create level exclusion patterns**
7. **Add "Register Entire Level" button** in editor
8. **Track which targets came from which level**
9. **Send target "offline" status before unload**

### Blueprint API

```
// Manual level control
Register Level(SubLevel) → Registers all targets in level
Unregister Level(SubLevel) → Unregisters all targets

// Query
Get Targets In Level(SubLevel) → Array of components
Get Loaded Levels With Targets → Level names
```

### Testing Strategy

| Test Case | Expected Behavior |
|-----------|-------------------|
| Level streams in | All targets auto-registered |
| Level streams out | All targets unregistered |
| Level excluded by pattern | No registration |
| World Partition cell loads | Targets in cell registered |
| Very fast load/unload | No race conditions |
| PIE with streaming | Streaming works correctly |
| Persistent level | Targets always registered |

### Edge Cases

- Target component added after level load → manual registration needed
- Level unloaded while sending registration → handle gracefully
- Same actor in multiple streaming levels → only one registration
- Level reload → re-register all targets
- Editor vs PIE streaming behavior differences

---

## Implementation Priority & Dependencies

### Phase 1: Foundation (Week 1-2)
1. **Feature 1: Groups & Tagging** - Foundation for organization
2. **Feature 5: Health Dashboard** - Immediate visibility improvement

Dependencies: None

### Phase 2: Bulk Operations (Week 3-4)
3. **Feature 2: Bulk Property Editing** - Requires groups/tagging
4. **Feature 3: In-Viewport Selection Sync** - Requires selection system

Dependencies: Feature 1

### Phase 3: Advanced Features (Week 5-6)
5. **Feature 6: Template System** - Requires understanding of registration
6. **Feature 7: Level Streaming** - Requires registration improvements

Dependencies: Features 1, 2

### Phase 4: Polish (Week 7-8)
7. **Feature 4: Presets & Snapshots** - Requires stable emitter system

Dependencies: All above

---

## Testing Strategy Overview

### Unit Tests
- Each manager class has isolated unit tests
- Mock subsystem for testing without network

### Integration Tests
- Full flow tests with real subsystem
- Network simulation for edge cases

### Performance Tests
- 10,000 target stress test
- 1,000 targets selected simultaneously
- 60Hz pulse rate sustained
- Memory leak detection

### User Acceptance Tests
- Artist workflow validation
- Feedback collection on UX

---

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| Performance regression | Benchmark before/after each feature |
| Breaking existing workflows | Backward compatibility layer |
| Complex merge conflicts | Feature branches, incremental PRs |
| Network protocol changes | Version negotiation with server |
| Memory growth | Periodic cleanup, weak references |

---

## Open Questions for User

1. **Persistence format**: Should groups/tags/presets persist to:
   - Project settings (DefaultGame.ini)?
   - Separate JSON file?
   - SaveGame object?
   - Sync with rship server?

2. **Editor vs Runtime**: Which features should work in:
   - Editor only (design time)?
   - PIE only (play in editor)?
   - Packaged game (runtime)?

3. **Blueprint vs C++ priority**: Should we prioritize:
   - Full Blueprint exposure for rapid iteration?
   - C++ API completeness for performance?

4. **Server sync**: Should tags/groups/presets sync with rship server for:
   - Multi-user collaboration?
   - UI visibility in rship web interface?
