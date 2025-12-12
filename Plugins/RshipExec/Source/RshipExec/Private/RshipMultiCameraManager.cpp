// Rship Multi-Camera Manager Implementation

#include "RshipMultiCameraManager.h"
#include "RshipSubsystem.h"
#include "Logs.h"
#include "Kismet/GameplayStatics.h"

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
void URshipMultiCameraManager::EvaluateAutoSwitchRules() { /* TODO: Implement rule evaluation */ }

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

void URshipMultiCameraManager::SendTallyToRship(const FString& ViewId, ERshipCameraTallyState State) { /* TODO: Send to rship */ }

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
