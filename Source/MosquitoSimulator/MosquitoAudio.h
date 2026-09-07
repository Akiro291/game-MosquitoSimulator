// Copyright your name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include "MosquitoAudio.generated.h"

/**
 * Prompt 14: mono 48 kHz procedural sound wave (zero audio assets).
 * UE 5.8: the format block (SampleRate/NumFrames) lives in the private
 * FSoundWaveData - configuration is done from inside the class via the
 * official setters plus the legacy direct NumChannels/Duration members.
 */
UCLASS(Transient)
class MOSQUITOSIMULATOR_API UMosquitoProceduralWave : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	/** Configure format + duration. bInLooping=true for continuous sources (buzz). */
	void Configure(int32 InNumFrames, bool bInLooping);
};

/** Prompt 14 procedural sound kit: buzz / clap / bite synthesized at runtime. */
namespace MosquitoAudio
{
	constexpr int32 SampleRate = 48000;

	/** Looping wave for continuous sources (mosquito buzz); FIFO is refilled from Tick. */
	UMosquitoProceduralWave* MakeBuzzWave(UObject* Outer);

	/** One-shot wave whose Duration matches the queued sample count exactly. */
	UMosquitoProceduralWave* MakeOneShotWave(UObject* Outer, int32 NumSamples);

	/** Converts int16 samples to the active mixer format and queues them. */
	void QueueSamples(USoundWaveProcedural* Wave, const TArray<int16>& Samples);

	/** Appends NumSamples of buzz (harmonics + wing-beat tremolo) into Out. */
	void GenerateBuzzChunk(TArray<int16>& Out, double& Phase, double& TremPhase, int64& SampleCount, float FreqHz, float Amp, int32 NumSamples);

	/** Hand clap: filtered noise + thump + palm echo (~0.22 s). Returns sample count. */
	int32 GenerateClap(TArray<int16>& Out);

	/** Bite: descending skin-poke blip (~0.12 s). Returns sample count. */
	int32 GenerateBite(TArray<int16>& Out);

	/** MVP 0.2 §1: spider attack tell - buzzing click train ending in a snap (~0.18 s). */
	int32 GenerateSpiderClick(TArray<int16>& Out);
}
