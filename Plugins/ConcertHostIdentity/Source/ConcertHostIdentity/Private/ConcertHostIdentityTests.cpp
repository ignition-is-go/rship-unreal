// Copyright Rocketship. All Rights Reserved.

#include "ConcertHostIdentity.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FConcertHostIdentityDeterminismTest,
	"Rship.ConcertHostIdentity.ColorFromHostname.IsDeterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcertHostIdentityDeterminismTest::RunTest(const FString& Parameters)
{
	const FLinearColor A = FConcertHostIdentityModule::ColorFromHostname(TEXT("WORKSTATION-01"));
	const FLinearColor B = FConcertHostIdentityModule::ColorFromHostname(TEXT("WORKSTATION-01"));
	TestEqual(TEXT("Same hostname produces same color"), A, B);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FConcertHostIdentityUniquenessTest,
	"Rship.ConcertHostIdentity.ColorFromHostname.ProducesDifferentColors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcertHostIdentityUniquenessTest::RunTest(const FString& Parameters)
{
	const FLinearColor ColorA = FConcertHostIdentityModule::ColorFromHostname(TEXT("STAGE-LEFT"));
	const FLinearColor ColorB = FConcertHostIdentityModule::ColorFromHostname(TEXT("STAGE-RIGHT"));
	const FLinearColor ColorC = FConcertHostIdentityModule::ColorFromHostname(TEXT("FOH-CONTROL"));

	TestNotEqual(TEXT("Different hostnames A vs B"), ColorA, ColorB);
	TestNotEqual(TEXT("Different hostnames A vs C"), ColorA, ColorC);
	TestNotEqual(TEXT("Different hostnames B vs C"), ColorB, ColorC);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FConcertHostIdentityValidRangeTest,
	"Rship.ConcertHostIdentity.ColorFromHostname.OutputIsValidRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FConcertHostIdentityValidRangeTest::RunTest(const FString& Parameters)
{
	const TArray<FString> Hostnames = {
		TEXT("NODE-001"), TEXT("NODE-002"), TEXT("NODE-003"),
		TEXT("MEDIA-SERVER"), TEXT("LIGHTING-DESK"), TEXT("AUDIO-CONSOLE")
	};

	for (const FString& Name : Hostnames)
	{
		const FLinearColor Color = FConcertHostIdentityModule::ColorFromHostname(Name);
		TestTrue(FString::Printf(TEXT("%s: R in [0,1]"), *Name), Color.R >= 0.f && Color.R <= 1.f);
		TestTrue(FString::Printf(TEXT("%s: G in [0,1]"), *Name), Color.G >= 0.f && Color.G <= 1.f);
		TestTrue(FString::Printf(TEXT("%s: B in [0,1]"), *Name), Color.B >= 0.f && Color.B <= 1.f);
	}
	return true;
}

#endif // WITH_AUTOMATION_TESTS
