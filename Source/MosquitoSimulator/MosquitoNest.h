// Copyright your name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MosquitoNest.generated.h"

class UStaticMeshComponent;
class AMosquitoCharacter;

/**
 * MVP 0.3 phase A SKELETON: the nest closes the GDD loop (hunt -> risk -> reward ->
 * reproduction). Like the spider web it is a GAMEPLAY QUERY only (distance to
 * NestCenter, no collision channels/profiles) - collision v5b stays untouched.
 * Fly home with enough blood, enter the radius, and the generation is scored:
 * reward -> GameInstance clutch bookkeeping (species point + mutation roll, S2)
 * -> the run ends via the EXISTING Die/Respawn loop (skeleton deliberately reuses
 * the proven death path; a bespoke generation screen is the next phase).
 */
UCLASS()
class MOSQUITOSIMULATOR_API AMosquitoNest : public AActor
{
	GENERATED_BODY()

public:
	AMosquitoNest();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

protected:
	/** Query radius in which a full-enough mosquito completes its life (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nest|Play")
	float NestRadius = 150.f;

	/** Min blood to lay a clutch. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nest|Play")
	float BloodRequiredToClutch = 60.f;

	/** Score paid on clutch (flows into the single AddScore pipe -> XP + lifetime). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nest|Play")
	int32 ClutchScore = 100;

	/** Dish + eggs: pure visual primitives, zero assets, zero collision. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Nest|Visual")
	TObjectPtr<UStaticMeshComponent> DishMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Nest|Visual")
	TObjectPtr<UStaticMeshComponent> EggMesh;

private:
	FVector NestCenter = FVector::ZeroVector;
	TWeakObjectPtr<AMosquitoCharacter> CachedMosquito;

	// --- Dev hook -NestTest (plan A skeleton verification) ---
	bool bDevTest = false;
	float DevTimer = 0.f;
	int32 DevStage = 0;

	void DevTestTick(float DeltaTime, AMosquitoCharacter* Mosquito);
};
