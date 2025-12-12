// Rship IES Profile Service
// Downloads, parses, and caches IES photometric profiles from rship asset store

#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureLightProfile.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "RshipIESProfileService.generated.h"

class URshipSubsystem;

/**
 * Parsed IES profile data
 */
USTRUCT(BlueprintType)
struct RSHIPEXEC_API FRshipIESProfile
{
    GENERATED_BODY()

    /** Source URL */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|IES")
    FString Url;

    /** Manufacturer name from IES file */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|IES")
    FString Manufacturer;

    /** Luminaire catalog number */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|IES")
    FString CatalogNumber;

    /** Lamp description */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|IES")
    FString LampDescription;

    /** Number of vertical angles */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|IES")
    int32 NumVerticalAngles = 0;

    /** Number of horizontal angles */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|IES")
    int32 NumHorizontalAngles = 0;

    /** Vertical angles array (degrees) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|IES")
    TArray<float> VerticalAngles;

    /** Horizontal angles array (degrees) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|IES")
    TArray<float> HorizontalAngles;

    /** Candela values [horizontal][vertical] flattened */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|IES")
    TArray<float> CandelaValues;

    /** Peak candela value */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|IES")
    float PeakCandela = 0.0f;

    /** Total lumens */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|IES")
    float TotalLumens = 0.0f;

    /** Beam angle (50% intensity) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|IES")
    float BeamAngle = 0.0f;

    /** Field angle (10% intensity) */
    UPROPERTY(BlueprintReadOnly, Category = "Rship|IES")
    float FieldAngle = 0.0f;

    /** Is this profile valid */
    bool IsValid() const { return NumVerticalAngles > 0 && NumHorizontalAngles > 0; }

    /**
     * Get candela at a specific angle
     * @param VerticalAngle Vertical angle in degrees (0 = down, 180 = up)
     * @param HorizontalAngle Horizontal angle in degrees (0-360)
     * @return Candela value
     */
    float GetCandela(float VerticalAngle, float HorizontalAngle = 0.0f) const;

    /**
     * Get normalized intensity at angle (0-1)
     */
    float GetIntensity(float VerticalAngle, float HorizontalAngle = 0.0f) const;
};

DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnIESProfileLoaded, bool, bSuccess, const FRshipIESProfile&, Profile);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnIESProfileCached, const FString&, Url, const FRshipIESProfile&, Profile);

/**
 * Service for loading and caching IES photometric profiles.
 * Downloads from rship asset store and generates UE light profile textures.
 */
UCLASS(BlueprintType)
class RSHIPEXEC_API URshipIESProfileService : public UObject
{
    GENERATED_BODY()

public:
    /**
     * Initialize the service
     */
    void Initialize(URshipSubsystem* InSubsystem);

    /**
     * Cleanup
     */
    void Shutdown();

    // ========================================================================
    // PROFILE LOADING
    // ========================================================================

    /**
     * Load an IES profile from URL (async)
     * Uses cache if available
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IES")
    void LoadProfile(const FString& Url, const FOnIESProfileLoaded& OnComplete);

    /**
     * Check if a profile is cached
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IES")
    bool IsProfileCached(const FString& Url) const;

    /**
     * Get cached profile (returns false if not cached)
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IES")
    bool GetCachedProfile(const FString& Url, FRshipIESProfile& OutProfile) const;

    /**
     * Clear the cache
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IES")
    void ClearCache();

    /**
     * Get number of cached profiles
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IES")
    int32 GetCacheCount() const { return ProfileCache.Num(); }

    // ========================================================================
    // TEXTURE GENERATION
    // ========================================================================

    /**
     * Generate a 1D light profile texture from IES data
     * This can be used with UE's IES light profile system
     * @param Profile The parsed IES profile
     * @param Resolution Texture resolution (default 256)
     * @return Generated texture or nullptr on failure
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IES")
    UTextureLightProfile* GenerateLightProfileTexture(const FRshipIESProfile& Profile, int32 Resolution = 256);

    /**
     * Generate a 2D lookup texture for custom shaders
     * X = vertical angle (0-180), Y = horizontal angle (0-360)
     * @param Profile The parsed IES profile
     * @param Resolution Texture resolution
     * @return Generated texture or nullptr on failure
     */
    UFUNCTION(BlueprintCallable, Category = "Rship|IES")
    UTexture2D* Generate2DLookupTexture(const FRshipIESProfile& Profile, int32 Resolution = 128);

    // ========================================================================
    // EVENTS
    // ========================================================================

    /** Fired when a profile is cached */
    UPROPERTY(BlueprintAssignable, Category = "Rship|IES")
    FOnIESProfileCached OnProfileCached;

private:
    UPROPERTY()
    URshipSubsystem* Subsystem;

    // Profile cache by URL
    TMap<FString, FRshipIESProfile> ProfileCache;

    // Texture cache by URL
    UPROPERTY()
    TMap<FString, UTextureLightProfile*> TextureCache;

    // Pending requests
    TMap<FString, TArray<FOnIESProfileLoaded>> PendingRequests;

    // Parse IES file content
    bool ParseIESContent(const FString& Content, FRshipIESProfile& OutProfile);

    // HTTP callback
    void OnHttpResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess, FString Url);

    // Calculate beam/field angles from candela data
    void CalculateBeamAngles(FRshipIESProfile& Profile);

    // Get cache directory
    FString GetCacheDirectory() const;

    // Load from disk cache
    bool LoadFromDiskCache(const FString& Url, FRshipIESProfile& OutProfile);

    // Save to disk cache
    void SaveToDiskCache(const FString& Url, const FString& Content);
};
