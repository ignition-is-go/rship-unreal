#include "PulseRDGDeformEditorModule.h"

#include "PulseDeformCacheAsset.h"
#include "PulseRDGDeformComponent.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StaticMeshComponent.h"
#include "ContentBrowserMenuContexts.h"
#include "Containers/ScriptArray.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "Math/Float16Color.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "ToolMenus.h"
#include "UObject/ObjectPtr.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "PulseRDGDeformEditor"

namespace
{
struct FLegacyCacheArrays
{
    TArray<FVector> RestPos;
    TArray<FVector> RestNormals;
    TArray<FLinearColor> Colors;
    int32 VCount = 0;
    int32 GridWidth = 0;
    int32 GridHeight = 0;
};

bool ReadIntProperty(const UObject* SourceObject, const FName PropertyName, int32& OutValue)
{
    const FIntProperty* Property = FindFProperty<FIntProperty>(SourceObject->GetClass(), PropertyName);
    if (!Property)
    {
        return false;
    }

    const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(SourceObject);
    OutValue = Property->GetPropertyValue(ValuePtr);
    return true;
}

template <typename T>
bool ReadStructArrayProperty(
    const UObject* SourceObject,
    const FName PropertyName,
    UScriptStruct* ExpectedStruct,
    TArray<T>& OutValues,
    FString& OutError)
{
    const FArrayProperty* ArrayProperty = FindFProperty<FArrayProperty>(SourceObject->GetClass(), PropertyName);
    if (!ArrayProperty)
    {
        OutError = FString::Printf(TEXT("Missing required array property '%s' on %s."), *PropertyName.ToString(), *SourceObject->GetName());
        return false;
    }

    const FStructProperty* StructProperty = CastField<FStructProperty>(ArrayProperty->Inner);
    if (!StructProperty || StructProperty->Struct != ExpectedStruct)
    {
        OutError = FString::Printf(TEXT("Property '%s' on %s has unexpected type."), *PropertyName.ToString(), *SourceObject->GetName());
        return false;
    }

    void* ArrayPtr = ArrayProperty->ContainerPtrToValuePtr<void>(const_cast<UObject*>(SourceObject));
    FScriptArrayHelper Helper(ArrayProperty, ArrayPtr);

    OutValues.SetNumUninitialized(Helper.Num());
    for (int32 Index = 0; Index < Helper.Num(); ++Index)
    {
        OutValues[Index] = *reinterpret_cast<const T*>(Helper.GetRawPtr(Index));
    }

    return true;
}

bool ExtractLegacyArrays(const UObject* SourceObject, FLegacyCacheArrays& OutArrays, FString& OutError)
{
    if (!ReadStructArrayProperty(SourceObject, TEXT("RestPosList"), TBaseStructure<FVector>::Get(), OutArrays.RestPos, OutError))
    {
        return false;
    }

    if (!ReadStructArrayProperty(SourceObject, TEXT("RestNormalList"), TBaseStructure<FVector>::Get(), OutArrays.RestNormals, OutError))
    {
        return false;
    }

    if (!ReadStructArrayProperty(SourceObject, TEXT("ColorList"), TBaseStructure<FLinearColor>::Get(), OutArrays.Colors, OutError))
    {
        return false;
    }

    if (OutArrays.RestPos.Num() == 0)
    {
        OutError = FString::Printf(TEXT("%s has an empty RestPosList."), *SourceObject->GetName());
        return false;
    }

    if (OutArrays.RestNormals.Num() != OutArrays.RestPos.Num() || OutArrays.Colors.Num() != OutArrays.RestPos.Num())
    {
        OutError = FString::Printf(TEXT("%s list lengths are not synchronized (RestPos=%d RestNormal=%d Color=%d)."),
            *SourceObject->GetName(),
            OutArrays.RestPos.Num(),
            OutArrays.RestNormals.Num(),
            OutArrays.Colors.Num());
        return false;
    }

    OutArrays.VCount = OutArrays.RestPos.Num();

    int32 SourceVCount = 0;
    if (ReadIntProperty(SourceObject, TEXT("VCount"), SourceVCount) && SourceVCount > 0 && SourceVCount != OutArrays.VCount)
    {
        OutError = FString::Printf(TEXT("%s VCount (%d) does not match list length (%d)."), *SourceObject->GetName(), SourceVCount, OutArrays.VCount);
        return false;
    }

    const bool bHasGridWidth = ReadIntProperty(SourceObject, TEXT("GridWidth"), OutArrays.GridWidth);
    const bool bHasGridHeight = ReadIntProperty(SourceObject, TEXT("GridHeight"), OutArrays.GridHeight);

    if (bHasGridWidth && bHasGridHeight && OutArrays.GridWidth > 0 && OutArrays.GridHeight > 0 && (OutArrays.GridWidth * OutArrays.GridHeight) >= OutArrays.VCount)
    {
        return true;
    }

    const int32 InferredWidth = FMath::CeilToInt(FMath::Sqrt(static_cast<double>(OutArrays.VCount)));
    const int32 InferredHeight = FMath::DivideAndRoundUp(OutArrays.VCount, InferredWidth);

    OutArrays.GridWidth = FMath::Max(InferredWidth, 1);
    OutArrays.GridHeight = FMath::Max(InferredHeight, 1);
    return true;
}

UTexture2D* CreateOrUpdateRGBA16FTexture(
    const FString& FolderPath,
    const FString& AssetName,
    int32 Width,
    int32 Height,
    const TArray<FFloat16Color>& PixelData)
{
    const FString PackageName = FolderPath / AssetName;
    const FString ObjectPath = PackageName + TEXT(".") + AssetName;

    UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *ObjectPath);
    if (!Texture)
    {
        UPackage* Package = CreatePackage(*PackageName);
        Texture = NewObject<UTexture2D>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
        FAssetRegistryModule::AssetCreated(Texture);
    }

    Texture->Modify();
    Texture->Source.Init(Width, Height, 1, 1, TSF_RGBA16F, reinterpret_cast<const uint8*>(PixelData.GetData()));
    Texture->SRGB = false;
    Texture->MipGenSettings = TMGS_NoMipmaps;
    Texture->CompressionSettings = TC_HDR;
    Texture->Filter = TF_Nearest;
    Texture->AddressX = TA_Clamp;
    Texture->AddressY = TA_Clamp;
    Texture->NeverStream = true;
    Texture->MarkPackageDirty();
    Texture->PostEditChange();

    return Texture;
}

UTexture2D* CreateOrUpdateMaskTexture(
    const FString& FolderPath,
    const FString& AssetName,
    int32 Width,
    int32 Height,
    const TArray<uint8>& PixelData)
{
    const FString PackageName = FolderPath / AssetName;
    const FString ObjectPath = PackageName + TEXT(".") + AssetName;

    UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *ObjectPath);
    if (!Texture)
    {
        UPackage* Package = CreatePackage(*PackageName);
        Texture = NewObject<UTexture2D>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
        FAssetRegistryModule::AssetCreated(Texture);
    }

    Texture->Modify();
    Texture->Source.Init(Width, Height, 1, 1, TSF_G8, PixelData.GetData());
    Texture->SRGB = false;
    Texture->MipGenSettings = TMGS_NoMipmaps;
    Texture->CompressionSettings = TC_Grayscale;
    Texture->Filter = TF_Nearest;
    Texture->AddressX = TA_Clamp;
    Texture->AddressY = TA_Clamp;
    Texture->NeverStream = true;
    Texture->MarkPackageDirty();
    Texture->PostEditChange();

    return Texture;
}

UPulseDeformCacheAsset* CreateOrLoadCacheAsset(const FString& FolderPath, const FString& AssetName)
{
    const FString PackageName = FolderPath / AssetName;
    const FString ObjectPath = PackageName + TEXT(".") + AssetName;

    UPulseDeformCacheAsset* CacheAsset = LoadObject<UPulseDeformCacheAsset>(nullptr, *ObjectPath);
    if (CacheAsset)
    {
        return CacheAsset;
    }

    UPackage* Package = CreatePackage(*PackageName);
    CacheAsset = NewObject<UPulseDeformCacheAsset>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
    FAssetRegistryModule::AssetCreated(CacheAsset);
    return CacheAsset;
}

bool BakeLegacyAssetToRDGCache(const UObject* SourceObject, FString& OutInfo, FString& OutError)
{
    FLegacyCacheArrays Arrays;
    if (!ExtractLegacyArrays(SourceObject, Arrays, OutError))
    {
        return false;
    }

    const FString SourcePackagePath = SourceObject->GetPackage()->GetName();
    const FString FolderPath = FPackageName::GetLongPackagePath(SourcePackagePath);
    const FString BaseName = SourceObject->GetName();

    const int32 GridWidth = Arrays.GridWidth;
    const int32 GridHeight = Arrays.GridHeight;
    const int32 PixelCount = GridWidth * GridHeight;

    TArray<FFloat16Color> RestPosPixels;
    TArray<FFloat16Color> RestNormalPixels;
    TArray<uint8> MaskPixels;

    RestPosPixels.SetNumZeroed(PixelCount);
    RestNormalPixels.SetNumZeroed(PixelCount);
    MaskPixels.SetNumZeroed(PixelCount);

    for (int32 Index = 0; Index < Arrays.VCount; ++Index)
    {
        const FVector& RestPos = Arrays.RestPos[Index];
        const FVector RestNormal = Arrays.RestNormals[Index].GetSafeNormal(KINDA_SMALL_NUMBER, FVector::UpVector);
        const float Mask = FMath::Clamp(Arrays.Colors[Index].R, 0.0f, 1.0f);

        RestPosPixels[Index] = FFloat16Color(FLinearColor(
            static_cast<float>(RestPos.X),
            static_cast<float>(RestPos.Y),
            static_cast<float>(RestPos.Z),
            1.0f));

        RestNormalPixels[Index] = FFloat16Color(FLinearColor(
            static_cast<float>(RestNormal.X),
            static_cast<float>(RestNormal.Y),
            static_cast<float>(RestNormal.Z),
            1.0f));

        MaskPixels[Index] = static_cast<uint8>(FMath::RoundToInt(Mask * 255.0f));
    }

    UTexture2D* RestPosTexture = CreateOrUpdateRGBA16FTexture(FolderPath, FString::Printf(TEXT("T_PulseRestPos_%s"), *BaseName), GridWidth, GridHeight, RestPosPixels);
    UTexture2D* RestNormalTexture = CreateOrUpdateRGBA16FTexture(FolderPath, FString::Printf(TEXT("T_PulseRestNrm_%s"), *BaseName), GridWidth, GridHeight, RestNormalPixels);
    UTexture2D* MaskTexture = CreateOrUpdateMaskTexture(FolderPath, FString::Printf(TEXT("T_PulseMask_%s"), *BaseName), GridWidth, GridHeight, MaskPixels);

    if (!RestPosTexture || !RestNormalTexture || !MaskTexture)
    {
        OutError = FString::Printf(TEXT("Failed to create one or more baked textures for %s."), *BaseName);
        return false;
    }

    UPulseDeformCacheAsset* CacheAsset = CreateOrLoadCacheAsset(FolderPath, FString::Printf(TEXT("DA_PulseRDGCache_%s"), *BaseName));
    if (!CacheAsset)
    {
        OutError = FString::Printf(TEXT("Failed to create cache asset for %s."), *BaseName);
        return false;
    }

    CacheAsset->Modify();
    CacheAsset->GridWidth = GridWidth;
    CacheAsset->GridHeight = GridHeight;
    CacheAsset->RestPositionTexture = RestPosTexture;
    CacheAsset->RestNormalTexture = RestNormalTexture;
    CacheAsset->MaskTexture = MaskTexture;
    CacheAsset->MarkPackageDirty();

    OutInfo = FString::Printf(TEXT("%s -> %s (%dx%d, VCount=%d)"), *BaseName, *CacheAsset->GetName(), GridWidth, GridHeight, Arrays.VCount);
    return true;
}

void CollectSelectedPulseComponents(TSet<UPulseRDGDeformComponent*>& OutComponents, bool bCreateMissingOnActorSelection)
{
    if (!GEditor)
    {
        return;
    }

    if (USelection* SelectedActors = GEditor->GetSelectedActors())
    {
        for (FSelectionIterator It(*SelectedActors); It; ++It)
        {
            AActor* Actor = Cast<AActor>(*It);
            if (!Actor)
            {
                continue;
            }

            TArray<UPulseRDGDeformComponent*> Components;
            Actor->GetComponents<UPulseRDGDeformComponent>(Components);

            if (Components.IsEmpty() && bCreateMissingOnActorSelection)
            {
                Actor->Modify();
                UPulseRDGDeformComponent* NewComponent = NewObject<UPulseRDGDeformComponent>(Actor, NAME_None, RF_Transactional);
                Actor->AddInstanceComponent(NewComponent);
                NewComponent->RegisterComponent();

                if (UStaticMeshComponent* StaticMeshComp = Actor->FindComponentByClass<UStaticMeshComponent>())
                {
                    NewComponent->TargetMeshComponent = StaticMeshComp;
                }

                Components.Add(NewComponent);
            }

            for (UPulseRDGDeformComponent* Component : Components)
            {
                OutComponents.Add(Component);
            }
        }
    }

    if (USelection* SelectedComponents = GEditor->GetSelectedComponents())
    {
        for (FSelectionIterator It(*SelectedComponents); It; ++It)
        {
            if (UPulseRDGDeformComponent* DeformComponent = Cast<UPulseRDGDeformComponent>(*It))
            {
                OutComponents.Add(DeformComponent);
                continue;
            }

            if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(*It))
            {
                AActor* Owner = StaticMeshComp->GetOwner();
                if (!Owner)
                {
                    continue;
                }

                UPulseRDGDeformComponent* DeformComponent = Owner->FindComponentByClass<UPulseRDGDeformComponent>();
                if (!DeformComponent)
                {
                    Owner->Modify();
                    DeformComponent = NewObject<UPulseRDGDeformComponent>(Owner, NAME_None, RF_Transactional);
                    Owner->AddInstanceComponent(DeformComponent);
                    DeformComponent->RegisterComponent();
                }

                DeformComponent->TargetMeshComponent = StaticMeshComp;
                OutComponents.Add(DeformComponent);
            }
        }
    }
}

bool ApplyCacheToComponent(UPulseRDGDeformComponent* Component, UPulseDeformCacheAsset* CacheAsset, FString& OutError)
{
    if (!Component || !CacheAsset)
    {
        OutError = TEXT("Null component or cache asset.");
        return false;
    }

    Component->Modify();
    Component->DeformCache = CacheAsset;

    if (!Component->TargetMeshComponent)
    {
        if (AActor* Owner = Component->GetOwner())
        {
            if (UStaticMeshComponent* StaticMeshComp = Owner->FindComponentByClass<UStaticMeshComponent>())
            {
                Component->TargetMeshComponent = StaticMeshComp;
            }
        }
    }

    Component->MarkPackageDirty();
    if (AActor* Owner = Component->GetOwner())
    {
        Owner->MarkPackageDirty();
    }

    return true;
}

UPulseDeformCacheAsset* FindGeneratedCacheForMesh(UStaticMesh* StaticMesh)
{
    if (!StaticMesh)
    {
        return nullptr;
    }

    const FString MeshPackagePath = StaticMesh->GetPackage()->GetName();
    const FString MeshFolder = FPackageName::GetLongPackagePath(MeshPackagePath);
    const FString CacheAssetName = FString::Printf(TEXT("DA_PulseRDGCache_%s"), *StaticMesh->GetName());
    const FString PreferredObjectPath = (MeshFolder / CacheAssetName) + TEXT(".") + CacheAssetName;

    if (UPulseDeformCacheAsset* ExactMatch = LoadObject<UPulseDeformCacheAsset>(nullptr, *PreferredObjectPath))
    {
        return ExactMatch;
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    FARFilter Filter;
    Filter.ClassPaths.Add(UPulseDeformCacheAsset::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;
    Filter.bRecursivePaths = true;
    Filter.PackagePaths.Add(*MeshFolder);

    TArray<FAssetData> Assets;
    AssetRegistryModule.Get().GetAssets(Filter, Assets);
    for (const FAssetData& Asset : Assets)
    {
        if (Asset.AssetName == FName(CacheAssetName))
        {
            return Cast<UPulseDeformCacheAsset>(Asset.GetAsset());
        }
    }

    return nullptr;
}
}

void FPulseRDGDeformEditorModule::StartupModule()
{
    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FPulseRDGDeformEditorModule::RegisterMenus));
}

void FPulseRDGDeformEditorModule::ShutdownModule()
{
    if (UToolMenus::TryGet())
    {
        UToolMenus::UnRegisterStartupCallback(this);
        UToolMenus::UnregisterOwner(this);
    }
}

void FPulseRDGDeformEditorModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.AssetActionsSubMenu");
    Menu->AddDynamicSection(NAME_None, FNewToolMenuDelegate::CreateLambda([this](UToolMenu* InMenu)
    {
        const UContentBrowserAssetContextMenuContext* Context = InMenu->FindContext<UContentBrowserAssetContextMenuContext>();
        if (!Context || !Context->bCanBeModified || Context->SelectedAssets.IsEmpty())
        {
            return;
        }

        FToolMenuSection& Section = InMenu->FindOrAddSection("AssetContextAdvancedActions");
        Section.AddMenuEntry(
            "PulseBakeLegacyCacheToTextures",
            LOCTEXT("BakeLegacyCacheLabel", "Bake Pulse Legacy Cache To Textures"),
            LOCTEXT("BakeLegacyCacheTooltip", "Bakes RestPosList/RestNormalList/ColorList arrays into RestPosition/RestNormal/Mask textures and creates a Pulse RDG cache asset."),
            FSlateIcon(),
            FToolMenuExecuteAction::CreateRaw(this, &FPulseRDGDeformEditorModule::ExecuteBakeFromLegacyCache));

        Section.AddMenuEntry(
            "PulseApplySelectedCacheToActors",
            LOCTEXT("ApplySelectedCacheLabel", "Apply Pulse RDG Cache To Selected Actors"),
            LOCTEXT("ApplySelectedCacheTooltip", "Applies a selected Pulse RDG cache asset to selected actors/components with Pulse deform components."),
            FSlateIcon(),
            FToolMenuExecuteAction::CreateRaw(this, &FPulseRDGDeformEditorModule::ExecuteApplySelectedCacheToActors));
    }));

    UToolMenu* ActorMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.ActorContextMenu");
    FToolMenuSection& ActorSection = ActorMenu->FindOrAddSection("ActorTypeTools");
    ActorSection.AddMenuEntry(
        "PulseAutoApplyGeneratedCaches",
        LOCTEXT("AutoApplyCachesLabel", "Auto-Apply Generated Pulse Caches"),
        LOCTEXT("AutoApplyCachesTooltip", "Finds DA_PulseRDGCache_<MeshName> and applies it to selected actors/components."),
        FSlateIcon(),
        FToolMenuExecuteAction::CreateRaw(this, &FPulseRDGDeformEditorModule::ExecuteAutoApplyGeneratedCaches));
}

void FPulseRDGDeformEditorModule::ExecuteBakeFromLegacyCache(const FToolMenuContext& InContext) const
{
    const UContentBrowserAssetContextMenuContext* Context = InContext.FindContext<UContentBrowserAssetContextMenuContext>();
    if (!Context)
    {
        return;
    }

    const TArray<UObject*> SelectedObjects = Context->LoadSelectedObjects<UObject>();
    if (SelectedObjects.IsEmpty())
    {
        return;
    }

    int32 SuccessCount = 0;
    int32 FailCount = 0;
    TArray<FString> SuccessMessages;
    TArray<FString> FailureMessages;

    for (const UObject* Object : SelectedObjects)
    {
        if (!Object)
        {
            ++FailCount;
            FailureMessages.Add(TEXT("(null object)"));
            continue;
        }

        FString Info;
        FString Error;
        if (BakeLegacyAssetToRDGCache(Object, Info, Error))
        {
            ++SuccessCount;
            SuccessMessages.Add(Info);
        }
        else
        {
            ++FailCount;
            FailureMessages.Add(FString::Printf(TEXT("%s: %s"), *Object->GetName(), *Error));
        }
    }

    FString Message = FString::Printf(TEXT("Pulse bake finished. Success: %d, Failed: %d\n\n"), SuccessCount, FailCount);
    if (SuccessMessages.Num() > 0)
    {
        Message += TEXT("Successful:\n");
        for (const FString& Line : SuccessMessages)
        {
            Message += TEXT(" - ") + Line + TEXT("\n");
        }
    }

    if (FailureMessages.Num() > 0)
    {
        Message += TEXT("\nFailed:\n");
        for (const FString& Line : FailureMessages)
        {
            Message += TEXT(" - ") + Line + TEXT("\n");
        }
    }

    FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Message));
}

void FPulseRDGDeformEditorModule::ExecuteApplySelectedCacheToActors(const FToolMenuContext& InContext) const
{
    const UContentBrowserAssetContextMenuContext* Context = InContext.FindContext<UContentBrowserAssetContextMenuContext>();
    if (!Context)
    {
        return;
    }

    const TArray<UPulseDeformCacheAsset*> SelectedCaches = Context->LoadSelectedObjects<UPulseDeformCacheAsset>();
    if (SelectedCaches.Num() != 1 || !SelectedCaches[0])
    {
        FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("SelectSingleCache", "Select exactly one UPulseDeformCacheAsset to apply."));
        return;
    }

    TSet<UPulseRDGDeformComponent*> Components;
    CollectSelectedPulseComponents(Components, true);

    if (Components.IsEmpty())
    {
        FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoTargetsForCache", "No target actors/components selected. Select actors or components in the level first."));
        return;
    }

    int32 AppliedCount = 0;
    TArray<FString> Failures;

    for (UPulseRDGDeformComponent* Component : Components)
    {
        FString Error;
        if (ApplyCacheToComponent(Component, SelectedCaches[0], Error))
        {
            ++AppliedCount;
        }
        else
        {
            const FString OwnerName = Component && Component->GetOwner() ? Component->GetOwner()->GetName() : TEXT("(unknown)");
            Failures.Add(FString::Printf(TEXT("%s: %s"), *OwnerName, *Error));
        }
    }

    FString Message = FString::Printf(TEXT("Applied cache %s to %d component(s)."), *SelectedCaches[0]->GetName(), AppliedCount);
    if (Failures.Num() > 0)
    {
        Message += TEXT("\n\nFailures:\n");
        for (const FString& Failure : Failures)
        {
            Message += TEXT(" - ") + Failure + TEXT("\n");
        }
    }

    FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Message));
}

void FPulseRDGDeformEditorModule::ExecuteAutoApplyGeneratedCaches(const FToolMenuContext& InContext) const
{
    (void)InContext;

    TSet<UPulseRDGDeformComponent*> Components;
    CollectSelectedPulseComponents(Components, true);

    if (Components.IsEmpty())
    {
        FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoSelectedTargetsAuto", "No target actors/components selected."));
        return;
    }

    int32 AppliedCount = 0;
    TArray<FString> Failures;

    for (UPulseRDGDeformComponent* Component : Components)
    {
        UStaticMesh* StaticMesh = nullptr;
        if (Component->TargetMeshComponent)
        {
            StaticMesh = Component->TargetMeshComponent->GetStaticMesh();
        }

        if (!StaticMesh && Component->GetOwner())
        {
            if (UStaticMeshComponent* OwnerMeshComponent = Component->GetOwner()->FindComponentByClass<UStaticMeshComponent>())
            {
                Component->TargetMeshComponent = OwnerMeshComponent;
                StaticMesh = OwnerMeshComponent->GetStaticMesh();
            }
        }

        if (!StaticMesh)
        {
            const FString OwnerName = Component->GetOwner() ? Component->GetOwner()->GetName() : TEXT("(unknown)");
            Failures.Add(FString::Printf(TEXT("%s: no static mesh found for cache lookup."), *OwnerName));
            continue;
        }

        UPulseDeformCacheAsset* CacheAsset = FindGeneratedCacheForMesh(StaticMesh);
        if (!CacheAsset)
        {
            const FString OwnerName = Component->GetOwner() ? Component->GetOwner()->GetName() : TEXT("(unknown)");
            Failures.Add(FString::Printf(TEXT("%s: no generated cache found for mesh %s."), *OwnerName, *StaticMesh->GetName()));
            continue;
        }

        FString Error;
        if (ApplyCacheToComponent(Component, CacheAsset, Error))
        {
            ++AppliedCount;
        }
        else
        {
            const FString OwnerName = Component->GetOwner() ? Component->GetOwner()->GetName() : TEXT("(unknown)");
            Failures.Add(FString::Printf(TEXT("%s: %s"), *OwnerName, *Error));
        }
    }

    FString Message = FString::Printf(TEXT("Auto-apply complete. Applied %d cache assignment(s)."), AppliedCount);
    if (Failures.Num() > 0)
    {
        Message += TEXT("\n\nIssues:\n");
        for (const FString& Failure : Failures)
        {
            Message += TEXT(" - ") + Failure + TEXT("\n");
        }
    }

    FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Message));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPulseRDGDeformEditorModule, PulseRDGDeformEditor)
