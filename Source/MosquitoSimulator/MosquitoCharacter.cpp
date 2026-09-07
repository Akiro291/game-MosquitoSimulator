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
#include "SpiderCharacter.h"
#include "MosquitoSimulatorGameInstance.h"

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

		// Collision v5 - ONE-SIDED mosquito-vs-human model:
		//
		// The mosquito's capsule gets the dedicated "MosquitoBody" OBJECT
		// channel and BLOCKS against the Pawn channel. That way the
		// mosquito's own swept movement (CharacterMovementComponent ->
		// SafeMoveUpdatedComponent -> ResolvePenetration) stops and slides
		// on the human's capsule using the standard engine mechanism, and a
		// penetration is resolved by moving ONLY the mosquito.
		//
		// The HUMAN ignores the MosquitoBody channel entirely (set in
		// AHumanCharacter ctor), so the human's movement can never collide
		// with, push, launch or depenetrate itself against the mosquito.
		// Human AI/speed/direction are physically untouched by the mosquito.
		//
		// A small manual depenetration in Tick (ResolvePawnPenetration)
		// covers the case where the human walks over a standing mosquito:
		// the mosquito is pushed out of the human capsule, position-only.
		Capsule->SetCollisionObjectType(ECC_GameTraceChannel1); // MosquitoBody
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
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

	StruggleAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Struggle"));
	StruggleAction->ValueType = EInputActionValueType::Axis1D;

	// MVP 0.2 §4: panel + four run-branch purchase actions.
	UpgradePanelAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_UpgradePanel"));
	BranchActions.Reset();
	static const FName BranchActionNames[4] = {
		TEXT("IA_Branch1"), TEXT("IA_Branch2"), TEXT("IA_Branch3"), TEXT("IA_Branch4")};
	for (int32 i = 0; i < 4; ++i)
	{
		BranchActions.Add(CreateDefaultSubobject<UInputAction>(BranchActionNames[i]));
	}

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

	// MVP 0.2 §1: R = struggle in a spider web.
	FlightContext->MapKey(StruggleAction, EKeys::R);

	// MVP 0.2 §4: Tab opens the upgrade panel; 1-4 buy the branches while it's open.
	FlightContext->MapKey(UpgradePanelAction, EKeys::Tab);
	FlightContext->MapKey(BranchActions[0], EKeys::One);
	FlightContext->MapKey(BranchActions[1], EKeys::Two);
	FlightContext->MapKey(BranchActions[2], EKeys::Three);
	FlightContext->MapKey(BranchActions[3], EKeys::Four);
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

	// MVP 0.2 §4: cache the persistent save + capture the canonical accel BEFORE
	// any upgrade multiplier is ever applied (FlightSpeed itself stays untouched).
	GameSave = UMosquitoSimulatorGameInstance::Get(this);
	if (const UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		BaseMaxAcceleration = Movement->MaxAcceleration;
		if (!GameSave)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Mosquito] GameInstance NOT found - progression disabled"));
		}
	}

	// Dev helper for headless progression proof (-SeedRunXP=250 -> level ups +
	// one purchase through the same BuyRunUpgrade API the Tab panel uses).
	float SeedXP = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("SeedRunXP="), SeedXP) && GameSave)
	{
		GameSave->AddXP(SeedXP);
		GameSave->BuyRunUpgrade(ERunBranch::MuscularPropulsion);
		UE_LOG(LogTemp, Display, TEXT("[Progression] Seeded run XP=%.0f"), SeedXP);
	}

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
		EnhancedInput->BindAction(StruggleAction, ETriggerEvent::Started, this, &AMosquitoCharacter::OnStruggleStarted);
		EnhancedInput->BindAction(StruggleAction, ETriggerEvent::Completed, this, &AMosquitoCharacter::OnStruggleCompleted);
		EnhancedInput->BindAction(UpgradePanelAction, ETriggerEvent::Started, this, &AMosquitoCharacter::OnToggleUpgradePanel);
		EnhancedInput->BindAction(BranchActions[0], ETriggerEvent::Started, this, &AMosquitoCharacter::OnBuyBranchWingControl);
		EnhancedInput->BindAction(BranchActions[1], ETriggerEvent::Started, this, &AMosquitoCharacter::OnBuyBranchPropulsion);
		EnhancedInput->BindAction(BranchActions[2], ETriggerEvent::Started, this, &AMosquitoCharacter::OnBuyBranchWebEscape);
		EnhancedInput->BindAction(BranchActions[3], ETriggerEvent::Started, this, &AMosquitoCharacter::OnBuyBranchMetabolism);
	}
}

void AMosquitoCharacter::MoveForward(const FInputActionValue& Value)
{
	if (bTrappedByWeb)
	{
		return; // MVP 0.2 §1: flight keys do NOT free the mosquito - only struggling does
	}
	const float AxisValue = Value.Get<float>();
	if (Controller && AxisValue != 0.f)
	{
		TakeOff(); // any flight key lifts a landed mosquito
		AddMovementInput(GetActorForwardVector(), AxisValue);
	}
}

void AMosquitoCharacter::MoveRight(const FInputActionValue& Value)
{
	if (bTrappedByWeb)
	{
		return;
	}
	const float AxisValue = Value.Get<float>();
	if (Controller && AxisValue != 0.f)
	{
		TakeOff();
		AddMovementInput(GetActorRightVector(), AxisValue);
	}
}

void AMosquitoCharacter::MoveUp(const FInputActionValue& Value)
{
	if (bTrappedByWeb)
	{
		return;
	}
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
	if (bTrappedByWeb)
	{
		// MVP 0.2 §1: the ONLY bite allowed while stuck is the counter-bite on the
		// spider (counts only in its windup window - the spider decides).
		if (ASpiderCharacter* Spider = NearestSpider.Get(); Spider && NearestSpiderDistance <= LandDistance)
		{
			Spider->TakeBite();
			return;
		}
		UE_LOG(LogTemp, Warning, TEXT("[Mosquito] Still glued to the web - mash R!"));
		return;
	}

	if (bIsLanded)
	{
		StartBite();
		return;
	}

	// Free flight: spider counter-bite wins over human landing if it's the nearer target.
	ASpiderCharacter* Spider = NearestSpider.Get();
	if (Spider && NearestSpiderDistance <= LandDistance && NearestSpiderDistance <= NearestHumanDistance)
	{
		Spider->TakeBite();
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

// --- MVP 0.2 §1: spider web trap & struggle ---------------------------------

void AMosquitoCharacter::EnterWeb()
{
	if (bDead || bTrappedByWeb || HasWebRetakeImmunity())
	{
		return;
	}
	if (bIsLanded)
	{
		TakeOff(); // trapped and landed are mutually exclusive (plan §1)
	}
	bTrappedByWeb = true;
	EscapeMeter = 1.f;
	UE_LOG(LogTemp, Warning, TEXT("[Mosquito] TRAPPED in spider web - mash R to struggle!"));
}

void AMosquitoCharacter::Struggle()
{
	if (!bTrappedByWeb)
	{
		return;
	}
	// MVP 0.2 §4: WebEscapeReflexes multiplies the per-tap damage.
	const float EscapeMult = GameSave ? GameSave->GetWebEscapeMult() : 1.f;
	EscapeMeter = FMath::Max(0.f, EscapeMeter - WebEscapePerTap * EscapeMult);
	if (EscapeMeter <= 0.f)
	{
		EscapeWeb();
	}
}

void AMosquitoCharacter::OnStruggleStarted(const FInputActionValue& Value)
{
	bStruggleHeld = true;
	Struggle();
}

void AMosquitoCharacter::OnStruggleCompleted(const FInputActionValue& Value)
{
	bStruggleHeld = false;
}

void AMosquitoCharacter::EscapeWeb()
{
	if (!bTrappedByWeb)
	{
		return;
	}
	bTrappedByWeb = false;
	bStruggleHeld = false;
	WebRetakeImmunityTimer = WebRetakeImmunitySeconds;
	UE_LOG(LogTemp, Log, TEXT("[Mosquito] ESCAPED the web - fly out fast (%.1f s retake immunity)"),
		WebRetakeImmunitySeconds);
}

// --- MVP 0.2 §4: upgrade panel + run-branch purchases ---------------------------------

void AMosquitoCharacter::OnToggleUpgradePanel(const FInputActionValue& Value)
{
	bUpgradePanelOpen = !bUpgradePanelOpen;
	UE_LOG(LogTemp, Log, TEXT("[Progression] Upgrade panel %s (world keeps running)"),
		bUpgradePanelOpen ? TEXT("OPEN") : TEXT("closed"));
}

static void BuyRunBranch(UMosquitoSimulatorGameInstance* GameSave, bool bPanelOpen, ERunBranch Branch)
{
	if (!bPanelOpen || !GameSave)
	{
		return; // purchase only while the panel is visible
	}
	GameSave->BuyRunUpgrade(Branch);
}

void AMosquitoCharacter::OnBuyBranchWingControl(const FInputActionValue& Value)
{
	BuyRunBranch(GameSave, bUpgradePanelOpen, ERunBranch::WingControl);
}

void AMosquitoCharacter::OnBuyBranchPropulsion(const FInputActionValue& Value)
{
	BuyRunBranch(GameSave, bUpgradePanelOpen, ERunBranch::MuscularPropulsion);
}

void AMosquitoCharacter::OnBuyBranchWebEscape(const FInputActionValue& Value)
{
	BuyRunBranch(GameSave, bUpgradePanelOpen, ERunBranch::WebEscapeReflexes);
}

void AMosquitoCharacter::OnBuyBranchMetabolism(const FInputActionValue& Value)
{
	BuyRunBranch(GameSave, bUpgradePanelOpen, ERunBranch::Metabolism);
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

	// MVP 0.2 §1: web trap. No physics/impulses - velocity is zeroed every
	// tick (same pattern as StickToLandedHuman) and the escape meter runs here.
	if (WebRetakeImmunityTimer > 0.f)
	{
		WebRetakeImmunityTimer = FMath::Max(0.f, WebRetakeImmunityTimer - DeltaTime);
	}
	if (bTrappedByWeb)
	{
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->Velocity = FVector::ZeroVector;
		}
		// Passive recovery vs. mash / gentle hold (owner decision: hold = ~0.3 of a tap).
		// MVP 0.2 §4 layer B: WebResistantAdhesion lowers the passive re-glue rate.
		const float AdhesionMult = GameSave ? GameSave->GetWebResistantAdhesionMult() : 1.f;
		float MeterDelta = WebEscapeRecoverPerSecond * AdhesionMult * DeltaTime;
		if (bStruggleHeld)
		{
			MeterDelta -= WebEscapeHoldPerSecond * DeltaTime;
		}
		EscapeMeter = FMath::Clamp(EscapeMeter + MeterDelta, 0.f, 1.f);
		if (EscapeMeter <= 0.f)
		{
			EscapeWeb();
		}
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
	DetectNearbySpiders();

	// Collision v5: keep the mosquito out of the human capsule (one-way:
	// the human is never touched). The engine's swept move handles the
	// mosquito flying INTO the human; this manual pass handles the human
	// walking over a mosquito.
	ResolvePawnPenetration(DeltaTime);

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
	// MVP 0.2 §4: 1 XP per live flying second (run layer, GameInstance-owned).
	if (GameSave && !bIsLanded)
	{
		GameSave->AddXP(DeltaTime);
	}

	// Hunger burns faster in the air; resting on a host slows it down.
	// MVP 0.2 §4: Metabolism run branch lowers the base hunger rate.
	const float HungerMult = bIsLanded ? 0.5f : 1.f;
	const float MetabolismMult = GameSave ? GameSave->GetMetabolismMult() : 1.f;
	CurrentHunger = FMath::Clamp(CurrentHunger + HungerRatePerSecond * HungerMult * MetabolismMult * DeltaTime, 0.f, MaxHunger);

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
		// MVP 0.2 §4: run upgrades multiply right next to the canonical modifiers -
		// MuscularPropulsion hits the same FlightSpeed term (canonical 150 stays put),
		// WingControl only touches MaxAcceleration (handling, not top speed).
		if (GameSave)
		{
			SpeedMult *= GameSave->GetPropulsionMult();
			Movement->MaxAcceleration = BaseMaxAcceleration * GameSave->GetWingControlMult();
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
	// MVP 0.2 §1: respawn must clear the web trap BEFORE EnableInput (plan §1 edge case).
	bTrappedByWeb = false;
	bStruggleHeld = false;
	EscapeMeter = 1.f;
	WebRetakeImmunityTimer = WebRetakeImmunitySeconds;
	bUpgradePanelOpen = false;

	// MVP 0.2 §4: the new mosquito starts the run at Level 1 (species bonuses apply in §5).
	if (GameSave)
	{
		GameSave->ResetRunProgress();
	}

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

void AMosquitoCharacter::DetectNearbySpiders()
{
	NearestSpider = nullptr;
	NearestSpiderDistance = TNumericLimits<float>::Max();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ASpiderCharacter> It(World); It; ++It)
	{
		const float Dist = FVector::Dist(GetActorLocation(), It->GetActorLocation());
		if (Dist < NearestSpiderDistance)
		{
			NearestSpiderDistance = Dist;
			NearestSpider = *It;
		}
	}
}

void AMosquitoCharacter::ResolvePawnPenetration(float DeltaTime)
{
	// TEMP diagnostics throttle: one [ Mosquito::HumanCollision ] entry max
	// every 0.5 s (never per-frame spam).
	HumanCollisionDiagTimer -= DeltaTime;

	// Collision v5 - one-way coverage for the hole the engine cannot fill:
	// the HUMAN ignores the mosquito's body channel, so when the human WALKS
	// OVER a hovering mosquito nothing pushes the mosquito out on its own.
	// Here the mosquito alone is pushed out of the human capsule along the
	// minimal separation vector (position only - velocity, acceleration and
	// movement mode untouched; no impulse/force/Launch on either side; the
	// human actor is never modified). Skipped while landed so the separate
	// StickToLandedHuman attachment path (Landing/Bite) is never disturbed.
	//
	// NOTE: when the MOSQUITO flies INTO the human, the engine itself blocks
	// the move (the mosquito's capsule blocks ECC_Pawn) - that path needs no
	// manual code and is reported via the BLOCKED diagnostics below.
	if (bIsLanded || bDead)
	{
		return;
	}

	// Cheap pre-filter: max meaningful center distance is ~87 cm (human
	// capsule height + both radii), so 150 cm never misses a penetration.
	if (NearestHumanDistance > PawnPenetrationQueryRadius)
	{
		return;
	}

	const AHumanCharacter* Human = NearestHuman.Get();
	const UCapsuleComponent* HumanCapsule = Human ? Human->GetCapsuleComponent() : nullptr;
	if (!HumanCapsule)
	{
		return;
	}

	const float HumanRadius = HumanCapsule->GetScaledCapsuleRadius();
	const float HumanSegHalf = FMath::Max(0.f, HumanCapsule->GetScaledCapsuleHalfHeight() - HumanRadius);
	const FVector HumanCenter = HumanCapsule->GetComponentLocation();

	const FVector MosPos = GetActorLocation();
	const float MosSegHalf = FMath::Max(0.f, CapsuleHalfHeight - CapsuleRadius);

	// Distance between the two upright capsule AXES (segment-to-segment for
	// parallel vertical axes): horizontal axis distance + vertical gap
	// between the capsule cylinder segments.
	const float VerticalGap = FMath::Max(0.f, FMath::Max(
		(HumanCenter.Z - HumanSegHalf) - (MosPos.Z + MosSegHalf),
		(MosPos.Z - MosSegHalf) - (HumanCenter.Z + HumanSegHalf)));
	const float HorizontalDist = FVector::Dist2D(MosPos, HumanCenter);
	const float AxisDist = FMath::Sqrt(HorizontalDist * HorizontalDist + VerticalGap * VerticalGap);

	const float SumRadii = CapsuleRadius + HumanRadius;
	const float Penetration = SumRadii - AxisDist;

	const FVector Vel = GetVelocity();
	const bool bMovingIntoHuman = (Vel.Size() > 20.f) &&
		(FVector::DotProduct(Vel, (HumanCenter - MosPos).GetSafeNormal()) > 0.3f);

	if (Penetration > 0.f)
	{
		// Minimal separation direction: AWAY from the human axis (v4 bug fix:
		// it pushed TOWARD the center and dragged the mosquito through the
		// body), plus the vertical component when above/below the caps.
		FVector Away(MosPos.X - HumanCenter.X, MosPos.Y - HumanCenter.Y, 0.f);
		if (VerticalGap > 0.f)
		{
			Away.Z = VerticalGap * FMath::Sign(MosPos.Z - HumanCenter.Z);
		}
		if (Away.IsNearlyZero())
		{
			// Dead center inside: push straight up until fully outside.
			Away = FVector(0.f, 0.f,
				(HumanCenter.Z + HumanSegHalf + HumanRadius + CapsuleRadius) - MosPos.Z + 1.f);
		}
		Away = Away.GetSafeNormal();

		// Swept so the correction cannot shove the mosquito inside world
		// geometry; blocked by walls, never by the human (human ignores us,
		// we block the human only during OUR own movement).
		const FVector NewLocation = MosPos + Away * (Penetration + 0.05f);
		SetActorLocation(NewLocation, /*bSweep=*/true, nullptr, ETeleportType::None);

		LogHumanCollision(bMovingIntoHuman ? TEXT("BLOCKED") : TEXT("DEPENETRATED"),
			HumanCenter, Away, Penetration);
		return;
	}

	// No penetration: report BLOCKED while the mosquito presses against the
	// human surface (the engine's swept move holds it there every frame).
	const float SurfaceDist = AxisDist - SumRadii;
	if (bMovingIntoHuman && SurfaceDist < 2.f)
	{
		LogHumanCollision(TEXT("BLOCKED"), HumanCenter,
			(MosPos - HumanCenter).GetSafeNormal(), 0.f);
	}
}

void AMosquitoCharacter::LogHumanCollision(const TCHAR* Type, const FVector& HumanCenter, const FVector& Normal, float Penetration)
{
	if (HumanCollisionDiagTimer > 0.f)
	{
		return;
	}
	HumanCollisionDiagTimer = 0.5f;

	// TEMP DIAGNOSTICS: remove after the owner's PIE verification.
	UE_LOG(LogTemp, Log, TEXT("[ Mosquito::HumanCollision ]\ntype=%s\nmosquitoLoc=%s\nhumanLoc=%s\nnormal=%s\npenetration=%.2f\nvelocity=%s"),
		Type,
		*GetActorLocation().ToString(),
		*HumanCenter.ToString(),
		*Normal.ToString(),
		Penetration,
		*GetVelocity().ToString());
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
	// MVP 0.2 §4 layer B: BloodEfficiency raises the per-bite gain cap multiplier.
	const float EfficiencyMult = GameSave ? GameSave->GetBloodEfficiencyMult() : 1.f;
	const float Gained = FMath::Min(BloodGainPerBite * EfficiencyMult, Room);
	CurrentBlood = FMath::Clamp(CurrentBlood + Gained, 0.f, MaxBlood);

	// MVP 0.2 §4: XP source - the blood actually drunk.
	if (GameSave)
	{
		GameSave->AddXP(Gained);
	}

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

	// MVP 0.2 §4 owner rule: Score CONVERTS to XP and feeds lifetime records
	// (HUD keeps showing this run's TotalScore unchanged).
	if (GameSave)
	{
		GameSave->AddXP(static_cast<float>(Points));
		GameSave->AddLifetimeScore(Points);
	}

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
