#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TutorialAssetTools.generated.h"

/** Creates missing tutorial assets only. Existing maps and material edits are never regenerated. */
UCLASS()
class THIRDPERSON_API UTutorialAssetTools : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Tutorial|Editor") static bool BuildForestTutorial();
    UFUNCTION(BlueprintCallable,Category="Tutorial|Editor") static bool ValidateForestTutorial();
};
