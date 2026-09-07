#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "ComboChainPointNotify.generated.h"

/** Place once at the earliest legal follow-up frame, on an attack montage. */
UCLASS(meta = (DisplayName = "Combo Chain Point"))
class THIRDPERSON_API UComboChainPointNotify : public UAnimNotify
{
    GENERATED_BODY()
public:
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference) override;
    virtual FString GetNotifyName_Implementation() const override { return TEXT("Combo Chain Point"); }
};
