#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "ComboChainPointNotify.generated.h"

/** 连招衔接通知：动画到达指定帧后才消费缓存输入，并验证蒙太奇实例，避免旧动作推进新连招。 */
UCLASS(meta = (DisplayName = "Combo Chain Point"))
class THIRDPERSON_API UComboChainPointNotify : public UAnimNotify
{
    GENERATED_BODY()
public:
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference) override;
    virtual FString GetNotifyName_Implementation() const override { return TEXT("Combo Chain Point"); }
};
