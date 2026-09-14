#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CountessBossAssetTools.generated.h"
/** 编辑器专用 Countess 资源工具；会生成或升级动作、行为树和测试地图，运行时不重建资产。 */
UCLASS()
class THIRDPERSON_API UCountessBossAssetTools : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="Boss|Editor") static bool BuildCountessAssets();
 UFUNCTION(BlueprintCallable,Category="Boss|Editor") static bool BuildCountessTestMap();
 /** Deliberately updates only this encounter's animation assets; never touches maps/vendor assets. */
 UFUNCTION(BlueprintCallable,Category="Boss|Editor") static bool UpgradeReadableAnimations();
};
