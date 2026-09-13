#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SwordMotionAssetTools.generated.h"

/** Local, additive repair of weapon tracks. Never rebuilds combat montages or character graphs. */
UCLASS()
class THIRDPERSON_API USwordMotionAssetTools : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Sword|Editor") static bool BuildSwordMotion();
};
