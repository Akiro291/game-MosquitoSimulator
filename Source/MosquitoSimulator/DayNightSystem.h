// Copyright your name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DayNightSystem.generated.h"

class ADirectionalLight;
class AExponentialHeightFog;
class ASkyLight;

/**
 * Day/Night cycle (Prompt 8). Rotates the level's sun (or spawns one), dims
 * the sky light at night, adds a dim bluish moon and tints the fog.
 * Purely light + time: no weather (per spec).
 * Time is hours 0-24; a full day lasts DayLengthSeconds real seconds.
 * Dev override: command line "-DayLength=N" (seconds per full day).
 */
UCLASS()
class MOSQUITOSIMULATOR_API ADayNightSystem : public AActor
{
	GENERATED_BODY()

public:
	ADayNightSystem();

	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category = "DayNight")
	float GetCurrentTime() const { return CurrentTime; }

	/** 0..1: 0 = deep night, 1 = noon. Prompt 9+ will tune gameplay by this. */
	UFUNCTION(BlueprintPure, Category = "DayNight")
	float GetDayFactor() const { return DayFactor; }

	/** True between 18:00 and 6:00 (people will sleep: Prompt 9+). */
	UFUNCTION(BlueprintPure, Category = "DayNight")
	bool IsNight() const;

protected:
	// --- Tunables (Blueprint-exposed per spec; EditAnywhere so they can be
	//     tweaked live on the spawned instance in PIE) ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Time")
	float DayLengthSeconds = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Time")
	float StartTime = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Sun")
	float MaxSunIntensity = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Sun")
	float SunYawOffset = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Moon")
	bool bSpawnMoonLight = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Moon")
	float MaxMoonIntensity = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Sky")
	bool bControlSkyLight = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Sky")
	float DaySkyLightIntensity = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Sky")
	float NightSkyLightIntensity = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Fog")
	bool bControlFog = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Fog")
	bool bSpawnFogIfMissing = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Fog")
	float DayFogDensity = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Fog")
	float NightFogDensity = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Fog")
	FLinearColor DayFogColor = FLinearColor(0.65f, 0.75f, 0.9f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Fog")
	FLinearColor NightFogColor = FLinearColor(0.02f, 0.03f, 0.08f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DayNight|Fog")
	float FogHeightFalloff = 0.2f;

	/** Hours 0-24. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "DayNight|State")
	float CurrentTime = 6.f;

private:
	void CacheLights();
	void ApplyTimeToLights();

	float DayFactor = 1.f;

	TWeakObjectPtr<ADirectionalLight> SunActor;
	TWeakObjectPtr<ADirectionalLight> MoonActor;
	TWeakObjectPtr<ASkyLight> SkyLightActor;
	TWeakObjectPtr<AExponentialHeightFog> FogActor;

	bool bSpawnedOwnSun = false;
	bool bSpawnedOwnMoon = false;
	bool bSpawnedOwnFog = false;
};