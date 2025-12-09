# Calibration Data Integration Plan

## Overview

This plan details how lighting and camera calibration data flows from rship server to both the **Unreal Engine Visualizer** (rship-unreal plugin) and the **Web Visualizer** (Threlte/WebGPU). The goal is to ensure both visualizers render fixtures and cameras with identical calibration-accurate behavior.

---

## Current State

### Web Visualizer (rship apps/ui) ✅ COMPLETE
- **Entity-based services (Kafka-persisted):**
  - `fixtureCalibrationService.ts` - uses `FixtureCalibration` entity from `@rship/entities-core`
  - `cameraCalibrationService.ts` - uses `ColorProfile` entity from `@rship/entities-core`
  - `photometricService.ts` - IES/LDT file loading and texture generation
  - `WebGPUFixtureModel.svelte` - applies calibration to 3D fixture rendering
  - `WebGPUCameraModel.svelte` - renders calibrated cameras with FOV visualization

- **Data storage:** Server-side via Kafka event sourcing ✅
- **Real-time sync:** RxJS observables with entity subscriptions ✅

### UE Plugin (rship-unreal)
- **Already implemented:**
  - WebSocket connection to rship server
  - Target/Emitter/Action system
  - Manager subsystems (Groups, Health, Presets, Templates, Levels, DataLayers)

- **To implement:**
  - Fixture entity support
  - Camera entity support
  - Calibration entity subscription
  - 3D fixture/camera visualization components

---

## Data Flow Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           RSHIP SERVER                                       │
│  ┌─────────────┐  ┌──────────────┐  ┌─────────────────────────────────────┐ │
│  │ FixtureType │  │   Fixture    │  │  FixtureCalibration (NEW ENTITY)    │ │
│  │   Entity    │  │    Entity    │  │  - fixtureTypeId                    │ │
│  │             │  │              │  │  - dimmerCurve[]                    │ │
│  │ - beamAngle │  │ - position   │  │  - colorCalibrations[]              │ │
│  │ - fieldAngle│  │ - rotation   │  │  - beamAngleMultiplier              │ │
│  │ - colorTemp │  │ - dmxPatch   │  │  - fieldAngleMultiplier             │ │
│  │ - iesUrl    │  │ - fixtureType│  │  - falloffExponent                  │ │
│  └─────────────┘  └──────────────┘  └─────────────────────────────────────┘ │
│                                                                              │
│  ┌─────────────┐  ┌──────────────┐  ┌─────────────────────────────────────┐ │
│  │   Camera    │  │ Calibration  │  │   CameraProfile (NEW ENTITY)        │ │
│  │   Entity    │  │   Entity     │  │   - whiteBalance                    │ │
│  │             │  │              │  │   - colorCheckerMatrix              │ │
│  │ - position  │  │ - cameraId   │  │   - recommendedExposure             │ │
│  │ - rotation  │  │ - savedResult│  │                                     │ │
│  │ - fov       │  │ - distortion │  │                                     │ │
│  └─────────────┘  └──────────────┘  └─────────────────────────────────────┘ │
│                                                                              │
│                    WebSocket (Event Sourcing)                                │
│                              │                                               │
└──────────────────────────────┼───────────────────────────────────────────────┘
                               │
        ┌──────────────────────┼──────────────────────┐
        │                      │                      │
        ▼                      ▼                      ▼
┌───────────────┐     ┌───────────────┐      ┌───────────────┐
│  Web UI       │     │   UE Plugin   │      │   Executors   │
│  (Threlte)    │     │               │      │               │
│               │     │               │      │               │
│ WebGPUFixture │     │ URshipFixture │      │ (Push DMX     │
│ Model.svelte  │     │ Component     │      │  state via    │
│               │     │               │      │  Pulses)      │
└───────────────┘     └───────────────┘      └───────────────┘
```

---

## Phase 1: Server-Side Calibration Entities ✅ COMPLETE

### 1.1 Entities (Now in Kafka)

**FixtureCalibration Entity** (`@rship/entities-core`)
```typescript
export class FixtureCalibration extends MItem<FixtureCalibration> {
  name!: string
  fixtureTypeId!: ID           // Per fixture-type calibration
  projectId!: ID               // Organization-level scope

  // Intensity calibration
  dimmerCurve!: DimmerCurvePoint[]  // [{dmxValue: 0-255, outputPercent: 0-1}]
  minVisibleDmx!: number

  // Color calibration
  colorCalibrations!: ColorCalibration[]
  actualWhitePoint?: number  // Kelvin

  // Beam calibration
  beamAngleMultiplier!: number  // 1.0 = no adjustment
  fieldAngleMultiplier!: number
  falloffExponent!: number

  // Reference data
  referencePhotoUrl?: string
  notes?: string
}

interface DimmerCurvePoint {
  dmxValue: number   // 0-255
  outputPercent: number  // 0-1
}

interface ColorCalibration {
  targetKelvin: number
  measuredKelvin: number
  chromaticityOffset: { x: number; y: number }  // CIE xy
  rgbCorrection: { r: number; g: number; b: number }
}
```

**Commands:**
- `CreateFixtureCalibration(projectId, fixtureTypeId, name)`
- `DeleteFixtureCalibration(id)`
- `RenameFixtureCalibration(id, name)`
- `SetFixtureCalibrationDimmerCurve(id, dimmerCurve[])`
- `SetFixtureCalibrationDimmerCurvePreset(id, preset)` - square, linear, sCurve
- `SetFixtureCalibrationColorCalibrations(id, colorCalibrations[])`
- `SetFixtureCalibrationBeamProfile(id, beamAngleMultiplier, fieldAngleMultiplier, falloffExponent)`
- `SetFixtureCalibrationReference(id, referencePhotoUrl, notes)`

**ColorProfile Entity** (`@rship/entities-core`)
```typescript
export class ColorProfile extends MItem<ColorProfile> {
  name!: string
  projectId!: ID
  manufacturer?: string
  model?: string
  cameraId?: ID               // Optional association with Camera entity

  // White balance calibration
  whiteBalance?: WhiteBalanceData

  // Color checker calibration
  colorChecker?: ColorCheckerData

  // Recommended capture settings
  recommendedExposure?: ExposureData
}

interface WhiteBalanceData {
  kelvin: number              // Estimated color temperature (e.g., 6500)
  tint: number                // Green-magenta tint correction
  measuredGray: RGBColor      // What we measured from gray card
  multipliers: RGBColor       // Correction multipliers (r, g, b)
  calibratedAt: string        // ISO timestamp
}

interface ColorCheckerData {
  measuredPatches: Partial<Record<ColorCheckerPatch, RGBColor>>
  colorMatrix: number[][]     // 3x3 correction matrix
  calibratedAt: string
  deltaE: number              // Average Delta E (calibration quality)
  maxDeltaE: number           // Worst-case Delta E
  patchDeltaEs: Partial<Record<ColorCheckerPatch, number>>
}

interface ExposureData {
  iso: number
  shutterSpeed: string
  aperture: number
  whiteBalanceKelvin: number
}

type ColorCheckerPatch =
  | 'darkSkin' | 'lightSkin' | 'blueSky' | 'foliage' | 'blueFlower' | 'bluishGreen'
  | 'orange' | 'purplishBlue' | 'moderateRed' | 'purple' | 'yellowGreen' | 'orangeYellow'
  | 'blue' | 'green' | 'red' | 'yellow' | 'magenta' | 'cyan'
  | 'white' | 'neutral8' | 'neutral65' | 'neutral5' | 'neutral35' | 'black'
```

**Commands:**
- `CreateColorProfile(projectId, name, manufacturer?, model?, cameraId?)`
- `DeleteColorProfile(id)`
- `RenameColorProfile(id, name)`
- `SetColorProfileCamera(id, cameraId)`
- `SetColorProfileWhiteBalance(id, whiteBalance)`
- `SetColorProfileColorChecker(id, colorChecker)`
- `SetColorProfileExposure(id, exposure)`

### 1.2 Web Service Migration ✅ COMPLETE

Both services now use entity-based storage:

**fixtureCalibrationService.ts:**
```typescript
// RxJS Observables for reactive subscriptions
export const fixtureCalibrations: Observable<FixtureCalibration[]>
export const fixtureCalibrationsById: Observable<Map<ID, FixtureCalibration>>
export const fixtureCalibrationsByFixtureType: Observable<Map<ID, FixtureCalibration[]>>

// Helper functions (to replicate in UE)
export function dmxToOutput(calibration: FixtureCalibration | null, dmxValue: number): number
export function getColorCorrection(calibration: FixtureCalibration | null, targetKelvin: number): RGBColor
export function getCalibratedBeamAngle(calibration: FixtureCalibration | null, specBeamAngle: number): number
export function getCalibratedFieldAngle(calibration: FixtureCalibration | null, specFieldAngle: number): number
export function getFalloffExponent(calibration: FixtureCalibration | null): number
```

**cameraCalibrationService.ts:**
```typescript
// RxJS Observables
export const colorProfiles: Observable<ColorProfile[]>
export const colorProfilesById: Observable<Map<ID, ColorProfile>>
export const activeColorProfile: Observable<ColorProfile | null>

// Helper functions (to replicate in UE)
export function applyColorCorrection(profile: ColorProfile, color: RGBColor): RGBColor
export function getCalibrationQuality(deltaE: number): 'excellent' | 'good' | 'acceptable' | 'poor'
```

---

## Phase 2: UE Plugin Implementation

### 2.1 New UE Components

**URshipFixtureManager** - Manages fixture entities
```cpp
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipFixtureManager : public UObject
{
    // Query fixtures by project
    TArray<FRshipFixtureInfo> GetFixturesByProject(const FString& ProjectId);

    // Get fixture type details (beam angles, IES, etc.)
    FRshipFixtureTypeInfo GetFixtureType(const FString& FixtureTypeId);

    // Get calibration for fixture type
    FRshipFixtureCalibration GetCalibration(const FString& FixtureTypeId);

    // Events
    UPROPERTY(BlueprintAssignable)
    FOnFixturesUpdated OnFixturesUpdated;

    UPROPERTY(BlueprintAssignable)
    FOnCalibrationUpdated OnCalibrationUpdated;
};
```

**URshipCameraManager** - Manages camera entities
```cpp
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipCameraManager : public UObject
{
    // Query cameras by project
    TArray<FRshipCameraInfo> GetCamerasByProject(const FString& ProjectId);

    // Get calibration result
    FRshipCameraCalibration GetCameraCalibration(const FString& CameraId);

    // Get camera profile (for color correction)
    FRshipCameraProfile GetCameraProfile(const FString& ProfileId);

    // Events
    UPROPERTY(BlueprintAssignable)
    FOnCamerasUpdated OnCamerasUpdated;
};
```

### 2.2 Data Structures

**FRshipFixtureInfo**
```cpp
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipFixtureInfo
{
    UPROPERTY(BlueprintReadOnly) FString Id;
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) FVector Position;
    UPROPERTY(BlueprintReadOnly) FRotator Rotation;
    UPROPERTY(BlueprintReadOnly) FString FixtureTypeId;
    UPROPERTY(BlueprintReadOnly) int32 Universe;
    UPROPERTY(BlueprintReadOnly) int32 Address;
    UPROPERTY(BlueprintReadOnly) FString EmitterId;  // For DMX state
};
```

**FRshipFixtureTypeInfo**
```cpp
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipFixtureTypeInfo
{
    UPROPERTY(BlueprintReadOnly) FString Id;
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) FString Manufacturer;
    UPROPERTY(BlueprintReadOnly) float BeamAngle;
    UPROPERTY(BlueprintReadOnly) float FieldAngle;
    UPROPERTY(BlueprintReadOnly) float ColorTemperature;
    UPROPERTY(BlueprintReadOnly) int32 Lumens;
    UPROPERTY(BlueprintReadOnly) FString IESProfileUrl;
    UPROPERTY(BlueprintReadOnly) FString GDTFUrl;
    UPROPERTY(BlueprintReadOnly) FString GeometryUrl;
    UPROPERTY(BlueprintReadOnly) bool bHasPanTilt;
    UPROPERTY(BlueprintReadOnly) bool bHasZoom;
    UPROPERTY(BlueprintReadOnly) bool bHasGobo;
    UPROPERTY(BlueprintReadOnly) float MaxPan;
    UPROPERTY(BlueprintReadOnly) float MaxTilt;
    UPROPERTY(BlueprintReadOnly) FVector2D ZoomRange;
};
```

**FRshipFixtureCalibration**
```cpp
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipFixtureCalibration
{
    UPROPERTY(BlueprintReadOnly) FString FixtureTypeId;
    UPROPERTY(BlueprintReadOnly) TArray<FRshipDimmerCurvePoint> DimmerCurve;
    UPROPERTY(BlueprintReadOnly) int32 MinVisibleDmx;
    UPROPERTY(BlueprintReadOnly) TArray<FRshipColorCalibration> ColorCalibrations;
    UPROPERTY(BlueprintReadOnly) float ActualWhitePoint;
    UPROPERTY(BlueprintReadOnly) float BeamAngleMultiplier;
    UPROPERTY(BlueprintReadOnly) float FieldAngleMultiplier;
    UPROPERTY(BlueprintReadOnly) float FalloffExponent;
};

USTRUCT(BlueprintType)
struct FRshipDimmerCurvePoint
{
    UPROPERTY(BlueprintReadOnly) int32 DmxValue;      // 0-255
    UPROPERTY(BlueprintReadOnly) float OutputPercent; // 0-1
};

USTRUCT(BlueprintType)
struct FRshipColorCalibration
{
    UPROPERTY(BlueprintReadOnly) float TargetKelvin;
    UPROPERTY(BlueprintReadOnly) float MeasuredKelvin;
    UPROPERTY(BlueprintReadOnly) FVector2D ChromaticityOffset;
    UPROPERTY(BlueprintReadOnly) FLinearColor RgbCorrection;
};
```

**FRshipCameraCalibration**
```cpp
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipCameraCalibration
{
    UPROPERTY(BlueprintReadOnly) FVector Position;
    UPROPERTY(BlueprintReadOnly) FRotator Rotation;
    UPROPERTY(BlueprintReadOnly) FVector2D FocalLength;     // fx, fy
    UPROPERTY(BlueprintReadOnly) FVector2D PrincipalPoint;  // cx, cy
    UPROPERTY(BlueprintReadOnly) float FOV;
    UPROPERTY(BlueprintReadOnly) FVector RadialDistortion;  // k1, k2, k3
    UPROPERTY(BlueprintReadOnly) FVector2D TangentialDistortion; // p1, p2
};
```

### 2.3 Visualization Components

**ARshipFixtureActor** - Visualizes a fixture in the scene
```cpp
UCLASS(BlueprintType)
class RSHIPEXEC_API ARshipFixtureActor : public AActor
{
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString FixtureId;

    // Components
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* BodyMesh;

    UPROPERTY(VisibleAnywhere)
    USpotLightComponent* BeamLight;

    UPROPERTY(VisibleAnywhere)
    UNiagaraComponent* VolumetricBeam;  // Haze visualization

    // Calibrated values (updated from manager)
    void ApplyCalibration(const FRshipFixtureCalibration& Calibration);
    void UpdateFromDMX(const TSharedPtr<FJsonObject>& DMXData);

private:
    FRshipFixtureCalibration CachedCalibration;

    // Apply dimmer curve: DMX value -> actual output
    float ApplyDimmerCurve(int32 DmxValue) const;

    // Apply color correction
    FLinearColor ApplyColorCalibration(const FLinearColor& InputColor, float ColorTemp) const;
};
```

**ARshipCameraActor** - Visualizes a calibrated camera
```cpp
UCLASS(BlueprintType)
class RSHIPEXEC_API ARshipCameraActor : public AActor
{
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString CameraId;

    // Components
    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* CameraMesh;

    UPROPERTY(VisibleAnywhere)
    USceneCaptureComponent2D* ViewFrustum;  // FOV visualization

    // Apply calibration result
    void ApplyCalibration(const FRshipCameraCalibration& Calibration);

    // Get undistorted view (applies lens distortion correction)
    UTextureRenderTarget2D* GetUndistortedView();
};
```

### 2.4 IES Profile Support

UE has native IES profile support via `IESLoader` and `IESTextureData`.

```cpp
// Loading IES from rship asset store
void URshipFixtureManager::LoadIESProfile(const FString& IESUrl)
{
    // Download from asset store
    // Parse with ULightMassIESReader or custom parser
    // Create UTextureLightProfile
    // Apply to SpotLight component
}
```

---

## Phase 3: Synchronization Protocol

### 3.1 WebSocket Message Types

**Entity Subscription**
```json
{
  "type": "subscribe",
  "entityTypes": ["fixture", "fixtureType", "fixtureCalibration", "camera", "calibration", "cameraProfile"],
  "projectId": "<project-id>"
}
```

**Entity Events** (SET/DEL via Myko event sourcing)
```json
{
  "type": "event",
  "entityType": "fixtureCalibration",
  "operation": "SET",
  "data": {
    "id": "cal-123",
    "fixtureTypeId": "ft-456",
    "dimmerCurve": [...],
    "colorCalibrations": [...],
    ...
  }
}
```

**Calibration Query Response**
```json
{
  "type": "query_result",
  "query": "GetFixtureCalibrationsByFixtureTypeId",
  "results": [{
    "id": "cal-123",
    "fixtureTypeId": "ft-456",
    ...
  }]
}
```

### 3.2 Real-time DMX State

DMX state flows through the existing Pulse system:
- Executor pushes DMX state as Pulse data
- Server routes to subscribers
- UE plugin receives via `WatchEmitterValue` equivalent
- Applies calibration before visualization

---

## Phase 4: Asset Management

### 4.1 IES/GDTF File Handling

**Web UI:**
- Files stored in rship asset store
- URLs in `FixtureType.iesProfileUrl`, `FixtureType.gdtfUrl`
- Loaded on-demand via fetch

**UE Plugin:**
- Download from asset store on first use
- Cache locally in `Saved/Rship/Assets/`
- Parse IES → `UTextureLightProfile`
- Parse GDTF → fixture geometry + DMX modes

### 4.2 Geometry Import

Both visualizers support imported 3D geometry:
- Web: Three.js GLTF/GLB loader
- UE: `UStaticMesh` from GLTF/FBX

Geometry URL in `FixtureType.geometryUrl` or `Fixture.geometryUrl` (override).

---

## Phase 5: Calibration Workflow

### 5.1 Fixture Calibration Flow

1. **Reference Capture** (Web UI)
   - Select fixture type
   - Capture reference photo with calibrated camera
   - Measure DMX response (dimmer curve)
   - Measure color at various temps

2. **Analysis** (Server-side via OpenCV FFI)
   - Analyze reference photo
   - Calculate color correction matrix
   - Fit dimmer curve

3. **Save Calibration** (via Command)
   - `CreateFixtureCalibration` command
   - Stored as entity, event-sourced

4. **Apply to Visualizers** (real-time)
   - Web UI subscribes to FixtureCalibration events
   - UE plugin subscribes to FixtureCalibration events
   - Both apply identical calibration math

### 5.2 Camera Calibration Flow

1. **Point Marking** (Web UI)
   - Mark 2D-3D point correspondences
   - At least 6 points for full calibration

2. **Solve** (Server-side via OpenCV FFI)
   - Nelder-Mead optimization
   - Outputs position, rotation, intrinsics, distortion

3. **Save Result**
   - `SetStaticCalibration` command
   - Stored in `Calibration.savedResult`

4. **Apply to Visualizers**
   - Web: Update camera position/FOV in Threlte
   - UE: Update `ARshipCameraActor` transform and FOV

---

## Implementation Order

### Phase 1: Server-Side Entities ✅ COMPLETE
1. ✅ Create `FixtureCalibration` entity + handlers
2. ✅ Create `ColorProfile` entity + handlers
3. ✅ Migrate web services to entity-based storage
4. ✅ Add entity queries and subscriptions

### Phase 2: UE Data Layer ✅ COMPLETE
1. ✅ Create `FRshipFixtureCalibration`, `FRshipColorProfile` structs
2. ✅ Create `FRshipFixtureInfo`, `FRshipFixtureTypeInfo` structs
3. ✅ Implement entity subscription infrastructure in `URshipSubsystem`
4. ✅ Implement `URshipFixtureManager`
5. ✅ Implement `URshipCameraManager`
6. ✅ WebSocket message parsing for calibration entity types

### Phase 3: UE Visualization ✅ COMPLETE (Basic)
1. ✅ Create `ARshipFixtureActor` with basic visualization
2. ✅ Implement dimmer curve application (`DmxToOutput()`)
3. ✅ Implement color calibration application
4. TODO: Add IES profile loading and application
5. ✅ Create `ARshipCameraActor` with FOV visualization

### Phase 4: Asset Pipeline
1. IES file download and caching
2. GDTF parsing for DMX modes
3. Geometry import pipeline
4. Asset store integration

### Phase 5: Advanced Features
1. Volumetric beam visualization (Niagara)
2. Gobo projection
3. Pan/tilt animation
4. Camera lens distortion visualization

---

## Testing Strategy

### Unit Tests
- Dimmer curve interpolation
- Color temperature conversion
- IES profile parsing
- Camera calibration math

### Integration Tests
- WebSocket entity subscription
- Calibration save/load cycle
- Real-time DMX updates

### Visual Comparison Tests
- Render same scene in Web + UE
- Compare fixture colors at various DMX values
- Compare beam shapes with/without IES
- Validate camera FOV visualization

---

## Performance Considerations

### Web Visualizer
- IES texture generation cached per fixture type
- Calibration lookups are O(1) Map access
- Dimmer curve interpolation per frame (cheap)

### UE Plugin
- IES textures loaded once, shared across instances
- Calibration data cached per fixture type
- DMX updates throttled to 30Hz max

### Both
- Entity subscriptions use delta updates
- Large scenes use LOD for fixture detail
- Calibration updates are infrequent (not per-frame)

---

## Design Decisions (Resolved)

1. **Calibration Ownership**: ✅ Per fixture-type
   - All fixtures of same type share calibration via `fixtureTypeId`
   - Optional per-fixture override via `calibrationId` on Fixture entity (future)

2. **IES Profile Storage**: ✅ URL reference to asset store
   - Content-addressable storage in rship asset store
   - `iesProfileUrl` on FixtureType entity

3. **GDTF Integration**: ✅ Full fixture definition support
   - Complete GDTF parsing with wheel data, DMX modes
   - Parsed server-side, available via FixtureType entity

4. **Real-time Calibration Updates**: ✅ Yes, live editing allowed
   - Full reactive system with Kafka event sourcing
   - Optimistic locking via entity hash for conflict resolution

5. **Calibration Versioning**: ✅ Mutable with history
   - Entity is mutable, event sourcing provides full history
   - No draft/publish workflow (skip it)

6. **Calibration Scope**: ✅ Organization-level (via projectId)
   - Calibrations are scoped to project/organization
   - Shared across all sessions using that project

---

## File Locations

### Server Entities (`@rship/entities-core`)
- ✅ `FixtureCalibration` entity (Kafka-persisted)
- ✅ `ColorProfile` entity (Kafka-persisted)
- ✅ `Fixture`, `FixtureType`, `Camera`, `Calibration` entities

### Web UI (apps/ui/src/lib/)
- ✅ `services/visualize/fixtureCalibrationService.ts` - Entity-based
- ✅ `services/visualize/cameraCalibrationService.ts` - Entity-based
- ✅ `services/visualize/photometricService.ts` - IES texture generation
- ✅ `components/visualize/WebGPUFixtureModel.svelte` - Applies calibration

### UE Plugin (Source/RshipExec/) ✅ CREATED
- ✅ `Public/RshipCalibrationTypes.h` + `Private/RshipCalibrationTypes.cpp` - All calibration structs
- ✅ `Public/RshipFixtureManager.h` + `Private/RshipFixtureManager.cpp` - Fixture entity management
- ✅ `Public/RshipCameraManager.h` + `Private/RshipCameraManager.cpp` - Camera/ColorProfile management
- ✅ `Public/RshipFixtureActor.h` + `Private/RshipFixtureActor.cpp` - Fixture visualization actor
- ✅ `Public/RshipCameraActor.h` + `Private/RshipCameraActor.cpp` - Camera visualization actor
- Modified: `Public/RshipSubsystem.h` + `Private/RshipSubsystem.cpp` - Entity event routing
