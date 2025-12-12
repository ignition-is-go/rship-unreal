// Copyright Rocketship. All Rights Reserved.

#include "ExternalProcessor/ExternalProcessorTypes.h"

// ============================================================================
// FOSCArgument
// ============================================================================

FOSCArgument FOSCArgument::MakeInt(int32 Value)
{
	FOSCArgument Arg;
	Arg.Type = EOSCArgumentType::Int32;
	Arg.IntValue = Value;
	return Arg;
}

FOSCArgument FOSCArgument::MakeFloat(float Value)
{
	FOSCArgument Arg;
	Arg.Type = EOSCArgumentType::Float;
	Arg.FloatValue = Value;
	return Arg;
}

FOSCArgument FOSCArgument::MakeString(const FString& Value)
{
	FOSCArgument Arg;
	Arg.Type = EOSCArgumentType::String;
	Arg.StringValue = Value;
	return Arg;
}

// ============================================================================
// FOSCMessage
// ============================================================================

void FOSCMessage::AddInt(int32 Value)
{
	Arguments.Add(FOSCArgument::MakeInt(Value));
}

void FOSCMessage::AddFloat(float Value)
{
	Arguments.Add(FOSCArgument::MakeFloat(Value));
}

void FOSCMessage::AddString(const FString& Value)
{
	Arguments.Add(FOSCArgument::MakeString(Value));
}

// Helper: Pad to 4-byte boundary
static void PadTo4Bytes(TArray<uint8>& Buffer)
{
	while (Buffer.Num() % 4 != 0)
	{
		Buffer.Add(0);
	}
}

// Helper: Write int32 big-endian
static void WriteInt32BE(TArray<uint8>& Buffer, int32 Value)
{
	Buffer.Add((Value >> 24) & 0xFF);
	Buffer.Add((Value >> 16) & 0xFF);
	Buffer.Add((Value >> 8) & 0xFF);
	Buffer.Add(Value & 0xFF);
}

// Helper: Write float big-endian
static void WriteFloatBE(TArray<uint8>& Buffer, float Value)
{
	union { float f; int32 i; } u;
	u.f = Value;
	WriteInt32BE(Buffer, u.i);
}

// Helper: Write OSC string (null-terminated, padded to 4 bytes)
static void WriteOSCString(TArray<uint8>& Buffer, const FString& Str)
{
	FTCHARToUTF8 UTF8String(*Str);
	const char* Data = UTF8String.Get();
	int32 Len = UTF8String.Length();

	for (int32 i = 0; i < Len; ++i)
	{
		Buffer.Add(static_cast<uint8>(Data[i]));
	}
	Buffer.Add(0);  // Null terminator
	PadTo4Bytes(Buffer);
}

// Helper: Read int32 big-endian
static int32 ReadInt32BE(const uint8* Data)
{
	return (Data[0] << 24) | (Data[1] << 16) | (Data[2] << 8) | Data[3];
}

// Helper: Read float big-endian
static float ReadFloatBE(const uint8* Data)
{
	union { float f; int32 i; } u;
	u.i = ReadInt32BE(Data);
	return u.f;
}

// Helper: Read OSC string
static FString ReadOSCString(const uint8* Data, int32 MaxLen, int32& OutBytesRead)
{
	FString Result;
	int32 i = 0;

	while (i < MaxLen && Data[i] != 0)
	{
		Result.AppendChar(static_cast<TCHAR>(Data[i]));
		++i;
	}

	// Skip null terminator and padding
	++i;
	while (i % 4 != 0)
	{
		++i;
	}

	OutBytesRead = i;
	return Result;
}

TArray<uint8> FOSCMessage::Serialize() const
{
	TArray<uint8> Buffer;

	// Write address
	WriteOSCString(Buffer, Address);

	// Build type tag string
	FString TypeTag = TEXT(",");
	for (const FOSCArgument& Arg : Arguments)
	{
		switch (Arg.Type)
		{
		case EOSCArgumentType::Int32:
			TypeTag.AppendChar('i');
			break;
		case EOSCArgumentType::Float:
			TypeTag.AppendChar('f');
			break;
		case EOSCArgumentType::String:
			TypeTag.AppendChar('s');
			break;
		case EOSCArgumentType::Blob:
			TypeTag.AppendChar('b');
			break;
		case EOSCArgumentType::BoolTrue:
			TypeTag.AppendChar('T');
			break;
		case EOSCArgumentType::BoolFalse:
			TypeTag.AppendChar('F');
			break;
		case EOSCArgumentType::Nil:
			TypeTag.AppendChar('N');
			break;
		case EOSCArgumentType::Int64:
			TypeTag.AppendChar('h');
			break;
		case EOSCArgumentType::Double:
			TypeTag.AppendChar('d');
			break;
		default:
			break;
		}
	}
	WriteOSCString(Buffer, TypeTag);

	// Write argument data
	for (const FOSCArgument& Arg : Arguments)
	{
		switch (Arg.Type)
		{
		case EOSCArgumentType::Int32:
			WriteInt32BE(Buffer, Arg.IntValue);
			break;

		case EOSCArgumentType::Float:
			WriteFloatBE(Buffer, Arg.FloatValue);
			break;

		case EOSCArgumentType::String:
			WriteOSCString(Buffer, Arg.StringValue);
			break;

		case EOSCArgumentType::Blob:
			WriteInt32BE(Buffer, Arg.BlobValue.Num());
			Buffer.Append(Arg.BlobValue);
			PadTo4Bytes(Buffer);
			break;

		case EOSCArgumentType::Int64:
			// 64-bit big-endian
			WriteInt32BE(Buffer, static_cast<int32>(Arg.IntValue >> 32));
			WriteInt32BE(Buffer, static_cast<int32>(Arg.IntValue & 0xFFFFFFFF));
			break;

		case EOSCArgumentType::Double:
			{
				union { double d; int64 i; } u;
				u.d = static_cast<double>(Arg.FloatValue);
				WriteInt32BE(Buffer, static_cast<int32>(u.i >> 32));
				WriteInt32BE(Buffer, static_cast<int32>(u.i & 0xFFFFFFFF));
			}
			break;

		// True, False, Nil have no data
		default:
			break;
		}
	}

	return Buffer;
}

bool FOSCMessage::Parse(const TArray<uint8>& Data, FOSCMessage& OutMessage)
{
	if (Data.Num() < 4)
	{
		return false;
	}

	const uint8* Ptr = Data.GetData();
	int32 Remaining = Data.Num();
	int32 BytesRead = 0;

	// Read address
	OutMessage.Address = ReadOSCString(Ptr, Remaining, BytesRead);
	if (OutMessage.Address.IsEmpty() || OutMessage.Address[0] != '/')
	{
		return false;
	}
	Ptr += BytesRead;
	Remaining -= BytesRead;

	// Read type tag
	if (Remaining < 4)
	{
		return false;
	}

	FString TypeTag = ReadOSCString(Ptr, Remaining, BytesRead);
	if (TypeTag.IsEmpty() || TypeTag[0] != ',')
	{
		return false;
	}
	Ptr += BytesRead;
	Remaining -= BytesRead;

	// Parse arguments based on type tag
	OutMessage.Arguments.Empty();
	for (int32 i = 1; i < TypeTag.Len(); ++i)
	{
		TCHAR TypeChar = TypeTag[i];
		FOSCArgument Arg;

		switch (TypeChar)
		{
		case 'i':
			if (Remaining < 4) return false;
			Arg.Type = EOSCArgumentType::Int32;
			Arg.IntValue = ReadInt32BE(Ptr);
			Ptr += 4;
			Remaining -= 4;
			break;

		case 'f':
			if (Remaining < 4) return false;
			Arg.Type = EOSCArgumentType::Float;
			Arg.FloatValue = ReadFloatBE(Ptr);
			Ptr += 4;
			Remaining -= 4;
			break;

		case 's':
			Arg.Type = EOSCArgumentType::String;
			Arg.StringValue = ReadOSCString(Ptr, Remaining, BytesRead);
			Ptr += BytesRead;
			Remaining -= BytesRead;
			break;

		case 'b':
			{
				if (Remaining < 4) return false;
				int32 BlobSize = ReadInt32BE(Ptr);
				Ptr += 4;
				Remaining -= 4;
				if (Remaining < BlobSize) return false;
				Arg.Type = EOSCArgumentType::Blob;
				Arg.BlobValue.SetNum(BlobSize);
				FMemory::Memcpy(Arg.BlobValue.GetData(), Ptr, BlobSize);
				int32 PaddedSize = (BlobSize + 3) & ~3;
				Ptr += PaddedSize;
				Remaining -= PaddedSize;
			}
			break;

		case 'T':
			Arg.Type = EOSCArgumentType::BoolTrue;
			break;

		case 'F':
			Arg.Type = EOSCArgumentType::BoolFalse;
			break;

		case 'N':
			Arg.Type = EOSCArgumentType::Nil;
			break;

		default:
			// Unknown type, skip
			continue;
		}

		OutMessage.Arguments.Add(Arg);
	}

	return true;
}

// ============================================================================
// FOSCBundle
// ============================================================================

TArray<uint8> FOSCBundle::Serialize() const
{
	TArray<uint8> Buffer;

	// Bundle header
	WriteOSCString(Buffer, TEXT("#bundle"));

	// Time tag (NTP format: 64-bit)
	WriteInt32BE(Buffer, static_cast<int32>(TimeTag >> 32));
	WriteInt32BE(Buffer, static_cast<int32>(TimeTag & 0xFFFFFFFF));

	// Bundle elements
	for (const FOSCMessage& Message : Messages)
	{
		TArray<uint8> MessageData = Message.Serialize();
		WriteInt32BE(Buffer, MessageData.Num());
		Buffer.Append(MessageData);
	}

	return Buffer;
}

bool FOSCBundle::Parse(const TArray<uint8>& Data, FOSCBundle& OutBundle)
{
	if (Data.Num() < 16)  // "#bundle" + timetag
	{
		return false;
	}

	const uint8* Ptr = Data.GetData();
	int32 Remaining = Data.Num();
	int32 BytesRead = 0;

	// Check header
	FString Header = ReadOSCString(Ptr, Remaining, BytesRead);
	if (Header != TEXT("#bundle"))
	{
		return false;
	}
	Ptr += BytesRead;
	Remaining -= BytesRead;

	// Read time tag
	if (Remaining < 8)
	{
		return false;
	}
	int64 HighBits = static_cast<int64>(ReadInt32BE(Ptr)) << 32;
	int64 LowBits = static_cast<uint32>(ReadInt32BE(Ptr + 4));
	OutBundle.TimeTag = HighBits | LowBits;
	Ptr += 8;
	Remaining -= 8;

	// Read bundle elements
	OutBundle.Messages.Empty();
	while (Remaining >= 4)
	{
		int32 ElementSize = ReadInt32BE(Ptr);
		Ptr += 4;
		Remaining -= 4;

		if (ElementSize <= 0 || ElementSize > Remaining)
		{
			break;
		}

		// Check if nested bundle or message
		if (ElementSize >= 8 && Ptr[0] == '#')
		{
			// Nested bundle - skip for now (could recurse)
		}
		else
		{
			TArray<uint8> ElementData;
			ElementData.SetNum(ElementSize);
			FMemory::Memcpy(ElementData.GetData(), Ptr, ElementSize);

			FOSCMessage Message;
			if (FOSCMessage::Parse(ElementData, Message))
			{
				OutBundle.Messages.Add(Message);
			}
		}

		Ptr += ElementSize;
		Remaining -= ElementSize;
	}

	return true;
}

// ============================================================================
// FProcessorCoordinateMapping
// ============================================================================

FVector FProcessorCoordinateMapping::ConvertPosition(const FVector& UnrealPosition) const
{
	// Apply origin offset
	FVector Relative = UnrealPosition - OriginOffset;

	// Apply rotation
	FVector Rotated = CoordinateRotation.RotateVector(Relative);

	// Apply scale
	FVector Scaled = Rotated * ScaleFactor;

	// Apply axis mapping and inversion
	FVector Result;
	Result.X = Scaled[AxisMapping.X] * AxisInvert.X;
	Result.Y = Scaled[AxisMapping.Y] * AxisInvert.Y;
	Result.Z = Scaled[AxisMapping.Z] * AxisInvert.Z;

	// Convert to target coordinate system if needed
	if (CoordinateSystem == EProcessorCoordinateSystem::Spherical)
	{
		// Convert Cartesian to Spherical (azimuth, elevation, distance)
		float Distance = Result.Size();
		float Azimuth = FMath::Atan2(Result.Y, Result.X) * (180.0f / PI);
		float Elevation = (Distance > KINDA_SMALL_NUMBER)
			? FMath::Asin(Result.Z / Distance) * (180.0f / PI)
			: 0.0f;
		return FVector(Azimuth, Elevation, Distance);
	}
	else if (CoordinateSystem == EProcessorCoordinateSystem::Polar)
	{
		// Convert Cartesian to Polar 2D (angle, distance)
		float Distance = FVector2D(Result.X, Result.Y).Size();
		float Angle = FMath::Atan2(Result.Y, Result.X) * (180.0f / PI);
		return FVector(Angle, Distance, Result.Z);
	}
	else if (CoordinateSystem == EProcessorCoordinateSystem::Normalized)
	{
		// Assume unit cube normalization - would need bounds
		// For now, just clamp to 0-1
		return FVector(
			FMath::Clamp(Result.X, 0.0f, 1.0f),
			FMath::Clamp(Result.Y, 0.0f, 1.0f),
			FMath::Clamp(Result.Z, 0.0f, 1.0f)
		);
	}

	return Result;
}

FVector FProcessorCoordinateMapping::ConvertPositionToUnreal(const FVector& ProcessorPosition) const
{
	FVector Cartesian = ProcessorPosition;

	// Convert from source coordinate system
	if (CoordinateSystem == EProcessorCoordinateSystem::Spherical)
	{
		// Convert Spherical (azimuth, elevation, distance) to Cartesian
		float Azimuth = ProcessorPosition.X * (PI / 180.0f);
		float Elevation = ProcessorPosition.Y * (PI / 180.0f);
		float Distance = ProcessorPosition.Z;

		float CosElev = FMath::Cos(Elevation);
		Cartesian.X = Distance * CosElev * FMath::Cos(Azimuth);
		Cartesian.Y = Distance * CosElev * FMath::Sin(Azimuth);
		Cartesian.Z = Distance * FMath::Sin(Elevation);
	}
	else if (CoordinateSystem == EProcessorCoordinateSystem::Polar)
	{
		// Convert Polar 2D to Cartesian
		float Angle = ProcessorPosition.X * (PI / 180.0f);
		float Distance = ProcessorPosition.Y;
		Cartesian.X = Distance * FMath::Cos(Angle);
		Cartesian.Y = Distance * FMath::Sin(Angle);
		Cartesian.Z = ProcessorPosition.Z;
	}

	// Reverse axis mapping and inversion
	FVector Unmapped;
	// Find which source axis maps to each target axis
	for (int32 i = 0; i < 3; ++i)
	{
		if (AxisMapping.X == i) Unmapped[i] = Cartesian.X / AxisInvert.X;
		else if (AxisMapping.Y == i) Unmapped[i] = Cartesian.Y / AxisInvert.Y;
		else if (AxisMapping.Z == i) Unmapped[i] = Cartesian.Z / AxisInvert.Z;
	}

	// Reverse scale
	FVector Unscaled = Unmapped / ScaleFactor;

	// Reverse rotation
	FVector Unrotated = CoordinateRotation.GetInverse().RotateVector(Unscaled);

	// Reverse origin offset
	return Unrotated + OriginOffset;
}
