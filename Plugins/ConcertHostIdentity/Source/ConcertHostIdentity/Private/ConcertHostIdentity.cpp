// Copyright Rocketship. All Rights Reserved.

#include "ConcertHostIdentity.h"
#include "HAL/PlatformProcess.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#if __has_include("ConcertClientSettings.h")
#include "ConcertClientSettings.h"
#define HAS_CONCERT 1
#else
#define HAS_CONCERT 0
#endif

DEFINE_LOG_CATEGORY(LogConcertHostIdentity);

#define LOCTEXT_NAMESPACE "FConcertHostIdentityModule"

FLinearColor FConcertHostIdentityModule::ColorFromHostname(const FString& Hostname)
{
	const uint32 Hash = FCrc::StrCrc32(*Hostname);
	const float Hue = static_cast<float>(Hash % 360);
	const float Saturation = 0.65f;
	const float Value = 0.9f;
	return FLinearColor(Hue, Saturation, Value).HSVToLinearRGB();
}

void FConcertHostIdentityModule::StartupModule()
{
#if HAS_CONCERT
	FString Hostname;
#if PLATFORM_WINDOWS
	// FPlatformProcess::ComputerName() uses GetComputerName() which is
	// limited to 15 chars (NetBIOS). Use the DNS hostname instead.
	TCHAR DnsBuffer[256];
	DWORD DnsSize = UE_ARRAY_COUNT(DnsBuffer);
	if (GetComputerNameExW(ComputerNameDnsHostname, DnsBuffer, &DnsSize))
	{
		Hostname = DnsBuffer;
	}
	else
#endif
	{
		Hostname = FPlatformProcess::ComputerName();
	}

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

	Config->bInstallEditorToolbarButton = true;
	Config->ClientSettings.DisplayName = Hostname;
	Config->ClientSettings.AvatarColor = AvatarColor;
	Config->SaveConfig();

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
