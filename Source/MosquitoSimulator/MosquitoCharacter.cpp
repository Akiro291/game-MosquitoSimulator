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
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->DefaultLandMovementMode = MOVE_Flying;
		Movement->DefaultWaterMovementMode = MOVE_Flying;
		Movement->SetMovementMode(MOVE_Flying);
		Movement->GravityScale = 0.f;
		Movement->BrakingDecelerationFlying = 400.f;
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
		UE_LOG(LogTemp, Log, TEXT("[Mosquito] Bite pressed: nearest human %.0f cm away (need <= %.0f)"),
			NearestHumanDistance, LandDistance);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Mosquito] Bite pressed in mid-air: no humans around"));
}

void AMosquitoCharacter::OnSensePressed()
{
	UE_LOG(LogTemp, Log, TEXT("[Mosquito] Mosquito Sense pressed (stub)"));
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

void AMosquitoCharacter::ApplySwatHit(float Damage, const FVector& PushImpulse)
{
	TakeOff(); // a swat always knocks the mosquito off its perch
	SetWingCondition(WingCondition - Damage);
	SetHealth(CurrentHealth - Damage * 0.5f);
	LaunchCharacter(PushImpulse, true, true);

	// Prompt 10: red damage flash on the HUD.
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (AMosquitoHUD* HUD = Cast<AMosquitoHUD>(PC->GetHUD()))
		{
			HUD->FlashDamage();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Mosquito] Swatted! Damage=%.1f (push %.0f)"), Damage, PushImpulse.Size());
}

// --- Prompt 9: bite & blood -------------------------------------------------

void AMosquitoCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsLanded)
	{
		StickToLandedHuman();
	}

	UpdateStats(DeltaTime);
	DetectNearbyHumans();

	if (bBiting)
	{
		BiteProgress += DeltaTime;
		if (BiteProgress >= BiteDuration)
		{
			CompleteBite();
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
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		const float SpeedMult = (CurrentEnergy <= 1.f) ? ExhaustedSpeedMult : 1.f;
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

	const float Room = FMath::Max(0.f, MaxBlood - CurrentBlood);
	const float Gained = FMath::Min(BloodGainPerBite, Room);
	CurrentBlood = FMath::Clamp(CurrentBlood + Gained, 0.f, MaxBlood);

	if (AHumanCharacter* Human = LandedOnHuman.Get())
	{
		// Escalates irritation and flips the human's state machine (Prompt 7).
		Human->OnBitten(Gained);
	}

	UE_LOG(LogTemp, Log, TEXT("[Mosquito] Bite #%d (+%.0f blood, total %.0f/%.0f)"),
		BiteCount, Gained, CurrentBlood, MaxBlood);
}
