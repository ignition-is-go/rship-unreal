// Rship Feedback Reporter
// In-plugin feedback and bug reporting with automatic context collection

#pragma once

#include "CoreMinimal.h"
#include "RshipFeedbackReporter.generated.h"

class URshipSubsystem;

// ============================================================================
// FEEDBACK TYPES
// ============================================================================

UENUM(BlueprintType)
enum class ERshipFeedbackType : uint8
{
    Bug         UMETA(DisplayName = "Bug Report"),
    Feature     UMETA(DisplayName = "Feature Request"),
    Feedback    UMETA(DisplayName = "General Feedback"),
    Question    UMETA(DisplayName = "Question"),
    Crash       UMETA(DisplayName = "Crash Report")
};

UENUM(BlueprintType)
enum class ERshipFeedbackSeverity : uint8
{
    Low         UMETA(DisplayName = "Low - Minor issue"),
    Medium      UMETA(DisplayName = "Medium - Affects workflow"),
    High        UMETA(DisplayName = "High - Blocking work"),
    Critical    UMETA(DisplayName = "Critical - Show stopper")
};

UENUM(BlueprintType)
enum class ERshipFeedbackCategory : uint8
{
    Connection      UMETA(DisplayName = "Connection/Network"),
    Performance     UMETA(DisplayName = "Performance"),
    UI              UMETA(DisplayName = "User Interface"),
    Calibration     UMETA(DisplayName = "Calibration/Accuracy"),
    Fixtures        UMETA(DisplayName = "Fixtures/Lights"),
    Cameras         UMETA(DisplayName = "Cameras"),
    SceneConversion UMETA(DisplayName = "Scene Conversion"),
    Sync            UMETA(DisplayName = "Synchronization"),
    Other           UMETA(DisplayName = "Other")
};

/**
 * Automatically collected system context
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipSystemContext
{
    GENERATED_BODY()

    // UE Info
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    FString UnrealVersion;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    FString PluginVersion;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    FString ProjectName;

    // Platform
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    FString Platform;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    FString OSVersion;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    FString CPUInfo;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    FString GPUInfo;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    int32 RAMInGB = 0;

    // Connection state
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    bool bIsConnected = false;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    FString ServerAddress;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    FString ClientId;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    FString ServiceId;

    // Session info
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    float SessionDurationSeconds = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    int32 ReconnectCount = 0;

    // Plugin state
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    int32 FixtureCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    int32 CameraCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    int32 TargetCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    float PulsesPerSecond = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    int32 QueueLength = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    bool bRateLimiterBackingOff = false;

    // Timestamp
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    FDateTime Timestamp;
};

/**
 * A feedback report ready for submission
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipFeedbackReport
{
    GENERATED_BODY()

    // User-provided content
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Feedback")
    FString Title;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Feedback")
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Feedback")
    FString StepsToReproduce;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Feedback")
    FString ExpectedBehavior;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Feedback")
    FString ActualBehavior;

    // Classification
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Feedback")
    ERshipFeedbackType Type = ERshipFeedbackType::Bug;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Feedback")
    ERshipFeedbackSeverity Severity = ERshipFeedbackSeverity::Medium;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Feedback")
    ERshipFeedbackCategory Category = ERshipFeedbackCategory::Other;

    // Contact (optional)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Feedback")
    FString ReporterEmail;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Feedback")
    FString ReporterName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Feedback")
    bool bAllowContact = true;

    // Auto-collected context
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    FRshipSystemContext SystemContext;

    // Attachments
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    TArray<FString> RecentLogLines;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    FString ScreenshotPath;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    bool bHasScreenshot = false;

    // Submission metadata
    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    FString ReportId;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    FDateTime SubmittedAt;
};

/**
 * Result of a feedback submission
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipFeedbackResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    bool bSuccess = false;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    FString ReportId;

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    FString IssueUrl;  // URL to the created issue (if available)

    UPROPERTY(BlueprintReadOnly, Category = "Rship|Feedback")
    FString ErrorMessage;
};

// ============================================================================
// DELEGATES
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFeedbackSubmitted, const FRshipFeedbackResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRshipScreenshotCaptured, const FString&, ScreenshotPath);

// ============================================================================
// FEEDBACK REPORTER SERVICE
// ============================================================================

/**
 * Handles feedback collection, context gathering, and submission to rship.
 * Integrates with rship server which routes to the issue tracking system.
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipFeedbackReporter : public UObject
{
    GENERATED_BODY()

public:
    /** Initialize with subsystem reference */
    void Initialize(URshipSubsystem* InSubsystem);

    /** Shutdown and cleanup */
    void Shutdown();

    // ========================================================================
    // REPORT CREATION
    // ========================================================================

    /**
     * Create a new feedback report with auto-collected context
     * Returns a report struct that can be modified before submission
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Feedback")
    FRshipFeedbackReport CreateReport(ERshipFeedbackType Type = ERshipFeedbackType::Bug);

    /**
     * Collect current system context
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Feedback")
    FRshipSystemContext CollectSystemContext() const;

    /**
     * Collect recent log lines (from LogRshipExec)
     * @param MaxLines Maximum lines to collect
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Feedback")
    TArray<FString> CollectRecentLogs(int32 MaxLines = 100) const;

    // ========================================================================
    // SCREENSHOTS
    // ========================================================================

    /**
     * Capture a screenshot for the feedback report
     * @param Report The report to attach the screenshot to
     * @return Whether capture was initiated (async)
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Feedback")
    bool CaptureScreenshot(UPARAM(ref) FRshipFeedbackReport& Report);

    /**
     * Capture viewport screenshot and save to temp file
     * @return Path to the saved screenshot
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Feedback")
    FString CaptureViewportScreenshot();

    // ========================================================================
    // SUBMISSION
    // ========================================================================

    /**
     * Submit a feedback report to rship
     * Report is sent to the server which routes to the issue system
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Feedback")
    void SubmitReport(const FRshipFeedbackReport& Report);

    /**
     * Quick submit - creates and submits a report in one call
     * Useful for programmatic bug reporting (e.g., after a crash)
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Feedback")
    void QuickSubmit(
        ERshipFeedbackType Type,
        const FString& Title,
        const FString& Description,
        ERshipFeedbackSeverity Severity = ERshipFeedbackSeverity::Medium
    );

    // ========================================================================
    // LOCAL STORAGE
    // ========================================================================

    /**
     * Save a report locally (for offline submission later)
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Feedback")
    bool SaveReportLocally(const FRshipFeedbackReport& Report);

    /**
     * Get pending local reports (not yet submitted)
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Feedback")
    TArray<FRshipFeedbackReport> GetPendingLocalReports() const;

    /**
     * Submit all pending local reports
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Feedback")
    void SubmitPendingReports();

    /**
     * Clear pending local reports
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Feedback")
    void ClearPendingReports();

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * Set default reporter contact info (persisted in config)
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Feedback")
    void SetDefaultReporterInfo(const FString& Email, const FString& Name);

    /**
     * Get default reporter email
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Feedback")
    FString GetDefaultReporterEmail() const;

    /**
     * Get default reporter name
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|Feedback")
    FString GetDefaultReporterName() const;

    // ========================================================================
    // EVENTS
    // ========================================================================

    /** Fired when a report is submitted (success or failure) */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Feedback")
    FOnFeedbackSubmitted OnFeedbackSubmitted;

    /** Fired when a screenshot is captured */
    UPROPERTY(BlueprintAssignable, Category = "Rship|Feedback")
    FOnRshipScreenshotCaptured OnScreenshotCaptured;

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    // Session tracking
    double SessionStartTime = 0.0;
    int32 ReconnectCounter = 0;

    // Cached reporter info
    FString CachedReporterEmail;
    FString CachedReporterName;

    // Log buffer for recent messages
    TArray<FString> LogBuffer;
    int32 MaxLogBufferSize = 500;
    FDelegateHandle LogDelegateHandle;

    // Pending reports directory
    FString GetPendingReportsPath() const;

    // Generate unique report ID
    FString GenerateReportId() const;

    // Convert report to JSON for submission
    TSharedPtr<FJsonObject> ReportToJson(const FRshipFeedbackReport& Report) const;

    // Load reporter info from config
    void LoadReporterConfig();

    // Save reporter info to config
    void SaveReporterConfig();

    // Handle log output (buffer recent lines)
    void OnLogMessage(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category);

    // Bind to log output
    void BindLogCapture();

    // Unbind from log output
    void UnbindLogCapture();
};
