// Copyright your name. All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include <Modules/ModuleManager.h>

class FMosquitoSimulatorModule : public IModuleInterface
{
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
