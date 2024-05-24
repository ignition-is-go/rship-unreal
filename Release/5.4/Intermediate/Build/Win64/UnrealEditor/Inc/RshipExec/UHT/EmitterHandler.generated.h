// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "EmitterHandler.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef RSHIPEXEC_EmitterHandler_generated_h
#error "EmitterHandler.generated.h already included, missing '#pragma once' in EmitterHandler.h"
#endif
#define RSHIPEXEC_EmitterHandler_generated_h

#define FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_EmitterHandler_h_12_RPC_WRAPPERS_NO_PURE_DECLS \
	DECLARE_FUNCTION(execProcessEmitter);


#define FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_EmitterHandler_h_12_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesAEmitterHandler(); \
	friend struct Z_Construct_UClass_AEmitterHandler_Statics; \
public: \
	DECLARE_CLASS(AEmitterHandler, AActor, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/RshipExec"), NO_API) \
	DECLARE_SERIALIZER(AEmitterHandler)


#define FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_EmitterHandler_h_12_ENHANCED_CONSTRUCTORS \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	AEmitterHandler(AEmitterHandler&&); \
	AEmitterHandler(const AEmitterHandler&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, AEmitterHandler); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(AEmitterHandler); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(AEmitterHandler) \
	NO_API virtual ~AEmitterHandler();


#define FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_EmitterHandler_h_9_PROLOG
#define FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_EmitterHandler_h_12_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_EmitterHandler_h_12_RPC_WRAPPERS_NO_PURE_DECLS \
	FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_EmitterHandler_h_12_INCLASS_NO_PURE_DECLS \
	FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_EmitterHandler_h_12_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> RSHIPEXEC_API UClass* StaticClass<class AEmitterHandler>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_EmitterHandler_h


PRAGMA_ENABLE_DEPRECATION_WARNINGS
