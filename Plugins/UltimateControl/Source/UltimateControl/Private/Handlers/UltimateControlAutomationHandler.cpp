// Copyright Rocketship. All Rights Reserved.

#include "Handlers/UltimateControlAutomationHandler.h"
#include "UltimateControlSubsystem.h"
#include "UltimateControl.h"
#include "UltimateControlVersion.h"

#include "Misc/AutomationTest.h"
#include "IAutomationControllerModule.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"

FUltimateControlAutomationHandler::FUltimateControlAutomationHandler(UUltimateControlSubsystem* InSubsystem)
	: FUltimateControlHandlerBase(InSubsystem)
{
	RegisterMethod(
		TEXT("automation.listTests"),
		TEXT("List all available automation tests"),
		TEXT("Automation"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAutomationHandler::HandleListTests));

	RegisterMethod(
		TEXT("automation.runTests"),
		TEXT("Run specified automation tests"),
		TEXT("Automation"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAutomationHandler::HandleRunTests));

	RegisterMethod(
		TEXT("automation.getTestResults"),
		TEXT("Get results of automation tests"),
		TEXT("Automation"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAutomationHandler::HandleGetTestResults));

	RegisterMethod(
		TEXT("build.cook"),
		TEXT("Cook content for a target platform"),
		TEXT("Build"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAutomationHandler::HandleCook),
		/* bIsDangerous */ true);

	RegisterMethod(
		TEXT("build.package"),
		TEXT("Package the project for distribution"),
		TEXT("Build"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAutomationHandler::HandlePackage),
		/* bIsDangerous */ true);

	RegisterMethod(
		TEXT("build.getStatus"),
		TEXT("Get current build status"),
		TEXT("Build"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAutomationHandler::HandleGetStatus));

	RegisterMethod(
		TEXT("build.runUAT"),
		TEXT("Run an Unreal Automation Tool command"),
		TEXT("Build"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlAutomationHandler::HandleRunUAT),
		/* bIsDangerous */ true);
}

bool FUltimateControlAutomationHandler::HandleListTests(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Filter = GetOptionalString(Params, TEXT("filter"), TEXT(""));

	TArray<TSharedPtr<FJsonValue>> TestsArray;

	// Get all automation tests
	TArray<FAutomationTestInfo> TestInfos;
	FAutomationTestFramework::Get().GetValidTestNames(TestInfos);

	for (const FAutomationTestInfo& TestInfo : TestInfos)
	{
		if (!Filter.IsEmpty() && !TestInfo.GetDisplayName().Contains(Filter))
		{
			continue;
		}

		TSharedPtr<FJsonObject> TestObj = MakeShared<FJsonObject>();
		TestObj->SetStringField(TEXT("name"), TestInfo.GetTestName());
		TestObj->SetStringField(TEXT("displayName"), TestInfo.GetDisplayName());
		TestObj->SetNumberField(TEXT("testFlags"), static_cast<int64>(TestInfo.GetTestFlags()));

		// Determine test type from flags
		// Note: EAutomationTestFlags is an enum class in UE 5.6+
		uint32 Flags = static_cast<uint32>(TestInfo.GetTestFlags());
		FString TestType = TEXT("Unknown");
		// Note: EAutomationTestFlags is an enum class in UE 5.6, need to cast for bitwise operations
		if (Flags & static_cast<uint32>(EAutomationTestFlags::SmokeFilter))
		{
			TestType = TEXT("Smoke");
		}
		else if (Flags & static_cast<uint32>(EAutomationTestFlags::EngineFilter))
		{
			TestType = TEXT("Engine");
		}
		else if (Flags & static_cast<uint32>(EAutomationTestFlags::ProductFilter))
		{
			TestType = TEXT("Product");
		}
		else if (Flags & static_cast<uint32>(EAutomationTestFlags::PerfFilter))
		{
			TestType = TEXT("Performance");
		}
		else if (Flags & static_cast<uint32>(EAutomationTestFlags::StressFilter))
		{
			TestType = TEXT("Stress");
		}
		TestObj->SetStringField(TEXT("type"), TestType);

		TestsArray.Add(MakeShared<FJsonValueObject>(TestObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("tests"), TestsArray);
	ResultObj->SetNumberField(TEXT("count"), TestsArray.Num());

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAutomationHandler::HandleRunTests(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	TArray<TSharedPtr<FJsonValue>> TestNames = GetOptionalArray(Params, TEXT("tests"));
	FString Filter = GetOptionalString(Params, TEXT("filter"), TEXT(""));

	if (TestNames.Num() == 0 && Filter.IsEmpty())
	{
		OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::InvalidParams, TEXT("Either 'tests' or 'filter' parameter is required"));
		return false;
	}

	// Get the automation controller
	IAutomationControllerModule& AutomationControllerModule = FModuleManager::LoadModuleChecked<IAutomationControllerModule>("AutomationController");
	IAutomationControllerManagerRef AutomationController = AutomationControllerModule.GetAutomationController();

	// Collect test names to run
	TArray<FString> TestsToRun;

	if (TestNames.Num() > 0)
	{
		for (const TSharedPtr<FJsonValue>& Value : TestNames)
		{
			TestsToRun.Add(Value->AsString());
		}
	}
	else
	{
		// Get tests matching filter
		TArray<FAutomationTestInfo> TestInfos;
		FAutomationTestFramework::Get().GetValidTestNames(TestInfos);

		for (const FAutomationTestInfo& TestInfo : TestInfos)
		{
			if (TestInfo.GetDisplayName().Contains(Filter) || TestInfo.GetTestName().Contains(Filter))
			{
				TestsToRun.Add(TestInfo.GetTestName());
			}
		}
	}

	if (TestsToRun.Num() == 0)
	{
		OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::NotFound, TEXT("No matching tests found"));
		return false;
	}

	// Queue tests
	AutomationController->SetEnabledTests(TestsToRun);
	AutomationController->RunTests();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetNumberField(TEXT("testsQueued"), TestsToRun.Num());

	TArray<TSharedPtr<FJsonValue>> QueuedArray;
	for (const FString& TestName : TestsToRun)
	{
		QueuedArray.Add(MakeShared<FJsonValueString>(TestName));
	}
	ResultObj->SetArrayField(TEXT("tests"), QueuedArray);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAutomationHandler::HandleGetTestResults(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	IAutomationControllerModule& AutomationControllerModule = FModuleManager::LoadModuleChecked<IAutomationControllerModule>("AutomationController");
	IAutomationControllerManagerRef AutomationController = AutomationControllerModule.GetAutomationController();

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	// Note: IsTestRunning() may have different signature in UE 5.6
	bool bIsRunning = false;
	// Check if tests are running by examining controller state
	// AutomationController->IsTestRunning() might need different approach
	ResultObj->SetBoolField(TEXT("isRunning"), bIsRunning);

	// Get report
	TArray<TSharedPtr<IAutomationReport>> Reports = AutomationController->GetReports();
	TArray<TSharedPtr<FJsonValue>> ReportsArray;

	for (const TSharedPtr<IAutomationReport>& Report : Reports)
	{
		if (Report.IsValid())
		{
			TSharedPtr<FJsonObject> ReportObj = MakeShared<FJsonObject>();
			ReportObj->SetStringField(TEXT("name"), Report->GetDisplayName());

			// UE 5.6+: GetState() requires (ClusterIndex, PassIndex); earlier versions only take ClusterIndex
			FString StateStr = TEXT("Unknown");
#if ULTIMATE_CONTROL_UE_5_6_OR_LATER
			EAutomationState State = Report->GetState(0, 0);
#else
			EAutomationState State = Report->GetState(0);
#endif
			switch (State)
			{
			case EAutomationState::NotRun: StateStr = TEXT("NotRun"); break;
			case EAutomationState::InProcess: StateStr = TEXT("InProcess"); break;
			case EAutomationState::Fail: StateStr = TEXT("Fail"); break;
			case EAutomationState::Success: StateStr = TEXT("Success"); break;
			default: break;
			}
			ReportObj->SetStringField(TEXT("state"), StateStr);

			ReportsArray.Add(MakeShared<FJsonValueObject>(ReportObj));
		}
	}
	ResultObj->SetArrayField(TEXT("reports"), ReportsArray);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAutomationHandler::HandleCook(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Platform = GetOptionalString(Params, TEXT("platform"), TEXT("WindowsNoEditor"));

	// Build the cook command
	FString UATPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build/BatchFiles/RunUAT.bat"));

#if PLATFORM_MAC
	UATPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build/BatchFiles/RunUAT.sh"));
#elif PLATFORM_LINUX
	UATPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build/BatchFiles/RunUAT.sh"));
#endif

	FString ProjectPath = FPaths::GetProjectFilePath();
	FString CommandLine = FString::Printf(TEXT("BuildCookRun -project=\"%s\" -cook -targetplatform=%s -nocompile"), *ProjectPath, *Platform);

	// Launch the process
	FProcHandle Handle = FPlatformProcess::CreateProc(*UATPath, *CommandLine, false, false, false, nullptr, 0, nullptr, nullptr);

	if (!Handle.IsValid())
	{
		OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::OperationFailed, TEXT("Failed to launch cook process"));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("platform"), Platform);
	ResultObj->SetStringField(TEXT("message"), TEXT("Cook process started in background"));

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAutomationHandler::HandlePackage(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Platform = GetOptionalString(Params, TEXT("platform"), TEXT("Win64"));
	FString Configuration = GetOptionalString(Params, TEXT("configuration"), TEXT("Development"));

	// Build the package command
	FString UATPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build/BatchFiles/RunUAT.bat"));

#if PLATFORM_MAC
	UATPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build/BatchFiles/RunUAT.sh"));
#elif PLATFORM_LINUX
	UATPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build/BatchFiles/RunUAT.sh"));
#endif

	FString ProjectPath = FPaths::GetProjectFilePath();
	FString CommandLine = FString::Printf(TEXT("BuildCookRun -project=\"%s\" -cook -stage -package -targetplatform=%s -clientconfig=%s"),
		*ProjectPath, *Platform, *Configuration);

	// Launch the process
	FProcHandle Handle = FPlatformProcess::CreateProc(*UATPath, *CommandLine, false, false, false, nullptr, 0, nullptr, nullptr);

	if (!Handle.IsValid())
	{
		OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::OperationFailed, TEXT("Failed to launch package process"));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("platform"), Platform);
	ResultObj->SetStringField(TEXT("configuration"), Configuration);
	ResultObj->SetStringField(TEXT("message"), TEXT("Package process started in background"));

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlAutomationHandler::HandleGetStatus(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	TSharedPtr<FJsonObject> StatusObj = MakeShared<FJsonObject>();

	// This would need to be implemented with actual build tracking
	// For now, return basic info
	StatusObj->SetBoolField(TEXT("isBuilding"), false);
	StatusObj->SetStringField(TEXT("lastBuildResult"), TEXT("Unknown"));

	OutResult = MakeShared<FJsonValueObject>(StatusObj);
	return true;
}

bool FUltimateControlAutomationHandler::HandleRunUAT(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Command;
	if (!RequireString(Params, TEXT("command"), Command, OutError))
	{
		return false;
	}

	FString UATPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build/BatchFiles/RunUAT.bat"));

#if PLATFORM_MAC
	UATPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build/BatchFiles/RunUAT.sh"));
#elif PLATFORM_LINUX
	UATPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build/BatchFiles/RunUAT.sh"));
#endif

	FString ProjectPath = FPaths::GetProjectFilePath();
	FString CommandLine = FString::Printf(TEXT("%s -project=\"%s\""), *Command, *ProjectPath);

	// Launch the process
	FProcHandle Handle = FPlatformProcess::CreateProc(*UATPath, *CommandLine, false, false, false, nullptr, 0, nullptr, nullptr);

	if (!Handle.IsValid())
	{
		OutError = UUltimateControlSubsystem::MakeError(EJsonRpcError::OperationFailed, TEXT("Failed to launch UAT process"));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("command"), Command);
	ResultObj->SetStringField(TEXT("message"), TEXT("UAT process started in background"));

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}
