// Copyright your name. All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include <GameFramework/GameModeBase.h>
#include "MosquitoSimulatorGameModeBase.generated.h"

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

	/** World-space spawn points (1 uu = 1 cm) for the human NPCs. */
	UPROPERTY(EditDefaultsOnly, Category = "Level")
	TArray<FVector> HumanSpawnPoints;
};
