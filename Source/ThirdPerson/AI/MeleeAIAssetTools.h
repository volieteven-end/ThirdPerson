#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MeleeAIAssetTools.generated.h"

/** 编辑器专用近战 AI 资源工具；会创建或更新黑板、行为树和测试资源，运行时敌人只读取生成结果。 */
UCLASS()
class THIRDPERSON_API UMeleeAIAssetTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "AI|Editor")
	static bool ConfigureMeleeBehaviorTree();
};
