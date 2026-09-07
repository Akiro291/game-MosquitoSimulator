using UnrealBuildTool;

public class MosquitoSimulator : ModuleRules
{
	public MosquitoSimulator(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });
		// Prompt 14: USoundWave subclass vtable pulls IAudioProxyDataFactory
		// (CreateNewProxyData) which lives in the AudioExtensions module.
		PrivateDependencyModuleNames.AddRange(new string[] { "AudioExtensions" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");
	}
}
