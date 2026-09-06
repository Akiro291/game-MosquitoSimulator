// Copyright your name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "MosquitoCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class UInputAction;
class UInputMappingContext;
class AHumanCharacter;

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

protected:
	/** Flight input callbacks (Enhanced Input). */
	void MoveForward(const FInputActionValue& Value);
	void MoveRight(const FInputActionValue& Value);
	void MoveUp(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	/** Bite: Prompt 9 (land + bite); sense is still a stub. */
	void OnBitePressed();
	void OnSensePressed();

	// --- Prompt 9: bite & blood ---
	void UpdateStats(float DeltaTime);
	void DetectNearbyHumans();
	void StickToLandedHuman();
	void LandOn(AHumanCharacter* Human);
	void TakeOff();
	void StartBite();
	void CompleteBite();

	// --- Bite tuning ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosquito|Bite")
	float LandDistance = 85.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosquito|Bite")
	float BiteDuration = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosquito|Bite")
	float BloodGainPerBite = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosquito|Bite")
	float BiteCooldown = 0.5f;

	// --- Stat tick tuning ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosquito|Stats")
	float HungerRatePerSecond = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosquito|Stats")
	float EnergyDrainFlyingPerSecond = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosquito|Stats")
	float EnergyRegenLandedPerSecond = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosquito|Stats")
	float BloodBurnHungerThreshold = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosquito|Stats")
	float BloodBurnPerSecond = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mosquito|Stats")
	float ExhaustedSpeedMult = 0.5f;

	// --- Bite runtime ---
	bool bIsLanded = false;
	bool bBiting = false;
	float BiteProgress = 0.f;
	float BiteCooldownTimer = 0.f;
	int32 BiteCount = 0;
	FVector LandedOffset = FVector::ZeroVector;
	TWeakObjectPtr<AHumanCharacter> LandedOnHuman;
	TWeakObjectPtr<AHumanCharacter> NearestHuman;
	float NearestHumanDistance = TNumericLimits<float>::Max();
	bool bLoggedExhausted = false;
	bool bLoggedStarving = false;

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
	float FlightSpeed = 120.f;

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
};
