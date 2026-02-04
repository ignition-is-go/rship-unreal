// Rship PCG Auto-Bind Types Implementation

#include "PCG/RshipPCGTypes.h"
#include "Logs.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"

// ============================================================================
// FRshipPCGInstanceId
// ============================================================================

void FRshipPCGInstanceId::GenerateStableGuid()
{
	uint32 Hash = RshipPCGUtils::HashPCGPoint(
		PCGComponentGuid,
		SourceKey,
		PointIndex,
		QuantizedDistance,
		Seed);

	// Create deterministic GUID from hash
	// Use hash as basis and spread across GUID components
	uint32 A = Hash;
	uint32 B = HashCombine(Hash, GetTypeHash(PCGComponentGuid));
	uint32 C = HashCombine(Hash, GetTypeHash(SourceKey));
	uint32 D = HashCombine(Hash, PointIndex);

	StableGuid = FGuid(A, B, C, D);
}

void FRshipPCGInstanceId::BuildTargetPath()
{
	// Format: /pcg/{PCGComponentGuid}/{SourceKey}/{PointKey}
	// PointKey is a compact representation of point identity

	FString PointKey;
	if (PointIndex >= 0)
	{
		PointKey = FString::Printf(TEXT("p%d"), PointIndex);
	}
	else
	{
		// Use quantized distance + seed as fallback
		PointKey = FString::Printf(TEXT("d%lld_s%d"), QuantizedDistance, Seed);
	}

	TargetPath = FString::Printf(TEXT("/pcg/%s/%s/%s"),
		*PCGComponentGuid.ToString(EGuidFormats::DigitsLower),
		*SourceKey,
		*PointKey);
}

bool FRshipPCGInstanceId::IsValid() const
{
	return StableGuid.IsValid() && !TargetPath.IsEmpty();
}

bool FRshipPCGInstanceId::operator==(const FRshipPCGInstanceId& Other) const
{
	return StableGuid == Other.StableGuid;
}

uint32 GetTypeHash(const FRshipPCGInstanceId& Id)
{
	return GetTypeHash(Id.StableGuid);
}

FRshipPCGInstanceId FRshipPCGInstanceId::FromPCGPoint(
	const FGuid& InPCGComponentGuid,
	const FString& InSourceKey,
	int32 InPointIndex,
	double InDistanceAlong,
	double InAlpha,
	int32 InSeed,
	const FString& InOptionalDisplayName)
{
	FRshipPCGInstanceId Id;
	Id.PCGComponentGuid = InPCGComponentGuid;
	Id.SourceKey = InSourceKey;
	Id.PointIndex = InPointIndex;
	Id.QuantizedDistance = RshipPCGUtils::QuantizeDistance(InDistanceAlong);
	Id.QuantizedAlpha = RshipPCGUtils::QuantizeAlpha(InAlpha);
	Id.Seed = InSeed;

	if (!InOptionalDisplayName.IsEmpty())
	{
		Id.DisplayName = InOptionalDisplayName;
	}
	else
	{
		// Generate display name from identity
		if (InPointIndex >= 0)
		{
			Id.DisplayName = FString::Printf(TEXT("PCG_%s_%d"), *InSourceKey, InPointIndex);
		}
		else
		{
			Id.DisplayName = FString::Printf(TEXT("PCG_%s_%.2f"), *InSourceKey, InAlpha);
		}
	}

	Id.GenerateStableGuid();
	Id.BuildTargetPath();

	return Id;
}

// ============================================================================
// FRshipPCGPropertyDescriptor
// ============================================================================

FRshipPCGPropertyDescriptor FRshipPCGPropertyDescriptor::FromProperty(FProperty* Property)
{
	FRshipPCGPropertyDescriptor Desc;
	if (!Property)
	{
		return Desc;
	}

	Desc.PropertyName = Property->GetFName();
	Desc.CachedProperty = Property;
	Desc.PropertyOffset = Property->GetOffset_ForInternal();
	Desc.PropertyType = DeterminePropertyType(Property);

	// Get UE type name
	Desc.UnrealTypeName = Property->GetCPPType();

	// Parse rShip metadata
	Desc.ParseMetadata(Property);

	return Desc;
}

ERshipPCGPropertyType FRshipPCGPropertyDescriptor::DeterminePropertyType(FProperty* Property)
{
	if (!Property)
	{
		return ERshipPCGPropertyType::Unknown;
	}

	// Check specific property types
	if (Property->IsA<FBoolProperty>())
	{
		return ERshipPCGPropertyType::Bool;
	}
	if (Property->IsA<FIntProperty>())
	{
		return ERshipPCGPropertyType::Int32;
	}
	if (Property->IsA<FInt64Property>())
	{
		return ERshipPCGPropertyType::Int64;
	}
	if (Property->IsA<FFloatProperty>())
	{
		return ERshipPCGPropertyType::Float;
	}
	if (Property->IsA<FDoubleProperty>())
	{
		return ERshipPCGPropertyType::Double;
	}
	if (Property->IsA<FStrProperty>())
	{
		return ERshipPCGPropertyType::String;
	}
	if (Property->IsA<FNameProperty>())
	{
		return ERshipPCGPropertyType::Name;
	}
	if (Property->IsA<FTextProperty>())
	{
		return ERshipPCGPropertyType::Text;
	}
	if (Property->IsA<FEnumProperty>() || Property->IsA<FByteProperty>())
	{
		// Check if byte property is an enum
		if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
		{
			if (ByteProp->Enum)
			{
				return ERshipPCGPropertyType::Enum;
			}
		}
		else if (Property->IsA<FEnumProperty>())
		{
			return ERshipPCGPropertyType::Enum;
		}
		return ERshipPCGPropertyType::Int32;
	}
	if (Property->IsA<FObjectProperty>())
	{
		return ERshipPCGPropertyType::Object;
	}

	// Check struct properties
	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		UScriptStruct* Struct = StructProp->Struct;
		if (Struct)
		{
			FName StructName = Struct->GetFName();

			if (StructName == NAME_Vector || StructName == TEXT("Vector3d"))
			{
				return ERshipPCGPropertyType::Vector;
			}
			if (StructName == NAME_Vector2D || StructName == TEXT("Vector2d"))
			{
				return ERshipPCGPropertyType::Vector2D;
			}
			if (StructName == NAME_Vector4 || StructName == TEXT("Vector4d"))
			{
				return ERshipPCGPropertyType::Vector4;
			}
			if (StructName == NAME_Rotator || StructName == TEXT("Rotator3d"))
			{
				return ERshipPCGPropertyType::Rotator;
			}
			if (StructName == NAME_Transform || StructName == TEXT("Transform3d"))
			{
				return ERshipPCGPropertyType::Transform;
			}
			if (StructName == NAME_Quat || StructName == TEXT("Quat4d"))
			{
				return ERshipPCGPropertyType::Quat;
			}
			if (StructName == NAME_LinearColor)
			{
				return ERshipPCGPropertyType::LinearColor;
			}
			if (StructName == NAME_Color)
			{
				return ERshipPCGPropertyType::Color;
			}

			return ERshipPCGPropertyType::Struct;
		}
	}

	return ERshipPCGPropertyType::Unknown;
}

void FRshipPCGPropertyDescriptor::ParseMetadata(FProperty* Property)
{
	if (!Property)
	{
		return;
	}

	FString Name;
	bool bReadable = true;
	bool bWritable = true;
	FString CategoryStr;
	float Min = 0.0f;
	float Max = 1.0f;
	bool bHasRangeLocal = false;
	ERshipPCGPulseMode LocalPulseMode = ERshipPCGPulseMode::Off;
	float LocalPulseRate = 10.0f;

	RshipPCGUtils::ParseRshipMetadata(
		Property,
		Name,
		bReadable,
		bWritable,
		CategoryStr,
		Min, Max,
		bHasRangeLocal,
		LocalPulseMode,
		LocalPulseRate);

	// Set display name
	if (!Name.IsEmpty())
	{
		DisplayName = Name;
	}
	else
	{
		// Use property name, removing RS_ prefix if present
		DisplayName = Property->GetName();
		if (DisplayName.StartsWith(TEXT("RS_")))
		{
			DisplayName = DisplayName.RightChop(3);
		}
	}

	// Set access mode
	if (bReadable && bWritable)
	{
		Access = ERshipPCGPropertyAccess::ReadWrite;
	}
	else if (bReadable)
	{
		Access = ERshipPCGPropertyAccess::ReadOnly;
	}
	else if (bWritable)
	{
		Access = ERshipPCGPropertyAccess::WriteOnly;
	}

	Category = CategoryStr;
	MinValue = Min;
	MaxValue = Max;
	bHasRange = bHasRangeLocal;
	PulseMode = LocalPulseMode;
	PulseRateHz = LocalPulseRate;

	// Get description from tooltip if available (editor-only)
#if WITH_EDITORONLY_DATA
	if (Property->HasMetaData(TEXT("ToolTip")))
	{
		Description = Property->GetMetaData(TEXT("ToolTip"));
	}
#endif

	// Handle enum path
	if (PropertyType == ERshipPCGPropertyType::Enum)
	{
		if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
		{
			if (UEnum* Enum = EnumProp->GetEnum())
			{
				EnumPath = Enum->GetPathName();
			}
		}
		else if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
		{
			if (ByteProp->Enum)
			{
				EnumPath = ByteProp->Enum->GetPathName();
			}
		}
	}
}

FString FRshipPCGPropertyDescriptor::GetJsonSchemaType() const
{
	switch (PropertyType)
	{
	case ERshipPCGPropertyType::Bool:
		return TEXT("boolean");
	case ERshipPCGPropertyType::Int32:
	case ERshipPCGPropertyType::Int64:
	case ERshipPCGPropertyType::Enum:
		return TEXT("integer");
	case ERshipPCGPropertyType::Float:
	case ERshipPCGPropertyType::Double:
		return TEXT("number");
	case ERshipPCGPropertyType::String:
	case ERshipPCGPropertyType::Name:
	case ERshipPCGPropertyType::Text:
		return TEXT("string");
	case ERshipPCGPropertyType::Vector:
	case ERshipPCGPropertyType::Vector2D:
	case ERshipPCGPropertyType::Vector4:
	case ERshipPCGPropertyType::Rotator:
	case ERshipPCGPropertyType::Transform:
	case ERshipPCGPropertyType::Quat:
	case ERshipPCGPropertyType::LinearColor:
	case ERshipPCGPropertyType::Color:
	case ERshipPCGPropertyType::Struct:
		return TEXT("object");
	case ERshipPCGPropertyType::Object:
		return TEXT("string"); // Reference path
	default:
		return TEXT("any");
	}
}

// ============================================================================
// FRshipPCGClassBindings
// ============================================================================

void FRshipPCGClassBindings::BuildFromClass(UClass* InClass)
{
	if (!InClass)
	{
		bIsValid = false;
		return;
	}

	BoundClass = InClass;
	Properties.Empty();

	// Iterate all properties in class hierarchy
	for (TFieldIterator<FProperty> PropIt(InClass); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;

		// Check for rShip metadata
		if (!RshipPCGUtils::HasRshipMetadata(Property))
		{
			continue;
		}

		FRshipPCGPropertyDescriptor Desc = FRshipPCGPropertyDescriptor::FromProperty(Property);
		if (Desc.PropertyType != ERshipPCGPropertyType::Unknown)
		{
			Properties.Add(MoveTemp(Desc));
		}
	}

	LastBuildTime = FPlatformTime::Seconds();
	bIsValid = true;

	UE_LOG(LogRshipExec, Log, TEXT("Built PCG bindings for class %s: %d properties"),
		*InClass->GetName(), Properties.Num());
}

void FRshipPCGClassBindings::RebuildPropertyPointers()
{
	if (!BoundClass)
	{
		return;
	}

	UClass* Class = BoundClass;
	for (FRshipPCGPropertyDescriptor& Desc : Properties)
	{
		Desc.CachedProperty = Class->FindPropertyByName(Desc.PropertyName);
		if (Desc.CachedProperty)
		{
			Desc.PropertyOffset = Desc.CachedProperty->GetOffset_ForInternal();
		}
	}
}

FRshipPCGPropertyDescriptor* FRshipPCGClassBindings::FindProperty(FName PropertyName)
{
	for (FRshipPCGPropertyDescriptor& Desc : Properties)
	{
		if (Desc.PropertyName == PropertyName)
		{
			return &Desc;
		}
	}
	return nullptr;
}

const FRshipPCGPropertyDescriptor* FRshipPCGClassBindings::FindProperty(FName PropertyName) const
{
	for (const FRshipPCGPropertyDescriptor& Desc : Properties)
	{
		if (Desc.PropertyName == PropertyName)
		{
			return &Desc;
		}
	}
	return nullptr;
}

TArray<FRshipPCGPropertyDescriptor*> FRshipPCGClassBindings::GetReadableProperties()
{
	TArray<FRshipPCGPropertyDescriptor*> Result;
	for (FRshipPCGPropertyDescriptor& Desc : Properties)
	{
		if (Desc.Access == ERshipPCGPropertyAccess::ReadOnly ||
			Desc.Access == ERshipPCGPropertyAccess::ReadWrite)
		{
			Result.Add(&Desc);
		}
	}
	return Result;
}

TArray<FRshipPCGPropertyDescriptor*> FRshipPCGClassBindings::GetWritableProperties()
{
	TArray<FRshipPCGPropertyDescriptor*> Result;
	for (FRshipPCGPropertyDescriptor& Desc : Properties)
	{
		if (Desc.Access == ERshipPCGPropertyAccess::WriteOnly ||
			Desc.Access == ERshipPCGPropertyAccess::ReadWrite)
		{
			Result.Add(&Desc);
		}
	}
	return Result;
}

// ============================================================================
// FRshipPCGPropertyState
// ============================================================================

bool FRshipPCGPropertyState::HasValueChanged(const void* CurrentValue, int32 ValueSize) const
{
	if (LastValueBytes.Num() != ValueSize)
	{
		return true;
	}
	return FMemory::Memcmp(LastValueBytes.GetData(), CurrentValue, ValueSize) != 0;
}

void FRshipPCGPropertyState::UpdateValue(const void* CurrentValue, int32 ValueSize)
{
	LastValueBytes.SetNumUninitialized(ValueSize);
	FMemory::Memcpy(LastValueBytes.GetData(), CurrentValue, ValueSize);
	bValueChanged = false;
}

// ============================================================================
// FRshipPCGInstanceState
// ============================================================================

bool FRshipPCGInstanceState::IsValid() const
{
	return InstanceId.IsValid() && Actor.IsValid();
}

// ============================================================================
// RshipPCGUtils
// ============================================================================

namespace RshipPCGUtils
{

TSharedPtr<FJsonValue> PropertyToJson(FProperty* Property, const void* ContainerPtr)
{
	if (!Property || !ContainerPtr)
	{
		return nullptr;
	}

	const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(ContainerPtr);

	// Handle different property types
	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		return MakeShared<FJsonValueBoolean>(BoolProp->GetPropertyValue(ValuePtr));
	}
	if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		return MakeShared<FJsonValueNumber>(IntProp->GetPropertyValue(ValuePtr));
	}
	if (FInt64Property* Int64Prop = CastField<FInt64Property>(Property))
	{
		return MakeShared<FJsonValueNumber>(static_cast<double>(Int64Prop->GetPropertyValue(ValuePtr)));
	}
	if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		return MakeShared<FJsonValueNumber>(FloatProp->GetPropertyValue(ValuePtr));
	}
	if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
	{
		return MakeShared<FJsonValueNumber>(DoubleProp->GetPropertyValue(ValuePtr));
	}
	if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		return MakeShared<FJsonValueString>(StrProp->GetPropertyValue(ValuePtr));
	}
	if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		return MakeShared<FJsonValueString>(NameProp->GetPropertyValue(ValuePtr).ToString());
	}
	if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
	{
		return MakeShared<FJsonValueString>(TextProp->GetPropertyValue(ValuePtr).ToString());
	}

	// Handle struct properties
	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		UScriptStruct* Struct = StructProp->Struct;
		if (!Struct)
		{
			return nullptr;
		}

		TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();

		// Handle known struct types
		if (Struct == TBaseStructure<FVector>::Get())
		{
			const FVector* Vec = static_cast<const FVector*>(ValuePtr);
			JsonObj->SetNumberField(TEXT("x"), Vec->X);
			JsonObj->SetNumberField(TEXT("y"), Vec->Y);
			JsonObj->SetNumberField(TEXT("z"), Vec->Z);
		}
		else if (Struct == TBaseStructure<FVector2D>::Get())
		{
			const FVector2D* Vec = static_cast<const FVector2D*>(ValuePtr);
			JsonObj->SetNumberField(TEXT("x"), Vec->X);
			JsonObj->SetNumberField(TEXT("y"), Vec->Y);
		}
		else if (Struct == TBaseStructure<FRotator>::Get())
		{
			const FRotator* Rot = static_cast<const FRotator*>(ValuePtr);
			JsonObj->SetNumberField(TEXT("Pitch"), Rot->Pitch);
			JsonObj->SetNumberField(TEXT("Yaw"), Rot->Yaw);
			JsonObj->SetNumberField(TEXT("Roll"), Rot->Roll);
		}
		else if (Struct == TBaseStructure<FLinearColor>::Get())
		{
			const FLinearColor* Color = static_cast<const FLinearColor*>(ValuePtr);
			JsonObj->SetNumberField(TEXT("r"), Color->R);
			JsonObj->SetNumberField(TEXT("g"), Color->G);
			JsonObj->SetNumberField(TEXT("b"), Color->B);
			JsonObj->SetNumberField(TEXT("a"), Color->A);
		}
		else if (Struct == TBaseStructure<FColor>::Get())
		{
			const FColor* Color = static_cast<const FColor*>(ValuePtr);
			JsonObj->SetNumberField(TEXT("r"), Color->R / 255.0f);
			JsonObj->SetNumberField(TEXT("g"), Color->G / 255.0f);
			JsonObj->SetNumberField(TEXT("b"), Color->B / 255.0f);
			JsonObj->SetNumberField(TEXT("a"), Color->A / 255.0f);
		}
		else if (Struct == TBaseStructure<FTransform>::Get())
		{
			const FTransform* Trans = static_cast<const FTransform*>(ValuePtr);
			TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
			LocObj->SetNumberField(TEXT("x"), Trans->GetLocation().X);
			LocObj->SetNumberField(TEXT("y"), Trans->GetLocation().Y);
			LocObj->SetNumberField(TEXT("z"), Trans->GetLocation().Z);
			JsonObj->SetObjectField(TEXT("location"), LocObj);

			TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
			FRotator Rot = Trans->GetRotation().Rotator();
			RotObj->SetNumberField(TEXT("pitch"), Rot.Pitch);
			RotObj->SetNumberField(TEXT("yaw"), Rot.Yaw);
			RotObj->SetNumberField(TEXT("roll"), Rot.Roll);
			JsonObj->SetObjectField(TEXT("rotation"), RotObj);

			TSharedPtr<FJsonObject> ScaleObj = MakeShared<FJsonObject>();
			ScaleObj->SetNumberField(TEXT("x"), Trans->GetScale3D().X);
			ScaleObj->SetNumberField(TEXT("y"), Trans->GetScale3D().Y);
			ScaleObj->SetNumberField(TEXT("z"), Trans->GetScale3D().Z);
			JsonObj->SetObjectField(TEXT("scale"), ScaleObj);
		}
		else
		{
			// Generic struct - iterate properties
			for (TFieldIterator<FProperty> It(Struct); It; ++It)
			{
				FProperty* SubProp = *It;
				TSharedPtr<FJsonValue> SubValue = PropertyToJson(SubProp, ValuePtr);
				if (SubValue.IsValid())
				{
					JsonObj->SetField(SubProp->GetName(), SubValue);
				}
			}
		}

		return MakeShared<FJsonValueObject>(JsonObj);
	}

	// Handle enum
	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		UEnum* Enum = EnumProp->GetEnum();
		FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
		int64 EnumValue = UnderlyingProp->GetSignedIntPropertyValue(ValuePtr);

		if (Enum)
		{
			FString EnumName = Enum->GetNameStringByValue(EnumValue);
			return MakeShared<FJsonValueString>(EnumName);
		}
		return MakeShared<FJsonValueNumber>(static_cast<double>(EnumValue));
	}

	if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		if (ByteProp->Enum)
		{
			uint8 EnumValue = ByteProp->GetPropertyValue(ValuePtr);
			FString EnumName = ByteProp->Enum->GetNameStringByValue(EnumValue);
			return MakeShared<FJsonValueString>(EnumName);
		}
		return MakeShared<FJsonValueNumber>(ByteProp->GetPropertyValue(ValuePtr));
	}

	return nullptr;
}

bool JsonToProperty(FProperty* Property, void* ContainerPtr, const TSharedPtr<FJsonValue>& JsonValue)
{
	if (!Property || !ContainerPtr || !JsonValue.IsValid())
	{
		return false;
	}

	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(ContainerPtr);

	// Handle different property types
	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		bool Value = false;
		if (JsonValue->TryGetBool(Value))
		{
			BoolProp->SetPropertyValue(ValuePtr, Value);
			return true;
		}
	}
	else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		int32 Value = 0;
		if (JsonValue->TryGetNumber(Value))
		{
			IntProp->SetPropertyValue(ValuePtr, Value);
			return true;
		}
	}
	else if (FInt64Property* Int64Prop = CastField<FInt64Property>(Property))
	{
		int64 Value = 0;
		if (JsonValue->TryGetNumber(Value))
		{
			Int64Prop->SetPropertyValue(ValuePtr, Value);
			return true;
		}
	}
	else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		double Value = 0.0;
		if (JsonValue->TryGetNumber(Value))
		{
			FloatProp->SetPropertyValue(ValuePtr, static_cast<float>(Value));
			return true;
		}
	}
	else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
	{
		double Value = 0.0;
		if (JsonValue->TryGetNumber(Value))
		{
			DoubleProp->SetPropertyValue(ValuePtr, Value);
			return true;
		}
	}
	else if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		FString Value;
		if (JsonValue->TryGetString(Value))
		{
			StrProp->SetPropertyValue(ValuePtr, Value);
			return true;
		}
	}
	else if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		FString Value;
		if (JsonValue->TryGetString(Value))
		{
			NameProp->SetPropertyValue(ValuePtr, FName(*Value));
			return true;
		}
	}
	else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		const TSharedPtr<FJsonObject>* JsonObj = nullptr;
		if (!JsonValue->TryGetObject(JsonObj) || !JsonObj->IsValid())
		{
			return false;
		}

		UScriptStruct* Struct = StructProp->Struct;
		if (!Struct)
		{
			return false;
		}

		// Handle known struct types
		if (Struct == TBaseStructure<FVector>::Get())
		{
			FVector* Vec = static_cast<FVector*>(ValuePtr);
			Vec->X = (*JsonObj)->GetNumberField(TEXT("x"));
			Vec->Y = (*JsonObj)->GetNumberField(TEXT("y"));
			Vec->Z = (*JsonObj)->GetNumberField(TEXT("z"));
			return true;
		}
		else if (Struct == TBaseStructure<FVector2D>::Get())
		{
			FVector2D* Vec = static_cast<FVector2D*>(ValuePtr);
			Vec->X = (*JsonObj)->GetNumberField(TEXT("x"));
			Vec->Y = (*JsonObj)->GetNumberField(TEXT("y"));
			return true;
		}
		else if (Struct == TBaseStructure<FRotator>::Get())
		{
			FRotator* Rot = static_cast<FRotator*>(ValuePtr);
			// Support both PascalCase (Pitch) and lowercase (pitch) for flexibility
			if ((*JsonObj)->HasField(TEXT("Pitch")))
			{
				Rot->Pitch = (*JsonObj)->GetNumberField(TEXT("Pitch"));
				Rot->Yaw = (*JsonObj)->GetNumberField(TEXT("Yaw"));
				Rot->Roll = (*JsonObj)->GetNumberField(TEXT("Roll"));
			}
			else
			{
				Rot->Pitch = (*JsonObj)->GetNumberField(TEXT("pitch"));
				Rot->Yaw = (*JsonObj)->GetNumberField(TEXT("yaw"));
				Rot->Roll = (*JsonObj)->GetNumberField(TEXT("roll"));
			}
			return true;
		}
		else if (Struct == TBaseStructure<FLinearColor>::Get())
		{
			FLinearColor* Color = static_cast<FLinearColor*>(ValuePtr);
			Color->R = (*JsonObj)->GetNumberField(TEXT("r"));
			Color->G = (*JsonObj)->GetNumberField(TEXT("g"));
			Color->B = (*JsonObj)->GetNumberField(TEXT("b"));
			Color->A = (*JsonObj)->HasField(TEXT("a")) ? (*JsonObj)->GetNumberField(TEXT("a")) : 1.0f;
			return true;
		}
		else if (Struct == TBaseStructure<FColor>::Get())
		{
			FColor* Color = static_cast<FColor*>(ValuePtr);
			Color->R = FMath::Clamp(static_cast<int32>((*JsonObj)->GetNumberField(TEXT("r")) * 255.0f), 0, 255);
			Color->G = FMath::Clamp(static_cast<int32>((*JsonObj)->GetNumberField(TEXT("g")) * 255.0f), 0, 255);
			Color->B = FMath::Clamp(static_cast<int32>((*JsonObj)->GetNumberField(TEXT("b")) * 255.0f), 0, 255);
			Color->A = (*JsonObj)->HasField(TEXT("a"))
				? FMath::Clamp(static_cast<int32>((*JsonObj)->GetNumberField(TEXT("a")) * 255.0f), 0, 255)
				: 255;
			return true;
		}
		else if (Struct == TBaseStructure<FTransform>::Get())
		{
			FTransform* Trans = static_cast<FTransform*>(ValuePtr);

			if ((*JsonObj)->HasField(TEXT("location")))
			{
				const TSharedPtr<FJsonObject>& LocObj = (*JsonObj)->GetObjectField(TEXT("location"));
				FVector Loc(
					LocObj->GetNumberField(TEXT("x")),
					LocObj->GetNumberField(TEXT("y")),
					LocObj->GetNumberField(TEXT("z")));
				Trans->SetLocation(Loc);
			}

			if ((*JsonObj)->HasField(TEXT("rotation")))
			{
				const TSharedPtr<FJsonObject>& RotObj = (*JsonObj)->GetObjectField(TEXT("rotation"));
				FRotator Rot(
					RotObj->GetNumberField(TEXT("pitch")),
					RotObj->GetNumberField(TEXT("yaw")),
					RotObj->GetNumberField(TEXT("roll")));
				Trans->SetRotation(Rot.Quaternion());
			}

			if ((*JsonObj)->HasField(TEXT("scale")))
			{
				const TSharedPtr<FJsonObject>& ScaleObj = (*JsonObj)->GetObjectField(TEXT("scale"));
				FVector Scale(
					ScaleObj->GetNumberField(TEXT("x")),
					ScaleObj->GetNumberField(TEXT("y")),
					ScaleObj->GetNumberField(TEXT("z")));
				Trans->SetScale3D(Scale);
			}

			return true;
		}
	}
	else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		UEnum* Enum = EnumProp->GetEnum();
		FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();

		FString StringValue;
		if (JsonValue->TryGetString(StringValue) && Enum)
		{
			int64 EnumValue = Enum->GetValueByNameString(StringValue);
			if (EnumValue != INDEX_NONE)
			{
				UnderlyingProp->SetIntPropertyValue(ValuePtr, EnumValue);
				return true;
			}
		}

		double NumericValue = 0.0;
		if (JsonValue->TryGetNumber(NumericValue))
		{
			UnderlyingProp->SetIntPropertyValue(ValuePtr, static_cast<int64>(NumericValue));
			return true;
		}
	}
	else if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		if (ByteProp->Enum)
		{
			FString StringValue;
			if (JsonValue->TryGetString(StringValue))
			{
				int64 EnumValue = ByteProp->Enum->GetValueByNameString(StringValue);
				if (EnumValue != INDEX_NONE)
				{
					ByteProp->SetPropertyValue(ValuePtr, static_cast<uint8>(EnumValue));
					return true;
				}
			}
		}

		double NumericValue = 0.0;
		if (JsonValue->TryGetNumber(NumericValue))
		{
			ByteProp->SetPropertyValue(ValuePtr, static_cast<uint8>(NumericValue));
			return true;
		}
	}

	return false;
}

int64 QuantizeDistance(double Distance)
{
	// Quantize to 0.1mm resolution (multiply by 10000 to get 0.01cm = 0.1mm)
	return static_cast<int64>(FMath::RoundToDouble(Distance * 10000.0));
}

int32 QuantizeAlpha(double Alpha)
{
	// Quantize to 0.01% resolution
	return static_cast<int32>(FMath::RoundToDouble(FMath::Clamp(Alpha, 0.0, 1.0) * 10000.0));
}

uint32 HashPCGPoint(
	const FGuid& PCGComponentGuid,
	const FString& SourceKey,
	int32 PointIndex,
	int64 QuantizedDistance,
	int32 Seed)
{
	uint32 Hash = GetTypeHash(PCGComponentGuid);
	Hash = HashCombine(Hash, GetTypeHash(SourceKey));
	Hash = HashCombine(Hash, GetTypeHash(PointIndex));
	Hash = HashCombine(Hash, GetTypeHash(QuantizedDistance));
	Hash = HashCombine(Hash, GetTypeHash(Seed));
	return Hash;
}

bool HasRshipMetadata(FProperty* Property)
{
	if (!Property)
	{
		return false;
	}

#if WITH_EDITORONLY_DATA
	// Check for explicit RShipParam metadata
	if (Property->HasMetaData(RshipPCGMetaKeys::Param))
	{
		return true;
	}
#endif

	// Also accept RS_ prefixed properties (legacy compatibility)
	if (Property->GetName().StartsWith(TEXT("RS_")))
	{
		return true;
	}

	return false;
}

void ParseRshipMetadata(
	FProperty* Property,
	FString& OutName,
	bool& bOutReadable,
	bool& bOutWritable,
	FString& OutCategory,
	float& OutMin,
	float& OutMax,
	bool& bOutHasRange,
	ERshipPCGPulseMode& OutPulseMode,
	float& OutPulseRate)
{
	// Defaults
	OutName = TEXT("");
	bOutReadable = true;
	bOutWritable = true;
	OutCategory = TEXT("");
	OutMin = 0.0f;
	OutMax = 1.0f;
	bOutHasRange = false;
	OutPulseMode = ERshipPCGPulseMode::Off;
	OutPulseRate = 10.0f;

	if (!Property)
	{
		return;
	}

#if WITH_EDITORONLY_DATA
	// Parse RShipParam (can be empty for default name, or contain custom name)
	if (Property->HasMetaData(RshipPCGMetaKeys::Param))
	{
		OutName = Property->GetMetaData(RshipPCGMetaKeys::Param);
	}

	// Parse readable/writable
	if (Property->HasMetaData(RshipPCGMetaKeys::Readable))
	{
		FString ReadableStr = Property->GetMetaData(RshipPCGMetaKeys::Readable);
		bOutReadable = ReadableStr.IsEmpty() || ReadableStr.ToBool();
	}

	if (Property->HasMetaData(RshipPCGMetaKeys::Writable))
	{
		FString WritableStr = Property->GetMetaData(RshipPCGMetaKeys::Writable);
		bOutWritable = WritableStr.IsEmpty() || WritableStr.ToBool();
	}

	// Parse category
	if (Property->HasMetaData(RshipPCGMetaKeys::Category))
	{
		OutCategory = Property->GetMetaData(RshipPCGMetaKeys::Category);
	}

	// Parse min/max
	if (Property->HasMetaData(RshipPCGMetaKeys::Min))
	{
		FString MinStr = Property->GetMetaData(RshipPCGMetaKeys::Min);
		OutMin = FCString::Atof(*MinStr);
		bOutHasRange = true;
	}

	if (Property->HasMetaData(RshipPCGMetaKeys::Max))
	{
		FString MaxStr = Property->GetMetaData(RshipPCGMetaKeys::Max);
		OutMax = FCString::Atof(*MaxStr);
		bOutHasRange = true;
	}

	// Also check UE's standard ClampMin/ClampMax
	if (Property->HasMetaData(TEXT("ClampMin")))
	{
		FString MinStr = Property->GetMetaData(TEXT("ClampMin"));
		OutMin = FCString::Atof(*MinStr);
		bOutHasRange = true;
	}

	if (Property->HasMetaData(TEXT("ClampMax")))
	{
		FString MaxStr = Property->GetMetaData(TEXT("ClampMax"));
		OutMax = FCString::Atof(*MaxStr);
		bOutHasRange = true;
	}

	// Parse pulse mode
	if (Property->HasMetaData(RshipPCGMetaKeys::PulseMode))
	{
		FString PulseModeStr = Property->GetMetaData(RshipPCGMetaKeys::PulseMode).ToLower();
		if (PulseModeStr == TEXT("onchange") || PulseModeStr == TEXT("on_change"))
		{
			OutPulseMode = ERshipPCGPulseMode::OnChange;
		}
		else if (PulseModeStr == TEXT("fixedrate") || PulseModeStr == TEXT("fixed_rate"))
		{
			OutPulseMode = ERshipPCGPulseMode::FixedRate;
		}
		else
		{
			OutPulseMode = ERshipPCGPulseMode::Off;
		}
	}

	// Parse pulse rate
	if (Property->HasMetaData(RshipPCGMetaKeys::PulseRate))
	{
		FString RateStr = Property->GetMetaData(RshipPCGMetaKeys::PulseRate);
		OutPulseRate = FMath::Clamp(FCString::Atof(*RateStr), 0.1f, 60.0f);
	}
#endif // WITH_EDITORONLY_DATA
}

} // namespace RshipPCGUtils
