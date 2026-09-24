using UnrealBuildTool;

public class HuntingGame : ModuleRules
{
	public HuntingGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// サブフォルダ (Animals/, Player/ ...) からの include をモジュールルート基準で解決する
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"AIModule",
			"NavigationSystem",
			"GameplayTasks",
			"PhysicsCore"
		});
	}
}
