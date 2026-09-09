#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "TPCFootstepNotify.generated.h"

UCLASS(meta=(DisplayName="TPC Footstep"))
class THIRDPERSON_API UTPCFootstepNotify : public UAnimNotify
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footstep") FName FootBone = TEXT("ball_l");
	virtual void Notify(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& Reference) override;
};
