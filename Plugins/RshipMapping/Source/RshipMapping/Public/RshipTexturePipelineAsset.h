// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Engine/DataAsset.h"
#include "RshipTexturePipelineAsset.generated.h"

UENUM(BlueprintType)
enum class ERshipPipelineNodeKind : uint8
{
	RenderContextInput,
	ExternalTextureInput,
	MediaProfileInput,
	Projection,
	TransformRect,
	Opacity,
	BlendComposite,
	MappingSurfaceOutput,
	MediaProfileOutput
};

UENUM(BlueprintType)
enum class ERshipPipelinePinDirection : uint8
{
	Input,
	Output
};

UENUM(BlueprintType)
enum class ERshipPipelineDiagnosticSeverity : uint8
{
	Info,
	Warning,
	Error
};

USTRUCT(BlueprintType)
struct RSHIPMAPPING_API FRshipPipelinePin
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	ERshipPipelinePinDirection Direction = ERshipPipelinePinDirection::Input;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString DataType;
};

USTRUCT(BlueprintType)
struct RSHIPMAPPING_API FRshipPipelineNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	ERshipPipelineNodeKind Kind = ERshipPipelineNodeKind::RenderContextInput;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FVector2D Position = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	TArray<FRshipPipelinePin> Pins;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	TMap<FString, FString> Params;
};

USTRUCT(BlueprintType)
struct RSHIPMAPPING_API FRshipPipelineEdge
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString FromNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString FromPinId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString ToNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString ToPinId;
};

USTRUCT(BlueprintType)
struct RSHIPMAPPING_API FRshipPipelineDiagnostic
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	ERshipPipelineDiagnosticSeverity Severity = ERshipPipelineDiagnosticSeverity::Info;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString Code;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString EdgeId;
};

USTRUCT(BlueprintType)
struct RSHIPMAPPING_API FRshipPipelineContextSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString ProjectId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString SourceType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString CameraId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString AssetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString ExternalSourceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	int32 Width = 1920;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	int32 Height = 1080;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString CaptureMode = TEXT("FinalColorLDR");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	bool bEnabled = true;
};

USTRUCT(BlueprintType)
struct RSHIPMAPPING_API FRshipPipelineSurfaceSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString ProjectId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString ActorPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString MeshComponentName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	int32 UVChannel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	TArray<int32> MaterialSlots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	bool bEnabled = true;
};

USTRUCT(BlueprintType)
struct RSHIPMAPPING_API FRshipPipelineMappingSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString ProjectId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString Type;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString ContextId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	TArray<FString> SurfaceIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	float Opacity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString ConfigJson;
};

USTRUCT(BlueprintType)
struct RSHIPMAPPING_API FRshipPipelineEndpointBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString AdapterId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString EndpointId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString Direction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString NodeId;
};

USTRUCT(BlueprintType)
struct RSHIPMAPPING_API FRshipCompiledPipelinePlan
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString AssetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	int64 Revision = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	bool bValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	TArray<FString> TopologicalOrder;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	TArray<FRshipPipelineContextSpec> Contexts;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	TArray<FRshipPipelineSurfaceSpec> Surfaces;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	TArray<FRshipPipelineMappingSpec> Mappings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	TArray<FRshipPipelineEndpointBinding> EndpointBindings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	TArray<FRshipPipelineDiagnostic> Diagnostics;
};

UINTERFACE(BlueprintType)
class RSHIPMAPPING_API URshipTexturePipelineEndpointAdapter : public UInterface
{
	GENERATED_BODY()
};

class RSHIPMAPPING_API IRshipTexturePipelineEndpointAdapter
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Rship|TexturePipeline")
	bool ValidateEndpoint(
		const FString& AdapterId,
		const FString& EndpointId,
		const FString& Direction,
		FString& OutError) const;
};

UCLASS(BlueprintType)
class RSHIPMAPPING_API URshipTexturePipelineAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	FString AssetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	int64 Revision = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	bool bStrictNoFallback = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	TArray<FRshipPipelineNode> Nodes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|TexturePipeline")
	TArray<FRshipPipelineEdge> Edges;
};
