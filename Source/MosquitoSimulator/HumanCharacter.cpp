// Copyright your name. All Rights Reserved.

#include "HumanCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MosquitoCharacter.h"
#include "Components/AudioComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MosquitoAudio.h"
#include "MosquitoPaint.h"
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
		// NPC has no Controller: without this flag PerformMovement() is skipped
		// entirely (see UCharacterMovementComponent::PerformMovement gate).
		Movement->bRunPhysicsWithNoController = true;
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

	// --- Prompt 14: procedural audio (no assets) ---
	SfxAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("SfxAudio"));
	SfxAudio->SetupAttachment(GetCapsuleComponent());
	SfxAudio->bAutoActivate = false;
}

void AHumanCharacter::BeginPlay()
{
	Super::BeginPlay();

	HomeLocation = GetActorLocation();
	RefreshStateFromIrritation();

	// Prompt 14: procedural clap + readable placeholder colors.
	InitClapSound();
	ApplyVisualColors();

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

	// ROOT-CAUSE FIX: StopMovementAndClearWander() used to call
	// StopMovementImmediately(), which internally does DisableMovement() ->
	// SetMovementMode(MOVE_None). In MOVE_None AddMovementInput() is a silent
	// no-op, so after passing through Noticed/Irritated the human could never
	// move again (diag showed mode=0, vel=0, accel=0 forever). Re-enable
	// walking if the component was left disabled by any legacy code path.
	if (Movement->MovementMode == MOVE_None)
	{
		Movement->SetMovementMode(MOVE_Walking);
		}

	// ---- TEMP DIAGNOSTICS: actual movement, every 0.5 s in ANY state ----
	// (FINAL HOTFIX: downgraded to Verbose so it no longer spams the Output Log;
	//  enable via "log LogTemp VeryVerbose" to inspect.)
	ChaseDiagTimer += DeltaTime;
	if (ChaseDiagTimer >= 0.5f)
	{
		ChaseDiagTimer = 0.f;
		UE_LOG(LogTemp, Verbose,
			TEXT("[Human::DIAG] t=%.2f state=%s loc=(%.1f,%.1f,%.1f) | vel=(%.1f,%.1f,%.1f)|%.1f cm/s | accel=(%.1f,%.1f,%.1f)|%.1f | mode=%d moveGround=%d ctrl=%s"),
			GetWorld()->GetTimeSeconds(),
			StateToString(CurrentState),
			GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z,
			GetVelocity().X, GetVelocity().Y, GetVelocity().Z, GetVelocity().Size(),
			Movement->GetCurrentAcceleration().X, Movement->GetCurrentAcceleration().Y, Movement->GetCurrentAcceleration().Z, Movement->GetCurrentAcceleration().Size(),
			(int32)Movement->MovementMode,
			Movement->IsMovingOnGround() ? 1 : 0,
			GetController() ? TEXT("yes") : TEXT("no"));
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
		Movement->bOrientRotationToMovement = false;  // Manual facing via FaceTowards

		// Prompt 11: tick the chase timer for scoring.
		ChaseTimer += DeltaTime;

		if (bHasLastKnown)
		{
			FVector ToTarget = LastKnownMosquitoPos - GetActorLocation();
			ToTarget.Z = 0.f;
			// Always face the target (even when close) вЂ” makes chase feel more aggressive.
			FaceTowards(LastKnownMosquitoPos, DeltaTime);

			if (ToTarget.Size() > 20.f)
			{
				FVector MoveDir = ToTarget.GetSafeNormal();
				AddMovementInput(MoveDir);
				UE_LOG(LogTemp, Verbose, TEXT("[Human::Chase] *** MOVING *** dir=(%.2f,%.2f), target=(%.0f,%.0f,%.0f)"), MoveDir.X, MoveDir.Y, LastKnownMosquitoPos.X, LastKnownMosquitoPos.Y, LastKnownMosquitoPos.Z);
			}
			else if (!bMosquitoDetected)
			{
				// Search around when reached last known position but mosquito not visible.
				SetActorRotation(GetActorRotation() + FRotator(0.f, SearchTurnSpeed * DeltaTime, 0.f));
			}
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[Human::Chase] CANNOT MOVE: bHasLastKnown=false!"));
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
		// ROOT-CAUSE FIX: StopMovementImmediately() internally calls
		// DisableMovement() which permanently sets MOVE_None вЂ” after that
		// AddMovementInput() never worked again (no movement in Angry/Chase).
		// Kill the speed WITHOUT disabling the movement component instead.
		Movement->Velocity = FVector::ZeroVector;
		Movement->UpdateComponentVelocity();
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

	// Prompt 14: procedural clap - audible even when the swat misses.
	if (SfxAudio && ClapWave && ClapSamples.Num() > 0)
	{
		ClapWave->ResetAudio();
		MosquitoAudio::QueueSamples(ClapWave, ClapSamples);
		SfxAudio->Play();
	}

	AMosquitoCharacter* Mosquito = GetMosquito();
	if (!Mosquito)
	{
		return;
	}

	const float Dist = FVector::Dist(GetActorLocation(), Mosquito->GetActorLocation());
	if (Dist <= AttackRange)
	{
		// The clap sends an air wave away from the human (GDD: "С…Р»РѕРїРѕРє СЃРѕР·РґР°С‘С‚ РІРѕР·РґСѓС€РЅСѓСЋ РІРѕР»РЅСѓ").
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

	const EHumanState OldState = CurrentState;
	CurrentState = NewState;
	UE_LOG(LogTemp, Log, TEXT("[Human] State -> %s (irritation %d)"), StateToString(CurrentState), IrritationLevel);

	// Prompt 11: detect Chase transitions for scoring.
	if (OldState == EHumanState::Chase && NewState != EHumanState::Chase)
	{
		OnChaseEnded();
	}
	else if (NewState == EHumanState::Chase && OldState != EHumanState::Chase)
	{
		OnChaseStarted();
	}
}

void AHumanCharacter::OnChaseStarted()
{
	ChaseTimer = 0.f;
	UE_LOG(LogTemp, Log, TEXT("[Human] Chase started"));
}

void AHumanCharacter::OnChaseEnded()
{
	LastChaseScore = CalculateChaseScore(ChaseTimer);
	UE_LOG(LogTemp, Log, TEXT("[Human] Chase ended after %.1f sec -> +%d points"), ChaseTimer, LastChaseScore);

	// Prompt 11: award points to the mosquito.
	if (AMosquitoCharacter* Mosquito = GetMosquito())
	{
		Mosquito->AddScore(LastChaseScore);
	}
}

int32 AHumanCharacter::CalculateChaseScore(float Duration) const
{
	// Prompt 11: step function based on GDD table.
	if (Duration >= 120.f) return 500;
	if (Duration >= 60.f)  return 150;
	if (Duration >= 30.f)  return 50;
	if (Duration >= 10.f)  return 10;
	return 0;
}

void AHumanCharacter::OnBitten(float BloodAmount)
{
	// Prompt 9 hook: each bite escalates the human. BloodAmount is kept for
	// future human ecology (fatigue, blood taste, ...).
	SetIrritationLevel(IrritationLevel + 1);
	UE_LOG(LogTemp, Log, TEXT("[Human] Bitten (%.0f blood) -> irritation %d"), BloodAmount, IrritationLevel);
}

// --- Prompt 14: procedural audio + readable colors --------------------------

void AHumanCharacter::InitClapSound()
{
	if (!SfxAudio)
	{
		return;
	}

	ClapWave = MosquitoAudio::MakeOneShotWave(this, MosquitoAudio::GenerateClap(ClapSamples));
	SfxAudio->SetSound(ClapWave);

	UE_LOG(LogTemp, Log, TEXT("[Human] Procedural clap ready (%d samples @ %d Hz, no assets)"), ClapSamples.Num(), MosquitoAudio::SampleRate);
}

void AHumanCharacter::ApplyVisualColors()
{
	// Prompt 14: tint the placeholder primitives so people read as people.
	const auto Paint = [](UStaticMeshComponent* MeshComp, const FLinearColor& Color)
	{
		MosquitoPaint::PaintMesh(MeshComp, Color); // QA MVP: correct param name
	};

	Paint(BodyMesh, FLinearColor(0.22f, 0.38f, 0.72f)); // shirt
	Paint(HeadMesh, FLinearColor(0.92f, 0.76f, 0.62f)); // skin
	Paint(ArmMesh,  FLinearColor(0.92f, 0.76f, 0.62f)); // skin
}
