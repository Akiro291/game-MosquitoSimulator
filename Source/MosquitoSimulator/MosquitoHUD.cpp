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

void AMosquitoHUD::DrawWebStatus(AMosquitoCharacter* Mosquito)
{
	if (!Mosquito || !Canvas)
	{
		return;
	}

	const float CX = Canvas->SizeX * 0.5f;

	if (Mosquito->IsTrapped())
	{
		// Plan §3.1: red plate + centered horizontal EscapeMeter bar.
		const float W = 300.f;
		const float H = 16.f;
		const float X = CX - W * 0.5f;
		const float Y = Canvas->SizeY * 0.35f;

		const bool bBlink = FMath::FloorToInt(GetWorld()->GetTimeSeconds() * 4.f) % 2 == 0;
		DrawRect(bBlink ? FLinearColor(0.75f, 0.05f, 0.05f) : FLinearColor(0.45f, 0.04f, 0.04f),
			X, Y - 26.f, W, 20.f);
		DrawText(TEXT("TRAPPED - MASH R!"), FLinearColor::White, X + 8.f, Y - 23.f, GEngine->GetSmallFont());

		DrawRect(FLinearColor::Black, X - 1.f, Y - 1.f, W + 2.f, H + 2.f);
		DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.85f), X, Y, W, H);
		DrawRect(FLinearColor(0.9f, 0.2f, 0.15f), X, Y, W * (1.f - Mosquito->GetEscapeProgress01()), H);
		DrawText(TEXT("ESCAPE"), FLinearColor::White, X + W + 8.f, Y, GEngine->GetSmallFont());
		return;
	}

	if (Mosquito->GetNearestSpiderDistance() <= 250.f)
	{
		const FString Chip = FString::Printf(TEXT("WEB AHEAD (%.1f m)"),
			Mosquito->GetNearestSpiderDistance() * 0.01f);
		const float W = 220.f;
		const float X = CX - W * 0.5f;
		const float Y = Canvas->SizeY * 0.25f;
		DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f), X, Y, W, 20.f);
		DrawText(Chip, FLinearColor(1.f, 0.45f, 0.2f), X + 10.f, Y + 2.f, GEngine->GetMediumFont());
		return;
	}

	// MVP 0.3 A: navigation chip to the nest (owner PIE feedback: hard to find).
	if (!Mosquito->HasClutchedThisRun() && Mosquito->GetNearestNestDistance() <= 900.f)
	{
		const FString Chip = FString::Printf(TEXT("NEST %.1f m - blood %d"),
			Mosquito->GetNearestNestDistance() * 0.01f,
			FMath::FloorToInt(Mosquito->GetBlood()));
		const float W = 240.f;
		const float X = CX - W * 0.5f;
		const float Y = Canvas->SizeY * 0.25f;
		DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f), X, Y, W, 20.f);
		DrawText(Chip, FLinearColor(0.5f, 1.f, 0.6f), X + 10.f, Y + 2.f, GEngine->GetMediumFont());
	}
}

void AMosquitoHUD::DrawGenerationScreen(AMosquitoCharacter* Mosquito)
{
	if (!Mosquito || !Canvas || !Mosquito->IsGenerationScreenOpen())
	{
		return;
	}
	const UMosquitoSimulatorGameInstance* GameSave = GetGameSave(this);
	if (!GameSave)
	{
		return;
	}

	const float W = 520.f;
	const float H = 210.f;
	const float X = (Canvas->SizeX - W) * 0.5f;
	const float Y = (Canvas->SizeY - H) * 0.5f;

	DrawRect(FLinearColor::Black, X - 2.f, Y - 2.f, W + 4.f, H + 4.f);
	DrawRect(FLinearColor(0.05f, 0.07f, 0.05f, 0.93f), X, Y, W, H);
	DrawText(TEXT("THE NEST - your life's work"), FLinearColor(0.5f, 1.f, 0.6f),
		X + 12.f, Y + 8.f, GEngine->GetMediumFont());

	if (GameSave)
	{
		DrawText(FString::Printf(TEXT("Run: Lv%d  score %d   |   lineage: Gen %d, clutches %d, species pts %d"),
				GameSave->RunLevel, Mosquito->GetTotalScore(), GameSave->Generations,
				GameSave->TotalClutches, GameSave->UnspentSpeciesPoints),
			FLinearColor(0.9f, 0.85f, 0.8f), X + 12.f, Y + 40.f, GEngine->GetSmallFont());

		const ESpeciesBranch Branches[3] = {
			ESpeciesBranch::BloodEfficiency, ESpeciesBranch::WebResistantAdhesion, ESpeciesBranch::Exoskeleton};
		for (int32 i = 0; i < 3; ++i)
		{
			const int32 Level = GameSave->GetSpeciesBranchLevel(Branches[i]);
			const FLinearColor RowColor = Level >= MosquitoProgression::MaxSpeciesBranchLevel
				? FLinearColor(0.5f, 0.5f, 0.5f) : FLinearColor(0.4f, 1.f, 0.5f);
			DrawText(FString::Printf(TEXT("%s  L%d/%d"),
					*MosquitoProgression::GetSpeciesBranchName(Branches[i]),
					Level, MosquitoProgression::MaxSpeciesBranchLevel),
				RowColor, X + 12.f, Y + 64.f + i * 22.f, GEngine->GetMediumFont());
		}
	}

	DrawText(FString::Printf(TEXT("[Enter] lay the clutch (+%d score, +1 species pt, +1 mutation)"),
			Mosquito->GetPendingClutchReward()),
		FLinearColor(1.f, 1.f, 0.2f), X + 12.f, Y + H - 44.f, GEngine->GetSmallFont());
	DrawText(TEXT("[Esc] fly away (re-enter the nest to open again)"),
		FLinearColor(0.7f, 0.7f, 0.7f), X + 12.f, Y + H - 24.f, GEngine->GetSmallFont());
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

void AMosquitoHUD::DrawDeathPanel(AMosquitoCharacter* Mosquito)
{
	if (!Mosquito || !Canvas || !Mosquito->IsDead())
	{
		return;
	}
	const UMosquitoSimulatorGameInstance* GameSave = GetGameSave(this);
	if (!GameSave)
	{
		return;
	}

	const float W = 560.f;
	const float H = 260.f;
	const float X = (Canvas->SizeX - W) * 0.5f;
	const float Y = Canvas->SizeY * 0.5f - 20.f;

	DrawRect(FLinearColor::Black, X - 2.f, Y - 2.f, W + 4.f, H + 4.f);
	DrawRect(FLinearColor(0.06f, 0.04f, 0.04f, 0.92f), X, Y, W, H);

	DrawText(TEXT("THE SWARM REMEMBERS HER"), FLinearColor(1.f, 0.4f, 0.3f),
		X + 12.f, Y + 8.f, GEngine->GetMediumFont());
	const int32 MutIdx = GameSave->LastMutation;
	const FString MutName = (MutIdx >= 1 && MutIdx <= 3)
		? MosquitoProgression::GetSpeciesBranchName(static_cast<ESpeciesBranch>(MutIdx - 1))
		: FString(TEXT("none yet"));

	DrawText(FString::Printf(
			TEXT("Run total: %d   Lifetime: %d   Best chase: %d   Generation: %d"),
			Mosquito->GetTotalScore(), GameSave->LifetimeScore,
			GameSave->BestChaseScore, GameSave->Generations),
		FLinearColor(0.9f, 0.85f, 0.8f), X + 12.f, Y + 36.f, GEngine->GetSmallFont());
	DrawText(FString::Printf(TEXT("Clutches laid: %d   last nest mutation: %s"),
			GameSave->TotalClutches, *MutName),
		FLinearColor(0.55f, 0.9f, 0.65f), X + 12.f, Y + 56.f, GEngine->GetSmallFont());
	DrawText(FString::Printf(TEXT("Species points: %d   (new mosquito: Lv1 run, blood 20, hunger 70 - soft punishment)"),
			GameSave->UnspentSpeciesPoints),
		FLinearColor(1.f, 1.f, 0.2f), X + 12.f, Y + 76.f, GEngine->GetSmallFont());

	const ESpeciesBranch Branches[3] = {
		ESpeciesBranch::BloodEfficiency, ESpeciesBranch::WebResistantAdhesion, ESpeciesBranch::Exoskeleton};

	for (int32 i = 0; i < 3; ++i)
	{
		const int32 Level = GameSave->GetSpeciesBranchLevel(Branches[i]);
		const bool bMaxed = Level >= MosquitoProgression::MaxSpeciesBranchLevel;
		const bool bAffordable = GameSave->UnspentSpeciesPoints > 0 && !bMaxed;
		const FLinearColor RowColor = bMaxed ? FLinearColor(0.5f, 0.5f, 0.5f)
			: (bAffordable ? FLinearColor(0.4f, 1.f, 0.5f) : FLinearColor(1.f, 0.35f, 0.3f));
		DrawText(FString::Printf(TEXT("[%d] %s   L%d/%d%s"),
				i + 1, *MosquitoProgression::GetSpeciesBranchName(Branches[i]),
				Level, MosquitoProgression::MaxSpeciesBranchLevel,
				bMaxed ? TEXT("  MAX") : TEXT("")),
				RowColor, X + 12.f, Y + 104.f + i * 22.f, GEngine->GetMediumFont());
	}

	DrawText(TEXT("keys 1-3 buy for the whole lineage - persisted immediately"),
		FLinearColor(0.7f, 0.7f, 0.7f), X + 12.f, Y + H - 24.f, GEngine->GetSmallFont());
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

		// Prompt 12: show death overlay. MVP 0.3 A: nest-clutch deaths read LIFE COMPLETE.
		if (Mosquito->IsDead())
		{
			const FString DeadText = Mosquito->IsLifeComplete()
				? TEXT("LIFE COMPLETE - the lineage continues...")
				: TEXT("DEAD — respawning...");
			DrawText(DeadText, Mosquito->IsLifeComplete() ? FLinearColor(0.4f, 1.f, 0.5f) : FLinearColor::Red,
				Canvas->SizeX * 0.5f - 140.f, Canvas->SizeY * 0.5f - 40.f, GEngine->GetLargeFont());
			DrawDeathPanel(Mosquito);
		}
	}

	DrawHumanState();
	DrawClock();
	DrawBiteProgress();
	DrawScore();
	DrawRunProgress(Mosquito);
	DrawWebStatus(Mosquito);
	DrawUpgradePanel(Mosquito);
	DrawGenerationScreen(Mosquito);
	DrawCrosshair();
}

