// Copyright your name. All Rights Reserved.

#include "MosquitoCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HumanCharacter.h"
#include "MosquitoHUD.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/AudioComponent.h"
#include "MosquitoAudio.h"

AMosquitoCharacter::AMosquitoCharacter()
{
	PrimaryActorTick.bCanEverTick = true; // Prompt 9: stats + bite progress + landing

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (Capsule)
	{
		Capsule->InitCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
		Capsule->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
						// FINAL HOTFIX MVP 0.1 — Mosquito <-> Human collision (v2, kinematic design):
		//
		// HUMAN does NOT perceive MOSQUITO as an obstacle. Mosquito cannot be
		// inside Human; when a penetration is detected only the Mosquito's
		// position is corrected (no impulse, no Launch, no physics on either
		// side):
		//  - ECC_Pawn is ECR_Overlap  -> Human never receives an impulse/vertical
		//    push from the mosquito (fixes "[Human] подбрасывается вверх").
		//    Human keeps walking normally (Wander/Chase) regardless of the mosquito.
		//  - Overlap events are enabled so the mosquito can run its own sweep,
		//    but the Human's CharacterMovement keeps bRunPhysicsWithNoController
		//    = true and is untouched.
		//  - Positional correction is applied manually in Tick::ResolvePawnPenetration
		//    ONLY while the mosquito is flying (!bIsLanded). Landing/Bite use a
		//    separate attachment path (StickToLandedHuman) that intentionally
		//    keeps the mosquito adjacent to the human surface, so correction is
		//    skipped there to never interfere with Bite/Landing.
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Capsule->SetGenerateOverlapEvents(true);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->DefaultLandMovementMode = MOVE_Flying;
		Movement->DefaultWaterMovementMode = MOVE_Flying;
		Movement->SetMovementMode(MOVE_Flying);
		Movement->GravityScale = 0.f;
		Movement->BrakingDecelerationFlying = 1800.f;
		Movement->MaxFlySpeed = FlightSpeed;
		Movement->MaxAcceleration = 800.f;
		Movement->bOrientRotationToMovement = false;
		Movement->RotationRate = FRotator(0.f, 360.f, 0.f);
	}

	// --- Placeholder body: 2 cm sphere so the player can see the mosquito ---
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereFinder.Succeeded())
	{
		BodyMesh->SetStaticMesh(SphereFinder.Object);
	}
	BodyMesh->SetupAttachment(GetRootComponent());
	BodyMesh->SetRelativeLocation(FVector::ZeroVector);
	BodyMesh->SetRelativeScale3D(FVector(0.02f)); // engine sphere is 100 uu wide -> 2 cm
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetMobility(EComponentMobility::Movable);

	// --- Prompt 14: procedural audio (no assets) ---
	BuzzAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("BuzzAudio"));
	BuzzAudio->SetupAttachment(GetRootComponent());
	BuzzAudio->bAutoActivate = false;

	SfxAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("SfxAudio"));
	SfxAudio->SetupAttachment(GetRootComponent());
	SfxAudio->bAutoActivate = false;

	// --- Third-person camera (Prompt 4) ---
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetRootComponent());
	SpringArm->TargetArmLength = CameraDistance;
	SpringArm->bUsePawnControlRotation = true;
	// The default camera probe would constantly collide with the floor at this
	// scale, so the collision test is disabled until the arm is tuned properly.
	SpringArm->bDoCollisionTest = false;
	SpringArm->bEnableCameraLag = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->FieldOfView = CameraFOV;
	// NOTE: near clipping plane is configured project-wide in DefaultEngine.ini
	// ([/Script/Engine.Engine] NearClipPlane=0.5). UCameraComponent lost its
	// per-camera CustomNearClippingPlane in UE 5.8, and 0.5 cm is required for
	// a world measured in centimeters.

	// --- Enhanced Input: actions + mapping context built in code (Prompt 3) ---
	FlightContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("IMC_MosquitoFlight"));

	MoveForwardAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_MoveForward"));
	MoveForwardAction->ValueType = EInputActionValueType::Axis1D;

	MoveRightAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_MoveRight"));
	MoveRightAction->ValueType = EInputActionValueType::Axis1D;

	MoveUpAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_MoveUp"));
	MoveUpAction->ValueType = EInputActionValueType::Axis1D;

	LookAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Look"));
	LookAction->ValueType = EInputActionValueType::Axis2D;

	BiteAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Bite"));

	SenseAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Sense"));

	// Flight: W/S, A/D, up = Space or E, down = LeftCtrl or Q.
	FlightContext->MapKey(MoveForwardAction, EKeys::W);
	UInputModifierNegate* NegateBack = CreateDefaultSubobject<UInputModifierNegate>(TEXT("Negate_MoveBack"));
	FlightContext->MapKey(MoveForwardAction, EKeys::S).Modifiers.Add(NegateBack);

	FlightContext->MapKey(MoveRightAction, EKeys::D);
	UInputModifierNegate* NegateLeft = CreateDefaultSubobject<UInputModifierNegate>(TEXT("Negate_MoveLeft"));
	FlightContext->MapKey(MoveRightAction, EKeys::A).Modifiers.Add(NegateLeft);

	FlightContext->MapKey(MoveUpAction, EKeys::SpaceBar);
	FlightContext->MapKey(MoveUpAction, EKeys::E);
	UInputModifierNegate* NegateCtrl = CreateDefaultSubobject<UInputModifierNegate>(TEXT("Negate_MoveDownCtrl"));
	FlightContext->MapKey(MoveUpAction, EKeys::LeftControl).Modifiers.Add(NegateCtrl);
	UInputModifierNegate* NegateQ = CreateDefaultSubobject<UInputModifierNegate>(TEXT("Negate_MoveDownQ"));
	FlightContext->MapKey(MoveUpAction, EKeys::Q).Modifiers.Add(NegateQ);

	// Look: mouse (same convention as the official UE5 third-person template).
	FlightContext->MapKey(LookAction, EKeys::Mouse2D);

	// Stubs for later prompts.
	FlightContext->MapKey(BiteAction, EKeys::LeftMouseButton);
	FlightContext->MapKey(SenseAction, EKeys::RightMouseButton);
}

void AMosquitoCharacter::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
	CurrentEnergy = MaxEnergy;
	WingCondition = MaxWingCondition;
	CurrentBlood = FMath::Clamp(CurrentBlood, 0.f, MaxBlood);
	CurrentHunger = FMath::Clamp(CurrentHunger, 0.f, MaxHunger);

	// Dev/test override: "-StatSpeed=100" accelerates hunger/energy for headless runs.
	float StatSpeed = 1.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("StatSpeed="), StatSpeed) && StatSpeed > 0.f)
	{
		HungerRatePerSecond *= StatSpeed;
		EnergyDrainFlyingPerSecond *= StatSpeed;
		UE_LOG(LogTemp, Log, TEXT("[Mosquito] Stat speed x%.0f (dev override)"), StatSpeed);
	}

	bIsFlying = true;

	// Prompt 14: start the procedural buzz + prepare the bite one-shot.
	InitAudio();
}

void AMosquitoCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// Register the flight mapping context as soon as a player takes control.
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(FlightContext, 0);
		}
	}
}

void AMosquitoCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInput->BindAction(MoveForwardAction, ETriggerEvent::Triggered, this, &AMosquitoCharacter::MoveForward);
		EnhancedInput->BindAction(MoveRightAction, ETriggerEvent::Triggered, this, &AMosquitoCharacter::MoveRight);
		EnhancedInput->BindAction(MoveUpAction, ETriggerEvent::Triggered, this, &AMosquitoCharacter::MoveUp);
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMosquitoCharacter::Look);
		EnhancedInput->BindAction(BiteAction, ETriggerEvent::Started, this, &AMosquitoCharacter::OnBitePressed);
		EnhancedInput->BindAction(SenseAction, ETriggerEvent::Started, this, &AMosquitoCharacter::OnSensePressed);
	}
}

void AMosquitoCharacter::MoveForward(const FInputActionValue& Value)
{
	const float AxisValue = Value.Get<float>();
	if (Controller && AxisValue != 0.f)
	{
		TakeOff(); // any flight key lifts a landed mosquito
		AddMovementInput(GetActorForwardVector(), AxisValue);
	}
}

void AMosquitoCharacter::MoveRight(const FInputActionValue& Value)
{
	const float AxisValue = Value.Get<float>();
	if (Controller && AxisValue != 0.f)
	{
		TakeOff();
		AddMovementInput(GetActorRightVector(), AxisValue);
	}
}

void AMosquitoCharacter::MoveUp(const FInputActionValue& Value)
{
	const float AxisValue = Value.Get<float>();
	if (Controller && AxisValue != 0.f)
	{
		TakeOff();
		AddMovementInput(FVector::UpVector, AxisValue);
	}
}

void AMosquitoCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxis = Value.Get<FVector2D>();
	AddControllerYawInput(LookAxis.X);
	AddControllerPitchInput(LookAxis.Y);
}

void AMosquitoCharacter::OnBitePressed()
{
	if (bIsLanded)
	{
		StartBite();
		return;
	}

	if (AHumanCharacter* Human = NearestHuman.Get())
	{
		if (NearestHumanDistance <= LandDistance)
		{
			LandOn(Human);
			StartBite(); // land + first bite in one press
			return;
		}
		// Warning log stands out in Output Log for player feedback.
		UE_LOG(LogTemp, Warning, TEXT("[Mosquito] Too far to bite (%.0f cm, need <= %.0f)"),
			NearestHumanDistance, LandDistance);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Mosquito] No human nearby to bite"));
}

void AMosquitoCharacter::OnSensePressed()
{
	UE_LOG(LogTemp, Warning, TEXT("[Mosquito] Sense is not implemented yet (Prompt 14)"));
}

void AMosquitoCharacter::SetHealth(float NewValue)
{
	CurrentHealth = FMath::Clamp(NewValue, 0.f, MaxHealth);
}

void AMosquitoCharacter::SetBlood(float NewValue)
{
	CurrentBlood = FMath::Clamp(NewValue, 0.f, MaxBlood);
}

void AMosquitoCharacter::SetEnergy(float NewValue)
{
	CurrentEnergy = FMath::Clamp(NewValue, 0.f, MaxEnergy);
}

void AMosquitoCharacter::SetHunger(float NewValue)
{
	CurrentHunger = FMath::Clamp(NewValue, 0.f, MaxHunger);
}

void AMosquitoCharacter::SetWingCondition(float NewValue)
{
	WingCondition = FMath::Clamp(NewValue, 0.f, MaxWingCondition);
}

void AMosquitoCharacter::ApplySwatHit(float Damage, const FVector& /*PushImpulse*/)
{
	// Prompt 14: ignore SWAT during immunity period.
	if (SwatImmunityTimer > 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Mosuito::ApplySwatHit] IMMUNE! Swat ignored (%.1f s left)"), SwatImmunityTimer);
		return;
	}

	// Prompt 14: no physical impulse - just damage and take off.
	TakeOff(); // a swat always knocks the mosquito off its perch
	SetWingCondition(WingCondition - Damage);
	SetHealth(CurrentHealth - Damage * 0.5f);
	// LaunchCharacter removed - flight control stays intact.

	// QA MVP: one-shot camera kick so the swat is FELT, not just seen.
	// Decays in Tick; the per-frame rotation step stays capped at 0.4 deg.
	SwatShakeImpulseDeg = FMath::Min(SwatShakeImpulseDeg + 1.2f, 2.0f);

	// Prompt 10: red damage flash on the HUD.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (AMosquitoHUD* HUD = Cast<AMosquitoHUD>(PC->GetHUD()))
		{
			HUD->FlashDamage();
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[Mosuito::ApplySwatHit] Damage=%.1f (NO impulse, clean damage)"), Damage);
}

// --- Prompt 9: bite & blood -------------------------------------------------

void AMosquitoCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Prompt 14: buzz loudness/pitch follow speed & wing state (runs even
	// while dead so the buzz fades out instead of stopping abruptly).
	UpdateBuzz(DeltaTime);

	// Prompt 12: handle respawn timer while dead.
	if (bDead)
	{
		DeathTimer += DeltaTime;
		if (DeathTimer >= RespawnDelay)
		{
			Respawn();
		}
		return;
	}

	// Prompt 12/13: light camera shake when wings are badly damaged (< 25).
	// ROOT-CAUSE FIX: the old code multiplied the per-frame rotation step by
	// the RAW DeltaTime, so one heavy editor hitch frame (dt up to 0.33 s)
	// injected a huge rotation step (log showed 343.8 deg) — the "world
	// shaking" bug. Now dt is capped at 1/30 s and every per-frame step is
	// absolutely clamped to 0.4 deg, so extreme values physically cannot
	// reach the camera. The effect itself (dying-wings shake) stays.
	// QA MVP: swat kick decays (~0.4 s) and stacks on the dying-wings shake.
	// Same safety caps as before: dt <= 1/30 s, step <= 0.4 deg per frame.
	SwatShakeImpulseDeg = FMath::Max(0.f, SwatShakeImpulseDeg - DeltaTime * 3.f);
	if ((WingCondition < 25.f || SwatShakeImpulseDeg > 0.f) && !bDead)
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			const float Shake = FMath::Clamp((25.f - WingCondition) * 0.02f, 0.f, 0.5f)
				+ SwatShakeImpulseDeg;
			const float SafeDt = FMath::Min(DeltaTime, 1.f / 30.f);
			const float StepDeg = FMath::Min(Shake * 40.f * SafeDt, 0.4f);
			PC->AddPitchInput(FMath::RandRange(-StepDeg, StepDeg));
			PC->AddYawInput(FMath::RandRange(-StepDeg, StepDeg));

			// ---- TEMP DIAGNOSTICS: camera shake source (every ~1 s) ----
			static float CameraShakeDiagTimer = 0.f;
			CameraShakeDiagTimer += DeltaTime;
			if (CameraShakeDiagTimer >= 1.f)
			{
				CameraShakeDiagTimer = 0.f;
				UE_LOG(LogTemp, Warning, TEXT("[Mosquito::CameraShake] ACTIVE wings=%.1f health=%.1f shakeMaxDeg=%.2f (deg per frame, hard cap 0.40)"),
					WingCondition, CurrentHealth, StepDeg);
			}
		}
	}

	if (bIsLanded)
	{
		StickToLandedHuman();
	}

	UpdateStats(DeltaTime);
	DetectNearbyHumans();

	// Collision v2: keep the mosquito out of the human capsule without
	// touching the human's movement in any way (no impulse/launch/Z change).
	ResolvePawnPenetration();

	if (bBiting)
	{
		BiteProgress += DeltaTime;
		if (BiteProgress >= BiteDuration)
		{
			CompleteBite();
		}
	}

	// Prompt 14: no recovery needed - no impulse was applied.
	bSwatRecovering = false;

	// Prompt 13: SWAT immunity countdown.
	if (SwatImmunityTimer > 0.f)
	{
		SwatImmunityTimer -= DeltaTime;
		if (SwatImmunityTimer <= 0.f)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Mosuito::Tick] SWAT immunity expired"));
		}
	}
}

void AMosquitoCharacter::UpdateStats(float DeltaTime)
{
	// Hunger burns faster in the air; resting on a host slows it down.
	const float HungerMult = bIsLanded ? 0.5f : 1.f;
	CurrentHunger = FMath::Clamp(CurrentHunger + HungerRatePerSecond * HungerMult * DeltaTime, 0.f, MaxHunger);

	// Flying drains energy, resting on the host regenerates it (risk/reward).
	if (bIsLanded)
	{
		CurrentEnergy = FMath::Clamp(CurrentEnergy + EnergyRegenLandedPerSecond * DeltaTime, 0.f, MaxEnergy);
	}
	else
	{
		CurrentEnergy = FMath::Clamp(CurrentEnergy - EnergyDrainFlyingPerSecond * DeltaTime, 0.f, MaxEnergy);
	}

	// Starvation: the body burns stored blood.
	if (CurrentHunger > BloodBurnHungerThreshold)
	{
		CurrentBlood = FMath::Clamp(CurrentBlood - BloodBurnPerSecond * DeltaTime, 0.f, MaxBlood);
	}

	// Exhaustion halves the flight speed until energy is restored.
	// Prompt 12: wing damage further reduces flight speed.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		float SpeedMult = 1.f;
		if (CurrentEnergy <= 1.f)
		{
			SpeedMult = ExhaustedSpeedMult;
		}
		if (WingCondition < 25.f)
		{
			SpeedMult *= WingSpeedMult25;
		}
		else if (WingCondition < 50.f)
		{
			SpeedMult *= WingSpeedMult50;
		}
		Movement->MaxFlySpeed = FlightSpeed * SpeedMult;
	}

	if (!bLoggedExhausted && CurrentEnergy <= 1.f)
	{
		bLoggedExhausted = true;
		UE_LOG(LogTemp, Log, TEXT("[Mosquito] Exhausted! Flight speed halved until rest"));
	}
	else if (bLoggedExhausted && CurrentEnergy > 10.f)
	{
		bLoggedExhausted = false;
	}

	if (!bLoggedStarving && CurrentHunger >= BloodBurnHungerThreshold)
	{
		bLoggedStarving = true;
		UE_LOG(LogTemp, Log, TEXT("[Mosquito] Starving - burning stored blood"));
	}
	else if (bLoggedStarving && CurrentHunger < BloodBurnHungerThreshold - 10.f)
	{
		bLoggedStarving = false;
	}

	BiteCooldownTimer = FMath::Max(0.f, BiteCooldownTimer - DeltaTime);

	// Prompt 12: check for death.
	if (!bDead && (CurrentHealth <= 0.f || WingCondition <= 0.f))
	{
		Die();
	}
}

void AMosquitoCharacter::Die()
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	DeathTimer = 0.f;

	// Prompt 12: disable input.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->DisableInput(PC);
	}

	UE_LOG(LogTemp, Log, TEXT("[Mosquito] Died! Health=%.1f Wings=%.1f — respawning in %.1f sec"),
		CurrentHealth, WingCondition, RespawnDelay);
}

void AMosquitoCharacter::Respawn()
{
	// Prompt 12: restore all stats.
	CurrentHealth = MaxHealth;
	CurrentBlood = MaxBlood;
	CurrentEnergy = MaxEnergy;
	CurrentHunger = 0.f;
	WingCondition = MaxWingCondition;
	bLoggedExhausted = false;
	bLoggedStarving = false;

	// Prompt 12: teleport to PlayerStart.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->EnableInput(PC);

		// Find any PlayerStart actor in the world.
		UWorld* World = GetWorld();
		if (World)
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (It->ActorHasTag(FName("PlayerStart")))
				{
					SetActorLocation(It->GetActorLocation());
					SetActorRotation(It->GetActorRotation());
					break;
				}
			}
		}
	}

	// Reset movement.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->MaxFlySpeed = FlightSpeed;
	}

	bDead = false;

	UE_LOG(LogTemp, Log, TEXT("[Mosquito] Respawned!"));
}

void AMosquitoCharacter::DetectNearbyHumans()
{
	NearestHuman = nullptr;
	NearestHumanDistance = TNumericLimits<float>::Max();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AHumanCharacter> It(World); It; ++It)
	{
		const float Dist = FVector::Dist(GetActorLocation(), It->GetActorLocation());
		if (Dist < NearestHumanDistance)
		{
			NearestHumanDistance = Dist;
			NearestHuman = *It;
		}
	}
}

void AMosquitoCharacter::ResolvePawnPenetration()
{
	// Collision v2 (FINAL design):
	//   Human does NOT perceive the mosquito as an obstacle. The human keeps
	//   wandering/chasing with zero reaction - no impulse, no Launch, no Z
	//   change, no movement change. The MOSQUITO cannot stay inside the human:
	//   when penetration is detected only the mosquito's position is corrected
	//   (positional depenetration via a point sweep, velocity untouched, no
	//   physics, no AddImpulse/AddForce/LaunchCharacter).
	// Landing/Bite use the separate StickToLandedHuman attachment path which
	// keeps the mosquito adjacent to (not inside) the host, so correction is
	// skipped while landed to never interfere with landing/biting.
	if (bIsLanded || bDead)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Cheap pre-filter: run the query only when the nearest human is close.
	if (NearestHumanDistance > PawnPenetrationQueryRadius)
	{
		return;
	}

	AHumanCharacter* Human = NearestHuman.Get();
	if (!Human)
	{
		return;
	}

	const UCapsuleComponent* HumanCapsule = Human->GetCapsuleComponent();
	if (!HumanCapsule)
	{
		return;
	}

	// Point sweep of the mosquito capsule shape from the human center outward:
	// if the mosquito capsule overlaps the human capsule, the sweep hit gives
	// the minimal push-out direction and depth. This is a read-only query
	// against the human's capsule only - no physics response on either side.
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(HumanCapsule->GetCollisionObjectType()); // ECC_Pawn

	const FCollisionShape MosquitoShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
	const FVector Start = Human->GetActorLocation();
	const FVector End = GetActorLocation();

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MosquitoPawnPenetration), false);
	Params.AddIgnoredActor(this); // the query shape belongs to the mosquito

	const bool bStartPenetrating = World->SweepSingleByObjectType(
		Hit, Start, End, FQuat::Identity, ObjectParams, MosquitoShape, Params);

	if (!bStartPenetrating || !Hit.bBlockingHit || Hit.bStartPenetrating)
	{
		// No overlap (or the sweep started free) - nothing to fix.
		return;
	}

	// Hit.bStartPenetrating + Hit.PenetrationDepth = minimal depenetration
	// vector; move the mosquito out along it. Position only - velocity,
	// acceleration and movement mode are untouched, so the correction adds
	// no extra speed to the mosquito.
	FVector PushOut = Hit.ImpactNormal * Hit.PenetrationDepth;
	if (PushOut.IsNearlyZero())
	{
		// Degenerate depth: fall back to the direction from the human center.
		FVector Dir = End - Start;
		if (Dir.IsNearlyZero())
		{
			Dir = FVector::UpVector;
		}
		Dir.Z = 0.f; // prefer lateral correction; human capsule is vertical
		if (Dir.IsNearlyZero())
		{
			Dir = FVector::RightVector;
		}
		PushOut = Dir.GetSafeNormal() * (CapsuleRadius + HumanCapsule->GetUnscaledCapsuleRadius() + 1.f);
	}

	const FVector NewLocation = GetActorLocation() + PushOut;
	SetActorLocation(NewLocation, false, nullptr, ETeleportType::None);

	UE_LOG(LogTemp, Verbose, TEXT("[Mosquito::PawnPenetration] pushed out %.2f cm (dist=%.1f)"),
		PushOut.Size(), NearestHumanDistance);
}

void AMosquitoCharacter::StickToLandedHuman()
{
	AHumanCharacter* Human = LandedOnHuman.Get();
	if (!Human)
	{
		TakeOff(); // the host disappeared
		return;
	}

	const FVector NewLoc = Human->GetActorLocation() + LandedOffset;
	SetActorLocation(NewLoc, false);
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->Velocity = FVector::ZeroVector;
	}

	// Face the host while drinking their blood.
	const FVector ToHuman = Human->GetActorLocation() - NewLoc;
	if (ToHuman.SizeSquared() > 1.f)
	{
		SetActorRotation(FRotator(0.f, ToHuman.Rotation().Yaw, 0.f));
	}
}

void AMosquitoCharacter::LandOn(AHumanCharacter* Human)
{
	if (!Human)
	{
		return;
	}

	bIsLanded = true;
	bBiting = false;
	BiteProgress = 0.f;
	LandedOnHuman = Human;
	LandedOffset = GetActorLocation() - Human->GetActorLocation();

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}

	UE_LOG(LogTemp, Log, TEXT("[Mosquito] Landed on Human"));
}

void AMosquitoCharacter::TakeOff()
{
	if (!bIsLanded)
	{
		return;
	}

	bIsLanded = false;
	bBiting = false;
	BiteProgress = 0.f;
	LandedOnHuman = nullptr;

	UE_LOG(LogTemp, Log, TEXT("[Mosquito] Take off"));
}

void AMosquitoCharacter::StartBite()
{
	if (!bIsLanded || bBiting || BiteCooldownTimer > 0.f)
	{
		return;
	}

	bBiting = true;
	BiteProgress = 0.f;
}

void AMosquitoCharacter::CompleteBite()
{
	bBiting = false;
	BiteCooldownTimer = BiteCooldown;
	++BiteCount;
	PlayOneShot(BiteWave, BiteSamples); // Prompt 14: bite sound

	const float Room = FMath::Max(0.f, MaxBlood - CurrentBlood);
	const float Gained = FMath::Min(BloodGainPerBite, Room);
	CurrentBlood = FMath::Clamp(CurrentBlood + Gained, 0.f, MaxBlood);

	if (AHumanCharacter* Human = LandedOnHuman.Get())
	{
		// Escalates irritation and flips the human's state machine (Prompt 7).
		Human->OnBitten(Gained);
	}

	UE_LOG(LogTemp, Warning, TEXT("[Mosuito::CompleteBite] BITE SUCCESS #%d (+%.0f blood, total %.0f/%.0f)"),
		BiteCount, Gained, CurrentBlood, MaxBlood);
}

void AMosquitoCharacter::AddScore(int32 Points)
{
	if (Points <= 0)
	{
		return;
	}
	CurrentScore = Points;
	TotalScore += Points;
	UE_LOG(LogTemp, Log, TEXT("[Mosquito] +%d points (total: %d)"), Points, TotalScore);
}

// --- Prompt 14: procedural audio (no assets) --------------------------------

void AMosquitoCharacter::InitAudio()
{
	BuzzWave = MosquitoAudio::MakeBuzzWave(this);
	BiteWave = MosquitoAudio::MakeOneShotWave(this, MosquitoAudio::GenerateBite(BiteSamples));

	if (BuzzAudio)
	{
		BuzzAudio->SetSound(BuzzWave);
		BuzzAudio->Play();
	}
	if (SfxAudio)
	{
		SfxAudio->SetSound(BiteWave);
	}

	UE_LOG(LogTemp, Log, TEXT("[Mosquito] Procedural audio ready (buzz + bite, %d Hz mono, no assets)"), MosquitoAudio::SampleRate);
}

void AMosquitoCharacter::UpdateBuzz(float DeltaTime)
{
	if (!BuzzWave || !BuzzAudio)
	{
		return;
	}

	const float Speed = GetVelocity().Size();
	const float SpeedFrac = FMath::Clamp(Speed / FMath::Max(1.f, FlightSpeed), 0.f, 1.f);
	const float WingFrac = FMath::Clamp(WingCondition / FMath::Max(1.f, MaxWingCondition), 0.f, 1.f);

	// Landed mosquitoes feed silently; dead ones are silent too.
	const float TargetAmp = (!bDead && !bIsLanded) ? FMath::Lerp(0.08f, 0.30f, SpeedFrac) : 0.f;
	// Damaged wings buzz deeper.
	const float TargetFreq = (200.f + 90.f * SpeedFrac) * (0.65f + 0.35f * WingFrac);

	BuzzAmp = FMath::FInterpTo(BuzzAmp, TargetAmp, DeltaTime, 8.f);
	BuzzFreq = FMath::FInterpTo(BuzzFreq, TargetFreq, DeltaTime, 6.f);

	// Keep the audio FIFO topped up (~0.2 s ahead) from the game thread.
	const int32 BufferedTargetBytes = MosquitoAudio::SampleRate * 2 / 5;
	if (BuzzWave->GetAvailableAudioByteCount() < BufferedTargetBytes)
	{
		TArray<int16> Chunk;
		MosquitoAudio::GenerateBuzzChunk(Chunk, BuzzPhase, BuzzTremPhase, BuzzSampleCount, BuzzFreq, BuzzAmp, 2400); // 50 ms
		MosquitoAudio::QueueSamples(BuzzWave, Chunk);
	}
}

void AMosquitoCharacter::PlayOneShot(USoundWaveProcedural* Wave, const TArray<int16>& Samples)
{
	if (!Wave || !SfxAudio || Samples.Num() == 0)
	{
		return;
	}

	Wave->ResetAudio();
	MosquitoAudio::QueueSamples(Wave, Samples);
	SfxAudio->SetSound(Wave);
	SfxAudio->Play();
}
