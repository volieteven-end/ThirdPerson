#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ArenaAssetTools.generated.h"

/** 编辑器专用竞技场布置工具；会保存地图入口、场地边界和导航相关资产，不是运行时刷怪器。 */
UCLASS()
class THIRDPERSON_API UArenaAssetTools : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Additive, tagged map authoring. Never rebuilds existing tutorial/Boss actors or montages. */
    UFUNCTION(BlueprintCallable, Category="Arena|Editor") static bool BuildArenaExpansion();
    UFUNCTION(BlueprintCallable, Category="Arena|Editor") static bool ValidateArenaExpansion();
};
