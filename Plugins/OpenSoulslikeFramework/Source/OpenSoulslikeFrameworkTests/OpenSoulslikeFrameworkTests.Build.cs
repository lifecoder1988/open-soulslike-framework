using UnrealBuildTool;

public class OpenSoulslikeFrameworkTests : ModuleRules
{
	public OpenSoulslikeFrameworkTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "RenderCore", "GameplayTags", "AIModule", "EnhancedInput", "OpenSoulslikeFramework"
		});
	}
}
