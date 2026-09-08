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
 UPROPERTY(BlueprintReadOnly, Category="Boss|Locomotion") EBossLocomotionState LocomotionState=EBossLocomotionState::Idle;
 UPROPERTY(BlueprintReadOnly, Category="Boss|Locomotion") float Acceleration=0;
 UPROPERTY(BlueprintReadOnly, Category="Boss|Locomotion") float DirectionChange=0;
 UPROPERTY(BlueprintReadOnly, Category="Boss|Locomotion") float FacingDelta=0;
 UPROPERTY(BlueprintReadOnly, Category="Boss|Locomotion") float YawRate=0;
 UPROPERTY(BlueprintReadOnly, Category="Boss|Locomotion") float CircleAlpha=0;
 UPROPERTY(BlueprintReadOnly, Category="Boss|Locomotion") float CircleRightAlpha=0;
 UPROPERTY(BlueprintReadOnly, Category="Boss|Locomotion") float TransitionAlpha=0;
 UPROPERTY(BlueprintReadOnly, Category="Boss|Locomotion") float TransitionTime=0;
 UPROPERTY(BlueprintReadOnly, Category="Boss|Locomotion") float TransitionRate=1;
 UPROPERTY(BlueprintReadOnly, Category="Boss|Locomotion") bool bTransitionActive=false;
 UPROPERTY(BlueprintReadOnly, Category="Boss|Locomotion") TObjectPtr<UAnimSequence> TransitionSequence;
 UPROPERTY(BlueprintReadOnly, Category="Boss") TObjectPtr<UAnimSequence> HitSequence;
 UPROPERTY(BlueprintReadOnly, Category="Boss") TObjectPtr<const UBossDefinition> BossDefinition;
protected:
 virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
 virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
private:
 float PreviousSpeed=0,PreviousYaw=0,LocomotionElapsed=0,TransitionDuration=0,TransitionCooldown=0;
 FVector PreviousMoveDirection=FVector::ForwardVector;
 int32 LatchedDirection=0;
 bool bMoving=false,bPoseInitialized=false;
 void UpdateLocomotion(const class ACountessBossCharacter* Boss,float Delta);
 void BeginLocomotionTransition(EBossLocomotionState State,UAnimSequence* Sequence,float Duration);
};
