# Windows Build Environment Setup

This guide covers the dependencies and configuration required to build the rship-unreal plugins on Windows.

## Prerequisites

### Required Software

| Software | Version | Download |
|----------|---------|----------|
| **Unreal Engine** | 5.6 or 5.7 | [Epic Games Launcher](https://www.unrealengine.com/download) |
| **Visual Studio** | 2022 (17.x) | [Visual Studio](https://visualstudio.microsoft.com/) |
| **Git** | Latest | [Git for Windows](https://git-scm.com/download/win) |

### Visual Studio Workloads

Install these workloads via Visual Studio Installer:

- **Desktop development with C++**
  - MSVC v143 build tools
  - Windows 10/11 SDK (10.0.x)
  - C++ CMake tools (for some dependencies)

- **Game development with C++**
  - Unreal Engine installer (optional)

### Required VS Components

Ensure these individual components are installed:

```
- MSVC v143 - VS 2022 C++ x64/x86 build tools
- Windows 10 SDK (10.0.19041.0 or later)
- C++ ATL for latest v143 build tools
- .NET Framework 4.8 SDK
```

---

## Plugin-Specific Dependencies

### RshipExec (Core)

**No external dependencies required.** IXWebSocket is bundled as source.

After cloning, initialize submodules:
```bash
git submodule update --init --recursive
```

**Windows System Libraries (auto-linked):**
- `Ws2_32.lib` (Windows Sockets)

#### Optional: Rust Display Management Runtime

RshipExec can optionally link a Rust static library for deterministic Windows display management:

```bash
cd Plugins/RshipExec/Source/RshipExec/ThirdParty/rship-display
cargo build --release -p rship-display-ffi
```

Or use helper script:

```bash
cd Plugins/RshipExec/Source/RshipExec/ThirdParty/rship-display
build.bat
```

When built successfully, expected artifact:
- `Plugins/RshipExec/Source/RshipExec/ThirdParty/rship-display/target/release/rship_display.lib`

If missing, RshipExec still compiles with `RSHIP_HAS_DISPLAY_RUST=0` and display orchestration runs in fallback mode.

---

### RshipNDI (NDI Streaming)

#### Rust Toolchain (Required)

1. Install Rust from [rustup.rs](https://rustup.rs):
   ```powershell
   # Run in PowerShell (downloads rustup-init.exe)
   Invoke-WebRequest -Uri https://win.rustup.rs -OutFile rustup-init.exe
   .\rustup-init.exe
   ```

2. Restart your terminal and verify:
   ```bash
   cargo --version
   rustc --version
   ```

3. The Rust library auto-builds during UE compilation if `cargo` is in PATH.

**Manual Build (if auto-build fails):**
```bash
cd Plugins/RshipNDI/Source/RshipNDI/ThirdParty/rship-ndi-sender
cargo build --release
```

#### NDI Tools (Runtime Dependency)

For NDI streaming to work at runtime, install:
- [NDI Tools](https://ndi.video/tools/) (free download, provides `ndi.dll`)

**Windows System Libraries (auto-linked):**
- `Ws2_32.lib`
- `Userenv.lib`
- `Bcrypt.lib`
- `ntdll.lib`
- `Advapi32.lib`

---

### Rship2110 (SMPTE 2110 / PTP)

#### NVIDIA Rivermax SDK (Optional but Required for 2110)

SMPTE 2110 streaming requires the NVIDIA Rivermax SDK and compatible hardware.

**SDK Setup:**

1. Download Rivermax SDK 1.8+ from [NVIDIA Developer](https://developer.nvidia.com/rivermax)
2. Place SDK files in:
   ```
   Plugins/Rship2110/Source/Rship2110/ThirdParty/Rivermax/
   ├── include/
   │   └── rivermax_api.h (or rivermax.h)
   ├── lib/
   │   └── x64/
   │       └── rivermax.lib
   └── rivermax.lic (your license file)
   ```

**Alternative: System Installation:**
- Install Rivermax SDK to `C:\Program Files\NVIDIA\Rivermax`
- Or set environment variable: `RIVERMAX_SDK_PATH=C:\path\to\sdk`

**Hardware Requirements:**
- NVIDIA ConnectX-5 or later NIC
- Mellanox OFED drivers
- PTP-capable network switch (for sync)

**Without Rivermax SDK:**
- Plugin compiles with stub implementations
- PTP features still work (uses system time)
- IPMX/NMOS REST API features work

**Windows System Libraries (auto-linked):**
- `Ws2_32.lib`
- `Iphlpapi.lib`
- `Winmm.lib`

---

### RshipSpatialAudio

**No external dependencies required.** Uses UE's built-in audio systems.

**Optional: ASIO Support**
- For low-latency audio output, install ASIO4ALL or vendor ASIO drivers
- Define `SPATIAL_AUDIO_ASIO_SUPPORT=1` is set automatically on Windows

---

### UltimateControl (AI Editor Control)

**No external dependencies for the UE plugin.**

#### MCP Server (For Claude Integration)

The MCP server can be built from Rust source or used via Python:

**Option A: Rust MCP Server (Recommended)**
```bash
cd Plugins/UltimateControl/MCP
cargo build --release
```

**Option B: Python Package**
```bash
pip install ue5-mcp-bridge
ue5-mcp-install
```

---

### RshipColorManagement

**No external dependencies required.**

---

## Environment Variables

Set these for enhanced functionality:

```powershell
# Rivermax SDK (if not using bundled location)
[Environment]::SetEnvironmentVariable("RIVERMAX_SDK_PATH", "C:\path\to\rivermax", "User")

# Rivermax License (alternative location)
[Environment]::SetEnvironmentVariable("RIVERMAX_LICENSE_PATH", "C:\licenses", "User")
```

---

## Build Configuration

### Memory Optimization

For machines with limited RAM (<32GB), add to your `BuildConfiguration.xml`:

```xml
<?xml version="1.0" encoding="utf-8"?>
<Configuration xmlns="https://www.unrealengine.com/BuildConfiguration">
  <BuildConfiguration>
    <MaxParallelActions>4</MaxParallelActions>
  </BuildConfiguration>
  <ParallelExecutor>
    <ProcessorCountMultiplier>0.5</ProcessorCountMultiplier>
  </ParallelExecutor>
</Configuration>
```

Location: `%APPDATA%\Unreal Engine\UnrealBuildTool\BuildConfiguration.xml`

### Build Command

From your UE project directory:

```bash
# Generate project files
"C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\GenerateProjectFiles.bat" YourProject.uproject

# Build from command line
"C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" YourProject Win64 Development -Project="YourProject.uproject"
```

---

## Troubleshooting

### Common Build Errors

#### "Cannot find cargo"
- Ensure Rust is installed and `cargo` is in PATH
- Restart Visual Studio/terminal after installing Rust
- Or manually build: `cd Plugins/RshipNDI/.../rship-ndi-sender && cargo build --release`

#### "rivermax.lib not found"
- Rivermax SDK not installed or path not detected
- Check `ThirdParty/Rivermax/lib/x64/rivermax.lib` exists
- Or set `RIVERMAX_SDK_PATH` environment variable

#### "IXWebSocket not found"
- Run `git submodule update --init --recursive`
- Check `Plugins/RshipExec/Source/RshipExec/ThirdParty/IXWebSocket/` exists

#### Link errors with Windows libs
- Ensure Windows SDK is installed via Visual Studio Installer
- Check Windows SDK version matches project requirements

#### Out of memory during compilation
- Reduce parallel jobs (see Memory Optimization above)
- Close other applications
- Use SSD for Intermediate folder

### Verifying Build Output

Check these logs during build:

```
RshipExec: IXWebSocket found, enabling high-performance WebSocket
RshipNDI: Rust NDI sender library found at ...
Rship2110: Using bundled Rivermax SDK from ThirdParty/Rivermax
```

---

## Plugin Dependencies Summary

```
RshipExec (core)
├── IXWebSocket (bundled, auto-compiles)
└── Windows: Ws2_32.lib

RshipNDI
├── Rust toolchain (for building)
├── NDI Tools (runtime)
└── Windows: Ws2_32, Userenv, Bcrypt, ntdll, Advapi32

Rship2110
├── NVIDIA Rivermax SDK (optional)
├── Rivermax License (optional)
└── Windows: Ws2_32, Iphlpapi, Winmm

RshipSpatialAudio
└── (no external dependencies)

UltimateControl
├── (no external dependencies for plugin)
└── Rust or Python (for MCP server)

RshipColorManagement
└── (no external dependencies)
```

---

## Version Compatibility

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| Unreal Engine | 5.6.0 | 5.7.x |
| Visual Studio | 2022 17.0 | 2022 17.8+ |
| Windows SDK | 10.0.19041 | 10.0.22621 |
| Rust | 1.70 | Latest stable |
| Rivermax SDK | 1.3 | 1.8+ |

---

## Next Steps

After setting up your environment:

1. Clone the repository with submodules
2. Open the `.uproject` in Unreal Engine
3. Enable desired plugins in Edit > Plugins
4. Configure Rocketship Settings in Project Settings
5. See [UPGRADE_GUIDE.md](UPGRADE_GUIDE.md) for feature documentation
