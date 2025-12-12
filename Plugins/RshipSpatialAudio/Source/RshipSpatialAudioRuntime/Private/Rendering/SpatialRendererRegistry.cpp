// Copyright Rocketship. All Rights Reserved.

#include "Rendering/SpatialRendererRegistry.h"
#include "Rendering/SpatialRendererVBAP.h"
#include "Rendering/SpatialRendererDBAP.h"
#include "Rendering/SpatialRendererHOA.h"
#include "HAL/IConsoleManager.h"

// Global registry instance
static TUniquePtr<FSpatialRendererRegistry> GRendererRegistry;

FSpatialRendererRegistry& GetGlobalRendererRegistry()
{
	if (!GRendererRegistry.IsValid())
	{
		GRendererRegistry = MakeUnique<FSpatialRendererRegistry>();
	}
	return *GRendererRegistry;
}

FSpatialRendererRegistry::FSpatialRendererRegistry()
	: VBAPUse2D(false)
	, VBAPReferencePoint(FVector::ZeroVector)
	, VBAPPhaseCoherent(true)
	, DBAPRolloffExponent(2.0f)
	, DBAPReferenceDistance(100.0f)  // 1 meter in cm
	, HOAOrder(1)  // First order by default
	, HOADecoderType(3)  // AllRAD by default
	, HOAListenerPosition(FVector::ZeroVector)
{
}

FSpatialRendererRegistry::~FSpatialRendererRegistry()
{
	// Unique pointers will clean up automatically
	CachedRenderers.Empty();
}

TUniquePtr<ISpatialRenderer> FSpatialRendererRegistry::CreateRenderer(ESpatialRendererType Type)
{
	switch (Type)
	{
	case ESpatialRendererType::VBAP:
		return MakeUnique<FSpatialRendererVBAP>();

	case ESpatialRendererType::DBAP:
		return MakeUnique<FSpatialRendererDBAP>();

	case ESpatialRendererType::HOA:
		return MakeUnique<FSpatialRendererHOA>();

	case ESpatialRendererType::Direct:
		// Direct routing doesn't need a spatial renderer
		return nullptr;

	default:
		UE_LOG(LogTemp, Error, TEXT("Unknown renderer type: %d"), static_cast<int32>(Type));
		return nullptr;
	}
}

TUniquePtr<ISpatialRenderer> FSpatialRendererRegistry::CreateConfiguredRenderer(
	ESpatialRendererType Type,
	const TArray<FSpatialSpeaker>& Speakers)
{
	TUniquePtr<ISpatialRenderer> Renderer = CreateRenderer(Type);
	if (Renderer.IsValid())
	{
		Renderer->Configure(Speakers);
	}
	return Renderer;
}

ISpatialRenderer* FSpatialRendererRegistry::GetOrCreateRenderer(
	ESpatialRendererType Type,
	const TArray<FSpatialSpeaker>& Speakers,
	const FSpatialRendererConfig& Config)
{
	// Compute hash of current speaker configuration
	uint32 CurrentHash = ComputeSpeakerHash(Speakers);

	// Check if we have a cached renderer with matching configuration
	if (TUniquePtr<ISpatialRenderer>* Existing = CachedRenderers.Find(Type))
	{
		if (uint32* CachedHash = ConfigurationHashes.Find(Type))
		{
			if (*CachedHash == CurrentHash && Existing->IsValid() && (*Existing)->IsConfigured())
			{
				// Cache hit - return existing renderer
				return Existing->Get();
			}
		}
	}

	// Create new renderer
	TUniquePtr<ISpatialRenderer> NewRenderer = CreateRenderer(Type);
	if (!NewRenderer.IsValid())
	{
		return nullptr;
	}

	// Apply type-specific configuration
	ApplyConfiguration(NewRenderer.Get(), Type);

	// Apply config overrides
	if (Type == ESpatialRendererType::VBAP)
	{
		FSpatialRendererVBAP* VBAPRenderer = static_cast<FSpatialRendererVBAP*>(NewRenderer.Get());
		VBAPRenderer->SetPhaseCoherent(Config.bPhaseCoherent);
		if (Config.ReferenceDistanceCm > 0.0f)
		{
			// Use reference distance to set reference point on forward axis
			VBAPRenderer->SetReferencePoint(FVector(Config.ReferenceDistanceCm, 0.0f, 0.0f));
		}
	}

	// Configure with speakers
	NewRenderer->Configure(Speakers);

	// Cache the renderer
	ISpatialRenderer* RawPtr = NewRenderer.Get();
	CachedRenderers.Add(Type, MoveTemp(NewRenderer));
	ConfigurationHashes.Add(Type, CurrentHash);

	return RawPtr;
}

ISpatialRenderer* FSpatialRendererRegistry::GetCachedRenderer(ESpatialRendererType Type) const
{
	if (const TUniquePtr<ISpatialRenderer>* Existing = CachedRenderers.Find(Type))
	{
		if (Existing->IsValid() && (*Existing)->IsConfigured())
		{
			return Existing->Get();
		}
	}
	return nullptr;
}

void FSpatialRendererRegistry::InvalidateCache()
{
	CachedRenderers.Empty();
	ConfigurationHashes.Empty();
}

void FSpatialRendererRegistry::InvalidateRenderer(ESpatialRendererType Type)
{
	CachedRenderers.Remove(Type);
	ConfigurationHashes.Remove(Type);
}

bool FSpatialRendererRegistry::IsRendererCached(ESpatialRendererType Type) const
{
	if (const TUniquePtr<ISpatialRenderer>* Existing = CachedRenderers.Find(Type))
	{
		return Existing->IsValid() && (*Existing)->IsConfigured();
	}
	return false;
}

void FSpatialRendererRegistry::SetVBAPConfig(bool bUse2D, const FVector& ReferencePoint, bool bPhaseCoherent)
{
	VBAPUse2D = bUse2D;
	VBAPReferencePoint = ReferencePoint;
	VBAPPhaseCoherent = bPhaseCoherent;

	// Invalidate cached VBAP renderer to force reconfiguration
	InvalidateRenderer(ESpatialRendererType::VBAP);
}

void FSpatialRendererRegistry::SetDBAPConfig(float RolloffExponent, float ReferenceDistance)
{
	DBAPRolloffExponent = RolloffExponent;
	DBAPReferenceDistance = ReferenceDistance;

	// Invalidate cached DBAP renderer to force reconfiguration
	InvalidateRenderer(ESpatialRendererType::DBAP);
}

void FSpatialRendererRegistry::SetHOAConfig(int32 Order, int32 DecoderType, const FVector& ListenerPosition)
{
	HOAOrder = FMath::Clamp(Order, 1, 5);
	HOADecoderType = FMath::Clamp(DecoderType, 0, 4);
	HOAListenerPosition = ListenerPosition;

	// Invalidate cached HOA renderer to force reconfiguration
	InvalidateRenderer(ESpatialRendererType::HOA);
}

FString FSpatialRendererRegistry::GetRendererTypeName(ESpatialRendererType Type)
{
	switch (Type)
	{
	case ESpatialRendererType::VBAP:
		return TEXT("VBAP");
	case ESpatialRendererType::DBAP:
		return TEXT("DBAP");
	case ESpatialRendererType::HOA:
		return TEXT("HOA");
	case ESpatialRendererType::Direct:
		return TEXT("Direct");
	default:
		return TEXT("Unknown");
	}
}

FString FSpatialRendererRegistry::GetRendererTypeDescription(ESpatialRendererType Type)
{
	switch (Type)
	{
	case ESpatialRendererType::VBAP:
		return TEXT("Vector Base Amplitude Panning - Psychoacoustically accurate panning using triangulated speaker configurations. Best for precise localization.");
	case ESpatialRendererType::DBAP:
		return TEXT("Distance Based Amplitude Panning - Distance-weighted panning to all speakers. Best for immersive soundscapes.");
	case ESpatialRendererType::HOA:
		return TEXT("Higher Order Ambisonics - Spherical harmonic encoding/decoding. Best for room-filling ambience.");
	case ESpatialRendererType::Direct:
		return TEXT("Direct Routing - No spatial processing, direct channel assignment.");
	default:
		return TEXT("Unknown renderer type");
	}
}

bool FSpatialRendererRegistry::IsRendererTypeSupported(ESpatialRendererType Type)
{
	switch (Type)
	{
	case ESpatialRendererType::VBAP:
		return true;  // Implemented
	case ESpatialRendererType::DBAP:
		return true;  // Implemented in M4
	case ESpatialRendererType::HOA:
		return true;  // Implemented in M10
	case ESpatialRendererType::Direct:
		return true;  // No renderer needed
	default:
		return false;
	}
}

TArray<ESpatialRendererType> FSpatialRendererRegistry::GetSupportedRendererTypes()
{
	return TArray<ESpatialRendererType>{
		ESpatialRendererType::VBAP,
		ESpatialRendererType::DBAP,
		ESpatialRendererType::HOA,
		ESpatialRendererType::Direct
	};
}

uint32 FSpatialRendererRegistry::ComputeSpeakerHash(const TArray<FSpatialSpeaker>& Speakers)
{
	uint32 Hash = GetTypeHash(Speakers.Num());

	for (const FSpatialSpeaker& Speaker : Speakers)
	{
		// Hash position (primary factor for spatial configuration)
		Hash = HashCombine(Hash, GetTypeHash(Speaker.WorldPosition.X));
		Hash = HashCombine(Hash, GetTypeHash(Speaker.WorldPosition.Y));
		Hash = HashCombine(Hash, GetTypeHash(Speaker.WorldPosition.Z));

		// Hash ID to detect speaker identity changes
		Hash = HashCombine(Hash, GetTypeHash(Speaker.Id));
	}

	return Hash;
}

void FSpatialRendererRegistry::ApplyConfiguration(ISpatialRenderer* Renderer, ESpatialRendererType Type)
{
	if (!Renderer)
	{
		return;
	}

	switch (Type)
	{
	case ESpatialRendererType::VBAP:
		if (FSpatialRendererVBAP* VBAPRenderer = static_cast<FSpatialRendererVBAP*>(Renderer))
		{
			VBAPRenderer->SetUse2DMode(VBAPUse2D);
			VBAPRenderer->SetReferencePoint(VBAPReferencePoint);
			VBAPRenderer->SetPhaseCoherent(VBAPPhaseCoherent);
		}
		break;

	case ESpatialRendererType::DBAP:
		if (FSpatialRendererDBAP* DBAPRenderer = static_cast<FSpatialRendererDBAP*>(Renderer))
		{
			DBAPRenderer->SetRolloffExponent(DBAPRolloffExponent);
			DBAPRenderer->SetReferenceDistance(DBAPReferenceDistance);
			DBAPRenderer->SetReferencePoint(VBAPReferencePoint);  // Share reference point
			DBAPRenderer->SetPhaseCoherent(VBAPPhaseCoherent);    // Share phase coherence setting
		}
		break;

	case ESpatialRendererType::HOA:
		if (FSpatialRendererHOA* HOARenderer = static_cast<FSpatialRendererHOA*>(Renderer))
		{
			HOARenderer->SetListenerPosition(HOAListenerPosition);
			HOARenderer->SetOrder(static_cast<EAmbisonicsOrder>(HOAOrder));
			HOARenderer->SetDecoderType(static_cast<EAmbisonicsDecoderType>(HOADecoderType));
		}
		break;

	default:
		break;
	}
}
