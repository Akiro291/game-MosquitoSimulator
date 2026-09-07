// Copyright your name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpiderCharacter.generated.h"

class UStaticMeshComponent;
class AMosquitoCharacter;

/**
 * MVP 0.2 §1: spider trapped on its own web. Deliberately NOT a Character - it
 * never leaves WebRadius, there is no pathfinding, no patrol/chase (owner decision).
 * Tick-driven mini-FSM like the human's (no Behavior Tree). The web itself is a
 * GAMEPLAY QUERY only (distance to WebCenter, like NearestHumanDistance) - no new
 * collision channels/profiles (collision v5b stays untouched).
 */
UCLASS()
class MOSQUITOSIMULATOR_API ASpiderCharacter : public AActor
{
	GENERATED_BODY()

public:
	ASpiderCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintPure, Category = "Spider")
	FVector GetWebCenter() const { return WebCenter; }

	UFUNCTION(BlueprintPure, Category = "Spider")
	float GetWebRadius() const { return WebRadius; }

protected:
	/** Gameplay radius of the web (cm) - entering it traps the mosquito. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spider|Web")
	float WebRadius = 120.f;

	/** Body placeholder: a small dark sphere (a spider that dwarfs the 2 cm mosquito). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spider|Visual")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

private:
	FVector WebCenter = FVector::ZeroVector;

	TWeakObjectPtr<AMosquitoCharacter> CachedMosquito;

	// --- Dev hook -SpiderTest (plan §1 Verification): headless trapped/escape proof ---
	bool bDevTest = false;
	float DevTestTimer = 0.f;
	int32 DevTapCount = 0;
	bool bDevTrappedRequested = false;
	bool bDevTestDone = false;

	void DevTestTick(float DeltaTime, AMosquitoCharacter* Mosquito);
};
