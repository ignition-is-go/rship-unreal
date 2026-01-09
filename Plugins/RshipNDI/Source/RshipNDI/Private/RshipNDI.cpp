// Copyright Lucid. All Rights Reserved.

#include "RshipNDI.h"

DEFINE_LOG_CATEGORY(LogRshipNDI);

#define LOCTEXT_NAMESPACE "FRshipNDIModule"

void FRshipNDIModule::StartupModule()
{
	UE_LOG(LogRshipNDI, Log, TEXT("RshipNDI module starting up"));

#if RSHIP_HAS_NDI_SENDER
	UE_LOG(LogRshipNDI, Log, TEXT("Rust NDI sender library is available"));
#else
	UE_LOG(LogRshipNDI, Warning, TEXT("Rust NDI sender library is NOT available. NDI streaming will not work."));
	UE_LOG(LogRshipNDI, Warning, TEXT("To enable NDI streaming, build the Rust library:"));
	UE_LOG(LogRshipNDI, Warning, TEXT("  cd Plugins/RshipNDI/Source/RshipNDI/ThirdParty/rship-ndi-sender"));
	UE_LOG(LogRshipNDI, Warning, TEXT("  cargo build --release"));
#endif
}

void FRshipNDIModule::ShutdownModule()
{
	UE_LOG(LogRshipNDI, Log, TEXT("RshipNDI module shutting down"));
}

bool FRshipNDIModule::IsNDISenderAvailable()
{
#if RSHIP_HAS_NDI_SENDER
	return true;
#else
	return false;
#endif
}

FRshipNDIModule& FRshipNDIModule::Get()
{
	return FModuleManager::LoadModuleChecked<FRshipNDIModule>("RshipNDI");
}

bool FRshipNDIModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded("RshipNDI");
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRshipNDIModule, RshipNDI)
