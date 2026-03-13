// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "Runtime/Launch/Resources/Version.h"

/**
 * Rship Version compatibility macros
 * Supports UE 5.5, 5.6, and 5.7
 *
 * Include this header in files that need version-specific code paths.
 * Other plugins (Rship2110, RshipNDI, RshipSpatialAudio) can depend on
 * RshipExec and use these macros.
 */

// Generic version check macro
#define RSHIP_UE_VERSION_AT_LEAST(Major, Minor) \
	(ENGINE_MAJOR_VERSION > (Major) || (ENGINE_MAJOR_VERSION == (Major) && ENGINE_MINOR_VERSION >= (Minor)))

// Version-specific feature flags
#define RSHIP_UE_5_5_OR_LATER RSHIP_UE_VERSION_AT_LEAST(5, 5)
#define RSHIP_UE_5_6_OR_LATER RSHIP_UE_VERSION_AT_LEAST(5, 6)
#define RSHIP_UE_5_7_OR_LATER RSHIP_UE_VERSION_AT_LEAST(5, 7)

/**
 * Known API Changes by Version:
 *
 * UE 5.6:
 * - FViewportClient::GetEngineShowFlags() returns pointer instead of reference
 * - IAutomationReport::GetState() takes (ClusterIndex, PassIndex) instead of just (ClusterIndex)
 * - FProjectDescriptor::TargetPlatforms is TArray<FName> instead of TArray<FString>
 * - UAnimSequence::GetFrameRate() renamed to GetSamplingFrameRate()
 * - FMaterialParameterInfo replaced with FHashedMaterialParameterInfo
 * - IHotReloadInterface::GetHotReloadInterface() removed (use Live Coding)
 * - FTabManager::GetAllSpawnerTabIds() removed
 * - GEditor->SetGridSize() removed
 * - FEditorModeTools::GetActiveScriptableModes() removed
 * - UNavigationSystemV1::IsNavigationBuildingNow() renamed to IsNavigationBuildInProgress()
 * - UNavigationSystemV1::TestPathSync() returns bool instead of ENavigationQueryResult
 * - AGroupActor::Lock()/Unlock() removed
 * - FViewportClient::ShouldShowFPS() removed (use stats commands)
 * - FNiagaraParameterStore::ReadParameterVariables() returns TArrayView<const FNiagaraVariableWithOffset>
 * - FRigControlValue::Set<FRotator>/Set<FTransform> changed for Control Rig
 *
 * UE 5.7:
 * - SDL2 to SDL3 transition on Linux
 * - Substrate materials production-ready
 * - PCG framework production-ready
 * - MegaLights directional and particle lighting beta
 * - Nanite Foliage experimental
 *
 * Note: Most internal APIs remain stable between 5.6 and 5.7. The RHI,
 * rendering, and core systems maintain backward compatibility.
 */
