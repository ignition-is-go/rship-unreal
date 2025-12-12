// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "Runtime/Launch/Resources/Version.h"

/**
 * UE Version compatibility macros for UltimateControl
 * Supports UE 5.5, 5.6, and 5.7
 */

// Check if we're on UE 5.6 or later
#define UE_VERSION_AT_LEAST(Major, Minor) \
	(ENGINE_MAJOR_VERSION > (Major) || (ENGINE_MAJOR_VERSION == (Major) && ENGINE_MINOR_VERSION >= (Minor)))

// UE 5.6+ specific features
#define ULTIMATE_CONTROL_UE_5_6_OR_LATER UE_VERSION_AT_LEAST(5, 6)

// UE 5.7+ specific features (for future use)
#define ULTIMATE_CONTROL_UE_5_7_OR_LATER UE_VERSION_AT_LEAST(5, 7)

/**
 * API Changes in UE 5.6:
 * - GetEngineShowFlags() returns pointer instead of reference
 * - IAutomationReport::GetState() takes (ClusterIndex, PassIndex) instead of just (ClusterIndex)
 * - FProjectDescriptor::TargetPlatforms is TArray<FName> instead of TArray<FString>
 * - UAnimSequence::GetFrameRate() renamed to GetSamplingFrameRate()
 * - FMaterialParameterInfo replaced with FHashedMaterialParameterInfo
 * - IHotReloadInterface::GetHotReloadInterface() removed (use Live Coding)
 */
