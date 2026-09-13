#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RangedAIAssetTools.generated.h"

/** Repeatable migration of the existing ranged enemy, blackboard and behavior tree. */
UCLASS()
class THIRDPERSON_API URangedAIAssetTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "AI|Editor")
	static bool ConfigureRangedCombat();

	/** Enemy health/death presentation only; does not rebuild AI, character meshes or maps. */
	UFUNCTION(BlueprintCallable, Category = "AI|Editor")
	static bool ConfigureEnemyPresentation();
};
