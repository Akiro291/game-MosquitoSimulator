// Copyright your name. All Rights Reserved.

#include "MosquitoHUD.h"

#include "Camera/PlayerCameraManager.h"
#include "DayNightSystem.h"
#include "Engine/Canvas.h"
#include "HumanCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "MosquitoCharacter.h"
#include "MosquitoProgressionTypes.h"
#include "MosquitoSimulatorGameInstance.h"
#include "MosquitoSimulatorPlayerController.h"

AMosquitoHUD::AMosquitoHUD()
{
	FlashAlpha = 0.f;
	FlashDuration = 0.4f;
	FlashTimer = 0.f;
}

void AMosquitoHUD::BeginPlay()
{
	Super::BeginPlay();
	// QA MVP: proves in any log that the debug HUD actually spawned.
	UE_LOG(LogTemp, Log, TEXT("[MosquitoHUD] Active: Canvas=%s Viewport=%dx%d"),
		Canvas ? TEXT("OK") : TEXT("NULL"),
		Canvas ? static_cast<int32>(Canvas->SizeX) : 0,
		Canvas ? static_cast<int32>(Canvas->SizeY) : 0);
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

	// Prompt 14: black outline + dark chip for contrast on any scene.
	DrawRect(FLinearColor::Black, X - 1.f, Y - 1.f, BarWidth + 2.f, BarHeight + 2.f);
	DrawRect(FLinearColor(0.05f, 0.05f, 0.05f, 0.85f), X, Y, BarWidth, BarHeight);

	if (Filled > 0.f)
	{
		DrawRect(Color, X, Y, BarWidth * Filled, BarHeight);
	}

	// Numeric value above the bar, colored label to the right of it.
	DrawText(FString::Printf(TEXT("%d"), FMath::RoundToInt(ClampedCurrent)),
		FLinearColor::White, X + 4.f, Y - 14.f, GEngine->GetSmallFont());
	DrawText(Label, Color, X + BarWidth + 6.f, Y - 1.f, GEngine->GetSmallFont());
}

void AMosquitoHUD::DrawHumanState()
{
	AMosquitoCharacter* Mosquito = Cast<AMosquitoSimulatorPlayerController>(Owner) ?
		Cast<AMosquitoCharacter>(Cast<AMosquitoSimulatorPlayerController>(Owner)->GetPawn()) : nullptr;

	if (!Mosquito || !Canvas)
	{
		return;
	}

	const float X = BarAnchorX;
	const float Y = BarAnchorY + 8 * BarSpacing;
	const float W = 280.f;
	const float H = 26.f;

	// Prompt 14: bigger font, distance in meters, colored state chip.
	DrawRect(FLinearColor(0.05f, 0.05f, 0.05f, 0.85f), X, Y, W, H);
	if (AHumanCharacter* Nearest = Mosquito->GetNearestHuman())
	{
		const EHumanState State = Nearest->GetCurrentState();
		const FLinearColor StateColor = HumanStateToColor(State);
		const FString Text = FString::Printf(TEXT("Human: %s  [%d/4]  %.1f m"),
			*HumanStateToString(State), Nearest->GetIrritationLevel(),
			Mosquito->GetNearestHumanDistanceMeters());

		DrawRect(StateColor, X, Y, 5.f, H);
		DrawText(Text, StateColor, X + 12.f, Y + 3.f, GEngine->GetMediumFont());
	}
	else
	{
		DrawText(TEXT("Human: ---"), FLinearColor::White, X + 12.f, Y + 3.f, GEngine->GetMediumFont());
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

void AMosquitoHUD::DrawScore()
{
	AMosquitoCharacter* Mosquito = Cast<AMosquitoSimulatorPlayerController>(Owner) ?
		Cast<AMosquitoCharacter>(Cast<AMosquitoSimulatorPlayerController>(Owner)->GetPawn()) : nullptr;

	if (!Mosquito || !Canvas)
	{
		return;
	}

	const float ScoreX = Canvas->SizeX - 200.f;
	const float ScoreY = BarAnchorY + 46.f;

	// Prompt 14: chip background so the score stays readable over bright sky.
	DrawRect(FLinearColor(0.05f, 0.05f, 0.05f, 0.8f), ScoreX - 8.f, ScoreY - 8.f, 192.f, 56.f);
	DrawText(FString::Printf(TEXT("Last chase: %d"), Mosquito->GetCurrentScore()),
		FLinearColor(1.f, 0.8f, 0.2f), ScoreX, ScoreY, GEngine->GetSmallFont());
	DrawText(FString::Printf(TEXT("Total: %d"), Mosquito->GetTotalScore()),
		FLinearColor(1.f, 0.9f, 0.4f), ScoreX, ScoreY + 22.f, GEngine->GetMediumFont());
}

static UMosquitoSimulatorGameInstance* GetGameSave(const AHUD* Hud)
{
	const UWorld* World = Hud ? Hud->GetWorld() : nullptr;
	return World ? World->GetGameInstance<UMosquitoSimulatorGameInstance>() : nullptr;
}

void AMosquitoHUD::DrawRunProgress(AMosquitoCharacter* Mosquito)
{
	if (!Mosquito || !Canvas)
	{
		return;
	}
	const UMosquitoSimulatorGameInstance* GameSave = GetGameSave(this);
	if (!GameSave)
	{
		return; // null-guard (plan §3): never let new data break the old HUD
	}

	const float X = Canvas->SizeX - 200.f;
	const float Y = BarAnchorY + 100.f;
	DrawRect(FLinearColor(0.05f, 0.05f, 0.05f, 0.8f), X - 8.f, Y - 8.f, 192.f, 40.f);
	DrawText(FString::Printf(TEXT("Level %d  XP %d/%d"), GameSave->RunLevel,
			FMath::FloorToInt(GameSave->RunXP), FMath::RoundToInt32(UMosquitoSimulatorGameInstance::XPToNext(GameSave->RunLevel))),
		FLinearColor(0.4f, 1.f, 0.6f), X, Y, GEngine->GetSmallFont());
	if (GameSave->UnspentLevelUpPoints > 0)
	{
		const float Blink = (FMath::FloorToInt(GetWorld()->GetTimeSeconds() * 2.f) % 2) == 0 ? 1.f : 0.35f;
		DrawText(FString::Printf(TEXT("%d point(s)! press Tab"), GameSave->UnspentLevelUpPoints),
			FLinearColor(1.f, 1.f, 0.2f, Blink), X, Y + 18.f, GEngine->GetSmallFont());
	}
	else
	{
		DrawText(TEXT("Tab: upgrades"), FLinearColor(0.8f, 0.8f, 0.8f), X, Y + 18.f, GEngine->GetSmallFont());
	}
}

void AMosquitoHUD::DrawUpgradePanel(AMosquitoCharacter* Mosquito)
{
	if (!Mosquito || !Canvas || !Mosquito->IsUpgradePanelOpen())
	{
		return;
	}
	const UMosquitoSimulatorGameInstance* GameSave = GetGameSave(this);
	if (!GameSave)
	{
		return;
	}

	const float W = 460.f;
	const float RowH = 40.f;
	const float H = 64.f + 4.f * RowH;
	const float X = (Canvas->SizeX - W) * 0.5f;
	const float Y = BarAnchorY + 60.f;

	DrawRect(FLinearColor::Black, X - 2.f, Y - 2.f, W + 4.f, H + 4.f);
	DrawRect(FLinearColor(0.07f, 0.07f, 0.09f, 0.92f), X, Y, W, H);
	DrawText(TEXT("MOSQUITO UPGRADES (run) - [Tab] close"),
		FLinearColor(1.f, 0.8f, 0.2f), X + 12.f, Y + 8.f, GEngine->GetMediumFont());
	DrawText(FString::Printf(TEXT("Level %d - free points: %d   (1 point per upgrade, max L3)"),
			GameSave->RunLevel, GameSave->UnspentLevelUpPoints),
		FLinearColor::White, X + 12.f, Y + 36.f, GEngine->GetSmallFont());

	const ERunBranch Branches[4] = {
		ERunBranch::WingControl, ERunBranch::MuscularPropulsion,
		ERunBranch::WebEscapeReflexes, ERunBranch::Metabolism};

	for (int32 i = 0; i < 4; ++i)
	{
		const int32 Level = GameSave->GetRunBranchLevel(Branches[i]);
		const bool bMaxed = Level >= MosquitoProgression::MaxRunBranchLevel;
		const bool bAffordable = GameSave->UnspentLevelUpPoints > 0 && !bMaxed;
		const FLinearColor RowColor = bMaxed ? FLinearColor(0.5f, 0.5f, 0.5f)
			: (bAffordable ? FLinearColor(0.4f, 1.f, 0.5f) : FLinearColor(1.f, 0.35f, 0.3f));
		const float RowY = Y + 60.f + i * RowH;
		DrawText(FString::Printf(TEXT("[%d] %s - %s   L%d/%d%s"),
				i + 1,
				*MosquitoProgression::GetRunBranchName(Branches[i]),
				*MosquitoProgression::GetRunBranchEffect(Branches[i]),
				Level, MosquitoProgression::MaxRunBranchLevel,
				bMaxed ? TEXT("  MAX") : TEXT("")),
			RowColor, X + 12.f, RowY, GEngine->GetMediumFont());
	}

	// Plan §3: world is NOT paused while the panel is open - make it visible intent.
	if (FMath::FloorToInt(GetWorld()->GetTimeSeconds() * 2.f) % 2 == 0)
	{
		DrawText(TEXT("(panel open - flight continues)"),
			FLinearColor(0.7f, 0.7f, 0.7f), X + 12.f, Y + H - 20.f, GEngine->GetSmallFont());
	}
}

void AMosquitoHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	if (!bLoggedFirstDraw)
	{
		bLoggedFirstDraw = true;
		UE_LOG(LogTemp, Log, TEXT("[MosquitoHUD] First draw: Canvas=%dx%d - debug HUD is rendering"),
			Canvas->SizeX, Canvas->SizeY);
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
		// Prompt 14: the wings bar turns orange/red as the wings wear out.
		const float Wings = Mosquito->GetWingCondition();
		const FLinearColor WingColor = (Wings < 25.f) ? FLinearColor(1.f, 0.2f, 0.1f)
			: ((Wings < 50.f) ? FLinearColor(1.f, 0.7f, 0.1f) : FLinearColor(0.2f, 0.9f, 0.4f));
		DrawBar(TEXT("Wing HP"), Wings, Mosquito->GetMaxWingCondition(), 4, WingColor);

		// Prompt 12: show death overlay.
		if (Mosquito->IsDead())
		{
			const FString DeadText = TEXT("DEAD — respawning...");
			DrawText(DeadText, FLinearColor::Red, Canvas->SizeX * 0.5f - 80.f, Canvas->SizeY * 0.5f, GEngine->GetLargeFont());
		}
	}

	DrawHumanState();
	DrawClock();
	DrawBiteProgress();
	DrawScore();
	DrawRunProgress(Mosquito);
	DrawUpgradePanel(Mosquito);
	DrawCrosshair();
}

