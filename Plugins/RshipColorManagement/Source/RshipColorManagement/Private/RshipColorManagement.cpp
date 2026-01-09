// Copyright Lucid. All Rights Reserved.

#include "RshipColorManagement.h"

DEFINE_LOG_CATEGORY(LogRshipColor);

#define LOCTEXT_NAMESPACE "FRshipColorManagementModule"

void FRshipColorManagementModule::StartupModule()
{
	UE_LOG(LogRshipColor, Log, TEXT("RshipColorManagement module starting up"));
}

void FRshipColorManagementModule::ShutdownModule()
{
	UE_LOG(LogRshipColor, Log, TEXT("RshipColorManagement module shutting down"));
}

FRshipColorManagementModule& FRshipColorManagementModule::Get()
{
	return FModuleManager::LoadModuleChecked<FRshipColorManagementModule>("RshipColorManagement");
}

bool FRshipColorManagementModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded("RshipColorManagement");
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRshipColorManagementModule, RshipColorManagement)
