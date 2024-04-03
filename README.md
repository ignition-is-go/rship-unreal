# Rship-Unreal

Rocketship's Unreal Engine integration is facilitated by the Rship-Unreal plugin.

## Quickstart

### Installation

1. Download and unzip this repo, or head to the Rocketship releases page: https://github.com/ignition-is-go/rocketship/releases (private)
2. Create a 'Plugins' folder in your Unreal project directory (the directory containing your .uproject file) and place the extracted folder there
3. Open the Unreal project (NOTE: If you are prompted to rebuild missing data, accept)
4. In the plugins manager (Edit > Plugins), search for the 'RshipExec' plugin and verify it is active (checked on)
5. Open the Unreal project settings (Edit > Project Settings) and search for the 'Rship Exec' plugin
6. Enter the address of the Rocketship server you wish to connect to in the 'Rocketship Host' field. Also set the 'Service Color' :)

### Usage

#### Create a Target

1. Add an Rship Target component to any Actor blueprint
2. Name the Target in the Actor properties pane (in the level editor).

#### Create an Action

1. Create a function in the Actor blueprint's Event Graph. **Make sure to prefix the function name with RS_** to add it to the Actor Target 
2. Create function arguments to define an Action Schema, which determines the data structure required for triggering the Action

#### Create an Emitter

1. Create an event dispatcher in the Actor blueprint. **Prefix the event dispatcher with RS_** to add it to the Actor Target
2. Create event dispatcher properties to define an Emitter Schema, which determines the data structure pulsed by the Emitter

### Current Limitations

Due to some pesky aspects of Unreal's implementation of C++, we are still working out the following limitations:

- Emitter Schema Size: 
    - An Emitter Cchema can have at most 32 properties. 

- Unsupported Data Types:  
    - Vector, Rotation, and Position (but note these structures can be easily decomposed into floats)

## Targets, Actions, and Emitters


| Targets          | Emitters                                  | Actions                  |
|------------------|-------------------------------------------|--------------------------|
| **Actors**       | - Any Event dispatcher Parameter          | - Any Function Argument  |
