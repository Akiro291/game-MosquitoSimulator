// Copyright your name. All Rights Reserved.

#include "MosquitoHUD.h"

#include "Camera/PlayerCameraManager.h"
#include "DayNightSystem.h"
#include "Engine/Canvas.h"
#include "HumanCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "MosquitoCharacter.h"
#include "MosquitoSimulatorPlayerController.h"

AMosquitoHUD::AMosquitoHUD()
{
	FlashAlpha = 0.f;
	FlashDuration = 0.4f;
	FlashTimer = 0.f;
}

void AMosquitoHUD::FlashDamage()
{
	FlashAlpha = 1.f;
	FlashTimer = 0.f;
}

static FString HumanStateToString(EHumanState State)
{
	switch (State)
	{
	case EHumanState::Calm: return TEXT("Calm");
	case EHumanState::Noticed: return TEXT("Noticed");
	case EHumanState::Irritated: return TEXT("Irritated");
	case EHumanState::Angry: return TEXT("Angry");
	case EHumanState::Chase: return TEXT("CHASE");
	default: return TEXT("?");
	}
}

static FLinearColor HumanStateToColor(EHumanState State)
{
	switch (State)
	{
	case EHumanState::Calm: return FLinearColor::Green;
	case EHumanState::Noticed: return FLinearColor::Yellow;
	case EHumanState::Irritated: return FLinearColor(1.f, 0.5f, 0.f);
	case EHumanState::Angry: return FLinearColor::Red;
	case EHumanState::Chase: return FLinearColor(1.f, 0.f, 0.5f);
	default: return FLinearColor::White;
	}
}

void AMosquitoHUD::DrawBar(const FString& Label, float Current, float Max, int32 Index, const FLinearColor& Color)
{
	if (!Canvas)
	{
		return;
	}

	const float X = BarAnchorX;
	const float Y = BarAnchorY + Index * BarSpacing;
	const float ClampedCurrent = FMath::Clamp(Current, 0.f, Max);
	const float Filled = (Max > 0.f) ? (ClampedCurrent / Max) : 0.f;

	DrawRect(FLinearColor(0.05f, 0.05f, 0.05f, 0.85f), X, Y, BarWidth, BarHeight);

	if (Filled > 0.f)
	{
		DrawRect(Color, X, Y, BarWidth * Filled, BarHeight);
	}

	const FString Text = FString::Printf(TEXT("%s: %d/%d"), *Label, FMath::RoundToInt(ClampedCurrent), FMath::RoundToInt(Max));
	DrawText(Text, FLinearColor::White, X + 4.f, Y - 14.f, GEngine->GetSmallFont());
}

void AMosquitoHUD::DrawHumanState()
{
	AMosquitoCharacter* Mosquito = Cast<AMosquitoSimulatorPlayerController>(Owner) ?
		Cast<AMosquitoCharacter>(Cast<AMosquitoSimulatorPlayerController>(Owner)->GetPawn()) : nullptr;

	if (!Mosquito || !Canvas)
	{
		return;
	}

	if (AHumanCharacter* Nearest = Mosquito->GetNearestHuman())
	{
		const EHumanState State = Nearest->GetCurrentState();
		const FString Text = FString::Printf(TEXT("Nearest: %s (%d/4)"),
			*HumanStateToString(State), Nearest->GetIrritationLevel());
		DrawText(Text, HumanStateToColor(State), BarAnchorX, BarAnchorY + 6 * BarSpacing, GEngine->GetSmallFont());
	}
	else
	{
DrawText(TEXT("Nearest: ---`"), FLinearColor::White, BarAnchorX, BarAnchorY + 6 * BarSpacing, GEngine->GetSmallFont());
	}
}
void AMosquitoHUD::DrawClock()
{
	if (!Canvas)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ADayNightSystem* DayNight = nullptr;
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(World, ADayNightSystem::StaticClass(), Found);
	if (Found.Num() > 0)
	{
		DayNight = Cast<ADayNightSystem>(Found[0]);
	}

	if (!DayNight)
	{
		return;
	}

	const float Time = DayNight->GetCurrentTime();
	const int32 Hours = FMath::FloorToInt(FMath::Fmod(Time, 24.f));
	const int32 Minutes = FMath::FloorToInt(FMath::Fmod(Time * 60.f, 60.f));
	const FString ClockText = FString::Printf(TEXT("%02d:%02d"), Hours, Minutes);

	const FLinearColor ClockColor = DayNight->IsNight() ? FLinearColor(0.6f, 0.7f, 1.f) : FLinearColor(1.f, 0.9f, 0.5f);

	const float ClockX = Canvas->SizeX - 90.f;
	const float ClockY = BarAnchorY;
	DrawText(ClockText, ClockColor, ClockX, ClockY, GEngine->GetLargeFont());
	DrawText(DayNight->IsNight() ? TEXT("Night") : TEXT("Day"), ClockColor, ClockX + 10.f, ClockY + 28.f, GEngine->GetSmallFont());
}

void AMosquitoHUD::DrawBiteProgress()
{
	AMosquitoCharacter* Mosquito = Cast<AMosquitoSimulatorPlayerController>(Owner) ?
		Cast<AMosquitoCharacter>(Cast<AMosquitoSimulatorPlayerController>(Owner)->GetPawn()) : nullptr;

	if (!Mosquito || !Canvas)
	{
		return;
	}

	if (Mosquito->IsLanded())
	{
		const float P = Mosquito->GetBiteProgress01();
		const float W = 200.f;
		const float H = 8.f;
		const float X = (Canvas->SizeX - W) * 0.5f;
		const float Y = Canvas->SizeY - 60.f;
		DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f), X, Y, W, H);
		DrawRect(FLinearColor(0.8f, 0.1f, 0.1f, 1.f), X, Y, W * P, H);
		const FString BiteText = Mosquito->IsBiting() ? TEXT("Biting...") : TEXT("Landed — press LMB to bite");
		DrawText(BiteText, FLinearColor::White, X, Y - 16.f, GEngine->GetSmallFont());
	}
}

void AMosquitoHUD::DrawCrosshair()
{
	if (!Canvas)
	{
		return;
	}
	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;
	const float L = 6.f;
	const float G = 3.f;
	const FLinearColor C(0.8f, 0.8f, 0.8f, 0.6f);
	DrawRect(C, CX - L, CY - 0.5f, L - G, 1.f);
	DrawRect(C, CX + G, CY - 0.5f, L - G, 1.f);
	DrawRect(C, CX - 0.5f, CY - L, 1.f, L - G);
	DrawRect(C, CX - 0.5f, CY + G, 1.f, L - G);
}

void AMosquitoHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	if (FlashAlpha > 0.f)
	{
		FlashTimer += GetWorld()->GetDeltaSeconds();
		FlashAlpha = FMath::Clamp(1.f - (FlashTimer / FlashDuration), 0.f, 1.f);
		DrawRect(FLinearColor(1.f, 0.f, 0.f, FlashAlpha * 0.4f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
	}

	AMosquitoCharacter* Mosquito = Cast<AMosquitoSimulatorPlayerController>(Owner) ?
		Cast<AMosquitoCharacter>(Cast<AMosquitoSimulatorPlayerController>(Owner)->GetPawn()) : nullptr;

	if (Mosquito)
	{
		DrawBar(TEXT("Health"), Mosquito->GetHealth(), Mosquito->GetMaxHealth(), 0, FLinearColor(0.8f, 0.1f, 0.1f));
		DrawBar(TEXT("Blood"), Mosquito->GetBlood(), Mosquito->GetMaxBlood(), 1, FLinearColor(0.8f, 0.2f, 0.4f));
		DrawBar(TEXT("Energy"), Mosquito->GetEnergy(), Mosquito->GetMaxEnergy(), 2, FLinearColor(0.2f, 0.6f, 1.f));
		DrawBar(TEXT("Hunger"), Mosquito->GetHunger(), Mosquito->GetMaxHunger(), 3, FLinearColor(1.f, 0.7f, 0.1f));
		DrawBar(TEXT("Wings"), Mosquito->GetWingCondition(), Mosquito->GetMaxWingCondition(), 4, FLinearColor(0.2f, 0.9f, 0.4f));
	}

	DrawHumanState();
	DrawClock();
	DrawBiteProgress();
	DrawCrosshair();
}

