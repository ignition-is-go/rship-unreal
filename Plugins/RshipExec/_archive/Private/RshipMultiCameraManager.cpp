// Rship Multi-Camera Manager Implementation

#include "RshipMultiCameraManager.h"
#include "RshipSubsystem.h"
#include "Logs.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void URshipMultiCameraManager::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;
    DefaultTransition.Type = ERshipTransitionType::Dissolve;
    DefaultTransition.Duration = 0.5f;
    UE_LOG(LogRshipExec, Log, TEXT("MultiCameraManager initialized"));
}

void URshipMultiCameraManager::Shutdown()
{
    StopRecording();
    Views.Empty();
    Presets.Empty();
    AutoSwitchRules.Empty();
    Subsystem = nullptr;
}

void URshipMultiCameraManager::Tick(float DeltaTime)
{
    if (bIsTransitioning) UpdateTransition(DeltaTime);
    if (bAutoSwitchEnabled && !bIsTransitioning) EvaluateAutoSwitchRules();
    if (CurrentRecording.bIsRecording) CurrentRecording.DurationSeconds += DeltaTime;
}

void URshipMultiCameraManager::AddView(const FRshipCameraView& View)
{
    FRshipCameraView V = View;
    if (V.Id.IsEmpty()) V.Id = FGuid::NewGuid().ToString();
    Views.Add(V.Id, V);
}

void URshipMultiCameraManager::RemoveView(const FString& ViewId) { Views.Remove(ViewId); }
TArray<FRshipCameraView> URshipMultiCameraManager::GetAllViews() const { TArray<FRshipCameraView> R; Views.GenerateValueArray(R); return R; }
bool URshipMultiCameraManager::GetView(const FString& ViewId, FRshipCameraView& OutView) const { const auto* V = Views.Find(ViewId); if (V) { OutView = *V; return true; } return false; }
void URshipMultiCameraManager::UpdateView(const FRshipCameraView& View) { if (Views.Contains(View.Id)) Views[View.Id] = View; }

void URshipMultiCameraManager::SwitchToView(const FString& ViewId)
{
    FRshipCameraTransition Cut; Cut.Type = ERshipTransitionType::Cut; Cut.Duration = 0.0f;
    SwitchWithTransition(ViewId, Cut);
}

void URshipMultiCameraManager::SwitchWithTransition(const FString& ViewId, const FRshipCameraTransition& Transition)
{
    FRshipCameraView* Target = Views.Find(ViewId);
    if (!Target) return;

    if (Transition.Type == ERshipTransitionType::Cut || Transition.Duration <= 0.0f)
    {
        FString OldId = ProgramView.Id;
        ProgramView = *Target;
        UpdateTallyStates();
        if (ProgramView.Camera)
        {
            APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
            if (PC) PC->SetViewTargetWithBlend(ProgramView.Camera, 0.0f);
        }
        OnCameraSwitched.Broadcast(OldId, ViewId);
        return;
    }

    bIsTransitioning = true;
    ActiveTransition = Transition;
    TransitionProgress = 0.0f;
    TransitionFromViewId = ProgramView.Id;
    TransitionToViewId = ViewId;
    OnTransitionStarted.Broadcast(ActiveTransition);
}

void URshipMultiCameraManager::SetPreviewView(const FString& ViewId)
{
    FRshipCameraView* V = Views.Find(ViewId);
    if (V) { PreviewView = *V; UpdateTallyStates(); }
}

void URshipMultiCameraManager::Take(const FRshipCameraTransition& Transition) { if (!PreviewView.Id.IsEmpty()) SwitchWithTransition(PreviewView.Id, Transition); }
void URshipMultiCameraManager::Cut() { if (!PreviewView.Id.IsEmpty()) SwitchToView(PreviewView.Id); }
void URshipMultiCameraManager::Auto() { Take(DefaultTransition); }
void URshipMultiCameraManager::FadeToBlack(float Duration) { UE_LOG(LogRshipExec, Log, TEXT("Fade to black: %.2fs"), Duration); }
void URshipMultiCameraManager::FadeFromBlack(float Duration) { UE_LOG(LogRshipExec, Log, TEXT("Fade from black: %.2fs"), Duration); }

void URshipMultiCameraManager::AddPreset(const FRshipCameraPreset& Preset)
{
    FRshipCameraPreset P = Preset;
    if (P.Id.IsEmpty()) P.Id = FGuid::NewGuid().ToString();
    Presets.Add(P);
}

void URshipMultiCameraManager::RemovePreset(const FString& PresetId) { Presets.RemoveAll([&](const FRshipCameraPreset& P) { return P.Id == PresetId; }); }

void URshipMultiCameraManager::ExecutePreset(const FString& PresetId)
{
    for (const auto& P : Presets) if (P.Id == PresetId) { SwitchWithTransition(P.ViewId, P.Transition); return; }
}

void URshipMultiCameraManager::SetAutoSwitchEnabled(bool bEnabled) { bAutoSwitchEnabled = bEnabled; }
void URshipMultiCameraManager::AddAutoSwitchRule(const FRshipAutoSwitchRule& Rule)
{
    FRshipAutoSwitchRule R = Rule;
    if (R.Id.IsEmpty()) R.Id = FGuid::NewGuid().ToString();
    AutoSwitchRules.Add(R);
    AutoSwitchRules.Sort([](const FRshipAutoSwitchRule& A, const FRshipAutoSwitchRule& B) { return A.Priority > B.Priority; });
}
void URshipMultiCameraManager::RemoveAutoSwitchRule(const FString& RuleId) { AutoSwitchRules.RemoveAll([&](const FRshipAutoSwitchRule& R) { return R.Id == RuleId; }); }

void URshipMultiCameraManager::EvaluateAutoSwitchRules()
{
    if (AutoSwitchRules.Num() == 0) return;

    double CurrentTime = FPlatformTime::Seconds();

    // Track timing state per view for time-based rules
    static TMap<FString, double> ViewSwitchTimes;
    static TMap<FString, double> RandomNextTimes;

    // Evaluate rules in priority order (already sorted)
    for (const FRshipAutoSwitchRule& Rule : AutoSwitchRules)
    {
        if (!Rule.bEnabled) continue;

        // Skip if already on target view
        if (ProgramView.Id == Rule.TargetViewId) continue;

        // Check target view exists
        if (!Views.Contains(Rule.TargetViewId)) continue;

        bool bShouldTrigger = false;

        // Evaluate trigger based on type
        if (Rule.TriggerType == TEXT("TimeBased"))
        {
            // Time-based auto-switch: switch after N seconds on current view
            // TriggerParams expected: {"intervalSeconds": 10.0}
            TSharedPtr<FJsonObject> Params;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Rule.TriggerParams);
            if (FJsonSerializer::Deserialize(Reader, Params) && Params.IsValid())
            {
                double Interval = 0.0;
                Params->TryGetNumberField(TEXT("intervalSeconds"), Interval);

                double* LastSwitch = ViewSwitchTimes.Find(ProgramView.Id);
                if (!LastSwitch)
                {
                    ViewSwitchTimes.Add(ProgramView.Id, CurrentTime);
                }
                else if (Interval > 0.0 && (CurrentTime - *LastSwitch) >= Interval)
                {
                    bShouldTrigger = true;
                }
            }
        }
        else if (Rule.TriggerType == TEXT("Random"))
        {
            // Random switching within a time window
            // TriggerParams expected: {"minSeconds": 5.0, "maxSeconds": 15.0}
            TSharedPtr<FJsonObject> Params;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Rule.TriggerParams);
            if (FJsonSerializer::Deserialize(Reader, Params) && Params.IsValid())
            {
                double MinSeconds = 5.0, MaxSeconds = 15.0;
                Params->TryGetNumberField(TEXT("minSeconds"), MinSeconds);
                Params->TryGetNumberField(TEXT("maxSeconds"), MaxSeconds);

                double* NextSwitch = RandomNextTimes.Find(Rule.Id);
                if (!NextSwitch)
                {
                    double RandomDelay = FMath::FRandRange(MinSeconds, MaxSeconds);
                    RandomNextTimes.Add(Rule.Id, CurrentTime + RandomDelay);
                }
                else if (CurrentTime >= *NextSwitch)
                {
                    bShouldTrigger = true;
                    double RandomDelay = FMath::FRandRange(MinSeconds, MaxSeconds);
                    RandomNextTimes[Rule.Id] = CurrentTime + RandomDelay;
                }
            }
        }
        // Additional trigger types can be added: "EmitterValue", "CuePoint", etc.

        if (bShouldTrigger)
        {
            UE_LOG(LogRshipExec, Log, TEXT("Auto-switch rule '%s' triggered, switching to view '%s'"),
                *Rule.Name, *Rule.TargetViewId);
            SwitchWithTransition(Rule.TargetViewId, Rule.Transition);

            // Update timing for view we just switched to
            ViewSwitchTimes.Add(Rule.TargetViewId, CurrentTime);
            break;  // Only trigger one rule per evaluation
        }
    }
}

void URshipMultiCameraManager::SetTallyState(const FString& ViewId, ERshipCameraTallyState State)
{
    FRshipCameraView* V = Views.Find(ViewId);
    if (V && V->TallyState != State) { V->TallyState = State; OnTallyStateChanged.Broadcast(ViewId, State); if (bTallyOutputEnabled) SendTallyToRship(ViewId, State); }
}

ERshipCameraTallyState URshipMultiCameraManager::GetTallyState(const FString& ViewId) const { const auto* V = Views.Find(ViewId); return V ? V->TallyState : ERshipCameraTallyState::Off; }
void URshipMultiCameraManager::SetTallyOutputEnabled(bool bEnabled) { bTallyOutputEnabled = bEnabled; }

void URshipMultiCameraManager::UpdateTallyStates()
{
    for (auto& P : Views) SetTallyState(P.Key, ERshipCameraTallyState::Off);
    if (!PreviewView.Id.IsEmpty()) SetTallyState(PreviewView.Id, ERshipCameraTallyState::Preview);
    if (!ProgramView.Id.IsEmpty()) SetTallyState(ProgramView.Id, ERshipCameraTallyState::Program);
}

void URshipMultiCameraManager::SendTallyToRship(const FString& ViewId, ERshipCameraTallyState State)
{
    if (!Subsystem) return;

    // Get the view to find the rship camera ID
    const FRshipCameraView* View = Views.Find(ViewId);
    if (!View) return;

    // Build tally state JSON
    TSharedPtr<FJsonObject> TallyData = MakeShareable(new FJsonObject);
    TallyData->SetStringField(TEXT("viewId"), ViewId);
    TallyData->SetStringField(TEXT("rshipCameraId"), View->RshipCameraId);

    // Convert tally state to string
    FString StateString;
    switch (State)
    {
        case ERshipCameraTallyState::Off: StateString = TEXT("off"); break;
        case ERshipCameraTallyState::Preview: StateString = TEXT("preview"); break;
        case ERshipCameraTallyState::Program: StateString = TEXT("program"); break;
        case ERshipCameraTallyState::Recording: StateString = TEXT("recording"); break;
    }
    TallyData->SetStringField(TEXT("tallyState"), StateString);

    // Build the event payload
    TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject);
    Payload->SetStringField(TEXT("event"), TEXT("ws:m:tally"));
    Payload->SetObjectField(TEXT("data"), TallyData);

    // Serialize and send
    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(Payload.ToSharedRef(), Writer);

    Subsystem->SendJsonDirect(JsonString);

    UE_LOG(LogRshipExec, Verbose, TEXT("Sent tally state to rship: %s = %s"), *ViewId, *StateString);
}

void URshipMultiCameraManager::StartRecording(const TArray<FString>& ViewIds, const FString& OutputPath)
{
    if (CurrentRecording.bIsRecording) StopRecording();
    CurrentRecording.Id = FGuid::NewGuid().ToString();
    CurrentRecording.StartTime = FDateTime::Now();
    CurrentRecording.RecordingViews = ViewIds;
    CurrentRecording.OutputPath = OutputPath;
    CurrentRecording.bIsRecording = true;
    CurrentRecording.DurationSeconds = 0.0f;
    for (const auto& Id : ViewIds) SetTallyState(Id, ERshipCameraTallyState::Recording);
    OnRecordingStateChanged.Broadcast(true);
}

void URshipMultiCameraManager::StopRecording()
{
    if (!CurrentRecording.bIsRecording) return;
    CurrentRecording.bIsRecording = false;
    UpdateTallyStates();
    OnRecordingStateChanged.Broadcast(false);
}

void URshipMultiCameraManager::SetDefaultTransition(const FRshipCameraTransition& Transition) { DefaultTransition = Transition; }

void URshipMultiCameraManager::UpdateTransition(float DeltaTime)
{
    TransitionProgress += DeltaTime / ActiveTransition.Duration;
    if (TransitionProgress >= 1.0f) { CompleteTransition(); return; }
    ApplyTransitionBlend(TransitionProgress);
}

void URshipMultiCameraManager::CompleteTransition()
{
    bIsTransitioning = false;
    FRshipCameraView* Target = Views.Find(TransitionToViewId);
    if (Target)
    {
        ProgramView = *Target;
        if (ProgramView.Camera)
        {
            APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
            if (PC) PC->SetViewTargetWithBlend(ProgramView.Camera, 0.0f);
        }
    }
    UpdateTallyStates();
    OnCameraSwitched.Broadcast(TransitionFromViewId, TransitionToViewId);
    OnTransitionCompleted.Broadcast();
}

void URshipMultiCameraManager::ApplyTransitionBlend(float Alpha)
{
    if (ActiveTransition.Type == ERshipTransitionType::Dissolve && Alpha < 0.01f)
    {
        FRshipCameraView* To = Views.Find(TransitionToViewId);
        if (To && To->Camera)
        {
            APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
            if (PC) PC->SetViewTargetWithBlend(To->Camera, ActiveTransition.Duration);
        }
    }
}

void URshipMultiCameraManager::ProcessCameraSwitchCommand(const TSharedPtr<FJsonObject>& Data)
{
    if (!Data.IsValid()) return;
    FString ViewId = Data->GetStringField(TEXT("viewId"));
    FString Type = Data->GetStringField(TEXT("transition"));
    float Duration = 0.0f; Data->TryGetNumberField(TEXT("duration"), Duration);
    FRshipCameraTransition T; T.Duration = Duration;
    if (Type == TEXT("cut")) T.Type = ERshipTransitionType::Cut;
    else if (Type == TEXT("dissolve")) T.Type = ERshipTransitionType::Dissolve;
    else if (Type == TEXT("fade")) T.Type = ERshipTransitionType::Fade;
    SwitchWithTransition(ViewId, T);
}
