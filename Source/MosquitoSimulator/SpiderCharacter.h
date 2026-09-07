// Copyright your name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpiderCharacter.generated.h"

class UStaticMeshComponent;
class AMosquitoCharacter;

/**
 * MVP 0.2 §1: Tick-driven mini-FSM (owner decision: Idle + short lunge only,
 * no pathfinding/patrol/chase, movement confined to WebRadius).
 */
UENUM(BlueprintType)
enum class ESpiderState : uint8
{
	Idle,
	Approach,
	Windup,
	StrikeCooldown,
	ReturnHome
};

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

	UFUNCTION(BlueprintPure, Category = "Spider")
	ESpiderState GetState() const { return CurrentState; }

protected:
	/** Gameplay radius of the web (cm) - entering it traps the mosquito. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spider|Web")
	float WebRadius = 120.f;

	// --- Lunge tuning (plan §1: 2-3 steps, windup 0.5 s, cooldown 2 s) ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spider|Attack")
	int32 ApproachSteps = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spider|Attack")
	float StepDuration = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spider|Attack")
	float StepPause = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spider|Attack")
	float WindupDuration = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spider|Attack")
	float StrikeCooldownDuration = 2.f;

	/** Same damage type as SWAT: wings + half on health, no impulse (via ApplySwatHit). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spider|Attack")
	float BiteDamage = 10.f;

	/** Body placeholder: a small dark sphere (a spider that dwarfs the 2 cm mosquito). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spider|Visual")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

private:
	FVector WebCenter = FVector::ZeroVector;

	ESpiderState CurrentState = ESpiderState::Idle;
	float StateTimer = 0.f;
	int32 StepIndex = 0;
	FVector StepStart = FVector::ZeroVector;
	FVector StepFinish = FVector::ZeroVector;
	FVector BaseBodyScale = FVector::ZeroVector;

	TWeakObjectPtr<AMosquitoCharacter> CachedMosquito;

	void TickIdle(float DeltaTime, AMosquitoCharacter* Mosquito);
	void TickApproach(float DeltaTime, AMosquitoCharacter* Mosquito);
	void TickWindup(float DeltaTime, AMosquitoCharacter* Mosquito);
	void TickStrikeCooldown(float DeltaTime, AMosquitoCharacter* Mosquito);
	void TickReturnHome(float DeltaTime);
	void EnterState(ESpiderState NewState);
	void BeginStep(const FVector& Target);
	void StepTowardPrey(AMosquitoCharacter* Mosquito);

	// --- Dev hook -SpiderTest (plan §1 Verification): headless trapped/escape proof ---

	// --- Dev hook -SpiderTest (plan §1 Verification): headless trapped/escape proof ---
	bool bDevTest = false;
	float DevTestTimer = 0.f;
	int32 DevTapCount = 0;
	bool bDevTrappedRequested = false;
	bool bDevTestDone = false;

	void DevTestTick(float DeltaTime, AMosquitoCharacter* Mosquito);
};
