// Copyright your name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MosquitoProgressionTypes.generated.h"

/**
 * MVP 0.2 §4: two biological progression layers. Generic RPG stats are banned by
 * the owner decision - branches are mosquito biology only, costs are flat and
 * defined here (one source of truth for HUD + purchase logic).
 */

/** Layer A - current mosquito (run): Tab panel keys 1-4. Resets on death. */
UENUM(BlueprintType)
enum class ERunBranch : uint8
{
	WingControl,          // +15% MaxAcceleration per level (flight handling)
	MuscularPropulsion,   // +8% FlightSpeed multiplier per level
	WebEscapeReflexes,    // +20% EscapeMeter damage per struggle tap
	Metabolism            // -15% HungerRate per level
};

/** Layer B - species/genetics (persistent): purchasable only on the death screen. */
UENUM(BlueprintType)
enum class ESpeciesBranch : uint8
{
	BloodEfficiency,      // +10% BloodGainPerBite per level
	WebResistantAdhesion, // slower passive EscapeMeter recovery (0.85x per level)
	Exoskeleton           // +10 MaxHealth per level
};

namespace MosquitoProgression
{
	// One level-up point per upgrade (plan §4); both layers cap at 3 levels.
	constexpr int32 MaxRunBranchLevel = 3;
	constexpr int32 MaxSpeciesBranchLevel = 3;
	constexpr int32 RunUpgradeCostPoints = 1;

	constexpr float WingControlAccelPerLevel = 1.15f;
	constexpr float PropulsionSpeedPerLevel = 1.08f;
	constexpr float WebEscapePerTapPerLevel = 1.20f;
	constexpr float MetabolismHungerPerLevel = 0.85f;

	constexpr float BloodEfficiencyGainPerLevel = 1.10f;
	constexpr float WebResistantAdhesionRecoverPerLevel = 0.85f;
	constexpr float ExoskeletonHpPerLevel = 10.f;

	inline FString GetRunBranchName(ERunBranch Branch)
	{
		switch (Branch)
		{
		case ERunBranch::WingControl: return TEXT("Wing Control");
		case ERunBranch::MuscularPropulsion: return TEXT("Muscular Propulsion");
		case ERunBranch::WebEscapeReflexes: return TEXT("Web Escape Reflexes");
		case ERunBranch::Metabolism: return TEXT("Metabolism");
		default: return TEXT("?");
		}
	}

	inline FString GetRunBranchEffect(ERunBranch Branch)
	{
		switch (Branch)
		{
		case ERunBranch::WingControl: return TEXT("+15% accel");
		case ERunBranch::MuscularPropulsion: return TEXT("+8% speed");
		case ERunBranch::WebEscapeReflexes: return TEXT("+20% escape");
		case ERunBranch::Metabolism: return TEXT("-15% hunger");
		default: return TEXT("");
		}
	}

	inline FString GetSpeciesBranchName(ESpeciesBranch Branch)
	{
		switch (Branch)
		{
		case ESpeciesBranch::BloodEfficiency: return TEXT("Blood Efficiency");
		case ESpeciesBranch::WebResistantAdhesion: return TEXT("Web-Resistant Adhesion");
		case ESpeciesBranch::Exoskeleton: return TEXT("Exoskeleton");
		default: return TEXT("?");
		}
	}
}
