using UnrealBuildTool;

public class OSFDemoTarget : TargetRules
{
	public OSFDemoTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("OSFDemo");
	}
}
