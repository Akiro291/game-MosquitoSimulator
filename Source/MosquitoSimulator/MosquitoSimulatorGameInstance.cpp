// Copyright your name. All Rights Reserved.

#include "MosquitoSimulatorGameInstance.h"
#include "Engine/Engine.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"

void UMosquitoSimulatorGameInstance::Init()
{
	Super::Init();

	if (FParse::Param(FCommandLine::Get(), TEXT("WIPESAVE")))
	{
		const FString Path = GetSaveFilePath();
		const bool bDeleted = IFileManager::Get().Delete(*Path);
		UE_LOG(LogTemp, Display, TEXT("[Save] Wiped %s (%s)"), *Path,
			bDeleted ? TEXT("deleted") : TEXT("nothing to delete"));
	}

	Load();

	if (Version != CurrentSaveVersion)
	{
		ResetToDefaults(Version);
		SaveNow();
	}

	UE_LOG(LogTemp, Display,
		TEXT("[Save] Loaded version=%d lifetime=%d bestChase=%d deaths=%d generations=%d ")
		TEXT("speciesPts=%d branches=%d/%d/%d clutch=%d lastMut=%d"),
		Version, LifetimeScore, BestChaseScore, TotalDeaths, Generations,
		UnspentSpeciesPoints, SpeciesBloodEfficiency, SpeciesWebResistantAdhesion,
		SpeciesExoskeleton, TotalClutches, LastMutation);

	// Dev helper for headless write->read proof (plan §2): writes deterministic values.
	if (FParse::Param(FCommandLine::Get(), TEXT("SEEDSAVE")))
	{
		LifetimeScore = 123;
		BestChaseScore = 50;
		TotalDeaths = 1;
		Generations = 1;
		UnspentSpeciesPoints = 1;
		SaveNow();
		UE_LOG(LogTemp, Display, TEXT("[Save] Seeded for headless verification"));
	}
}

void UMosquitoSimulatorGameInstance::Shutdown()
{
	if (bDirty)
	{
		SaveNow();
	}
	Super::Shutdown();
}

UMosquitoSimulatorGameInstance* UMosquitoSimulatorGameInstance::Get(const UObject* WorldContextObject)
{
	if (GEngine)
	{
		if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
		{
			return World->GetGameInstance<UMosquitoSimulatorGameInstance>();
		}
	}
	return nullptr;
}

FString UMosquitoSimulatorGameInstance::GetSaveFilePath()
{
	// Plan §2: the save file is literally "MosquitoSave.ini" under a Config dir -
	// we place it in Saved (user data, survives builds), not the source-controlled Config/.
	return FPaths::ProjectSavedDir() / TEXT("Config") / TEXT("MosquitoSave.ini");
}

void UMosquitoSimulatorGameInstance::AddXP(float Amount)
{
	RunXP += Amount;
	while (RunXP >= XPToNext(RunLevel))
	{
		RunXP -= XPToNext(RunLevel);
		++RunLevel;
		++UnspentLevelUpPoints;
		UE_LOG(LogTemp, Display, TEXT("[Progression] Level up! RunLevel=%d points=%d"), RunLevel, UnspentLevelUpPoints);
	}
}

bool UMosquitoSimulatorGameInstance::TrySpendLevelUpPoint()
{
	if (UnspentLevelUpPoints > 0)
	{
		--UnspentLevelUpPoints;
		return true;
	}
	return false;
}

// --- §4 layer A: run branches ---

int32 UMosquitoSimulatorGameInstance::GetRunBranchLevel(ERunBranch Branch) const
{
	const int32 Index = static_cast<int32>(Branch);
	return (Index >= 0 && Index <= static_cast<int32>(ERunBranch::Metabolism))
		? RunBranchLevels[Index] : 0;
}

bool UMosquitoSimulatorGameInstance::BuyRunUpgrade(ERunBranch Branch)
{
	const int32 Index = static_cast<int32>(Branch);
	if (Index < 0 || Index > static_cast<int32>(ERunBranch::Metabolism))
	{
		return false;
	}
	int32& Level = RunBranchLevels[Index];
	if (Level >= MosquitoProgression::MaxRunBranchLevel)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Progression] %s already at max level"),
			*MosquitoProgression::GetRunBranchName(Branch));
		return false;
	}
	if (!TrySpendLevelUpPoint())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Progression] No level-up points for %s"),
			*MosquitoProgression::GetRunBranchName(Branch));
		return false;
	}
	++Level;
	UE_LOG(LogTemp, Display, TEXT("[Progression] Bought %s -> L%d (%d point(s) left)"),
		*MosquitoProgression::GetRunBranchName(Branch), Level, UnspentLevelUpPoints);
	return true;
}

void UMosquitoSimulatorGameInstance::ResetRunProgress()
{
	RunXP = 0.f;
	RunLevel = 1;
	UnspentLevelUpPoints = 0;
	RunBranchLevels[static_cast<int32>(ERunBranch::WingControl)] = 0;
	RunBranchLevels[static_cast<int32>(ERunBranch::MuscularPropulsion)] = 0;
	RunBranchLevels[static_cast<int32>(ERunBranch::WebEscapeReflexes)] = 0;
	RunBranchLevels[static_cast<int32>(ERunBranch::Metabolism)] = 0;
}

float UMosquitoSimulatorGameInstance::GetWingControlMult() const
{
	return FMath::Pow(MosquitoProgression::WingControlAccelPerLevel, GetRunBranchLevel(ERunBranch::WingControl));
}

float UMosquitoSimulatorGameInstance::GetPropulsionMult() const
{
	return FMath::Pow(MosquitoProgression::PropulsionSpeedPerLevel, GetRunBranchLevel(ERunBranch::MuscularPropulsion));
}

float UMosquitoSimulatorGameInstance::GetWebEscapeMult() const
{
	return FMath::Pow(MosquitoProgression::WebEscapePerTapPerLevel, GetRunBranchLevel(ERunBranch::WebEscapeReflexes));
}

float UMosquitoSimulatorGameInstance::GetMetabolismMult() const
{
	return FMath::Pow(MosquitoProgression::MetabolismHungerPerLevel, GetRunBranchLevel(ERunBranch::Metabolism));
}

// --- §4 layer B: species branches ---

int32 UMosquitoSimulatorGameInstance::GetSpeciesBranchLevel(ESpeciesBranch Branch) const
{
	switch (Branch)
	{
	case ESpeciesBranch::BloodEfficiency: return SpeciesBloodEfficiency;
	case ESpeciesBranch::WebResistantAdhesion: return SpeciesWebResistantAdhesion;
	case ESpeciesBranch::Exoskeleton: return SpeciesExoskeleton;
	default: return 0;
	}
}

bool UMosquitoSimulatorGameInstance::BuySpeciesBranchLevel(ESpeciesBranch Branch)
{
	int32* Level = nullptr;
	switch (Branch)
	{
	case ESpeciesBranch::BloodEfficiency: Level = &SpeciesBloodEfficiency; break;
	case ESpeciesBranch::WebResistantAdhesion: Level = &SpeciesWebResistantAdhesion; break;
	case ESpeciesBranch::Exoskeleton: Level = &SpeciesExoskeleton; break;
	default: return false;
	}

	if (*Level >= MaxSpeciesBranchLevel || UnspentSpeciesPoints <= 0)
	{
		return false;
	}
	--UnspentSpeciesPoints;
	++(*Level);
	bDirty = true;
	UE_LOG(LogTemp, Display, TEXT("[Progression] Species bought %s -> L%d (%d point(s) left)"),
		*MosquitoProgression::GetSpeciesBranchName(Branch), *Level, UnspentSpeciesPoints);
	SaveNow(); // plan §2: persist immediately on species purchase
	return true;
}

float UMosquitoSimulatorGameInstance::GetBloodEfficiencyMult() const
{
	return FMath::Pow(MosquitoProgression::BloodEfficiencyGainPerLevel, SpeciesBloodEfficiency);
}

float UMosquitoSimulatorGameInstance::GetWebResistantAdhesionMult() const
{
	return FMath::Pow(MosquitoProgression::WebResistantAdhesionRecoverPerLevel, SpeciesWebResistantAdhesion);
}

float UMosquitoSimulatorGameInstance::GetExoskeletonHpBonus() const
{
	return MosquitoProgression::ExoskeletonHpPerLevel * SpeciesExoskeleton;
}

void UMosquitoSimulatorGameInstance::AddLifetimeScore(int32 Amount)
{
	if (Amount > 0)
	{
		LifetimeScore += Amount;
		bDirty = true;
	}
}

void UMosquitoSimulatorGameInstance::NotifyClutchLaid()
{
	++TotalClutches;
	++UnspentSpeciesPoints; // guaranteed biological payout for reaching the nest

	// Deterministic mutation roll so the headless proof is reproducible: seeded
	// by the clutch number, ONE species branch +1 level. If the rolled branch is
	// already maxed, walk forward to the next non-max one (a clutch never wastes
	// its mutation). All three maxed -> skip.
	FRandomStream Rng(20260910 + TotalClutches * 7919);
	const int32 Roll = Rng.RandRange(0, 2);
	ESpeciesBranch Branch = ESpeciesBranch::BloodEfficiency;
	bool bFound = false;
	for (int32 Offset = 0; Offset <= 2 && !bFound; ++Offset)
	{
		const ESpeciesBranch Candidate = static_cast<ESpeciesBranch>((Roll + Offset) % 3);
		if (GetSpeciesBranchLevel(Candidate) < MaxSpeciesBranchLevel)
		{
			Branch = Candidate;
			bFound = true;
		}
	}
	bool bApplied = false;
	int32 NewLevel = 0;
	if (bFound)
	{
		switch (Branch)
		{
		case ESpeciesBranch::BloodEfficiency: ++SpeciesBloodEfficiency; break;
		case ESpeciesBranch::WebResistantAdhesion: ++SpeciesWebResistantAdhesion; break;
		case ESpeciesBranch::Exoskeleton: ++SpeciesExoskeleton; break;
		default: break;
		}
		NewLevel = GetSpeciesBranchLevel(Branch);
		LastMutation = static_cast<int32>(Branch) + 1;
		bApplied = true;
	}

	UE_LOG(LogTemp, Display, TEXT("[Nest] Clutch #%d booked: +1 species pt (total=%d)%s%s"),
		TotalClutches, UnspentSpeciesPoints,
		bApplied ? TEXT(", mutation: ") : TEXT(", all branches maxed - mutation skipped"),
		bApplied ? *FString::Printf(TEXT("%s -> L%d"), *MosquitoProgression::GetSpeciesBranchName(Branch), NewLevel)
			: TEXT(""));

	SaveNow(); // the generation is a persistence event (plan §2: death/purchase/...)
}

void UMosquitoSimulatorGameInstance::SetBestChaseScoreIfHigher(int32 Score)
{
	if (Score > BestChaseScore)
	{
		BestChaseScore = Score;
		bDirty = true;
	}
}

void UMosquitoSimulatorGameInstance::AwardSpeciesPoints()
{
	const int32 Pending = (LifetimeScore - SpeciesAwardBaseScore) / SpeciesPointsPerLifetimeScore;
	if (Pending > 0)
	{
		UnspentSpeciesPoints += Pending;
		SpeciesAwardBaseScore += Pending * SpeciesPointsPerLifetimeScore;
		UE_LOG(LogTemp, Display, TEXT("[Progression] Species reward +%d point(s) (total=%d)"),
			Pending, UnspentSpeciesPoints);
	}
}

void UMosquitoSimulatorGameInstance::SaveNow()
{
	Version = CurrentSaveVersion;

	const FString Path = GetSaveFilePath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);

	TArray<FString> Lines;
	Lines.Add(TEXT("; MosquitoSimulator persistent save (MVP 0.2). Do not edit while the game runs."));
	Lines.Add(TEXT("[MosquitoSave]"));
	Lines.Add(FString::Printf(TEXT("Version=%d"), Version));
	Lines.Add(FString::Printf(TEXT("LifetimeScore=%d"), LifetimeScore));
	Lines.Add(FString::Printf(TEXT("BestChaseScore=%d"), BestChaseScore));
	Lines.Add(FString::Printf(TEXT("TotalDeaths=%d"), TotalDeaths));
	Lines.Add(FString::Printf(TEXT("Generations=%d"), Generations));
	Lines.Add(FString::Printf(TEXT("UnspentSpeciesPoints=%d"), UnspentSpeciesPoints));
	Lines.Add(FString::Printf(TEXT("SpeciesBloodEfficiency=%d"), SpeciesBloodEfficiency));
	Lines.Add(FString::Printf(TEXT("SpeciesWebResistantAdhesion=%d"), SpeciesWebResistantAdhesion));
	Lines.Add(FString::Printf(TEXT("SpeciesExoskeleton=%d"), SpeciesExoskeleton));
	Lines.Add(FString::Printf(TEXT("SpeciesAwardBaseScore=%d"), SpeciesAwardBaseScore));
	Lines.Add(FString::Printf(TEXT("TotalClutches=%d"), TotalClutches));
	Lines.Add(FString::Printf(TEXT("LastMutation=%d"), LastMutation));

	// Atomic write: temp file first, then replace - a crash mid-write can never
	// corrupt the previous save (the loader also tolerates garbage, belt & braces).
	const FString TempPath = Path + TEXT(".tmp");
	if (!FFileHelper::SaveStringArrayToFile(Lines, *TempPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Save] FAILED writing %s"), *TempPath);
		return;
	}
	IFileManager::Get().Delete(*Path); // MoveFile over an existing file needs ReplaceExisting semantics
	if (!IFileManager::Get().Move(*Path, *TempPath, /*bReplace=*/true))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Save] FAILED to move %s -> %s"), *TempPath, *Path);
		return;
	}
	bDirty = false;
	UE_LOG(LogTemp, Display, TEXT("[Save] Written version=%d lifetime=%d speciesPts=%d clutch=%d"),
		Version, LifetimeScore, UnspentSpeciesPoints, TotalClutches);
}

void UMosquitoSimulatorGameInstance::Load()
{
	const FString Path = GetSaveFilePath();
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *Path))
	{
		UE_LOG(LogTemp, Display, TEXT("[Save] No save file at %s - fresh defaults"), *Path);
		return;
	}

	for (FString& RawLine : Lines)
	{
		const FString Line = RawLine.TrimStartAndEnd();
		if (Line.IsEmpty() || Line.StartsWith(TEXT(";")) || Line.StartsWith(TEXT("[")))
		{
			continue;
		}
		int32 EqIdx;
		if (!Line.FindChar(TEXT('='), EqIdx))
		{
			continue; // corrupt line - ignore (load with defaults, no crash, plan §2)
		}
		const FString Key = Line.Left(EqIdx).TrimStartAndEnd();
		int32 Value;
		if (!LexTryParseString(Value, *Line.Mid(EqIdx + 1).TrimStartAndEnd()))
		{
			continue; // corrupt value - ignore
		}

		if (Key == TEXT("Version")) { Version = Value; }
		else if (Key == TEXT("LifetimeScore")) { LifetimeScore = FMath::Max(0, Value); }
		else if (Key == TEXT("BestChaseScore")) { BestChaseScore = FMath::Max(0, Value); }
		else if (Key == TEXT("TotalDeaths")) { TotalDeaths = FMath::Max(0, Value); }
		else if (Key == TEXT("Generations")) { Generations = FMath::Max(0, Value); }
		else if (Key == TEXT("UnspentSpeciesPoints")) { UnspentSpeciesPoints = FMath::Max(0, Value); }
		else if (Key == TEXT("SpeciesBloodEfficiency")) { SpeciesBloodEfficiency = FMath::Clamp(Value, 0, MaxSpeciesBranchLevel); }
		else if (Key == TEXT("SpeciesWebResistantAdhesion")) { SpeciesWebResistantAdhesion = FMath::Clamp(Value, 0, MaxSpeciesBranchLevel); }
		else if (Key == TEXT("SpeciesExoskeleton")) { SpeciesExoskeleton = FMath::Clamp(Value, 0, MaxSpeciesBranchLevel); }
		else if (Key == TEXT("SpeciesAwardBaseScore")) { SpeciesAwardBaseScore = Value; }
		else if (Key == TEXT("TotalClutches")) { TotalClutches = FMath::Max(0, Value); }
		else if (Key == TEXT("LastMutation")) { LastMutation = FMath::Clamp(Value, 0, 3); }
	}
}

void UMosquitoSimulatorGameInstance::ResetToDefaults(int32 ReasonVersion)
{
	UE_LOG(LogTemp, Warning, TEXT("[Save] Version mismatch (got %d, want %d) - resetting to defaults"),
		ReasonVersion, CurrentSaveVersion);
	Version = CurrentSaveVersion;
	LifetimeScore = 0;
	BestChaseScore = 0;
	TotalDeaths = 0;
	Generations = 0;
	UnspentSpeciesPoints = 0;
	SpeciesBloodEfficiency = 0;
	SpeciesWebResistantAdhesion = 0;
	SpeciesExoskeleton = 0;
	SpeciesAwardBaseScore = 0;
	// MVP 0.3 A: reset-to-defaults wipes the clutch bookkeeping too (fresh lineage).
	TotalClutches = 0;
	LastMutation = 0;
}
