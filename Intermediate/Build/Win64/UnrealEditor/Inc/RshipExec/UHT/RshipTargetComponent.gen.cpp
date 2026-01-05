// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "RshipTargetComponent.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

void EmptyLinkFunctionForGeneratedCodeRshipTargetComponent() {}

// ********** Begin Cross Module References ********************************************************
ENGINE_API UClass* Z_Construct_UClass_UActorComponent();
RSHIPEXEC_API UClass* Z_Construct_UClass_URshipTargetComponent();
RSHIPEXEC_API UClass* Z_Construct_UClass_URshipTargetComponent_NoRegister();
RSHIPEXEC_API UFunction* Z_Construct_UDelegateFunction_RshipExec_OnRshipData__DelegateSignature();
UPackage* Z_Construct_UPackage__Script_RshipExec();
// ********** End Cross Module References **********************************************************

// ********** Begin Delegate FOnRshipData **********************************************************
struct Z_Construct_UDelegateFunction_RshipExec_OnRshipData__DelegateSignature_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/RshipTargetComponent.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FDelegateFunctionParams FuncParams;
};
const UECodeGen_Private::FDelegateFunctionParams Z_Construct_UDelegateFunction_RshipExec_OnRshipData__DelegateSignature_Statics::FuncParams = { { (UObject*(*)())Z_Construct_UPackage__Script_RshipExec, nullptr, "OnRshipData__DelegateSignature", nullptr, 0, 0, RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x00130000, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UDelegateFunction_RshipExec_OnRshipData__DelegateSignature_Statics::Function_MetaDataParams), Z_Construct_UDelegateFunction_RshipExec_OnRshipData__DelegateSignature_Statics::Function_MetaDataParams)},  };
UFunction* Z_Construct_UDelegateFunction_RshipExec_OnRshipData__DelegateSignature()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUDelegateFunction(&ReturnFunction, Z_Construct_UDelegateFunction_RshipExec_OnRshipData__DelegateSignature_Statics::FuncParams);
	}
	return ReturnFunction;
}
void FOnRshipData_DelegateWrapper(const FMulticastScriptDelegate& OnRshipData)
{
	OnRshipData.ProcessMulticastDelegate<UObject>(NULL);
}
// ********** End Delegate FOnRshipData ************************************************************

// ********** Begin Class URshipTargetComponent Function Reconnect *********************************
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
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_URshipTargetComponent_Reconnect_Statics::FuncParams = { { (UObject*(*)())Z_Construct_UClass_URshipTargetComponent, nullptr, "Reconnect", nullptr, 0, 0, RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_URshipTargetComponent_Reconnect_Statics::Function_MetaDataParams), Z_Construct_UFunction_URshipTargetComponent_Reconnect_Statics::Function_MetaDataParams)},  };
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
// ********** End Class URshipTargetComponent Function Reconnect ***********************************

// ********** Begin Class URshipTargetComponent Function Register **********************************
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
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_URshipTargetComponent_Register_Statics::FuncParams = { { (UObject*(*)())Z_Construct_UClass_URshipTargetComponent, nullptr, "Register", nullptr, 0, 0, RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_URshipTargetComponent_Register_Statics::Function_MetaDataParams), Z_Construct_UFunction_URshipTargetComponent_Register_Statics::Function_MetaDataParams)},  };
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
// ********** End Class URshipTargetComponent Function Register ************************************

// ********** Begin Class URshipTargetComponent Function SetTargetId *******************************
struct Z_Construct_UFunction_URshipTargetComponent_SetTargetId_Statics
{
	struct RshipTargetComponent_eventSetTargetId_Parms
	{
		FString newTargetId;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "CallInEditor", "true" },
		{ "Category", "RshipTarget" },
		{ "ModuleRelativePath", "Public/RshipTargetComponent.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStrPropertyParams NewProp_newTargetId;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_URshipTargetComponent_SetTargetId_Statics::NewProp_newTargetId = { "newTargetId", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(RshipTargetComponent_eventSetTargetId_Parms, newTargetId), METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_URshipTargetComponent_SetTargetId_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_URshipTargetComponent_SetTargetId_Statics::NewProp_newTargetId,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_URshipTargetComponent_SetTargetId_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_URshipTargetComponent_SetTargetId_Statics::FuncParams = { { (UObject*(*)())Z_Construct_UClass_URshipTargetComponent, nullptr, "SetTargetId", Z_Construct_UFunction_URshipTargetComponent_SetTargetId_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_URshipTargetComponent_SetTargetId_Statics::PropPointers), sizeof(Z_Construct_UFunction_URshipTargetComponent_SetTargetId_Statics::RshipTargetComponent_eventSetTargetId_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_URshipTargetComponent_SetTargetId_Statics::Function_MetaDataParams), Z_Construct_UFunction_URshipTargetComponent_SetTargetId_Statics::Function_MetaDataParams)},  };
static_assert(sizeof(Z_Construct_UFunction_URshipTargetComponent_SetTargetId_Statics::RshipTargetComponent_eventSetTargetId_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_URshipTargetComponent_SetTargetId()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_URshipTargetComponent_SetTargetId_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(URshipTargetComponent::execSetTargetId)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_newTargetId);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->SetTargetId(Z_Param_newTargetId);
	P_NATIVE_END;
}
// ********** End Class URshipTargetComponent Function SetTargetId *********************************

// ********** Begin Class URshipTargetComponent ****************************************************
void URshipTargetComponent::StaticRegisterNativesURshipTargetComponent()
{
	UClass* Class = URshipTargetComponent::StaticClass();
	static const FNameNativePtrPair Funcs[] = {
		{ "Reconnect", &URshipTargetComponent::execReconnect },
		{ "Register", &URshipTargetComponent::execRegister },
		{ "SetTargetId", &URshipTargetComponent::execSetTargetId },
	};
	FNativeFunctionRegistrar::RegisterFunctions(Class, Funcs, UE_ARRAY_COUNT(Funcs));
}
FClassRegistrationInfo Z_Registration_Info_UClass_URshipTargetComponent;
UClass* URshipTargetComponent::GetPrivateStaticClass()
{
	using TClass = URshipTargetComponent;
	if (!Z_Registration_Info_UClass_URshipTargetComponent.InnerSingleton)
	{
		GetPrivateStaticClassBody(
			StaticPackage(),
			TEXT("RshipTargetComponent"),
			Z_Registration_Info_UClass_URshipTargetComponent.InnerSingleton,
			StaticRegisterNativesURshipTargetComponent,
			sizeof(TClass),
			alignof(TClass),
			TClass::StaticClassFlags,
			TClass::StaticClassCastFlags(),
			TClass::StaticConfigName(),
			(UClass::ClassConstructorType)InternalConstructor<TClass>,
			(UClass::ClassVTableHelperCtorCallerType)InternalVTableHelperCtorCaller<TClass>,
			UOBJECT_CPPCLASS_STATICFUNCTIONS_FORCLASS(TClass),
			&TClass::Super::StaticClass,
			&TClass::WithinClass::StaticClass
		);
	}
	return Z_Registration_Info_UClass_URshipTargetComponent.InnerSingleton;
}
UClass* Z_Construct_UClass_URshipTargetComponent_NoRegister()
{
	return URshipTargetComponent::GetPrivateStaticClass();
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
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_OnRshipData_MetaData[] = {
		{ "ModuleRelativePath", "Public/RshipTargetComponent.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_targetName_MetaData[] = {
		{ "Category", "RshipTarget" },
		{ "DisplayName", "Target Id" },
		{ "ModuleRelativePath", "Public/RshipTargetComponent.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FMulticastDelegatePropertyParams NewProp_OnRshipData;
	static const UECodeGen_Private::FStrPropertyParams NewProp_targetName;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FClassFunctionLinkInfo FuncInfo[] = {
		{ &Z_Construct_UFunction_URshipTargetComponent_Reconnect, "Reconnect" }, // 3039823271
		{ &Z_Construct_UFunction_URshipTargetComponent_Register, "Register" }, // 3288716340
		{ &Z_Construct_UFunction_URshipTargetComponent_SetTargetId, "SetTargetId" }, // 469023497
	};
	static_assert(UE_ARRAY_COUNT(FuncInfo) < 2048);
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<URshipTargetComponent>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FMulticastDelegatePropertyParams Z_Construct_UClass_URshipTargetComponent_Statics::NewProp_OnRshipData = { "OnRshipData", nullptr, (EPropertyFlags)0x0010000010080000, UECodeGen_Private::EPropertyGenFlags::InlineMulticastDelegate, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(URshipTargetComponent, OnRshipData), Z_Construct_UDelegateFunction_RshipExec_OnRshipData__DelegateSignature, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_OnRshipData_MetaData), NewProp_OnRshipData_MetaData) }; // 1619050811
const UECodeGen_Private::FStrPropertyParams Z_Construct_UClass_URshipTargetComponent_Statics::NewProp_targetName = { "targetName", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(URshipTargetComponent, targetName), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_targetName_MetaData), NewProp_targetName_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_URshipTargetComponent_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_URshipTargetComponent_Statics::NewProp_OnRshipData,
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
URshipTargetComponent::URshipTargetComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR(URshipTargetComponent);
URshipTargetComponent::~URshipTargetComponent() {}
// ********** End Class URshipTargetComponent ******************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_RshipPluginSource_Plugins_rship_unreal_Source_RshipExec_Public_RshipTargetComponent_h__Script_RshipExec_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_URshipTargetComponent, URshipTargetComponent::StaticClass, TEXT("URshipTargetComponent"), &Z_Registration_Info_UClass_URshipTargetComponent, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(URshipTargetComponent), 4207180872U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_RshipPluginSource_Plugins_rship_unreal_Source_RshipExec_Public_RshipTargetComponent_h__Script_RshipExec_2526931645(TEXT("/Script/RshipExec"),
	Z_CompiledInDeferFile_FID_RshipPluginSource_Plugins_rship_unreal_Source_RshipExec_Public_RshipTargetComponent_h__Script_RshipExec_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_RshipPluginSource_Plugins_rship_unreal_Source_RshipExec_Public_RshipTargetComponent_h__Script_RshipExec_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0);
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
