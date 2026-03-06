# Adaptive Outbound Pipeline for Rocketship WebSocket

## Overview

This document describes the adaptive outbound pipeline implemented in the Rocketship Unreal Engine plugin. The system is designed to maximize throughput through bandwidth-constrained WebSocket connections while gracefully handling backpressure and rate limits.

## Architecture

```
[High-Speed Input Sources]
         |
         v
+-------------------+
| EnqueueMessage()  | <-- Thread-safe ingress from game thread or delegates
+-------------------+
         |
         v
+-------------------+
| Priority Queue    | <-- Sorted by priority (Critical > High > Normal > Low)
|                   |     Coalesced by key (deduplicates rapid updates)
+-------------------+
         |
         v
+-------------------+
| Downsampling      | <-- Under pressure: keeps every Nth low-priority message
|                   |     Configurable per-priority sample rates
+-------------------+
         |
         v
+-------------------+
| Rate Limiter      | <-- Dual token bucket (messages/sec + bytes/sec)
|                   |     Adaptive rate control based on backpressure
+-------------------+
         |
         v
+-------------------+
| Batch Builder     | <-- Combines messages into single WebSocket frames
|                   |     Reduces per-message overhead dramatically
+-------------------+
         |
         v
[WebSocket Send]
```

## Key Features

### 1. Message Batching
**Purpose**: Reduce per-message WebSocket frame overhead.

Instead of sending 10 small messages as 10 separate WebSocket frames, batching combines them into a single frame containing an array of payloads. This dramatically reduces:
- Network round-trip overhead
- WebSocket frame header costs
- Number of send operations hitting provider rate limits

**Configuration**:
```ini
[/Script/RshipExec.RshipSettings]
bEnableBatching=true
MaxBatchMessages=10        ; Max messages per batch
MaxBatchBytes=65536        ; Max batch size (64KB)
MaxBatchIntervalMs=16      ; Max wait time before flushing
bCriticalBypassBatching=true  ; Critical messages sent immediately
```

### 2. Dual Token Bucket Rate Limiting
**Purpose**: Smooth rate limiting with burst capability.

Two separate token buckets control the outbound rate:
- **Message tokens**: Limits messages per second
- **Byte tokens**: Limits bytes per second

This ensures both small frequent messages AND large payloads are rate-limited appropriately.

**Configuration**:
```ini
[/Script/RshipExec.RshipSettings]
MaxMessagesPerSecond=50.0
MaxBurstSize=20
bEnableBytesRateLimiting=true
MaxBytesPerSecond=1048576  ; 1 MB/s
MaxBurstBytes=262144       ; 256 KB
```

### 3. Priority Queue with 4 Levels
**Purpose**: Ensure critical messages are never dropped.

| Priority | Use Case | Behavior |
|----------|----------|----------|
| Critical | Command responses | Never dropped, may bypass batching/backoff |
| High | Registration, status | Protected from downsampling |
| Normal | Standard messages | Subject to downsampling under pressure |
| Low | Telemetry, emitter pulses | First to be downsampled or dropped |

### 4. Adaptive Rate Control
**Purpose**: Dynamically adjust send rate based on observed backpressure.

When backpressure is detected (tokens depleted, backoff triggered):
- Rate is decreased by `RateDecreaseFactor` (default: 0.5 = halve)
- Minimum rate is `MinRateFraction` of configured max (default: 10%)

When no backpressure for `RateAdjustmentInterval`:
- Rate is increased by `RateIncreaseFactor` (default: 1.1 = +10%)
- Maximum is the configured `MaxMessagesPerSecond`

**Configuration**:
```ini
[/Script/RshipExec.RshipSettings]
bEnableAdaptiveRate=true
RateIncreaseFactor=1.1
RateDecreaseFactor=0.5
MinRateFraction=0.1
RateAdjustmentInterval=1.0
```

### 5. Downsampling Under Pressure
**Purpose**: Keep representative samples instead of dropping all low-priority messages.

When queue pressure exceeds `QueuePressureThreshold`:
- Low priority: Keep 1 in `LowPrioritySampleRate` messages
- Normal priority: Keep 1 in `NormalPrioritySampleRate` messages
- High/Critical: Always kept

**Configuration**:
```ini
[/Script/RshipExec.RshipSettings]
bEnableDownsampling=true
LowPrioritySampleRate=5    ; Keep 1 in 5
NormalPrioritySampleRate=2 ; Keep 1 in 2
QueuePressureThreshold=0.7 ; Start at 70% queue full
```

### 6. Message Coalescing
**Purpose**: Deduplicate rapid updates from the same source.

When multiple messages have the same `CoalesceKey` and `Type`:
- Only the newest message is kept
- Older duplicates are silently replaced

This is especially useful for emitter pulses where only the latest value matters.

### 7. Exponential Backoff
**Purpose**: Gracefully handle server rate limits.

When rate limit is detected (WebSocket close codes 429, 1008):
- Initial backoff: `InitialBackoffSeconds`
- Backoff multiplied by `BackoffMultiplier` on each consecutive error
- Maximum backoff: `MaxBackoffSeconds`
- Optionally: Critical messages can bypass backoff

**Configuration**:
```ini
[/Script/RshipExec.RshipSettings]
InitialBackoffSeconds=1.0
MaxBackoffSeconds=60.0
BackoffMultiplier=2.0
bCriticalBypassBackoff=false  ; Set true to allow critical during backoff
```

## Recognizing Pipeline Behavior in Logs

### Batching Active
```
LogRshipExec: RateLimiter: Sent batch: 8 messages, 4523 bytes (efficiency: 8.0 msg/frame)
```

### Rate Limit Detected
```
LogRshipExec: Warning: Rate limit detected from server (code 429)
LogRshipExec: Error: RateLimiter: Rate limit error - backing off for 2.0 seconds (consecutive: 1)
```

### Adaptive Rate Adjustment
```
LogRshipExec: RateLimiter: Adaptive rate decreased: 100.0% -> 50.0% (backpressure detected)
LogRshipExec: Verbose: RateLimiter: Adaptive rate increased: 50.0% -> 55.0%
```

### Downsampling Active
```
LogRshipExec: Verbose: RateLimiter: Downsampled message (Priority: 3, Key: target1:emitter1, Pressure: 75.0%)
```

### Message Dropped
```
LogRshipExec: Warning: RateLimiter: Queue full, dropping incoming message (Priority: 3, Key: target1:emitter1)
LogRshipExec: Warning: RateLimiter: Dropping expired message (age: 31.2s, priority: 2, key: target2:emitter2)
```

### Metrics Summary (every MetricsLogInterval seconds)
```
LogRshipExec: RateLimiter: Metrics: 45 msg/s, 12543 B/s, queue=23 (5%), drops=0, rate=50.0/s [BATCH]
```

## Blueprint API

The following functions are available from Blueprint for runtime monitoring:

| Function | Returns | Description |
|----------|---------|-------------|
| `IsConnected` | bool | WebSocket connection state |
| `GetQueueLength` | int32 | Messages currently queued |
| `GetQueueBytes` | int32 | Estimated bytes in queue |
| `GetQueuePressure` | float | Queue fullness (0.0-1.0) |
| `GetMessagesSentPerSecond` | int32 | Throughput |
| `GetBytesSentPerSecond` | int32 | Bandwidth usage |
| `GetMessagesDropped` | int32 | Total dropped messages |
| `IsRateLimiterBackingOff` | bool | Backoff active |
| `GetBackoffRemaining` | float | Seconds until backoff ends |
| `GetCurrentRateLimit` | float | Effective msg/s limit |
| `ResetRateLimiterStats` | void | Reset all counters |

## Tuning Guide

### For Maximum Throughput (Local/High-Bandwidth Server)
```ini
MaxMessagesPerSecond=200.0
MaxBurstSize=50
MaxBytesPerSecond=10485760  ; 10 MB/s
MaxBurstBytes=1048576       ; 1 MB
MaxBatchMessages=20
MaxBatchBytes=131072        ; 128 KB
bEnableDownsampling=false
```

### For Strict Provider Limits (Cloud/SaaS)
```ini
MaxMessagesPerSecond=30.0
MaxBurstSize=10
MaxBytesPerSecond=524288    ; 512 KB/s
bEnableAdaptiveRate=true
RateDecreaseFactor=0.3      ; Aggressive decrease
MinRateFraction=0.05        ; Allow dropping to 5%
bEnableDownsampling=true
LowPrioritySampleRate=10    ; Keep only 1 in 10
```

### For Low Latency (Real-Time Control)
```ini
MaxBatchIntervalMs=8        ; Flush quickly
MaxBatchMessages=5          ; Small batches
bCriticalBypassBatching=true
bCriticalBypassBackoff=true
```

### For High-Frequency Telemetry
```ini
bEnableCoalescing=true      ; Merge duplicate sources
bEnableDownsampling=true
LowPrioritySampleRate=10    ; Aggressive sampling
QueuePressureThreshold=0.5  ; Start early
```

## Testing Checklist

### To Simulate High Input Rate
1. Create an emitter that fires rapidly (e.g., every tick)
2. Enable `bLogBatchDetails=true` to see batch formation
3. Monitor `GetQueuePressure()` via Blueprint

### To Observe Batching
1. Set `LogVerbosity=3` for verbose logging
2. Look for "Sent batch: N messages, M bytes"
3. Compare message count vs batch count in metrics

### To Test Rate Limiting
1. Temporarily set `MaxMessagesPerSecond=5`
2. Generate burst of messages
3. Observe queue building and metrics showing rate compliance

### To Test Backpressure Handling
1. Set `MaxQueueLength=20` (small)
2. Generate high message rate
3. Observe downsampling and dropping in logs
4. Verify Critical messages are never dropped

### To Verify Adaptive Rate Control
1. Enable `bEnableAdaptiveRate=true`
2. Set `RateAdjustmentInterval=0.5` for faster testing
3. Cause backpressure (fill queue)
4. Observe "Adaptive rate decreased" in logs
5. Let pressure subside
6. Observe "Adaptive rate increased" in logs

## Trade-offs

| Setting | Increases | Decreases |
|---------|-----------|-----------|
| Higher batch size | Throughput efficiency | Latency |
| Higher queue size | Burst absorption | Memory usage |
| Lower sample rate | Throughput | Data fidelity |
| Adaptive rate | Stability | Peak throughput |
| Critical bypass | Reliability | Rate compliance |

## Server-Side Batch Handling

When batching is enabled, the server receives messages in this format:
```json
{
  "event": "ws:m:batch",
  "data": [
    { "event": "ws:m:set", "data": {...} },
    { "event": "ws:m:set", "data": {...} },
    ...
  ]
}
```

The server should:
1. Check for `event === "ws:m:batch"`
2. If true, iterate over `data` array and process each payload individually
3. If false, process the message normally

---

## Low-Level WebSocket Plugin Fixes

### The 30Hz Bottleneck

If you see this warning in logs:
```
LogRshipExec: Warning: WebSocket send throttled: 33.3ms between sends
```

This indicates **Unreal's WebSocket plugin service loop is throttled** at ~30Hz. Even with batching, you're limited to 30 WebSocket frames per second.

### Root Causes (in UE's WebSockets plugin)

1. **`lws_service()` timeout too high** - Service loop sleeps 10-30ms between wakes
2. **Extra `FPlatformProcess::Sleep()`** in the service thread
3. **Nagle's algorithm** - TCP batching small packets (adds 40ms delay)
4. **permessage-deflate compression** - Adds buffering latency

### Fixes (require patching UE's WebSockets plugin)

#### Fix 1: Hot Service Loop
In `Engine/Plugins/Online/WebSockets/Source/WebSockets/Private/`:

```cpp
// Find the service loop and change:
while (!bStopping)
{
    lws_service(Context, 0);  // 0 = no sleep, was probably 10-30
    // REMOVE any FPlatformProcess::Sleep() here
}
```

#### Fix 2: TCP_NODELAY (disable Nagle)
After WebSocket handshake completes:

```cpp
// In LWS_CALLBACK_CLIENT_ESTABLISHED or equivalent:
int fd = lws_get_socket_fd(Wsi);
int one = 1;
setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char*)&one, sizeof(one));
```

#### Fix 3: Disable Compression
When creating the WebSocket connection, ensure permessage-deflate is disabled:

```cpp
// Connection info should NOT include compression extensions
```

### Server-Side Fixes

#### Node.js with `ws` package:
```javascript
const wss = new WebSocketServer({
  port: 9001,
  perMessageDeflate: false  // Disable compression
});

wss.on('connection', ws => {
  ws._socket.setNoDelay(true);  // TCP_NODELAY
  // ...
});
```

#### NGINX proxy:
```nginx
location /ws {
    proxy_pass http://backend;
    proxy_http_version 1.1;
    proxy_set_header Upgrade $http_upgrade;
    proxy_set_header Connection "upgrade";

    # Critical for low latency:
    proxy_buffering off;
    tcp_nodelay on;
}
```

### Quick Test: Is It the Plugin?

Run a minimal local echo server:
```javascript
// test-echo.js
import { WebSocketServer } from 'ws';
const wss = new WebSocketServer({ port: 9001, perMessageDeflate: false });
wss.on('connection', ws => {
  ws._socket.setNoDelay(true);
  ws.on('message', data => ws.send(data));
});
console.log('Echo server on ws://localhost:9001');
```

Connect your UE client to `localhost:9001`. If the 30Hz throttle disappears, your production server/proxy is the bottleneck. If it persists, it's UE's plugin.

### High-Performance WebSocket (Integrated Solution)

**This plugin now includes a built-in high-performance WebSocket option** that bypasses UE's 30Hz throttle without requiring any engine patches.

#### How It Works

The plugin provides two WebSocket implementations:

1. **FRshipWebSocket with IXWebSocket** (best performance)
   - Uses the IXWebSocket library's dedicated send thread
   - No 30Hz throttle
   - TCP_NODELAY enabled by default
   - Compression disabled by default

2. **FRshipWebSocket fallback with dedicated send thread**
   - Uses UE's WebSocket but adds a background send thread
   - 1ms service loop (1000Hz capability vs 30Hz)
   - Works without any third-party libraries

#### Configuring High-Performance Mode

High-performance WebSocket is always enabled. Configure options in Project Settings > Game > Rocketship Settings:

```ini
[/Script/RshipExec.RshipSettings]
bTcpNoDelay=true                    ; Disable Nagle's algorithm (default: true)
bDisableCompression=true            ; Disable permessage-deflate (default: true)
PingIntervalSeconds=30              ; Keep-alive interval
```

#### IXWebSocket (Bundled)

IXWebSocket is bundled as a git submodule in `Source/RshipExec/ThirdParty/IXWebSocket`.

When cloning the plugin repo, use:
```bash
git clone --recursive <repo-url>
# Or if already cloned:
git submodule update --init --recursive
```

The build system auto-detects IXWebSocket and compiles it automatically. No manual installation needed.

#### Verifying High-Performance Mode

Check the log on startup:
```
LogRshipExec: Connecting to ws://localhost:5155/myko
LogRshipExec: RshipWebSocket: Started dedicated send thread   ; Fallback mode
# OR
LogRshipExec: RshipWebSocket: Connected                       ; IXWebSocket mode
```

---

## Two-Layer Bottleneck Summary

| Layer | This Plugin Addresses | High-Perf WebSocket | UE Plugin Patch |
|-------|----------------------|---------------------|-----------------|
| Application batching | ✅ 10x fewer messages | ✅ | - |
| Priority/dropping | ✅ Graceful degradation | ✅ | - |
| Adaptive rate | ✅ Backpressure response | ✅ | - |
| Service loop (30Hz) | Partially (more per wake) | ✅ 1000Hz | ✅ Full fix |
| TCP Nagle delay | - | ✅ TCP_NODELAY | ✅ TCP_NODELAY |
| Compression delay | - | ✅ Disabled | ✅ Disable deflate |

High-performance WebSocket is always enabled, bypassing UE's 30Hz throttle without engine patches.

---

## Changelog

### v2.2 - Simplified WebSocket
- Removed `bUseHighPerformanceWebSocket` setting - high-performance mode is now always enabled
- Simplified codebase by removing standard UE WebSocket path

### v2.1 - High-Performance WebSocket Integration
- Added `FRshipWebSocket` wrapper with dual implementation:
  - IXWebSocket path for maximum performance
  - Fallback with dedicated send thread (1ms loop)
- Added `bTcpNoDelay` setting to disable Nagle's algorithm
- Added `bDisableCompression` setting to disable permessage-deflate
- Build system auto-detects IXWebSocket in ThirdParty folder
- Bypasses UE's 30Hz WebSocket throttle without engine patches

### v2.0 - Adaptive Outbound Pipeline
- Added message batching to reduce per-message overhead
- Added bytes-aware rate limiting (dual token bucket)
- Added adaptive rate control based on backpressure
- Added downsampling for graceful degradation
- Added comprehensive metrics and logging
- Added Blueprint-callable diagnostics
- Significantly expanded configuration options
- Added send timing instrumentation to detect 30Hz throttle
