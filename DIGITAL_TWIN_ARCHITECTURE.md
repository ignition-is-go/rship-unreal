# Rship-UE Digital Twin Architecture

## Vision

The ultimate visualization interface where **rship owns all data** and **UE provides photorealistic, calibrated rendering**. Every light, camera, and surface in UE reflects the true physical characteristics and real-time state from rship.

---

## 1. Data Subscription Layer

### Entity Subscriptions
UE subscribes to rship entity streams to maintain synchronized state:

```
Fixtures       → ARshipFixtureActor instances
FixtureTypes   → Light profiles, beam characteristics
Cameras        → ARshipCameraActor instances
ColorProfiles  → Post-process volumes, LUTs
Calibrations   → Intensity curves, falloff, color temp
IESProfiles    → UTextureLightProfile assets
Surfaces       → Material properties (new entity type)
Environments   → Atmospheric settings (new entity type)
```

### Real-Time Value Subscriptions
Beyond entity state, subscribe to live control values:

```
DMX Universe streams    → Per-fixture intensity/color/position
Pulse streams          → Emitter values for bound fixtures
Timeline playhead      → Current cue state
Scene activation       → Which bindings are live
```

### Subscription Architecture
```cpp
UCLASS()
class URshipDataSubscription : public UObject
{
    // What we're subscribed to
    TSet<FString> SubscribedEntityTypes;
    TSet<FString> SubscribedUniverses;
    TSet<FString> SubscribedEmitters;

    // Callbacks
    FOnEntityChanged OnEntityChanged;
    FOnDMXReceived OnDMXReceived;
    FOnPulseReceived OnPulseReceived;

    // Automatic actor spawning/despawning
    bool bAutoSpawnActors = true;
    bool bAutoDestroyOnDelete = true;
};
```

---

## 2. Calibrated Rendering Pipeline

### The Problem
Raw DMX value 128 → ??? actual light output

A real fixture at DMX 128 might output:
- 47% of max lumens (due to dimmer curve)
- At 3200K (not 5600K like UE default)
- With specific beam/field angles
- With manufacturer-specific falloff

### The Solution: Calibration Application Stack

```
Raw DMX Value (0-255)
    ↓
[Dimmer Curve LUT] → Calibrated intensity (0.0-1.0)
    ↓
[Color Temperature Mapping] → Calibrated color (FLinearColor)
    ↓
[Beam Profile / IES] → Spatial distribution
    ↓
[Falloff Exponent] → Distance attenuation
    ↓
UE Light Component with physically accurate output
```

### Implementation
```cpp
UCLASS()
class URshipLightCalibrationApplicator : public UObject
{
public:
    // Apply full calibration stack to a light component
    void ApplyCalibration(
        ULightComponent* Light,
        const FRshipFixtureCalibration& Calibration,
        const FRshipDMXValues& DMXValues
    );

    // Individual transforms
    float ApplyDimmerCurve(int32 DMXValue, const TArray<FVector2D>& Curve);
    FLinearColor ApplyColorTemperature(float Kelvin, const FRshipColorProfile& Profile);
    void ApplyBeamProfile(USpotLightComponent* Spot, const FRshipIESProfile& IES);
};
```

---

## 3. Live DMX Visualization

### DMX Universe Subscription
```cpp
UCLASS()
class URshipDMXReceiver : public UObject
{
    // Subscribe to universes
    void SubscribeToUniverse(int32 Universe);
    void UnsubscribeFromUniverse(int32 Universe);

    // Get current values
    TArray<uint8> GetUniverseValues(int32 Universe) const;
    uint8 GetChannelValue(int32 Universe, int32 Channel) const;

    // Events
    FOnUniverseUpdated OnUniverseUpdated;  // Fires on any change
    FOnChannelChanged OnChannelChanged;    // Fires per-channel

private:
    // Ring buffer for each universe
    TMap<int32, TArray<uint8>> UniverseBuffers;

    // Throttling - don't update faster than render
    float MinUpdateInterval = 0.016f;  // ~60Hz max
};
```

### Fixture DMX Binding
```cpp
UCLASS()
class URshipFixtureDMXBinding : public UActorComponent
{
    UPROPERTY()
    FString FixtureId;

    UPROPERTY()
    int32 Universe;

    UPROPERTY()
    int32 StartAddress;

    UPROPERTY()
    FRshipDMXPersonality Personality;  // Channel mapping

    // Called when DMX values update
    void OnDMXValuesChanged(const TArray<uint8>& Values);

    // Apply to owning actor's light component
    void ApplyToLight();
};
```

### Channel Personalities
Different fixtures use DMX channels differently:

```cpp
USTRUCT()
struct FRshipDMXPersonality
{
    // Standard channels
    int32 IntensityChannel = 0;
    int32 RedChannel = -1;       // -1 = not present
    int32 GreenChannel = -1;
    int32 BlueChannel = -1;
    int32 WhiteChannel = -1;
    int32 ColorTempChannel = -1;
    int32 PanChannel = -1;
    int32 TiltChannel = -1;
    int32 ZoomChannel = -1;
    int32 FocusChannel = -1;
    int32 IrisChannel = -1;
    int32 GoboChannel = -1;

    // 16-bit handling
    bool bIntensity16Bit = false;
    bool bPanTilt16Bit = false;

    // Total footprint
    int32 ChannelCount = 1;
};
```

---

## 4. Multi-Camera System

### Camera Entity Sync
Each rship Camera becomes an ARshipCameraActor with:
- Position/rotation from calibration
- FOV from lens calibration
- Color correction from ColorProfile
- Distortion coefficients for accurate matching

### Camera Preview Viewports
```cpp
UCLASS()
class URshipCameraPreviewManager : public UObject
{
    // Active camera previews
    TMap<FString, UTextureRenderTarget2D*> CameraPreviews;

    // Render a camera's view to texture
    void RenderCameraPreview(const FString& CameraId, FIntPoint Resolution);

    // Get preview for UI display
    UTexture* GetPreviewTexture(const FString& CameraId);

    // Comparison mode
    void SetComparisonReference(const FString& CameraId, UTexture2D* ReferencePhoto);
    void SetComparisonMode(ERshipComparisonMode Mode);  // SideBySide, Overlay, Diff
};
```

### Color-Accurate Rendering
```cpp
UCLASS()
class ARshipCameraActor : public ACameraActor
{
    // Color profile for this camera
    UPROPERTY()
    FString ColorProfileId;

    // Post-process for color correction
    UPROPERTY()
    UPostProcessComponent* ColorCorrectionVolume;

    // Apply color profile as post-process
    void ApplyColorProfile(const FRshipColorProfile& Profile);

    // Generate LUT from color profile
    UTexture2D* GenerateColorLUT(const FRshipColorProfile& Profile);
};
```

---

## 5. Temporal Visualization

### Timeline Integration
```cpp
UCLASS()
class URshipTimelineController : public UObject
{
    // Current playback state
    UPROPERTY(BlueprintReadOnly)
    float CurrentTime;

    UPROPERTY(BlueprintReadOnly)
    bool bIsPlaying;

    UPROPERTY(BlueprintReadOnly)
    float PlaybackRate = 1.0f;

    // Timeline bounds
    float StartTime;
    float EndTime;

    // Scrubbing
    UFUNCTION(BlueprintCallable)
    void SeekToTime(float Time);

    UFUNCTION(BlueprintCallable)
    void Play();

    UFUNCTION(BlueprintCallable)
    void Pause();

    // Preview mode - request server to send cue state at time
    void PreviewCueAtTime(float Time);

    // Events
    FOnTimeChanged OnTimeChanged;
    FOnCueTriggered OnCueTriggered;
};
```

### Event Track Visualization
```cpp
UCLASS()
class URshipEventTrackVisualizer : public UObject
{
    // Visualize which fixtures are affected at current time
    void HighlightAffectedFixtures(float Time);

    // Show event flow: Trigger → Binding → Action
    void VisualizeEventChain(const FString& TriggerId);

    // Ghost positions - show where fixtures will be at future time
    void ShowGhostPositions(float FutureTime);
};
```

---

## 6. Fixture Library System

### Fixture Type Database
```cpp
USTRUCT()
struct FRshipFixtureTypeDefinition
{
    FString Id;
    FString Manufacturer;
    FString Model;
    FString Category;  // "Moving Head", "PAR", "Fresnel", etc.

    // Physical characteristics
    float MaxLumens;
    float MinColorTemp;
    float MaxColorTemp;
    float BeamAngle;
    float FieldAngle;
    float MaxZoomAngle;

    // DMX personalities
    TArray<FRshipDMXPersonality> Personalities;

    // Default IES profile ID (if available)
    FString DefaultIESProfileId;

    // 3D model for visualization
    FString MeshAssetPath;

    // Photometric data
    FString PhotometricDataUrl;
};
```

### Fixture Spawning from Library
```cpp
UCLASS()
class URshipFixtureLibrary : public UObject
{
    // Available fixture types
    TArray<FRshipFixtureTypeDefinition> FixtureTypes;

    // Spawn a fixture of given type
    ARshipFixtureActor* SpawnFixture(
        const FString& FixtureTypeId,
        FVector Location,
        FRotator Rotation
    );

    // Auto-configure UE light from fixture type
    void ConfigureLightFromFixtureType(
        ULightComponent* Light,
        const FRshipFixtureTypeDefinition& Type
    );

    // Import fixture definition from GDTF/MVR
    bool ImportFromGDTF(const FString& FilePath);
    bool ImportFromMVR(const FString& FilePath);
};
```

---

## 7. Environmental Simulation

### Atmospheric Effects
```cpp
UCLASS()
class URshipAtmosphereManager : public UObject
{
    // Haze/fog density (affects light visibility)
    UPROPERTY(EditAnywhere)
    float HazeDensity = 0.0f;

    // Atmospheric fog component
    UPROPERTY()
    UExponentialHeightFogComponent* FogComponent;

    // Volumetric rendering for light beams
    UPROPERTY()
    bool bEnableVolumetricBeams = true;

    // Sync from rship Environment entity
    void SyncFromEnvironment(const FRshipEnvironment& Env);
};
```

### Surface Materials
```cpp
USTRUCT()
struct FRshipSurfaceDefinition
{
    FString Id;
    FString Name;

    // Physical properties
    float Reflectivity;
    float Roughness;
    FLinearColor BaseColor;

    // For projection surfaces
    float Gain;  // Screen gain for projection
    bool bIsProjectionSurface;
};
```

---

## 8. Session & Context Awareness

### Project/Session Binding
```cpp
UCLASS()
class URshipSessionManager : public UObject
{
    // Current context
    FString OrganizationId;
    FString ProjectId;
    FString SessionId;

    // Session state
    bool bIsConnected;
    bool bIsAuthenticated;

    // Join a specific session
    void JoinSession(const FString& SessionId);
    void LeaveSession();

    // Events
    FOnSessionJoined OnSessionJoined;
    FOnSessionLeft OnSessionLeft;
    FOnProjectChanged OnProjectChanged;
};
```

### Offline Mode
```cpp
UCLASS()
class URshipOfflineCache : public UObject
{
    // Cache entities locally for offline work
    void CacheEntity(const FString& Type, const TSharedPtr<FJsonObject>& Data);

    // Load cached data
    bool LoadCachedEntities();

    // Sync changes when reconnected
    void SyncPendingChanges();

    // Conflict resolution
    FOnSyncConflict OnSyncConflict;
};
```

---

## 9. Visualization Modes

### Mode System
```cpp
UENUM()
enum class ERshipVisualizationMode : uint8
{
    // Normal rendering with calibrated lights
    Realistic,

    // Show fixture positions without light simulation
    Schematic,

    // Highlight specific aspects
    ShowBeamCones,
    ShowDMXAddresses,
    ShowFixtureTypes,
    ShowGroupMembership,

    // Comparison modes
    CompareToReference,
    ShowDeltaFromTarget,

    // Debug modes
    ShowCalibrationData,
    ShowEventFlow,
    ShowBindingConnections
};
```

### Selection & Highlighting
```cpp
UCLASS()
class URshipSelectionVisualizer : public UObject
{
    // Highlight fixtures by various criteria
    void HighlightByGroup(const FString& GroupId, FLinearColor Color);
    void HighlightByType(const FString& FixtureTypeId, FLinearColor Color);
    void HighlightByUniverse(int32 Universe, FLinearColor Color);
    void HighlightByTag(const FString& Tag, FLinearColor Color);

    // Show connections
    void ShowBindingConnections(const FString& BindingId);
    void ShowGroupConnections(const FString& GroupId);

    // Clear all highlights
    void ClearHighlights();
};
```

---

## 10. Implementation Phases

### Phase 1: Core Data Sync (Current)
- [x] Fixture/Camera entity sync
- [x] Position sync (editor → server)
- [x] Basic calibration application
- [x] IES profile loading

### Phase 2: Live Control Values
- [ ] DMX universe subscription
- [ ] Real-time value application
- [ ] Channel personality mapping
- [ ] Calibrated intensity output

### Phase 3: Multi-Camera & Color
- [ ] Camera preview rendering
- [ ] Color profile application
- [ ] LUT generation
- [ ] Reference comparison

### Phase 4: Timeline Integration
- [ ] Timeline scrubbing
- [ ] Cue preview
- [ ] Event visualization
- [ ] Ghost positions

### Phase 5: Fixture Library
- [ ] GDTF/MVR import
- [ ] Fixture type database
- [ ] Auto-configuration
- [ ] 3D fixture meshes

### Phase 6: Environment & Polish
- [ ] Atmospheric effects
- [ ] Surface materials
- [ ] Visualization modes
- [ ] Offline support

---

## Key Design Principles

1. **rship is always the source of truth** - UE never invents data
2. **Calibration bridges virtual to physical** - Every value passes through calibration
3. **Real-time is reactive** - Subscribe, don't poll
4. **Visualization serves understanding** - Modes help users comprehend complex systems
5. **Offline-first for reliability** - Cache aggressively, sync gracefully
6. **Performance at scale** - Design for 1000s of fixtures, not 10s
