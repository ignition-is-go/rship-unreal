// Rship PCG Binding
// Main include header for PCG integration

#pragma once

// Include all PCG types and components
#include "PCG/RshipPCGTypes.h"
#include "PCG/RshipPCGAutoBindComponent.h"
#include "PCG/RshipPCGManager.h"

// PCG Spawn Actor Settings is conditionally included based on PCG plugin availability
#if RSHIP_HAS_PCG
#include "PCG/RshipPCGSpawnActorSettings.h"
#endif
