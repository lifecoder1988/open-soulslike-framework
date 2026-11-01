using UnrealBuildTool;

public class OpenSoulslikeFramework : ModuleRules
{
	public OpenSoulslikeFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "GameplayTags", "Json", "AIModule", "NavigationSystem", "DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate", "SlateCore", "AssetRegistry"
		});
	}
}
