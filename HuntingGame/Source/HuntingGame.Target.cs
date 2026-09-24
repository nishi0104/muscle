using UnrealBuildTool;

public class HuntingGameTarget : TargetRules
{
	public HuntingGameTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("HuntingGame");
	}
}
