#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CountessBossAssetTools.generated.h"
/** Editor-only implementation. Writes only missing assets under Third/Bosses/Countess, never Paragon originals. */
UCLASS()
class THIRDPERSON_API UCountessBossAssetTools : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="Boss|Editor") static bool BuildCountessAssets();
 UFUNCTION(BlueprintCallable,Category="Boss|Editor") static bool BuildCountessTestMap();
};
