// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"
#include "SpatialAudioBenchmark.generated.h"

/**
 * Performance benchmark results for spatial audio operations.
 */
USTRUCT(BlueprintType)
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialAudioBenchmarkResult
{
	GENERATED_BODY()

	/** Operation name */
	UPROPERTY(BlueprintReadOnly, Category = "Benchmark")
	FString OperationName;

	/** Number of iterations */
	UPROPERTY(BlueprintReadOnly, Category = "Benchmark")
	int32 Iterations = 0;

	/** Total time in milliseconds */
	UPROPERTY(BlueprintReadOnly, Category = "Benchmark")
	double TotalTimeMs = 0.0;

	/** Average time per iteration in milliseconds */
	UPROPERTY(BlueprintReadOnly, Category = "Benchmark")
	double AverageTimeMs = 0.0;

	/** Minimum time in milliseconds */
	UPROPERTY(BlueprintReadOnly, Category = "Benchmark")
	double MinTimeMs = 0.0;

	/** Maximum time in milliseconds */
	UPROPERTY(BlueprintReadOnly, Category = "Benchmark")
	double MaxTimeMs = 0.0;

	/** Operations per second */
	UPROPERTY(BlueprintReadOnly, Category = "Benchmark")
	double OpsPerSecond = 0.0;

	/** Format as string */
	FString ToString() const
	{
		return FString::Printf(
			TEXT("%s: %.3fms avg (%.3f-%.3f) over %d iterations = %.0f ops/sec"),
			*OperationName, AverageTimeMs, MinTimeMs, MaxTimeMs, Iterations, OpsPerSecond
		);
	}
};

/**
 * Scoped timer for benchmarking.
 */
class RSHIPSPATIALAUDIORUNTIME_API FSpatialBenchmarkTimer
{
public:
	FSpatialBenchmarkTimer()
		: StartTime(0.0)
		, bRunning(false)
	{
	}

	void Start()
	{
		StartTime = FPlatformTime::Seconds();
		bRunning = true;
	}

	double Stop()
	{
		if (!bRunning)
		{
			return 0.0;
		}
		bRunning = false;
		return (FPlatformTime::Seconds() - StartTime) * 1000.0;  // Convert to ms
	}

	double GetElapsedMs() const
	{
		if (bRunning)
		{
			return (FPlatformTime::Seconds() - StartTime) * 1000.0;
		}
		return 0.0;
	}

private:
	double StartTime;
	bool bRunning;
};

/**
 * RAII-style scoped benchmark that automatically records to result.
 */
class RSHIPSPATIALAUDIORUNTIME_API FScopedBenchmark
{
public:
	FScopedBenchmark(FSpatialAudioBenchmarkResult& InResult)
		: Result(InResult)
	{
		Timer.Start();
	}

	~FScopedBenchmark()
	{
		double Elapsed = Timer.Stop();
		Result.Iterations++;
		Result.TotalTimeMs += Elapsed;
		Result.AverageTimeMs = Result.TotalTimeMs / Result.Iterations;

		if (Result.Iterations == 1 || Elapsed < Result.MinTimeMs)
		{
			Result.MinTimeMs = Elapsed;
		}
		if (Elapsed > Result.MaxTimeMs)
		{
			Result.MaxTimeMs = Elapsed;
		}

		if (Result.AverageTimeMs > 0.0)
		{
			Result.OpsPerSecond = 1000.0 / Result.AverageTimeMs;
		}
	}

private:
	FSpatialAudioBenchmarkResult& Result;
	FSpatialBenchmarkTimer Timer;
};

/**
 * Spatial audio performance benchmark utility.
 *
 * Use this to profile rendering, DSP, and network operations.
 *
 * Usage:
 *   FSpatialAudioBenchmark Benchmark;
 *
 *   // Run VBAP benchmark
 *   FSpatialAudioBenchmarkResult VBAPResult = Benchmark.BenchmarkVBAP(256, 1000);
 *   UE_LOG(LogTemp, Log, TEXT("%s"), *VBAPResult.ToString());
 */
UCLASS(BlueprintType)
class RSHIPSPATIALAUDIORUNTIME_API USpatialAudioBenchmark : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Benchmark VBAP gain computation.
	 * @param NumSpeakers Number of speakers to test with.
	 * @param Iterations Number of iterations to run.
	 * @return Benchmark result.
	 */
	UFUNCTION(BlueprintCallable, Category = "SpatialAudio|Benchmark")
	static FSpatialAudioBenchmarkResult BenchmarkVBAP(int32 NumSpeakers, int32 Iterations = 1000);

	/**
	 * Benchmark DBAP gain computation.
	 */
	UFUNCTION(BlueprintCallable, Category = "SpatialAudio|Benchmark")
	static FSpatialAudioBenchmarkResult BenchmarkDBAP(int32 NumSpeakers, int32 Iterations = 1000);

	/**
	 * Benchmark HOA encoding.
	 */
	UFUNCTION(BlueprintCallable, Category = "SpatialAudio|Benchmark")
	static FSpatialAudioBenchmarkResult BenchmarkHOAEncode(int32 Order, int32 Iterations = 1000);

	/**
	 * Benchmark HOA decoding.
	 */
	UFUNCTION(BlueprintCallable, Category = "SpatialAudio|Benchmark")
	static FSpatialAudioBenchmarkResult BenchmarkHOADecode(int32 NumSpeakers, int32 Order, int32 Iterations = 1000);

	/**
	 * Benchmark biquad filter processing.
	 */
	UFUNCTION(BlueprintCallable, Category = "SpatialAudio|Benchmark")
	static FSpatialAudioBenchmarkResult BenchmarkBiquadFilter(int32 BufferSize, int32 Iterations = 1000);

	/**
	 * Benchmark full speaker DSP chain.
	 */
	UFUNCTION(BlueprintCallable, Category = "SpatialAudio|Benchmark")
	static FSpatialAudioBenchmarkResult BenchmarkSpeakerDSP(int32 BufferSize, int32 NumEQBands, int32 Iterations = 1000);

	/**
	 * Benchmark OSC message serialization.
	 */
	UFUNCTION(BlueprintCallable, Category = "SpatialAudio|Benchmark")
	static FSpatialAudioBenchmarkResult BenchmarkOSCSerialization(int32 NumMessages, int32 Iterations = 1000);

	/**
	 * Run all benchmarks and return summary.
	 */
	UFUNCTION(BlueprintCallable, Category = "SpatialAudio|Benchmark")
	static TArray<FSpatialAudioBenchmarkResult> RunAllBenchmarks();

	/**
	 * Log benchmark results to output log.
	 */
	UFUNCTION(BlueprintCallable, Category = "SpatialAudio|Benchmark")
	static void LogBenchmarkResults(const TArray<FSpatialAudioBenchmarkResult>& Results);
};

/**
 * Performance targets for spatial audio operations.
 * These represent the maximum acceptable latency for real-time audio.
 */
namespace SpatialAudioPerformanceTargets
{
	/** Maximum VBAP computation time per object (ms) */
	constexpr double MaxVBAPComputeTimeMs = 0.1;

	/** Maximum DBAP computation time per object (ms) */
	constexpr double MaxDBAPComputeTimeMs = 0.1;

	/** Maximum HOA encode time per object (ms) */
	constexpr double MaxHOAEncodeTimeMs = 0.2;

	/** Maximum per-sample DSP processing time (ms) */
	constexpr double MaxDSPPerSampleTimeMs = 0.001;

	/** Maximum buffer DSP processing time for 256 samples (ms) */
	constexpr double MaxDSP256BufferTimeMs = 0.5;

	/** Maximum OSC message round-trip latency (ms) */
	constexpr double MaxOSCLatencyMs = 5.0;

	/** Target frame budget at 60fps (ms) */
	constexpr double TargetFrameBudgetMs = 16.67;

	/** Maximum spatial audio budget per frame (ms) */
	constexpr double MaxSpatialAudioBudgetMs = 2.0;
}
