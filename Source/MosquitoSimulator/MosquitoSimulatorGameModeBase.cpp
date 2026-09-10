// Copyright your name. All Rights Reserved.

#include "MosquitoSimulatorGameModeBase.h"
#include "DayNightSystem.h"
#include "HumanCharacter.h"
#include "MosquitoCharacter.h"
#include "MosquitoSimulatorPlayerController.h"
#include "MosquitoWorldBlockout.h"
#include "MosquitoHUD.h"
#include "MosquitoNest.h"
#include "SpiderCharacter.h"
#include "Misc/CommandLine.h"

AMosquitoSimulatorGameModeBase::AMosquitoSimulatorGameModeBase()
{
	DefaultMap = NAME_None;
	DefaultPawnClass = AMosquitoCharacter::StaticClass();
	PlayerControllerClass = AMosquitoSimulatorPlayerController::StaticClass();

	// MVP 0.2 §0: the persistent GameInstance is wired via GameInstanceClass under
	// [/Script/EngineSettings.GameMapsSettings] in DefaultEngine.ini. In UE 5.8 the
	// engine creates the GameInstance from that key at UGameEngine::Init/PIE startup
	// (before any GameMode exists); AGameModeBase no longer has a GameInstanceClass field.

	// QA MVP fix: the GameMode pushes its HUDClass to the player via
	// ClientSetHUD() (AGameModeBase.cpp:892). It defaulted to the EMPTY AHUD,
	// so the debug HUD never appeared in PIE even though DrawHUD was correct.
	HUDClass = AMosquitoHUD::StaticClass();

	// Same three spots where the static "human" props used to stand.
	HumanSpawnPoints = {
		FVector(-300.f, 250.f, 90.f),
		FVector(150.f, 550.f, 90.f),
		FVector(500.f, -200.f, 90.f)
	};
}

void AMosquitoSimulatorGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	// MVP 0.2 §0: proof that the persistent GameInstance from the INI is live.
	UE_LOG(LogTemp, Display, TEXT("[GameMode] GameInstance=%s"),
		GetGameInstance() ? *GetGameInstance()->GetClass()->GetName() : TEXT("NONE"));

	// Temporary: provide a testable blockout world until the real level is built.
	if (bSpawnBlockoutWorld && GetWorld() && GetWorld()->IsGameWorld())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags |= RF_Transient;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		GetWorld()->SpawnActor<AMosquitoWorldBlockout>(
			AMosquitoWorldBlockout::StaticClass(), FTransform::Identity, SpawnParams);
	}

	// Prompt 7: living humans replace the static blockout "human" props.
	if (bSpawnHumans && GetWorld() && GetWorld()->IsGameWorld())
	{
		FActorSpawnParameters HumanSpawnParams;
		HumanSpawnParams.ObjectFlags |= RF_Transient;
		HumanSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		for (const FVector& SpawnPoint : HumanSpawnPoints)
		{
			AHumanCharacter* Human = GetWorld()->SpawnActor<AHumanCharacter>(
				AHumanCharacter::StaticClass(), FTransform(SpawnPoint), HumanSpawnParams);
			UE_LOG(LogTemp, Log, TEXT("[Human] Spawn %s at (%.0f, %.0f, %.0f)"),
				Human ? TEXT("OK") : TEXT("FAILED"), SpawnPoint.X, SpawnPoint.Y, SpawnPoint.Z);
		}
	}

	// Prompt 8: day/night cycle (rotates the sun, dims the night).
	if (bSpawnDayNight && GetWorld() && GetWorld()->IsGameWorld())
	{
		FActorSpawnParameters SysParams;
		SysParams.ObjectFlags |= RF_Transient;
		SysParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADayNightSystem* DayNight = GetWorld()->SpawnActor<ADayNightSystem>(
			ADayNightSystem::StaticClass(), FTransform::Identity, SysParams);
		UE_LOG(LogTemp, Log, TEXT("[DayNight] System %s"),
			DayNight ? TEXT("spawned") : TEXT("FAILED"));
	}

	// MVP 0.2 §1: one spider per web (default: a single web in the NW quadrant).
	if (bSpawnSpider && GetWorld() && GetWorld()->IsGameWorld())
	{
		// Dev override for headless cooldown verification: -SpiderRespawn=N.
		float SpiderRespawnOverride = 0.f;
		if (FParse::Value(FCommandLine::Get(), TEXT("SpiderRespawn="), SpiderRespawnOverride) && SpiderRespawnOverride > 0.f)
		{
			SpiderRespawnDelay = SpiderRespawnOverride;
		}

		if (SpiderWebLocations.IsEmpty())
		{
			SpiderWebLocations.Add(FVector(-800.f, 650.f, 60.f)); // by TreeTrunk2
		}
		// Dev override for multi-web headless verification: -SpiderWebs=N
		// (duplicates the default web, offset 15 m apart so zones never share a center).
		int32 SpiderWebsOverride = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("SpiderWebs="), SpiderWebsOverride) && SpiderWebsOverride > SpiderWebLocations.Num())
		{
			const FVector Base = SpiderWebLocations[0];
			while (SpiderWebLocations.Num() < SpiderWebsOverride)
			{
				SpiderWebLocations.Add(Base + FVector(1500.f * SpiderWebLocations.Num(), 0.f, 0.f));
			}
		}

		SpiderWebs.SetNum(SpiderWebLocations.Num());
		for (int32 i = 0; i < SpiderWebLocations.Num(); ++i)
		{
			SpawnSpiderAt(i);
		}
	}

	// MVP 0.3 phase A: nests are permanent - spawn once per world, transient like the rest.
	if (bSpawnNest && GetWorld() && GetWorld()->IsGameWorld())
	{
		if (NestLocations.IsEmpty())
		{
			NestLocations.Add(FVector(900.f, 120.f, 25.f)); // behind the house
		}
		int32 NestCountOverride = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("NestCount="), NestCountOverride) && NestCountOverride > NestLocations.Num())
		{
			const FVector Base = NestLocations[0];
			while (NestLocations.Num() < NestCountOverride)
			{
				NestLocations.Add(Base + FVector(0.f, 2000.f * NestLocations.Num(), 0.f));
			}
		}
		FActorSpawnParameters NestParams;
		NestParams.ObjectFlags |= RF_Transient;
		NestParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		for (const FVector& NestLocation : NestLocations)
		{
			AMosquitoNest* Nest = GetWorld()->SpawnActor<AMosquitoNest>(
				AMosquitoNest::StaticClass(), FTransform(NestLocation), NestParams);
			if (!Nest)
			{
				UE_LOG(LogTemp, Error, TEXT("[Nest] Spawn FAILED at (%.0f, %.0f, %.0f)"),
					NestLocation.X, NestLocation.Y, NestLocation.Z);
			}
		}
	}

	if (DefaultMap != NAME_None)
	{
		GetWorld()->ServerTravel(DefaultMap.ToString());
	}
}

void AMosquitoSimulatorGameModeBase::SpawnSpiderAt(int32 WebIndex)
{
	if (!SpiderWebLocations.IsValidIndex(WebIndex))
	{
		return;
	}
	const FVector& WebLocation = SpiderWebLocations[WebIndex];
	FActorSpawnParameters SpiderParams;
	SpiderParams.ObjectFlags |= RF_Transient;
	SpiderParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASpiderCharacter* Spider = GetWorld()->SpawnActor<ASpiderCharacter>(
		ASpiderCharacter::StaticClass(), FTransform(WebLocation), SpiderParams);
	if (!Spider)
	{
		UE_LOG(LogTemp, Error, TEXT("[Spider] Spawn FAILED at (%.0f, %.0f, %.0f)"),
			WebLocation.X, WebLocation.Y, WebLocation.Z);
		return;
	}
	Spider->WebIndex = WebIndex;
	SpiderWebs[WebIndex].Spider = Spider;
}

void AMosquitoSimulatorGameModeBase::NotifySpiderDied(int32 WebIndex)
{
	if (!GetWorld() || !SpiderWebs.IsValidIndex(WebIndex))
	{
		return;
	}
	// PIE-FIX #3 (owner report): kills used to leave the web empty forever within a
	// session, or the spider came back at an arbitrary moment on world restarts.
	// Now: exactly one timer PER WEB, strict SpiderRespawnDelay, no double-arming.
	FSpiderWeb& Web = SpiderWebs[WebIndex];
	Web.Spider = nullptr;
	if (Web.RespawnTimer.IsValid())
	{
		return; // a respawn is already scheduled for this web
	}
	GetWorldTimerManager().SetTimer(Web.RespawnTimer, FTimerDelegate::CreateUObject(
		this, &AMosquitoSimulatorGameModeBase::RespawnSpiderWeb, WebIndex), SpiderRespawnDelay, false);
	UE_LOG(LogTemp, Display, TEXT("[Spider] Cooldown %.0f s until web %d is rebuilt"), SpiderRespawnDelay, WebIndex);
}

void AMosquitoSimulatorGameModeBase::RespawnSpiderWeb(int32 WebIndex)
{
	if (!SpiderWebs.IsValidIndex(WebIndex))
	{
		return;
	}
	FSpiderWeb& Web = SpiderWebs[WebIndex];
	Web.RespawnTimer.Invalidate();
	if (Web.Spider.IsValid())
	{
		return; // a spider is somehow alive already - never double-spawn
	}
	UE_LOG(LogTemp, Display, TEXT("[Spider] Cooldown over - rebuilding the web"));
	SpawnSpiderAt(WebIndex);
}
