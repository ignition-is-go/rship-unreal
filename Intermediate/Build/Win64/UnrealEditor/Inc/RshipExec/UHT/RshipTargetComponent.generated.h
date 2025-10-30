// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "RshipTargetComponent.h"

#ifdef RSHIPEXEC_RshipTargetComponent_generated_h
#error "RshipTargetComponent.generated.h already included, missing '#pragma once' in RshipTargetComponent.h"
#endif
#define RSHIPEXEC_RshipTargetComponent_generated_h

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// ********** Begin Class URshipTargetComponent ****************************************************
#define FID_Users_Administrator_Documents_Unreal_Projects_RshipPluginSource_Plugins_RshipExec_Source_RshipExec_Public_RshipTargetComponent_h_14_RPC_WRAPPERS_NO_PURE_DECLS \
	DECLARE_FUNCTION(execRegister); \
	DECLARE_FUNCTION(execReconnect);


RSHIPEXEC_API UClass* Z_Construct_UClass_URshipTargetComponent_NoRegister();

#define FID_Users_Administrator_Documents_Unreal_Projects_RshipPluginSource_Plugins_RshipExec_Source_RshipExec_Public_RshipTargetComponent_h_14_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesURshipTargetComponent(); \
	friend struct Z_Construct_UClass_URshipTargetComponent_Statics; \
	static UClass* GetPrivateStaticClass(); \
	friend RSHIPEXEC_API UClass* Z_Construct_UClass_URshipTargetComponent_NoRegister(); \
public: \
	DECLARE_CLASS2(URshipTargetComponent, UActorComponent, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/RshipExec"), Z_Construct_UClass_URshipTargetComponent_NoRegister) \
	DECLARE_SERIALIZER(URshipTargetComponent)


#define FID_Users_Administrator_Documents_Unreal_Projects_RshipPluginSource_Plugins_RshipExec_Source_RshipExec_Public_RshipTargetComponent_h_14_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API URshipTargetComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	/** Deleted move- and copy-constructors, should never be used */ \
	URshipTargetComponent(URshipTargetComponent&&) = delete; \
	URshipTargetComponent(const URshipTargetComponent&) = delete; \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, URshipTargetComponent); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(URshipTargetComponent); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(URshipTargetComponent) \
	NO_API virtual ~URshipTargetComponent();


#define FID_Users_Administrator_Documents_Unreal_Projects_RshipPluginSource_Plugins_RshipExec_Source_RshipExec_Public_RshipTargetComponent_h_11_PROLOG
#define FID_Users_Administrator_Documents_Unreal_Projects_RshipPluginSource_Plugins_RshipExec_Source_RshipExec_Public_RshipTargetComponent_h_14_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_Users_Administrator_Documents_Unreal_Projects_RshipPluginSource_Plugins_RshipExec_Source_RshipExec_Public_RshipTargetComponent_h_14_RPC_WRAPPERS_NO_PURE_DECLS \
	FID_Users_Administrator_Documents_Unreal_Projects_RshipPluginSource_Plugins_RshipExec_Source_RshipExec_Public_RshipTargetComponent_h_14_INCLASS_NO_PURE_DECLS \
	FID_Users_Administrator_Documents_Unreal_Projects_RshipPluginSource_Plugins_RshipExec_Source_RshipExec_Public_RshipTargetComponent_h_14_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


class URshipTargetComponent;

// ********** End Class URshipTargetComponent ******************************************************

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_Users_Administrator_Documents_Unreal_Projects_RshipPluginSource_Plugins_RshipExec_Source_RshipExec_Public_RshipTargetComponent_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
