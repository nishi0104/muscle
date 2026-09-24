using UnrealBuildTool;

public class HuntingGameEditorTarget : TargetRules
{
	public HuntingGameEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("HuntingGame");
	}
}
