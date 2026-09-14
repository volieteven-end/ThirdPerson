#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "TPCFootstepNotify.generated.h"

/** 脚步接触通知，把脚的位置和接触事件交给角色音频组件，统一处理声音选择和去重。 */
UCLASS(meta=(DisplayName="TPC Footstep"))
class THIRDPERSON_API UTPCFootstepNotify : public UAnimNotify
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footstep") FName FootBone = TEXT("ball_l");
	virtual void Notify(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& Reference) override;
};
