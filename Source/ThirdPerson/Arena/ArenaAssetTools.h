#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ArenaAssetTools.generated.h"

UCLASS()
class THIRDPERSON_API UArenaAssetTools : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Additive, tagged map authoring. Never rebuilds existing tutorial/Boss actors or montages. */
    UFUNCTION(BlueprintCallable, Category="Arena|Editor") static bool BuildArenaExpansion();
    UFUNCTION(BlueprintCallable, Category="Arena|Editor") static bool ValidateArenaExpansion();
};
