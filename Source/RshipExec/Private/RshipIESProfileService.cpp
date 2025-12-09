// Rship IES Profile Service Implementation

#include "RshipIESProfileService.h"
#include "RshipSubsystem.h"
#include "Logs.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureLightProfile.h"

// ============================================================================
// FRshipIESProfile Implementation
// ============================================================================

float FRshipIESProfile::GetCandela(float VerticalAngle, float HorizontalAngle) const
{
    if (!IsValid() || CandelaValues.Num() == 0)
    {
        return 0.0f;
    }

    // Clamp angles
    VerticalAngle = FMath::Clamp(VerticalAngle, 0.0f, 180.0f);
    HorizontalAngle = FMath::Fmod(HorizontalAngle, 360.0f);
    if (HorizontalAngle < 0.0f) HorizontalAngle += 360.0f;

    // Find surrounding vertical angles
    int32 VLow = 0, VHigh = 0;
    float VT = 0.0f;

    for (int32 i = 0; i < VerticalAngles.Num() - 1; i++)
    {
        if (VerticalAngle >= VerticalAngles[i] && VerticalAngle <= VerticalAngles[i + 1])
        {
            VLow = i;
            VHigh = i + 1;
            float Range = VerticalAngles[VHigh] - VerticalAngles[VLow];
            VT = (Range > 0.0f) ? (VerticalAngle - VerticalAngles[VLow]) / Range : 0.0f;
            break;
        }
    }

    // Handle edge cases
    if (VerticalAngle <= VerticalAngles[0])
    {
        VLow = VHigh = 0;
        VT = 0.0f;
    }
    else if (VerticalAngle >= VerticalAngles.Last())
    {
        VLow = VHigh = VerticalAngles.Num() - 1;
        VT = 0.0f;
    }

    // Find surrounding horizontal angles
    int32 HLow = 0, HHigh = 0;
    float HT = 0.0f;

    if (NumHorizontalAngles == 1)
    {
        // Symmetric distribution
        HLow = HHigh = 0;
        HT = 0.0f;
    }
    else
    {
        for (int32 i = 0; i < HorizontalAngles.Num() - 1; i++)
        {
            if (HorizontalAngle >= HorizontalAngles[i] && HorizontalAngle <= HorizontalAngles[i + 1])
            {
                HLow = i;
                HHigh = i + 1;
                float Range = HorizontalAngles[HHigh] - HorizontalAngles[HLow];
                HT = (Range > 0.0f) ? (HorizontalAngle - HorizontalAngles[HLow]) / Range : 0.0f;
                break;
            }
        }
    }

    // Bilinear interpolation
    auto GetValue = [this](int32 H, int32 V) -> float
    {
        int32 Index = H * NumVerticalAngles + V;
        return (Index >= 0 && Index < CandelaValues.Num()) ? CandelaValues[Index] : 0.0f;
    };

    float V00 = GetValue(HLow, VLow);
    float V01 = GetValue(HLow, VHigh);
    float V10 = GetValue(HHigh, VLow);
    float V11 = GetValue(HHigh, VHigh);

    float V0 = FMath::Lerp(V00, V01, VT);
    float V1 = FMath::Lerp(V10, V11, VT);

    return FMath::Lerp(V0, V1, HT);
}

float FRshipIESProfile::GetIntensity(float VerticalAngle, float HorizontalAngle) const
{
    if (PeakCandela <= 0.0f)
    {
        return 0.0f;
    }
    return GetCandela(VerticalAngle, HorizontalAngle) / PeakCandela;
}

// ============================================================================
// URshipIESProfileService Implementation
// ============================================================================

void URshipIESProfileService::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;

    // Ensure cache directory exists
    FString CacheDir = GetCacheDirectory();
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*CacheDir))
    {
        PlatformFile.CreateDirectoryTree(*CacheDir);
    }

    UE_LOG(LogRshipExec, Log, TEXT("IESProfileService initialized, cache: %s"), *CacheDir);
}

void URshipIESProfileService::Shutdown()
{
    ProfileCache.Empty();
    TextureCache.Empty();
    PendingRequests.Empty();
    Subsystem = nullptr;

    UE_LOG(LogRshipExec, Log, TEXT("IESProfileService shutdown"));
}

FString URshipIESProfileService::GetCacheDirectory() const
{
    return FPaths::ProjectSavedDir() / TEXT("Rship") / TEXT("IESCache");
}

void URshipIESProfileService::LoadProfile(const FString& Url, const FOnIESProfileLoaded& OnComplete)
{
    if (Url.IsEmpty())
    {
        OnComplete.ExecuteIfBound(false, FRshipIESProfile());
        return;
    }

    // Check memory cache first
    if (FRshipIESProfile* Cached = ProfileCache.Find(Url))
    {
        OnComplete.ExecuteIfBound(true, *Cached);
        return;
    }

    // Check disk cache
    FRshipIESProfile DiskCached;
    if (LoadFromDiskCache(Url, DiskCached))
    {
        ProfileCache.Add(Url, DiskCached);
        OnComplete.ExecuteIfBound(true, DiskCached);
        OnProfileCached.Broadcast(Url, DiskCached);
        return;
    }

    // Check if request is already pending
    if (TArray<FOnIESProfileLoaded>* Pending = PendingRequests.Find(Url))
    {
        Pending->Add(OnComplete);
        return;
    }

    // Start new request
    PendingRequests.Add(Url, { OnComplete });

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Url);
    Request->SetVerb(TEXT("GET"));
    Request->OnProcessRequestComplete().BindUObject(this, &URshipIESProfileService::OnHttpResponseReceived, Url);
    Request->ProcessRequest();

    UE_LOG(LogRshipExec, Log, TEXT("IES: Fetching %s"), *Url);
}

void URshipIESProfileService::OnHttpResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess, FString Url)
{
    FRshipIESProfile Profile;
    bool bParsed = false;

    if (bSuccess && Response.IsValid() && Response->GetResponseCode() == 200)
    {
        FString Content = Response->GetContentAsString();

        if (ParseIESContent(Content, Profile))
        {
            Profile.Url = Url;
            CalculateBeamAngles(Profile);

            // Cache in memory
            ProfileCache.Add(Url, Profile);

            // Cache on disk
            SaveToDiskCache(Url, Content);

            bParsed = true;

            UE_LOG(LogRshipExec, Log, TEXT("IES: Loaded %s (beam=%.1f° field=%.1f° peak=%.0f cd)"),
                *Url, Profile.BeamAngle, Profile.FieldAngle, Profile.PeakCandela);

            OnProfileCached.Broadcast(Url, Profile);
        }
        else
        {
            UE_LOG(LogRshipExec, Warning, TEXT("IES: Failed to parse %s"), *Url);
        }
    }
    else
    {
        UE_LOG(LogRshipExec, Warning, TEXT("IES: Failed to fetch %s (code=%d)"),
            *Url, Response.IsValid() ? Response->GetResponseCode() : 0);
    }

    // Notify all pending callbacks
    if (TArray<FOnIESProfileLoaded>* Pending = PendingRequests.Find(Url))
    {
        for (const FOnIESProfileLoaded& Callback : *Pending)
        {
            Callback.ExecuteIfBound(bParsed, Profile);
        }
        PendingRequests.Remove(Url);
    }
}

bool URshipIESProfileService::ParseIESContent(const FString& Content, FRshipIESProfile& OutProfile)
{
    TArray<FString> Lines;
    Content.ParseIntoArrayLines(Lines);

    if (Lines.Num() < 10)
    {
        return false;
    }

    // Parse IES header
    int32 LineIndex = 0;
    bool bFoundTilt = false;

    // Skip header lines until TILT=
    while (LineIndex < Lines.Num())
    {
        FString Line = Lines[LineIndex].TrimStartAndEnd();

        // Extract metadata from keywords
        if (Line.StartsWith(TEXT("[MANUFAC]")))
        {
            OutProfile.Manufacturer = Line.RightChop(9).TrimStartAndEnd();
        }
        else if (Line.StartsWith(TEXT("[LUMCAT]")))
        {
            OutProfile.CatalogNumber = Line.RightChop(8).TrimStartAndEnd();
        }
        else if (Line.StartsWith(TEXT("[LAMP]")))
        {
            OutProfile.LampDescription = Line.RightChop(6).TrimStartAndEnd();
        }
        else if (Line.StartsWith(TEXT("TILT=")))
        {
            bFoundTilt = true;
            LineIndex++;
            break;
        }

        LineIndex++;
    }

    if (!bFoundTilt || LineIndex >= Lines.Num())
    {
        return false;
    }

    // Collect all remaining data into a single string and parse
    FString DataString;
    for (int32 i = LineIndex; i < Lines.Num(); i++)
    {
        DataString += Lines[i] + TEXT(" ");
    }

    TArray<FString> Values;
    DataString.ParseIntoArrayWS(Values);

    if (Values.Num() < 13)
    {
        return false;
    }

    int32 ValueIndex = 0;

    // Parse lamp data line
    int32 NumLamps = FCString::Atoi(*Values[ValueIndex++]);
    float LumensPerLamp = FCString::Atof(*Values[ValueIndex++]);
    float CandelaMultiplier = FCString::Atof(*Values[ValueIndex++]);
    OutProfile.NumVerticalAngles = FCString::Atoi(*Values[ValueIndex++]);
    OutProfile.NumHorizontalAngles = FCString::Atoi(*Values[ValueIndex++]);
    int32 PhotometricType = FCString::Atoi(*Values[ValueIndex++]);
    int32 UnitsType = FCString::Atoi(*Values[ValueIndex++]);
    float Width = FCString::Atof(*Values[ValueIndex++]);
    float Length = FCString::Atof(*Values[ValueIndex++]);
    float Height = FCString::Atof(*Values[ValueIndex++]);

    // Skip ballast factor, future use, input watts
    ValueIndex += 3;

    // Calculate total lumens
    OutProfile.TotalLumens = NumLamps * LumensPerLamp;

    // Parse vertical angles
    OutProfile.VerticalAngles.Reserve(OutProfile.NumVerticalAngles);
    for (int32 i = 0; i < OutProfile.NumVerticalAngles && ValueIndex < Values.Num(); i++)
    {
        OutProfile.VerticalAngles.Add(FCString::Atof(*Values[ValueIndex++]));
    }

    // Parse horizontal angles
    OutProfile.HorizontalAngles.Reserve(OutProfile.NumHorizontalAngles);
    for (int32 i = 0; i < OutProfile.NumHorizontalAngles && ValueIndex < Values.Num(); i++)
    {
        OutProfile.HorizontalAngles.Add(FCString::Atof(*Values[ValueIndex++]));
    }

    // Parse candela values
    int32 NumCandela = OutProfile.NumVerticalAngles * OutProfile.NumHorizontalAngles;
    OutProfile.CandelaValues.Reserve(NumCandela);
    OutProfile.PeakCandela = 0.0f;

    for (int32 i = 0; i < NumCandela && ValueIndex < Values.Num(); i++)
    {
        float Candela = FCString::Atof(*Values[ValueIndex++]) * CandelaMultiplier;
        OutProfile.CandelaValues.Add(Candela);
        OutProfile.PeakCandela = FMath::Max(OutProfile.PeakCandela, Candela);
    }

    return OutProfile.IsValid();
}

void URshipIESProfileService::CalculateBeamAngles(FRshipIESProfile& Profile)
{
    if (!Profile.IsValid() || Profile.PeakCandela <= 0.0f)
    {
        return;
    }

    float BeamThreshold = Profile.PeakCandela * 0.5f;  // 50% for beam angle
    float FieldThreshold = Profile.PeakCandela * 0.1f; // 10% for field angle

    Profile.BeamAngle = 0.0f;
    Profile.FieldAngle = 0.0f;

    // Sample vertical angles to find beam/field angles
    for (float Angle = 0.0f; Angle <= 90.0f; Angle += 0.5f)
    {
        float Intensity = Profile.GetCandela(Angle, 0.0f);

        if (Profile.BeamAngle == 0.0f && Intensity < BeamThreshold)
        {
            Profile.BeamAngle = Angle * 2.0f; // Full cone angle
        }

        if (Profile.FieldAngle == 0.0f && Intensity < FieldThreshold)
        {
            Profile.FieldAngle = Angle * 2.0f;
            break;
        }
    }

    // Default if not found
    if (Profile.BeamAngle == 0.0f) Profile.BeamAngle = 25.0f;
    if (Profile.FieldAngle == 0.0f) Profile.FieldAngle = Profile.BeamAngle * 1.4f;
}

bool URshipIESProfileService::IsProfileCached(const FString& Url) const
{
    return ProfileCache.Contains(Url);
}

bool URshipIESProfileService::GetCachedProfile(const FString& Url, FRshipIESProfile& OutProfile) const
{
    if (const FRshipIESProfile* Cached = ProfileCache.Find(Url))
    {
        OutProfile = *Cached;
        return true;
    }
    return false;
}

void URshipIESProfileService::ClearCache()
{
    ProfileCache.Empty();
    TextureCache.Empty();
    UE_LOG(LogRshipExec, Log, TEXT("IES cache cleared"));
}

bool URshipIESProfileService::LoadFromDiskCache(const FString& Url, FRshipIESProfile& OutProfile)
{
    // Generate cache filename from URL hash
    FString CacheFile = GetCacheDirectory() / FString::Printf(TEXT("%08X.ies"), GetTypeHash(Url));

    FString Content;
    if (FFileHelper::LoadFileToString(Content, *CacheFile))
    {
        if (ParseIESContent(Content, OutProfile))
        {
            OutProfile.Url = Url;
            CalculateBeamAngles(OutProfile);
            return true;
        }
    }
    return false;
}

void URshipIESProfileService::SaveToDiskCache(const FString& Url, const FString& Content)
{
    FString CacheFile = GetCacheDirectory() / FString::Printf(TEXT("%08X.ies"), GetTypeHash(Url));
    FFileHelper::SaveStringToFile(Content, *CacheFile);
}

UTextureLightProfile* URshipIESProfileService::GenerateLightProfileTexture(const FRshipIESProfile& Profile, int32 Resolution)
{
    if (!Profile.IsValid())
    {
        return nullptr;
    }

    // Check cache
    if (UTextureLightProfile** Cached = TextureCache.Find(Profile.Url))
    {
        return *Cached;
    }

    // Create new texture
    UTextureLightProfile* Texture = NewObject<UTextureLightProfile>(this);
    if (!Texture)
    {
        return nullptr;
    }

    // Generate 1D profile (vertical angle only, assuming symmetry)
    TArray<uint8> TextureData;
    TextureData.SetNum(Resolution);

    for (int32 i = 0; i < Resolution; i++)
    {
        float Angle = (static_cast<float>(i) / (Resolution - 1)) * 180.0f;
        float Intensity = Profile.GetIntensity(Angle, 0.0f);
        TextureData[i] = static_cast<uint8>(FMath::Clamp(Intensity * 255.0f, 0.0f, 255.0f));
    }

    // Initialize texture
    Texture->Source.Init(Resolution, 1, 1, 1, TSF_G8, TextureData.GetData());
    Texture->UpdateResource();

    // Cache it
    TextureCache.Add(Profile.Url, Texture);

    UE_LOG(LogRshipExec, Log, TEXT("IES: Generated light profile texture for %s"), *Profile.Url);

    return Texture;
}

UTexture2D* URshipIESProfileService::Generate2DLookupTexture(const FRshipIESProfile& Profile, int32 Resolution)
{
    if (!Profile.IsValid())
    {
        return nullptr;
    }

    UTexture2D* Texture = UTexture2D::CreateTransient(Resolution, Resolution, PF_R32_FLOAT);
    if (!Texture)
    {
        return nullptr;
    }

    // Lock texture for writing
    FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
    void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);

    float* FloatData = static_cast<float*>(Data);

    for (int32 Y = 0; Y < Resolution; Y++)
    {
        float HorizontalAngle = (static_cast<float>(Y) / (Resolution - 1)) * 360.0f;

        for (int32 X = 0; X < Resolution; X++)
        {
            float VerticalAngle = (static_cast<float>(X) / (Resolution - 1)) * 180.0f;
            float Intensity = Profile.GetIntensity(VerticalAngle, HorizontalAngle);

            FloatData[Y * Resolution + X] = Intensity;
        }
    }

    Mip.BulkData.Unlock();
    Texture->UpdateResource();

    UE_LOG(LogRshipExec, Log, TEXT("IES: Generated 2D lookup texture for %s (%dx%d)"),
        *Profile.Url, Resolution, Resolution);

    return Texture;
}
