// Copyright your name. All Rights Reserved.

#include "MosquitoNest.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "MosquitoCharacter.h"
#include "MosquitoPaint.h"
#include "MosquitoSimulatorGameInstance.h"
#include "UObject/ConstructorHelpers.h"

AMosquitoNest::AMosquitoNest()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	// Flattened sphere = a nest "dish" on the ground behind the house.
	DishMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DishMesh"));
	DishMesh->SetupAttachment(RootComponent);
	if (SphereFinder.Succeeded())
	{
		DishMesh->SetStaticMesh(SphereFinder.Object);
	}
	DishMesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 0.18f));
	DishMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DishMesh->SetMobility(EComponentMobility::Movable);

	EggMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EggMesh"));
	EggMesh->SetupAttachment(DishMesh);
	if (SphereFinder.Succeeded())
	{
		EggMesh->SetStaticMesh(SphereFinder.Object);
	}
	EggMesh->SetRelativeLocation(FVector(0.f, 0.f, 0.35f));
	EggMesh->SetRelativeScale3D(FVector(0.25f, 0.25f, 0.15f));
	EggMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EggMesh->SetMobility(EComponentMobility::Movable);
}

void AMosquitoNest::BeginPlay()
{
	Super::BeginPlay();

	NestCenter = GetActorLocation();
	bDevTest = FParse::Param(FCommandLine::Get(), TEXT("NESTTEST"));
	bDevDismiss = FParse::Param(FCommandLine::Get(), TEXT("NESTDISMISSTEST"));

	MosquitoPaint::PaintMesh(DishMesh, FLinearColor(0.32f, 0.22f, 0.10f)); // straw brown
	MosquitoPaint::PaintMesh(EggMesh, FLinearColor(0.85f, 0.82f, 0.75f));  // pale eggs

	UE_LOG(LogTemp, Display, TEXT("[Nest] at (%.0f, %.0f, %.0f) query radius=%.0f cm, blood>=%.0f -> clutch +%d score%s"),
		NestCenter.X, NestCenter.Y, NestCenter.Z, NestRadius, BloodRequiredToClutch, ClutchScore,
		bDevTest ? TEXT(" [NestTest dev flow armed]") : TEXT(""));
}

void AMosquitoNest::Tick(float DeltaTime)
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
	}

	// Generation screen gate (query only): entering the radius with enough blood
	// AUTO-OPENS the screen (owner spec); Esc dismisses the visit until the player
	// leaves and re-enters; Enter confirms the clutch (mosquito-side input handler).
	const bool bInNest = FVector::Dist(Mosquito->GetActorLocation(), NestCenter) <= NestRadius;
	if (bInNest)
	{
		if (!Mosquito->IsDead() && !Mosquito->HasClutchedThisRun() &&
			Mosquito->GetBlood() >= BloodRequiredToClutch)
		{
			Mosquito->OpenGenerationScreen(this, ClutchScore);
		}
	}
	else
	{
		Mosquito->ResetNestVisit(this);
	}
}

void AMosquitoNest::DevTestTick(float DeltaTime, AMosquitoCharacter* Mosquito)
{
	if (DevStage >= 9)
	{
		return;
	}
	DevTimer += DeltaTime;

	const auto GameSave = GetWorld() ? GetWorld()->GetGameInstance<UMosquitoSimulatorGameInstance>() : nullptr;

	if (DevStage == 0 && DevTimer >= 1.5f)
	{
		DevStage = 1;
		Mosquito->SetBlood(BloodRequiredToClutch + 20.f);
		Mosquito->SetActorLocation(NestCenter + FVector(0.f, 0.f, 30.f));
		UE_LOG(LogTemp, Display, TEXT("[NestDev] Seeded blood + teleported to the nest (blood=%.0f need=%.0f)"),
			Mosquito->GetBlood(), BloodRequiredToClutch);
	}

	if (DevStage == 1 && !bDevDismiss)
	{
		// plain flow: t=3 Enter-confirm, t=5 verdict.
		if (!bDevConfirmed && DevTimer >= 3.f)
		{
			bDevConfirmed = true;
			Mosquito->ConfirmClutch();
		}
		if (DevTimer >= 5.f)
		{
			DevStage = 9;
			const int32 Clutches = GameSave ? GameSave->TotalClutches : -1;
			UE_LOG(LogTemp, Display, TEXT("[NestDev] Test complete: TotalClutches=%d %s"),
				Clutches, Clutches >= 1 ? TEXT("(clutch + generation bonus proven)") : TEXT("- FAIL"));
		}
		return;
	}

	if (DevStage == 1 && bDevDismiss)
	{
		// dismiss flow: Esc blocks confirm until a real fly-out/fly-in.
		if (!bDevDismissed && DevTimer >= 2.5f)
		{
			bDevDismissed = true;
			Mosquito->DismissGenerationScreen();
		}
		if (bDevDismissed && !bDevConfirmed && DevTimer >= 3.f)
		{
			bDevConfirmed = true;
			const int32 Before = GameSave ? GameSave->TotalClutches : 0;
			Mosquito->ConfirmClutch(); // must be ignored - screen closed
			const int32 After = GameSave ? GameSave->TotalClutches : -1;
			UE_LOG(LogTemp, Display, TEXT("[NestDev] Confirm-after-Esc ignored=%s (clutch %d->%d)"),
				(Before == After) ? TEXT("yes") : TEXT("NO - FAIL"), Before, After);
		}
		if (bDevConfirmed && !bDevFlewOut && DevTimer >= 3.5f)
		{
			bDevFlewOut = true;
			Mosquito->ResetNestVisit(this); // fly out (one-shot: holding the pose outside
			Mosquito->SetActorLocation(NestCenter + FVector(500.f, 0.f, 30.f)); // would keep closing the reopened screen)
		}
		if (bDevFlewOut && !bDevSecondEntry && DevTimer >= 4.f)
		{
			bDevSecondEntry = true;
			Mosquito->SetActorLocation(NestCenter + FVector(0.f, 0.f, 30.f)); // fly back in -> reopens
		}
		if (bDevSecondEntry && !bDevConfirmed2 && DevTimer >= 4.5f)
		{
			bDevConfirmed2 = true;
			Mosquito->ConfirmClutch(); // now it must land
		}
		if (bDevSecondEntry && DevTimer >= 5.5f)
		{
			DevStage = 9;
			const int32 Clutches = GameSave ? GameSave->TotalClutches : -1;
			UE_LOG(LogTemp, Display, TEXT("[NestDev] Dismiss test complete: TotalClutches=%d %s"),
				Clutches, Clutches == 1 ? TEXT("(Esc blocked, re-entry reopened, Enter confirmed)") : TEXT("- FAIL"));
		}
	}
}
