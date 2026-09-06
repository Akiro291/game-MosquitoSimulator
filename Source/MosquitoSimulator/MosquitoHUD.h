// Copyright your name. All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include <GameFramework/HUD.h>
#include "MosquitoHUD.generated.h"

/**
 * Debug HUD (Prompt 10). Pure C++ AHUD — no UMG assets, zero manual editor steps.
 * Draws survival bars (Health/Blood/Energy/Hunger/Wings), nearest-human state,
 * bite progress, and a clock fed by DayNightSystem.
 */
UCLASS()
class MOSQUITOSIMULATOR_API AMosquitoHUD : public AHUD
{
	GENERATED_BODY()

public:
	AMosquitoHUD();

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
};
