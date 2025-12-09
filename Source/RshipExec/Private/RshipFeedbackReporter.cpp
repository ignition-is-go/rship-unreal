// Rship Feedback Reporter Implementation

#include "RshipFeedbackReporter.h"
#include "RshipSubsystem.h"
#include "RshipFixtureManager.h"
#include "RshipCameraManager.h"
#include "RshipPulseReceiver.h"
#include "RshipSettings.h"
#include "Logs.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "RHI.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ImageUtils.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Slate/SceneViewport.h"
#include "Framework/Application/SlateApplication.h"

#if WITH_EDITOR
#include "Editor.h"
#include "LevelEditor.h"
#endif

// Plugin version - update this with releases
#define RSHIP_PLUGIN_VERSION TEXT("1.0.0")

void URshipFeedbackReporter::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;
    SessionStartTime = FPlatformTime::Seconds();

    // Load cached reporter info
    LoadReporterConfig();

    // Start capturing logs
    BindLogCapture();

    UE_LOG(LogRshipExec, Log, TEXT("FeedbackReporter initialized"));
}

void URshipFeedbackReporter::Shutdown()
{
    UnbindLogCapture();
    LogBuffer.Empty();
    Subsystem = nullptr;

    UE_LOG(LogRshipExec, Log, TEXT("FeedbackReporter shutdown"));
}

// ============================================================================
// LOG CAPTURE
// ============================================================================

void URshipFeedbackReporter::BindLogCapture()
{
    // Use output device to capture logs
    // We'll capture all LogRshipExec messages
    LogDelegateHandle = FOutputDeviceRedirector::Get()->OnLogMessage().AddUObject(
        this, &URshipFeedbackReporter::OnLogMessage);
}

void URshipFeedbackReporter::UnbindLogCapture()
{
    if (LogDelegateHandle.IsValid())
    {
        FOutputDeviceRedirector::Get()->OnLogMessage().Remove(LogDelegateHandle);
        LogDelegateHandle.Reset();
    }
}

void URshipFeedbackReporter::OnLogMessage(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category)
{
    // Only capture rship-related logs
    if (Category == TEXT("LogRshipExec") || Category.ToString().Contains(TEXT("Rship")))
    {
        FString LogLine = FString::Printf(TEXT("[%s] %s: %s"),
            *FDateTime::Now().ToString(),
            *Category.ToString(),
            Message);

        LogBuffer.Add(LogLine);

        // Trim buffer if too large
        while (LogBuffer.Num() > MaxLogBufferSize)
        {
            LogBuffer.RemoveAt(0);
        }
    }
}

// ============================================================================
// CONTEXT COLLECTION
// ============================================================================

FRshipSystemContext URshipFeedbackReporter::CollectSystemContext() const
{
    FRshipSystemContext Context;

    // UE Info
    Context.UnrealVersion = FEngineVersion::Current().ToString();
    Context.PluginVersion = RSHIP_PLUGIN_VERSION;
    Context.ProjectName = FApp::GetProjectName();

    // Platform
    Context.Platform = FPlatformProperties::IniPlatformName();
    Context.OSVersion = FPlatformMisc::GetOSVersion();
    Context.CPUInfo = FPlatformMisc::GetCPUBrand();

    // GPU
    Context.GPUInfo = GRHIAdapterName;

    // RAM
    FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
    Context.RAMInGB = (int32)(MemStats.TotalPhysical / (1024 * 1024 * 1024));

    // Connection state
    if (Subsystem)
    {
        Context.bIsConnected = Subsystem->IsConnected();
        Context.ServiceId = Subsystem->GetServiceId();

        const URshipSettings* Settings = GetDefault<URshipSettings>();
        if (Settings)
        {
            Context.ServerAddress = FString::Printf(TEXT("%s:%d"),
                *Settings->rshipHostAddress, Settings->rshipServerPort);
        }

        // Plugin state
        URshipFixtureManager* FixtureManager = const_cast<URshipSubsystem*>(Subsystem)->GetFixtureManager();
        if (FixtureManager)
        {
            Context.FixtureCount = FixtureManager->GetFixtureCount();
        }

        URshipCameraManager* CameraManager = const_cast<URshipSubsystem*>(Subsystem)->GetCameraManager();
        if (CameraManager)
        {
            Context.CameraCount = CameraManager->GetCameraCount();
        }

        URshipPulseReceiver* PulseReceiver = const_cast<URshipSubsystem*>(Subsystem)->GetPulseReceiver();
        if (PulseReceiver)
        {
            Context.PulsesPerSecond = PulseReceiver->GetTotalPulseRate();
        }

        Context.QueueLength = Subsystem->GetQueueLength();
        Context.bRateLimiterBackingOff = Subsystem->IsRateLimiterBackingOff();

        if (Subsystem->TargetComponents)
        {
            Context.TargetCount = Subsystem->TargetComponents->Num();
        }
    }

    // Session info
    Context.SessionDurationSeconds = (float)(FPlatformTime::Seconds() - SessionStartTime);
    Context.ReconnectCount = ReconnectCounter;

    // Timestamp
    Context.Timestamp = FDateTime::UtcNow();

    return Context;
}

TArray<FString> URshipFeedbackReporter::CollectRecentLogs(int32 MaxLines) const
{
    TArray<FString> Result;

    int32 StartIdx = FMath::Max(0, LogBuffer.Num() - MaxLines);
    for (int32 i = StartIdx; i < LogBuffer.Num(); i++)
    {
        Result.Add(LogBuffer[i]);
    }

    return Result;
}

// ============================================================================
// REPORT CREATION
// ============================================================================

FRshipFeedbackReport URshipFeedbackReporter::CreateReport(ERshipFeedbackType Type)
{
    FRshipFeedbackReport Report;

    Report.Type = Type;
    Report.ReportId = GenerateReportId();
    Report.SystemContext = CollectSystemContext();
    Report.RecentLogLines = CollectRecentLogs(100);

    // Set default reporter info
    Report.ReporterEmail = CachedReporterEmail;
    Report.ReporterName = CachedReporterName;

    // Set defaults based on type
    switch (Type)
    {
    case ERshipFeedbackType::Bug:
        Report.Title = TEXT("Bug: ");
        break;
    case ERshipFeedbackType::Feature:
        Report.Title = TEXT("Feature Request: ");
        Report.Severity = ERshipFeedbackSeverity::Low;
        break;
    case ERshipFeedbackType::Crash:
        Report.Title = TEXT("Crash Report: ");
        Report.Severity = ERshipFeedbackSeverity::Critical;
        break;
    default:
        break;
    }

    return Report;
}

FString URshipFeedbackReporter::GenerateReportId() const
{
    return FString::Printf(TEXT("rship-ue-%s-%s"),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S")),
        *FGuid::NewGuid().ToString(EGuidFormats::Short));
}

// ============================================================================
// SCREENSHOTS
// ============================================================================

bool URshipFeedbackReporter::CaptureScreenshot(FRshipFeedbackReport& Report)
{
    FString ScreenshotPath = CaptureViewportScreenshot();

    if (!ScreenshotPath.IsEmpty())
    {
        Report.ScreenshotPath = ScreenshotPath;
        Report.bHasScreenshot = true;
        OnScreenshotCaptured.Broadcast(ScreenshotPath);
        return true;
    }

    return false;
}

FString URshipFeedbackReporter::CaptureViewportScreenshot()
{
    FString ScreenshotPath;

#if WITH_EDITOR
    // In editor, capture the active viewport
    if (GEditor)
    {
        FString ScreenshotDir = FPaths::ProjectSavedDir() / TEXT("RshipFeedback/Screenshots");
        IFileManager::Get().MakeDirectory(*ScreenshotDir, true);

        FString Filename = FString::Printf(TEXT("screenshot_%s.png"),
            *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
        ScreenshotPath = ScreenshotDir / Filename;

        // Request screenshot
        FScreenshotRequest::RequestScreenshot(ScreenshotPath, false, false);

        UE_LOG(LogRshipExec, Log, TEXT("FeedbackReporter: Screenshot requested at %s"), *ScreenshotPath);
    }
#else
    // In game, capture game viewport
    if (GEngine && GEngine->GameViewport)
    {
        FString ScreenshotDir = FPaths::ProjectSavedDir() / TEXT("RshipFeedback/Screenshots");
        IFileManager::Get().MakeDirectory(*ScreenshotDir, true);

        FString Filename = FString::Printf(TEXT("screenshot_%s.png"),
            *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
        ScreenshotPath = ScreenshotDir / Filename;

        FScreenshotRequest::RequestScreenshot(ScreenshotPath, false, false);
    }
#endif

    return ScreenshotPath;
}

// ============================================================================
// SUBMISSION
// ============================================================================

void URshipFeedbackReporter::SubmitReport(const FRshipFeedbackReport& Report)
{
    if (!Subsystem)
    {
        // Save locally if not connected
        SaveReportLocally(Report);

        FRshipFeedbackResult Result;
        Result.bSuccess = false;
        Result.ErrorMessage = TEXT("Not connected to rship server. Report saved locally.");
        OnFeedbackSubmitted.Broadcast(Result);
        return;
    }

    if (!Subsystem->IsConnected())
    {
        // Save locally if not connected
        SaveReportLocally(Report);

        FRshipFeedbackResult Result;
        Result.bSuccess = false;
        Result.ErrorMessage = TEXT("Not connected to rship server. Report saved locally for later submission.");
        OnFeedbackSubmitted.Broadcast(Result);
        return;
    }

    // Convert to JSON and send
    TSharedPtr<FJsonObject> ReportJson = ReportToJson(Report);

    // Send as a special feedback entity
    TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject);
    Payload->SetStringField(TEXT("event"), TEXT("ws:m:feedback"));
    Payload->SetObjectField(TEXT("data"), ReportJson);

    // Send via subsystem
    Subsystem->SendJson(Payload);

    UE_LOG(LogRshipExec, Log, TEXT("FeedbackReporter: Submitted report %s"), *Report.ReportId);

    // Return success (actual confirmation would come async from server)
    FRshipFeedbackResult Result;
    Result.bSuccess = true;
    Result.ReportId = Report.ReportId;
    OnFeedbackSubmitted.Broadcast(Result);

    // Cache reporter info for next time
    if (!Report.ReporterEmail.IsEmpty())
    {
        CachedReporterEmail = Report.ReporterEmail;
        CachedReporterName = Report.ReporterName;
        SaveReporterConfig();
    }
}

void URshipFeedbackReporter::QuickSubmit(
    ERshipFeedbackType Type,
    const FString& Title,
    const FString& Description,
    ERshipFeedbackSeverity Severity)
{
    FRshipFeedbackReport Report = CreateReport(Type);
    Report.Title = Title;
    Report.Description = Description;
    Report.Severity = Severity;

    SubmitReport(Report);
}

TSharedPtr<FJsonObject> URshipFeedbackReporter::ReportToJson(const FRshipFeedbackReport& Report) const
{
    TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject);

    // Report metadata
    Json->SetStringField(TEXT("reportId"), Report.ReportId);
    Json->SetStringField(TEXT("type"), UEnum::GetValueAsString(Report.Type));
    Json->SetStringField(TEXT("severity"), UEnum::GetValueAsString(Report.Severity));
    Json->SetStringField(TEXT("category"), UEnum::GetValueAsString(Report.Category));
    Json->SetStringField(TEXT("submittedAt"), Report.SubmittedAt.IsSet() ?
        Report.SubmittedAt.GetValue().ToIso8601() : FDateTime::UtcNow().ToIso8601());

    // User content
    Json->SetStringField(TEXT("title"), Report.Title);
    Json->SetStringField(TEXT("description"), Report.Description);
    Json->SetStringField(TEXT("stepsToReproduce"), Report.StepsToReproduce);
    Json->SetStringField(TEXT("expectedBehavior"), Report.ExpectedBehavior);
    Json->SetStringField(TEXT("actualBehavior"), Report.ActualBehavior);

    // Reporter info
    Json->SetStringField(TEXT("reporterEmail"), Report.ReporterEmail);
    Json->SetStringField(TEXT("reporterName"), Report.ReporterName);
    Json->SetBoolField(TEXT("allowContact"), Report.bAllowContact);

    // System context
    TSharedPtr<FJsonObject> ContextJson = MakeShareable(new FJsonObject);
    ContextJson->SetStringField(TEXT("unrealVersion"), Report.SystemContext.UnrealVersion);
    ContextJson->SetStringField(TEXT("pluginVersion"), Report.SystemContext.PluginVersion);
    ContextJson->SetStringField(TEXT("projectName"), Report.SystemContext.ProjectName);
    ContextJson->SetStringField(TEXT("platform"), Report.SystemContext.Platform);
    ContextJson->SetStringField(TEXT("osVersion"), Report.SystemContext.OSVersion);
    ContextJson->SetStringField(TEXT("cpuInfo"), Report.SystemContext.CPUInfo);
    ContextJson->SetStringField(TEXT("gpuInfo"), Report.SystemContext.GPUInfo);
    ContextJson->SetNumberField(TEXT("ramInGB"), Report.SystemContext.RAMInGB);
    ContextJson->SetBoolField(TEXT("isConnected"), Report.SystemContext.bIsConnected);
    ContextJson->SetStringField(TEXT("serverAddress"), Report.SystemContext.ServerAddress);
    ContextJson->SetStringField(TEXT("clientId"), Report.SystemContext.ClientId);
    ContextJson->SetStringField(TEXT("serviceId"), Report.SystemContext.ServiceId);
    ContextJson->SetNumberField(TEXT("sessionDurationSeconds"), Report.SystemContext.SessionDurationSeconds);
    ContextJson->SetNumberField(TEXT("reconnectCount"), Report.SystemContext.ReconnectCount);
    ContextJson->SetNumberField(TEXT("fixtureCount"), Report.SystemContext.FixtureCount);
    ContextJson->SetNumberField(TEXT("cameraCount"), Report.SystemContext.CameraCount);
    ContextJson->SetNumberField(TEXT("targetCount"), Report.SystemContext.TargetCount);
    ContextJson->SetNumberField(TEXT("pulsesPerSecond"), Report.SystemContext.PulsesPerSecond);
    ContextJson->SetNumberField(TEXT("queueLength"), Report.SystemContext.QueueLength);
    ContextJson->SetBoolField(TEXT("rateLimiterBackingOff"), Report.SystemContext.bRateLimiterBackingOff);
    ContextJson->SetStringField(TEXT("timestamp"), Report.SystemContext.Timestamp.ToIso8601());
    Json->SetObjectField(TEXT("systemContext"), ContextJson);

    // Recent logs
    TArray<TSharedPtr<FJsonValue>> LogsJson;
    for (const FString& Line : Report.RecentLogLines)
    {
        LogsJson.Add(MakeShareable(new FJsonValueString(Line)));
    }
    Json->SetArrayField(TEXT("recentLogs"), LogsJson);

    // Screenshot info
    Json->SetBoolField(TEXT("hasScreenshot"), Report.bHasScreenshot);
    if (Report.bHasScreenshot)
    {
        Json->SetStringField(TEXT("screenshotPath"), Report.ScreenshotPath);
        // TODO: Could base64 encode and include small screenshot
    }

    return Json;
}

// ============================================================================
// LOCAL STORAGE
// ============================================================================

FString URshipFeedbackReporter::GetPendingReportsPath() const
{
    return FPaths::ProjectSavedDir() / TEXT("RshipFeedback/Pending");
}

bool URshipFeedbackReporter::SaveReportLocally(const FRshipFeedbackReport& Report)
{
    FString PendingDir = GetPendingReportsPath();
    IFileManager::Get().MakeDirectory(*PendingDir, true);

    FString Filename = FString::Printf(TEXT("%s.json"), *Report.ReportId);
    FString FilePath = PendingDir / Filename;

    TSharedPtr<FJsonObject> Json = ReportToJson(Report);

    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    if (FJsonSerializer::Serialize(Json.ToSharedRef(), Writer))
    {
        if (FFileHelper::SaveStringToFile(JsonString, *FilePath))
        {
            UE_LOG(LogRshipExec, Log, TEXT("FeedbackReporter: Saved report locally: %s"), *FilePath);
            return true;
        }
    }

    UE_LOG(LogRshipExec, Warning, TEXT("FeedbackReporter: Failed to save report locally"));
    return false;
}

TArray<FRshipFeedbackReport> URshipFeedbackReporter::GetPendingLocalReports() const
{
    TArray<FRshipFeedbackReport> Reports;

    FString PendingDir = GetPendingReportsPath();
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *(PendingDir / TEXT("*.json")), true, false);

    for (const FString& File : Files)
    {
        FString FilePath = PendingDir / File;
        FString JsonString;

        if (FFileHelper::LoadFileToString(JsonString, *FilePath))
        {
            // Parse JSON and reconstruct report
            // (Simplified - in production would fully deserialize)
            FRshipFeedbackReport Report;
            Report.ReportId = FPaths::GetBaseFilename(File);
            Reports.Add(Report);
        }
    }

    return Reports;
}

void URshipFeedbackReporter::SubmitPendingReports()
{
    if (!Subsystem || !Subsystem->IsConnected())
    {
        UE_LOG(LogRshipExec, Warning, TEXT("FeedbackReporter: Cannot submit pending reports - not connected"));
        return;
    }

    FString PendingDir = GetPendingReportsPath();
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *(PendingDir / TEXT("*.json")), true, false);

    for (const FString& File : Files)
    {
        FString FilePath = PendingDir / File;
        FString JsonString;

        if (FFileHelper::LoadFileToString(JsonString, *FilePath))
        {
            // Parse and send
            TSharedPtr<FJsonObject> Json;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

            if (FJsonSerializer::Deserialize(Reader, Json))
            {
                TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject);
                Payload->SetStringField(TEXT("event"), TEXT("ws:m:feedback"));
                Payload->SetObjectField(TEXT("data"), Json);

                Subsystem->SendJson(Payload);

                // Delete the local file
                IFileManager::Get().Delete(*FilePath);

                UE_LOG(LogRshipExec, Log, TEXT("FeedbackReporter: Submitted pending report: %s"), *File);
            }
        }
    }
}

void URshipFeedbackReporter::ClearPendingReports()
{
    FString PendingDir = GetPendingReportsPath();
    IFileManager::Get().DeleteDirectory(*PendingDir, false, true);

    UE_LOG(LogRshipExec, Log, TEXT("FeedbackReporter: Cleared pending reports"));
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void URshipFeedbackReporter::SetDefaultReporterInfo(const FString& Email, const FString& Name)
{
    CachedReporterEmail = Email;
    CachedReporterName = Name;
    SaveReporterConfig();
}

FString URshipFeedbackReporter::GetDefaultReporterEmail() const
{
    return CachedReporterEmail;
}

FString URshipFeedbackReporter::GetDefaultReporterName() const
{
    return CachedReporterName;
}

void URshipFeedbackReporter::LoadReporterConfig()
{
    FString ConfigPath = FPaths::ProjectSavedDir() / TEXT("RshipFeedback/reporter.ini");

    if (FPaths::FileExists(ConfigPath))
    {
        FString ConfigContent;
        if (FFileHelper::LoadFileToString(ConfigContent, *ConfigPath))
        {
            TArray<FString> Lines;
            ConfigContent.ParseIntoArrayLines(Lines);

            for (const FString& Line : Lines)
            {
                FString Key, Value;
                if (Line.Split(TEXT("="), &Key, &Value))
                {
                    Key = Key.TrimStartAndEnd();
                    Value = Value.TrimStartAndEnd();

                    if (Key == TEXT("Email"))
                    {
                        CachedReporterEmail = Value;
                    }
                    else if (Key == TEXT("Name"))
                    {
                        CachedReporterName = Value;
                    }
                }
            }
        }
    }
}

void URshipFeedbackReporter::SaveReporterConfig()
{
    FString ConfigDir = FPaths::ProjectSavedDir() / TEXT("RshipFeedback");
    IFileManager::Get().MakeDirectory(*ConfigDir, true);

    FString ConfigPath = ConfigDir / TEXT("reporter.ini");
    FString ConfigContent = FString::Printf(TEXT("Email=%s\nName=%s"),
        *CachedReporterEmail, *CachedReporterName);

    FFileHelper::SaveStringToFile(ConfigContent, *ConfigPath);
}
