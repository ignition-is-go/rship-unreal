// Rship Pulse Receiver Implementation

#include "RshipPulseReceiver.h"
#include "RshipSubsystem.h"
#include "RshipFixtureManager.h"
#include "Logs.h"

// ============================================================================
// PULSE DATA HELPERS
// ============================================================================

bool FRshipFixturePulse::GetCustomFloat(const FString& Key, float& OutValue) const
{
    if (!RawData.IsValid())
    {
        return false;
    }

    return RawData->TryGetNumberField(Key, OutValue);
}

bool FRshipFixturePulse::GetCustomString(const FString& Key, FString& OutValue) const
{
    if (!RawData.IsValid())
    {
        return false;
    }

    return RawData->TryGetStringField(Key, OutValue);
}

// ============================================================================
// PULSE RECEIVER
// ============================================================================

void URshipPulseReceiver::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;

    // Build initial mappings
    RebuildFixtureEmitterMappings();

    UE_LOG(LogRshipExec, Log, TEXT("PulseReceiver initialized"));
}

void URshipPulseReceiver::Shutdown()
{
    UnsubscribeFromAll();

    FixtureToEmitter.Empty();
    EmitterToFixture.Empty();
    LastPulses.Empty();
    PulseRates.Empty();

    Subsystem = nullptr;

    UE_LOG(LogRshipExec, Log, TEXT("PulseReceiver shutdown"));
}

// ============================================================================
// SUBSCRIPTION MANAGEMENT
// ============================================================================

void URshipPulseReceiver::SubscribeToFixture(const FString& FixtureId)
{
    if (FixtureId.IsEmpty())
    {
        return;
    }

    SubscribedFixtures.Add(FixtureId);

    // TODO: Send subscription request to server
    // The server needs to know we want to receive pulses for this fixture's emitter
    // This would be a WatchEmitterValue report subscription

    UE_LOG(LogRshipExec, Log, TEXT("PulseReceiver: Subscribed to fixture %s"), *FixtureId);
}

void URshipPulseReceiver::SubscribeToAllFixtures()
{
    bSubscribedToAll = true;

    // Add all known fixtures
    if (Subsystem)
    {
        URshipFixtureManager* FixtureManager = Subsystem->GetFixtureManager();
        if (FixtureManager)
        {
            TArray<FRshipFixtureInfo> Fixtures = FixtureManager->GetAllFixtures();
            for (const FRshipFixtureInfo& Fixture : Fixtures)
            {
                SubscribedFixtures.Add(Fixture.Id);
            }
        }
    }

    UE_LOG(LogRshipExec, Log, TEXT("PulseReceiver: Subscribed to all fixtures (%d)"), SubscribedFixtures.Num());
}

void URshipPulseReceiver::UnsubscribeFromFixture(const FString& FixtureId)
{
    SubscribedFixtures.Remove(FixtureId);
    LastPulses.Remove(FixtureId);
    PulseRates.Remove(FixtureId);

    UE_LOG(LogRshipExec, Log, TEXT("PulseReceiver: Unsubscribed from fixture %s"), *FixtureId);
}

void URshipPulseReceiver::UnsubscribeFromAll()
{
    SubscribedFixtures.Empty();
    bSubscribedToAll = false;
    LastPulses.Empty();
    PulseRates.Empty();

    UE_LOG(LogRshipExec, Log, TEXT("PulseReceiver: Unsubscribed from all fixtures"));
}

bool URshipPulseReceiver::IsSubscribedToFixture(const FString& FixtureId) const
{
    return bSubscribedToAll || SubscribedFixtures.Contains(FixtureId);
}

// ============================================================================
// PULSE QUERIES
// ============================================================================

bool URshipPulseReceiver::GetLastPulse(const FString& FixtureId, FRshipFixturePulse& OutPulse) const
{
    if (const FRshipFixturePulse* Found = LastPulses.Find(FixtureId))
    {
        OutPulse = *Found;
        return true;
    }
    return false;
}

float URshipPulseReceiver::GetPulseRate(const FString& FixtureId) const
{
    if (const FPulseRateTracker* Tracker = PulseRates.Find(FixtureId))
    {
        return Tracker->CachedRate;
    }
    return 0.0f;
}

float URshipPulseReceiver::GetTotalPulseRate() const
{
    float Total = 0.0f;
    for (const auto& Pair : PulseRates)
    {
        Total += Pair.Value.CachedRate;
    }
    return Total;
}

// ============================================================================
// PULSE PROCESSING
// ============================================================================

void URshipPulseReceiver::ProcessPulseEvent(const FString& EmitterId, const TSharedPtr<FJsonObject>& Data)
{
    if (EmitterId.IsEmpty() || !Data.IsValid())
    {
        return;
    }

    // Broadcast raw event for any listeners
    OnEmitterPulseReceived.Broadcast(EmitterId, Data);

    // Find the fixture for this emitter
    FString* FixtureIdPtr = EmitterToFixture.Find(EmitterId);
    if (!FixtureIdPtr)
    {
        // Try to rebuild mappings - maybe the fixture was just registered
        RebuildFixtureEmitterMappings();
        FixtureIdPtr = EmitterToFixture.Find(EmitterId);
    }

    if (!FixtureIdPtr)
    {
        // Not a fixture emitter we know about
        UE_LOG(LogRshipExec, Verbose, TEXT("PulseReceiver: Unknown emitter %s"), *EmitterId);
        return;
    }

    const FString& FixtureId = *FixtureIdPtr;

    // Check if we're subscribed
    if (!IsSubscribedToFixture(FixtureId))
    {
        return;
    }

    // Parse the pulse
    FRshipFixturePulse Pulse = ParsePulse(EmitterId, Data);

    // Store as last pulse
    LastPulses.Add(FixtureId, Pulse);

    // Update rate tracking
    UpdatePulseRate(FixtureId);

    // Broadcast to listeners
    OnFixturePulseReceived.Broadcast(FixtureId, Pulse);

    UE_LOG(LogRshipExec, Verbose, TEXT("PulseReceiver: Received pulse for fixture %s (intensity=%.2f)"),
        *FixtureId, Pulse.Intensity);
}

FRshipFixturePulse URshipPulseReceiver::ParsePulse(const FString& EmitterId, const TSharedPtr<FJsonObject>& Data) const
{
    FRshipFixturePulse Pulse;
    Pulse.EmitterId = EmitterId;
    Pulse.Timestamp = FPlatformTime::Seconds();
    Pulse.RawData = Data;

    // Try to extract common fields from the data
    // The data structure depends on the emitter's schema, so we check for various possibilities

    // ========================================================================
    // INTENSITY
    // ========================================================================

    // Try: intensity, value, dim, dimmer, level, brightness
    double IntensityValue;
    if (Data->TryGetNumberField(TEXT("intensity"), IntensityValue) ||
        Data->TryGetNumberField(TEXT("value"), IntensityValue) ||
        Data->TryGetNumberField(TEXT("dim"), IntensityValue) ||
        Data->TryGetNumberField(TEXT("dimmer"), IntensityValue) ||
        Data->TryGetNumberField(TEXT("level"), IntensityValue) ||
        Data->TryGetNumberField(TEXT("brightness"), IntensityValue))
    {
        Pulse.Intensity = FMath::Clamp((float)IntensityValue, 0.0f, 1.0f);
        Pulse.bHasIntensity = true;
    }

    // ========================================================================
    // COLOR
    // ========================================================================

    // Try RGB as separate fields
    double R, G, B;
    bool bHasR = Data->TryGetNumberField(TEXT("r"), R) || Data->TryGetNumberField(TEXT("red"), R);
    bool bHasG = Data->TryGetNumberField(TEXT("g"), G) || Data->TryGetNumberField(TEXT("green"), G);
    bool bHasB = Data->TryGetNumberField(TEXT("b"), B) || Data->TryGetNumberField(TEXT("blue"), B);

    if (bHasR && bHasG && bHasB)
    {
        Pulse.Color = FLinearColor((float)R, (float)G, (float)B, 1.0f);
        Pulse.bHasColor = true;
    }

    // Try color as hex string
    FString ColorHex;
    if (Data->TryGetStringField(TEXT("color"), ColorHex) ||
        Data->TryGetStringField(TEXT("colour"), ColorHex))
    {
        // Parse hex color (e.g., "#ff0000" or "ff0000")
        FString HexValue = ColorHex.Replace(TEXT("#"), TEXT(""));
        if (HexValue.Len() >= 6)
        {
            int32 RVal = FParse::HexNumber(*HexValue.Mid(0, 2));
            int32 GVal = FParse::HexNumber(*HexValue.Mid(2, 2));
            int32 BVal = FParse::HexNumber(*HexValue.Mid(4, 2));
            Pulse.Color = FLinearColor(RVal / 255.0f, GVal / 255.0f, BVal / 255.0f, 1.0f);
            Pulse.bHasColor = true;
        }
    }

    // Try color as nested object
    const TSharedPtr<FJsonObject>* ColorObj;
    if (Data->TryGetObjectField(TEXT("color"), ColorObj) ||
        Data->TryGetObjectField(TEXT("colour"), ColorObj))
    {
        double CR, CG, CB;
        if ((*ColorObj)->TryGetNumberField(TEXT("r"), CR) &&
            (*ColorObj)->TryGetNumberField(TEXT("g"), CG) &&
            (*ColorObj)->TryGetNumberField(TEXT("b"), CB))
        {
            Pulse.Color = FLinearColor((float)CR, (float)CG, (float)CB, 1.0f);
            Pulse.bHasColor = true;
        }
    }

    // ========================================================================
    // COLOR TEMPERATURE
    // ========================================================================

    double ColorTemp;
    if (Data->TryGetNumberField(TEXT("colorTemperature"), ColorTemp) ||
        Data->TryGetNumberField(TEXT("colorTemp"), ColorTemp) ||
        Data->TryGetNumberField(TEXT("cct"), ColorTemp) ||
        Data->TryGetNumberField(TEXT("kelvin"), ColorTemp) ||
        Data->TryGetNumberField(TEXT("temperature"), ColorTemp))
    {
        Pulse.ColorTemperature = (float)ColorTemp;
        Pulse.bHasColorTemperature = true;
    }

    // ========================================================================
    // BEAM CONTROL
    // ========================================================================

    double ZoomValue;
    if (Data->TryGetNumberField(TEXT("zoom"), ZoomValue) ||
        Data->TryGetNumberField(TEXT("beamAngle"), ZoomValue))
    {
        Pulse.Zoom = FMath::Clamp((float)ZoomValue, 0.0f, 1.0f);
        Pulse.bHasZoom = true;
    }

    double FocusValue;
    if (Data->TryGetNumberField(TEXT("focus"), FocusValue))
    {
        Pulse.Focus = FMath::Clamp((float)FocusValue, 0.0f, 1.0f);
        Pulse.bHasFocus = true;
    }

    double IrisValue;
    if (Data->TryGetNumberField(TEXT("iris"), IrisValue))
    {
        Pulse.Iris = FMath::Clamp((float)IrisValue, 0.0f, 1.0f);
        Pulse.bHasIris = true;
    }

    // ========================================================================
    // POSITION (PAN/TILT)
    // ========================================================================

    double PanValue;
    if (Data->TryGetNumberField(TEXT("pan"), PanValue))
    {
        Pulse.Pan = (float)PanValue;
        Pulse.bHasPan = true;
    }

    double TiltValue;
    if (Data->TryGetNumberField(TEXT("tilt"), TiltValue))
    {
        Pulse.Tilt = (float)TiltValue;
        Pulse.bHasTilt = true;
    }

    // ========================================================================
    // EFFECTS
    // ========================================================================

    double StrobeValue;
    if (Data->TryGetNumberField(TEXT("strobe"), StrobeValue))
    {
        Pulse.Strobe = FMath::Clamp((float)StrobeValue, 0.0f, 1.0f);
        Pulse.bHasStrobe = true;
    }

    double GoboValue;
    if (Data->TryGetNumberField(TEXT("gobo"), GoboValue))
    {
        Pulse.Gobo = (float)GoboValue;
        Pulse.bHasGobo = true;
    }

    double GoboRotValue;
    if (Data->TryGetNumberField(TEXT("goboRotation"), GoboRotValue) ||
        Data->TryGetNumberField(TEXT("goboRot"), GoboRotValue))
    {
        Pulse.GoboRotation = (float)GoboRotValue;
        Pulse.bHasGoboRotation = true;
    }

    bool bPrismValue;
    if (Data->TryGetBoolField(TEXT("prism"), bPrismValue))
    {
        Pulse.bPrism = bPrismValue;
        Pulse.bHasPrism = true;
    }

    return Pulse;
}

void URshipPulseReceiver::UpdatePulseRate(const FString& FixtureId)
{
    double Now = FPlatformTime::Seconds();

    FPulseRateTracker& Tracker = PulseRates.FindOrAdd(FixtureId);
    Tracker.RecentTimestamps.Add(Now);

    // Remove timestamps older than 1 second
    Tracker.RecentTimestamps.RemoveAll([Now](double Timestamp) {
        return (Now - Timestamp) > 1.0;
    });

    // Recalculate rate periodically (not every pulse for performance)
    if (Now - Tracker.LastRateCalcTime > 0.25)
    {
        Tracker.CachedRate = (float)Tracker.RecentTimestamps.Num();
        Tracker.LastRateCalcTime = Now;
    }
}

void URshipPulseReceiver::RebuildFixtureEmitterMappings()
{
    FixtureToEmitter.Empty();
    EmitterToFixture.Empty();

    if (!Subsystem)
    {
        return;
    }

    URshipFixtureManager* FixtureManager = Subsystem->GetFixtureManager();
    if (!FixtureManager)
    {
        return;
    }

    TArray<FRshipFixtureInfo> Fixtures = FixtureManager->GetAllFixtures();
    for (const FRshipFixtureInfo& Fixture : Fixtures)
    {
        if (!Fixture.EmitterId.IsEmpty())
        {
            FixtureToEmitter.Add(Fixture.Id, Fixture.EmitterId);
            EmitterToFixture.Add(Fixture.EmitterId, Fixture.Id);
        }
    }

    UE_LOG(LogRshipExec, Log, TEXT("PulseReceiver: Rebuilt mappings for %d fixtures"), Fixtures.Num());
}
