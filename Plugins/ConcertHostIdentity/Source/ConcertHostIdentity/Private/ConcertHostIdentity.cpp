// Copyright Rocketship. All Rights Reserved.

#include "ConcertHostIdentity.h"
#include "HAL/PlatformProcess.h"

#if __has_include("ConcertClientConfig.h")
#include "ConcertClientConfig.h"
#define HAS_CONCERT 1
#else
#define HAS_CONCERT 0
#endif

DEFINE_LOG_CATEGORY(LogConcertHostIdentity);

#define LOCTEXT_NAMESPACE "FConcertHostIdentityModule"

namespace
{
	// Deterministic color from a string: hash to pick hue, fixed saturation/value
	FLinearColor ColorFromHostname(const FString& Hostname)
	{
		const uint32 Hash = FCrc::StrCrc32(*Hostname.ToLower());
		const float Hue = (Hash % 360);
		const float Saturation = 0.65f;
		const float Value = 0.9f;
		return FLinearColor::MakeFromHSV8(
			static_cast<uint8>(Hue * 255.f / 360.f),
			static_cast<uint8>(Saturation * 255.f),
			static_cast<uint8>(Value * 255.f));
	}
}

void FConcertHostIdentityModule::StartupModule()
{
#if HAS_CONCERT
	const FString Hostname = FPlatformProcess::ComputerName();

	if (Hostname.IsEmpty())
	{
		UE_LOG(LogConcertHostIdentity, Warning, TEXT("Could not determine hostname, skipping Concert identity setup"));
		return;
	}

	UConcertClientConfig* Config = GetMutableDefault<UConcertClientConfig>();
	if (!Config)
	{
		UE_LOG(LogConcertHostIdentity, Warning, TEXT("UConcertClientConfig not available"));
		return;
	}

	const FLinearColor AvatarColor = ColorFromHostname(Hostname);

	Config->ClientSettings.DisplayName = Hostname;
	Config->ClientSettings.AvatarColor = AvatarColor;

	UE_LOG(LogConcertHostIdentity, Log, TEXT("Set Concert display name to \"%s\" with color (R=%.2f G=%.2f B=%.2f)"),
		*Hostname, AvatarColor.R, AvatarColor.G, AvatarColor.B);
#else
	UE_LOG(LogConcertHostIdentity, Log, TEXT("Concert not available, module inactive"));
#endif
}

void FConcertHostIdentityModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FConcertHostIdentityModule, ConcertHostIdentity)
