// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "RshipExec/Public/RshipTargetComponent.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeRshipTargetComponent() {}
// Cross Module References
	ENGINE_API UClass* Z_Construct_UClass_UActorComponent();
	RSHIPEXEC_API UClass* Z_Construct_UClass_URshipTargetComponent();
	RSHIPEXEC_API UClass* Z_Construct_UClass_URshipTargetComponent_NoRegister();
	RSHIPEXEC_API UFunction* Z_Construct_UDelegateFunction_RshipExec_ActionCallBack__DelegateSignature();
	UPackage* Z_Construct_UPackage__Script_RshipExec();
// End Cross Module References
	struct Z_Construct_UDelegateFunction_RshipExec_ActionCallBack__DelegateSignature_Statics
	{
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[];
#endif
		static const UECodeGen_Private::FFunctionParams FuncParams;
	};
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UDelegateFunction_RshipExec_ActionCallBack__DelegateSignature_Statics::Function_MetaDataParams[] = {
		{ "ModuleRelativePath", "Public/RshipTargetComponent.h" },
	};
#endif
	const UECodeGen_Private::FFunctionParams Z_Construct_UDelegateFunction_RshipExec_ActionCallBack__DelegateSignature_Statics::FuncParams = { (UObject*(*)())Z_Construct_UPackage__Script_RshipExec, nullptr, "ActionCallBack__DelegateSignature", nullptr, nullptr, 0, nullptr, 0, RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x00120000, 0, 0, METADATA_PARAMS(Z_Construct_UDelegateFunction_RshipExec_ActionCallBack__DelegateSignature_Statics::Function_MetaDataParams, UE_ARRAY_COUNT(Z_Construct_UDelegateFunction_RshipExec_ActionCallBack__DelegateSignature_Statics::Function_MetaDataParams)) };
	UFunction* Z_Construct_UDelegateFunction_RshipExec_ActionCallBack__DelegateSignature()
	{
		static UFunction* ReturnFunction = nullptr;
		if (!ReturnFunction)
		{
			UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UDelegateFunction_RshipExec_ActionCallBack__DelegateSignature_Statics::FuncParams);
		}
		return ReturnFunction;
	}
	DEFINE_FUNCTION(URshipTargetComponent::execBindAction)
	{
		P_GET_PROPERTY(FDelegateProperty,Z_Param_callback);
		P_GET_PROPERTY(FStrProperty,Z_Param_actionId);
		P_FINISH;
		P_NATIVE_BEGIN;
		P_THIS->BindAction(FActionCallBack(Z_Param_callback),Z_Param_actionId);
		P_NATIVE_END;
	}
	void URshipTargetComponent::StaticRegisterNativesURshipTargetComponent()
	{
		UClass* Class = URshipTargetComponent::StaticClass();
		static const FNameNativePtrPair Funcs[] = {
			{ "BindAction", &URshipTargetComponent::execBindAction },
		};
		FNativeFunctionRegistrar::RegisterFunctions(Class, Funcs, UE_ARRAY_COUNT(Funcs));
	}
	struct Z_Construct_UFunction_URshipTargetComponent_BindAction_Statics
	{
		struct RshipTargetComponent_eventBindAction_Parms
		{
			FScriptDelegate callback;
			FString actionId;
		};
		static const UECodeGen_Private::FDelegatePropertyParams NewProp_callback;
		static const UECodeGen_Private::FStrPropertyParams NewProp_actionId;
		static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[];
#endif
		static const UECodeGen_Private::FFunctionParams FuncParams;
	};
	const UECodeGen_Private::FDelegatePropertyParams Z_Construct_UFunction_URshipTargetComponent_BindAction_Statics::NewProp_callback = { "callback", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Delegate, RF_Public|RF_Transient|RF_MarkAsNative, 1, nullptr, nullptr, STRUCT_OFFSET(RshipTargetComponent_eventBindAction_Parms, callback), Z_Construct_UDelegateFunction_RshipExec_ActionCallBack__DelegateSignature, METADATA_PARAMS(nullptr, 0) }; // 2233971257
	const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_URshipTargetComponent_BindAction_Statics::NewProp_actionId = { "actionId", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, 1, nullptr, nullptr, STRUCT_OFFSET(RshipTargetComponent_eventBindAction_Parms, actionId), METADATA_PARAMS(nullptr, 0) };
	const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_URshipTargetComponent_BindAction_Statics::PropPointers[] = {
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_URshipTargetComponent_BindAction_Statics::NewProp_callback,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_URshipTargetComponent_BindAction_Statics::NewProp_actionId,
	};
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UFunction_URshipTargetComponent_BindAction_Statics::Function_MetaDataParams[] = {
		{ "Category", "RShip" },
		{ "ModuleRelativePath", "Public/RshipTargetComponent.h" },
	};
#endif
	const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_URshipTargetComponent_BindAction_Statics::FuncParams = { (UObject*(*)())Z_Construct_UClass_URshipTargetComponent, nullptr, "BindAction", nullptr, nullptr, sizeof(Z_Construct_UFunction_URshipTargetComponent_BindAction_Statics::RshipTargetComponent_eventBindAction_Parms), Z_Construct_UFunction_URshipTargetComponent_BindAction_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_URshipTargetComponent_BindAction_Statics::PropPointers), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(Z_Construct_UFunction_URshipTargetComponent_BindAction_Statics::Function_MetaDataParams, UE_ARRAY_COUNT(Z_Construct_UFunction_URshipTargetComponent_BindAction_Statics::Function_MetaDataParams)) };
	UFunction* Z_Construct_UFunction_URshipTargetComponent_BindAction()
	{
		static UFunction* ReturnFunction = nullptr;
		if (!ReturnFunction)
		{
			UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_URshipTargetComponent_BindAction_Statics::FuncParams);
		}
		return ReturnFunction;
	}
	IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(URshipTargetComponent);
	UClass* Z_Construct_UClass_URshipTargetComponent_NoRegister()
	{
		return URshipTargetComponent::StaticClass();
	}
	struct Z_Construct_UClass_URshipTargetComponent_Statics
	{
		static UObject* (*const DependentSingletons[])();
		static const FClassFunctionLinkInfo FuncInfo[];
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[];
#endif
		static const FCppClassTypeInfoStatic StaticCppClassTypeInfo;
		static const UECodeGen_Private::FClassParams ClassParams;
	};
	UObject* (*const Z_Construct_UClass_URshipTargetComponent_Statics::DependentSingletons[])() = {
		(UObject* (*)())Z_Construct_UClass_UActorComponent,
		(UObject* (*)())Z_Construct_UPackage__Script_RshipExec,
	};
	const FClassFunctionLinkInfo Z_Construct_UClass_URshipTargetComponent_Statics::FuncInfo[] = {
		{ &Z_Construct_UFunction_URshipTargetComponent_BindAction, "BindAction" }, // 647511562
	};
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UClass_URshipTargetComponent_Statics::Class_MetaDataParams[] = {
		{ "BlueprintSpawnableComponent", "" },
		{ "ClassGroupNames", "Custom" },
		{ "IncludePath", "RshipTargetComponent.h" },
		{ "ModuleRelativePath", "Public/RshipTargetComponent.h" },
	};
#endif
	const FCppClassTypeInfoStatic Z_Construct_UClass_URshipTargetComponent_Statics::StaticCppClassTypeInfo = {
		TCppClassTypeTraits<URshipTargetComponent>::IsAbstract,
	};
	const UECodeGen_Private::FClassParams Z_Construct_UClass_URshipTargetComponent_Statics::ClassParams = {
		&URshipTargetComponent::StaticClass,
		"Engine",
		&StaticCppClassTypeInfo,
		DependentSingletons,
		FuncInfo,
		nullptr,
		nullptr,
		UE_ARRAY_COUNT(DependentSingletons),
		UE_ARRAY_COUNT(FuncInfo),
		0,
		0,
		0x00A000A4u,
		METADATA_PARAMS(Z_Construct_UClass_URshipTargetComponent_Statics::Class_MetaDataParams, UE_ARRAY_COUNT(Z_Construct_UClass_URshipTargetComponent_Statics::Class_MetaDataParams))
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
	DEFINE_VTABLE_PTR_HELPER_CTOR(URshipTargetComponent);
	URshipTargetComponent::~URshipTargetComponent() {}
	struct Z_CompiledInDeferFile_FID_RshipExecPluginDev_Plugins_RshipExec_3_0_0_canary_52_Source_RshipExec_Public_RshipTargetComponent_h_Statics
	{
		static const FClassRegisterCompiledInInfo ClassInfo[];
	};
	const FClassRegisterCompiledInInfo Z_CompiledInDeferFile_FID_RshipExecPluginDev_Plugins_RshipExec_3_0_0_canary_52_Source_RshipExec_Public_RshipTargetComponent_h_Statics::ClassInfo[] = {
		{ Z_Construct_UClass_URshipTargetComponent, URshipTargetComponent::StaticClass, TEXT("URshipTargetComponent"), &Z_Registration_Info_UClass_URshipTargetComponent, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(URshipTargetComponent), 2590297032U) },
	};
	static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_RshipExecPluginDev_Plugins_RshipExec_3_0_0_canary_52_Source_RshipExec_Public_RshipTargetComponent_h_2817258030(TEXT("/Script/RshipExec"),
		Z_CompiledInDeferFile_FID_RshipExecPluginDev_Plugins_RshipExec_3_0_0_canary_52_Source_RshipExec_Public_RshipTargetComponent_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_RshipExecPluginDev_Plugins_RshipExec_3_0_0_canary_52_Source_RshipExec_Public_RshipTargetComponent_h_Statics::ClassInfo),
		nullptr, 0,
		nullptr, 0);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
