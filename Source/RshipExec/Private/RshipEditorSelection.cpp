// Rocketship Editor Selection Sync Implementation

#include "RshipEditorSelection.h"
#include "RshipSubsystem.h"
#include "RshipTargetComponent.h"
#include "RshipBulkOperations.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/Selection.h"
#include "LevelEditor.h"
#include "ILevelEditor.h"
#include "LevelEditorViewport.h"
#include "EditorViewportClient.h"
#endif

URshipEditorSelection::URshipEditorSelection()
{
}

void URshipEditorSelection::Initialize(URshipSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;

#if WITH_EDITOR
	BindEditorEvents();
#endif

	UE_LOG(LogTemp, Log, TEXT("RshipEditorSelection: Initialized (Editor=%s)"),
		IsEditorSyncAvailable() ? TEXT("Yes") : TEXT("No"));
}

void URshipEditorSelection::Shutdown()
{
#if WITH_EDITOR
	UnbindEditorEvents();
#endif

	Subsystem = nullptr;
	UE_LOG(LogTemp, Log, TEXT("RshipEditorSelection: Shutdown"));
}

// ============================================================================
// SYNC CONTROL
// ============================================================================

void URshipEditorSelection::SetSyncMode(ERshipSelectionSyncMode Mode)
{
	if (SyncMode == Mode) return;

	SyncMode = Mode;

	UE_LOG(LogTemp, Log, TEXT("RshipEditorSelection: Sync mode set to %d"), static_cast<int32>(Mode));

	// Perform initial sync based on mode
	if (Mode == ERshipSelectionSyncMode::EditorToRship || Mode == ERshipSelectionSyncMode::Bidirectional)
	{
		SyncEditorToRship();
	}
}

bool URshipEditorSelection::IsEditorSyncAvailable() const
{
#if WITH_EDITOR
	return GEditor != nullptr;
#else
	return false;
#endif
}

// ============================================================================
// MANUAL SYNC
// ============================================================================

int32 URshipEditorSelection::SyncEditorToRship()
{
#if WITH_EDITOR
	if (!GEditor || !Subsystem) return 0;

	bIsSyncing = true;

	TArray<URshipTargetComponent*> Targets = GetTargetsFromEditorSelection();

	// Clear and set Rship selection
	URshipBulkOperations::ClearSelection();
	URshipBulkOperations::SelectTargets(Targets);

	bIsSyncing = false;

	OnSelectionSynced.Broadcast(Targets.Num());

	UE_LOG(LogTemp, Verbose, TEXT("RshipEditorSelection: Synced %d targets from Editor to Rship"),
		Targets.Num());

	return Targets.Num();
#else
	return 0;
#endif
}

int32 URshipEditorSelection::SyncRshipToEditor()
{
#if WITH_EDITOR
	if (!GEditor || !Subsystem) return 0;

	bIsSyncing = true;

	TArray<URshipTargetComponent*> Targets = URshipBulkOperations::GetSelectedTargets();
	int32 Count = SelectActorsInEditor(Targets);

	bIsSyncing = false;

	OnSelectionSynced.Broadcast(Count);

	UE_LOG(LogTemp, Verbose, TEXT("RshipEditorSelection: Synced %d targets from Rship to Editor"),
		Count);

	return Count;
#else
	return 0;
#endif
}

int32 URshipEditorSelection::SelectActorsInEditor(const TArray<URshipTargetComponent*>& Targets)
{
#if WITH_EDITOR
	if (!GEditor) return 0;

	USelection* Selection = GEditor->GetSelectedActors();
	if (!Selection) return 0;

	// Begin transaction for undo support
	GEditor->SelectNone(false, true, false);

	int32 Count = 0;
	for (URshipTargetComponent* Target : Targets)
	{
		if (Target && Target->GetOwner())
		{
			GEditor->SelectActor(Target->GetOwner(), true, true, false);
			Count++;
		}
	}

	// Notify of selection change
	GEditor->NoteSelectionChange();

	return Count;
#else
	return 0;
#endif
}

TArray<URshipTargetComponent*> URshipEditorSelection::GetTargetsFromEditorSelection()
{
	TArray<URshipTargetComponent*> Result;

#if WITH_EDITOR
	if (!GEditor) return Result;

	USelection* Selection = GEditor->GetSelectedActors();
	if (!Selection) return Result;

	for (FSelectionIterator It(*Selection); It; ++It)
	{
		AActor* Actor = Cast<AActor>(*It);
		if (Actor)
		{
			URshipTargetComponent* Target = Actor->FindComponentByClass<URshipTargetComponent>();
			if (Target)
			{
				Result.Add(Target);
			}
		}
	}
#endif

	return Result;
}

// ============================================================================
// VIEWPORT FOCUS
// ============================================================================

void URshipEditorSelection::FocusOnSelectedTargets()
{
	TArray<URshipTargetComponent*> Targets = URshipBulkOperations::GetSelectedTargets();
	FocusOnTargets(Targets);
}

void URshipEditorSelection::FocusOnTargets(const TArray<URshipTargetComponent*>& Targets)
{
#if WITH_EDITOR
	if (!GEditor || Targets.Num() == 0) return;

	// Calculate bounding box of all target actors
	FBox BoundingBox(ForceInit);
	bool bHasValidBounds = false;

	for (URshipTargetComponent* Target : Targets)
	{
		if (Target && Target->GetOwner())
		{
			FBox ActorBounds = Target->GetOwner()->GetComponentsBoundingBox();
			if (ActorBounds.IsValid)
			{
				if (bHasValidBounds)
				{
					BoundingBox += ActorBounds;
				}
				else
				{
					BoundingBox = ActorBounds;
					bHasValidBounds = true;
				}
			}
		}
	}

	if (!bHasValidBounds) return;

	// Get the level editor viewport and focus on the bounds
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetFirstLevelEditor();

	if (LevelEditor.IsValid())
	{
		TSharedPtr<SLevelViewport> ActiveViewport = LevelEditor->GetActiveViewportInterface();
		if (ActiveViewport.IsValid())
		{
			FEditorViewportClient& ViewportClient = ActiveViewport->GetLevelViewportClient();

			// Focus viewport on the bounding box
			ViewportClient.FocusViewportOnBox(BoundingBox, true);

			UE_LOG(LogTemp, Log, TEXT("RshipEditorSelection: Focused viewport on %d targets"),
				Targets.Num());
		}
	}
#endif
}

// ============================================================================
// EDITOR EVENT HANDLERS
// ============================================================================

#if WITH_EDITOR

void URshipEditorSelection::OnEditorSelectionChanged(UObject* Object)
{
	if (bIsSyncing) return;

	if (SyncMode == ERshipSelectionSyncMode::EditorToRship ||
		SyncMode == ERshipSelectionSyncMode::Bidirectional)
	{
		SyncEditorToRship();
	}
}

void URshipEditorSelection::OnRshipSelectionChanged()
{
	if (bIsSyncing) return;

	if (SyncMode == ERshipSelectionSyncMode::RshipToEditor ||
		SyncMode == ERshipSelectionSyncMode::Bidirectional)
	{
		SyncRshipToEditor();
	}
}

void URshipEditorSelection::BindEditorEvents()
{
	if (!GEditor) return;

	// Bind to editor selection changed
	USelection* Selection = GEditor->GetSelectedActors();
	if (Selection)
	{
		EditorSelectionHandle = Selection->SelectionChangedEvent.AddUObject(
			this, &URshipEditorSelection::OnEditorSelectionChanged);
	}

	// Bind to Rship selection changed via subsystem delegate
	if (Subsystem)
	{
		Subsystem->OnSelectionChanged.AddDynamic(this, &URshipEditorSelection::OnRshipSelectionChanged);
	}

	UE_LOG(LogTemp, Verbose, TEXT("RshipEditorSelection: Bound to editor events"));
}

void URshipEditorSelection::UnbindEditorEvents()
{
	if (GEditor)
	{
		USelection* Selection = GEditor->GetSelectedActors();
		if (Selection)
		{
			Selection->SelectionChangedEvent.Remove(EditorSelectionHandle);
		}
	}

	if (Subsystem)
	{
		Subsystem->OnSelectionChanged.RemoveDynamic(this, &URshipEditorSelection::OnRshipSelectionChanged);
	}

	UE_LOG(LogTemp, Verbose, TEXT("RshipEditorSelection: Unbound from editor events"));
}

#endif // WITH_EDITOR
