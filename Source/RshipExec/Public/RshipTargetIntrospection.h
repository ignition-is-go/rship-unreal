#pragma once

#include "CoreMinimal.h"
#include "RshipTargetIntrospection.generated.h"

USTRUCT(BlueprintType)
struct FRshipSchemaField
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship")
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship")
    FString Type;
};

USTRUCT(BlueprintType)
struct FRshipActionDescription
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship")
    FString ActionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship")
    FString FunctionName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship")
    TArray<FRshipSchemaField> Parameters;
};

USTRUCT(BlueprintType)
struct FRshipEmitterDescription
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship")
    FString EmitterId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship")
    TArray<FRshipSchemaField> Payload;
};

USTRUCT(BlueprintType)
struct FRshipTargetDescription
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship")
    FString TargetId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship")
    FString TargetName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship")
    TArray<FRshipActionDescription> Actions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship")
    TArray<FRshipEmitterDescription> Emitters;
};

