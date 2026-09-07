// Copyright your name. All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include <GameFramework/HUD.h>
#include "MosquitoHUD.generated.h"

class AMosquitoCharacter;
class UCanvas;

/**
 * Debug HUD (Prompt 10). Pure C++ AHUD — no UMG assets, zero manual editor steps.
 * Draws survival bars (Health/Blood/Energy/Hunger/Wings), nearest-human state,
 * bite progress, and a clock fed by DayNightSystem. MVP 0.2 §3: all new elements
 * (web bar, XP line, upgrade panel, death screen) extend THIS canvas - UMG is
 * explicitly out of scope (owner decision).
 */
UCLASS()
class MOSQUITOSIMULATOR_API AMosquitoHUD : public AHUD
{
	GENERATED_BODY()

public:
	AMosquitoHUD();

	virtual void BeginPlay() override;

	virtual void DrawHUD();

	/** Prompt 10/12: red damage flash, fades over FlashDuration. */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void FlashDamage();

protected:
	void DrawBar(const FString& Label, float Current, float Max, int32 Index, const FLinearColor& Color);
	void DrawHumanState();
	void DrawClock();
	void DrawBiteProgress();
	void DrawCrosshair();
	void DrawScore();

	/** MVP 0.2 §3: Run XP/level line under the Score chip. */
	void DrawRunProgress(class AMosquitoCharacter* Mosquito);

	/** MVP 0.2 §3/§4: Tab-purchased run-branch panel (no world pause). */
	void DrawUpgradePanel(class AMosquitoCharacter* Mosquito);

	/** MVP 0.2 §3.4: species section shown automatically over the DEAD overlay. */
	void DrawDeathPanel(class AMosquitoCharacter* Mosquito);

	/** MVP 0.2 §3.1: WEB AHEAD chip + centered escape meter while trapped. */
	void DrawWebStatus(class AMosquitoCharacter* Mosquito);

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	float BarWidth = 140.f;

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	float BarHeight = 12.f;

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	float BarPadding = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	float BarAnchorX = 16.f;

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	float BarAnchorY = 16.f;

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	float BarSpacing = 22.f;

private:
	float FlashAlpha = 0.f;
	float FlashDuration = 0.4f;
	float FlashTimer = 0.f;
	bool bLoggedFirstDraw = false;
};
