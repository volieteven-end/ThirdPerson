
using UnrealBuildTool;

// 游戏构建目标；沿用同一运行时模块，不包含编辑器资源重建依赖。
public class ThirdPersonTarget : TargetRules
{
	public ThirdPersonTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;

		ExtraModuleNames.AddRange( new string[] { "ThirdPerson" } );
	}
}
