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
		TEXT("speciesPts=%d branches=%d/%d/%d"),
		Version, LifetimeScore, BestChaseScore, TotalDeaths, Generations,
		UnspentSpeciesPoints, SpeciesBloodEfficiency, SpeciesWebResistantAdhesion,
		SpeciesExoskeleton);

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

bool UMosquitoSimulatorGameInstance::BuySpeciesBranchLevel(int32& InOutBranchLevel)
{
	if (InOutBranchLevel >= MaxSpeciesBranchLevel || UnspentSpeciesPoints <= 0)
	{
		return false;
	}
	--UnspentSpeciesPoints;
	++InOutBranchLevel;
	bDirty = true;
	return true;
}

void UMosquitoSimulatorGameInstance::AddLifetimeScore(int32 Amount)
{
	if (Amount > 0)
	{
		LifetimeScore += Amount;
		bDirty = true;
	}
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

	if (!FFileHelper::SaveStringArrayToFile(Lines, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Save] FAILED writing %s"), *Path);
		return;
	}
	bDirty = false;
	UE_LOG(LogTemp, Display, TEXT("[Save] Written version=%d lifetime=%d speciesPts=%d"),
		Version, LifetimeScore, UnspentSpeciesPoints);
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
		else if (Key == TEXT("LifetimeScore")) { LifetimeScore = Value; }
		else if (Key == TEXT("BestChaseScore")) { BestChaseScore = Value; }
		else if (Key == TEXT("TotalDeaths")) { TotalDeaths = Value; }
		else if (Key == TEXT("Generations")) { Generations = Value; }
		else if (Key == TEXT("UnspentSpeciesPoints")) { UnspentSpeciesPoints = Value; }
		else if (Key == TEXT("SpeciesBloodEfficiency")) { SpeciesBloodEfficiency = FMath::Clamp(Value, 0, MaxSpeciesBranchLevel); }
		else if (Key == TEXT("SpeciesWebResistantAdhesion")) { SpeciesWebResistantAdhesion = FMath::Clamp(Value, 0, MaxSpeciesBranchLevel); }
		else if (Key == TEXT("SpeciesExoskeleton")) { SpeciesExoskeleton = FMath::Clamp(Value, 0, MaxSpeciesBranchLevel); }
		else if (Key == TEXT("SpeciesAwardBaseScore")) { SpeciesAwardBaseScore = Value; }
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
}
