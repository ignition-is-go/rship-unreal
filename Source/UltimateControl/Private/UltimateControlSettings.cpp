// Copyright Rocketship. All Rights Reserved.

#include "UltimateControlSettings.h"

#define LOCTEXT_NAMESPACE "UltimateControlSettings"

UUltimateControlSettings::UUltimateControlSettings()
{
	// Generate a default auth token if not set
	if (AuthToken.IsEmpty())
	{
		// Generate a simple random token
		AuthToken = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	}
}

FText UUltimateControlSettings::GetSectionText() const
{
	return LOCTEXT("SectionText", "Ultimate Control");
}

FText UUltimateControlSettings::GetSectionDescription() const
{
	return LOCTEXT("SectionDescription", "Configure the Ultimate Control HTTP JSON-RPC API server for AI agent and external tool integration.");
}

const UUltimateControlSettings* UUltimateControlSettings::Get()
{
	return GetDefault<UUltimateControlSettings>();
}

UUltimateControlSettings* UUltimateControlSettings::GetMutable()
{
	return GetMutableDefault<UUltimateControlSettings>();
}

#undef LOCTEXT_NAMESPACE
