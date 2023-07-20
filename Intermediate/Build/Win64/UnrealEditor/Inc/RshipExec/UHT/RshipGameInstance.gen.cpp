// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "RshipExec/Public/RshipGameInstance.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeRshipGameInstance() {}
// Cross Module References
	ENGINE_API UClass* Z_Construct_UClass_UGameInstance();
	RSHIPEXEC_API UClass* Z_Construct_UClass_URshipGameInstance();
	RSHIPEXEC_API UClass* Z_Construct_UClass_URshipGameInstance_NoRegister();
	UPackage* Z_Construct_UPackage__Script_RshipExec();
// End Cross Module References
	void URshipGameInstance::StaticRegisterNativesURshipGameInstance()
	{
	}
	IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(URshipGameInstance);
	UClass* Z_Construct_UClass_URshipGameInstance_NoRegister()
	{
		return URshipGameInstance::StaticClass();
	}
	struct Z_Construct_UClass_URshipGameInstance_Statics
	{
		static UObject* (*const DependentSingletons[])();
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[];
#endif
		static const FCppClassTypeInfoStatic StaticCppClassTypeInfo;
		static const UECodeGen_Private::FClassParams ClassParams;
	};
	UObject* (*const Z_Construct_UClass_URshipGameInstance_Statics::DependentSingletons[])() = {
		(UObject* (*)())Z_Construct_UClass_UGameInstance,
		(UObject* (*)())Z_Construct_UPackage__Script_RshipExec,
	};
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UClass_URshipGameInstance_Statics::Class_MetaDataParams[] = {
		{ "Comment", "/**\n *\n */" },
		{ "IncludePath", "RshipGameInstance.h" },
		{ "ModuleRelativePath", "Public/RshipGameInstance.h" },
	};
#endif
	const FCppClassTypeInfoStatic Z_Construct_UClass_URshipGameInstance_Statics::StaticCppClassTypeInfo = {
		TCppClassTypeTraits<URshipGameInstance>::IsAbstract,
	};
	const UECodeGen_Private::FClassParams Z_Construct_UClass_URshipGameInstance_Statics::ClassParams = {
		&URshipGameInstance::StaticClass,
		nullptr,
		&StaticCppClassTypeInfo,
		DependentSingletons,
		nullptr,
		nullptr,
		nullptr,
		UE_ARRAY_COUNT(DependentSingletons),
		0,
		0,
		0,
		0x008000A8u,
		METADATA_PARAMS(Z_Construct_UClass_URshipGameInstance_Statics::Class_MetaDataParams, UE_ARRAY_COUNT(Z_Construct_UClass_URshipGameInstance_Statics::Class_MetaDataParams))
	};
	UClass* Z_Construct_UClass_URshipGameInstance()
	{
		if (!Z_Registration_Info_UClass_URshipGameInstance.OuterSingleton)
		{
			UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_URshipGameInstance.OuterSingleton, Z_Construct_UClass_URshipGameInstance_Statics::ClassParams);
		}
		return Z_Registration_Info_UClass_URshipGameInstance.OuterSingleton;
	}
	template<> RSHIPEXEC_API UClass* StaticClass<URshipGameInstance>()
	{
		return URshipGameInstance::StaticClass();
	}
	DEFINE_VTABLE_PTR_HELPER_CTOR(URshipGameInstance);
	URshipGameInstance::~URshipGameInstance() {}
	struct Z_CompiledInDeferFile_FID_RshipExecPluginDev_Plugins_RshipExec_3_0_0_canary_52_Source_RshipExec_Public_RshipGameInstance_h_Statics
	{
		static const FClassRegisterCompiledInInfo ClassInfo[];
	};
	const FClassRegisterCompiledInInfo Z_CompiledInDeferFile_FID_RshipExecPluginDev_Plugins_RshipExec_3_0_0_canary_52_Source_RshipExec_Public_RshipGameInstance_h_Statics::ClassInfo[] = {
		{ Z_Construct_UClass_URshipGameInstance, URshipGameInstance::StaticClass, TEXT("URshipGameInstance"), &Z_Registration_Info_UClass_URshipGameInstance, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(URshipGameInstance), 417951396U) },
	};
	static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_RshipExecPluginDev_Plugins_RshipExec_3_0_0_canary_52_Source_RshipExec_Public_RshipGameInstance_h_2929129305(TEXT("/Script/RshipExec"),
		Z_CompiledInDeferFile_FID_RshipExecPluginDev_Plugins_RshipExec_3_0_0_canary_52_Source_RshipExec_Public_RshipGameInstance_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_RshipExecPluginDev_Plugins_RshipExec_3_0_0_canary_52_Source_RshipExec_Public_RshipGameInstance_h_Statics::ClassInfo),
		nullptr, 0,
		nullptr, 0);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
