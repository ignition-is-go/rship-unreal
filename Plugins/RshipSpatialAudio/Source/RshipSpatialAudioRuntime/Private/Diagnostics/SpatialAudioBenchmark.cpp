// Copyright Rocketship. All Rights Reserved.

#include "Diagnostics/SpatialAudioBenchmark.h"
#include "Rendering/SpatialRendererVBAP.h"
#include "Rendering/SpatialRendererDBAP.h"
#include "Rendering/SpatialRendererHOA.h"
#include "DSP/SpatialBiquadFilter.h"
#include "DSP/SpatialSpeakerDSP.h"
#include "ExternalProcessor/ExternalProcessorTypes.h"
#include "Core/SpatialSpeaker.h"

namespace
{
	TArray<FSpatialSpeaker> CreateTestSpeakers(int32 NumSpeakers)
	{
		TArray<FSpatialSpeaker> Speakers;
		Speakers.Reserve(NumSpeakers);

		// Create speakers in a sphere around origin
		for (int32 i = 0; i < NumSpeakers; ++i)
		{
			float Angle = (float(i) / float(NumSpeakers)) * 2.0f * PI;
			float Elevation = FMath::Sin(float(i) * 0.5f) * 0.5f;

			FSpatialSpeaker Speaker;
			Speaker.Id = FGuid::NewGuid();
			Speaker.Name = FString::Printf(TEXT("Speaker_%d"), i);
			Speaker.Position = FVector(
				FMath::Cos(Angle) * 500.0f,
				FMath::Sin(Angle) * 500.0f,
				Elevation * 300.0f
			);
			Speaker.OutputChannel = i + 1;
			Speaker.Type = ESpatialSpeakerType::PointSource;

			Speakers.Add(Speaker);
		}

		return Speakers;
	}
}

FSpatialAudioBenchmarkResult USpatialAudioBenchmark::BenchmarkVBAP(int32 NumSpeakers, int32 Iterations)
{
	FSpatialAudioBenchmarkResult Result;
	Result.OperationName = FString::Printf(TEXT("VBAP (%d speakers)"), NumSpeakers);

	// Create test speakers
	TArray<FSpatialSpeaker> Speakers = CreateTestSpeakers(NumSpeakers);

	// Create and configure renderer
	FSpatialRendererVBAP Renderer;
	Renderer.Configure(Speakers);

	if (!Renderer.IsConfigured())
	{
		Result.OperationName += TEXT(" [FAILED TO CONFIGURE]");
		return Result;
	}

	// Random test positions
	TArray<FVector> TestPositions;
	for (int32 i = 0; i < Iterations; ++i)
	{
		TestPositions.Add(FVector(
			FMath::RandRange(-400.0f, 400.0f),
			FMath::RandRange(-400.0f, 400.0f),
			FMath::RandRange(-200.0f, 200.0f)
		));
	}

	// Run benchmark
	TArray<FSpatialSpeakerGain> OutGains;
	for (int32 i = 0; i < Iterations; ++i)
	{
		FScopedBenchmark Scope(Result);
		Renderer.ComputeGains(TestPositions[i], 0.0f, OutGains);
	}

	return Result;
}

FSpatialAudioBenchmarkResult USpatialAudioBenchmark::BenchmarkDBAP(int32 NumSpeakers, int32 Iterations)
{
	FSpatialAudioBenchmarkResult Result;
	Result.OperationName = FString::Printf(TEXT("DBAP (%d speakers)"), NumSpeakers);

	// Create test speakers
	TArray<FSpatialSpeaker> Speakers = CreateTestSpeakers(NumSpeakers);

	// Create and configure renderer
	FSpatialRendererDBAP Renderer;
	Renderer.Configure(Speakers);

	if (!Renderer.IsConfigured())
	{
		Result.OperationName += TEXT(" [FAILED TO CONFIGURE]");
		return Result;
	}

	// Random test positions
	TArray<FVector> TestPositions;
	for (int32 i = 0; i < Iterations; ++i)
	{
		TestPositions.Add(FVector(
			FMath::RandRange(-400.0f, 400.0f),
			FMath::RandRange(-400.0f, 400.0f),
			FMath::RandRange(-200.0f, 200.0f)
		));
	}

	// Run benchmark
	TArray<FSpatialSpeakerGain> OutGains;
	for (int32 i = 0; i < Iterations; ++i)
	{
		FScopedBenchmark Scope(Result);
		Renderer.ComputeGains(TestPositions[i], 0.0f, OutGains);
	}

	return Result;
}

FSpatialAudioBenchmarkResult USpatialAudioBenchmark::BenchmarkHOAEncode(int32 Order, int32 Iterations)
{
	FSpatialAudioBenchmarkResult Result;
	Result.OperationName = FString::Printf(TEXT("HOA Encode (Order %d)"), Order);

	// Create encoder
	FAmbisonicsEncoder Encoder;
	Encoder.SetOrder(static_cast<EAmbisonicsOrder>(FMath::Clamp(Order, 1, 5)));

	// Random test directions
	TArray<FVector> TestDirections;
	for (int32 i = 0; i < Iterations; ++i)
	{
		FVector Dir(
			FMath::RandRange(-1.0f, 1.0f),
			FMath::RandRange(-1.0f, 1.0f),
			FMath::RandRange(-1.0f, 1.0f)
		);
		Dir.Normalize();
		TestDirections.Add(Dir);
	}

	// Run benchmark
	TArray<float> OutCoefficients;
	for (int32 i = 0; i < Iterations; ++i)
	{
		FScopedBenchmark Scope(Result);
		Encoder.Encode(TestDirections[i], OutCoefficients);
	}

	return Result;
}

FSpatialAudioBenchmarkResult USpatialAudioBenchmark::BenchmarkHOADecode(int32 NumSpeakers, int32 Order, int32 Iterations)
{
	FSpatialAudioBenchmarkResult Result;
	Result.OperationName = FString::Printf(TEXT("HOA Decode (%d speakers, Order %d)"), NumSpeakers, Order);

	// Create test speakers
	TArray<FSpatialSpeaker> Speakers = CreateTestSpeakers(NumSpeakers);

	// Create decoder
	FAmbisonicsDecoder Decoder;
	Decoder.Configure(
		Speakers,
		static_cast<EAmbisonicsOrder>(FMath::Clamp(Order, 1, 5)),
		EAmbisonicsDecoderType::AllRAD
	);

	if (!Decoder.IsConfigured())
	{
		Result.OperationName += TEXT(" [FAILED TO CONFIGURE]");
		return Result;
	}

	// Create test coefficients
	int32 NumChannels = (Order + 1) * (Order + 1);
	TArray<TArray<float>> TestCoefficients;
	for (int32 i = 0; i < Iterations; ++i)
	{
		TArray<float> Coeffs;
		Coeffs.SetNum(NumChannels);
		for (int32 j = 0; j < NumChannels; ++j)
		{
			Coeffs[j] = FMath::RandRange(-1.0f, 1.0f);
		}
		TestCoefficients.Add(Coeffs);
	}

	// Run benchmark
	TArray<float> OutGains;
	for (int32 i = 0; i < Iterations; ++i)
	{
		FScopedBenchmark Scope(Result);
		Decoder.Decode(TestCoefficients[i], OutGains);
	}

	return Result;
}

FSpatialAudioBenchmarkResult USpatialAudioBenchmark::BenchmarkBiquadFilter(int32 BufferSize, int32 Iterations)
{
	FSpatialAudioBenchmarkResult Result;
	Result.OperationName = FString::Printf(TEXT("Biquad Filter (%d samples)"), BufferSize);

	// Create filter
	FSpatialBiquadFilter Filter;
	Filter.SetPeakingEQ(48000.0f, 1000.0f, 3.0f, 1.0f);

	// Create test buffer
	TArray<float> Buffer;
	Buffer.SetNum(BufferSize);
	for (int32 i = 0; i < BufferSize; ++i)
	{
		Buffer[i] = FMath::RandRange(-1.0f, 1.0f);
	}

	// Run benchmark
	for (int32 i = 0; i < Iterations; ++i)
	{
		// Reset buffer
		for (int32 j = 0; j < BufferSize; ++j)
		{
			Buffer[j] = FMath::RandRange(-1.0f, 1.0f);
		}

		FScopedBenchmark Scope(Result);
		Filter.ProcessBuffer(Buffer.GetData(), BufferSize);
	}

	return Result;
}

FSpatialAudioBenchmarkResult USpatialAudioBenchmark::BenchmarkSpeakerDSP(int32 BufferSize, int32 NumEQBands, int32 Iterations)
{
	FSpatialAudioBenchmarkResult Result;
	Result.OperationName = FString::Printf(TEXT("Speaker DSP (%d samples, %d EQ bands)"), BufferSize, NumEQBands);

	// Create DSP processor
	FSpatialSpeakerDSP DSP;
	DSP.Initialize(48000.0f, 500.0f);

	// Configure
	FSpatialSpeakerDSPConfig Config;
	Config.InputGainDb = -3.0f;
	Config.OutputGainDb = 0.0f;
	Config.DelayMs = 10.0f;

	// Add EQ bands
	for (int32 i = 0; i < NumEQBands; ++i)
	{
		FSpatialDSPEQBand Band;
		Band.Type = ESpatialBiquadType::PeakingEQ;
		Band.Frequency = 100.0f * FMath::Pow(2.0f, float(i));
		Band.GainDb = FMath::RandRange(-6.0f, 6.0f);
		Band.Q = 1.0f;
		Band.bEnabled = true;
		Config.EQBands.Add(Band);
	}

	// Add limiter
	Config.Limiter.bEnabled = true;
	Config.Limiter.ThresholdDb = -6.0f;
	Config.Limiter.AttackMs = 0.1f;
	Config.Limiter.ReleaseMs = 100.0f;

	DSP.ApplyConfig(Config);

	// Create test buffer
	TArray<float> Buffer;
	Buffer.SetNum(BufferSize);

	// Run benchmark
	for (int32 i = 0; i < Iterations; ++i)
	{
		// Reset buffer
		for (int32 j = 0; j < BufferSize; ++j)
		{
			Buffer[j] = FMath::RandRange(-1.0f, 1.0f);
		}

		FScopedBenchmark Scope(Result);
		DSP.ProcessBuffer(Buffer.GetData(), BufferSize);
	}

	return Result;
}

FSpatialAudioBenchmarkResult USpatialAudioBenchmark::BenchmarkOSCSerialization(int32 NumMessages, int32 Iterations)
{
	FSpatialAudioBenchmarkResult Result;
	Result.OperationName = FString::Printf(TEXT("OSC Serialization (%d messages)"), NumMessages);

	// Create test messages
	TArray<FOSCMessage> Messages;
	for (int32 i = 0; i < NumMessages; ++i)
	{
		FOSCMessage Msg;
		Msg.Address = TEXT("/dbaudio1/coordinatemapping/source_position_xy");
		Msg.AddInt(1);
		Msg.AddInt(i + 1);
		Msg.AddFloat(FMath::RandRange(0.0f, 1.0f));
		Msg.AddFloat(FMath::RandRange(0.0f, 1.0f));
		Messages.Add(Msg);
	}

	// Run benchmark
	for (int32 i = 0; i < Iterations; ++i)
	{
		FScopedBenchmark Scope(Result);

		for (const FOSCMessage& Msg : Messages)
		{
			TArray<uint8> Data = Msg.Serialize();
		}
	}

	return Result;
}

TArray<FSpatialAudioBenchmarkResult> USpatialAudioBenchmark::RunAllBenchmarks()
{
	TArray<FSpatialAudioBenchmarkResult> Results;

	UE_LOG(LogTemp, Log, TEXT("=== Running Spatial Audio Benchmarks ==="));

	// VBAP benchmarks
	Results.Add(BenchmarkVBAP(8, 1000));
	Results.Add(BenchmarkVBAP(32, 1000));
	Results.Add(BenchmarkVBAP(128, 1000));
	Results.Add(BenchmarkVBAP(256, 1000));

	// DBAP benchmarks
	Results.Add(BenchmarkDBAP(8, 1000));
	Results.Add(BenchmarkDBAP(64, 1000));
	Results.Add(BenchmarkDBAP(256, 1000));

	// HOA benchmarks
	Results.Add(BenchmarkHOAEncode(1, 1000));
	Results.Add(BenchmarkHOAEncode(3, 1000));
	Results.Add(BenchmarkHOAEncode(5, 1000));

	Results.Add(BenchmarkHOADecode(8, 1, 1000));
	Results.Add(BenchmarkHOADecode(32, 3, 1000));
	Results.Add(BenchmarkHOADecode(64, 5, 500));

	// DSP benchmarks
	Results.Add(BenchmarkBiquadFilter(256, 1000));
	Results.Add(BenchmarkBiquadFilter(1024, 1000));

	Results.Add(BenchmarkSpeakerDSP(256, 4, 1000));
	Results.Add(BenchmarkSpeakerDSP(256, 8, 1000));
	Results.Add(BenchmarkSpeakerDSP(1024, 8, 500));

	// OSC benchmarks
	Results.Add(BenchmarkOSCSerialization(1, 1000));
	Results.Add(BenchmarkOSCSerialization(64, 1000));

	return Results;
}

void USpatialAudioBenchmark::LogBenchmarkResults(const TArray<FSpatialAudioBenchmarkResult>& Results)
{
	UE_LOG(LogTemp, Log, TEXT(""));
	UE_LOG(LogTemp, Log, TEXT("=== Spatial Audio Benchmark Results ==="));
	UE_LOG(LogTemp, Log, TEXT(""));

	for (const FSpatialAudioBenchmarkResult& Result : Results)
	{
		// Check against performance targets
		bool bMeetsTarget = true;
		FString TargetNote;

		if (Result.OperationName.Contains(TEXT("VBAP")))
		{
			bMeetsTarget = Result.AverageTimeMs <= SpatialAudioPerformanceTargets::MaxVBAPComputeTimeMs;
			TargetNote = FString::Printf(TEXT("(target: %.3fms)"), SpatialAudioPerformanceTargets::MaxVBAPComputeTimeMs);
		}
		else if (Result.OperationName.Contains(TEXT("DBAP")))
		{
			bMeetsTarget = Result.AverageTimeMs <= SpatialAudioPerformanceTargets::MaxDBAPComputeTimeMs;
			TargetNote = FString::Printf(TEXT("(target: %.3fms)"), SpatialAudioPerformanceTargets::MaxDBAPComputeTimeMs);
		}
		else if (Result.OperationName.Contains(TEXT("HOA Encode")))
		{
			bMeetsTarget = Result.AverageTimeMs <= SpatialAudioPerformanceTargets::MaxHOAEncodeTimeMs;
			TargetNote = FString::Printf(TEXT("(target: %.3fms)"), SpatialAudioPerformanceTargets::MaxHOAEncodeTimeMs);
		}

		FString StatusStr = bMeetsTarget ? TEXT("[OK]") : TEXT("[SLOW]");

		UE_LOG(LogTemp, Log, TEXT("%s %s %s"), *StatusStr, *Result.ToString(), *TargetNote);
	}

	UE_LOG(LogTemp, Log, TEXT(""));
	UE_LOG(LogTemp, Log, TEXT("=== End Benchmark Results ==="));
}
