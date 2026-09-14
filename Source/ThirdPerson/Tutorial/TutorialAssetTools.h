#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TutorialAssetTools.generated.h"

/** 编辑器专用教程资源工具；会保存课程、训练场地图及关联配置，日常检查不应执行重建入口。 */
UCLASS()
class THIRDPERSON_API UTutorialAssetTools : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Tutorial|Editor") static bool BuildForestTutorial();
    UFUNCTION(BlueprintCallable,Category="Tutorial|Editor") static bool ValidateForestTutorial();
};
