using UnrealBuildTool;

public class OSFDemoEditorTarget : TargetRules
{
	public OSFDemoEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("OSFDemo");
	}
}
