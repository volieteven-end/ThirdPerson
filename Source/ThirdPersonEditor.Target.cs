
using UnrealBuildTool;

// 编辑器构建目标，启用资源编辑和开发测试所需的模块条件。
public class ThirdPersonEditorTarget : TargetRules
{
	public ThirdPersonEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;

		ExtraModuleNames.AddRange( new string[] { "ThirdPerson" } );
	}
}
