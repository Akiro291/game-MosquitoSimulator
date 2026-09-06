// Copyright your name. All Rights Reserved.

#include "DayNightSystem.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	// Sun elevation as a sine over the day: 0 at 6:00 (sunrise), 1 at noon,
	// 0 at 18:00 (sunset), negative during the night. Continuous across midnight.
	float ComputeElevation(float TimeHours)
	{
		return FMath::Sin(((TimeHours - 6.f) / 12.f) * PI);
	}

	const FLinearColor SunriseColor(1.f, 0.55f, 0.25f);
	const FLinearColor NoonColor(1.f, 0.96f, 0.9f);
	const FLinearColor MoonBlueColor(0.35f, 0.45f, 0.85f);
}

ADayNightSystem::ADayNightSystem()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(false);
	CurrentTime = StartTime;
}

void ADayNightSystem::BeginPlay()
{
	Super::BeginPlay();

	// Dev override: "-DayLength=1" squeezes a full day into 1 s (headless tests).
	float CmdDayLength = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("DayLength="), CmdDayLength) && CmdDayLength > 0.f)
	{
		DayLengthSeconds = CmdDayLength;
	}

	CurrentTime = FMath::Fmod(StartTime, 24.f);
	CacheLights();
	ApplyTimeToLights();

	UE_LOG(LogTemp, Log, TEXT("[DayNight] Started at %.1f h, day length %.1f s (sun=%s sky=%s fog=%s)"),
		CurrentTime, DayLengthSeconds,
		SunActor.IsValid() ? TEXT("ok") : TEXT("none"),
		SkyLightActor.IsValid() ? TEXT("ok") : TEXT("none"),
		FogActor.IsValid() ? TEXT("ok") : TEXT("none"));
}

void ADayNightSystem::CacheLights()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Sun: reuse the level's directional light, spawn one if the level has none.
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		SunActor = *It;
		break;
	}
	if (!SunActor.IsValid())
	{
		ADirectionalLight* NewSun = World->SpawnActor<ADirectionalLight>(
			ADirectionalLight::StaticClass(), FTransform::Identity, Params);
		SunActor = NewSun;
		bSpawnedOwnSun = NewSun != nullptr;
	}
	if (ADirectionalLight* Sun = SunActor.Get())
	{
		if (ULightComponent* Comp = Sun->GetLightComponent())
		{
			Comp->SetMobility(EComponentMobility::Movable); // rotated every tick
		}
	}

	// Moon: always our own dim, bluish light so night hunting stays possible.
	if (bSpawnMoonLight)
	{
		ADirectionalLight* Moon = World->SpawnActor<ADirectionalLight>(
			ADirectionalLight::StaticClass(), FTransform::Identity, Params);
		MoonActor = Moon;
		bSpawnedOwnMoon = Moon != nullptr;
		if (Moon)
		{
			if (ULightComponent* Comp = Moon->GetLightComponent())
			{
				Comp->SetMobility(EComponentMobility::Movable);
				Comp->SetLightColor(MoonBlueColor);
				Comp->SetCastShadows(false); // a dim moon does not need shadow cost
			}
		}
	}

	// Sky light: bright at day, nearly black at night.
	if (bControlSkyLight)
	{
		for (TActorIterator<ASkyLight> It(World); It; ++It)
		{
			SkyLightActor = *It;
			break;
		}
	}

	// Fog: atmosphere tint per time of day; spawn one if the level has none.
	if (bControlFog)
	{
		for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
		{
			FogActor = *It;
			break;
		}
		if (!FogActor.IsValid() && bSpawnFogIfMissing)
		{
			AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(
				AExponentialHeightFog::StaticClass(), FTransform::Identity, Params);
			FogActor = Fog;
			bSpawnedOwnFog = Fog != nullptr;
		}
	}
}

void ADayNightSystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const float SafeDayLength = FMath::Max(DayLengthSeconds, 0.1f);
	const float HoursPerSecond = 24.f / SafeDayLength;

	const float PreviousTime = CurrentTime;
	CurrentTime = FMath::Fmod(CurrentTime + HoursPerSecond * DeltaTime, 24.f);

	ApplyTimeToLights();

	// Sunrise/sunset markers double as the headless-verification signal.
	const bool bWasDay = (PreviousTime >= 6.f && PreviousTime < 18.f);
	const bool bIsDay = (CurrentTime >= 6.f && CurrentTime < 18.f);
	if (bWasDay != bIsDay)
	{
		UE_LOG(LogTemp, Log, TEXT("[DayNight] %s at %.1f h"),
			bIsDay ? TEXT("SUNRISE") : TEXT("SUNSET"), CurrentTime);
	}
}

bool ADayNightSystem::IsNight() const
{
	return (CurrentTime < 6.f || CurrentTime >= 18.f);
}

void ADayNightSystem::ApplyTimeToLights()
{
	const float Elevation = ComputeElevation(CurrentTime); // -1..1, 1 = noon
	DayFactor = FMath::Clamp(Elevation, 0.f, 1.f);

	// --- Sun ---
	if (ADirectionalLight* Sun = SunActor.Get())
	{
		if (ULightComponent* Comp = Sun->GetLightComponent())
		{
			// 0 deg at the horizon (6:00/18:00), -90 deg (straight down) at noon.
			const float Pitch = -Elevation * 90.f;
			const float Yaw = ((CurrentTime - 6.f) / 24.f) * 360.f + SunYawOffset;
			Comp->SetWorldRotation(FRotator(Pitch, Yaw, 0.f));

			const float SunStrength = FMath::Pow(DayFactor, 0.6f); // soft sunrise/sunset
			Comp->SetIntensity(MaxSunIntensity * SunStrength);
			Comp->SetLightColor(FMath::Lerp(SunriseColor, NoonColor, FMath::Clamp(Elevation * 2.f, 0.f, 1.f)));
		}
	}

	// --- Moon: up while the sun is down ---
	if (ADirectionalLight* Moon = MoonActor.Get())
	{
		if (ULightComponent* Comp = Moon->GetLightComponent())
		{
			const float MoonElevation = FMath::Clamp(-Elevation, 0.f, 1.f);
			const float Pitch = -MoonElevation * 90.f;
			const float Yaw = ((CurrentTime - 18.f) / 24.f) * 360.f;
			Comp->SetWorldRotation(FRotator(Pitch, Yaw, 0.f));
			Comp->SetIntensity(MaxMoonIntensity * MoonElevation * MoonElevation);
		}
	}

	// --- Sky light: bright day, nearly black night ---
	if (bControlSkyLight)
	{
		if (ASkyLight* Sky = SkyLightActor.Get())
		{
			if (USkyLightComponent* Comp = Sky->GetLightComponent())
			{
				Comp->SetIntensity(FMath::Lerp(NightSkyLightIntensity, DaySkyLightIntensity, DayFactor));
			}
		}
	}

	// --- Fog: dense and dark at night, light blue haze at day ---
	if (bControlFog)
	{
		if (AExponentialHeightFog* Fog = FogActor.Get())
		{
			if (UExponentialHeightFogComponent* Comp = Fog->GetComponent())
			{
				Comp->SetFogDensity(FMath::Lerp(NightFogDensity, DayFogDensity, DayFactor));
				Comp->SetFogInscatteringColor(FMath::Lerp(NightFogColor, DayFogColor, DayFactor));
				Comp->SetFogHeightFalloff(FogHeightFalloff);
			}
		}
	}
}