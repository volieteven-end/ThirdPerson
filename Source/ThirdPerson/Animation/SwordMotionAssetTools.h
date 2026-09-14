#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SwordMotionAssetTools.generated.h"

/** 编辑器专用武器换手与骨骼修正工具；会保存对应剑术动画、骨架和预览配置，不应在运行时调用。 */
UCLASS()
class THIRDPERSON_API USwordMotionAssetTools : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Rotation-only mode preserves existing weapon positions/scales and never saves meshes or sockets. */
    UFUNCTION(BlueprintCallable, Category="Sword|Editor") static bool BuildSwordMotion(bool bRotationOnly = false);
};
