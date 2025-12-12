// Copyright Lucid. All Rights Reserved.

#include "RshipNDIStreaming.h"

DEFINE_LOG_CATEGORY(LogRshipNDI);

#define LOCTEXT_NAMESPACE "FRshipNDIStreamingModule"

void FRshipNDIStreamingModule::StartupModule()
{
	UE_LOG(LogRshipNDI, Log, TEXT("RshipNDIStreaming module starting up"));

#if RSHIP_HAS_NDI_SENDER
	UE_LOG(LogRshipNDI, Log, TEXT("Rust NDI sender library is available"));
#else
	UE_LOG(LogRshipNDI, Warning, TEXT("Rust NDI sender library is NOT available. NDI streaming will not work."));
	UE_LOG(LogRshipNDI, Warning, TEXT("To enable NDI streaming, build the Rust library:"));
	UE_LOG(LogRshipNDI, Warning, TEXT("  cd Plugins/RshipNDIStreaming/Source/RshipNDIStreaming/ThirdParty/rship-ndi-sender"));
	UE_LOG(LogRshipNDI, Warning, TEXT("  cargo build --release"));
#endif
}

void FRshipNDIStreamingModule::ShutdownModule()
{
	UE_LOG(LogRshipNDI, Log, TEXT("RshipNDIStreaming module shutting down"));
}

bool FRshipNDIStreamingModule::IsNDISenderAvailable()
{
#if RSHIP_HAS_NDI_SENDER
	return true;
#else
	return false;
#endif
}

FRshipNDIStreamingModule& FRshipNDIStreamingModule::Get()
{
	return FModuleManager::LoadModuleChecked<FRshipNDIStreamingModule>("RshipNDIStreaming");
}

bool FRshipNDIStreamingModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded("RshipNDIStreaming");
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRshipNDIStreamingModule, RshipNDIStreaming)
