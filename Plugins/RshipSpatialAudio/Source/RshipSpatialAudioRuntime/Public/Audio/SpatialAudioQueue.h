// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SpatialAudioTypes.h"
#include "HAL/PlatformAtomics.h"

/**
 * Lock-free Single-Producer Single-Consumer (SPSC) ring buffer.
 *
 * Used for communication between game thread (producer) and audio thread (consumer).
 * This is the foundational primitive for all audio thread communication.
 *
 * Thread Safety:
 * - Exactly one thread may call Push() (producer)
 * - Exactly one thread may call Pop() (consumer)
 * - No external synchronization required
 *
 * Memory ordering uses acquire/release semantics to ensure proper
 * visibility of data between threads.
 */
template<typename T, uint32 Capacity>
class TSpatialSPSCQueue
{
	static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");

public:
	TSpatialSPSCQueue()
		: Head(0)
		, Tail(0)
	{
	}

	/**
	 * Push an item to the queue (producer thread only).
	 * @param Item The item to push.
	 * @return True if successful, false if queue is full.
	 */
	bool Push(const T& Item)
	{
		const uint32 CurrentTail = Tail.load(std::memory_order_relaxed);
		const uint32 NextTail = (CurrentTail + 1) & (Capacity - 1);

		// Check if queue is full
		if (NextTail == Head.load(std::memory_order_acquire))
		{
			return false;
		}

		Buffer[CurrentTail] = Item;

		// Release ensures the item write is visible before tail update
		Tail.store(NextTail, std::memory_order_release);
		return true;
	}

	/**
	 * Push an item, overwriting oldest if full (producer thread only).
	 * Use when dropping old data is acceptable (e.g., position updates).
	 * @param Item The item to push.
	 */
	void PushOverwrite(const T& Item)
	{
		const uint32 CurrentTail = Tail.load(std::memory_order_relaxed);
		const uint32 NextTail = (CurrentTail + 1) & (Capacity - 1);

		// If full, advance head (discard oldest)
		if (NextTail == Head.load(std::memory_order_acquire))
		{
			Head.store((Head.load(std::memory_order_relaxed) + 1) & (Capacity - 1),
				std::memory_order_release);
		}

		Buffer[CurrentTail] = Item;
		Tail.store(NextTail, std::memory_order_release);
	}

	/**
	 * Pop an item from the queue (consumer thread only).
	 * @param OutItem Output: the popped item.
	 * @return True if successful, false if queue is empty.
	 */
	bool Pop(T& OutItem)
	{
		const uint32 CurrentHead = Head.load(std::memory_order_relaxed);

		// Check if queue is empty
		if (CurrentHead == Tail.load(std::memory_order_acquire))
		{
			return false;
		}

		OutItem = Buffer[CurrentHead];

		// Release ensures we're done reading before head update
		Head.store((CurrentHead + 1) & (Capacity - 1), std::memory_order_release);
		return true;
	}

	/**
	 * Peek at the front item without removing (consumer thread only).
	 * @param OutItem Output: the front item.
	 * @return True if item available, false if queue is empty.
	 */
	bool Peek(T& OutItem) const
	{
		const uint32 CurrentHead = Head.load(std::memory_order_relaxed);

		if (CurrentHead == Tail.load(std::memory_order_acquire))
		{
			return false;
		}

		OutItem = Buffer[CurrentHead];
		return true;
	}

	/**
	 * Check if queue is empty.
	 */
	bool IsEmpty() const
	{
		return Head.load(std::memory_order_acquire) == Tail.load(std::memory_order_acquire);
	}

	/**
	 * Get approximate number of items in queue.
	 * May be slightly stale due to concurrent access.
	 */
	uint32 Size() const
	{
		const uint32 H = Head.load(std::memory_order_acquire);
		const uint32 T = Tail.load(std::memory_order_acquire);
		return (T - H) & (Capacity - 1);
	}

	/**
	 * Get queue capacity.
	 */
	static constexpr uint32 GetCapacity() { return Capacity; }

private:
	alignas(64) T Buffer[Capacity];  // Cache line aligned
	alignas(64) std::atomic<uint32> Head;  // Consumer index (separate cache line)
	alignas(64) std::atomic<uint32> Tail;  // Producer index (separate cache line)
};

// ============================================================================
// AUDIO THREAD COMMANDS
// ============================================================================

/**
 * Command types for audio thread.
 */
enum class ESpatialAudioCommand : uint8
{
	None,

	// Object commands
	UpdateObjectPosition,
	UpdateObjectGains,
	SetObjectSpread,
	SetObjectGain,
	RemoveObject,

	// Speaker commands
	UpdateSpeakerDSP,
	SetSpeakerMute,
	SetSpeakerGain,
	SetSpeakerDelay,

	// Full DSP chain commands
	EnableDSPChain,
	SetDSPBypass,

	// Global commands
	ReconfigureSpeakers,
	SetMasterGain,
	Flush
};

/**
 * Position update for an audio object.
 * Sent from game thread to audio thread.
 */
struct FSpatialObjectPositionUpdate
{
	FGuid ObjectId;
	FVector Position;
	float Spread;
};

/**
 * Computed gains for an audio object.
 * Sent from game thread after renderer computes gains.
 */
struct FSpatialObjectGainsUpdate
{
	FGuid ObjectId;
	TStaticArray<FSpatialSpeakerGain, SPATIAL_AUDIO_MAX_SPEAKERS_PER_OBJECT> Gains;
	int32 GainCount;
};

/**
 * Speaker DSP update.
 */
struct FSpatialSpeakerDSPUpdate
{
	int32 SpeakerIndex;
	float Gain;      // Linear gain
	float DelayMs;   // Delay in milliseconds
	bool bMuted;
};

/**
 * DSP chain control data.
 */
struct FSpatialDSPChainControl
{
	bool bEnable;
	bool bBypass;
};

/**
 * Union of all command payloads.
 */
struct FSpatialAudioCommandData
{
	ESpatialAudioCommand Type;

	union
	{
		FSpatialObjectPositionUpdate Position;
		FSpatialObjectGainsUpdate Gains;
		FSpatialSpeakerDSPUpdate SpeakerDSP;
		FSpatialDSPChainControl DSPControl;
		float MasterGain;
	};

	FSpatialAudioCommandData() : Type(ESpatialAudioCommand::None) {}

	static FSpatialAudioCommandData MakePositionUpdate(const FGuid& ObjectId, const FVector& Pos, float Spread)
	{
		FSpatialAudioCommandData Cmd;
		Cmd.Type = ESpatialAudioCommand::UpdateObjectPosition;
		Cmd.Position.ObjectId = ObjectId;
		Cmd.Position.Position = Pos;
		Cmd.Position.Spread = Spread;
		return Cmd;
	}

	static FSpatialAudioCommandData MakeGainsUpdate(const FGuid& ObjectId, const TArray<FSpatialSpeakerGain>& InGains)
	{
		FSpatialAudioCommandData Cmd;
		Cmd.Type = ESpatialAudioCommand::UpdateObjectGains;
		Cmd.Gains.ObjectId = ObjectId;
		Cmd.Gains.GainCount = FMath::Min(InGains.Num(), (int32)SPATIAL_AUDIO_MAX_SPEAKERS_PER_OBJECT);
		for (int32 i = 0; i < Cmd.Gains.GainCount; ++i)
		{
			Cmd.Gains.Gains[i] = InGains[i];
		}
		return Cmd;
	}

	static FSpatialAudioCommandData MakeSpeakerDSP(int32 Index, float Gain, float DelayMs, bool bMuted)
	{
		FSpatialAudioCommandData Cmd;
		Cmd.Type = ESpatialAudioCommand::UpdateSpeakerDSP;
		Cmd.SpeakerDSP.SpeakerIndex = Index;
		Cmd.SpeakerDSP.Gain = Gain;
		Cmd.SpeakerDSP.DelayMs = DelayMs;
		Cmd.SpeakerDSP.bMuted = bMuted;
		return Cmd;
	}

	static FSpatialAudioCommandData MakeMasterGain(float Gain)
	{
		FSpatialAudioCommandData Cmd;
		Cmd.Type = ESpatialAudioCommand::SetMasterGain;
		Cmd.MasterGain = Gain;
		return Cmd;
	}

	static FSpatialAudioCommandData MakeFlush()
	{
		FSpatialAudioCommandData Cmd;
		Cmd.Type = ESpatialAudioCommand::Flush;
		return Cmd;
	}

	static FSpatialAudioCommandData MakeEnableDSPChain(bool bEnable)
	{
		FSpatialAudioCommandData Cmd;
		Cmd.Type = ESpatialAudioCommand::EnableDSPChain;
		Cmd.DSPControl.bEnable = bEnable;
		return Cmd;
	}

	static FSpatialAudioCommandData MakeSetDSPBypass(bool bBypass)
	{
		FSpatialAudioCommandData Cmd;
		Cmd.Type = ESpatialAudioCommand::SetDSPBypass;
		Cmd.DSPControl.bBypass = bBypass;
		return Cmd;
	}
};

// ============================================================================
// AUDIO THREAD FEEDBACK
// ============================================================================

/**
 * Feedback types from audio thread to game thread.
 */
enum class ESpatialAudioFeedback : uint8
{
	None,
	MeterUpdate,
	LimiterGRUpdate,
	BufferUnderrun,
	LatencyReport
};

/**
 * Meter reading from audio thread.
 */
struct FSpatialMeterFeedback
{
	int32 SpeakerIndex;
	float PeakLevel;
	float RMSLevel;
};

/**
 * Limiter gain reduction feedback.
 */
struct FSpatialLimiterGRFeedback
{
	int32 SpeakerIndex;
	float GainReductionDb;
};

/**
 * Feedback data union.
 */
struct FSpatialAudioFeedbackData
{
	ESpatialAudioFeedback Type;

	union
	{
		FSpatialMeterFeedback Meter;
		FSpatialLimiterGRFeedback LimiterGR;
		uint32 UnderrunCount;
		float LatencyMs;
	};

	FSpatialAudioFeedbackData() : Type(ESpatialAudioFeedback::None) {}
};

// ============================================================================
// QUEUE TYPE ALIASES
// ============================================================================

/** Command queue: Game thread -> Audio thread */
using FSpatialCommandQueue = TSpatialSPSCQueue<FSpatialAudioCommandData, 1024>;

/** Feedback queue: Audio thread -> Game thread */
using FSpatialFeedbackQueue = TSpatialSPSCQueue<FSpatialAudioFeedbackData, 256>;
