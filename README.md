# Rship-Unreal

Rocketship's Unreal Engine integration is facilitated by the Rship-Unreal plugin.

## Quickstart

### Install and Configure the Plugin

1. Download and unzip the Rship-Unreal plugin.
2. Create a 'Plugins' folder in your Unreal project directory, next to the .uproject file (it may already exist), and place the extracted Rship-Unreal folder in the Plugins folder.
3. Open an Unreal project and verify the Rship-Unreal plugin is active in the Plugins Manager (you may need to restart Unreal).
4. Open Project Settings and change the Project Game Instance Class to RshipGameInstance.
5. Find the 'Rship Exec' item in the left sidebar of Project Settings.
6. Change the Rocketship Host field to the address of the Rocketship server you wish to connect to.
7. Optionally, set the Service Color.

### Usage

#### Connect a Project to Rocketship

1. Workflow TBD

#### Create a Target

1. Add an Rship Target component to any Actor blueprint

#### Create an Action

1. Create a function on the Actor blueprint. **Prefix the function name with RS_** to add it to the Actor Target 
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
