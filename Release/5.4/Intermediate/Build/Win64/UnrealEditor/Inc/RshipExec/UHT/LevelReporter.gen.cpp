// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "RshipExec/Public/LevelReporter.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeLevelReporter() {}

// Begin Cross Module References
ENGINE_API UClass* Z_Construct_UClass_AActor();
RSHIPEXEC_API UClass* Z_Construct_UClass_ALevelReporter();
RSHIPEXEC_API UClass* Z_Construct_UClass_ALevelReporter_NoRegister();
UPackage* Z_Construct_UPackage__Script_RshipExec();
// End Cross Module References

// Begin Class ALevelReporter
void ALevelReporter::StaticRegisterNativesALevelReporter()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(ALevelReporter);
UClass* Z_Construct_UClass_ALevelReporter_NoRegister()
{
	return ALevelReporter::StaticClass();
}
struct Z_Construct_UClass_ALevelReporter_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "IncludePath", "LevelReporter.h" },
		{ "ModuleRelativePath", "Public/LevelReporter.h" },
	};
#endif // WITH_METADATA
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<ALevelReporter>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
UObject* (*const Z_Construct_UClass_ALevelReporter_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_AActor,
	(UObject* (*)())Z_Construct_UPackage__Script_RshipExec,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_ALevelReporter_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_ALevelReporter_Statics::ClassParams = {
	&ALevelReporter::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	nullptr,
	nullptr,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
	0,
	0,
	0x009000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_ALevelReporter_Statics::Class_MetaDataParams), Z_Construct_UClass_ALevelReporter_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_ALevelReporter()
{
	if (!Z_Registration_Info_UClass_ALevelReporter.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_ALevelReporter.OuterSingleton, Z_Construct_UClass_ALevelReporter_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_ALevelReporter.OuterSingleton;
}
template<> RSHIPEXEC_API UClass* StaticClass<ALevelReporter>()
{
	return ALevelReporter::StaticClass();
}
DEFINE_VTABLE_PTR_HELPER_CTOR(ALevelReporter);
ALevelReporter::~ALevelReporter() {}
// End Class ALevelReporter

// Begin Registration
struct Z_CompiledInDeferFile_FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_LevelReporter_h_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_ALevelReporter, ALevelReporter::StaticClass, TEXT("ALevelReporter"), &Z_Registration_Info_UClass_ALevelReporter, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(ALevelReporter), 1715950721U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_LevelReporter_h_2761198509(TEXT("/Script/RshipExec"),
	Z_CompiledInDeferFile_FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_LevelReporter_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_LevelReporter_h_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0);
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
