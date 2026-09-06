// Copyright your name. All Rights Reserved.

#include "MosquitoSimulatorPlayerController.h"

#include "MosquitoHUD.h"

AMosquitoSimulatorPlayerController::AMosquitoSimulatorPlayerController()
{
	bEnableMouseLock = true;
	MouseLockThreshold = 10.0f;
}

void AMosquitoSimulatorPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (bEnableMouseLock && GetWorld())
	{
		// Hide cursor and lock to center for camera control
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
	}
}

void AMosquitoSimulatorPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	// Input actions will be bound by the player character
	// This controller handles mouse lock and input mode
}
