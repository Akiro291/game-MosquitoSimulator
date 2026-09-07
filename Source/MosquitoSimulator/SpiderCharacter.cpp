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
	BaseBodyScale = BodyMesh ? BodyMesh->GetRelativeScale3D() : FVector::ZeroVector;
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
	}

	switch (CurrentState)
	{
	case ESpiderState::Idle:          TickIdle(DeltaTime, Mosquito); break;
	case ESpiderState::Approach:      TickApproach(DeltaTime, Mosquito); break;
	case ESpiderState::Windup:        TickWindup(DeltaTime, Mosquito); break;
	case ESpiderState::StrikeCooldown:TickStrikeCooldown(DeltaTime, Mosquito); break;
	case ESpiderState::ReturnHome:    TickReturnHome(DeltaTime); break;
	}
}

// --- FSM (plan §1: Idle -> 3 smooth steps to prey -> windup -> ApplySwatHit -> cooldown) ---

void ASpiderCharacter::EnterState(ESpiderState NewState)
{
	CurrentState = NewState;
	StateTimer = 0.f;

	switch (NewState)
	{
	case ESpiderState::Approach:
		StepIndex = 0;
		break;
	case ESpiderState::ReturnHome:
		StepStart = GetActorLocation();
		[[fallthrough]];
	case ESpiderState::Idle:
	case ESpiderState::StrikeCooldown:
		if (BodyMesh && !BaseBodyScale.IsNearlyZero())
		{
			BodyMesh->SetRelativeScale3D(BaseBodyScale);
		}
		break;
	default:
		break;
	}
}

void ASpiderCharacter::BeginStep(const FVector& Target)
{
	StepStart = GetActorLocation();
	FVector Finish = Target;
	// The web is a leash: steps can never leave WebRadius (plan §1).
	const FVector Offset(Finish.X - WebCenter.X, Finish.Y - WebCenter.Y, 0.f);
	if (Offset.Size() > (WebRadius - 10.f) && !Offset.IsNearlyZero())
	{
		Finish = WebCenter + Offset.GetSafeNormal2D() * (WebRadius - 10.f);
	}
	StepFinish = Finish;
	StateTimer = 0.f;
}

void ASpiderCharacter::StepTowardPrey(AMosquitoCharacter* Mosquito)
{
	const FVector Here = GetActorLocation();
	const FVector MosPos = Mosquito->GetActorLocation();
	// Halfway per step - the final step lands right at the prey (plan: 2-3 steps).
	const FVector Target = (StepIndex >= ApproachSteps - 1) ? MosPos : FMath::Lerp(Here, MosPos, 0.5f);
	BeginStep(Target);
}

void ASpiderCharacter::TickIdle(float /*DeltaTime*/, AMosquitoCharacter* Mosquito)
{
	// Natural trap: gameplay query only (like NearestHumanDistance). Humans never
	// query the web - they simply walk through it (plan §1).
	if (!Mosquito->IsDead() && !Mosquito->IsTrapped() &&
		FVector::Dist(Mosquito->GetActorLocation(), WebCenter) <= WebRadius)
	{
		Mosquito->EnterWeb();
	}

	if (Mosquito->IsTrapped())
	{
		EnterState(ESpiderState::Approach);
		StepTowardPrey(Mosquito);
		UE_LOG(LogTemp, Log, TEXT("[Spider] Prey caught - approaching"));
	}
}

void ASpiderCharacter::TickApproach(float DeltaTime, AMosquitoCharacter* Mosquito)
{
	if (Mosquito->IsDead() || !Mosquito->IsTrapped())
	{
		EnterState(ESpiderState::ReturnHome);
		return;
	}

	StateTimer += DeltaTime;
	if (StateTimer <= StepDuration)
	{
		const float Alpha = FMath::Clamp(StateTimer / StepDuration, 0.f, 1.f);
		SetActorLocation(FMath::Lerp(StepStart, StepFinish, FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f)),
			false, nullptr, ETeleportType::None);
		return;
	}

	if (StateTimer >= StepDuration + StepPause)
	{
		++StepIndex;
		if (StepIndex >= ApproachSteps)
		{
			EnterState(ESpiderState::Windup);
		}
		else
		{
			StepTowardPrey(Mosquito);
		}
	}
}

void ASpiderCharacter::TickWindup(float DeltaTime, AMosquitoCharacter* Mosquito)
{
	StateTimer += DeltaTime;

	// "Visually crouch/tilt the mesh" windup tell (plan §1) - the hit telegraphs.
	const float Squat = 1.f - 0.4f * FMath::Clamp(StateTimer / WindupDuration, 0.f, 1.f);
	if (BodyMesh && !BaseBodyScale.IsNearlyZero())
	{
		BodyMesh->SetRelativeScale3D(FVector(BaseBodyScale.X, BaseBodyScale.Y, BaseBodyScale.Z * Squat));
	}

	if (StateTimer < WindupDuration)
	{
		return;
	}

	if (Mosquito->IsDead() || !Mosquito->IsTrapped())
	{
		EnterState(ESpiderState::ReturnHome);
		return;
	}

	// Damage through the EXISTING swat path: wings + half on health + camera kick +
	// red flash, no impulse (plan: identical feel to a human clap).
	Mosquito->ApplySwatHit(BiteDamage, FVector::ZeroVector);
	UE_LOG(LogTemp, Warning, TEXT("[Spider] Bite! damage=%.0f (prey still trapped - cooldown %.1f s)"),
		BiteDamage, StrikeCooldownDuration);
	EnterState(ESpiderState::StrikeCooldown);
}

void ASpiderCharacter::TickStrikeCooldown(float DeltaTime, AMosquitoCharacter* Mosquito)
{
	StateTimer += DeltaTime;
	if (StateTimer < StrikeCooldownDuration)
	{
		return;
	}

	if (Mosquito->IsTrapped() && !Mosquito->IsDead())
	{
		EnterState(ESpiderState::Approach);
		StepTowardPrey(Mosquito);
	}
	else
	{
		EnterState(ESpiderState::ReturnHome);
	}
}

void ASpiderCharacter::TickReturnHome(float DeltaTime)
{
	StateTimer += DeltaTime;
	const FVector Here = GetActorLocation();
	const float Ease = FMath::Clamp(StateTimer / 0.6f, 0.f, 1.f);
	SetActorLocation(FMath::Lerp(StepStart, WebCenter, Ease), false, nullptr, ETeleportType::None);
	if (Ease >= 1.f || StateTimer > 2.f)
	{
		SetActorLocation(WebCenter, false, nullptr, ETeleportType::None);
		StepStart = WebCenter;
		EnterState(ESpiderState::Idle);
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
