// Rship Control Rig Binding
// Bind pulse data to Control Rig parameters for procedural animation

#include "RshipControlRigBinding.h"
#include "RshipSubsystem.h"
#include "RshipPulseReceiver.h"
#include "RshipVersion.h"
#include "ControlRig.h"
#include "ControlRigComponent.h"
#include "Rigs/RigHierarchy.h"
#include "Rigs/RigHierarchyElements.h"
#include "Curves/CurveFloat.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/ExpressionParser.h"
#include "Logging/LogMacros.h"

// ============================================================================
// CONTROL RIG BINDING COMPONENT
// ============================================================================

URshipControlRigBinding::URshipControlRigBinding()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    bAutoDiscoverControlRig = true;
    ControlRigComponent = nullptr;
    ControlRig = nullptr;
    Subsystem = nullptr;
}

void URshipControlRigBinding::BeginPlay()
{
    Super::BeginPlay();

    // Get subsystem
    Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();

    // Discover Control Rig
    DiscoverControlRig();

    // Initialize binding states
    BindingStates.SetNum(BindingConfig.Bindings.Num());

    // Bind to pulse receiver
    BindToPulseReceiver();

    // Register with manager
    if (Subsystem)
    {
        URshipControlRigManager* Manager = Subsystem->GetControlRigManager();
        if (Manager)
        {
            Manager->RegisterBinding(this);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("RshipControlRigBinding: Started with %d bindings"), BindingConfig.Bindings.Num());
}

void URshipControlRigBinding::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Unregister from manager
    if (Subsystem)
    {
        URshipControlRigManager* Manager = Subsystem->GetControlRigManager();
        if (Manager)
        {
            Manager->UnregisterBinding(this);
        }
    }

    // Unbind from pulse receiver
    UnbindFromPulseReceiver();

    Super::EndPlay(EndPlayReason);
}

void URshipControlRigBinding::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!BindingConfig.bEnabled || !ControlRig)
    {
        return;
    }

    // Update all bindings
    for (int32 i = 0; i < BindingConfig.Bindings.Num(); ++i)
    {
        if (BindingConfig.Bindings[i].bEnabled)
        {
            UpdateBinding(i, DeltaTime);
            ApplyBindingToControlRig(i);
        }
    }
}

void URshipControlRigBinding::DiscoverControlRig()
{
    if (bAutoDiscoverControlRig)
    {
        AActor* Owner = GetOwner();
        if (Owner)
        {
            ControlRigComponent = Owner->FindComponentByClass<UControlRigComponent>();
        }
    }

    if (ControlRigComponent)
    {
        ControlRig = ControlRigComponent->GetControlRig();
    }
}

void URshipControlRigBinding::BindToPulseReceiver()
{
    if (!Subsystem)
    {
        return;
    }

    URshipPulseReceiver* PulseReceiver = Subsystem->GetPulseReceiver();
    if (!PulseReceiver)
    {
        return;
    }

    // Bind to all emitters referenced in our bindings
    TSet<FString> EmitterIds;
    for (const FRshipControlRigPropertyBinding& Binding : BindingConfig.Bindings)
    {
        if (Binding.bEnabled && !Binding.EmitterId.IsEmpty())
        {
            // For wildcard patterns, we'll check at receive time
            EmitterIds.Add(Binding.EmitterId);
        }
    }

    // Use a general pulse callback
    PulseReceiverHandle = PulseReceiver->OnEmitterPulseReceived.AddLambda(
        [this](const FString& EmitterId, float Intensity, FLinearColor Color, TSharedPtr<FJsonObject> Data)
        {
            OnPulseReceived(EmitterId, Intensity, Color, Data);
        }
    );
}

void URshipControlRigBinding::UnbindFromPulseReceiver()
{
    if (!Subsystem)
    {
        return;
    }

    URshipPulseReceiver* PulseReceiver = Subsystem->GetPulseReceiver();
    if (PulseReceiver && PulseReceiverHandle.IsValid())
    {
        PulseReceiver->OnEmitterPulseReceived.Remove(PulseReceiverHandle);
        PulseReceiverHandle.Reset();
    }
}

void URshipControlRigBinding::OnPulseReceived(const FString& EmitterId, float /*Intensity*/, FLinearColor /*Color*/, TSharedPtr<FJsonObject> Data)
{
    if (!Data.IsValid() || !BindingConfig.bEnabled)
    {
        return;
    }

    // Check each binding for matching emitter
    for (int32 i = 0; i < BindingConfig.Bindings.Num(); ++i)
    {
        FRshipControlRigPropertyBinding& Binding = BindingConfig.Bindings[i];
        if (!Binding.bEnabled)
        {
            continue;
        }

        if (MatchesEmitterPattern(EmitterId, Binding.EmitterId))
        {
            // Extract and map the value
            float RawValue = ExtractFieldValue(Data, Binding.SourceField);
            float MappedValue = MapValue(RawValue, Binding);

            // Ensure we have a state for this binding
            if (i >= BindingStates.Num())
            {
                BindingStates.SetNum(i + 1);
            }

            // Set target value (interpolation happens in tick)
            BindingStates[i].TargetValue = MappedValue;
        }
    }
}

float URshipControlRigBinding::ExtractFieldValue(TSharedPtr<FJsonObject> Data, const FString& FieldPath) const
{
    if (!Data.IsValid())
    {
        return 0.0f;
    }

    // Handle nested paths like "color.r" or "position.x"
    TArray<FString> PathParts;
    FieldPath.ParseIntoArray(PathParts, TEXT("."), true);

    TSharedPtr<FJsonObject> CurrentObj = Data;
    float Result = 0.0f;

    for (int32 i = 0; i < PathParts.Num(); ++i)
    {
        const FString& Part = PathParts[i];
        bool bIsLast = (i == PathParts.Num() - 1);

        if (bIsLast)
        {
            // Try to get as number
            if (CurrentObj->TryGetNumberField(Part, Result))
            {
                return Result;
            }

            // Try as boolean
            bool bValue;
            if (CurrentObj->TryGetBoolField(Part, bValue))
            {
                return bValue ? 1.0f : 0.0f;
            }
        }
        else
        {
            // Navigate to nested object
            const TSharedPtr<FJsonObject>* NestedObj;
            if (CurrentObj->TryGetObjectField(Part, NestedObj))
            {
                CurrentObj = *NestedObj;
            }
            else
            {
                return 0.0f;
            }
        }
    }

    return Result;
}

float URshipControlRigBinding::MapValue(float Input, const FRshipControlRigPropertyBinding& Binding) const
{
    float Output = Input;

    switch (Binding.MappingFunc)
    {
    case ERshipControlRigMappingFunc::Direct:
        // No mapping
        break;

    case ERshipControlRigMappingFunc::Remap:
        // Linear remap from input range to output range
        if (Binding.InputMax != Binding.InputMin)
        {
            float Normalized = (Input - Binding.InputMin) / (Binding.InputMax - Binding.InputMin);
            Output = Binding.OutputMin + Normalized * (Binding.OutputMax - Binding.OutputMin);
        }
        break;

    case ERshipControlRigMappingFunc::Curve:
        // Curve-based mapping using UCurveFloat
        if (Binding.ResponseCurve)
        {
            // Normalize input to 0-1 range for curve lookup
            float NormalizedInput = Input;
            if (Binding.InputMax != Binding.InputMin)
            {
                NormalizedInput = (Input - Binding.InputMin) / (Binding.InputMax - Binding.InputMin);
                NormalizedInput = FMath::Clamp(NormalizedInput, 0.0f, 1.0f);
            }

            // Sample the curve (X is normalized input, Y is output)
            float CurveOutput = Binding.ResponseCurve->GetFloatValue(NormalizedInput);

            // Scale to output range
            Output = Binding.OutputMin + CurveOutput * (Binding.OutputMax - Binding.OutputMin);
        }
        else
        {
            // No curve assigned, fall back to linear remap
            if (Binding.InputMax != Binding.InputMin)
            {
                float Normalized = (Input - Binding.InputMin) / (Binding.InputMax - Binding.InputMin);
                Output = Binding.OutputMin + Normalized * (Binding.OutputMax - Binding.OutputMin);
            }
        }
        break;

    case ERshipControlRigMappingFunc::Expression:
        // Expression-based mapping with simple math expressions
        // Supports: x (input), t (time), +, -, *, /, sin, cos, abs, pow, sqrt, min, max
        if (!Binding.Expression.IsEmpty())
        {
            // Normalize input
            float NormalizedInput = Input;
            if (Binding.InputMax != Binding.InputMin)
            {
                NormalizedInput = (Input - Binding.InputMin) / (Binding.InputMax - Binding.InputMin);
            }

            // Parse and evaluate expression
            Output = EvaluateExpression(Binding.Expression, NormalizedInput);

            // Scale to output range
            Output = Binding.OutputMin + FMath::Clamp(Output, 0.0f, 1.0f) * (Binding.OutputMax - Binding.OutputMin);
        }
        break;
    }

    // Apply multiplier and offset
    Output = Output * Binding.Multiplier + Binding.Offset;

    // Clamp if requested
    if (Binding.bClampOutput)
    {
        float MinVal = FMath::Min(Binding.OutputMin, Binding.OutputMax);
        float MaxVal = FMath::Max(Binding.OutputMin, Binding.OutputMax);
        Output = FMath::Clamp(Output, MinVal, MaxVal);
    }

    return Output;
}

float URshipControlRigBinding::InterpolateValue(float Current, float Target, const FRshipControlRigPropertyBinding& Binding, float DeltaTime, float& Velocity) const
{
    switch (Binding.InterpMode)
    {
    case ERshipControlRigInterpMode::None:
        return Target;

    case ERshipControlRigInterpMode::Linear:
        return FMath::FInterpTo(Current, Target, DeltaTime, Binding.InterpSpeed);

    case ERshipControlRigInterpMode::EaseIn:
        {
            float Alpha = FMath::Clamp(DeltaTime * Binding.InterpSpeed, 0.0f, 1.0f);
            Alpha = Alpha * Alpha;  // Quadratic ease in
            return FMath::Lerp(Current, Target, Alpha);
        }

    case ERshipControlRigInterpMode::EaseOut:
        {
            float Alpha = FMath::Clamp(DeltaTime * Binding.InterpSpeed, 0.0f, 1.0f);
            Alpha = 1.0f - (1.0f - Alpha) * (1.0f - Alpha);  // Quadratic ease out
            return FMath::Lerp(Current, Target, Alpha);
        }

    case ERshipControlRigInterpMode::EaseInOut:
        {
            float Alpha = FMath::Clamp(DeltaTime * Binding.InterpSpeed, 0.0f, 1.0f);
            Alpha = Alpha < 0.5f ? 2.0f * Alpha * Alpha : 1.0f - FMath::Pow(-2.0f * Alpha + 2.0f, 2.0f) / 2.0f;
            return FMath::Lerp(Current, Target, Alpha);
        }

    case ERshipControlRigInterpMode::Spring:
        {
            // Spring dynamics
            float Diff = Target - Current;
            float SpringForce = Diff * Binding.SpringStiffness;
            float DampingForce = -Velocity * Binding.SpringDamping;
            float Acceleration = SpringForce + DampingForce;

            Velocity += Acceleration * DeltaTime;
            return Current + Velocity * DeltaTime;
        }

    default:
        return Target;
    }
}

float URshipControlRigBinding::EvaluateExpression(const FString& ExpressionStr, float X) const
{
    // Simple expression evaluator supporting common math operations
    // Variables: x (normalized input 0-1), pi, e
    // Functions: sin, cos, tan, abs, sqrt, pow, min, max, clamp, lerp
    // Operators: +, -, *, /, ^

    if (ExpressionStr.IsEmpty())
    {
        return X;
    }

    // Build the expression string with variable substitution
    FString Expr = ExpressionStr.ToLower();

    // Replace variables
    Expr = Expr.Replace(TEXT("x"), *FString::Printf(TEXT("%.6f"), X));
    Expr = Expr.Replace(TEXT("pi"), *FString::Printf(TEXT("%.10f"), PI));
    Expr = Expr.Replace(TEXT("e"), *FString::Printf(TEXT("%.10f"), 2.71828182845904523536));

    // Get current time for animations
    double CurrentTime = FPlatformTime::Seconds();
    Expr = Expr.Replace(TEXT("t"), *FString::Printf(TEXT("%.6f"), CurrentTime));

    // Simple recursive descent parser
    int32 Pos = 0;
    float Result = ParseAddSub(Expr, Pos);

    return Result;
}

float URshipControlRigBinding::ParseAddSub(const FString& Expr, int32& Pos) const
{
    float Result = ParseMulDiv(Expr, Pos);

    while (Pos < Expr.Len())
    {
        SkipWhitespace(Expr, Pos);
        if (Pos >= Expr.Len()) break;

        TCHAR Op = Expr[Pos];
        if (Op == '+')
        {
            Pos++;
            Result += ParseMulDiv(Expr, Pos);
        }
        else if (Op == '-')
        {
            Pos++;
            Result -= ParseMulDiv(Expr, Pos);
        }
        else
        {
            break;
        }
    }

    return Result;
}

float URshipControlRigBinding::ParseMulDiv(const FString& Expr, int32& Pos) const
{
    float Result = ParsePower(Expr, Pos);

    while (Pos < Expr.Len())
    {
        SkipWhitespace(Expr, Pos);
        if (Pos >= Expr.Len()) break;

        TCHAR Op = Expr[Pos];
        if (Op == '*')
        {
            Pos++;
            Result *= ParsePower(Expr, Pos);
        }
        else if (Op == '/')
        {
            Pos++;
            float Divisor = ParsePower(Expr, Pos);
            if (!FMath::IsNearlyZero(Divisor))
            {
                Result /= Divisor;
            }
        }
        else
        {
            break;
        }
    }

    return Result;
}

float URshipControlRigBinding::ParsePower(const FString& Expr, int32& Pos) const
{
    float Result = ParseUnary(Expr, Pos);

    SkipWhitespace(Expr, Pos);
    if (Pos < Expr.Len() && Expr[Pos] == '^')
    {
        Pos++;
        float Exponent = ParsePower(Expr, Pos);  // Right associative
        Result = FMath::Pow(Result, Exponent);
    }

    return Result;
}

float URshipControlRigBinding::ParseUnary(const FString& Expr, int32& Pos) const
{
    SkipWhitespace(Expr, Pos);

    if (Pos < Expr.Len() && Expr[Pos] == '-')
    {
        Pos++;
        return -ParsePrimary(Expr, Pos);
    }
    else if (Pos < Expr.Len() && Expr[Pos] == '+')
    {
        Pos++;
    }

    return ParsePrimary(Expr, Pos);
}

float URshipControlRigBinding::ParsePrimary(const FString& Expr, int32& Pos) const
{
    SkipWhitespace(Expr, Pos);

    // Check for functions
    if (Pos < Expr.Len() && FChar::IsAlpha(Expr[Pos]))
    {
        int32 Start = Pos;
        while (Pos < Expr.Len() && FChar::IsAlpha(Expr[Pos]))
        {
            Pos++;
        }
        FString FuncName = Expr.Mid(Start, Pos - Start);

        SkipWhitespace(Expr, Pos);
        if (Pos < Expr.Len() && Expr[Pos] == '(')
        {
            Pos++;  // Skip '('
            float Arg1 = ParseAddSub(Expr, Pos);

            // Check for second argument
            SkipWhitespace(Expr, Pos);
            float Arg2 = 0.0f;
            bool bHasSecondArg = false;
            if (Pos < Expr.Len() && Expr[Pos] == ',')
            {
                Pos++;
                Arg2 = ParseAddSub(Expr, Pos);
                bHasSecondArg = true;
            }

            // Skip closing parenthesis
            SkipWhitespace(Expr, Pos);
            if (Pos < Expr.Len() && Expr[Pos] == ')')
            {
                Pos++;
            }

            // Evaluate function
            if (FuncName == TEXT("sin")) return FMath::Sin(Arg1);
            if (FuncName == TEXT("cos")) return FMath::Cos(Arg1);
            if (FuncName == TEXT("tan")) return FMath::Tan(Arg1);
            if (FuncName == TEXT("abs")) return FMath::Abs(Arg1);
            if (FuncName == TEXT("sqrt")) return FMath::Sqrt(FMath::Max(0.0f, Arg1));
            if (FuncName == TEXT("pow") && bHasSecondArg) return FMath::Pow(Arg1, Arg2);
            if (FuncName == TEXT("min") && bHasSecondArg) return FMath::Min(Arg1, Arg2);
            if (FuncName == TEXT("max") && bHasSecondArg) return FMath::Max(Arg1, Arg2);
            if (FuncName == TEXT("clamp"))
            {
                // clamp(value, min, max) - need third arg
                SkipWhitespace(Expr, Pos);
                float Arg3 = 1.0f;
                if (Pos > 0 && Expr[Pos - 1] == ',')
                {
                    Arg3 = ParseAddSub(Expr, Pos);
                    SkipWhitespace(Expr, Pos);
                    if (Pos < Expr.Len() && Expr[Pos] == ')') Pos++;
                }
                return FMath::Clamp(Arg1, Arg2, Arg3);
            }
            if (FuncName == TEXT("lerp") && bHasSecondArg)
            {
                // lerp(a, b, t) - need third arg
                SkipWhitespace(Expr, Pos);
                float Arg3 = 0.5f;
                if (Pos < Expr.Len() && Expr[Pos] == ',')
                {
                    Pos++;
                    Arg3 = ParseAddSub(Expr, Pos);
                    SkipWhitespace(Expr, Pos);
                    if (Pos < Expr.Len() && Expr[Pos] == ')') Pos++;
                }
                return FMath::Lerp(Arg1, Arg2, Arg3);
            }
            if (FuncName == TEXT("floor")) return FMath::Floor(Arg1);
            if (FuncName == TEXT("ceil")) return FMath::CeilToFloat(Arg1);
            if (FuncName == TEXT("round")) return FMath::RoundToFloat(Arg1);
            if (FuncName == TEXT("frac")) return FMath::Frac(Arg1);
        }

        // Unknown function, return 0
        return 0.0f;
    }

    // Check for parentheses
    if (Pos < Expr.Len() && Expr[Pos] == '(')
    {
        Pos++;  // Skip '('
        float Result = ParseAddSub(Expr, Pos);
        SkipWhitespace(Expr, Pos);
        if (Pos < Expr.Len() && Expr[Pos] == ')')
        {
            Pos++;  // Skip ')'
        }
        return Result;
    }

    // Parse number
    return ParseNumber(Expr, Pos);
}

float URshipControlRigBinding::ParseNumber(const FString& Expr, int32& Pos) const
{
    SkipWhitespace(Expr, Pos);

    int32 Start = Pos;
    bool bHasDecimal = false;

    // Handle optional sign
    if (Pos < Expr.Len() && (Expr[Pos] == '-' || Expr[Pos] == '+'))
    {
        Pos++;
    }

    // Parse digits
    while (Pos < Expr.Len())
    {
        TCHAR C = Expr[Pos];
        if (FChar::IsDigit(C))
        {
            Pos++;
        }
        else if (C == '.' && !bHasDecimal)
        {
            bHasDecimal = true;
            Pos++;
        }
        else
        {
            break;
        }
    }

    if (Pos > Start)
    {
        FString NumStr = Expr.Mid(Start, Pos - Start);
        return FCString::Atof(*NumStr);
    }

    return 0.0f;
}

void URshipControlRigBinding::SkipWhitespace(const FString& Expr, int32& Pos) const
{
    while (Pos < Expr.Len() && FChar::IsWhitespace(Expr[Pos]))
    {
        Pos++;
    }
}

void URshipControlRigBinding::UpdateBinding(int32 Index, float DeltaTime)
{
    if (Index >= BindingConfig.Bindings.Num() || Index >= BindingStates.Num())
    {
        return;
    }

    FRshipControlRigPropertyBinding& Binding = BindingConfig.Bindings[Index];
    FRshipControlRigBindingState& State = BindingStates[Index];

    // Check for manual override
    if (ManualOverrides.Contains(Binding.ControlName))
    {
        float BlendTime = OverrideBlendTimers.FindRef(Binding.ControlName);
        if (BlendTime > 0.0f)
        {
            // Blend to override
            State.TargetValue = ManualOverrides[Binding.ControlName];
        }
        else
        {
            State.CurrentValue = ManualOverrides[Binding.ControlName];
            return;
        }
    }

    // Interpolate to target
    State.CurrentValue = InterpolateValue(
        State.CurrentValue,
        State.TargetValue,
        Binding,
        DeltaTime,
        State.Velocity
    );

    // Apply weight and global weight
    float FinalWeight = Binding.Weight * BindingConfig.GlobalWeight;
    State.CurrentValue *= FinalWeight;
}

void URshipControlRigBinding::ApplyBindingToControlRig(int32 Index)
{
    if (!ControlRig || Index >= BindingConfig.Bindings.Num() || Index >= BindingStates.Num())
    {
        return;
    }

    const FRshipControlRigPropertyBinding& Binding = BindingConfig.Bindings[Index];
    const FRshipControlRigBindingState& State = BindingStates[Index];

    URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
    if (!Hierarchy)
    {
        return;
    }

    FRigElementKey Key(Binding.ControlName, ERigElementType::Control);
    int32 ControlIndex = Hierarchy->GetIndex(Key);

    if (ControlIndex == INDEX_NONE)
    {
        return;
    }

    switch (Binding.PropertyType)
    {
    case ERshipControlRigPropertyType::Float:
        {
            FRigControlValue Value;
            Value.Set<float>(State.CurrentValue);
            Hierarchy->SetControlValue(Key, Value, ERigControlValueType::Current);
        }
        break;

    case ERshipControlRigPropertyType::Vector:
        {
            FVector Vec = State.CurrentVector;
            if (Binding.VectorComponent == TEXT("X"))
            {
                Vec.X = State.CurrentValue;
            }
            else if (Binding.VectorComponent == TEXT("Y"))
            {
                Vec.Y = State.CurrentValue;
            }
            else if (Binding.VectorComponent == TEXT("Z"))
            {
                Vec.Z = State.CurrentValue;
            }
            else
            {
                Vec = FVector(State.CurrentValue);
            }

            FRigControlValue Value;
            Value.Set<FVector3f>(FVector3f(Vec));
            Hierarchy->SetControlValue(Key, Value, ERigControlValueType::Current);
        }
        break;

    case ERshipControlRigPropertyType::Rotator:
        {
#if RSHIP_UE_5_6_OR_LATER
            // UE 5.6+: Store euler angles as FVector3f (pitch, yaw, roll) for rotator controls
            FVector3f EulerAngles(
                State.CurrentRotator.Pitch,
                State.CurrentRotator.Yaw,
                State.CurrentRotator.Roll
            );
            FRigControlValue Value;
            Value.Set<FVector3f>(EulerAngles);
            Hierarchy->SetControlValue(Key, Value, ERigControlValueType::Current);
#else
            // UE 5.5 and earlier: Use Set<FRotator> directly
            FRigControlValue Value;
            Value.Set<FRotator>(State.CurrentRotator);
            Hierarchy->SetControlValue(Key, Value, ERigControlValueType::Current);
#endif
        }
        break;

    case ERshipControlRigPropertyType::Transform:
        {
#if RSHIP_UE_5_6_OR_LATER
            // UE 5.6+: Use hierarchy's MakeControlValueFromEulerTransform method
            FEulerTransform EulerTransform;
            EulerTransform.SetLocation(State.CurrentTransform.GetLocation());
            EulerTransform.SetRotator(State.CurrentTransform.Rotator());
            EulerTransform.SetScale3D(State.CurrentTransform.GetScale3D());
            FRigControlValue Value = Hierarchy->MakeControlValueFromEulerTransform(EulerTransform);
            Hierarchy->SetControlValue(Key, Value, ERigControlValueType::Current);
#else
            // UE 5.5 and earlier: Use Set<FTransform> directly
            FRigControlValue Value;
            Value.Set<FTransform>(State.CurrentTransform);
            Hierarchy->SetControlValue(Key, Value, ERigControlValueType::Current);
#endif
        }
        break;

    case ERshipControlRigPropertyType::Bool:
        {
            FRigControlValue Value;
            Value.Set<bool>(State.CurrentValue > 0.5f);
            Hierarchy->SetControlValue(Key, Value, ERigControlValueType::Current);
        }
        break;

    case ERshipControlRigPropertyType::Integer:
        {
            FRigControlValue Value;
            Value.Set<int32>(FMath::RoundToInt(State.CurrentValue));
            Hierarchy->SetControlValue(Key, Value, ERigControlValueType::Current);
        }
        break;

    case ERshipControlRigPropertyType::Color:
        {
            // Interpret CurrentValue as packed color or use RGB from vector
            FLinearColor Color(State.CurrentVector.X, State.CurrentVector.Y, State.CurrentVector.Z, 1.0f);
            FRigControlValue Value;
            Value.Set<FVector4f>(FVector4f(Color.R, Color.G, Color.B, Color.A));
            Hierarchy->SetControlValue(Key, Value, ERigControlValueType::Current);
        }
        break;
    }

    // Broadcast update
    OnBindingUpdated.Broadcast(Binding.ControlName, State.CurrentValue);
}

bool URshipControlRigBinding::MatchesEmitterPattern(const FString& EmitterId, const FString& Pattern) const
{
    // Simple wildcard matching
    if (Pattern.Contains(TEXT("*")))
    {
        // Convert pattern to regex-like matching
        FString RegexPattern = Pattern.Replace(TEXT("*"), TEXT(".*"));
        return EmitterId.MatchesWildcard(Pattern);
    }

    return EmitterId == Pattern;
}

// ========================================================================
// BINDING MANAGEMENT
// ========================================================================

void URshipControlRigBinding::AddBinding(const FRshipControlRigPropertyBinding& Binding)
{
    BindingConfig.Bindings.Add(Binding);
    BindingStates.Add(FRshipControlRigBindingState());

    // Rebind to pulse receiver to pick up new emitter IDs
    UnbindFromPulseReceiver();
    BindToPulseReceiver();
}

void URshipControlRigBinding::RemoveBinding(int32 Index)
{
    if (Index >= 0 && Index < BindingConfig.Bindings.Num())
    {
        BindingConfig.Bindings.RemoveAt(Index);
        if (Index < BindingStates.Num())
        {
            BindingStates.RemoveAt(Index);
        }
    }
}

void URshipControlRigBinding::ClearBindings()
{
    BindingConfig.Bindings.Empty();
    BindingStates.Empty();
}

void URshipControlRigBinding::SetBindingEnabled(int32 Index, bool bEnabled)
{
    if (Index >= 0 && Index < BindingConfig.Bindings.Num())
    {
        BindingConfig.Bindings[Index].bEnabled = bEnabled;
    }
}

void URshipControlRigBinding::SetGlobalWeight(float Weight)
{
    BindingConfig.GlobalWeight = FMath::Clamp(Weight, 0.0f, 1.0f);
}

// ========================================================================
// CONFIGURATION MANAGEMENT
// ========================================================================

void URshipControlRigBinding::SaveCurrentConfig(const FString& Name)
{
    BindingConfig.Name = Name;

    // Check if config with same name exists
    int32 ExistingIndex = SavedConfigs.IndexOfByPredicate([&Name](const FRshipControlRigConfig& Config)
    {
        return Config.Name == Name;
    });

    if (ExistingIndex != INDEX_NONE)
    {
        SavedConfigs[ExistingIndex] = BindingConfig;
    }
    else
    {
        SavedConfigs.Add(BindingConfig);
    }
}

bool URshipControlRigBinding::LoadConfig(const FString& Name)
{
    const FRshipControlRigConfig* Found = SavedConfigs.FindByPredicate([&Name](const FRshipControlRigConfig& Config)
    {
        return Config.Name == Name;
    });

    if (Found)
    {
        BindingConfig = *Found;
        BindingStates.SetNum(BindingConfig.Bindings.Num());

        // Rebind
        UnbindFromPulseReceiver();
        BindToPulseReceiver();

        OnConfigChanged.Broadcast(BindingConfig);
        return true;
    }

    return false;
}

bool URshipControlRigBinding::DeleteConfig(const FString& Name)
{
    int32 Index = SavedConfigs.IndexOfByPredicate([&Name](const FRshipControlRigConfig& Config)
    {
        return Config.Name == Name;
    });

    if (Index != INDEX_NONE)
    {
        SavedConfigs.RemoveAt(Index);
        return true;
    }

    return false;
}

TArray<FString> URshipControlRigBinding::GetSavedConfigNames() const
{
    TArray<FString> Names;
    for (const FRshipControlRigConfig& Config : SavedConfigs)
    {
        Names.Add(Config.Name);
    }
    return Names;
}

// ========================================================================
// QUICK BINDING HELPERS
// ========================================================================

void URshipControlRigBinding::BindIntensityToFloat(const FString& EmitterId, FName ControlName, float OutputMin, float OutputMax)
{
    FRshipControlRigPropertyBinding Binding;
    Binding.EmitterId = EmitterId;
    Binding.SourceField = TEXT("intensity");
    Binding.ControlName = ControlName;
    Binding.PropertyType = ERshipControlRigPropertyType::Float;
    Binding.MappingFunc = ERshipControlRigMappingFunc::Remap;
    Binding.InputMin = 0.0f;
    Binding.InputMax = 1.0f;
    Binding.OutputMin = OutputMin;
    Binding.OutputMax = OutputMax;
    Binding.InterpMode = ERshipControlRigInterpMode::Linear;
    Binding.InterpSpeed = 10.0f;

    AddBinding(Binding);
}

void URshipControlRigBinding::BindColorToVector(const FString& EmitterId, FName ControlName)
{
    // Add R binding
    FRshipControlRigPropertyBinding BindingR;
    BindingR.EmitterId = EmitterId;
    BindingR.SourceField = TEXT("color.r");
    BindingR.ControlName = ControlName;
    BindingR.PropertyType = ERshipControlRigPropertyType::Vector;
    BindingR.VectorComponent = TEXT("X");
    BindingR.MappingFunc = ERshipControlRigMappingFunc::Remap;
    BindingR.InputMin = 0.0f;
    BindingR.InputMax = 1.0f;
    BindingR.OutputMin = 0.0f;
    BindingR.OutputMax = 1.0f;
    AddBinding(BindingR);

    // Add G binding
    FRshipControlRigPropertyBinding BindingG = BindingR;
    BindingG.SourceField = TEXT("color.g");
    BindingG.VectorComponent = TEXT("Y");
    AddBinding(BindingG);

    // Add B binding
    FRshipControlRigPropertyBinding BindingB = BindingR;
    BindingB.SourceField = TEXT("color.b");
    BindingB.VectorComponent = TEXT("Z");
    AddBinding(BindingB);
}

void URshipControlRigBinding::BindPositionToTransform(const FString& EmitterId, FName ControlName)
{
    FRshipControlRigPropertyBinding Binding;
    Binding.EmitterId = EmitterId;
    Binding.SourceField = TEXT("position");
    Binding.ControlName = ControlName;
    Binding.PropertyType = ERshipControlRigPropertyType::Transform;
    Binding.MappingFunc = ERshipControlRigMappingFunc::Direct;
    Binding.InterpMode = ERshipControlRigInterpMode::Linear;
    Binding.InterpSpeed = 10.0f;

    AddBinding(Binding);
}

void URshipControlRigBinding::BindRotationToRotator(const FString& EmitterId, FName ControlName)
{
    FRshipControlRigPropertyBinding Binding;
    Binding.EmitterId = EmitterId;
    Binding.SourceField = TEXT("rotation");
    Binding.ControlName = ControlName;
    Binding.PropertyType = ERshipControlRigPropertyType::Rotator;
    Binding.MappingFunc = ERshipControlRigMappingFunc::Direct;
    Binding.InterpMode = ERshipControlRigInterpMode::Linear;
    Binding.InterpSpeed = 10.0f;

    AddBinding(Binding);
}

// ========================================================================
// DISCOVERY
// ========================================================================

TArray<FName> URshipControlRigBinding::GetAvailableControls() const
{
    TArray<FName> Controls;

    if (!ControlRig)
    {
        return Controls;
    }

    URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
    if (!Hierarchy)
    {
        return Controls;
    }

    Hierarchy->ForEach<FRigControlElement>([&Controls](FRigControlElement* Element) -> bool
    {
        Controls.Add(Element->GetFName());
        return true;
    });

    return Controls;
}

ERshipControlRigPropertyType URshipControlRigBinding::GetControlType(FName ControlName) const
{
    if (!ControlRig)
    {
        return ERshipControlRigPropertyType::Float;
    }

    URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
    if (!Hierarchy)
    {
        return ERshipControlRigPropertyType::Float;
    }

    FRigElementKey Key(ControlName, ERigElementType::Control);
    const FRigControlElement* Element = Hierarchy->Find<FRigControlElement>(Key);

    if (!Element)
    {
        return ERshipControlRigPropertyType::Float;
    }

    switch (Element->Settings.ControlType)
    {
    case ERigControlType::Float:
    case ERigControlType::ScaleFloat:
        return ERshipControlRigPropertyType::Float;

    case ERigControlType::Integer:
        return ERshipControlRigPropertyType::Integer;

    case ERigControlType::Bool:
        return ERshipControlRigPropertyType::Bool;

    case ERigControlType::Vector2D:
    case ERigControlType::Position:
    case ERigControlType::Scale:
        return ERshipControlRigPropertyType::Vector;

    case ERigControlType::Rotator:
        return ERshipControlRigPropertyType::Rotator;

    case ERigControlType::Transform:
    case ERigControlType::TransformNoScale:
    case ERigControlType::EulerTransform:
        return ERshipControlRigPropertyType::Transform;

    default:
        return ERshipControlRigPropertyType::Float;
    }
}

void URshipControlRigBinding::AutoGenerateBindings(const FString& EmitterPattern, const FString& ControlPattern)
{
    TArray<FName> Controls = GetAvailableControls();

    for (const FName& ControlName : Controls)
    {
        FString ControlStr = ControlName.ToString();

        // Check if control matches pattern
        if (!ControlStr.MatchesWildcard(ControlPattern))
        {
            continue;
        }

        // Create binding
        FRshipControlRigPropertyBinding Binding;
        Binding.EmitterId = EmitterPattern;
        Binding.SourceField = TEXT("intensity");
        Binding.ControlName = ControlName;
        Binding.PropertyType = GetControlType(ControlName);
        Binding.MappingFunc = ERshipControlRigMappingFunc::Remap;
        Binding.InterpMode = ERshipControlRigInterpMode::Linear;
        Binding.InterpSpeed = 10.0f;

        AddBinding(Binding);
    }
}

// ========================================================================
// RUNTIME
// ========================================================================

float URshipControlRigBinding::GetBindingValue(FName ControlName) const
{
    for (int32 i = 0; i < BindingConfig.Bindings.Num(); ++i)
    {
        if (BindingConfig.Bindings[i].ControlName == ControlName)
        {
            if (i < BindingStates.Num())
            {
                return BindingStates[i].CurrentValue;
            }
        }
    }
    return 0.0f;
}

void URshipControlRigBinding::SetBindingOverride(FName ControlName, float Value, float BlendTime)
{
    ManualOverrides.Add(ControlName, Value);
    OverrideBlendTimers.Add(ControlName, BlendTime);
}

void URshipControlRigBinding::ClearBindingOverride(FName ControlName)
{
    ManualOverrides.Remove(ControlName);
    OverrideBlendTimers.Remove(ControlName);
}

void URshipControlRigBinding::ResetAllBindings()
{
    for (FRshipControlRigBindingState& State : BindingStates)
    {
        State.CurrentValue = 0.0f;
        State.TargetValue = 0.0f;
        State.Velocity = 0.0f;
        State.CurrentVector = FVector::ZeroVector;
        State.TargetVector = FVector::ZeroVector;
    }

    ManualOverrides.Empty();
    OverrideBlendTimers.Empty();
}

// ============================================================================
// CONTROL RIG BINDING MANAGER
// ============================================================================

void URshipControlRigManager::Initialize(URshipSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;
    LoadTemplatesFromFile();

    UE_LOG(LogTemp, Log, TEXT("RshipControlRigManager: Initialized with %d templates"), Templates.Num());
}

void URshipControlRigManager::Shutdown()
{
    SaveTemplatesToFile();
    RegisteredBindings.Empty();
    Subsystem = nullptr;
}

void URshipControlRigManager::RegisterBinding(URshipControlRigBinding* Binding)
{
    if (Binding && !RegisteredBindings.Contains(Binding))
    {
        RegisteredBindings.Add(Binding);
    }
}

void URshipControlRigManager::UnregisterBinding(URshipControlRigBinding* Binding)
{
    RegisteredBindings.Remove(Binding);
}

TArray<URshipControlRigBinding*> URshipControlRigManager::GetAllBindings() const
{
    return RegisteredBindings;
}

void URshipControlRigManager::SetGlobalWeightAll(float Weight)
{
    for (URshipControlRigBinding* Binding : RegisteredBindings)
    {
        if (Binding)
        {
            Binding->SetGlobalWeight(Weight);
        }
    }
}

void URshipControlRigManager::SetEnabledAll(bool bEnabled)
{
    for (URshipControlRigBinding* Binding : RegisteredBindings)
    {
        if (Binding)
        {
            Binding->BindingConfig.bEnabled = bEnabled;
        }
    }
}

void URshipControlRigManager::ResetAll()
{
    for (URshipControlRigBinding* Binding : RegisteredBindings)
    {
        if (Binding)
        {
            Binding->ResetAllBindings();
        }
    }
}

void URshipControlRigManager::SaveTemplate(const FString& TemplateName, const FRshipControlRigConfig& Config)
{
    Templates.Add(TemplateName, Config);
    SaveTemplatesToFile();
}

bool URshipControlRigManager::LoadTemplate(const FString& TemplateName, FRshipControlRigConfig& OutConfig)
{
    if (Templates.Contains(TemplateName))
    {
        OutConfig = Templates[TemplateName];
        return true;
    }
    return false;
}

TArray<FString> URshipControlRigManager::GetTemplateNames() const
{
    TArray<FString> Names;
    Templates.GetKeys(Names);
    return Names;
}

bool URshipControlRigManager::DeleteTemplate(const FString& TemplateName)
{
    if (Templates.Remove(TemplateName) > 0)
    {
        SaveTemplatesToFile();
        return true;
    }
    return false;
}

FString URshipControlRigManager::GetTemplatesFilePath() const
{
    return FPaths::ProjectSavedDir() / TEXT("Rship") / TEXT("ControlRigTemplates.json");
}

void URshipControlRigManager::LoadTemplatesFromFile()
{
    FString FilePath = GetTemplatesFilePath();
    FString JsonString;

    if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
    {
        return;
    }

    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        return;
    }

    const TSharedPtr<FJsonObject>* TemplatesObject;
    if (!RootObject->TryGetObjectField(TEXT("templates"), TemplatesObject))
    {
        return;
    }

    for (const auto& Pair : (*TemplatesObject)->Values)
    {
        const TSharedPtr<FJsonObject>* ConfigObject;
        if (Pair.Value->TryGetObject(ConfigObject))
        {
            FRshipControlRigConfig Config;
            Config.Name = Pair.Key;

            (*ConfigObject)->TryGetBoolField(TEXT("enabled"), Config.bEnabled);
            (*ConfigObject)->TryGetNumberField(TEXT("globalWeight"), Config.GlobalWeight);

            const TArray<TSharedPtr<FJsonValue>>* BindingsArray;
            if ((*ConfigObject)->TryGetArrayField(TEXT("bindings"), BindingsArray))
            {
                for (const TSharedPtr<FJsonValue>& BindingValue : *BindingsArray)
                {
                    const TSharedPtr<FJsonObject>* BindingObj;
                    if (BindingValue->TryGetObject(BindingObj))
                    {
                        FRshipControlRigPropertyBinding Binding;

                        (*BindingObj)->TryGetBoolField(TEXT("enabled"), Binding.bEnabled);
                        (*BindingObj)->TryGetStringField(TEXT("emitterId"), Binding.EmitterId);
                        (*BindingObj)->TryGetStringField(TEXT("sourceField"), Binding.SourceField);

                        FString ControlNameStr;
                        if ((*BindingObj)->TryGetStringField(TEXT("controlName"), ControlNameStr))
                        {
                            Binding.ControlName = *ControlNameStr;
                        }

                        (*BindingObj)->TryGetNumberField(TEXT("inputMin"), Binding.InputMin);
                        (*BindingObj)->TryGetNumberField(TEXT("inputMax"), Binding.InputMax);
                        (*BindingObj)->TryGetNumberField(TEXT("outputMin"), Binding.OutputMin);
                        (*BindingObj)->TryGetNumberField(TEXT("outputMax"), Binding.OutputMax);
                        (*BindingObj)->TryGetNumberField(TEXT("multiplier"), Binding.Multiplier);
                        (*BindingObj)->TryGetNumberField(TEXT("offset"), Binding.Offset);
                        (*BindingObj)->TryGetNumberField(TEXT("interpSpeed"), Binding.InterpSpeed);
                        (*BindingObj)->TryGetNumberField(TEXT("weight"), Binding.Weight);
                        (*BindingObj)->TryGetBoolField(TEXT("additive"), Binding.bAdditive);
                        (*BindingObj)->TryGetBoolField(TEXT("clampOutput"), Binding.bClampOutput);

                        Config.Bindings.Add(Binding);
                    }
                }
            }

            Templates.Add(Pair.Key, Config);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("RshipControlRigManager: Loaded %d templates from file"), Templates.Num());
}

void URshipControlRigManager::SaveTemplatesToFile()
{
    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
    TSharedPtr<FJsonObject> TemplatesObject = MakeShareable(new FJsonObject);

    for (const auto& Pair : Templates)
    {
        TSharedPtr<FJsonObject> ConfigObject = MakeShareable(new FJsonObject);
        const FRshipControlRigConfig& Config = Pair.Value;

        ConfigObject->SetBoolField(TEXT("enabled"), Config.bEnabled);
        ConfigObject->SetNumberField(TEXT("globalWeight"), Config.GlobalWeight);

        TArray<TSharedPtr<FJsonValue>> BindingsArray;
        for (const FRshipControlRigPropertyBinding& Binding : Config.Bindings)
        {
            TSharedPtr<FJsonObject> BindingObj = MakeShareable(new FJsonObject);

            BindingObj->SetBoolField(TEXT("enabled"), Binding.bEnabled);
            BindingObj->SetStringField(TEXT("emitterId"), Binding.EmitterId);
            BindingObj->SetStringField(TEXT("sourceField"), Binding.SourceField);
            BindingObj->SetStringField(TEXT("controlName"), Binding.ControlName.ToString());
            BindingObj->SetNumberField(TEXT("inputMin"), Binding.InputMin);
            BindingObj->SetNumberField(TEXT("inputMax"), Binding.InputMax);
            BindingObj->SetNumberField(TEXT("outputMin"), Binding.OutputMin);
            BindingObj->SetNumberField(TEXT("outputMax"), Binding.OutputMax);
            BindingObj->SetNumberField(TEXT("multiplier"), Binding.Multiplier);
            BindingObj->SetNumberField(TEXT("offset"), Binding.Offset);
            BindingObj->SetNumberField(TEXT("interpSpeed"), Binding.InterpSpeed);
            BindingObj->SetNumberField(TEXT("weight"), Binding.Weight);
            BindingObj->SetBoolField(TEXT("additive"), Binding.bAdditive);
            BindingObj->SetBoolField(TEXT("clampOutput"), Binding.bClampOutput);

            BindingsArray.Add(MakeShareable(new FJsonValueObject(BindingObj)));
        }
        ConfigObject->SetArrayField(TEXT("bindings"), BindingsArray);

        TemplatesObject->SetObjectField(Pair.Key, ConfigObject);
    }

    RootObject->SetObjectField(TEXT("templates"), TemplatesObject);

    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

    FString FilePath = GetTemplatesFilePath();
    FString Directory = FPaths::GetPath(FilePath);
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    PlatformFile.CreateDirectoryTree(*Directory);

    FFileHelper::SaveStringToFile(JsonString, *FilePath);
}
