#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SwordActionAssetTools.generated.h"

/** Deterministic editor authoring for the sword design; runtime never constructs project assets. */
UCLASS()
class THIRDPERSON_API USwordActionAssetTools : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Sword|Editor") static bool BuildSwordActionAssets();
    UFUNCTION(BlueprintCallable, Category="Sword|Editor") static FString InspectSwordGraphs();
    /** Re-author only the feedback-tuned graph, input context and recoil defaults. */
    UFUNCTION(BlueprintCallable, Category="Sword|Editor") static bool ApplyGameplayFeedbackAssets();
    /** Tune sprint recovery, airborne pose extraction and the arrow visual component only. */
    UFUNCTION(BlueprintCallable, Category="Sword|Editor") static bool ApplyInertiaFeedbackAssets();
    /** Save only the requested keyboard mappings and per-direction dodge control handoff. */
    UFUNCTION(BlueprintCallable, Category="Sword|Editor") static bool ApplyInputDodgeFeedbackAssets();
};
