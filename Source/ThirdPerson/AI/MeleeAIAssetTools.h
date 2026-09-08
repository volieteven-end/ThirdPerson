#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MeleeAIAssetTools.generated.h"

/** Scoped, repeatable editor migration for the existing melee behavior tree. */
UCLASS()
class THIRDPERSON_API UMeleeAIAssetTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "AI|Editor")
	static bool ConfigureMeleeBehaviorTree();
};
