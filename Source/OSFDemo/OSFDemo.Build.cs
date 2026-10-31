using UnrealBuildTool;

public class OSFDemo : ModuleRules
{
	public OSFDemo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "GameplayTags", "OpenSoulslikeFramework"
		});
	}
}
