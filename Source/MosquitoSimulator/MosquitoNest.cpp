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

	// Clutch gate (query only): a living, not-yet-reproduced, full-enough mosquito
	// inside the radius completes its life through the existing death accounting.
	if (!Mosquito->IsDead() && !Mosquito->HasClutchedThisRun() &&
		Mosquito->GetBlood() >= BloodRequiredToClutch &&
		FVector::Dist(Mosquito->GetActorLocation(), NestCenter) <= NestRadius)
	{
		Mosquito->CompleteLifeCycle(ClutchScore);
	}
}

void AMosquitoNest::DevTestTick(float DeltaTime, AMosquitoCharacter* Mosquito)
{
	if (DevStage >= 2)
	{
		return;
	}
	DevTimer += DeltaTime;

	if (DevStage == 0 && DevTimer >= 1.5f)
	{
		DevStage = 1;
		Mosquito->SetBlood(BloodRequiredToClutch + 20.f);
		Mosquito->SetActorLocation(NestCenter + FVector(0.f, 0.f, 30.f));
		UE_LOG(LogTemp, Display, TEXT("[NestDev] Seeded blood + teleported to the nest (blood=%.0f need=%.0f)"),
			Mosquito->GetBlood(), BloodRequiredToClutch);
	}
	else if (DevStage == 1 && DevTimer >= 5.f)
	{
		DevStage = 2;
		const UMosquitoSimulatorGameInstance* GameSave =
			GetWorld() ? GetWorld()->GetGameInstance<UMosquitoSimulatorGameInstance>() : nullptr;
		const int32 Clutches = GameSave ? GameSave->TotalClutches : -1;
		UE_LOG(LogTemp, Display, TEXT("[NestDev] Test complete: TotalClutches=%d %s"),
			Clutches, Clutches >= 1 ? TEXT("(clutch + generation bonus proven)") : TEXT("- FAIL"));
	}
}
