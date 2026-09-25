using UnrealBuildTool;

public class HuntingGameTarget : TargetRules
{
	public HuntingGameTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("HuntingGame");
	}
}
