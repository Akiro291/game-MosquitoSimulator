// Copyright your name. All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include <GameFramework/PlayerController.h>
#include "MosquitoSimulatorPlayerController.generated.h"

/**
 * PlayerController for Mosquito Simulator.
 */
UCLASS()
class AMosquitoSimulatorPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMosquitoSimulatorPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	bool bEnableMouseLock = true;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	float MouseLockThreshold = 10.0f;
};
