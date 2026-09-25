using UnrealBuildTool;

public class HuntingGameEditorTarget : TargetRules
{
	public HuntingGameEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("HuntingGame");
	}
}
