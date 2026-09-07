#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "RangedAttackReleaseNotify.generated.h"

/** Releases the pending projectile exactly when the bow/string animation fires. */
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
