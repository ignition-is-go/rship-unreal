# RshipExec Plugin Upgrade Guide

## Upgrading from Main Branch to High-Performance Branch

This guide helps users familiar with the main branch understand the enhanced features available in the `feature/high-perf-websocket` branch.

---

## What's Changed?

The plugin has been restructured from `Source/RshipExec/` to `Plugins/RshipExec/` and significantly enhanced with **39 new feature modules** while maintaining 100% backward compatibility with the original API.

### At a Glance

| Category | Main Branch | This Branch |
|----------|-------------|-------------|
| Source Files | 13 | 52+ |
| WebSocket | UE Built-in (30Hz throttle) | High-perf dedicated thread |
| Blueprint Functions | Basic | 60+ convenience functions |
| Rate Limiting | None | Token bucket with batching |
| Health Monitoring | None | Full dashboard |
| Target Organization | Basic | Groups, tags, bulk ops |
| Integrations | None | LiveLink, OSC, DMX, Control Rig, Niagara |
| Recording/Playback | None | Full recording system |
| Timecode | None | Multi-source sync |

---

## Part 1: Core Improvements

### 1.1 High-Performance WebSocket

**What Changed:** The original plugin used Unreal's built-in WebSocket module which has a 30Hz send throttle and Nagle's algorithm enabled. This caused noticeable latency in high-frequency pulse scenarios.

**New Behavior:**
- Dedicated send thread bypasses the 30Hz throttle
- TCP_NODELAY enabled by default (no Nagle buffering)
- Configurable auto-reconnect with exponential backoff
- Backpressure detection via `GetPendingSendCount()`

**Migration:** No code changes required. The new WebSocket is used automatically.

**New Configuration (Project Settings > Rocketship):**
```
bTcpNoDelay = true
bDisableCompression = true
PingIntervalSeconds = 30
bAutoReconnect = true
MinReconnectWaitSeconds = 1
MaxReconnectWaitSeconds = 60
```

---

### 1.2 Enhanced Settings

**What Changed:** Settings expanded from 20 lines to 260 lines with comprehensive configuration.

**New Settings Categories:**
- **Connection**: Host, port, TCP options, compression, heartbeat
- **Rate Limiting**: Message limits, queue sizes, timeouts
- **Batching**: Batch size, interval, critical message bypass
- **Bandwidth**: Bytes-per-second limiting with burst control
- **Priority**: Message prioritization and downsampling
- **Adaptive**: Dynamic rate adjustment
- **Backoff**: Reconnection and error recovery
- **Diagnostics**: Logging verbosity, metrics collection

**Access:** Project Settings > Game > Rocketship Settings

---

### 1.3 Rate Limiting & Message Batching

**Problem Solved:** High-frequency pulses could overwhelm the server or saturate bandwidth.

**New Features:**
- Token bucket rate limiting with configurable limits
- Automatic message batching (configurable size and interval)
- 4-level priority system: Critical > High > Normal > Low
- Message coalescing (same key = only latest value sent)
- Adaptive backpressure handling

**Usage:**
```cpp
// Messages now support priority and coalescing
Subsystem->SetItem("myType", JsonData, ERshipMessagePriority::High, "myCoalesceKey");
```

**Monitoring:**
```cpp
FRshipRateLimiterMetrics Metrics = Subsystem->GetRateLimiter()->GetMetrics();
// Metrics includes: messages sent, dropped, queue length, throughput, etc.
```

---

### 1.4 Health Monitoring

**Problem Solved:** No visibility into connection health or target activity during live operation.

**New Features:**
- Real-time connection status with latency tracking
- Per-target activity monitoring
- Automatic detection of inactive/failed targets
- Overall health score (0-100)
- Blueprint events for health state changes

**Usage:**
```cpp
// Get current health snapshot
FRshipHealthStatus Health = HealthMonitor->GetCurrentHealth();

// Find problematic targets
TArray<FString> Inactive = HealthMonitor->GetInactiveTargets(30.0f); // 30 sec threshold
TArray<FString> Errors = HealthMonitor->GetErrorTargets();

// Get most active targets
TArray<FRshipTargetActivity> Hot = HealthMonitor->GetHotTargets(10);
```

**Blueprint Events:**
- `OnConnectionLost`
- `OnConnectionRestored`
- `OnBackpressureWarning`
- `OnHealthChanged`

---

## Part 2: Target Organization

### 2.1 Target Groups

**Problem Solved:** Managing hundreds or thousands of targets was difficult.

**New Features:**
- Create named groups with colors
- Auto-populate groups by actor class or proximity
- Wildcard pattern matching for queries
- Persist groups to JSON files

**Usage:**
```cpp
// Create a group
FRshipTargetGroup Group;
Group.Id = "stage-left-lights";
Group.DisplayName = "Stage Left Lights";
Group.Color = FLinearColor::Blue;
GroupManager->CreateGroup(Group);

// Add targets
GroupManager->AddTargetToGroup("fixture-001", "stage-left-lights");

// Query by group
TArray<URshipTargetComponent*> Targets = GroupManager->GetTargetsByGroup("stage-left-lights");

// Query by pattern
TArray<URshipTargetComponent*> Moving = GroupManager->GetTargetsByPattern("moving-*");
```

### 2.2 Tags

**Problem Solved:** Need flexible, user-defined organization beyond groups.

**New Features:**
- Add arbitrary tags to any target
- Query targets by single tag or multiple tags (AND/OR)
- Bulk tag operations

**Usage:**
```cpp
// Add tags to a target component
TargetComponent->Tags.Add("downstage");
TargetComponent->Tags.Add("warm");

// Query by tags
TArray<URshipTargetComponent*> Warm = GroupManager->GetTargetsByTag("warm");
TArray<URshipTargetComponent*> DownstageWarm = GroupManager->GetTargetsByTags({"downstage", "warm"}); // AND
TArray<URshipTargetComponent*> Either = GroupManager->GetTargetsByAnyTag({"warm", "cool"}); // OR
```

### 2.3 Bulk Operations

**Problem Solved:** Editing many targets one-by-one was tedious.

**New Features:**
- Selection management (select by tag, group, pattern)
- Bulk tag add/remove/replace
- Bulk group membership changes
- Copy/paste target configuration
- Find and replace in names/tags

**Usage:**
```cpp
// Select all fixtures matching a pattern
URshipBulkOperations::SelectTargetsByPattern(World, "fixture-*");

// Bulk add a tag to selection
int32 Count = URshipBulkOperations::BulkAddTag(World, "needs-focus");

// Copy config from one target, paste to selection
FRshipTargetConfig Config = URshipBulkOperations::CopyTargetConfig(SourceTarget);
URshipBulkOperations::PasteTargetConfigToTargets(SelectedTargets, Config);
```

---

## Part 3: Recording & Playback

### 3.1 Pulse Recording

**Problem Solved:** No way to capture pulse data for previz, debugging, or rehearsal.

**New Features:**
- Record all pulse data with timestamps
- Filter by target pattern or max rate
- Playback at variable speed with looping
- Save/load recordings to disk

**Usage:**
```cpp
// Start recording
FRshipRecordingFilter Filter;
Filter.TargetPatterns.Add("fixture-*");
Filter.MaxPulsesPerSecond = 60.0f;
Recorder->StartRecording(Filter);

// ... time passes ...

// Stop and save
Recorder->StopRecording();
Recorder->SaveRecording("rehearsal-2024-01-15");

// Later: playback
Recorder->LoadRecording("rehearsal-2024-01-15");
FRshipPlaybackOptions Options;
Options.PlaybackSpeed = 1.0f;
Options.bLoop = true;
Recorder->StartPlayback(Options);
```

### 3.2 Presets

**Problem Solved:** No way to save and recall emitter states.

**New Features:**
- Capture current state as named preset
- Recall with optional fade/interpolation
- Crossfade between presets
- Save presets to JSON files for sharing

**Usage:**
```cpp
// Capture current state
PresetManager->CapturePreset("look-1", "First Look", {"lighting"});

// Recall with 2-second fade
PresetManager->RecallPresetWithFade("look-1", 2.0f);

// Crossfade between presets
PresetManager->CrossfadePresets("look-1", "look-2", 3.0f);
```

---

## Part 4: Timecode & Synchronization

### 4.1 Timecode Sync

**Problem Solved:** No synchronization with external timecode sources.

**Supported Sources:**
- Internal (Unreal clock)
- Rship server timecode
- LTC (Linear Timecode audio)
- MTC (MIDI Timecode)
- Art-Net timecode
- PTP (Precision Time Protocol)
- NTP
- Manual

**Usage:**
```cpp
// Set timecode source
TimecodeSync->SetTimecodeSource(ERshipTimecodeSource::Rship);

// Playback control
TimecodeSync->Play();
TimecodeSync->SeekToTimecode(FTimecode(1, 0, 0, 0)); // 1:00:00:00

// Add cue points
FRshipCuePoint Cue;
Cue.Id = "intro-start";
Cue.Timecode = FTimecode(0, 0, 30, 0);
Cue.PreRollFrames = 5;
TimecodeSync->AddCuePoint(Cue);
```

**Blueprint Events:**
- `OnTimecodeChanged` (every frame)
- `OnCuePointReached`
- `OnStateChanged`
- `OnSyncStatusChanged`

### 4.2 Sequencer Sync

**Problem Solved:** Sequencer playback wasn't synchronized with rship timecode.

**Sync Modes:**
- `FollowTimecode`: Sequencer follows rship timecode
- `DriveTimecode`: Sequencer drives rship timecode
- `Bidirectional`: Two-way sync

**Usage:**
```cpp
// Quick sync a sequence to timecode
SequencerSync->QuickSyncSequence(MyLevelSequence, FTimecode(0, 1, 0, 0));

// Set sync mode
SequencerSync->SetSyncMode(ERshipSequencerSyncMode::FollowTimecode);
```

---

## Part 5: External Integrations

### 5.1 Live Link

**Purpose:** Expose rship data as Live Link subjects for streaming to other systems.

**Subject Types:**
- Transform (position/rotation/scale)
- Camera (FOV, focus, aperture)
- Light (intensity, color, cone angles)
- Animation (bone transforms)

**Usage:**
```cpp
// Start Live Link source
LiveLinkService->StartSource();

// Add subjects
LiveLinkService->AddTransformSubject("fixture-001", Config);
LiveLinkService->AddCameraSubject("camera-main", CameraConfig);

// Quick setup from fixtures
LiveLinkService->CreateSubjectsFromFixtures();
```

### 5.2 OSC Bridge

**Purpose:** Bidirectional communication with OSC controllers (TouchOSC, QLab, etc.).

**Features:**
- Wildcard address pattern matching
- Value transforms (scale, remap, invert)
- Pre-built mappings for TouchOSC and QLab

**Usage:**
```cpp
// Start OSC server
OSCBridge->StartServer(8000);

// Add mapping
FRshipOSCMapping Mapping;
Mapping.AddressPattern = "/fixture/*/intensity";
Mapping.EmitterId = "intensity";
OSCBridge->AddMapping(Mapping);

// Send messages
OSCBridge->SendFloat("/cue/go", 1.0f, Destination);

// Quick setup
OSCBridge->CreateTouchOSCMappings();
```

### 5.3 DMX Output

**Purpose:** Send fixture values to real DMX universes via Art-Net or sACN.

**Features:**
- Art-Net and sACN protocol support
- Fixture profiles with channel layouts
- Per-fixture master dimmer
- Global blackout control
- 16-bit channel support

**Usage:**
```cpp
// Configure output
DMXOutput->SetProtocol(ERshipDMXProtocol::ArtNet);
DMXOutput->SetArtNetDestination("192.168.1.100", 0); // Universe 0

// Map fixture to DMX
FRshipDMXFixtureOutput Output;
Output.FixtureId = "fixture-001";
Output.Universe = 0;
Output.StartAddress = 1;
Output.ProfileId = "generic-rgb";
DMXOutput->AddFixtureOutput(Output);

// Auto-map all fixtures
DMXOutput->AutoMapAllFixtures();

// Blackout
DMXOutput->Blackout();
DMXOutput->ReleaseBlackout();
```

### 5.4 Control Rig Binding

**Purpose:** Drive Control Rig parameters from rship pulses for procedural animation.

**Mapping Functions:**
- Direct (1:1)
- Remap (input range to output range)
- Curve (float curve asset)
- Expression (math expression)

**Interpolation Modes:**
- None, Linear, EaseIn, EaseOut, EaseInOut, Spring

**Usage:**
```cpp
// Bind intensity emitter to control property
ControlRigBinding->BindIntensityToFloat("arm_length", 0.0f, 100.0f);

// Bind color to vector property
ControlRigBinding->BindColorToVector("tint_color");

// Auto-generate bindings from emitter names
ControlRigBinding->AutoGenerateBindings();
```

### 5.5 Niagara Binding

**Purpose:** Drive Niagara VFX parameters from rship pulses.

**Binding Modes:**
- Direct, Normalized, Scaled, Mapped, Curve, Trigger

**Usage:**
```cpp
// Bind float parameter
FRshipNiagaraParameterBinding Binding;
Binding.EmitterId = "intensity";
Binding.ParameterName = "SpawnRate";
Binding.Mode = ERshipNiagaraBindingMode::Scaled;
Binding.Scale = 1000.0f;
NiagaraBinding->AddBinding(Binding);
```

### 5.6 Material Binding

**Purpose:** Drive material parameters from rship pulses.

**Parameter Types:**
- Scalar (float with curve/remap)
- Vector (color with HDR support)
- Texture (texture switching)

**Usage:**
```cpp
// Bind intensity to scalar parameter
FRshipMaterialScalarBinding ScalarBinding;
ScalarBinding.EmitterId = "intensity";
ScalarBinding.ParameterName = "Emissive_Intensity";
MaterialBinding->AddScalarBinding(ScalarBinding);

// Global controls
MaterialManager->SetGlobalIntensityMultiplier(0.5f);
MaterialManager->SetGlobalColorTint(FLinearColor::Red);
```

---

## Part 6: Fixture & Camera Systems

### 6.1 Fixture Library (GDTF Support)

**Purpose:** Manage fixture type definitions with GDTF import support.

**Features:**
- Complete fixture profiles (beam, color, movement, gobos, DMX modes)
- GDTF file import
- Server sync for shared libraries
- Search by manufacturer, category, or tags

**Usage:**
```cpp
// Import GDTF files
FixtureLibrary->ImportGDTF("/path/to/fixture.gdtf");
FixtureLibrary->ImportGDTFDirectory("/path/to/gdtf/");

// Search
TArray<FRshipFixtureProfile> Moving = FixtureLibrary->GetProfilesByCategory(ERshipFixtureCategory::MovingHead);
TArray<FRshipFixtureProfile> Robe = FixtureLibrary->GetProfilesByManufacturer("Robe");
```

### 6.2 Fixture Visualization

**Visualization Modes:**
- None
- BeamCone (simple cone)
- BeamVolume (volumetric)
- Symbol (2D icon)
- Full (beam + symbol)

**Quality Levels:** Low, Medium, High, Ultra

**Usage:**
```cpp
// Set visualization mode
Visualizer->SetVisualizationMode(ERshipVisualizationMode::BeamVolume);

// Apply preset
VisualizationManager->ApplyProgrammingPreset(); // Full viz for programming
VisualizationManager->ApplyShowPreset();        // Minimal for performance

// Global controls
VisualizationManager->SetGlobalBeamOpacity(0.5f);
VisualizationManager->SetGlobalBeamLength(10.0f);
```

### 6.3 IES Profile Service

**Purpose:** Download and cache IES photometric profiles for accurate beam simulation.

**Usage:**
```cpp
// Load profile (async with caching)
IESService->LoadProfile("https://assets.rship.io/ies/profile.ies",
    FOnIESProfileLoaded::CreateLambda([](const FRshipIESProfile& Profile) {
        // Use profile
    }));

// Generate UE light profile texture
UTextureLightProfile* Texture = IESService->GenerateLightProfileTexture(Profile);
```

### 6.4 Multi-Camera Management

**Purpose:** Virtual production multi-camera switching with transitions.

**Features:**
- Program/Preview view model
- Cut, dissolve, wipe transitions
- Tally state management
- Auto-switching rules
- Recording sessions

**Usage:**
```cpp
// Add camera views
FRshipCameraView View;
View.Id = "cam-1";
View.DisplayName = "Camera 1";
View.CameraActor = MyCameraActor;
MultiCameraManager->AddView(View);

// Switch with transition
FRshipCameraTransition Transition;
Transition.Type = ERshipTransitionType::Dissolve;
Transition.Duration = 1.0f;
MultiCameraManager->SwitchWithTransition("cam-2", Transition);

// Live switching
MultiCameraManager->SetPreviewView("cam-2");
MultiCameraManager->Take(); // Preview becomes program
```

---

## Part 7: Blueprint Library

All major features are exposed through `URshipBlueprintLibrary` with 60+ static functions:

### Connection
- `IsConnectedToRship()`
- `ReconnectToRship()`
- `GetRshipServiceId()`

### Targets
- `GetAllTargets()`
- `FindTargetById()`
- `PulseEmitter()`

### Fixtures
- `GetAllFixtures()`
- `SetFixtureIntensity()`
- `SetFixtureColor()`

### Cameras
- `SwitchCameraView()`
- `GetProgramView()`
- `GetPreviewView()`

### Timecode
- `GetCurrentTimecode()`
- `SeekToTimecode()`
- `SetPlaybackSpeed()`

### And many more...

---

## Migration Checklist

1. **No breaking changes** - All original API functions work unchanged
2. **Update includes** - Path changed from `Source/RshipExec/` to `Plugins/RshipExec/Source/RshipExec/`
3. **Review settings** - Check Project Settings > Rocketship for new options
4. **Optional: Enable features** - New systems are opt-in; enable what you need

## Getting Help

- Check the header files for detailed documentation on each class
- All Blueprint-exposed functions have tooltips
- Events and delegates are documented in header comments

---

*This guide covers the major new features. Each header file contains additional documentation for advanced usage.*
