#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatVFXAssetTools.generated.h"

/** 编辑器专用特效配置工具；会保存项目特效副本及相关配置，Boss 局部调整不重建整套动作或原素材。 */
UCLASS()
class THIRDPERSON_API UCombatVFXAssetTools : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Combat|Editor") static FString InspectSources();
    UFUNCTION(BlueprintCallable, Category="Combat|Editor") static bool BuildCombatVFX();
    UFUNCTION(BlueprintCallable, Category="Combat|Editor") static FString InspectCountessReadability();
    /** Boss-only, idempotent tuning. Does not rebuild montages or touch player/vendor assets. */
    UFUNCTION(BlueprintCallable, Category="Combat|Editor") static bool BuildCountessReadability();
};
