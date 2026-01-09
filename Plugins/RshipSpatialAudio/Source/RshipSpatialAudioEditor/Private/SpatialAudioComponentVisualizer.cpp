// Copyright Rocketship. All Rights Reserved.

#include "SpatialAudioComponentVisualizer.h"
#include "SpatialAudioVisualizerComponent.h"
#include "Components/SpatialSpeakerComponent.h"
#include "Components/SpatialAudioSourceComponent.h"
#include "RshipSpatialAudioManager.h"
#include "Core/SpatialSpeaker.h"
#include "Core/SpatialZone.h"
#include "Core/SpatialAudioObject.h"
#include "SceneManagement.h"
#include "CanvasItem.h"

// ============================================================================
// FSpatialAudioComponentVisualizer
// ============================================================================

FSpatialAudioComponentVisualizer::FSpatialAudioComponentVisualizer()
{
}

FSpatialAudioComponentVisualizer::~FSpatialAudioComponentVisualizer()
{
}

void FSpatialAudioComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const USpatialAudioVisualizerComponent* Visualizer = Cast<USpatialAudioVisualizerComponent>(Component);
	if (!Visualizer)
	{
		return;
	}

	URshipSpatialAudioManager* Manager = Visualizer->GetAudioManager();
	if (!Manager)
	{
		return;
	}

	// Draw speakers
	if (Visualizer->bShowSpeakers)
	{
		DrawSpeakers(Visualizer, View, PDI);
	}

	// Draw zones
	if (Visualizer->bShowZones)
	{
		DrawZones(Visualizer, View, PDI);
	}

	// Draw audio objects
	if (Visualizer->bShowAudioObjects)
	{
		DrawAudioObjects(Visualizer, View, PDI);
	}

	// Draw routing lines
	if (Visualizer->bShowRoutingLines)
	{
		DrawRoutingLines(Visualizer, View, PDI);
	}
}

void FSpatialAudioComponentVisualizer::DrawSpeakers(const USpatialAudioVisualizerComponent* Visualizer, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	URshipSpatialAudioManager* Manager = Visualizer->GetAudioManager();
	if (!Manager)
	{
		return;
	}

	TArray<FSpatialSpeaker> Speakers = Manager->GetAllSpeakers();
	for (const FSpatialSpeaker& Speaker : Speakers)
	{
		// Determine color based on state
		FLinearColor Color = Speaker.Color;
		if (Speaker.DSP.bMuted)
		{
			Color = Visualizer->MutedSpeakerColor;
		}
		else if (Speaker.Type == ESpatialSpeakerType::Subwoofer)
		{
			Color = Visualizer->SubwooferColor;
		}
		else if (Color == FLinearColor::White)
		{
			Color = Visualizer->SpeakerColor;
		}

		// Get meter level
		float MeterLevel = Speaker.LastMeterReading.Peak;

		// Label
		FString Label = Speaker.Label.IsEmpty() ? Speaker.Name : Speaker.Label;

		DrawSpeaker(
			Speaker.WorldPosition,
			Speaker.Orientation,
			Visualizer->SpeakerSize,
			Speaker.NominalDispersionH,
			Speaker.NominalDispersionV,
			Color,
			Visualizer->bShowCoveragePatterns,
			Visualizer->CoverageOpacity,
			Label,
			Visualizer->bShowSpeakerLabels,
			MeterLevel,
			Visualizer->bShowMetering,
			PDI
		);
	}
}

void FSpatialAudioComponentVisualizer::DrawSpeaker(
	const FVector& Position,
	const FRotator& Orientation,
	float Size,
	float DispersionH,
	float DispersionV,
	const FLinearColor& Color,
	bool bShowCoverage,
	float CoverageOpacity,
	const FString& Label,
	bool bShowLabel,
	float MeterLevel,
	bool bShowMeter,
	FPrimitiveDrawInterface* PDI)
{
	const FColor DrawColor = Color.ToFColor(true);
	const float LineThickness = 2.0f;
	const uint8 DepthPriority = SDPG_World;

	// Draw speaker box
	FVector Forward = Orientation.Vector();
	FVector Right = FRotationMatrix(Orientation).GetUnitAxis(EAxis::Y);
	FVector Up = FRotationMatrix(Orientation).GetUnitAxis(EAxis::Z);

	float HalfSize = Size * 0.5f;

	// Speaker cabinet outline (front face larger)
	TArray<FVector> CabinetPoints;
	CabinetPoints.Add(Position + Forward * HalfSize + Right * HalfSize * 0.8f + Up * HalfSize);  // Front top right
	CabinetPoints.Add(Position + Forward * HalfSize + Right * HalfSize * 0.8f - Up * HalfSize);  // Front bottom right
	CabinetPoints.Add(Position + Forward * HalfSize - Right * HalfSize * 0.8f - Up * HalfSize);  // Front bottom left
	CabinetPoints.Add(Position + Forward * HalfSize - Right * HalfSize * 0.8f + Up * HalfSize);  // Front top left
	CabinetPoints.Add(Position - Forward * HalfSize * 0.3f + Right * HalfSize * 0.6f + Up * HalfSize * 0.8f);  // Back top right
	CabinetPoints.Add(Position - Forward * HalfSize * 0.3f + Right * HalfSize * 0.6f - Up * HalfSize * 0.8f);  // Back bottom right
	CabinetPoints.Add(Position - Forward * HalfSize * 0.3f - Right * HalfSize * 0.6f - Up * HalfSize * 0.8f);  // Back bottom left
	CabinetPoints.Add(Position - Forward * HalfSize * 0.3f - Right * HalfSize * 0.6f + Up * HalfSize * 0.8f);  // Back top left

	// Front face
	PDI->DrawLine(CabinetPoints[0], CabinetPoints[1], DrawColor, DepthPriority, LineThickness);
	PDI->DrawLine(CabinetPoints[1], CabinetPoints[2], DrawColor, DepthPriority, LineThickness);
	PDI->DrawLine(CabinetPoints[2], CabinetPoints[3], DrawColor, DepthPriority, LineThickness);
	PDI->DrawLine(CabinetPoints[3], CabinetPoints[0], DrawColor, DepthPriority, LineThickness);

	// Back face
	PDI->DrawLine(CabinetPoints[4], CabinetPoints[5], DrawColor, DepthPriority, LineThickness);
	PDI->DrawLine(CabinetPoints[5], CabinetPoints[6], DrawColor, DepthPriority, LineThickness);
	PDI->DrawLine(CabinetPoints[6], CabinetPoints[7], DrawColor, DepthPriority, LineThickness);
	PDI->DrawLine(CabinetPoints[7], CabinetPoints[4], DrawColor, DepthPriority, LineThickness);

	// Connecting edges
	PDI->DrawLine(CabinetPoints[0], CabinetPoints[4], DrawColor, DepthPriority, LineThickness);
	PDI->DrawLine(CabinetPoints[1], CabinetPoints[5], DrawColor, DepthPriority, LineThickness);
	PDI->DrawLine(CabinetPoints[2], CabinetPoints[6], DrawColor, DepthPriority, LineThickness);
	PDI->DrawLine(CabinetPoints[3], CabinetPoints[7], DrawColor, DepthPriority, LineThickness);

	// Direction arrow
	FVector ArrowEnd = Position + Forward * Size * 1.5f;
	PDI->DrawLine(Position, ArrowEnd, DrawColor, DepthPriority, LineThickness);
	PDI->DrawLine(ArrowEnd, ArrowEnd - Forward * Size * 0.3f + Right * Size * 0.15f, DrawColor, DepthPriority, LineThickness);
	PDI->DrawLine(ArrowEnd, ArrowEnd - Forward * Size * 0.3f - Right * Size * 0.15f, DrawColor, DepthPriority, LineThickness);

	// Draw coverage pattern
	if (bShowCoverage && CoverageOpacity > 0.0f)
	{
		DrawCoverageCone(Position, Forward, DispersionH, DispersionV, Size * 4.0f, Color, CoverageOpacity, PDI);
	}

	// Draw meter bar
	if (bShowMeter && MeterLevel > 0.01f)
	{
		DrawMeterBar(Position + Up * Size * 1.2f, MeterLevel, Size * 0.8f, Size * 0.15f, Up, Right, PDI);
	}
}

void FSpatialAudioComponentVisualizer::DrawZones(const USpatialAudioVisualizerComponent* Visualizer, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	// Zone visualization would iterate through zones and draw their boundaries
	// For now, this is a placeholder for zone boundary visualization
}

void FSpatialAudioComponentVisualizer::DrawAudioObjects(const USpatialAudioVisualizerComponent* Visualizer, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	URshipSpatialAudioManager* Manager = Visualizer->GetAudioManager();
	if (!Manager)
	{
		return;
	}

	TArray<FSpatialAudioObject> Objects = Manager->GetAllAudioObjects();
	for (const FSpatialAudioObject& Object : Objects)
	{
		DrawAudioObject(
			Object.Position,
			Object.Spread,
			Visualizer->ObjectSize,
			Visualizer->ObjectColor,
			Object.Name,
			PDI
		);
	}
}

void FSpatialAudioComponentVisualizer::DrawAudioObject(
	const FVector& Position,
	float Spread,
	float Size,
	const FLinearColor& Color,
	const FString& Name,
	FPrimitiveDrawInterface* PDI)
{
	const FColor DrawColor = Color.ToFColor(true);
	const float LineThickness = 2.0f;
	const uint8 DepthPriority = SDPG_World;

	// Draw sphere/circle to represent audio object
	const int32 NumSegments = 16;
	const float Radius = Size * 0.5f;

	// Draw three circles (XY, XZ, YZ planes)
	for (int32 i = 0; i < NumSegments; ++i)
	{
		float Angle1 = (float(i) / NumSegments) * 2.0f * PI;
		float Angle2 = (float(i + 1) / NumSegments) * 2.0f * PI;

		// XY plane
		FVector P1XY = Position + FVector(FMath::Cos(Angle1) * Radius, FMath::Sin(Angle1) * Radius, 0.0f);
		FVector P2XY = Position + FVector(FMath::Cos(Angle2) * Radius, FMath::Sin(Angle2) * Radius, 0.0f);
		PDI->DrawLine(P1XY, P2XY, DrawColor, DepthPriority, LineThickness);

		// XZ plane
		FVector P1XZ = Position + FVector(FMath::Cos(Angle1) * Radius, 0.0f, FMath::Sin(Angle1) * Radius);
		FVector P2XZ = Position + FVector(FMath::Cos(Angle2) * Radius, 0.0f, FMath::Sin(Angle2) * Radius);
		PDI->DrawLine(P1XZ, P2XZ, DrawColor, DepthPriority, LineThickness);

		// YZ plane
		FVector P1YZ = Position + FVector(0.0f, FMath::Cos(Angle1) * Radius, FMath::Sin(Angle1) * Radius);
		FVector P2YZ = Position + FVector(0.0f, FMath::Cos(Angle2) * Radius, FMath::Sin(Angle2) * Radius);
		PDI->DrawLine(P1YZ, P2YZ, DrawColor, DepthPriority, LineThickness);
	}

	// Draw spread indicator (larger circle on ground)
	if (Spread > 0.0f)
	{
		float SpreadRadius = Radius + (Spread / 180.0f) * Size * 2.0f;
		FColor SpreadColor = FColor(DrawColor.R, DrawColor.G, DrawColor.B, 128);

		for (int32 i = 0; i < NumSegments; ++i)
		{
			float Angle1 = (float(i) / NumSegments) * 2.0f * PI;
			float Angle2 = (float(i + 1) / NumSegments) * 2.0f * PI;

			FVector P1 = Position + FVector(FMath::Cos(Angle1) * SpreadRadius, FMath::Sin(Angle1) * SpreadRadius, 0.0f);
			FVector P2 = Position + FVector(FMath::Cos(Angle2) * SpreadRadius, FMath::Sin(Angle2) * SpreadRadius, 0.0f);
			PDI->DrawLine(P1, P2, SpreadColor, DepthPriority, LineThickness * 0.5f);
		}
	}
}

void FSpatialAudioComponentVisualizer::DrawRoutingLines(const USpatialAudioVisualizerComponent* Visualizer, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	// This would draw lines from audio objects to the speakers they're routed to
	// based on the renderer's computed gains. Left as placeholder for future implementation.
}

void FSpatialAudioComponentVisualizer::DrawCoverageCone(
	const FVector& Position,
	const FVector& Direction,
	float HorizontalAngle,
	float VerticalAngle,
	float Length,
	const FLinearColor& Color,
	float Opacity,
	FPrimitiveDrawInterface* PDI)
{
	const uint8 DepthPriority = SDPG_World;
	FColor DrawColor = Color.ToFColor(true);
	DrawColor.A = FMath::Clamp(static_cast<uint8>(Opacity * 255.0f), (uint8)0, (uint8)128);

	// Calculate cone edges
	float HalfH = FMath::DegreesToRadians(HorizontalAngle * 0.5f);
	float HalfV = FMath::DegreesToRadians(VerticalAngle * 0.5f);

	FRotator DirRotation = Direction.Rotation();
	FVector Right = FRotationMatrix(DirRotation).GetUnitAxis(EAxis::Y);
	FVector Up = FRotationMatrix(DirRotation).GetUnitAxis(EAxis::Z);

	// Draw cone outline
	const int32 NumHSegments = 8;
	const int32 NumVSegments = 4;

	for (int32 h = 0; h < NumHSegments; ++h)
	{
		float HAngle1 = FMath::Lerp(-HalfH, HalfH, float(h) / NumHSegments);
		float HAngle2 = FMath::Lerp(-HalfH, HalfH, float(h + 1) / NumHSegments);

		for (int32 v = 0; v < NumVSegments; ++v)
		{
			float VAngle1 = FMath::Lerp(-HalfV, HalfV, float(v) / NumVSegments);
			float VAngle2 = FMath::Lerp(-HalfV, HalfV, float(v + 1) / NumVSegments);

			// Calculate cone surface points
			FVector Dir1 = (Direction + Right * FMath::Tan(HAngle1) + Up * FMath::Tan(VAngle1)).GetSafeNormal();
			FVector Dir2 = (Direction + Right * FMath::Tan(HAngle2) + Up * FMath::Tan(VAngle1)).GetSafeNormal();

			FVector P1 = Position + Dir1 * Length;
			FVector P2 = Position + Dir2 * Length;

			PDI->DrawLine(P1, P2, DrawColor, DepthPriority, 1.0f);
		}
	}

	// Draw cone edges from origin
	FVector CornerTL = (Direction + Right * FMath::Tan(-HalfH) + Up * FMath::Tan(HalfV)).GetSafeNormal() * Length + Position;
	FVector CornerTR = (Direction + Right * FMath::Tan(HalfH) + Up * FMath::Tan(HalfV)).GetSafeNormal() * Length + Position;
	FVector CornerBL = (Direction + Right * FMath::Tan(-HalfH) + Up * FMath::Tan(-HalfV)).GetSafeNormal() * Length + Position;
	FVector CornerBR = (Direction + Right * FMath::Tan(HalfH) + Up * FMath::Tan(-HalfV)).GetSafeNormal() * Length + Position;

	PDI->DrawLine(Position, CornerTL, DrawColor, DepthPriority, 1.0f);
	PDI->DrawLine(Position, CornerTR, DrawColor, DepthPriority, 1.0f);
	PDI->DrawLine(Position, CornerBL, DrawColor, DepthPriority, 1.0f);
	PDI->DrawLine(Position, CornerBR, DrawColor, DepthPriority, 1.0f);
}

void FSpatialAudioComponentVisualizer::DrawMeterBar(
	const FVector& Position,
	float Level,
	float MaxHeight,
	float Width,
	const FVector& UpVector,
	const FVector& RightVector,
	FPrimitiveDrawInterface* PDI)
{
	const uint8 DepthPriority = SDPG_World;
	float Height = FMath::Clamp(Level, 0.0f, 1.0f) * MaxHeight;

	// Background (dark)
	FVector BgStart = Position - RightVector * Width * 0.5f;
	FVector BgEnd = BgStart + UpVector * MaxHeight;
	PDI->DrawLine(BgStart, BgEnd, FColor(40, 40, 40), DepthPriority, Width);

	// Level (green to yellow to red)
	if (Height > 0.0f)
	{
		FColor LevelColor;
		if (Level < 0.7f)
		{
			LevelColor = FColor::Green;
		}
		else if (Level < 0.9f)
		{
			LevelColor = FColor::Yellow;
		}
		else
		{
			LevelColor = FColor::Red;
		}

		FVector LevelEnd = BgStart + UpVector * Height;
		PDI->DrawLine(BgStart, LevelEnd, LevelColor, DepthPriority, Width * 0.8f);
	}
}

// ============================================================================
// FSpatialSpeakerComponentVisualizer
// ============================================================================

void FSpatialSpeakerComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const USpatialSpeakerComponent* SpeakerComp = Cast<USpatialSpeakerComponent>(Component);
	if (!SpeakerComp)
	{
		return;
	}

	AActor* Owner = SpeakerComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	const FVector Position = Owner->GetActorLocation();
	const FRotator Orientation = Owner->GetActorRotation() + SpeakerComp->AimOffset;
	const float Size = 50.0f;

	// Determine color based on speaker type and state
	FLinearColor Color;
	switch (SpeakerComp->SpeakerType)
	{
	case ESpatialSpeakerType::Subwoofer:
		Color = FLinearColor(0.8f, 0.4f, 0.1f);  // Orange for subs
		break;
	case ESpatialSpeakerType::Monitor:
		Color = FLinearColor(0.5f, 0.8f, 0.5f);  // Light green for monitors
		break;
	default:
		Color = FLinearColor(0.2f, 0.8f, 0.2f);  // Green for main speakers
		break;
	}

	if (SpeakerComp->bStartMuted)
	{
		Color = FLinearColor(0.5f, 0.5f, 0.5f);
	}

	const FColor DrawColor = Color.ToFColor(true);
	const float LineThickness = 2.0f;
	const uint8 DepthPriority = SDPG_World;

	// Draw simple speaker representation
	FVector Forward = Orientation.Vector();
	FVector Right = FRotationMatrix(Orientation).GetUnitAxis(EAxis::Y);
	FVector Up = FRotationMatrix(Orientation).GetUnitAxis(EAxis::Z);

	float HalfSize = Size * 0.5f;

	// Speaker box front face
	TArray<FVector> FrontFace;
	FrontFace.Add(Position + Forward * HalfSize + Right * HalfSize * 0.8f + Up * HalfSize);
	FrontFace.Add(Position + Forward * HalfSize + Right * HalfSize * 0.8f - Up * HalfSize);
	FrontFace.Add(Position + Forward * HalfSize - Right * HalfSize * 0.8f - Up * HalfSize);
	FrontFace.Add(Position + Forward * HalfSize - Right * HalfSize * 0.8f + Up * HalfSize);

	for (int32 i = 0; i < 4; ++i)
	{
		PDI->DrawLine(FrontFace[i], FrontFace[(i + 1) % 4], DrawColor, DepthPriority, LineThickness);
	}

	// Direction indicator
	FVector ArrowEnd = Position + Forward * Size * 1.5f;
	PDI->DrawLine(Position, ArrowEnd, DrawColor, DepthPriority, LineThickness);

	// Dispersion cone outline using component's coverage angles
	float HalfH = FMath::DegreesToRadians(SpeakerComp->HorizontalCoverage * 0.5f);
	FVector ConeLeft = (Forward + Right * FMath::Tan(-HalfH)).GetSafeNormal() * Size * 3.0f + Position;
	FVector ConeRight = (Forward + Right * FMath::Tan(HalfH)).GetSafeNormal() * Size * 3.0f + Position;

	FColor ConeColor = DrawColor;
	ConeColor.A = 100;
	PDI->DrawLine(Position, ConeLeft, ConeColor, DepthPriority, 1.0f);
	PDI->DrawLine(Position, ConeRight, ConeColor, DepthPriority, 1.0f);
	PDI->DrawLine(ConeLeft, ConeRight, ConeColor, DepthPriority, 1.0f);

	// Show registration status
	if (SpeakerComp->IsRegistered())
	{
		PDI->DrawPoint(Position, FColor::Green, 6.0f, DepthPriority);
	}
	else
	{
		PDI->DrawPoint(Position, FColor::Yellow, 6.0f, DepthPriority);
	}
}

// ============================================================================
// FSpatialAudioSourceComponentVisualizer
// ============================================================================

void FSpatialAudioSourceComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const USpatialAudioSourceComponent* SourceComp = Cast<USpatialAudioSourceComponent>(Component);
	if (!SourceComp)
	{
		return;
	}

	AActor* Owner = SourceComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	const FVector Position = Owner->GetActorLocation() + SourceComp->PositionOffset;
	const float Size = 30.0f;
	const FLinearColor Color(0.3f, 0.6f, 1.0f);
	const FColor DrawColor = Color.ToFColor(true);
	const float LineThickness = 2.0f;
	const uint8 DepthPriority = SDPG_World;

	// Draw sphere wireframe
	const int32 NumSegments = 12;
	const float Radius = Size * 0.5f;

	for (int32 i = 0; i < NumSegments; ++i)
	{
		float Angle1 = (float(i) / NumSegments) * 2.0f * PI;
		float Angle2 = (float(i + 1) / NumSegments) * 2.0f * PI;

		// XY plane circle
		FVector P1XY = Position + FVector(FMath::Cos(Angle1) * Radius, FMath::Sin(Angle1) * Radius, 0.0f);
		FVector P2XY = Position + FVector(FMath::Cos(Angle2) * Radius, FMath::Sin(Angle2) * Radius, 0.0f);
		PDI->DrawLine(P1XY, P2XY, DrawColor, DepthPriority, LineThickness);

		// XZ plane circle
		FVector P1XZ = Position + FVector(FMath::Cos(Angle1) * Radius, 0.0f, FMath::Sin(Angle1) * Radius);
		FVector P2XZ = Position + FVector(FMath::Cos(Angle2) * Radius, 0.0f, FMath::Sin(Angle2) * Radius);
		PDI->DrawLine(P1XZ, P2XZ, DrawColor, DepthPriority, LineThickness);
	}

	// Draw spread indicator
	float Spread = SourceComp->InitialSpread;
	if (Spread > 0.0f)
	{
		float SpreadRadius = Radius + (Spread / 180.0f) * Size * 2.0f;
		FColor SpreadColor = FColor(DrawColor.R, DrawColor.G, DrawColor.B, 80);

		for (int32 i = 0; i < NumSegments; ++i)
		{
			float Angle1 = (float(i) / NumSegments) * 2.0f * PI;
			float Angle2 = (float(i + 1) / NumSegments) * 2.0f * PI;

			FVector P1 = Position + FVector(FMath::Cos(Angle1) * SpreadRadius, FMath::Sin(Angle1) * SpreadRadius, 0.0f);
			FVector P2 = Position + FVector(FMath::Cos(Angle2) * SpreadRadius, FMath::Sin(Angle2) * SpreadRadius, 0.0f);
			PDI->DrawLine(P1, P2, SpreadColor, DepthPriority, 1.0f);
		}
	}

	// Registration status indicator
	if (SourceComp->IsRegistered())
	{
		// Green dot at center when registered
		PDI->DrawPoint(Position, FColor::Green, 8.0f, DepthPriority);
	}
	else
	{
		// Red dot when not registered
		PDI->DrawPoint(Position, FColor::Red, 6.0f, DepthPriority);
	}
}
