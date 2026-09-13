#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatUIAssetTools.generated.h"
UCLASS()
class THIRDPERSON_API UCombatUIAssetTools : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="UI|Editor") static bool InspectUI();
    UFUNCTION(BlueprintCallable, Category="UI|Editor") static bool BuildUIUpgrade();
};
