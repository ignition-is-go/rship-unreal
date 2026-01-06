// Rship PCG Spawn Actor Settings Implementation

#include "PCG/RshipPCGSpawnActorSettings.h"

#if RSHIP_HAS_PCG

#include "PCG/RshipPCGAutoBindComponent.h"
#include "PCG/RshipPCGManager.h"
#include "RshipSubsystem.h"
#include "Logs.h"
#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGData.h"
#include "PCGPoint.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Helpers/PCGHelpers.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"

#endif // RSHIP_HAS_PCG

// ============================================================================
// URSHIPPCGSPAWNACTORSETTINGS - Constructor always available
// ============================================================================

URshipPCGSpawnActorSettings::URshipPCGSpawnActorSettings()
{
	// Add default "pcg" tag
	DefaultTags.Add(TEXT("pcg"));
}

// ============================================================================
// PCG-SPECIFIC IMPLEMENTATION
// ============================================================================

#if RSHIP_HAS_PCG

#if WITH_EDITOR
FText URshipPCGSpawnActorSettings::GetNodeTooltipText() const
{
	return NSLOCTEXT("RshipPCG", "SpawnActorTooltip",
		"Spawns actors from PCG points with automatic rShip binding.\n\n"
		"Each spawned actor becomes an rShip Target with:\n"
		"- Stable, deterministic Target ID\n"
		"- Automatic property binding (Actions/Emitters)\n"
		"- Clean lifecycle management\n\n"
		"Mark properties with meta=(RShipParam) to expose them to rShip.");
}
#endif

TArray<FPCGPinProperties> URshipPCGSpawnActorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point);
	return Pins;
}

TArray<FPCGPinProperties> URshipPCGSpawnActorSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	// Output the spawned actors as spatial data
	Pins.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Spatial);
	return Pins;
}

FPCGElementPtr URshipPCGSpawnActorSettings::CreateElement() const
{
	return MakeShared<FRshipPCGSpawnActorElement>();
}

// ============================================================================
// FRSHIPPCGSPAWNACTORELEMENT
// ============================================================================

bool FRshipPCGSpawnActorElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FRshipPCGSpawnActorElement::ExecuteInternal);

	const URshipPCGSpawnActorSettings* Settings = Context->GetInputSettings<URshipPCGSpawnActorSettings>();
	check(Settings);

	if (!Settings->TemplateActorClass)
	{
		PCGE_LOG(Error, GraphAndLog, NSLOCTEXT("RshipPCG", "NoActorClass", "No actor class specified"));
		return true;
	}

	UWorld* World = Context->SourceComponent.IsValid() ? Context->SourceComponent->GetWorld() : nullptr;
	if (!World)
	{
		PCGE_LOG(Error, GraphAndLog, NSLOCTEXT("RshipPCG", "NoWorld", "No world available"));
		return true;
	}

	// Get PCG component GUID for identity
	FGuid PCGComponentGuid;
	FString SourceKey = TEXT("unknown");

	if (Context->SourceComponent.IsValid())
	{
		UPCGComponent* PCGComp = Context->SourceComponent.Get();
		PCGComponentGuid = PCGComp->GetUniqueID();

		// Try to get source key from owner
		if (AActor* Owner = PCGComp->GetOwner())
		{
			SourceKey = Owner->GetName();
		}
	}
	else
	{
		PCGComponentGuid = FGuid::NewGuid();
	}

	// Get input data
	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	int32 TotalSpawned = 0;

	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);
		if (!SpatialData)
		{
			continue;
		}

		const UPCGPointData* PointData = SpatialData->ToPointData(Context);
		if (!PointData)
		{
			continue;
		}

		const TArray<FPCGPoint>& Points = PointData->GetPoints();
		const UPCGMetadata* Metadata = PointData->ConstMetadata();

		// Get attribute accessors
		const FPCGMetadataAttribute<int32>* SeedAttr = nullptr;
		const FPCGMetadataAttribute<float>* DistanceAttr = nullptr;
		const FPCGMetadataAttribute<float>* AlphaAttr = nullptr;
		const FPCGMetadataAttribute<int32>* IndexAttr = nullptr;

		if (Metadata)
		{
			if (Settings->SeedAttribute != NAME_None)
			{
				SeedAttr = Metadata->GetConstTypedAttribute<int32>(Settings->SeedAttribute);
			}
			if (Settings->DistanceAttribute != NAME_None)
			{
				DistanceAttr = Metadata->GetConstTypedAttribute<float>(Settings->DistanceAttribute);
			}
			if (Settings->AlphaAttribute != NAME_None)
			{
				AlphaAttr = Metadata->GetConstTypedAttribute<float>(Settings->AlphaAttribute);
			}
			if (Settings->PointIndexAttribute != NAME_None)
			{
				IndexAttr = Metadata->GetConstTypedAttribute<int32>(Settings->PointIndexAttribute);
			}
		}

		// Override source key from attribute if specified
		FString EffectiveSourceKey = SourceKey;
		if (Settings->SourceKeyAttribute != NAME_None && Metadata)
		{
			const FPCGMetadataAttribute<FString>* SourceKeyAttr =
				Metadata->GetConstTypedAttribute<FString>(Settings->SourceKeyAttribute);
			if (SourceKeyAttr && Points.Num() > 0)
			{
				// Use first point's value as source key
				EffectiveSourceKey = SourceKeyAttr->GetValueFromItemKey(Points[0].MetadataEntry);
			}
		}

		// Create output point data
		UPCGPointData* OutputPointData = NewObject<UPCGPointData>();
		TArray<FPCGPoint>& OutputPoints = OutputPointData->GetMutablePoints();

		// Spawn actors
		for (int32 i = 0; i < Points.Num(); ++i)
		{
			const FPCGPoint& Point = Points[i];

			AActor* SpawnedActor = SpawnActorFromPoint(
				Context,
				Settings,
				Point,
				i,
				PCGComponentGuid,
				EffectiveSourceKey);

			if (SpawnedActor)
			{
				// Configure rShip binding
				if (Settings->bEnableRshipBinding)
				{
					URshipPCGAutoBindComponent* BindComp = SpawnedActor->FindComponentByClass<URshipPCGAutoBindComponent>();
					if (BindComp)
					{
						// Build instance ID
						FRshipPCGInstanceId InstanceId = BuildInstanceId(
							Settings,
							Point,
							i,
							SeedAttr,
							DistanceAttr,
							AlphaAttr,
							IndexAttr,
							PCGComponentGuid,
							EffectiveSourceKey);

						BindComp->SetInstanceId(InstanceId);
						BindComp->TargetCategory = Settings->TargetCategory;
						BindComp->Tags = Settings->DefaultTags;
						BindComp->bIncludeSiblingComponents = Settings->bIncludeSiblingComponents;
						BindComp->bIncludeInheritedProperties = Settings->bIncludeInheritedProperties;
						BindComp->DefaultPulseMode = Settings->DefaultPulseMode;
						BindComp->DefaultPulseRateHz = Settings->DefaultPulseRateHz;
					}
				}

				// Add to output
				FPCGPoint& OutputPoint = OutputPoints.Add_GetRef(Point);
				// Store actor reference in point metadata if needed
				// (OutputPoint.MetadataEntry can be used to track spawned actors)

				TotalSpawned++;
			}
		}

		// Add output
		FPCGTaggedData& Output = Outputs.Emplace_GetRef();
		Output.Data = OutputPointData;
		Output.Tags = Input.Tags;
	}

	UE_LOG(LogRshipExec, Log, TEXT("RshipPCGSpawnActor: Spawned %d actors"), TotalSpawned);

	return true;
}

AActor* FRshipPCGSpawnActorElement::SpawnActorFromPoint(
	FPCGContext* Context,
	const URshipPCGSpawnActorSettings* Settings,
	const FPCGPoint& Point,
	int32 PointIndex,
	const FGuid& PCGComponentGuid,
	const FString& SourceKey) const
{
	UWorld* World = Context->SourceComponent.IsValid() ? Context->SourceComponent->GetWorld() : nullptr;
	if (!World || !Settings->TemplateActorClass)
	{
		return nullptr;
	}

	// Calculate spawn transform from point
	FTransform SpawnTransform = Point.Transform;

	// Apply point scale and rotation
	SpawnTransform.SetScale3D(SpawnTransform.GetScale3D() * Point.GetExtents());

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = Settings->CollisionHandling;
	SpawnParams.bNoFail = true;
	SpawnParams.bDeferConstruction = false;

	if (Context->SourceComponent.IsValid())
	{
		SpawnParams.Owner = Context->SourceComponent->GetOwner();
	}

	// Spawn the actor
	AActor* SpawnedActor = World->SpawnActor<AActor>(
		Settings->TemplateActorClass,
		SpawnTransform,
		SpawnParams);

	if (!SpawnedActor)
	{
		UE_LOG(LogRshipExec, Warning, TEXT("RshipPCGSpawnActor: Failed to spawn actor at index %d"), PointIndex);
		return nullptr;
	}

	// Add rShip binding component if enabled
	if (Settings->bEnableRshipBinding)
	{
		// Check if component already exists (e.g., in Blueprint)
		URshipPCGAutoBindComponent* BindComp = SpawnedActor->FindComponentByClass<URshipPCGAutoBindComponent>();
		if (!BindComp)
		{
			BindComp = NewObject<URshipPCGAutoBindComponent>(SpawnedActor, NAME_None, RF_Transactional);
			BindComp->RegisterComponent();
			SpawnedActor->AddInstanceComponent(BindComp);
		}
	}

	// Attach to component if requested
	if (Settings->bAttachToComponent && Context->SourceComponent.IsValid())
	{
		if (USceneComponent* RootComponent = Context->SourceComponent->GetOwner()->GetRootComponent())
		{
			SpawnedActor->AttachToComponent(
				RootComponent,
				FAttachmentTransformRules::KeepWorldTransform);
		}
	}

	// Set actor label in editor
#if WITH_EDITOR
	FString DisplayName = ApplyNamingPattern(Settings, Point, PointIndex, SourceKey);
	SpawnedActor->SetActorLabel(DisplayName);
#endif

	return SpawnedActor;
}

FRshipPCGInstanceId FRshipPCGSpawnActorElement::BuildInstanceId(
	const URshipPCGSpawnActorSettings* Settings,
	const FPCGPoint& Point,
	int32 PointIndex,
	const FPCGMetadataAttribute<int32>* SeedAttr,
	const FPCGMetadataAttribute<float>* DistanceAttr,
	const FPCGMetadataAttribute<float>* AlphaAttr,
	const FPCGMetadataAttribute<int32>* IndexAttr,
	const FGuid& PCGComponentGuid,
	const FString& SourceKey) const
{
	// Extract values from attributes
	int32 Seed = Point.Seed; // PCG points have built-in seed
	double Distance = 0.0;
	double Alpha = 0.0;
	int32 Index = PointIndex;

	if (SeedAttr)
	{
		Seed = SeedAttr->GetValueFromItemKey(Point.MetadataEntry);
	}
	if (DistanceAttr)
	{
		Distance = static_cast<double>(DistanceAttr->GetValueFromItemKey(Point.MetadataEntry));
	}
	if (AlphaAttr)
	{
		Alpha = static_cast<double>(AlphaAttr->GetValueFromItemKey(Point.MetadataEntry));
	}
	if (IndexAttr)
	{
		Index = IndexAttr->GetValueFromItemKey(Point.MetadataEntry);
	}

	// Generate display name
	FString DisplayName = ApplyNamingPattern(Settings, Point, PointIndex, SourceKey);

	return FRshipPCGInstanceId::FromPCGPoint(
		PCGComponentGuid,
		SourceKey,
		Index,
		Distance,
		Alpha,
		Seed,
		DisplayName);
}

FString FRshipPCGSpawnActorElement::ApplyNamingPattern(
	const URshipPCGSpawnActorSettings* Settings,
	const FPCGPoint& Point,
	int32 PointIndex,
	const FString& SourceKey) const
{
	FString Result = Settings->TargetNamePattern;

	// Replace placeholders
	if (Settings->TemplateActorClass)
	{
		Result = Result.Replace(TEXT("{class}"), *Settings->TemplateActorClass->GetName());
	}
	Result = Result.Replace(TEXT("{index}"), *FString::Printf(TEXT("%d"), PointIndex));
	Result = Result.Replace(TEXT("{source}"), *SourceKey);

	FVector Location = Point.Transform.GetLocation();
	Result = Result.Replace(TEXT("{x}"), *FString::Printf(TEXT("%.0f"), Location.X));
	Result = Result.Replace(TEXT("{y}"), *FString::Printf(TEXT("%.0f"), Location.Y));
	Result = Result.Replace(TEXT("{z}"), *FString::Printf(TEXT("%.0f"), Location.Z));

	return Result;
}

#endif // RSHIP_HAS_PCG
