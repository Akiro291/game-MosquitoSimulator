// Copyright your name. All Rights Reserved.

#include "SpiderCharacter.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Misc/CommandLine.h"
#include "MosquitoCharacter.h"
#include "UObject/ConstructorHelpers.h"

ASpiderCharacter::ASpiderCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	if (SphereFinder.Succeeded())
	{
		BodyMesh->SetStaticMesh(SphereFinder.Object);
	}
	// Engine sphere is 100 uu (=100 cm) wide; 0.1 -> a 10 cm spider, visible vs the 2 cm mosquito.
	BodyMesh->SetRelativeScale3D(FVector(0.1f));
	// Web/spider are visual + query only: zero collision objects (plan §1).
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetMobility(EComponentMobility::Movable);
}

void ASpiderCharacter::BeginPlay()
{
	Super::BeginPlay();

	WebCenter = GetActorLocation();
	bDevTest = FParse::Param(FCommandLine::Get(), TEXT("SPIDERTEST"));

	UE_LOG(LogTemp, Display, TEXT("[Spider] Spawn OK. Web at (%.0f, %.0f, %.0f) radius=%.0f cm%s"),
		WebCenter.X, WebCenter.Y, WebCenter.Z, WebRadius,
		bDevTest ? TEXT(" [SpiderTest dev flow armed]") : TEXT(""));
}

void ASpiderCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!CachedMosquito.IsValid())
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return;
		}
		for (TActorIterator<AMosquitoCharacter> It(World); It; ++It)
		{
			CachedMosquito = *It;
			break;
		}
	}
	AMosquitoCharacter* Mosquito = CachedMosquito.Get();
	if (!Mosquito)
	{
		return;
	}

	if (bDevTest)
	{
		DevTestTick(DeltaTime, Mosquito);
		return; // the scripted dev flow owns the test run
	}

	// Natural trap: gameplay query only (like NearestHumanDistance). Humans never
	// query the web - they simply walk through it (plan §1).
	if (!Mosquito->IsDead() && !Mosquito->IsTrapped() &&
		FVector::Dist(Mosquito->GetActorLocation(), WebCenter) <= WebRadius)
	{
		Mosquito->EnterWeb();
	}
}

void ASpiderCharacter::DevTestTick(float DeltaTime, AMosquitoCharacter* Mosquito)
{
	if (bDevTestDone)
	{
		return;
	}
	DevTestTimer += DeltaTime;

	// t=1.5 s: drop the player into the middle of the web.
	if (!bDevTrappedRequested && DevTestTimer >= 1.5f)
	{
		bDevTrappedRequested = true;
		Mosquito->SetActorLocation(WebCenter);
		Mosquito->EnterWeb();
		UE_LOG(LogTemp, Display, TEXT("[SpiderDev] Teleported player into the web (trapped=%s)"),
			Mosquito->IsTrapped() ? TEXT("yes") : TEXT("NO - FAIL"));
	}

	// t=3.0 s..: emulate fast R mashing - 12 taps at 4/s beats 0.05/s recovery.
	// Escape observed after the tap sequence - close the test and leave the zone.
	if (bDevTrappedRequested && !Mosquito->IsTrapped() && DevTapCount > 0)
	{
		bDevTestDone = true;
		Mosquito->SetActorLocation(WebCenter + FVector(400.f, 0.f, 0.f)); // leave the zone for good
		UE_LOG(LogTemp, Display, TEXT("[SpiderDev] Test complete - ESCAPED proven (taps=%d)"), 12);
		return;
	}
	if (bDevTrappedRequested && Mosquito->IsTrapped() && DevTestTimer >= 3.f)
	{
		const int32 TargetTaps = FMath::Min(12, 1 + FMath::FloorToInt((DevTestTimer - 3.f) / 0.25f));
		while (DevTapCount < TargetTaps)
		{
			++DevTapCount;
			Mosquito->Struggle();
		}
	}
}
