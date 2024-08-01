// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "RshipExec/Public/RshipTargetComponent.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeRshipTargetComponent() {}

// Begin Cross Module References
ENGINE_API UClass* Z_Construct_UClass_UActorComponent();
RSHIPEXEC_API UClass* Z_Construct_UClass_URshipTargetComponent();
RSHIPEXEC_API UClass* Z_Construct_UClass_URshipTargetComponent_NoRegister();
UPackage* Z_Construct_UPackage__Script_RshipExec();
// End Cross Module References

// Begin Class URshipTargetComponent Function Reconnect
struct Z_Construct_UFunction_URshipTargetComponent_Reconnect_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "CallInEditor", "true" },
		{ "Category", "RshipTarget" },
		{ "ModuleRelativePath", "Public/RshipTargetComponent.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_URshipTargetComponent_Reconnect_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_URshipTargetComponent, nullptr, "Reconnect", nullptr, nullptr, nullptr, 0, 0, RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_URshipTargetComponent_Reconnect_Statics::Function_MetaDataParams), Z_Construct_UFunction_URshipTargetComponent_Reconnect_Statics::Function_MetaDataParams) };
UFunction* Z_Construct_UFunction_URshipTargetComponent_Reconnect()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_URshipTargetComponent_Reconnect_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(URshipTargetComponent::execReconnect)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->Reconnect();
	P_NATIVE_END;
}
// End Class URshipTargetComponent Function Reconnect

// Begin Class URshipTargetComponent Function Register
struct Z_Construct_UFunction_URshipTargetComponent_Register_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "CallInEditor", "true" },
		{ "Category", "RshipTarget" },
		{ "ModuleRelativePath", "Public/RshipTargetComponent.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_URshipTargetComponent_Register_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_URshipTargetComponent, nullptr, "Register", nullptr, nullptr, nullptr, 0, 0, RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_URshipTargetComponent_Register_Statics::Function_MetaDataParams), Z_Construct_UFunction_URshipTargetComponent_Register_Statics::Function_MetaDataParams) };
UFunction* Z_Construct_UFunction_URshipTargetComponent_Register()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_URshipTargetComponent_Register_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(URshipTargetComponent::execRegister)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->Register();
	P_NATIVE_END;
}
// End Class URshipTargetComponent Function Register

// Begin Class URshipTargetComponent
void URshipTargetComponent::StaticRegisterNativesURshipTargetComponent()
{
	UClass* Class = URshipTargetComponent::StaticClass();
	static const FNameNativePtrPair Funcs[] = {
		{ "Reconnect", &URshipTargetComponent::execReconnect },
		{ "Register", &URshipTargetComponent::execRegister },
	};
	FNativeFunctionRegistrar::RegisterFunctions(Class, Funcs, UE_ARRAY_COUNT(Funcs));
}
IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(URshipTargetComponent);
UClass* Z_Construct_UClass_URshipTargetComponent_NoRegister()
{
	return URshipTargetComponent::StaticClass();
}
struct Z_Construct_UClass_URshipTargetComponent_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "BlueprintSpawnableComponent", "" },
		{ "ClassGroupNames", "Custom" },
		{ "IncludePath", "RshipTargetComponent.h" },
		{ "ModuleRelativePath", "Public/RshipTargetComponent.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_targetName_MetaData[] = {
		{ "Category", "RshipTarget" },
		{ "DisplayName", "Target Name" },
		{ "ModuleRelativePath", "Public/RshipTargetComponent.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_targetName;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FClassFunctionLinkInfo FuncInfo[] = {
		{ &Z_Construct_UFunction_URshipTargetComponent_Reconnect, "Reconnect" }, // 4035552282
		{ &Z_Construct_UFunction_URshipTargetComponent_Register, "Register" }, // 2630145320
	};
	static_assert(UE_ARRAY_COUNT(FuncInfo) < 2048);
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<URshipTargetComponent>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_URshipTargetComponent_Statics::NewProp_targetName = { "targetName", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(URshipTargetComponent, targetName), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_targetName_MetaData), NewProp_targetName_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_URshipTargetComponent_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_URshipTargetComponent_Statics::NewProp_targetName,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_URshipTargetComponent_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_URshipTargetComponent_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UActorComponent,
	(UObject* (*)())Z_Construct_UPackage__Script_RshipExec,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_URshipTargetComponent_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_URshipTargetComponent_Statics::ClassParams = {
	&URshipTargetComponent::StaticClass,
	"Engine",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	FuncInfo,
	Z_Construct_UClass_URshipTargetComponent_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	UE_ARRAY_COUNT(FuncInfo),
	UE_ARRAY_COUNT(Z_Construct_UClass_URshipTargetComponent_Statics::PropPointers),
	0,
	0x00A000A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_URshipTargetComponent_Statics::Class_MetaDataParams), Z_Construct_UClass_URshipTargetComponent_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_URshipTargetComponent()
{
	if (!Z_Registration_Info_UClass_URshipTargetComponent.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_URshipTargetComponent.OuterSingleton, Z_Construct_UClass_URshipTargetComponent_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_URshipTargetComponent.OuterSingleton;
}
template<> RSHIPEXEC_API UClass* StaticClass<URshipTargetComponent>()
{
	return URshipTargetComponent::StaticClass();
}
URshipTargetComponent::URshipTargetComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR(URshipTargetComponent);
URshipTargetComponent::~URshipTargetComponent() {}
// End Class URshipTargetComponent

// Begin Registration
struct Z_CompiledInDeferFile_FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_RshipTargetComponent_h_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_URshipTargetComponent, URshipTargetComponent::StaticClass, TEXT("URshipTargetComponent"), &Z_Registration_Info_UClass_URshipTargetComponent, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(URshipTargetComponent), 3772872014U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_RshipTargetComponent_h_3751850(TEXT("/Script/RshipExec"),
	Z_CompiledInDeferFile_FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_RshipTargetComponent_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Unreal_Projects_RshipPluginDev054_Plugins_rship_unreal_Source_RshipExec_Public_RshipTargetComponent_h_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0);
// End Registration
PRAGMA_ENABLE_DEPRECATION_WARNINGS
