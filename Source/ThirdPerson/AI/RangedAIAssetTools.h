#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RangedAIAssetTools.generated.h"

/** 编辑器专用远程 AI 资源工具；生成或配置黑板、行为树及测试资产，调用前确认目标资源可被修改。 */
UCLASS()
class THIRDPERSON_API URangedAIAssetTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "AI|Editor")
	static bool ConfigureRangedCombat();

	/** Enemy health/death presentation only; does not rebuild AI, character meshes or maps. */
	UFUNCTION(BlueprintCallable, Category = "AI|Editor")
	static bool ConfigureEnemyPresentation();
};
