// Copyright your name. All Rights Reserved.

#include "MosquitoSimulatorGameModeBase.h"
#include "DayNightSystem.h"
#include "HumanCharacter.h"
#include "MosquitoCharacter.h"
#include "MosquitoSimulatorPlayerController.h"
#include "MosquitoWorldBlockout.h"
#include "MosquitoHUD.h"
#include "SpiderCharacter.h"

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

	// MVP 0.2 §1: one spider on one web in the NW quadrant - transient, like humans.
	if (bSpawnSpider && GetWorld() && GetWorld()->IsGameWorld())
	{
		FActorSpawnParameters SpiderParams;
		SpiderParams.ObjectFlags |= RF_Transient;
		SpiderParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASpiderCharacter* Spider = GetWorld()->SpawnActor<ASpiderCharacter>(
			ASpiderCharacter::StaticClass(), FTransform(SpiderWebLocation), SpiderParams);
		if (!Spider)
		{
			UE_LOG(LogTemp, Error, TEXT("[Spider] Spawn FAILED at (%.0f, %.0f, %.0f)"),
				SpiderWebLocation.X, SpiderWebLocation.Y, SpiderWebLocation.Z);
		}
	}

	if (DefaultMap != NAME_None)
	{
		GetWorld()->ServerTravel(DefaultMap.ToString());
	}
}
