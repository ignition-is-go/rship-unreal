#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RshipFieldTypes.h"
#include "RshipFieldTargetComponent.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UTexture;
class UTexture2D;
class UTextureRenderTarget2D;
class URshipFieldBindingRegistryAsset;
class URshipFieldSubsystem;

UCLASS(ClassGroup = (Rendering), meta = (BlueprintSpawnableComponent))
class RSHIPFIELD_API URshipFieldTargetComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    URshipFieldTargetComponent();

    virtual void OnRegister() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Identity")
    FString VisibleTargetPath;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rship|Field|Identity", AdvancedDisplay)
    FGuid StableGuid;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rship|Field|Identity")
    FString FingerprintActorPath;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rship|Field|Identity")
    FString FingerprintComponentName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rship|Field|Identity")
    FString FingerprintMeshPath;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|Field|Identity", AdvancedDisplay)
    TObjectPtr<URshipFieldBindingRegistryAsset> BindingRegistryAsset = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|Field|Cache")
    TObjectPtr<UTexture2D> RestPositionTexture = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|Field|Cache")
    TObjectPtr<UTexture2D> RestNormalTexture = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|Field|Cache")
    TObjectPtr<UTexture2D> MaskTexture = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|Field|Output")
    TObjectPtr<UTextureRenderTarget2D> DeformedPositionRT = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|Field|Output")
    TObjectPtr<UTextureRenderTarget2D> DeformedNormalRT = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|Field|Target")
    TObjectPtr<UStaticMeshComponent> TargetMeshComponent = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Axis", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AxisWeightNormal = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Axis", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AxisWeightWorld = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Axis", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AxisWeightObject = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Axis")
    FVector WorldAxis = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Axis")
    FVector ObjectAxis = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Axis")
    float ScalarGain = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Axis")
    float VectorGain = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Axis", meta = (ClampMin = "0.0"))
    float MaxDisplacementCm = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rship|Field|Performance")
    bool bUseAsyncCompute = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|Field|Material")
    FName RestPositionParameterName = TEXT("RestPositionTex");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|Field|Material")
    FName DeformedPositionParameterName = TEXT("DeformedPositionTex");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|Field|Material")
    FName DeformedNormalParameterName = TEXT("DeformedNormalTex");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|Field|Material")
    FName GlobalScalarFieldParameterName = TEXT("RshipFieldScalarAtlasTex");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rship|Field|Material")
    FName GlobalVectorFieldParameterName = TEXT("RshipFieldVectorAtlasTex");

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Rship|Field|Identity")
    void RefreshDeterministicIdentity();

    bool ValidateConfig(FString* OutError = nullptr) const;
    void UpdateMaterialBindings(UTexture* GlobalScalarAtlas, UTexture* GlobalVectorAtlas);
    FRshipFieldTargetIdentity BuildIdentity() const;

private:
    void PersistIdentityToRegistry();

    TArray<TObjectPtr<UMaterialInstanceDynamic>> DynamicMaterials;
};
