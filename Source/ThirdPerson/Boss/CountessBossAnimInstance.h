#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "BossDefinition.h"
#include "CountessBossAnimInstance.generated.h"
/** Works without the legacy player AnimBP. A custom AnimBP may instead consume these same variables. */
UCLASS(Transient,Blueprintable)
class THIRDPERSON_API UCountessBossAnimInstance : public UAnimInstance
{
 GENERATED_BODY()
public:
 virtual void NativeUpdateAnimation(float Delta) override;
 UPROPERTY(BlueprintReadOnly, Category="Boss") FVector LocalVelocity=FVector::ZeroVector;
 UPROPERTY(BlueprintReadOnly, Category="Boss") float Speed=0;
 UPROPERTY(BlueprintReadOnly, Category="Boss") bool bFalling=false;
 UPROPERTY(BlueprintReadOnly, Category="Boss") EBossState BossState=EBossState::Dormant;
 UPROPERTY(BlueprintReadOnly, Category="Boss") float HitAlpha=0;
 UPROPERTY(BlueprintReadOnly, Category="Boss") int32 HitDirection=0;
 UPROPERTY(BlueprintReadOnly, Category="Boss") int32 BossPhase=1;
 UPROPERTY(BlueprintReadOnly, Category="Boss") float MoveX=0;
 UPROPERTY(BlueprintReadOnly, Category="Boss") float MoveY=0;
 UPROPERTY(BlueprintReadOnly, Category="Boss") float MoveRate=1;
 UPROPERTY(BlueprintReadOnly, Category="Boss") float CombatAlpha=0;
 UPROPERTY(BlueprintReadOnly, Category="Boss") float FallAlpha=0;
 UPROPERTY(BlueprintReadOnly, Category="Boss") float StunAlpha=0;
 UPROPERTY(BlueprintReadOnly, Category="Boss") float DeathAlpha=0;
 UPROPERTY(BlueprintReadOnly, Category="Boss") float DeathTime=0;
 UPROPERTY(BlueprintReadOnly, Category="Boss") float HitTime=0;
 UPROPERTY(BlueprintReadOnly, Category="Boss") TObjectPtr<UAnimSequence> HitSequence;
 UPROPERTY(BlueprintReadOnly, Category="Boss") TObjectPtr<const UBossDefinition> BossDefinition;
protected:
 virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
 virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
