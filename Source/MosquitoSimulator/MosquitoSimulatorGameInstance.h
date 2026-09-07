// Copyright your name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "MosquitoSimulatorGameInstance.generated.h"

/**
 * MVP 0.2 §0: single persistent storage for the whole game.
 * Save format: one flat .ini written by hand (int keys under [MosquitoSave]) to
 * `<Saved>/Config/MosquitoSave.ini`. No JSON, no plugin, no custom UCLASS(Config=...)
 * files - UE 5.8 does not flush unknown config branches, so the file round-trips
 * through FFileHelper instead. Assigned via GameInstanceClass under
 * [/Script/EngineSettings.GameMapsSettings] (DefaultEngine.ini) - the single key
 * read by UGameEngine::Init (standalone/-game) AND PlayLevel.cpp (PIE).
 */
UCLASS()
class MOSQUITOSIMULATOR_API UMosquitoSimulatorGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	/** API trap 5.8: UGameInstance has no EndPlay - Shutdown() is the exit hook. */
	virtual void Shutdown() override;

	/** Null-safe accessor for actors/HUD. */
	static UMosquitoSimulatorGameInstance* Get(const UObject* WorldContextObject);

	static FString GetSaveFilePath();

	// --- Lifetime metrics (persisted) ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Lifetime")
	int32 LifetimeScore = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Lifetime")
	int32 BestChaseScore = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Lifetime")
	int32 TotalDeaths = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Lifetime")
	int32 Generations = 0;

	// --- Run progress (transient on purpose: never saved, plan §2) ---
	float RunXP = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Progression|Run")
	int32 RunLevel = 1;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Progression|Run")
	int32 UnspentLevelUpPoints = 0;

	/** Curve: 100 * level XP to reach the next level; the overflow carries into it. */
	static float XPToNext(int32 Level) { return 100.f * Level; }

	void AddXP(float Amount);
	bool TrySpendLevelUpPoint();

	// --- Species / genetics (persisted, §4 layer B) ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Species")
	int32 UnspentSpeciesPoints = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Species")
	int32 SpeciesBloodEfficiency = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Species")
	int32 SpeciesWebResistantAdhesion = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Species")
	int32 SpeciesExoskeleton = 0;

	/** Current species branches are capped at 3 levels (plan §4 layer B: 2-3 levels). */
	static constexpr int32 MaxSpeciesBranchLevel = 3;

	bool BuySpeciesBranchLevel(int32& InOutBranchLevel);

	void AddLifetimeScore(int32 Amount);
	void MarkDeath() { ++TotalDeaths; bDirty = true; }
	void SetBestChaseScoreIfHigher(int32 Score);

	/** +1 species point per 200 lifetime scores earned since the last award (post-death, §4). */
	void AwardSpeciesPoints();

	/** Flush the flat .ini to disk + [Save] Written log. */
	void SaveNow();

private:
	int32 Version = 0;

	/** Lifetime score already converted into species points. */
	int32 SpeciesAwardBaseScore = 0;

	static constexpr int32 CurrentSaveVersion = 1;
	static constexpr int32 SpeciesPointsPerLifetimeScore = 200;

	void Load();
	void ResetToDefaults(int32 ReasonVersion);

	bool bDirty = false;
};
