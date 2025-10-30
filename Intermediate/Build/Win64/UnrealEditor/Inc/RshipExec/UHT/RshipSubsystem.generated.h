// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "RshipSubsystem.h"

#ifdef RSHIPEXEC_RshipSubsystem_generated_h
#error "RshipSubsystem.generated.h already included, missing '#pragma once' in RshipSubsystem.h"
#endif
#define RSHIPEXEC_RshipSubsystem_generated_h

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// ********** Begin Delegate FRshipMessageDelegate *************************************************
#define FID_Users_Administrator_Documents_Unreal_Projects_RshipPluginSource_Plugins_RshipExec_Source_RshipExec_Public_RshipSubsystem_h_18_DELEGATE \
RSHIPEXEC_API void FRshipMessageDelegate_DelegateWrapper(const FScriptDelegate& RshipMessageDelegate);


// ********** End Delegate FRshipMessageDelegate ***************************************************

// ********** Begin Class URshipSubsystem **********************************************************
RSHIPEXEC_API UClass* Z_Construct_UClass_URshipSubsystem_NoRegister();

#define FID_Users_Administrator_Documents_Unreal_Projects_RshipPluginSource_Plugins_RshipExec_Source_RshipExec_Public_RshipSubsystem_h_26_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesURshipSubsystem(); \
	friend struct Z_Construct_UClass_URshipSubsystem_Statics; \
	static UClass* GetPrivateStaticClass(); \
	friend RSHIPEXEC_API UClass* Z_Construct_UClass_URshipSubsystem_NoRegister(); \
public: \
	DECLARE_CLASS2(URshipSubsystem, UEngineSubsystem, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/RshipExec"), Z_Construct_UClass_URshipSubsystem_NoRegister) \
	DECLARE_SERIALIZER(URshipSubsystem)


#define FID_Users_Administrator_Documents_Unreal_Projects_RshipPluginSource_Plugins_RshipExec_Source_RshipExec_Public_RshipSubsystem_h_26_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API URshipSubsystem(); \
	/** Deleted move- and copy-constructors, should never be used */ \
	URshipSubsystem(URshipSubsystem&&) = delete; \
	URshipSubsystem(const URshipSubsystem&) = delete; \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, URshipSubsystem); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(URshipSubsystem); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(URshipSubsystem) \
	NO_API virtual ~URshipSubsystem();


#define FID_Users_Administrator_Documents_Unreal_Projects_RshipPluginSource_Plugins_RshipExec_Source_RshipExec_Public_RshipSubsystem_h_23_PROLOG
#define FID_Users_Administrator_Documents_Unreal_Projects_RshipPluginSource_Plugins_RshipExec_Source_RshipExec_Public_RshipSubsystem_h_26_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_Users_Administrator_Documents_Unreal_Projects_RshipPluginSource_Plugins_RshipExec_Source_RshipExec_Public_RshipSubsystem_h_26_INCLASS_NO_PURE_DECLS \
	FID_Users_Administrator_Documents_Unreal_Projects_RshipPluginSource_Plugins_RshipExec_Source_RshipExec_Public_RshipSubsystem_h_26_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


class URshipSubsystem;

// ********** End Class URshipSubsystem ************************************************************

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_Users_Administrator_Documents_Unreal_Projects_RshipPluginSource_Plugins_RshipExec_Source_RshipExec_Public_RshipSubsystem_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
