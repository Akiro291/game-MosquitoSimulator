using UnrealBuildTool;
using System.Collections.Generic;

public class MosquitoSimulatorTarget : TargetRules
{
	public MosquitoSimulatorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("MosquitoSimulator");
	}
}
