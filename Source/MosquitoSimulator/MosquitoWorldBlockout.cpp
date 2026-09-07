// Copyright your name. All Rights Reserved.

#include "MosquitoWorldBlockout.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MosquitoPaint.h"
#include "Materials/MaterialInterface.h"

AMosquitoWorldBlockout::AMosquitoWorldBlockout()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	// QA MVP: static root so the static primitive meshes can attach without
	// the 10x "cannot attach static to non-static" PIE warnings (actor never moves).
	Root->SetMobility(EComponentMobility::Static);
	SetRootComponent(Root);

	// All sizes assume 1 uu = 1 cm.

	// Big ground plane: 200 m x 200 m, 1 cm above the template floor to avoid z-fighting.
	MakePlane(TEXT("Ground"), FVector(200.f, 200.f, 1.f), FVector(0.f, 0.f, 1.f));

	// House: 5 x 4 x 3 m box, 6 m from the spawn.
	MakeBox(TEXT("House"), FVector(5.f, 4.f, 3.f), FVector(600.f, 0.f, 151.f));

	// Garden table: 1.2 x 0.8 m top at 75 cm height, single pedestal.
	MakeBox(TEXT("TableTop"), FVector(1.2f, 0.8f, 0.06f), FVector(-250.f, -350.f, 76.f));
	MakeCylinder(TEXT("TableLeg"), FVector(0.4f, 0.4f, 0.75f), FVector(-250.f, -350.f, 38.5f));

	// Water sources (future breeding spots).
	MakeCylinder(TEXT("WaterBucket"), FVector(0.35f, 0.35f, 0.35f), FVector(350.f, -450.f, 18.5f));
	MakeCylinder(TEXT("WaterBarrel"), FVector(0.6f, 0.6f, 0.9f), FVector(-550.f, -650.f, 46.f));

	// Trees.
	MakeCylinder(TEXT("TreeTrunk1"), FVector(0.25f, 0.25f, 2.5f), FVector(900.f, 500.f, 126.f));
	MakeSphere(TEXT("TreeCrown1"), FVector(1.3f, 1.3f, 1.1f), FVector(900.f, 500.f, 280.f));
	MakeCylinder(TEXT("TreeTrunk2"), FVector(0.25f, 0.25f, 2.5f), FVector(-800.f, 650.f, 126.f));
	MakeSphere(TEXT("TreeCrown2"), FVector(1.2f, 1.2f, 1.0f), FVector(-800.f, 650.f, 265.f));

	// NOTE: static "human" props were replaced by living AHumanCharacter NPCs
	// spawned by the GameMode (Prompt 7) at the same three spots.

	// Optional sun (MainLevel already provides lighting, so off by default).
	if (bSpawnLighting)
	{
		Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
		Sun->SetupAttachment(Root);
		Sun->SetRelativeRotation(FRotator(-45.f, -30.f, 0.f));
		Sun->SetIntensity(10.f);
		Sun->SetMobility(EComponentMobility::Movable); // rotated by the day/night system later
	}
}

void AMosquitoWorldBlockout::BeginPlay()
{
	Super::BeginPlay();

	if (bSpawnPlayerStart && GetWorld())
	{
		bool bFoundPlayerStart = false;
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			bFoundPlayerStart = true;
			break;
		}

		if (!bFoundPlayerStart)
		{
			FActorSpawnParameters Params;
			Params.ObjectFlags |= RF_Transient;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			APlayerStart* NewStart = GetWorld()->SpawnActor<APlayerStart>(
				APlayerStart::StaticClass(),
				FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 120.f)),
				Params);
			UE_LOG(LogTemp, Log, TEXT("[MosquitoBlockout] PlayerStart %s"),
				NewStart ? TEXT("spawned at (0,0,120)") : TEXT("spawn FAILED"));
		}
	}

	// Prompt 14: paint the primitives so the world reads visually.
	ApplyBlockoutColors();

	UE_LOG(LogTemp, Log, TEXT("[MosquitoBlockout] Blockout village ready (1 uu = 1 cm)"));
}

UStaticMeshComponent* AMosquitoWorldBlockout::MakeBox(const FName& Name, const FVector& Scale, const FVector& Location)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	// CreateDefaultSubobject (NOT NewObject+RegisterComponent): registering a
	// component from the constructor fails the CDO build with
	// "ensure(MyOwnerWorld)" because the CDO has no world yet. Default
	// subobjects are registered automatically when the actor enters a world.
	UStaticMeshComponent* Comp = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	if (MeshFinder.Succeeded())
	{
		Comp->SetStaticMesh(MeshFinder.Object);
	}
	Comp->SetupAttachment(GetRootComponent());
	Comp->SetRelativeLocation(Location);
	Comp->SetRelativeScale3D(Scale); // engine cube is 100 uu -> scale 1.0 = 1 m
	Comp->SetMobility(EComponentMobility::Static);
	return Comp;
}

UStaticMeshComponent* AMosquitoWorldBlockout::MakeCylinder(const FName& Name, const FVector& Scale, const FVector& Location)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	// See MakeBox(): CreateDefaultSubobject, no manual RegisterComponent.
	UStaticMeshComponent* Comp = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	if (MeshFinder.Succeeded())
	{
		Comp->SetStaticMesh(MeshFinder.Object);
	}
	Comp->SetupAttachment(GetRootComponent());
	Comp->SetRelativeLocation(Location);
	Comp->SetRelativeScale3D(Scale); // engine cylinder is 100 uu tall/wide
	Comp->SetMobility(EComponentMobility::Static);
	return Comp;
}

UStaticMeshComponent* AMosquitoWorldBlockout::MakeSphere(const FName& Name, const FVector& Scale, const FVector& Location)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	// See MakeBox(): CreateDefaultSubobject, no manual RegisterComponent.
	UStaticMeshComponent* Comp = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	if (MeshFinder.Succeeded())
	{
		Comp->SetStaticMesh(MeshFinder.Object);
	}
	Comp->SetupAttachment(GetRootComponent());
	Comp->SetRelativeLocation(Location);
	Comp->SetRelativeScale3D(Scale); // engine sphere is 100 uu in diameter
	Comp->SetMobility(EComponentMobility::Static);
	return Comp;
}

UStaticMeshComponent* AMosquitoWorldBlockout::MakePlane(const FName& Name, const FVector& Scale, const FVector& Location)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	// See MakeBox(): CreateDefaultSubobject, no manual RegisterComponent.
	UStaticMeshComponent* Comp = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	if (MeshFinder.Succeeded())
	{
		Comp->SetStaticMesh(MeshFinder.Object);
	}
	Comp->SetupAttachment(GetRootComponent());
	Comp->SetRelativeLocation(Location);
	Comp->SetRelativeScale3D(Scale); // engine plane is 100 x 100 uu
	Comp->SetMobility(EComponentMobility::Static);
	return Comp;
}

void AMosquitoWorldBlockout::ApplyBlockoutColors()
{
	// GetComponents() instead of walking AttachChildren: robust regardless of
	// attachment timing (AttachChildren can still be empty at BeginPlay).
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents<UStaticMeshComponent>(Meshes);
	if (Meshes.Num() == 0)
	{
		return;
	}

	// The engine BasicShapeMaterial exposes a BaseColor VectorParameter.
	const FLinearColor Grass  (0.25f, 0.55f, 0.22f);
	const FLinearColor Wall   (0.78f, 0.65f, 0.46f);
	const FLinearColor Wood   (0.50f, 0.32f, 0.16f);
	const FLinearColor Metal  (0.58f, 0.63f, 0.70f);
	const FLinearColor Bark   (0.40f, 0.27f, 0.15f);
	const FLinearColor Foliage(0.18f, 0.52f, 0.20f);

	int32 Painted = 0;
	for (UStaticMeshComponent* Mesh : Meshes)
	{

		const FName N = Mesh->GetFName();
		FLinearColor Color;
		if      (N == FName(TEXT("Ground")))                              Color = Grass;
		else if (N == FName(TEXT("House")))                               Color = Wall;
		else if (N == FName(TEXT("TableTop")) || N == FName(TEXT("TableLeg"))) Color = Wood;
		else if (N == FName(TEXT("WaterBucket")) || N == FName(TEXT("WaterBarrel"))) Color = Metal;
		else if (N.ToString().StartsWith(TEXT("TreeTrunk")))              Color = Bark;
		else if (N.ToString().StartsWith(TEXT("TreeCrown")))              Color = Foliage;
		else                                                              continue;

		MosquitoPaint::PaintMesh(Mesh, Color);
		++Painted;
	}

	UE_LOG(LogTemp, Log, TEXT("[MosquitoBlockout] Colors painted on %d components"), Painted);
}
