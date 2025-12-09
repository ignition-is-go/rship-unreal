# Rship-Unreal

Rocketship's Unreal Engine integration is facilitated by the Rship-Unreal plugin.

## Quickstart

### Installation

1. Download the plugin from [releases](https://docs.rship.io/releases)
2. Create a 'Plugins' folder inside a UE project directory and place the extracted folder there
3. Restart Unreal
4. In the plugins manager (Edit > Plugins), search for the 'RshipExec' plugin and verify it exists and is active (checked on)
5. Open the Unreal project settings (Edit > Project Settings) and search for the 'Rship Exec' plugin
6. Enter the address of the Rocketship server you wish to connect to in the 'Rocketship Host' field. Also set the 'Service Color' :)
7. Currently the rship connection management lives in the RshipTarget blueprint component. Create a Target to establish a connection with rship

### Usage

#### Create a Target

1. Add an RshipTarget component to any Actor blueprint
2. Name the Target in the Actor properties pane (in the level editor).
3. In the Editor Details pane, select the RshipTarget component and click 'Register' or 'Reconnect'

#### Create an Action

1. Create a function in the Actor blueprint. **Make sure to prefix the function name with RS_** to add it to the Actor's Target 
2. Edit the function arguments to define an Action Schema, which determines the data structure required for triggering the Action

![Screenshot 2024-04-03 at 11 38 28 AM](https://github.com/ignition-is-go/rship-unreal/assets/131498134/92378b33-281f-48ff-9228-6434604c02b5)

3. Connect the RS function to some other function

![Unreal_Action_Blueprint_Example](https://github.com/ignition-is-go/rship-unreal/assets/131498134/3111ee2d-a857-4c35-998f-396f745dc9d7)

#### Create an Emitter

1. Create an event dispatcher in the Actor blueprint. **Make sure to prefix the event dispatcher name with RS_** to add it to the Actor's Target
2. Edit the event dispatcher properties to define an Emitter Schema, which determines the data structure pulsed by the Emitter

### Rate Limiting & Backoff Settings

The plugin includes a robust rate limiting system to prevent overwhelming the Rocketship server. Configure these settings in **Project Settings > Plugins > Rocketship Settings**.

#### Rate Limiting

| Setting | Default | Description |
|---------|---------|-------------|
| Enable Rate Limiting | `true` | Enable/disable client-side rate limiting |
| Max Messages Per Second | `50` | Maximum messages that can be sent per second |
| Max Burst Size | `20` | Token bucket capacity for handling bursts |
| Max Queue Length | `100` | Maximum queued messages before dropping low-priority ones |
| Message Timeout | `30s` | Messages older than this are dropped (critical messages exempt) |
| Enable Coalescing | `true` | Combine duplicate messages (e.g., rapid emitter pulses) |

#### Backoff & Reconnection

| Setting | Default | Description |
|---------|---------|-------------|
| Initial Backoff | `1.0s` | First backoff delay after error |
| Max Backoff | `60s` | Maximum backoff delay |
| Backoff Multiplier | `2.0` | Exponential multiplier for consecutive errors |
| Max Retry Count | `5` | Retries before dropping message (0 = unlimited) |
| Auto Reconnect | `true` | Automatically reconnect on disconnect |
| Max Reconnect Attempts | `10` | Maximum reconnection attempts (0 = unlimited) |

#### Diagnostics

| Setting | Default | Description |
|---------|---------|-------------|
| Log Verbosity | `1` | 0=Errors, 1=Warnings, 2=Info, 3=Verbose |
| Enable Metrics | `false` | Collect performance metrics |
| Queue Process Interval | `0.016s` | How often to process queued messages (~60Hz) |

#### Message Priority Levels

Messages are prioritized to ensure critical operations are never dropped:

1. **Critical** - Command responses (never dropped)
2. **High** - Registration messages (target, action, emitter definitions)
3. **Normal** - Standard messages
4. **Low** - Emitter pulses (can be coalesced/dropped under load)

#### Blueprint Diagnostics

The following functions are available for monitoring:

```cpp
// Check connection status
bool IsConnected()

// Get current message queue length
int32 GetQueueLength()

// Get messages sent in the last second
int32 GetMessagesSentPerSecond()

// Check if rate limiter is backing off
bool IsRateLimiterBackingOff()

// Get remaining backoff time in seconds
float GetBackoffRemaining()
```

### Current Limitations

Due to some pesky aspects of Unreal's implementation of C++, we are still working out the following limitations:

- Emitter Schema Size:
    - An Emitter Schema can have at most 32 properties.

- Unsupported Data Types:
    - Vector, Rotation, and Position (note these structures can be easily decomposed into floats)

## Targets, Actions, and Emitters


| Targets          | Emitters                                  | Actions                  |
|------------------|-------------------------------------------|--------------------------|
| **Actors**       | - Any Event dispatcher Parameter          | - Any Function Argument  |
