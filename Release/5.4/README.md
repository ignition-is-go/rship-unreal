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
7. Verify that your Unreal Instance appears in the Rocketship UI (click 'Reconnect' if necessary)

### Usage

#### Create a Target

1. Add an Rship Target component to any Actor blueprint
2. Name the Target in the Actor properties pane (in the level editor).

#### Create an Action

1. Create a function in the Actor blueprint. **Make sure to prefix the function name with RS_** to add it to the Actor's Target 
2. Edit the function arguments to define an Action Schema, which determines the data structure required for triggering the Action

![Screenshot 2024-04-03 at 11 38 28 AM](https://github.com/ignition-is-go/rship-unreal/assets/131498134/92378b33-281f-48ff-9228-6434604c02b5)

3. Connect the RS function to some other function

![Unreal_Action_Blueprint_Example](https://github.com/ignition-is-go/rship-unreal/assets/131498134/3111ee2d-a857-4c35-998f-396f745dc9d7)

#### Create an Emitter

1. Create an event dispatcher in the Actor blueprint. **Make sure to prefix the event dispatcher name with RS_** to add it to the Actor's Target
2. Edit the event dispatcher properties to define an Emitter Schema, which determines the data structure pulsed by the Emitter

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
