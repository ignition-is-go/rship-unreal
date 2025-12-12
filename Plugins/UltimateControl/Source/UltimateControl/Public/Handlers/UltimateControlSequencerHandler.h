// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Handlers/UltimateControlHandlerBase.h"

class ULevelSequence;
class ALevelSequenceActor;
class UMovieSceneTrack;
class UMovieSceneSection;
class ISequencer;

/**
 * Handler for Sequencer/Level Sequence operations
 */
class ULTIMATECONTROL_API FUltimateControlSequencerHandler : public FUltimateControlHandlerBase
{
public:
	using FUltimateControlHandlerBase::FUltimateControlHandlerBase;

private:
	// Sequence listing and info
	bool HandleListSequences(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleGetSequence(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleCreateSequence(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Sequence playback
	bool HandlePlaySequence(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleStopSequence(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandlePauseSequence(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleScrubSequence(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Playback state
	bool HandleGetPlaybackState(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleGetCurrentTime(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetCurrentTime(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleGetPlaybackRate(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetPlaybackRate(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Sequence properties
	bool HandleGetSequenceLength(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleGetFrameRate(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetFrameRate(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleGetPlaybackRange(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetPlaybackRange(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Tracks
	bool HandleGetTracks(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleAddTrack(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleRemoveTrack(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleGetTrackSections(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Bindings
	bool HandleGetBindings(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleAddBinding(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleRemoveBinding(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetBindingActor(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Sections
	bool HandleAddSection(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleRemoveSection(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleSetSectionRange(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Keys
	bool HandleAddKey(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleRemoveKey(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleGetKeys(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Sequencer Editor
	bool HandleOpenSequence(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleCloseSequence(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);
	bool HandleGetOpenSequence(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& Result, TSharedPtr<FJsonObject>& Error);

	// Helper methods
	TSharedPtr<FJsonObject> SequenceToJson(ULevelSequence* Sequence);
	TSharedPtr<FJsonObject> TrackToJson(UMovieSceneTrack* Track);
	TSharedPtr<FJsonObject> SectionToJson(UMovieSceneSection* Section);
	ALevelSequenceActor* FindSequenceActor(ULevelSequence* Sequence);
	ISequencer* GetSequencer();
};
