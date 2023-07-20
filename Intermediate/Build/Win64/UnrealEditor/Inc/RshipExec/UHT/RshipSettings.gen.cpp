// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "RshipExec/Public/RshipSettings.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeRshipSettings() {}
// Cross Module References
	COREUOBJECT_API UClass* Z_Construct_UClass_UObject();
	COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FLinearColor();
	RSHIPEXEC_API UClass* Z_Construct_UClass_URshipSettings();
	RSHIPEXEC_API UClass* Z_Construct_UClass_URshipSettings_NoRegister();
	UPackage* Z_Construct_UPackage__Script_RshipExec();
// End Cross Module References
	void URshipSettings::StaticRegisterNativesURshipSettings()
	{
	}
	IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(URshipSettings);
	UClass* Z_Construct_UClass_URshipSettings_NoRegister()
	{
		return URshipSettings::StaticClass();
	}
	struct Z_Construct_UClass_URshipSettings_Statics
	{
		static UObject* (*const DependentSingletons[])();
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[];
#endif
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_rshipHostAddress_MetaData[];
#endif
		static const UECodeGen_Private::FStrPropertyParams NewProp_rshipHostAddress;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_ServiceColor_MetaData[];
#endif
		static const UECodeGen_Private::FStructPropertyParams NewProp_ServiceColor;
		static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const FCppClassTypeInfoStatic StaticCppClassTypeInfo;
		static const UECodeGen_Private::FClassParams ClassParams;
	};
	UObject* (*const Z_Construct_UClass_URshipSettings_Statics::DependentSingletons[])() = {
		(UObject* (*)())Z_Construct_UClass_UObject,
		(UObject* (*)())Z_Construct_UPackage__Script_RshipExec,
	};
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UClass_URshipSettings_Statics::Class_MetaDataParams[] = {
		{ "DisplayName", "Rocketship Settings" },
		{ "IncludePath", "RshipSettings.h" },
		{ "ModuleRelativePath", "Public/RshipSettings.h" },
	};
#endif
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UClass_URshipSettings_Statics::NewProp_rshipHostAddress_MetaData[] = {
		{ "Category", "RshipExec" },
		{ "DisplayName", "Rocketship Host" },
		{ "ModuleRelativePath", "Public/RshipSettings.h" },
	};
#endif
	const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_URshipSettings_Statics::NewProp_rshipHostAddress = { "rshipHostAddress", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, 1, nullptr, nullptr, STRUCT_OFFSET(URshipSettings, rshipHostAddress), METADATA_PARAMS(Z_Construct_UClass_URshipSettings_Statics::NewProp_rshipHostAddress_MetaData, UE_ARRAY_COUNT(Z_Construct_UClass_URshipSettings_Statics::NewProp_rshipHostAddress_MetaData)) };
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UClass_URshipSettings_Statics::NewProp_ServiceColor_MetaData[] = {
		{ "Category", "RshipExec" },
		{ "DisplayNAme", "Service Color" },
		{ "ModuleRelativePath", "Public/RshipSettings.h" },
	};
#endif
	const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_URshipSettings_Statics::NewProp_ServiceColor = { "ServiceColor", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, 1, nullptr, nullptr, STRUCT_OFFSET(URshipSettings, ServiceColor), Z_Construct_UScriptStruct_FLinearColor, METADATA_PARAMS(Z_Construct_UClass_URshipSettings_Statics::NewProp_ServiceColor_MetaData, UE_ARRAY_COUNT(Z_Construct_UClass_URshipSettings_Statics::NewProp_ServiceColor_MetaData)) };
	const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_URshipSettings_Statics::PropPointers[] = {
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_URshipSettings_Statics::NewProp_rshipHostAddress,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_URshipSettings_Statics::NewProp_ServiceColor,
	};
	const FCppClassTypeInfoStatic Z_Construct_UClass_URshipSettings_Statics::StaticCppClassTypeInfo = {
		TCppClassTypeTraits<URshipSettings>::IsAbstract,
	};
	const UECodeGen_Private::FClassParams Z_Construct_UClass_URshipSettings_Statics::ClassParams = {
		&URshipSettings::StaticClass,
		"Game",
		&StaticCppClassTypeInfo,
		DependentSingletons,
		nullptr,
		Z_Construct_UClass_URshipSettings_Statics::PropPointers,
		nullptr,
		UE_ARRAY_COUNT(DependentSingletons),
		0,
		UE_ARRAY_COUNT(Z_Construct_UClass_URshipSettings_Statics::PropPointers),
		0,
		0x000000A6u,
		METADATA_PARAMS(Z_Construct_UClass_URshipSettings_Statics::Class_MetaDataParams, UE_ARRAY_COUNT(Z_Construct_UClass_URshipSettings_Statics::Class_MetaDataParams))
	};
	UClass* Z_Construct_UClass_URshipSettings()
	{
		if (!Z_Registration_Info_UClass_URshipSettings.OuterSingleton)
		{
			UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_URshipSettings.OuterSingleton, Z_Construct_UClass_URshipSettings_Statics::ClassParams);
		}
		return Z_Registration_Info_UClass_URshipSettings.OuterSingleton;
	}
	template<> RSHIPEXEC_API UClass* StaticClass<URshipSettings>()
	{
		return URshipSettings::StaticClass();
	}
	DEFINE_VTABLE_PTR_HELPER_CTOR(URshipSettings);
	URshipSettings::~URshipSettings() {}
	struct Z_CompiledInDeferFile_FID_RshipExecPluginDev_Plugins_RshipExec_3_0_0_canary_52_Source_RshipExec_Public_RshipSettings_h_Statics
	{
		static const FClassRegisterCompiledInInfo ClassInfo[];
	};
	const FClassRegisterCompiledInInfo Z_CompiledInDeferFile_FID_RshipExecPluginDev_Plugins_RshipExec_3_0_0_canary_52_Source_RshipExec_Public_RshipSettings_h_Statics::ClassInfo[] = {
		{ Z_Construct_UClass_URshipSettings, URshipSettings::StaticClass, TEXT("URshipSettings"), &Z_Registration_Info_UClass_URshipSettings, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(URshipSettings), 1976246316U) },
	};
	static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_RshipExecPluginDev_Plugins_RshipExec_3_0_0_canary_52_Source_RshipExec_Public_RshipSettings_h_1463284031(TEXT("/Script/RshipExec"),
		Z_CompiledInDeferFile_FID_RshipExecPluginDev_Plugins_RshipExec_3_0_0_canary_52_Source_RshipExec_Public_RshipSettings_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_RshipExecPluginDev_Plugins_RshipExec_3_0_0_canary_52_Source_RshipExec_Public_RshipSettings_h_Statics::ClassInfo),
		nullptr, 0,
		nullptr, 0);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
