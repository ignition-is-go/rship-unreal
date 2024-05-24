// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "RshipExec/Public/RshipSettings.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeRshipSettings() {}

// Begin Cross Module References
COREUOBJECT_API UClass* Z_Construct_UClass_UObject();
COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FLinearColor();
RSHIPEXEC_API UClass* Z_Construct_UClass_URshipSettings();
RSHIPEXEC_API UClass* Z_Construct_UClass_URshipSettings_NoRegister();
UPackage* Z_Construct_UPackage__Script_RshipExec();
// End Cross Module References

// Begin Class URshipSettings
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
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "DisplayName", "Rocketship Settings" },
		{ "IncludePath", "RshipSettings.h" },
		{ "ModuleRelativePath", "Public/RshipSettings.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_rshipHostAddress_MetaData[] = {
		{ "Category", "RshipExec" },
		{ "DisplayName", "Rocketship Host" },
		{ "ModuleRelativePath", "Public/RshipSettings.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ServiceColor_MetaData[] = {
		{ "Category", "RshipExec" },
		{ "DisplayName", "Service Color" },
		{ "ModuleRelativePath", "Public/RshipSettings.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_rshipHostAddress;
	static const UECodeGen_Private::FStructPropertyParams NewProp_ServiceColor;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<URshipSettings>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_URshipSettings_Statics::NewProp_rshipHostAddress = { "rshipHostAddress", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(URshipSettings, rshipHostAddress), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_rshipHostAddress_MetaData), NewProp_rshipHostAddress_MetaData) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_URshipSettings_Statics::NewProp_ServiceColor = { "ServiceColor", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(URshipSettings, ServiceColor), Z_Construct_UScriptStruct_FLinearColor, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ServiceColor_MetaData), NewProp_ServiceColor_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_URshipSettings_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_URshipSettings_Statics::NewProp_rshipHostAddress,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_URshipSettings_Statics::NewProp_ServiceColor,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_URshipSettings_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_URshipSettings_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UObject,
	(UObject* (*)())Z_Construct_UPackage__Script_RshipExec,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_URshipSettings_Statics::DependentSingletons) < 16);
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
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_URshipSettings_Statics::Class_MetaDataParams), Z_Construct_UClass_URshipSettings_Statics::Class_MetaDataParams)
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
URshipSettings::URshipSettings(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR(URshipSettings);
URshipSettings::~URshipSettings() {}
// End Class URshipSettings

// Begin Registration
struct Z_CompiledInDeferFile_FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_RshipSettings_h_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_URshipSettings, URshipSettings::StaticClass, TEXT("URshipSettings"), &Z_Registration_Info_UClass_URshipSettings, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(URshipSettings), 1563628159U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_RshipSettings_h_4017490990(TEXT("/Script/RshipExec"),
	Z_CompiledInDeferFile_FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_RshipSettings_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_RshipSettings_h_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0);
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
