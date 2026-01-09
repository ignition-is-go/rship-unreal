# RshipColorManagement Plugin

Centralized broadcast-grade color management for predictable output across viewport, NDI, and ST 2110.

> **Part of [rship-unreal](../../README.md)** - See the main README for an overview of all plugins.
>
> **Beta** - Color pipeline configuration for professional broadcast workflows.

## Features

- **Unified Color Pipeline** - Single source of truth for color transforms
- **Multiple Output Targets** - Configure color for viewport, NDI, 2110 independently
- **Color Space Support** - sRGB, Rec.709, Rec.2020, DCI-P3, ACES
- **HDR Support** - PQ (ST 2084) and HLG transfer functions
- **LUT Integration** - Apply 3D LUTs for show-specific color grades
- **OCIO Integration** - OpenColorIO configuration support (planned)

## Quick Start

```json
// In your .uproject file
{
  "Plugins": [
    { "Name": "RshipColorManagement", "Enabled": true }
  ]
}
```

### Configuration

**Project Settings > Rocketship > Color Management:**

| Setting | Default | Description |
|---------|---------|-------------|
| Working Color Space | `sRGB` | Internal rendering color space |
| NDI Output Space | `Rec.709` | Color space for NDI streams |
| 2110 Output Space | `Rec.709` | Color space for ST 2110 streams |
| Enable HDR | `false` | Enable HDR output pipeline |
| HDR Max Nits | `1000` | Peak brightness for HDR |

### C++ Usage

```cpp
#include "RshipColorManagementSubsystem.h"

// Get subsystem
URshipColorManagementSubsystem* ColorMgmt =
    GEngine->GetEngineSubsystem<URshipColorManagementSubsystem>();

// Get current config
FRshipColorConfig Config = ColorMgmt->GetColorConfig();

// Modify for HDR output
Config.bEnableHDR = true;
Config.HDRMaxNits = 1000.0f;
Config.OutputColorSpace = ERshipColorSpace::Rec2020;
Config.TransferFunction = ERshipTransferFunction::PQ;

ColorMgmt->SetColorConfig(Config);

// Apply color transform to a texture
ColorMgmt->TransformTexture(SourceTexture, DestTexture,
    ERshipColorSpace::sRGB, ERshipColorSpace::Rec2020);
```

### Blueprint Usage

1. Get `RshipColorManagementSubsystem` reference
2. Use `Get Color Config` / `Set Color Config` for configuration
3. Use `Transform Color` for individual color value conversion

## Color Spaces

| Space | Gamut | Common Use |
|-------|-------|------------|
| sRGB | sRGB | Web, general display |
| Rec.709 | Rec.709 | HD broadcast, SDR video |
| Rec.2020 | Rec.2020 | UHD/HDR broadcast |
| DCI-P3 | DCI-P3 | Digital cinema, HDR displays |
| ACES | AP0/AP1 | VFX interchange |

## Transfer Functions

| Function | Description |
|----------|-------------|
| sRGB | Standard sRGB gamma (~2.2) |
| Rec709 | BT.1886 broadcast gamma |
| PQ (ST 2084) | HDR Perceptual Quantizer |
| HLG | Hybrid Log-Gamma (BBC/NHK HDR) |
| Linear | Linear light (no curve) |

## Integration with Other Plugins

### RshipNDI
Color management automatically applies configured transforms to NDI output when both plugins are enabled.

### Rship2110
ST 2110 streams use the configured color space and transfer function for broadcast-compliant output.

### RshipExec
Color targets can be controlled via rship bindings for real-time color adjustments.

## Console Commands

```
rship.color.status    # Show current color configuration
rship.color.space     # Set output color space
rship.color.hdr       # Toggle HDR mode
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Colors look washed out | Check transfer function matches display |
| NDI colors don't match viewport | Configure NDI Output Space to match |
| HDR clipping | Adjust HDR Max Nits, check tone mapping |
| Color banding | Enable higher bit depth in output config |
