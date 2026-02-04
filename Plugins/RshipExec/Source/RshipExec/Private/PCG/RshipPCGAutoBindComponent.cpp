// Rship PCG Auto-Bind Component Implementation

#include "PCG/RshipPCGAutoBindComponent.h"
#include "PCG/RshipPCGManager.h"
#include "RshipSubsystem.h"
#include "Logs.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

URshipPCGAutoBindComponent::URshipPCGAutoBindComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // Enable only when needed
	PrimaryComponentTick.TickInterval = 0.1f; // 10Hz default

	bIsRegistered = false;
	bIsInitialized = false;
	ClassBindings = nullptr;
	LastPulseCheckTime = 0.0;
}

void URshipPCGAutoBindComponent::OnRegister()
{
	Super::OnRegister();

	// Don't initialize during construction scripts or in editor preview
	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		InitializeBinding();
	}
}

void URshipPCGAutoBindComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!bIsInitialized)
	{
		InitializeBinding();
	}

	// Enable tick if we have any properties with pulse modes
	bool bNeedsTick = false;
	if (ClassBindings)
	{
		for (const FRshipPCGPropertyDescriptor& Desc : ClassBindings->Properties)
		{
			if (Desc.PulseMode != ERshipPCGPulseMode::Off)
			{
				bNeedsTick = true;
				break;
			}
		}
	}

	if (bNeedsTick)
	{
		PrimaryComponentTick.SetTickFunctionEnable(true);
	}
}

void URshipPCGAutoBindComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromManager();
	Super::EndPlay(EndPlayReason);
}

void URshipPCGAutoBindComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	UnregisterFromManager();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void URshipPCGAutoBindComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsRegistered && ClassBindings)
	{
		CheckPropertyChanges(DeltaTime);
	}
}

void URshipPCGAutoBindComponent::InitializeBinding()
{
	if (bIsInitialized)
	{
		return;
	}

	// Get subsystem
	if (!GEngine)
	{
		return;
	}

	Subsystem = GEngine->GetEngineSubsystem<URshipSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogRshipExec, Warning, TEXT("URshipPCGAutoBindComponent: Failed to get RshipSubsystem"));
		return;
	}

	// Generate ID if needed
	if (!InstanceId.IsValid() && bAutoGenerateId)
	{
		GenerateAutoInstanceId();
	}

	if (!InstanceId.IsValid())
	{
		UE_LOG(LogRshipExec, Warning, TEXT("URshipPCGAutoBindComponent: No valid InstanceId for actor %s"),
			GetOwner() ? *GetOwner()->GetName() : TEXT("unknown"));
		return;
	}

	// Build property bindings
	if (bAutoBindProperties)
	{
		BuildPropertyBindings();
	}

	// Initialize property states
	InitializePropertyStates();

	// Register with manager
	RegisterWithManager();

	bIsInitialized = true;

	// Fire bound event
	OnRshipBound.Broadcast();

	UE_LOG(LogRshipExec, Log, TEXT("URshipPCGAutoBindComponent: Initialized binding for %s (%s)"),
		*InstanceId.DisplayName, *InstanceId.TargetPath);
}

void URshipPCGAutoBindComponent::BuildPropertyBindings()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Get or create class bindings from PCG manager
	URshipPCGManager* PCGManager = Subsystem->GetPCGManager();
	if (!PCGManager)
	{
		UE_LOG(LogRshipExec, Warning, TEXT("URshipPCGAutoBindComponent: PCG Manager not available"));
		return;
	}

	UClass* OwnerClass = Owner->GetClass();
	ClassBindings = PCGManager->GetOrCreateClassBindings(OwnerClass);

	// Scan actor for rShip properties
	auto ScanObjectForProperties = [this](UObject* Obj, UClass* ObjClass)
	{
		EFieldIteratorFlags::SuperClassFlags SuperFlags = bIncludeInheritedProperties
			? EFieldIteratorFlags::IncludeSuper
			: EFieldIteratorFlags::ExcludeSuper;

		for (TFieldIterator<FProperty> PropIt(ObjClass, SuperFlags); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;

			if (RshipPCGUtils::HasRshipMetadata(Property))
			{
				PropertyOwners.Add(Property->GetFName(), Obj);
			}
		}
	};

	// Scan actor
	ScanObjectForProperties(Owner, OwnerClass);

	// Scan sibling components if enabled
	if (bIncludeSiblingComponents)
	{
		TArray<UActorComponent*> Components;
		Owner->GetComponents(Components);

		for (UActorComponent* Component : Components)
		{
			if (Component && Component != this)
			{
				ScanObjectForProperties(Component, Component->GetClass());
			}
		}
	}

	UE_LOG(LogRshipExec, Verbose, TEXT("URshipPCGAutoBindComponent: Found %d property owners for %s"),
		PropertyOwners.Num(), *Owner->GetName());
}

void URshipPCGAutoBindComponent::InitializePropertyStates()
{
	PropertyStates.Empty();

	if (!ClassBindings)
	{
		return;
	}

	for (int32 i = 0; i < ClassBindings->Properties.Num(); ++i)
	{
		const FRshipPCGPropertyDescriptor& Desc = ClassBindings->Properties[i];

		FRshipPCGPropertyState State;
		State.DescriptorIndex = i;

		// Apply default pulse mode if not specified
		ERshipPCGPulseMode EffectivePulseMode = Desc.PulseMode;
		if (EffectivePulseMode == ERshipPCGPulseMode::Off &&
			(Desc.Access == ERshipPCGPropertyAccess::ReadOnly || Desc.Access == ERshipPCGPropertyAccess::ReadWrite))
		{
			EffectivePulseMode = DefaultPulseMode;
		}

		// Set up pulse timing
		if (EffectivePulseMode == ERshipPCGPulseMode::FixedRate)
		{
			float Rate = Desc.PulseRateHz > 0.0f ? Desc.PulseRateHz : DefaultPulseRateHz;
			double Now = FPlatformTime::Seconds();
			State.NextPulseTime = Now + (1.0 / Rate);
		}

		PropertyStates.Add(MoveTemp(State));
	}
}

void URshipPCGAutoBindComponent::RegisterWithManager()
{
	if (bIsRegistered)
	{
		return;
	}

	URshipPCGManager* PCGManager = Subsystem ? Subsystem->GetPCGManager() : nullptr;
	if (!PCGManager)
	{
		UE_LOG(LogRshipExec, Warning, TEXT("URshipPCGAutoBindComponent: Cannot register - PCG Manager not available"));
		return;
	}

	PCGManager->RegisterInstance(this);
	bIsRegistered = true;
}

void URshipPCGAutoBindComponent::UnregisterFromManager()
{
	if (!bIsRegistered)
	{
		return;
	}

	if (GEngine)
	{
		URshipSubsystem* Sub = GEngine->GetEngineSubsystem<URshipSubsystem>();
		if (Sub)
		{
			URshipPCGManager* PCGManager = Sub->GetPCGManager();
			if (PCGManager)
			{
				PCGManager->UnregisterInstance(this);
			}
		}
	}

	bIsRegistered = false;
}

void URshipPCGAutoBindComponent::GenerateAutoInstanceId()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Try to find PCG component on parent or in world
	FGuid PCGComponentGuid = FGuid::NewGuid(); // Fallback
	FString SourceKey = TEXT("auto");
	int32 PointIndex = -1;
	double Distance = 0.0;
	double Alpha = 0.0;
	int32 Seed = 0;

	// Use actor position for deterministic ID
	FVector Location = Owner->GetActorLocation();
	Distance = Location.Size();

	// Use hashed position as seed
	Seed = GetTypeHash(Location);

	// Generate display name
	FString DisplayName = CustomTargetName;
	if (DisplayName.IsEmpty())
	{
#if WITH_EDITOR
		DisplayName = Owner->GetActorLabel();
#endif
		if (DisplayName.IsEmpty())
		{
			DisplayName = Owner->GetName();
		}
	}

	InstanceId = FRshipPCGInstanceId::FromPCGPoint(
		PCGComponentGuid,
		SourceKey,
		PointIndex,
		Distance,
		Alpha,
		Seed,
		DisplayName);
}

void URshipPCGAutoBindComponent::SetInstanceId(const FRshipPCGInstanceId& InInstanceId)
{
	bool bWasRegistered = bIsRegistered;

	if (bWasRegistered)
	{
		UnregisterFromManager();
	}

	InstanceId = InInstanceId;

	if (bWasRegistered && InstanceId.IsValid())
	{
		RegisterWithManager();
	}
}

void URshipPCGAutoBindComponent::Reregister()
{
	UnregisterFromManager();

	if (InstanceId.IsValid())
	{
		RegisterWithManager();
	}
}

void URshipPCGAutoBindComponent::RescanProperties()
{
	PropertyOwners.Empty();
	PropertyStates.Empty();
	ClassBindings = nullptr;

	if (bAutoBindProperties)
	{
		BuildPropertyBindings();
	}

	InitializePropertyStates();

	// Re-register to update schema
	if (bIsRegistered)
	{
		Reregister();
	}
}

TArray<FName> URshipPCGAutoBindComponent::GetBoundPropertyNames() const
{
	TArray<FName> Names;
	if (ClassBindings)
	{
		for (const FRshipPCGPropertyDescriptor& Desc : ClassBindings->Properties)
		{
			Names.Add(Desc.PropertyName);
		}
	}
	return Names;
}

FString URshipPCGAutoBindComponent::GetTargetPath() const
{
	return InstanceId.TargetPath;
}

void URshipPCGAutoBindComponent::CheckPropertyChanges(float DeltaTime)
{
	if (!ClassBindings || PropertyStates.Num() != ClassBindings->Properties.Num())
	{
		return;
	}

	double Now = FPlatformTime::Seconds();

	for (int32 i = 0; i < PropertyStates.Num(); ++i)
	{
		FRshipPCGPropertyState& State = PropertyStates[i];
		const FRshipPCGPropertyDescriptor& Desc = ClassBindings->Properties[State.DescriptorIndex];

		// Skip non-readable properties
		if (Desc.Access == ERshipPCGPropertyAccess::WriteOnly)
		{
			continue;
		}

		ERshipPCGPulseMode EffectivePulseMode = Desc.PulseMode;
		if (EffectivePulseMode == ERshipPCGPulseMode::Off)
		{
			EffectivePulseMode = DefaultPulseMode;
		}

		if (EffectivePulseMode == ERshipPCGPulseMode::Off)
		{
			continue;
		}

		// Get current value
		const FRshipPCGPropertyDescriptor* DescPtr = &Desc;
		const UObject* Owner = nullptr;

		if (!FindPropertyAndOwner(Desc.PropertyName, DescPtr, Owner))
		{
			continue;
		}

		FProperty* Property = Desc.CachedProperty;
		if (!Property || !Owner)
		{
			continue;
		}

		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Owner);
		int32 ValueSize = Property->GetSize();

		bool bShouldPulse = false;

		switch (EffectivePulseMode)
		{
		case ERshipPCGPulseMode::OnChange:
			if (State.HasValueChanged(ValuePtr, ValueSize))
			{
				State.UpdateValue(ValuePtr, ValueSize);
				bShouldPulse = true;
			}
			break;

		case ERshipPCGPulseMode::FixedRate:
			if (Now >= State.NextPulseTime)
			{
				float Rate = Desc.PulseRateHz > 0.0f ? Desc.PulseRateHz : DefaultPulseRateHz;
				State.NextPulseTime = Now + (1.0 / Rate);
				bShouldPulse = true;
			}
			break;

		default:
			break;
		}

		if (bShouldPulse)
		{
			EmitPropertyPulse(i);
		}
	}
}

void URshipPCGAutoBindComponent::EmitPropertyPulse(int32 PropertyIndex)
{
	if (!ClassBindings || PropertyIndex < 0 || PropertyIndex >= ClassBindings->Properties.Num())
	{
		return;
	}

	const FRshipPCGPropertyDescriptor& Desc = ClassBindings->Properties[PropertyIndex];

	// Get owner and value
	const FRshipPCGPropertyDescriptor* DescPtr = &Desc;
	const UObject* Owner = nullptr;

	if (!FindPropertyAndOwner(Desc.PropertyName, DescPtr, Owner))
	{
		return;
	}

	FProperty* Property = Desc.CachedProperty;
	if (!Property || !Owner)
	{
		return;
	}

	// Convert to JSON
	TSharedPtr<FJsonValue> JsonValue = RshipPCGUtils::PropertyToJson(Property, Owner);
	if (!JsonValue.IsValid())
	{
		return;
	}

	// Build pulse data
	TSharedPtr<FJsonObject> PulseData = MakeShared<FJsonObject>();
	PulseData->SetField(Desc.DisplayName, JsonValue);

	// Emit through manager
	URshipPCGManager* PCGManager = Subsystem ? Subsystem->GetPCGManager() : nullptr;
	if (PCGManager)
	{
		FString EmitterId = FString::Printf(TEXT("%s:%s"), *InstanceId.TargetPath, *Desc.PropertyName.ToString());
		PCGManager->EmitPulse(this, EmitterId, PulseData);
	}
}

void URshipPCGAutoBindComponent::EmitPulse(FName PropertyName)
{
	if (!ClassBindings)
	{
		return;
	}

	for (int32 i = 0; i < ClassBindings->Properties.Num(); ++i)
	{
		if (ClassBindings->Properties[i].PropertyName == PropertyName)
		{
			EmitPropertyPulse(i);
			return;
		}
	}

	UE_LOG(LogRshipExec, Warning, TEXT("URshipPCGAutoBindComponent: Property %s not found for pulse emission"),
		*PropertyName.ToString());
}

void URshipPCGAutoBindComponent::EmitAllPulses()
{
	if (!ClassBindings)
	{
		return;
	}

	for (int32 i = 0; i < ClassBindings->Properties.Num(); ++i)
	{
		const FRshipPCGPropertyDescriptor& Desc = ClassBindings->Properties[i];
		if (Desc.Access != ERshipPCGPropertyAccess::WriteOnly)
		{
			EmitPropertyPulse(i);
		}
	}
}

void URshipPCGAutoBindComponent::HandleAction(const FString& ActionId, const TSharedPtr<FJsonObject>& Data)
{
	if (!ClassBindings)
	{
		return;
	}

	// Parse action ID to get property name
	// ActionId format: "targetPath:PropertyName"
	int32 ColonIndex = INDEX_NONE;
	ActionId.FindLastChar(':', ColonIndex);

	FString PropertyNameStr = (ColonIndex != INDEX_NONE)
		? ActionId.RightChop(ColonIndex + 1)
		: ActionId;

	FName PropertyName(*PropertyNameStr);

	// Find property descriptor and owner
	FRshipPCGPropertyDescriptor* Desc = nullptr;
	UObject* Owner = nullptr;

	if (!FindPropertyAndOwner(PropertyName, Desc, Owner))
	{
		UE_LOG(LogRshipExec, Warning, TEXT("URshipPCGAutoBindComponent: Property %s not found for action"),
			*PropertyNameStr);
		return;
	}

	// Apply the action
	if (ApplyActionToProperty(*Desc, Owner, Data))
	{
		// Fire events
		OnRshipParamChanged.Broadcast(PropertyName);

		FString DataString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&DataString);
		if (Data.IsValid())
		{
			FJsonSerializer::Serialize(Data.ToSharedRef(), Writer);
		}
		OnRshipActionReceived.Broadcast(PropertyName, DataString);

		UE_LOG(LogRshipExec, Verbose, TEXT("URshipPCGAutoBindComponent: Applied action %s to %s"),
			*ActionId, *GetOwner()->GetName());
	}
}

bool URshipPCGAutoBindComponent::FindPropertyAndOwner(FName PropertyName, FRshipPCGPropertyDescriptor*& OutDesc, UObject*& OutOwner)
{
	if (!ClassBindings)
	{
		return false;
	}

	OutDesc = ClassBindings->FindProperty(PropertyName);
	if (!OutDesc)
	{
		return false;
	}

	TWeakObjectPtr<UObject>* OwnerPtr = PropertyOwners.Find(PropertyName);
	if (!OwnerPtr || !OwnerPtr->IsValid())
	{
		// Default to owner actor
		OutOwner = GetOwner();
	}
	else
	{
		OutOwner = OwnerPtr->Get();
	}

	return OutOwner != nullptr;
}

bool URshipPCGAutoBindComponent::FindPropertyAndOwner(FName PropertyName, const FRshipPCGPropertyDescriptor*& OutDesc, const UObject*& OutOwner) const
{
	if (!ClassBindings)
	{
		return false;
	}

	OutDesc = ClassBindings->FindProperty(PropertyName);
	if (!OutDesc)
	{
		return false;
	}

	const TWeakObjectPtr<UObject>* OwnerPtr = PropertyOwners.Find(PropertyName);
	if (!OwnerPtr || !OwnerPtr->IsValid())
	{
		OutOwner = GetOwner();
	}
	else
	{
		OutOwner = OwnerPtr->Get();
	}

	return OutOwner != nullptr;
}

bool URshipPCGAutoBindComponent::ApplyActionToProperty(const FRshipPCGPropertyDescriptor& Desc, UObject* Owner, const TSharedPtr<FJsonObject>& Data)
{
	if (!Owner || !Desc.CachedProperty)
	{
		return false;
	}

	// Check if property is writable
	if (Desc.Access == ERshipPCGPropertyAccess::ReadOnly)
	{
		UE_LOG(LogRshipExec, Warning, TEXT("URshipPCGAutoBindComponent: Property %s is read-only"),
			*Desc.PropertyName.ToString());
		return false;
	}

	// Get value from JSON
	// Try by display name first, then property name
	TSharedPtr<FJsonValue> JsonValue = Data->TryGetField(Desc.DisplayName);
	if (!JsonValue.IsValid())
	{
		JsonValue = Data->TryGetField(Desc.PropertyName.ToString());
	}

	// If still not found, use entire data object for struct types
	if (!JsonValue.IsValid())
	{
		if (Desc.PropertyType == ERshipPCGPropertyType::Struct ||
			Desc.PropertyType == ERshipPCGPropertyType::Vector ||
			Desc.PropertyType == ERshipPCGPropertyType::Rotator ||
			Desc.PropertyType == ERshipPCGPropertyType::LinearColor ||
			Desc.PropertyType == ERshipPCGPropertyType::Transform)
		{
			JsonValue = MakeShared<FJsonValueObject>(Data);
		}
	}

	if (!JsonValue.IsValid())
	{
		UE_LOG(LogRshipExec, Warning, TEXT("URshipPCGAutoBindComponent: No value found for property %s"),
			*Desc.PropertyName.ToString());
		return false;
	}

	// Apply to property
	bool bSuccess = RshipPCGUtils::JsonToProperty(Desc.CachedProperty, Owner, JsonValue);

	if (bSuccess && Desc.bHasRange)
	{
		void* ValuePtr = Desc.CachedProperty->ContainerPtrToValuePtr<void>(Owner);
		ClampPropertyValue(Desc, ValuePtr);
	}

	return bSuccess;
}

void URshipPCGAutoBindComponent::ClampPropertyValue(const FRshipPCGPropertyDescriptor& Desc, void* ValuePtr)
{
	if (!Desc.bHasRange || !ValuePtr)
	{
		return;
	}

	switch (Desc.PropertyType)
	{
	case ERshipPCGPropertyType::Float:
	{
		float* Value = static_cast<float*>(ValuePtr);
		*Value = FMath::Clamp(*Value, Desc.MinValue, Desc.MaxValue);
		break;
	}
	case ERshipPCGPropertyType::Double:
	{
		double* Value = static_cast<double*>(ValuePtr);
		*Value = FMath::Clamp(*Value, static_cast<double>(Desc.MinValue), static_cast<double>(Desc.MaxValue));
		break;
	}
	case ERshipPCGPropertyType::Int32:
	{
		int32* Value = static_cast<int32*>(ValuePtr);
		*Value = FMath::Clamp(*Value, static_cast<int32>(Desc.MinValue), static_cast<int32>(Desc.MaxValue));
		break;
	}
	default:
		// Other types don't support range clamping
		break;
	}
}

// ============================================================================
// TYPE-SAFE PROPERTY ACCESS
// ============================================================================

FString URshipPCGAutoBindComponent::GetPropertyValueAsString(FName PropertyName) const
{
	const FRshipPCGPropertyDescriptor* Desc = nullptr;
	const UObject* Owner = nullptr;

	if (!FindPropertyAndOwner(PropertyName, Desc, Owner))
	{
		return TEXT("");
	}

	FProperty* Property = Desc->CachedProperty;
	if (!Property)
	{
		return TEXT("");
	}

	const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Owner);

	FString Result;
	Property->ExportTextItem_Direct(Result, ValuePtr, nullptr, nullptr, PPF_None);
	return Result;
}

bool URshipPCGAutoBindComponent::SetPropertyValueFromString(FName PropertyName, const FString& Value)
{
	FRshipPCGPropertyDescriptor* Desc = nullptr;
	UObject* Owner = nullptr;

	if (!FindPropertyAndOwner(PropertyName, Desc, Owner))
	{
		return false;
	}

	FProperty* Property = Desc->CachedProperty;
	if (!Property)
	{
		return false;
	}

	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Owner);
	const TCHAR* Result = Property->ImportText_Direct(*Value, ValuePtr, Owner, PPF_None);

	return Result != nullptr && *Result == 0;
}

FString URshipPCGAutoBindComponent::GetPropertyValueAsJson(FName PropertyName) const
{
	const FRshipPCGPropertyDescriptor* Desc = nullptr;
	const UObject* Owner = nullptr;

	if (!FindPropertyAndOwner(PropertyName, Desc, Owner))
	{
		return TEXT("null");
	}

	TSharedPtr<FJsonValue> JsonValue = RshipPCGUtils::PropertyToJson(Desc->CachedProperty, Owner);
	if (!JsonValue.IsValid())
	{
		return TEXT("null");
	}

	FString Result;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Result);
	FJsonSerializer::Serialize(JsonValue.ToSharedRef(), TEXT(""), Writer);
	return Result;
}

bool URshipPCGAutoBindComponent::SetPropertyValueFromJson(FName PropertyName, const FString& JsonValueStr)
{
	FRshipPCGPropertyDescriptor* Desc = nullptr;
	UObject* Owner = nullptr;

	if (!FindPropertyAndOwner(PropertyName, Desc, Owner))
	{
		return false;
	}

	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonValueStr);
	TSharedPtr<FJsonValue> JsonValue;

	if (!FJsonSerializer::Deserialize(Reader, JsonValue) || !JsonValue.IsValid())
	{
		return false;
	}

	return RshipPCGUtils::JsonToProperty(Desc->CachedProperty, Owner, JsonValue);
}

float URshipPCGAutoBindComponent::GetFloatProperty(FName PropertyName, bool& bSuccess) const
{
	bSuccess = false;

	const FRshipPCGPropertyDescriptor* Desc = nullptr;
	const UObject* Owner = nullptr;

	if (!FindPropertyAndOwner(PropertyName, Desc, Owner))
	{
		return 0.0f;
	}

	if (Desc->PropertyType != ERshipPCGPropertyType::Float &&
		Desc->PropertyType != ERshipPCGPropertyType::Double)
	{
		return 0.0f;
	}

	FProperty* Property = Desc->CachedProperty;
	const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Owner);

	bSuccess = true;

	if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		return FloatProp->GetPropertyValue(ValuePtr);
	}
	if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
	{
		return static_cast<float>(DoubleProp->GetPropertyValue(ValuePtr));
	}

	bSuccess = false;
	return 0.0f;
}

bool URshipPCGAutoBindComponent::SetFloatProperty(FName PropertyName, float Value)
{
	FRshipPCGPropertyDescriptor* Desc = nullptr;
	UObject* Owner = nullptr;

	if (!FindPropertyAndOwner(PropertyName, Desc, Owner))
	{
		return false;
	}

	if (Desc->PropertyType != ERshipPCGPropertyType::Float &&
		Desc->PropertyType != ERshipPCGPropertyType::Double)
	{
		return false;
	}

	FProperty* Property = Desc->CachedProperty;
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Owner);

	if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		if (Desc->bHasRange)
		{
			Value = FMath::Clamp(Value, Desc->MinValue, Desc->MaxValue);
		}
		FloatProp->SetPropertyValue(ValuePtr, Value);
		return true;
	}
	if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
	{
		if (Desc->bHasRange)
		{
			Value = FMath::Clamp(Value, Desc->MinValue, Desc->MaxValue);
		}
		DoubleProp->SetPropertyValue(ValuePtr, static_cast<double>(Value));
		return true;
	}

	return false;
}

int32 URshipPCGAutoBindComponent::GetIntProperty(FName PropertyName, bool& bSuccess) const
{
	bSuccess = false;

	const FRshipPCGPropertyDescriptor* Desc = nullptr;
	const UObject* Owner = nullptr;

	if (!FindPropertyAndOwner(PropertyName, Desc, Owner))
	{
		return 0;
	}

	if (Desc->PropertyType != ERshipPCGPropertyType::Int32)
	{
		return 0;
	}

	FProperty* Property = Desc->CachedProperty;
	const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Owner);

	if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		bSuccess = true;
		return IntProp->GetPropertyValue(ValuePtr);
	}

	return 0;
}

bool URshipPCGAutoBindComponent::SetIntProperty(FName PropertyName, int32 Value)
{
	FRshipPCGPropertyDescriptor* Desc = nullptr;
	UObject* Owner = nullptr;

	if (!FindPropertyAndOwner(PropertyName, Desc, Owner))
	{
		return false;
	}

	if (Desc->PropertyType != ERshipPCGPropertyType::Int32)
	{
		return false;
	}

	FProperty* Property = Desc->CachedProperty;
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Owner);

	if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		if (Desc->bHasRange)
		{
			Value = FMath::Clamp(Value, static_cast<int32>(Desc->MinValue), static_cast<int32>(Desc->MaxValue));
		}
		IntProp->SetPropertyValue(ValuePtr, Value);
		return true;
	}

	return false;
}

bool URshipPCGAutoBindComponent::GetBoolProperty(FName PropertyName, bool& bSuccess) const
{
	bSuccess = false;

	const FRshipPCGPropertyDescriptor* Desc = nullptr;
	const UObject* Owner = nullptr;

	if (!FindPropertyAndOwner(PropertyName, Desc, Owner))
	{
		return false;
	}

	if (Desc->PropertyType != ERshipPCGPropertyType::Bool)
	{
		return false;
	}

	FProperty* Property = Desc->CachedProperty;
	const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Owner);

	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		bSuccess = true;
		return BoolProp->GetPropertyValue(ValuePtr);
	}

	return false;
}

bool URshipPCGAutoBindComponent::SetBoolProperty(FName PropertyName, bool Value)
{
	FRshipPCGPropertyDescriptor* Desc = nullptr;
	UObject* Owner = nullptr;

	if (!FindPropertyAndOwner(PropertyName, Desc, Owner))
	{
		return false;
	}

	if (Desc->PropertyType != ERshipPCGPropertyType::Bool)
	{
		return false;
	}

	FProperty* Property = Desc->CachedProperty;
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Owner);

	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		BoolProp->SetPropertyValue(ValuePtr, Value);
		return true;
	}

	return false;
}

FVector URshipPCGAutoBindComponent::GetVectorProperty(FName PropertyName, bool& bSuccess) const
{
	bSuccess = false;

	const FRshipPCGPropertyDescriptor* Desc = nullptr;
	const UObject* Owner = nullptr;

	if (!FindPropertyAndOwner(PropertyName, Desc, Owner))
	{
		return FVector::ZeroVector;
	}

	if (Desc->PropertyType != ERshipPCGPropertyType::Vector)
	{
		return FVector::ZeroVector;
	}

	FProperty* Property = Desc->CachedProperty;
	const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Owner);

	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		if (StructProp->Struct == TBaseStructure<FVector>::Get())
		{
			bSuccess = true;
			return *static_cast<const FVector*>(ValuePtr);
		}
	}

	return FVector::ZeroVector;
}

bool URshipPCGAutoBindComponent::SetVectorProperty(FName PropertyName, FVector Value)
{
	FRshipPCGPropertyDescriptor* Desc = nullptr;
	UObject* Owner = nullptr;

	if (!FindPropertyAndOwner(PropertyName, Desc, Owner))
	{
		return false;
	}

	if (Desc->PropertyType != ERshipPCGPropertyType::Vector)
	{
		return false;
	}

	FProperty* Property = Desc->CachedProperty;
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Owner);

	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		if (StructProp->Struct == TBaseStructure<FVector>::Get())
		{
			*static_cast<FVector*>(ValuePtr) = Value;
			return true;
		}
	}

	return false;
}

FRotator URshipPCGAutoBindComponent::GetRotatorProperty(FName PropertyName, bool& bSuccess) const
{
	bSuccess = false;

	const FRshipPCGPropertyDescriptor* Desc = nullptr;
	const UObject* Owner = nullptr;

	if (!FindPropertyAndOwner(PropertyName, Desc, Owner))
	{
		return FRotator::ZeroRotator;
	}

	if (Desc->PropertyType != ERshipPCGPropertyType::Rotator)
	{
		return FRotator::ZeroRotator;
	}

	FProperty* Property = Desc->CachedProperty;
	const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Owner);

	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		if (StructProp->Struct == TBaseStructure<FRotator>::Get())
		{
			bSuccess = true;
			return *static_cast<const FRotator*>(ValuePtr);
		}
	}

	return FRotator::ZeroRotator;
}

bool URshipPCGAutoBindComponent::SetRotatorProperty(FName PropertyName, FRotator Value)
{
	FRshipPCGPropertyDescriptor* Desc = nullptr;
	UObject* Owner = nullptr;

	if (!FindPropertyAndOwner(PropertyName, Desc, Owner))
	{
		return false;
	}

	if (Desc->PropertyType != ERshipPCGPropertyType::Rotator)
	{
		return false;
	}

	FProperty* Property = Desc->CachedProperty;
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Owner);

	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		if (StructProp->Struct == TBaseStructure<FRotator>::Get())
		{
			*static_cast<FRotator*>(ValuePtr) = Value;
			return true;
		}
	}

	return false;
}

FLinearColor URshipPCGAutoBindComponent::GetColorProperty(FName PropertyName, bool& bSuccess) const
{
	bSuccess = false;

	const FRshipPCGPropertyDescriptor* Desc = nullptr;
	const UObject* Owner = nullptr;

	if (!FindPropertyAndOwner(PropertyName, Desc, Owner))
	{
		return FLinearColor::Black;
	}

	if (Desc->PropertyType != ERshipPCGPropertyType::LinearColor &&
		Desc->PropertyType != ERshipPCGPropertyType::Color)
	{
		return FLinearColor::Black;
	}

	FProperty* Property = Desc->CachedProperty;
	const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Owner);

	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		if (StructProp->Struct == TBaseStructure<FLinearColor>::Get())
		{
			bSuccess = true;
			return *static_cast<const FLinearColor*>(ValuePtr);
		}
		if (StructProp->Struct == TBaseStructure<FColor>::Get())
		{
			bSuccess = true;
			return FLinearColor(*static_cast<const FColor*>(ValuePtr));
		}
	}

	return FLinearColor::Black;
}

bool URshipPCGAutoBindComponent::SetColorProperty(FName PropertyName, FLinearColor Value)
{
	FRshipPCGPropertyDescriptor* Desc = nullptr;
	UObject* Owner = nullptr;

	if (!FindPropertyAndOwner(PropertyName, Desc, Owner))
	{
		return false;
	}

	if (Desc->PropertyType != ERshipPCGPropertyType::LinearColor &&
		Desc->PropertyType != ERshipPCGPropertyType::Color)
	{
		return false;
	}

	FProperty* Property = Desc->CachedProperty;
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Owner);

	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		if (StructProp->Struct == TBaseStructure<FLinearColor>::Get())
		{
			*static_cast<FLinearColor*>(ValuePtr) = Value;
			return true;
		}
		if (StructProp->Struct == TBaseStructure<FColor>::Get())
		{
			*static_cast<FColor*>(ValuePtr) = Value.ToFColor(true);
			return true;
		}
	}

	return false;
}
