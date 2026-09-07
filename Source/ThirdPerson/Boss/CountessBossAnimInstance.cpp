#include "CountessBossAnimInstance.h"
#include "CountessBossCharacter.h"
#include "BossActionComponent.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "AnimNodes/AnimNode_Slot.h"
#include "AnimNodes/AnimNode_ApplyAdditive.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
/** Native graph: 4-way locomotion -> DefaultSlot -> local additive -> death override.
 * Standalone nodes avoid AnimBlueprint constant-folded property storage. Game-thread PreUpdate owns all UObject reads. */
struct FCountessAnimProxy : FAnimInstanceProxy
{
 FAnimNode_SequencePlayer_Standalone Idle,Relaxed,Jog[4],Stun,Death,Hit,Falling;
 FAnimNode_TwoWayBlend ForwardBack,LeftRight,Direction,Locomotion,IdleBlend,StunBlend,DeathBlend,FallBlend;
 FAnimNode_Slot Slot;
 FAnimNode_ApplyAdditive Additive;
 bool bWasDead=false;
 const UBossDefinition* LastDefinition=nullptr;
 FCountessAnimProxy(UAnimInstance* Instance):FAnimInstanceProxy(Instance)
 {
  IdleBlend.A.SetLinkNode(&Relaxed); IdleBlend.B.SetLinkNode(&Idle);
  ForwardBack.A.SetLinkNode(&Jog[0]); ForwardBack.B.SetLinkNode(&Jog[1]);
  LeftRight.A.SetLinkNode(&Jog[2]); LeftRight.B.SetLinkNode(&Jog[3]);
  Direction.A.SetLinkNode(&ForwardBack); Direction.B.SetLinkNode(&LeftRight);
  Locomotion.A.SetLinkNode(&IdleBlend); Locomotion.B.SetLinkNode(&Direction);
  FallBlend.A.SetLinkNode(&Locomotion); FallBlend.B.SetLinkNode(&Falling);
  StunBlend.A.SetLinkNode(&FallBlend); StunBlend.B.SetLinkNode(&Stun);
  Slot.Source.SetLinkNode(&StunBlend); Slot.SlotName=TEXT("DefaultSlot"); Slot.bAlwaysUpdateSourcePose=true;
  Additive.Base.SetLinkNode(&Slot); Additive.Additive.SetLinkNode(&Hit);
  // Native transitions use explicit blend alphas, not inertial requests (which require compiled AnimBP node data).
  DeathBlend.A.SetLinkNode(&Additive); DeathBlend.B.SetLinkNode(&Death);
  Death.SetLoopAnimation(false); Hit.SetLoopAnimation(false);
 }
 virtual FAnimNode_Base* GetCustomRootNode() override { return &DeathBlend; }
 virtual void PreUpdate(UAnimInstance* Instance,float Delta) override
 {
  FAnimInstanceProxy::PreUpdate(Instance,Delta);
  const auto* A=Cast<UCountessBossAnimInstance>(Instance);
  const auto* D=A && A->BossDefinition?A->BossDefinition.Get():GetDefault<UBossDefinition>();
  if (D!=LastDefinition)
  {
   LastDefinition=D; Idle.SetSequence(D->Idle); Relaxed.SetSequence(D->Relaxed); Stun.SetSequence(D->StunLoop); Death.SetSequence(D->Death);
   Falling.SetSequence(D->Falling);
   for (int32 I=0;I<4;++I) if (D->Jog.IsValidIndex(I)) { Jog[I].SetSequence(D->Jog[I]); Jog[I].SetGroupName(TEXT("CountessWalk")); Jog[I].SetGroupMethod(EAnimSyncMethod::SyncGroup); }
  }
  if (!A) return;
  const FVector V=A->LocalVelocity;
  for (int32 I=0;I<4;++I) Jog[I].SetPlayRate(D->JogReferenceSpeeds.IsValidIndex(I)?FMath::Clamp(A->Speed/FMath::Max(1.f,D->JogReferenceSpeeds[I]),.1f,3.f):1.f);
  ForwardBack.Alpha=FMath::FInterpTo(ForwardBack.Alpha,V.X<0?1.f:0.f,Delta,10);
  LeftRight.Alpha=FMath::FInterpTo(LeftRight.Alpha,V.Y>0?1.f:0.f,Delta,10);
  Direction.Alpha=FMath::FInterpTo(Direction.Alpha,FMath::Abs(V.Y)/FMath::Max(1.f,FMath::Abs(V.X)+FMath::Abs(V.Y)),Delta,10);
  Locomotion.Alpha=FMath::FInterpTo(Locomotion.Alpha,FMath::Clamp(A->Speed/150.f,0.f,1.f),Delta,10);
  FallBlend.Alpha=FMath::FInterpTo(FallBlend.Alpha,A->bFalling?1.f:0.f,Delta,12);
  IdleBlend.Alpha=FMath::FInterpTo(IdleBlend.Alpha,A->BossState==EBossState::Dormant?0.f:1.f,Delta,8);
  StunBlend.Alpha=FMath::FInterpTo(StunBlend.Alpha,A->BossState==EBossState::PoiseBroken?1.f:0.f,Delta,12);
  if (D->HitReactions.IsValidIndex(A->HitDirection)) Hit.SetSequence(D->HitReactions[A->HitDirection]);
  if (A->HitAlpha>Additive.Alpha+.02f) Hit.SetAccumulatedTime(0.f);
  Additive.Alpha=A->HitAlpha;
  const bool bDead=A->BossState==EBossState::Dead;
  if (bDead && !bWasDead) Death.SetAccumulatedTime(0.f);
  bWasDead=bDead; DeathBlend.Alpha=FMath::FInterpConstantTo(DeathBlend.Alpha,bDead?1.f:0.f,Delta,10);
 }
};
}
FAnimInstanceProxy* UCountessBossAnimInstance::CreateAnimInstanceProxy()
{ return Cast<UAnimBlueprintGeneratedClass>(GetClass())?Super::CreateAnimInstanceProxy():new FCountessAnimProxy(this); }
void UCountessBossAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
void UCountessBossAnimInstance::NativeUpdateAnimation(float Delta)
{
 Super::NativeUpdateAnimation(Delta);
 if (const auto* Boss=Cast<ACountessBossCharacter>(TryGetPawnOwner()))
 {
  LocalVelocity=Boss->GetActorTransform().InverseTransformVectorNoScale(Boss->GetVelocity()); Speed=LocalVelocity.Size2D();
  bFalling=Boss->GetCharacterMovement()->IsFalling(); BossState=Boss->BossActions->State;
  HitAlpha=Boss->BossActions->GetHitReactionAlpha(); HitDirection=Boss->BossActions->GetHitReactionDirection(); BossDefinition=Boss->BossActions->GetDefinition();
  BossPhase=Boss->BossActions->Phase;
  const FVector Input=LocalVelocity/FMath::Max(150.f,Speed);
  MoveX=FMath::FInterpTo(MoveX,Input.X,Delta,10); MoveY=FMath::FInterpTo(MoveY,Input.Y,Delta,10);
  const auto& Rates=BossDefinition->JogReferenceSpeeds;
  const float SideWeight=FMath::Abs(LocalVelocity.Y)/FMath::Max(1.f,FMath::Abs(LocalVelocity.X)+FMath::Abs(LocalVelocity.Y));
  const float Reference=Rates.Num()==4?FMath::Lerp(Rates[LocalVelocity.X>=0?0:1],Rates[LocalVelocity.Y<0?2:3],SideWeight):300.f;
  MoveRate=FMath::Clamp(Speed/FMath::Max(1.f,Reference),.1f,3.f);
  CombatAlpha=FMath::FInterpTo(CombatAlpha,BossState==EBossState::Dormant?0.f:1.f,Delta,8);
  FallAlpha=FMath::FInterpTo(FallAlpha,bFalling?1.f:0.f,Delta,12);
  StunAlpha=FMath::FInterpTo(StunAlpha,BossState==EBossState::PoiseBroken?1.f:0.f,Delta,12);
  DeathAlpha=FMath::FInterpConstantTo(DeathAlpha,BossState==EBossState::Dead?1.f:0.f,Delta,10);
  DeathTime=BossState==EBossState::Dead?FMath::Min(Boss->BossActions->GetStateElapsed(),BossDefinition->Death->GetPlayLength()):0;
  HitTime=(1.f-HitAlpha/.25f)*.35f;
  if (BossDefinition->HitReactions.IsValidIndex(HitDirection)) HitSequence=BossDefinition->HitReactions[HitDirection];
 }
}
