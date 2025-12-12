// Rship Fixture Library Implementation

#include "RshipFixtureLibrary.h"
#include "RshipSubsystem.h"
#include "RshipFixtureManager.h"
#include "Logs.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformFileManager.h"

void URshipFixtureLibrary::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;
    LoadLibrary();
    UE_LOG(LogRshipExec, Log, TEXT("FixtureLibrary initialized with %d profiles"), Profiles.Num());
}

void URshipFixtureLibrary::Shutdown()
{
    SaveLibrary();
    Profiles.Empty();
    Subsystem = nullptr;
}

TArray<FRshipFixtureProfile> URshipFixtureLibrary::GetAllProfiles() const { TArray<FRshipFixtureProfile> R; Profiles.GenerateValueArray(R); return R; }

bool URshipFixtureLibrary::GetProfile(const FString& ProfileId, FRshipFixtureProfile& OutProfile) const
{
    const FRshipFixtureProfile* F = Profiles.Find(ProfileId);
    if (F) { OutProfile = *F; return true; }
    return false;
}

TArray<FRshipFixtureProfile> URshipFixtureLibrary::GetProfilesByManufacturer(const FString& Manufacturer) const
{
    TArray<FRshipFixtureProfile> R;
    for (const auto& P : Profiles) if (P.Value.Manufacturer.Equals(Manufacturer, ESearchCase::IgnoreCase)) R.Add(P.Value);
    return R;
}

TArray<FRshipFixtureProfile> URshipFixtureLibrary::GetProfilesByCategory(ERshipFixtureCategory Category) const
{
    TArray<FRshipFixtureProfile> R;
    for (const auto& P : Profiles) if (P.Value.Category == Category) R.Add(P.Value);
    return R;
}

TArray<FRshipFixtureProfile> URshipFixtureLibrary::GetProfilesByTag(const FString& Tag) const
{
    TArray<FRshipFixtureProfile> R;
    for (const auto& P : Profiles) if (P.Value.Tags.Contains(Tag)) R.Add(P.Value);
    return R;
}

TArray<FRshipFixtureProfile> URshipFixtureLibrary::SearchProfiles(const FString& SearchText) const
{
    TArray<FRshipFixtureProfile> R;
    FString L = SearchText.ToLower();
    for (const auto& P : Profiles)
        if (P.Value.DisplayName.ToLower().Contains(L) || P.Value.Manufacturer.ToLower().Contains(L) || P.Value.Model.ToLower().Contains(L))
            R.Add(P.Value);
    return R;
}

TArray<FString> URshipFixtureLibrary::GetManufacturers() const
{
    TSet<FString> S; for (const auto& P : Profiles) S.Add(P.Value.Manufacturer);
    TArray<FString> R = S.Array(); R.Sort(); return R;
}

void URshipFixtureLibrary::AddProfile(const FRshipFixtureProfile& Profile)
{
    FRshipFixtureProfile N = Profile;
    if (N.Id.IsEmpty()) { N.Id = FString::Printf(TEXT("%s_%s"), *N.Manufacturer, *N.Model); N.Id = N.Id.Replace(TEXT(" "), TEXT("_")); }
    if (N.DisplayName.IsEmpty()) N.DisplayName = FString::Printf(TEXT("%s %s"), *N.Manufacturer, *N.Model);
    N.LastUpdated = FDateTime::Now();
    Profiles.Add(N.Id, N);
    OnProfileLoaded.Broadcast(N);
    OnLibraryUpdated.Broadcast(Profiles.Num());
}

bool URshipFixtureLibrary::RemoveProfile(const FString& ProfileId)
{
    if (Profiles.Remove(ProfileId) > 0) { OnLibraryUpdated.Broadcast(Profiles.Num()); return true; }
    return false;
}

FRshipFixtureProfile URshipFixtureLibrary::CreateProfileFromFixture(const FString& FixtureId)
{
    FRshipFixtureProfile P;
    if (!Subsystem) return P;
    URshipFixtureManager* FM = Subsystem->GetFixtureManager();
    if (!FM) return P;
    FRshipFixtureInfo Info;
    if (!FM->GetFixtureById(FixtureId, Info)) return P;
    P.Id = FString::Printf(TEXT("custom_%s"), *FixtureId);
    P.DisplayName = Info.Name; P.Manufacturer = TEXT("Custom"); P.Model = Info.Name; P.Source = TEXT("UE_Reverse");
    FRshipFixtureCalibration Cal;
    if (FM->GetCalibrationForFixture(FixtureId, Cal)) { P.DefaultCalibration = Cal; P.BeamProfile.BeamAngleMin = 25.0f * Cal.BeamAngleMultiplier; P.BeamProfile.FieldAngleMin = 35.0f * Cal.FieldAngleMultiplier; }
    P.LastUpdated = FDateTime::Now();
    return P;
}

bool URshipFixtureLibrary::ImportGDTF(const FString& FilePath, FRshipFixtureProfile& OutProfile, FString& OutError)
{
    if (!FPaths::FileExists(FilePath)) { OutError = TEXT("File not found"); return false; }
    FString FN = FPaths::GetBaseFilename(FilePath);
    TArray<FString> Parts; FN.ParseIntoArray(Parts, TEXT("@"));
    if (Parts.Num() >= 2) { OutProfile.Manufacturer = Parts[0]; OutProfile.Model = Parts[1]; if (Parts.Num() >= 3) OutProfile.Revision = Parts[2]; }
    else { OutProfile.Manufacturer = TEXT("Unknown"); OutProfile.Model = FN; }
    OutProfile.Id = FString::Printf(TEXT("gdtf_%s_%s"), *OutProfile.Manufacturer, *OutProfile.Model).Replace(TEXT(" "), TEXT("_"));
    OutProfile.DisplayName = FString::Printf(TEXT("%s %s"), *OutProfile.Manufacturer, *OutProfile.Model);
    OutProfile.GDTFId = FN; OutProfile.Source = TEXT("GDTF"); OutProfile.LastUpdated = FDateTime::Now();
    AddProfile(OutProfile);
    OnGDTFImportComplete.Broadcast(true, TEXT(""));
    return true;
}

int32 URshipFixtureLibrary::ImportGDTFDirectory(const FString& DirectoryPath)
{
    int32 Count = 0;
    TArray<FString> Files; IFileManager::Get().FindFiles(Files, *FPaths::Combine(DirectoryPath, TEXT("*.gdtf")), true, false);
    for (const FString& F : Files) { FRshipFixtureProfile P; FString E; if (ImportGDTF(FPaths::Combine(DirectoryPath, F), P, E)) Count++; }
    return Count;
}

void URshipFixtureLibrary::DownloadGDTF(const FString& FixtureId) { UE_LOG(LogRshipExec, Log, TEXT("GDTF download: %s"), *FixtureId); }
void URshipFixtureLibrary::SyncWithServer() { UE_LOG(LogRshipExec, Log, TEXT("FixtureLibrary sync requested")); }
void URshipFixtureLibrary::UploadProfile(const FString& ProfileId) { UE_LOG(LogRshipExec, Log, TEXT("Upload profile: %s"), *ProfileId); }
void URshipFixtureLibrary::DownloadProfile(const FString& ProfileId) { UE_LOG(LogRshipExec, Log, TEXT("Download profile: %s"), *ProfileId); }

FString URshipFixtureLibrary::GetLibraryPath() const { return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Rship"), TEXT("FixtureLibrary.json")); }

bool URshipFixtureLibrary::SaveLibrary()
{
    FString Path = GetLibraryPath();
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    TArray<TSharedPtr<FJsonValue>> Arr;
    for (const auto& P : Profiles) Arr.Add(MakeShareable(new FJsonValueObject(ProfileToJson(P.Value))));
    TSharedRef<FJsonObject> Root = MakeShareable(new FJsonObject);
    Root->SetArrayField(TEXT("profiles"), Arr);
    FString Json; TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Json);
    if (!FJsonSerializer::Serialize(Root, W)) return false;
    return FFileHelper::SaveStringToFile(Json, *Path);
}

bool URshipFixtureLibrary::LoadLibrary()
{
    FString Path = GetLibraryPath();
    if (!FPaths::FileExists(Path)) return false;
    FString Json; if (!FFileHelper::LoadFileToString(Json, *Path)) return false;
    TSharedPtr<FJsonObject> Root; TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(R, Root) || !Root.IsValid()) return false;
    Profiles.Empty();
    const TArray<TSharedPtr<FJsonValue>>* Arr;
    if (Root->TryGetArrayField(TEXT("profiles"), Arr))
        for (const auto& V : *Arr) { auto O = V->AsObject(); if (O.IsValid()) { auto P = JsonToProfile(O); if (!P.Id.IsEmpty()) Profiles.Add(P.Id, P); } }
    OnLibraryUpdated.Broadcast(Profiles.Num());
    return true;
}

TSharedPtr<FJsonObject> URshipFixtureLibrary::ProfileToJson(const FRshipFixtureProfile& P) const
{
    TSharedPtr<FJsonObject> O = MakeShareable(new FJsonObject);
    O->SetStringField(TEXT("id"), P.Id); O->SetStringField(TEXT("manufacturer"), P.Manufacturer);
    O->SetStringField(TEXT("model"), P.Model); O->SetStringField(TEXT("displayName"), P.DisplayName);
    O->SetNumberField(TEXT("category"), (int32)P.Category); O->SetStringField(TEXT("source"), P.Source);
    O->SetNumberField(TEXT("wattage"), P.Wattage); O->SetNumberField(TEXT("lumensOutput"), P.LumensOutput);
    TSharedPtr<FJsonObject> B = MakeShareable(new FJsonObject);
    B->SetNumberField(TEXT("beamAngleMin"), P.BeamProfile.BeamAngleMin); B->SetNumberField(TEXT("fieldAngleMin"), P.BeamProfile.FieldAngleMin);
    B->SetBoolField(TEXT("hasZoom"), P.BeamProfile.bHasZoom);
    O->SetObjectField(TEXT("beamProfile"), B);
    TArray<TSharedPtr<FJsonValue>> Tags; for (const auto& T : P.Tags) Tags.Add(MakeShareable(new FJsonValueString(T)));
    O->SetArrayField(TEXT("tags"), Tags);
    return O;
}

FRshipFixtureProfile URshipFixtureLibrary::JsonToProfile(const TSharedPtr<FJsonObject>& J) const
{
    FRshipFixtureProfile P;
    if (!J.IsValid()) return P;
    P.Id = J->GetStringField(TEXT("id")); P.Manufacturer = J->GetStringField(TEXT("manufacturer"));
    P.Model = J->GetStringField(TEXT("model")); P.DisplayName = J->GetStringField(TEXT("displayName"));
    int32 Cat = 0; J->TryGetNumberField(TEXT("category"), Cat); P.Category = (ERshipFixtureCategory)Cat;
    P.Source = J->GetStringField(TEXT("source"));
    J->TryGetNumberField(TEXT("wattage"), P.Wattage); J->TryGetNumberField(TEXT("lumensOutput"), P.LumensOutput);
    const TSharedPtr<FJsonObject>* B;
    if (J->TryGetObjectField(TEXT("beamProfile"), B)) { (*B)->TryGetNumberField(TEXT("beamAngleMin"), P.BeamProfile.BeamAngleMin); (*B)->TryGetBoolField(TEXT("hasZoom"), P.BeamProfile.bHasZoom); }
    const TArray<TSharedPtr<FJsonValue>>* Tags;
    if (J->TryGetArrayField(TEXT("tags"), Tags)) for (const auto& T : *Tags) P.Tags.Add(T->AsString());
    return P;
}

void URshipFixtureLibrary::ProcessProfileEvent(const TSharedPtr<FJsonObject>& Data, bool bIsDelete)
{
    if (!Data.IsValid()) return;
    FString Id = Data->GetStringField(TEXT("id"));
    if (bIsDelete) RemoveProfile(Id); else AddProfile(JsonToProfile(Data));
}
