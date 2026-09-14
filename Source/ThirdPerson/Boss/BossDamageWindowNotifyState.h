#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "BossDamageWindowNotifyState.generated.h"
/** Boss 动画的伤害区间通知，为当前招式提供命中时机；过期动画不能打开下一招的窗口。 */
UCLASS(meta=(DisplayName="Boss Damage Window"))
class THIRDPERSON_API UBossDamageWindowNotifyState : public UAnimNotifyState
{
 GENERATED_BODY()
public:
 virtual void NotifyBegin(USkeletalMeshComponent* Mesh,UAnimSequenceBase* Animation,float Duration,const FAnimNotifyEventReference& Ref) override;
 virtual void NotifyEnd(USkeletalMeshComponent* Mesh,UAnimSequenceBase* Animation,const FAnimNotifyEventReference& Ref) override;
};
