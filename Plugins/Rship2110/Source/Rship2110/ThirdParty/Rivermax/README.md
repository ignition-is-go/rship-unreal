# Rivermax SDK Bundling Guide

This directory contains the NVIDIA Rivermax SDK for SMPTE 2110 streaming support.

## Quick Setup

1. **Obtain Rivermax SDK** from NVIDIA (requires developer account)
2. **Copy SDK files** to this directory (see structure below)
3. **Place your license file** (`rivermax.lic`) in this directory

That's it! The plugin will automatically detect and use the bundled SDK.

## Required Directory Structure

```
ThirdParty/Rivermax/
├── README.md           (this file)
├── rivermax.lic        ← YOUR LICENSE FILE GOES HERE
├── include/
│   ├── rivermax_api.h
│   ├── rivermax_defs.h
│   └── ... (other headers)
├── lib/
│   └── x64/
│       └── rivermax.lib
└── bin/
    └── x64/
        ├── rivermax.dll
        ├── dpcp.dll
        └── mlx5devx.dll
```

## Files to Copy from Rivermax Installation

### From Windows Installation (typically `C:\Program Files\Mellanox\Rivermax\`)

**Headers** → `include/`
```
rivermax_api.h
rivermax_defs.h
rivermax_common_defs.h
```

**Libraries** → `lib/x64/`
```
rivermax.lib
```

**Runtime DLLs** → `bin/x64/`
```
rivermax.dll
dpcp.dll
mlx5devx.dll
```

### From Linux Installation (typically `/opt/mellanox/rivermax/`)

**Headers** → `include/`
```
rivermax_api.h
rivermax_defs.h
rivermax_common_defs.h
```

**Libraries** → `lib/`
```
librivermax.so
libdpcp.so
```

## License File

The license file is required for Rivermax to function at runtime.

**Accepted license file names:**
- `rivermax.lic` (preferred)
- `license.lic`
- `RIVERMAX.lic`
- `LICENSE.lic`

**Where to place it:**
- Primary: `ThirdParty/Rivermax/rivermax.lic` (this directory)
- Alternative: Set `RIVERMAX_LICENSE_PATH` environment variable

The build system will automatically copy the license file to the output directory.

## Build Output

During compilation, the following files are copied to the binary output directory:
- `rivermax.dll` / `librivermax.so`
- `dpcp.dll` / `libdpcp.so`
- `mlx5devx.dll`
- `rivermax.lic` (your license file)

## Verification

When you build the project, look for these messages in the build output:

**Success (with license):**
```
Rship2110: Using bundled Rivermax SDK from ThirdParty/Rivermax
Rship2110: Found Rivermax license: .../ThirdParty/Rivermax/rivermax.lic
Rship2110: Rivermax SDK found with license - full 2110 support enabled
Rship2110: License file will be copied to output directory
```

**Warning (SDK found, no license):**
```
Rship2110: Using bundled Rivermax SDK from ThirdParty/Rivermax
Rship2110: WARNING - Rivermax SDK found but NO LICENSE FILE detected!
Rship2110: Place your rivermax.lic file in: .../ThirdParty/Rivermax
Rship2110: Streaming will fail at runtime without a valid license.
```

**Fallback (no SDK):**
```
Rship2110: Rivermax SDK not found, using stub implementations
```

## Obtaining a Rivermax License

1. Visit [NVIDIA Rivermax](https://developer.nvidia.com/networking/rivermax)
2. Register for a developer account
3. Request a license for your ConnectX NIC
4. Download the `.lic` file and place it in this directory

## Hardware Requirements

Rivermax requires NVIDIA ConnectX network adapters:
- ConnectX-5 or newer
- Updated firmware
- Mellanox OFED drivers installed

## Troubleshooting

### "Rivermax SDK not found"
- Verify `include/rivermax_api.h` exists
- Verify `lib/x64/rivermax.lib` (Windows) or `lib/librivermax.so` (Linux) exists
- Check directory structure matches the layout above

### "No license file detected"
- Ensure license file is named correctly (see accepted names above)
- Verify the file is in this directory or `RIVERMAX_LICENSE_PATH` is set

### Runtime DLL errors
- Ensure all DLLs from `bin/x64/` are copied
- Verify the DLLs are compatible with your Rivermax SDK version
- Check that ConnectX drivers are installed

## Version Compatibility

This plugin is tested with:
- Rivermax SDK 1.20.x and newer
- ConnectX-5, ConnectX-6, ConnectX-7 adapters
- Windows 10/11 and Ubuntu 20.04/22.04
