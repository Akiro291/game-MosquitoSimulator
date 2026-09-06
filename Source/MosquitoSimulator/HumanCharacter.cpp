// Copyright your name. All Rights Reserved.

#include "HumanCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MosquitoCharacter.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const TCHAR* StateToString(EHumanState State)
	{
		switch (State)
		{
			case EHumanState::Calm:      return TEXT("Calm");
			case EHumanState::Noticed:   return TEXT("Noticed");
			case EHumanState::Irritated: return TEXT("Irritated");
			case EHumanState::Angry:     return TEXT("Angry");
			case EHumanState::Chase:     return TEXT("CHASE");
		}
		return TEXT("Unknown");
	}
}

AHumanCharacter::AHumanCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (Capsule)
	{
		Capsule->InitCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.f, TurnSpeed, 0.f);
		Movement->MaxWalkSpeed = WalkSpeedCalm;
	}

	// --- Placeholder visuals: cylinder body + sphere head + a swinging box arm ---

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderFinder.Succeeded())
	{
		BodyMesh->SetStaticMesh(CylinderFinder.Object);
	}
	BodyMesh->SetupAttachment(GetCapsuleComponent());
	BodyMesh->SetRelativeLocation(FVector::ZeroVector);
	BodyMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.7f)); // 50 cm wide, 170 cm tall
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereFinder.Succeeded())
	{
		HeadMesh->SetStaticMesh(SphereFinder.Object);
	}
	HeadMesh->SetupAttachment(GetCapsuleComponent());
	HeadMesh->SetRelativeLocation(FVector(0.f, 0.f, HeadHeight)); // head sits on top of the body
	HeadMesh->SetRelativeScale3D(FVector(0.22f, 0.22f, 0.22f));   // 22 cm head
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// The arm hangs from a pivot at the shoulder so it can swing when swatting.
	ArmPivot = CreateDefaultSubobject<USceneComponent>(TEXT("ArmPivot"));
	ArmPivot->SetupAttachment(GetCapsuleComponent());
	ArmPivot->SetRelativeLocation(FVector(0.f, -28.f, 35.f));

	ArmMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArmMesh"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		ArmMesh->SetStaticMesh(CubeFinder.Object);
	}
	ArmMesh->SetupAttachment(ArmPivot);
	ArmMesh->SetRelativeLocation(FVector(0.f, 0.f, -22.f));
	ArmMesh->SetRelativeScale3D(FVector(0.06f, 0.06f, 0.5f)); // 6 x 6 x 50 cm arm
	ArmMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AHumanCharacter::BeginPlay()
{
	Super::BeginPlay();

	HomeLocation = GetActorLocation();
	RefreshStateFromIrritation();

	UE_LOG(LogTemp, Log, TEXT("[Human] Spawned at (%.0f, %.0f, %.0f) state=%s detection=%.0f cm"),
		HomeLocation.X, HomeLocation.Y, HomeLocation.Z,
		StateToString(CurrentState), DetectionRadius);
}

void AHumanCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdatePerception(DeltaTime);
	UpdateStateMachine(DeltaTime);
	UpdateArmAnimation(DeltaTime);
}

AMosquitoCharacter* AHumanCharacter::GetMosquito() const
{
	if (CachedMosquito.IsValid())
	{
		return CachedMosquito.Get();
	}
	CachedMosquito = Cast<AMosquitoCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	return CachedMosquito.Get();
}

void AHumanCharacter::UpdatePerception(float DeltaTime)
{
	const AMosquitoCharacter* Mosquito = GetMosquito();
	bMosquitoDetected = false;

	if (Mosquito)
	{
		DistanceToMosquito = FVector::Dist(GetActorLocation(), Mosquito->GetActorLocation());

		// Humans mostly HEAR the buzz: a louder mosquito is noticed from farther away.
		const float EffectiveRadius = DetectionRadius * (0.6f + 0.8f * FMath::Clamp(Mosquito->GetNoiseLevel(), 0.f, 1.f));
		if (DistanceToMosquito <= EffectiveRadius || DistanceToMosquito <= CloseProximityRadius)
		{
			bMosquitoDetected = true;
			LastKnownMosquitoPos = Mosquito->GetActorLocation();
			bHasLastKnown = true;
			DecayTimer = 0.f;
		}
	}

	// Buzzing right next to the human escalates irritation over time.
	if (bMosquitoDetected && IrritationLevel < 4 && DistanceToMosquito <= CloseProximityRadius)
	{
		ProximityIrritationTimer += DeltaTime;
		if (ProximityIrritationTimer >= IrritationProximitySeconds)
		{
			ProximityIrritationTimer = 0.f;
			SetIrritationLevel(IrritationLevel + 1);
			UE_LOG(LogTemp, Log, TEXT("[Human] Mosquito buzzes too close -> irritation %d"), IrritationLevel);
		}
	}
	else
	{
		ProximityIrritationTimer = 0.f;
	}

	// Calm down slowly when the mosquito stays away (formal chase scoring: Prompt 10).
	if (!bMosquitoDetected && IrritationLevel > 0)
	{
		DecayTimer += DeltaTime;
		if (DecayTimer >= IrritationDecaySeconds)
		{
			DecayTimer = 0.f;
			SetIrritationLevel(IrritationLevel - 1);
			UE_LOG(LogTemp, Log, TEXT("[Human] The mosquito is gone -> irritation %d"), IrritationLevel);
		}
	}
}

void AHumanCharacter::UpdateStateMachine(float DeltaTime)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	switch (CurrentState)
	{
	case EHumanState::Calm:
	{
		// Stand around or wander slowly near home.
		Movement->MaxWalkSpeed = WalkSpeedCalm;
		UpdateWander(DeltaTime);
		break;
	}
	case EHumanState::Noticed:
	{
		// Turn towards the suspicious buzz, stay in place.
		Movement->MaxWalkSpeed = WalkSpeedCalm;
		StopMovementAndClearWander();
		if (bHasLastKnown)
		{
			FaceTowards(LastKnownMosquitoPos, DeltaTime);
		}
		break;
	}
	case EHumanState::Irritated:
	{
		// Scratch / wave the arm, swat when the mosquito is close.
		Movement->MaxWalkSpeed = WalkSpeedCalm;
		StopMovementAndClearWander();
		if (bHasLastKnown)
		{
			FaceTowards(LastKnownMosquitoPos, DeltaTime);
		}
		if (bMosquitoDetected && DistanceToMosquito <= AttackTriggerRange)
		{
			PerformAttack();
		}
		break;
	}
	case EHumanState::Angry:
	{
		// Go to the last known position and look around there.
		Movement->MaxWalkSpeed = WalkSpeedAngry;
		if (bHasLastKnown)
		{
			FVector ToTarget = LastKnownMosquitoPos - GetActorLocation();
			ToTarget.Z = 0.f;
			if (ToTarget.Size() > 60.f)
			{
				AddMovementInput(ToTarget.GetSafeNormal());
			}
			else
			{
				SetActorRotation(GetActorRotation() + FRotator(0.f, SearchTurnSpeed * DeltaTime, 0.f));
			}
		}
		if (bMosquitoDetected && DistanceToMosquito <= AttackTriggerRange)
		{
			PerformAttack();
		}
		break;
	}
	case EHumanState::Chase:
	{
		// CHASE MODE: sprint at the mosquito and swat on cooldown.
		Movement->MaxWalkSpeed = ChaseSpeed;
		if (bHasLastKnown)
		{
			FVector ToTarget = LastKnownMosquitoPos - GetActorLocation();
			ToTarget.Z = 0.f;
			if (ToTarget.Size() > 40.f)
			{
				AddMovementInput(ToTarget.GetSafeNormal());
			}
			else if (!bMosquitoDetected)
			{
				SetActorRotation(GetActorRotation() + FRotator(0.f, SearchTurnSpeed * DeltaTime, 0.f));
			}
		}
		if (DistanceToMosquito <= AttackTriggerRange)
		{
			PerformAttack();
		}

		// Give up when the mosquito stays far away for too long -> back to Angry.
		if (DistanceToMosquito > LoseChaseDistance)
		{
			ChaseLostTimer += DeltaTime;
			if (ChaseLostTimer >= ChaseGiveUpTime)
			{
				ChaseLostTimer = 0.f;
				SetIrritationLevel(3);
			}
		}
		else
		{
			ChaseLostTimer = 0.f;
		}
		break;
	}
	}
}

void AHumanCharacter::UpdateWander(float DeltaTime)
{
	if (bHasWanderTarget)
	{
		FVector ToTarget = WanderTarget - GetActorLocation();
		ToTarget.Z = 0.f;
		if (ToTarget.Size() > 30.f)
		{
			AddMovementInput(ToTarget.GetSafeNormal());
		}
		else
		{
			bHasWanderTarget = false;
			WanderTimer = FMath::FRandRange(2.f, 5.f); // pause before the next stroll
		}
	}
	else
	{
		WanderTimer -= DeltaTime;
		if (WanderTimer <= 0.f)
		{
			const float Angle = FMath::FRandRange(0.f, 2.f * PI);
			const float Dist = FMath::FRandRange(0.3f, 1.f) * WanderRadius;
			WanderTarget = HomeLocation + FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.f);
			bHasWanderTarget = true;
		}
	}
}

void AHumanCharacter::FaceTowards(const FVector& Target, float DeltaTime)
{
	FVector ToTarget = Target - GetActorLocation();
	ToTarget.Z = 0.f;
	if (ToTarget.SizeSquared() < 1.f)
	{
		return;
	}
	const FRotator TargetRot(0.f, ToTarget.Rotation().Yaw, 0.f);
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, 3.f));
}

void AHumanCharacter::StopMovementAndClearWander()
{
	bHasWanderTarget = false;
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}
}

void AHumanCharacter::UpdateArmAnimation(float DeltaTime)
{
	if (!ArmPivot)
	{
		return;
	}

	float Pitch = 0.f;
	if (bArmSwinging)
	{
		ArmSwingTimer += DeltaTime;
		const float Phase = FMath::Clamp(ArmSwingTimer / ArmSwingDuration, 0.f, 1.f);
		Pitch = -FMath::Sin(Phase * PI) * 130.f; // fast clap arc
		if (Phase >= 1.f)
		{
			bArmSwinging = false;
		}
	}
	else if (CurrentState == EHumanState::Irritated)
	{
		// Nervous scratching while irritated.
		Pitch = FMath::Sin(GetWorld()->GetTimeSeconds() * 9.f) * 18.f;
	}

	ArmPivot->SetRelativeRotation(FRotator(Pitch, 0.f, 0.f));
}

void AHumanCharacter::PerformAttack()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	if (Now < NextAttackTime)
	{
		return;
	}
	NextAttackTime = Now + AttackCooldown;

	// Visual: swing the arm.
	bArmSwinging = true;
	ArmSwingTimer = 0.f;

	AMosquitoCharacter* Mosquito = GetMosquito();
	if (!Mosquito)
	{
		return;
	}

	const float Dist = FVector::Dist(GetActorLocation(), Mosquito->GetActorLocation());
	if (Dist <= AttackRange)
	{
		// The clap sends an air wave away from the human (GDD: "хлопок создаёт воздушную волну").
		FVector PushDir = Mosquito->GetActorLocation() - GetActorLocation();
		PushDir.Z = 0.f;
		PushDir = PushDir.GetSafeNormal();
		PushDir.Z = 0.6f;
		PushDir = PushDir.GetSafeNormal();
		const float Strength = SwatPushStrength * (1.f - 0.5f * Dist / AttackRange);
		Mosquito->ApplySwatHit(SwatDamage, PushDir * Strength);
		UE_LOG(LogTemp, Log, TEXT("[Human] SWAT HIT (dist=%.0f cm, damage=%.0f)"), Dist, SwatDamage);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[Human] SWAT miss (dist=%.0f cm)"), Dist);
	}
}

void AHumanCharacter::SetIrritationLevel(int32 NewLevel)
{
	const int32 Clamped = FMath::Clamp(NewLevel, 0, 4);
	if (Clamped == IrritationLevel)
	{
		return;
	}
	IrritationLevel = Clamped;
	RefreshStateFromIrritation();
}

void AHumanCharacter::RefreshStateFromIrritation()
{
	const EHumanState NewState = static_cast<EHumanState>(FMath::Clamp(IrritationLevel, 0, 4));
	if (NewState == CurrentState)
	{
		return;
	}
	CurrentState = NewState;
	UE_LOG(LogTemp, Log, TEXT("[Human] State -> %s (irritation %d)"), StateToString(CurrentState), IrritationLevel);
}

void AHumanCharacter::OnBitten(float BloodAmount)
{
	// Prompt 9 hook: each bite escalates the human. BloodAmount is kept for
	// future human ecology (fatigue, blood taste, ...).
	SetIrritationLevel(IrritationLevel + 1);
	UE_LOG(LogTemp, Log, TEXT("[Human] Bitten (%.0f blood) -> irritation %d"), BloodAmount, IrritationLevel);
}