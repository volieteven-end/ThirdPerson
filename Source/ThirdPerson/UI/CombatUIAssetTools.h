#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatUIAssetTools.generated.h"
/** 编辑器专用 UI 工具；会保存主菜单地图、Widget 蓝图及背包布局，运行时界面不调用这些生成入口。 */
UCLASS()
class THIRDPERSON_API UCombatUIAssetTools : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="UI|Editor") static bool InspectUI();
    UFUNCTION(BlueprintCallable, Category="UI|Editor") static bool BuildUIUpgrade();
};
