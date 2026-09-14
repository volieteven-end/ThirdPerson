#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "ActionCommitNotify.generated.h"

/** 动作提交通知：在动画指定帧落实下砸、Buff 或收拔刀等行为；过期蒙太奇通知不生效。 */
UCLASS(meta=(DisplayName="Action Commit"))
class THIRDPERSON_API UActionCommitNotify : public UAnimNotify
{
    GENERATED_BODY()
public:
    virtual void Notify(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& Reference) override;
    virtual FString GetNotifyName_Implementation() const override { return TEXT("Action Commit"); }
};
