// Copyright your name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "MosquitoCharacter.generated.h"

class UAudioComponent;
class USoundWaveProcedural;

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class UInputAction;
class UInputMappingContext;
class AHumanCharacter;
class ASpiderCharacter;
class UMosquitoSimulatorGameInstance;

/**
 * Player mosquito.
 * Prompt 2: survival stats (health / blood / energy / hunger / wings).
 * Prompt 3: flight input via Enhanced Input (actions + mapping context are
 *           created in C++, so no editor assets are required yet).
 * Prompt 4: third-person camera sized for a 1-2 cm creature (1 uu = 1 cm).
 */
UCLASS()
class MOSQUITOSIMULATOR_API AMosquitoCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMosquitoCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintPure, Category = "Mosquito")
	float GetHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Mosquito")
	float GetBlood() const { return CurrentBlood; }

	UFUNCTION(BlueprintPure, Category = "Mosquito")
	float GetEnergy() const { return CurrentEnergy; }

	UFUNCTION(BlueprintPure, Category = "Mosquito")
	float GetHunger() const { return CurrentHunger; }

	UFUNCTION(BlueprintPure, Category = "Mosquito")
	float GetWingCondition() const { return WingCondition; }

	UFUNCTION(BlueprintPure, Category = "Mosquito")
	float GetNoiseLevel() const { return NoiseLevel; }

	/** Prompt 10: HUD reads these for the bars. */
	UFUNCTION(BlueprintPure, Category = "Mosquito")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Mosquito")
	float GetMaxBlood() const { return MaxBlood; }

	UFUNCTION(BlueprintPure, Category = "Mosquito")
	float GetMaxEnergy() const { return MaxEnergy; }

	UFUNCTION(BlueprintPure, Category = "Mosquito")
	float GetMaxHunger() const { return MaxHunger; }

	UFUNCTION(BlueprintPure, Category = "Mosquito")
	float GetMaxWingCondition() const { return MaxWingCondition; }

	/** Prompt 10: HUD shows the nearest human's state. */
	UFUNCTION(BlueprintPure, Category = "Mosquito")
	AHumanCharacter* GetNearestHuman() const { return NearestHuman.Get(); }

	UFUNCTION(BlueprintPure, Category = "Mosquito|Flight")
	bool IsFlying() const { return bIsFlying; }

	UFUNCTION(BlueprintPure, Category = "Mosquito|Bite")
	bool IsLanded() const { return bIsLanded; }

	UFUNCTION(BlueprintPure, Category = "Mosquito|Bite")
	bool IsBiting() const { return bBiting; }

	/** 0..1 — bite progress (for the HUD in Prompt 10). */
	UFUNCTION(BlueprintPure, Category = "Mosquito|Bite")
	float GetBiteProgress01() const
	{
		return (BiteDuration > 0.f) ? FMath::Clamp(BiteProgress / BiteDuration, 0.f, 1.f) : 0.f;
	}

	/** Prompt 14: HUD readability - distance to the nearest human in meters. */
	UFUNCTION(BlueprintPure, Category = "Mosquito|Bite")
	float GetNearestHumanDistanceMeters() const { return NearestHumanDistance * 0.01f; }

	UFUNCTION(BlueprintCallable, Category = "Mosquito")
	void SetHealth(float NewValue);

	UFUNCTION(BlueprintCallable, Category = "Mosquito")
	void SetBlood(float NewValue);

	UFUNCTION(BlueprintCallable, Category = "Mosquito")
	void SetEnergy(float NewValue);

	UFUNCTION(BlueprintCallable, Category = "Mosquito")
	void SetHunger(float NewValue);

	UFUNCTION(BlueprintCallable, Category = "Mosquito")
	void SetWingCondition(float NewValue);

	/** Prompt 7: hit by a human's clap — wing damage + air-wave push. */
	UFUNCTION(BlueprintCallable, Category = "Mosquito")
	void ApplySwatHit(float Damage, const FVector& PushImpulse);

	// --- Prompt 11: Chase Score ---
	UFUNCTION(BlueprintPure, Category = "Mosquito|Score")
	int32 GetCurrentScore() const { return CurrentScore; }

	UFUNCTION(BlueprintPure, Category = "Mosquito|Score")
	int32 GetTotalScore() const { return TotalScore; }

	UFUNCTION(BlueprintCallable, Category = "Mosquito|Score")
	/** MVP 0.2 §4: forwards to the persistent save. bCountsAsChase=false for non-chase
	    rewards (spider kill) so the chase record stays honest. */
	void AddScore(int32 Points, bool bCountsAsChase = true);

	// --- Prompt 12: Death & Respawn ---
	UFUNCTION(BlueprintCallable, Category = "Mosquito")
	void Die();

	UFUNCTION(BlueprintCallable, Category = "Mosquito")
	void Respawn();

	UFUNCTION(BlueprintPure, Category = "Mosquito")
	bool IsDead() const { return bDead; }

	// --- MVP 0.2 §1: Spider web trap & struggle ---
	/** Called by ASpiderCharacter when the mosquito enters WebRadius. */
	UFUNCTION(BlueprintCallable, Category = "Mosquito|Web")
	void EnterWeb();

	/** One struggle tap (public so dev headsless hooks can emulate the R key). */
	UFUNCTION(BlueprintCallable, Category = "Mosquito|Web")
	void Struggle();

	UFUNCTION(BlueprintPure, Category = "Mosquito|Web")
	bool IsTrapped() const { return bTrappedByWeb; }

	/** 1.0 -> 0.0 while trapped; escape at 0. */
	UFUNCTION(BlueprintPure, Category = "Mosquito|Web")
	float GetEscapeProgress01() const { return FMath::Clamp(EscapeMeter, 0.f, 1.f); }

	/** Short grace window after an escape so the web cannot instantly re-trap (P11 pattern). */
	bool HasWebRetakeImmunity() const { return WebRetakeImmunityTimer > 0.f; }

	/** Free the mosquito (struggle success or spider death). */
	void EscapeWeb();

	// --- MVP 0.2 §3/§4: run-upgrade panel (Tab; purchase keys 1-4; NO world pause) ---
	UFUNCTION(BlueprintPure, Category = "Mosquito|Progression")
	bool IsUpgradePanelOpen() const { return bUpgradePanelOpen; }

protected:
	/** Flight input callbacks (Enhanced Input). */
	void MoveForward(const FInputActionValue& Value);
	void MoveRight(const FInputActionValue& Value);
	void MoveUp(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	/** Bite: Prompt 9 (land + bite); sense is still a stub. */
	void OnBitePressed();
	void OnSensePressed();

	/** MVP 0.2 §1: struggle against the spider web (IA_Struggle = R). */
	void OnStruggleStarted(const FInputActionValue& Value);
	void OnStruggleCompleted(const FInputActionValue& Value);

	/** MVP 0.2 §4: Tab panel + run-branch purchases (keys 1-4). */
	void OnToggleUpgradePanel(const FInputActionValue& Value);
	void OnBuyBranchWingControl(const FInputActionValue& Value);
	void OnBuyBranchPropulsion(const FInputActionValue& Value);
	void OnBuyBranchWebEscape(const FInputActionValue& Value);
	void OnBuyBranchMetabolism(const FInputActionValue& Value);

	// --- Prompt 9: bite & blood ---
	void UpdateStats(float DeltaTime);
	void DetectNearbyHumans();
	/** MVP 0.2 §1: spider proximity (same query pattern, for the counter-bite). */
	void DetectNearbySpiders();
	void ResolvePawnPenetration(float DeltaTime);
	void LogHumanCollision(const TCHAR* Type, const FVector& HumanCenter, const FVector& Normal, float Penetration);
	void StickToLandedHuman();
	void LandOn(AHumanCharacter* Human);
	void TakeOff();
	void StartBite();
	void CompleteBite();

	// --- Prompt 14: procedural audio (no assets) ---
	void InitAudio();
	void UpdateBuzz(float DeltaTime);
	void PlayOneShot(USoundWaveProcedural* Wave, const TArray<int16>& Samples);

	// --- Bite tuning ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosquito|Bite")
	float LandDistance = 85.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosquito|Bite")
	float BiteDuration = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosquito|Bite")
	float BloodGainPerBite = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosquito|Bite")
	float BiteCooldown = 0.5f;

	// --- Collision v5: one-way Mosquito<->Human (see ResolvePawnPenetration) ---
	/** Overlap query runs only while the nearest human is closer than this (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mosquito|Collision")
	float PawnPenetrationQueryRadius = 150.f;

	/** TEMP: throttle for [ Mosquito::HumanCollision ] diagnostics (seconds). */
	float HumanCollisionDiagTimer = 0.f;

	// --- Stat tick tuning ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosquito|Stats")
	float HungerRatePerSecond = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosquito|Stats")
	float EnergyDrainFlyingPerSecond = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosquito|Stats")
	float EnergyRegenLandedPerSecond = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosquito|Stats")
	float BloodBurnHungerThreshold = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosquito|Stats")
	float BloodBurnPerSecond = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosquito|Stats")
	float ExhaustedSpeedMult = 0.5f;

	// --- MVP 0.2 §1: web trap tuning (player-feel numbers, plan: tap -0.12,
	// hold auto ~x0.3 of a tap/sec, passive recovery +0.05/s) ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosquito|Web")
	float WebEscapePerTap = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosquito|Web")
	float WebEscapeHoldPerSecond = 0.036f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosquito|Web")
	float WebEscapeRecoverPerSecond = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mosquito|Web")
	float WebRetakeImmunitySeconds = 1.f;

	// --- Web trap runtime ---
	bool bTrappedByWeb = false;
	float EscapeMeter = 1.f;
	bool bStruggleHeld = false;
	float WebRetakeImmunityTimer = 0.f;

	// --- MVP 0.2 §4: progression runtime ---
	UPROPERTY()
	TObjectPtr<UMosquitoSimulatorGameInstance> GameSave = nullptr;

	// Dev flags (plan §4 Verification headless): -SeedScore=N, -KillMe
	bool bDevKillSelf = false;
	float DevKillTimer = 0.f;

	/** Base for WingControl upgrades - captured BEFORE multipliers ever touch it. */
	float BaseMaxAcceleration = 800.f;

	/** Base for the Exoskeleton species bonus (same capture-before-modify rule). */
	float BaseMaxHealth = 100.f;

	bool bUpgradePanelOpen = false;

	// --- Prompt 9: bite runtime ---
	bool bIsLanded = false;
	bool bBiting = false;
	float BiteProgress = 0.f;
	float BiteCooldownTimer = 0.f;
	int32 BiteCount = 0;
	FVector LandedOffset = FVector::ZeroVector;
	TWeakObjectPtr<AHumanCharacter> LandedOnHuman;
	TWeakObjectPtr<AHumanCharacter> NearestHuman;
	float NearestHumanDistance = TNumericLimits<float>::Max();
	TWeakObjectPtr<ASpiderCharacter> NearestSpider;
	float NearestSpiderDistance = TNumericLimits<float>::Max();
	bool bLoggedExhausted = false;
	bool bLoggedStarving = false;

	// --- Prompt 14: procedural audio (48 kHz mono, synthesized, no assets) ---
	UPROPERTY(Transient) TObjectPtr<USoundWaveProcedural> BuzzWave = nullptr;
	UPROPERTY(Transient) TObjectPtr<USoundWaveProcedural> BiteWave = nullptr;
	UPROPERTY() TObjectPtr<UAudioComponent> BuzzAudio = nullptr;
	UPROPERTY() TObjectPtr<UAudioComponent> SfxAudio = nullptr;
	TArray<int16> BiteSamples;
	double BuzzPhase = 0.0;
	double BuzzTremPhase = 0.0;
	int64 BuzzSampleCount = 0;
	float BuzzAmp = 0.f;
	float BuzzFreq = 220.f;
	float SwatShakeImpulseDeg = 0.f; // QA MVP: one-shot camera kick on SWAT

	// --- Prompt 11: Chase Score ---
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Mosquito|Score")
	int32 CurrentScore = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Mosquito|Score")
	int32 TotalScore = 0;

	// --- Prompt 12: Death & Respawn ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mosquito")
	float RespawnDelay = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mosquito|Wings")
	float WingSpeedMult50 = 0.7f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mosquito|Wings")
	float WingSpeedMult25 = 0.4f;

	float DeathTimer = 0.f;
	bool bDead = false;

	// Prompt 13: SWAT recovery & immunity
	float SwatRecoverTimer = 0.f;
	bool bSwatRecovering = false;
	float SwatImmunityTimer = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mosquito")
	float MaxHealth = 100.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Mosquito")
	float CurrentHealth = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mosquito")
	float MaxBlood = 100.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Mosquito")
	float CurrentBlood = 20.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mosquito")
	float MaxEnergy = 100.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Mosquito")
	float CurrentEnergy = 80.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mosquito")
	float MaxHunger = 100.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Mosquito")
	float CurrentHunger = 30.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mosquito")
	float FlightSpeed = 150.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Mosquito")
	float NoiseLevel = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Mosquito")
	float Visibility = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mosquito")
	float MaxWingCondition = 100.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Mosquito")
	float WingCondition = 100.f;

	/** True while airborne; the mosquito currently never walks. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Mosquito|Flight")
	bool bIsFlying = false;

	/** Capsule in cm: mosquito is tiny compared to a human. */
	UPROPERTY(EditDefaultsOnly, Category = "Mosquito|Scale")
	float CapsuleRadius = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Mosquito|Scale")
	float CapsuleHalfHeight = 1.0f;

	/** Third-person camera (Prompt 4). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mosquito|Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mosquito|Camera")
	TObjectPtr<UCameraComponent> Camera;

	/** Spring arm length in cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mosquito|Camera")
	float CameraDistance = 18.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mosquito|Camera")
	float CameraFOV = 90.f;

	/** Placeholder body so the mosquito is visible on screen (2 cm sphere). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mosquito|Visual")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	// --- Enhanced Input: created in C++ so no editor assets are needed yet ---

	UPROPERTY()
	TObjectPtr<UInputMappingContext> FlightContext;

	UPROPERTY()
	TObjectPtr<UInputAction> MoveForwardAction;

	UPROPERTY()
	TObjectPtr<UInputAction> MoveRightAction;

	UPROPERTY()
	TObjectPtr<UInputAction> MoveUpAction;

	UPROPERTY()
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY()
	TObjectPtr<UInputAction> BiteAction;

	UPROPERTY()
	TObjectPtr<UInputAction> SenseAction;

	UPROPERTY()
	TObjectPtr<UInputAction> StruggleAction;

	UPROPERTY()
	TObjectPtr<UInputAction> UpgradePanelAction;

	UPROPERTY()
	TArray<TObjectPtr<UInputAction>> BranchActions;
};
