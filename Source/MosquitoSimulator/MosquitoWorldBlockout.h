// Copyright your name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosquitoWorldBlockout.generated.h"

class UDirectionalLightComponent;
class APlayerStart;

/**
 * Temporary blockout village (lite version of Prompt 6).
 * Spawned automatically by the GameMode in game worlds so PIE is testable
 * without hand-editing the level. Built entirely from engine primitive meshes.
 * Scale: 1 uu = 1 cm, so a "human" prop is 170 uu (1.7 m) tall.
 * Delete or disable this once a real level exists.
 */
UCLASS(NotPlaceable)
class MOSQUITOSIMULATOR_API AMosquitoWorldBlockout : public AActor
{
	GENERATED_BODY()

public:
	AMosquitoWorldBlockout();

protected:
	virtual void BeginPlay() override;

	/** Spawns a fallback PlayerStart at (0,0,120) only if the level has none. */
	UPROPERTY(EditDefaultsOnly, Category = "Blockout")
	bool bSpawnPlayerStart = true;

	/** Adds a simple movable sun. Off by default: MainLevel already has lighting. */
	UPROPERTY(EditDefaultsOnly, Category = "Blockout")
	bool bSpawnLighting = false;

private:
	UStaticMeshComponent* MakeBox(const FName& Name, const FVector& Scale, const FVector& Location);
	UStaticMeshComponent* MakeCylinder(const FName& Name, const FVector& Scale, const FVector& Location);
	UStaticMeshComponent* MakeSphere(const FName& Name, const FVector& Scale, const FVector& Location);
	UStaticMeshComponent* MakePlane(const FName& Name, const FVector& Scale, const FVector& Location);

	UPROPERTY()
	TObjectPtr<UDirectionalLightComponent> Sun;
};