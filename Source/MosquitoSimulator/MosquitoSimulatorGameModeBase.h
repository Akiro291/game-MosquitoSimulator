// Copyright your name. All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include <GameFramework/GameModeBase.h>
#include "MosquitoSimulatorGameModeBase.generated.h"

class ASpiderCharacter;

/**
 * Main GameMode for Mosquito Simulator.
 */
UCLASS()
class AMosquitoSimulatorGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMosquitoSimulatorGameModeBase();

	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "Level")
	FName DefaultMap;

	/** Temporary: auto-spawn a primitive blockout village so PIE is testable
	    without hand-editing the level. Disable once a real level exists. */
	UPROPERTY(EditDefaultsOnly, Category = "Level", meta = (DisplayName = "Spawn Blockout World"))
	bool bSpawnBlockoutWorld = true;

	/** Prompt 7: spawn living human NPCs (replaces static blockout "human" props). */
	UPROPERTY(EditDefaultsOnly, Category = "Level", meta = (DisplayName = "Spawn Humans"))
	bool bSpawnHumans = true;

	/** Prompt 8: auto-spawn the day/night cycle system. */
	UPROPERTY(EditDefaultsOnly, Category = "Level", meta = (DisplayName = "Spawn Day/Night System"))
	bool bSpawnDayNight = true;

	/** MVP 0.2 §1: auto-spawn the spider + its web in the NW quadrant (away from
	    human spawns and PlayerStart). */
	UPROPERTY(EditDefaultsOnly, Category = "Level", meta = (DisplayName = "Spawn Spider"))
	bool bSpawnSpider = true;

	/** Web center (1 uu = 1 cm), by TreeTrunk2 (-800, 650). */
	UPROPERTY(EditDefaultsOnly, Category = "Level")
	FVector SpiderWebLocation = FVector(-800.f, 650.f, 60.f);

	/** PIE-FIX #3: a killed spider takes the web back only after this cooldown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level", meta = (ClampMin = "1"))
	float SpiderRespawnDelay = 45.f;

	/** MVP 0.3 phase A skeleton: one permanent nest behind the house. */
	UPROPERTY(EditDefaultsOnly, Category = "Level", meta = (DisplayName = "Spawn Nest"))
	bool bSpawnNest = true;

	UPROPERTY(EditDefaultsOnly, Category = "Level")
	FVector NestLocation = FVector(900.f, 120.f, 25.f);

	/** Called by ASpiderCharacter when it dies - arms the strict respawn cooldown. */
	void NotifySpiderDied();

private:
	void SpawnSpiderActor();
	void RespawnSpider();

	TWeakObjectPtr<ASpiderCharacter> ActiveSpider;
	FTimerHandle SpiderRespawnTimerHandle;

	/** World-space spawn points (1 uu = 1 cm) for the human NPCs. */
	UPROPERTY(EditDefaultsOnly, Category = "Level")
	TArray<FVector> HumanSpawnPoints;
};
