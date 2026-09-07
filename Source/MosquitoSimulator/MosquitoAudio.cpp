// Copyright your name. All Rights Reserved.

#include "MosquitoAudio.h"

#include "AudioDevice.h"
#include "Math/RandomStream.h"

void UMosquitoProceduralWave::Configure(int32 InNumFrames, bool bInLooping)
{
	// UE 5.8: the format block lives in FSoundWaveData (private). SetSampleRate()
	// writes both it and the legacy direct member; SetNumFrames() writes the
	// FSoundWaveData block. NumChannels/Duration have no setters - the mixer
	// itself reads the legacy direct members (see FMixerBuffer ctor), so set them.
	SetSampleRate(static_cast<uint32>(MosquitoAudio::SampleRate));
	SetNumFrames(static_cast<uint32>(InNumFrames));
	NumChannels = 1;
	Duration = (MosquitoAudio::SampleRate > 0)
		? (InNumFrames / static_cast<float>(MosquitoAudio::SampleRate))
		: 0.f;
	bLooping = bInLooping;
}

namespace MosquitoAudio
{
	UMosquitoProceduralWave* MakeBuzzWave(UObject* Outer)
	{
		UMosquitoProceduralWave* Wave = NewObject<UMosquitoProceduralWave>(Outer, NAME_None, RF_Transient);
		Wave->Configure(SampleRate / 2, true); // nominal 0.5 s, refilled from Tick
		return Wave;
	}

	UMosquitoProceduralWave* MakeOneShotWave(UObject* Outer, int32 NumSamples)
	{
		UMosquitoProceduralWave* Wave = NewObject<UMosquitoProceduralWave>(Outer, NAME_None, RF_Transient);
		Wave->Configure(FMath::Max(1, NumSamples), false);
		return Wave;
	}

	void QueueSamples(USoundWaveProcedural* Wave, const TArray<int16>& Samples)
	{
		if (!Wave || Samples.Num() == 0)
		{
			return;
		}

		if (Wave->GetGeneratedPCMDataFormat() == Audio::EAudioMixerStreamDataFormat::Float)
		{
			TArray<float> FloatSamples;
			FloatSamples.SetNumUninitialized(Samples.Num());
			for (int32 i = 0; i < Samples.Num(); ++i)
			{
				FloatSamples[i] = Samples[i] / 32767.f;
			}
			Wave->QueueAudio(reinterpret_cast<const uint8*>(FloatSamples.GetData()), FloatSamples.Num() * sizeof(float));
		}
		else
		{
			Wave->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
		}
	}

	static int16 ToSample(float Value)
	{
		return static_cast<int16>(FMath::Clamp(Value * 32767.f, -32767.f, 32767.f));
	}

	void GenerateBuzzChunk(TArray<int16>& Out, double& Phase, double& TremPhase, int64& SampleCount, float FreqHz, float Amp, int32 NumSamples)
	{
		Out.Reset();
		if (NumSamples <= 0)
		{
			return;
		}
		Out.AddUninitialized(NumSamples);

		const double PhaseStep = 2.0 * PI * static_cast<double>(FreqHz) / static_cast<double>(SampleRate);
		const double TremStep = 2.0 * PI * 47.0 / static_cast<double>(SampleRate); // wing-beat tremolo

		for (int32 i = 0; i < NumSamples; ++i)
		{
			Phase = FMath::Fmod(Phase + PhaseStep, 2.0 * PI);
			TremPhase = FMath::Fmod(TremPhase + TremStep, 2.0 * PI);

			// Buzzy insect timbre: fundamental + 2nd/3rd harmonics.
			const double Body = 0.52 * FMath::Sin(Phase)
				+ 0.30 * FMath::Sin(2.0 * Phase + 0.6)
				+ 0.13 * FMath::Sin(3.0 * Phase + 1.4);
			const double Trem = 0.82 + 0.18 * FMath::Sin(TremPhase);

			++SampleCount;
			Out[i] = ToSample(static_cast<float>(Body * Trem) * FMath::Clamp(Amp, 0.f, 1.f));
		}
	}

	int32 GenerateClap(TArray<int16>& Out)
	{
		constexpr int32 NumSamples = static_cast<int32>(0.22f * SampleRate);
		Out.Reset();
		Out.Init(0, NumSamples);

		FRandomStream Rng(20260907);
		double NoiseState = 0.0;

		for (int32 i = 0; i < NumSamples; ++i)
		{
			const float T = i / static_cast<float>(SampleRate);

			// Two palm impulses (clap echo) drive a decaying noise burst.
			const float Envelope = (T < 0.002f)
				? (T / 0.002f)
				: FMath::Exp(-(T - 0.002f) / 0.035f)
					+ ((T > 0.035f) ? (0.55f * FMath::Exp(-(T - 0.035f) / 0.030f)) : 0.f);

			// One-pole low-pass keeps the noise meaty instead of hissy.
			NoiseState += 0.38 * (Rng.FRandRange(-1.f, 1.f) - NoiseState);

			// Chest-level thump under the slap.
			const float Thump = 0.55f * FMath::Sin(2.f * PI * 68.f * T) * FMath::Exp(-T / 0.05f);

			Out[i] = ToSample(0.9f * static_cast<float>(NoiseState) * Envelope + Thump * Envelope);
		}
		return NumSamples;
	}

	int32 GenerateBite(TArray<int16>& Out)
	{
		constexpr int32 NumSamples = static_cast<int32>(0.12f * SampleRate);
		Out.Reset();
		Out.Init(0, NumSamples);

		double Phase = 0.0;
		for (int32 i = 0; i < NumSamples; ++i)
		{
			const float T = i / static_cast<float>(SampleRate);
			const float K = T / 0.12f;

			// Descending skin poke: 950 -> 260 Hz.
			const float FreqHz = FMath::Lerp(950.f, 260.f, FMath::Square(K));
			Phase += 2.0 * PI * FreqHz / SampleRate;

			const float Envelope = (T < 0.004f) ? (T / 0.004f) : FMath::Exp(-(T - 0.004f) / 0.028f);
			const float Tone = FMath::Sin(static_cast<float>(Phase)) + 0.3f * FMath::Sin(2.f * static_cast<float>(Phase));

			Out[i] = ToSample(0.5f * Tone * Envelope);
		}
		return NumSamples;
	}
}
