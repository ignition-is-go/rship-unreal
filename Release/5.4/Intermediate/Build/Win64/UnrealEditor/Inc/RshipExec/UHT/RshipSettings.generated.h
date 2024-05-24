// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "RshipSettings.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#ifdef RSHIPEXEC_RshipSettings_generated_h
#error "RshipSettings.generated.h already included, missing '#pragma once' in RshipSettings.h"
#endif
#define RSHIPEXEC_RshipSettings_generated_h

#define FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_RshipSettings_h_10_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesURshipSettings(); \
	friend struct Z_Construct_UClass_URshipSettings_Statics; \
public: \
	DECLARE_CLASS(URshipSettings, UObject, COMPILED_IN_FLAGS(0 | CLASS_DefaultConfig | CLASS_Config), CASTCLASS_None, TEXT("/Script/RshipExec"), NO_API) \
	DECLARE_SERIALIZER(URshipSettings) \
	static const TCHAR* StaticConfigName() {return TEXT("Game");} \



#define FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_RshipSettings_h_10_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API URshipSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
private: \
	/** Private move- and copy-constructors, should never be used */ \
	URshipSettings(URshipSettings&&); \
	URshipSettings(const URshipSettings&); \
public: \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, URshipSettings); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(URshipSettings); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(URshipSettings) \
	NO_API virtual ~URshipSettings();


#define FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_RshipSettings_h_7_PROLOG
#define FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_RshipSettings_h_10_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_RshipSettings_h_10_INCLASS_NO_PURE_DECLS \
	FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_RshipSettings_h_10_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


template<> RSHIPEXEC_API UClass* StaticClass<class URshipSettings>();

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_RshipSettings_h


PRAGMA_ENABLE_DEPRECATION_WARNINGS
