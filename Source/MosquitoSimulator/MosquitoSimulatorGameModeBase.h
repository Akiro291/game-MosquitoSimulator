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

	/** Spider webs (1 uu = 1 cm) - each web gets its own spider + its own respawn
	    cooldown. Default (set in BeginPlay when empty): one web by TreeTrunk2. */
	UPROPERTY(EditDefaultsOnly, Category = "Level")
	TArray<FVector> SpiderWebLocations;

	/** PIE-FIX #3: a killed spider takes the web back only after this cooldown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level", meta = (ClampMin = "1"))
	float SpiderRespawnDelay = 45.f;

	/** MVP 0.3 phase A skeleton: nests (1 uu = 1 cm). Default (BeginPlay when
	    empty): the one permanent nest behind the house. */
	UPROPERTY(EditDefaultsOnly, Category = "Level", meta = (DisplayName = "Spawn Nests"))
	bool bSpawnNest = true;

	UPROPERTY(EditDefaultsOnly, Category = "Level")
	TArray<FVector> NestLocations;

	/** Called by ASpiderCharacter when it dies - arms the strict per-web respawn cooldown. */
	void NotifySpiderDied(int32 WebIndex);

private:
	struct FSpiderWeb
	{
		TWeakObjectPtr<ASpiderCharacter> Spider;
		FTimerHandle RespawnTimer;
	};

	void SpawnSpiderAt(int32 WebIndex);
	void RespawnSpiderWeb(int32 WebIndex);

	TArray<FSpiderWeb> SpiderWebs;

	/** World-space spawn points (1 uu = 1 cm) for the human NPCs. */
	UPROPERTY(EditDefaultsOnly, Category = "Level")
	TArray<FVector> HumanSpawnPoints;
};
