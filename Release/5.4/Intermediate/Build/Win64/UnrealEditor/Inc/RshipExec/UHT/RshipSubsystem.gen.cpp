// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "RshipExec/Public/RshipSubsystem.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeRshipSubsystem() {}

// Begin Cross Module References
ENGINE_API UClass* Z_Construct_UClass_UEngineSubsystem();
RSHIPEXEC_API UClass* Z_Construct_UClass_URshipSubsystem();
RSHIPEXEC_API UClass* Z_Construct_UClass_URshipSubsystem_NoRegister();
RSHIPEXEC_API UFunction* Z_Construct_UDelegateFunction_RshipExec_RshipMessageDelegate__DelegateSignature();
UPackage* Z_Construct_UPackage__Script_RshipExec();
// End Cross Module References

// Begin Delegate FRshipMessageDelegate
struct Z_Construct_UDelegateFunction_RshipExec_RshipMessageDelegate__DelegateSignature_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/RshipSubsystem.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FFunctionParams Z_Construct_UDelegateFunction_RshipExec_RshipMessageDelegate__DelegateSignature_Statics::FuncParams = { (UObject*(*)())Z_Construct_UPackage__Script_RshipExec, nullptr, "RshipMessageDelegate__DelegateSignature", nullptr, nullptr, nullptr, 0, 0, RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x00120000, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UDelegateFunction_RshipExec_RshipMessageDelegate__DelegateSignature_Statics::Function_MetaDataParams), Z_Construct_UDelegateFunction_RshipExec_RshipMessageDelegate__DelegateSignature_Statics::Function_MetaDataParams) };
UFunction* Z_Construct_UDelegateFunction_RshipExec_RshipMessageDelegate__DelegateSignature()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UDelegateFunction_RshipExec_RshipMessageDelegate__DelegateSignature_Statics::FuncParams);
	}
	return ReturnFunction;
}
void FRshipMessageDelegate_DelegateWrapper(const FScriptDelegate& RshipMessageDelegate)
{
	RshipMessageDelegate.ProcessDelegate<UObject>(NULL);
}
// End Delegate FRshipMessageDelegate

// Begin Class URshipSubsystem
void URshipSubsystem::StaticRegisterNativesURshipSubsystem()
{
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(URshipSubsystem);
UClass* Z_Construct_UClass_URshipSubsystem_NoRegister()
{
	return URshipSubsystem::StaticClass();
}
struct Z_Construct_UClass_URshipSubsystem_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n *\n */" },
#endif
		{ "IncludePath", "RshipSubsystem.h" },
		{ "ModuleRelativePath", "Public/RshipSubsystem.h" },
	};
#endif // WITH_METADATA
	static UObject* (*const DependentSingletons[])();
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<URshipSubsystem>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
UObject* (*const Z_Construct_UClass_URshipSubsystem_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UEngineSubsystem,
	(UObject* (*)())Z_Construct_UPackage__Script_RshipExec,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_URshipSubsystem_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_URshipSubsystem_Statics::ClassParams = {
	&URshipSubsystem::StaticClass,
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
	0x001000A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_URshipSubsystem_Statics::Class_MetaDataParams), Z_Construct_UClass_URshipSubsystem_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_URshipSubsystem()
{
	if (!Z_Registration_Info_UClass_URshipSubsystem.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_URshipSubsystem.OuterSingleton, Z_Construct_UClass_URshipSubsystem_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_URshipSubsystem.OuterSingleton;
}
template<> RSHIPEXEC_API UClass* StaticClass<URshipSubsystem>()
{
	return URshipSubsystem::StaticClass();
}
URshipSubsystem::URshipSubsystem() {}
DEFINE_VTABLE_PTR_HELPER_CTOR(URshipSubsystem);
URshipSubsystem::~URshipSubsystem() {}
// End Class URshipSubsystem

// Begin Registration
struct Z_CompiledInDeferFile_FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_RshipSubsystem_h_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_URshipSubsystem, URshipSubsystem::StaticClass, TEXT("URshipSubsystem"), &Z_Registration_Info_UClass_URshipSubsystem, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(URshipSubsystem), 2457300179U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_RshipSubsystem_h_2470477822(TEXT("/Script/RshipExec"),
	Z_CompiledInDeferFile_FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_RshipSubsystem_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_RshipSubsystem_h_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0);
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
