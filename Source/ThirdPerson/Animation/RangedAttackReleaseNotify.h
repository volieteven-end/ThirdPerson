#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "RangedAttackReleaseNotify.generated.h"

/** 弓箭释放通知：在动画松弦帧请求战斗组件生成投射物，并校验当前攻击实例。 */
UCLASS(meta = (DisplayName = "Release Ranged Projectile"))
class THIRDPERSON_API URangedAttackReleaseNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
