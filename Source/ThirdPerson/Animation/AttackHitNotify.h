
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AttackHitNotify.generated.h"

/** 单帧近战命中通知，将检测交给战斗组件；兼容仍使用此通知的动画资源。 */
UCLASS()
class THIRDPERSON_API UAttackHitNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp,UAnimSequenceBase* Animation,const FAnimNotifyEventReference& EventReference) override;
};