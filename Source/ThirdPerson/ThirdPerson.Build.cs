
using UnrealBuildTool;

// 模块依赖配置：运行时仅使用游戏所需模块，资产构建依赖仅在 Editor 目标启用。
public class ThirdPerson : ModuleRules
{
	public ThirdPerson(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AnimGraphRuntime",
			"InputCore",
			"UMG",
			"AIModule",
			"NavigationSystem",
			"MotionWarping",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "EnhancedInput", "Niagara" });
		// 这些依赖服务于资源编辑工具；不要把编辑器模块无条件带入 Game 构建。
		if (Target.bBuildEditor)
		{
            PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "AssetRegistry", "AnimGraph", "BlueprintGraph", "Kismet", "KismetCompiler", "BehaviorTreeEditor", "AIGraph", "UMGEditor", "AnimationBlueprintLibrary", "AudioMixer", "RHI", "SkeletalMeshModifiers" });
		}

	}
}
