// Copyright your name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HumanCharacter.generated.h"

class AMosquitoCharacter;
class UAudioComponent;
class USoundWaveProcedural;
class UStaticMeshComponent;
class USceneComponent;

UENUM(BlueprintType)
enum class EHumanState : uint8
{
	Calm,
	Noticed,
	Irritated,
	Angry,
	Chase
};

/**
 * Human NPC (Prompt 7). Simple Tick-driven FSM inside the class (no Behavior
 * Tree, no skeletal animation): cylinder body + sphere head + a swinging box
 * arm. IrritationLevel (0-4) drives the state:
 *   0 Calm      - stands / wanders slowly near home
 *   1 Noticed   - turns toward the mosquito
 *   2 Irritated - scratches, swats when the mosquito is close
 *   3 Angry     - walks to the last known mosquito position and searches
 *   4 Chase     - CHASE MODE: sprints at the mosquito, swats on cooldown
 * Prompt 9 pushes bites in via OnBitten(); Prompt 10 refines chase scoring.
 */
UCLASS()
class MOSQUITOSIMULATOR_API AHumanCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AHumanCharacter();

	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category = "Human")
	EHumanState GetCurrentState() const { return CurrentState; }

	UFUNCTION(BlueprintPure, Category = "Human")
	int32 GetIrritationLevel() const { return IrritationLevel; }

	UFUNCTION(BlueprintCallable, Category = "Human")
	void SetIrritationLevel(int32 NewLevel);

	/** Prompt 9 hook: the mosquito bit this human -> irritation goes up. */
	UFUNCTION(BlueprintCallable, Category = "Human")
	void OnBitten(float BloodAmount);

	/** Clap/swat: swings the arm; if the mosquito is in range -> hit + air wave. */
	UFUNCTION(BlueprintCallable, Category = "Human")
	void PerformAttack();

protected:
	// --- Tuning: perception & irritation ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Human|State")
	float AttackCooldown = 2.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Human|State")
	float DetectionRadius = 300.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Human|State")
	float CloseProximityRadius = 120.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Human|State")
	float IrritationProximitySeconds = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Human|State")
	float IrritationDecaySeconds = 8.f;

	// --- Tuning: attack ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Human|Attack")
	float AttackRange = 150.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Human|Attack")
	float AttackTriggerRange = 220.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Human|Attack")
	float SwatDamage = 6.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Human|Attack")
	float SwatPushStrength = 120.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Human|Attack")
	float ArmSwingDuration = 0.5f;

	// --- Prompt 14: procedural audio + readable placeholder colors ---
	void InitClapSound();
	void ApplyVisualColors();

	UPROPERTY(Transient) TObjectPtr<USoundWaveProcedural> ClapWave = nullptr;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> SfxAudio = nullptr;
	TArray<int16> ClapSamples;

	// --- Tuning: movement (1 uu = 1 cm) ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Human|Movement")
	float WalkSpeedCalm = 60.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Human|Movement")
	float WalkSpeedAngry = 150.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Human|Movement")
	float ChaseSpeed = 340.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Human|Movement")
	float TurnSpeed = 240.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Human|Movement")
	float SearchTurnSpeed = 120.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Human|Chase")
	float LoseChaseDistance = 900.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Human|Chase")
	float ChaseGiveUpTime = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Human|Wander")
	float WanderRadius = 250.f;

	// --- Runtime state ---
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Human|State")
	int32 IrritationLevel = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Human|State")
	EHumanState CurrentState = EHumanState::Calm;

	// --- Placeholder visuals (no skeletal animation in Prompt 7) ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Human|Visual")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Human|Visual")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Human|Visual")
	TObjectPtr<USceneComponent> ArmPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Human|Visual")
	TObjectPtr<UStaticMeshComponent> ArmMesh;

	// --- Scale (1 uu = 1 cm): human is 1.7 m tall ---
	UPROPERTY(EditDefaultsOnly, Category = "Human|Scale")
	float CapsuleRadius = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "Human|Scale")
	float CapsuleHalfHeight = 85.f;

	UPROPERTY(EditDefaultsOnly, Category = "Human|Scale")
	float HeadHeight = 74.f;

private:
	// --- Internal helpers ---
	void UpdatePerception(float DeltaTime);
	void UpdateStateMachine(float DeltaTime);
	void UpdateWander(float DeltaTime);
	void FaceTowards(const FVector& Target, float DeltaTime);
	void StopMovementAndClearWander();
	void UpdateArmAnimation(float DeltaTime);
	void RefreshStateFromIrritation();
	AMosquitoCharacter* GetMosquito() const;
	void OnChaseStarted();
	void OnChaseEnded();
	int32 CalculateChaseScore(float Duration) const;

	// --- Runtime (non-UPROPERTY) ---
	float NextAttackTime = 0.f;
	float ProximityIrritationTimer = 0.f;
	float DecayTimer = 0.f;
	float ChaseLostTimer = 0.f;
	float WanderTimer = 0.f;
	float ArmSwingTimer = 0.f;
	float DistanceToMosquito = TNumericLimits<float>::Max();
	float ChaseTimer = 0.f;
	int32 LastChaseScore = 0;
	float ChaseDiagTimer = 0.f;   // temp: chase movement diagnostics (0.5 s interval)

	bool bArmSwinging = false;
	bool bHasWanderTarget = false;
	bool bHasLastKnown = false;
	bool bMosquitoDetected = false;

	FVector HomeLocation = FVector::ZeroVector;
	FVector WanderTarget = FVector::ZeroVector;
	FVector LastKnownMosquitoPos = FVector::ZeroVector;

	mutable TWeakObjectPtr<AMosquitoCharacter> CachedMosquito;
};