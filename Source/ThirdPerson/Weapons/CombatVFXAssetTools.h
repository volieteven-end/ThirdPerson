#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatVFXAssetTools.generated.h"

/** Targeted editor migration: never reconstructs combat montages, maps or vendor assets. */
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
