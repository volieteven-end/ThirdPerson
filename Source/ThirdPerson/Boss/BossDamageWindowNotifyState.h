#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "BossDamageWindowNotifyState.generated.h"
/** Visible montage annotation. Runtime montage-position sampling also covers skipped notify frames. */
UCLASS(meta=(DisplayName="Boss Damage Window"))
class THIRDPERSON_API UBossDamageWindowNotifyState : public UAnimNotifyState
{
 GENERATED_BODY()
public:
 virtual void NotifyBegin(USkeletalMeshComponent* Mesh,UAnimSequenceBase* Animation,float Duration,const FAnimNotifyEventReference& Ref) override;
 virtual void NotifyEnd(USkeletalMeshComponent* Mesh,UAnimSequenceBase* Animation,const FAnimNotifyEventReference& Ref) override;
};
