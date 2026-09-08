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
#include "AnimNodes/AnimNode_SequenceEvaluator.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
/** Native graph: 4-way locomotion -> DefaultSlot -> local additive -> death override.
 * Standalone nodes avoid AnimBlueprint constant-folded property storage. Game-thread PreUpdate owns all UObject reads. */
struct FCountessAnimProxy : FAnimInstanceProxy
{
 FAnimNode_SequencePlayer_Standalone Idle,Relaxed,Jog[4],CircleJog[2][4],Stun,Death,Hit,Falling;
 FAnimNode_TwoWayBlend CircleForwardBack[2],CircleLeftRight[2],CircleDirection[2],CircleSides,CircleBlend;
 FAnimNode_SequenceEvaluator_Standalone Transition;
 FAnimNode_TwoWayBlend ForwardBack,LeftRight,Direction,Locomotion,IdleBlend,StunBlend,DeathBlend,FallBlend,TransitionBlend;
 FAnimNode_Slot Slot;
 FAnimNode_ApplyAdditive Additive;
 bool bWasDead=false;
 UAnimSequence* PreviousTransition=nullptr;
 float PreviousTransitionTime=0;
 const UBossDefinition* LastDefinition=nullptr;
 FCountessAnimProxy(UAnimInstance* Instance):FAnimInstanceProxy(Instance)
 {
  IdleBlend.A.SetLinkNode(&Relaxed); IdleBlend.B.SetLinkNode(&Idle);
  ForwardBack.A.SetLinkNode(&Jog[0]); ForwardBack.B.SetLinkNode(&Jog[1]);
  LeftRight.A.SetLinkNode(&Jog[2]); LeftRight.B.SetLinkNode(&Jog[3]);
  Direction.A.SetLinkNode(&ForwardBack); Direction.B.SetLinkNode(&LeftRight);
  for (int32 I=0;I<2;++I)
  {
   CircleForwardBack[I].A.SetLinkNode(&CircleJog[I][0]); CircleForwardBack[I].B.SetLinkNode(&CircleJog[I][1]);
   CircleLeftRight[I].A.SetLinkNode(&CircleJog[I][2]); CircleLeftRight[I].B.SetLinkNode(&CircleJog[I][3]);
   CircleDirection[I].A.SetLinkNode(&CircleForwardBack[I]); CircleDirection[I].B.SetLinkNode(&CircleLeftRight[I]);
  }
  CircleSides.A.SetLinkNode(&CircleDirection[0]); CircleSides.B.SetLinkNode(&CircleDirection[1]);
  CircleBlend.A.SetLinkNode(&Direction); CircleBlend.B.SetLinkNode(&CircleSides);
  Locomotion.A.SetLinkNode(&IdleBlend); Locomotion.B.SetLinkNode(&CircleBlend);
  TransitionBlend.A.SetLinkNode(&Locomotion); TransitionBlend.B.SetLinkNode(&Transition);
  Transition.SetGroupName(TEXT("CountessWalk")); Transition.SetGroupMethod(EAnimSyncMethod::SyncGroup);
  Transition.SetGroupRole(EAnimGroupRole::TransitionLeader); Transition.SetShouldLoop(false);
  FallBlend.A.SetLinkNode(&TransitionBlend); FallBlend.B.SetLinkNode(&Falling);
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
   for (int32 Side=0;Side<2;++Side) for (int32 I=0;I<4;++I)
   {
    const auto& Clips=Side==0?D->CircleLeft:D->CircleRight;
    CircleJog[Side][I].SetSequence(Clips.IsValidIndex(I)?Clips[I].Get():(D->Jog.IsValidIndex(I)?D->Jog[I].Get():nullptr));
    CircleJog[Side][I].SetGroupName(TEXT("CountessWalk")); CircleJog[Side][I].SetGroupMethod(EAnimSyncMethod::SyncGroup);
   }
  }
  if (!A) return;
  const FVector V=A->LocalVelocity;
  for (int32 I=0;I<4;++I) Jog[I].SetPlayRate(A->MoveRate);
  ForwardBack.Alpha=FMath::FInterpTo(ForwardBack.Alpha,V.X<0?1.f:0.f,Delta,10);
  LeftRight.Alpha=FMath::FInterpTo(LeftRight.Alpha,V.Y>0?1.f:0.f,Delta,10);
  Direction.Alpha=FMath::FInterpTo(Direction.Alpha,FMath::Abs(V.Y)/FMath::Max(1.f,FMath::Abs(V.X)+FMath::Abs(V.Y)),Delta,10);
  for (int32 Side=0;Side<2;++Side)
  {
   for (int32 I=0;I<4;++I) CircleJog[Side][I].SetPlayRate(A->MoveRate);
   CircleForwardBack[Side].Alpha=ForwardBack.Alpha; CircleLeftRight[Side].Alpha=LeftRight.Alpha; CircleDirection[Side].Alpha=Direction.Alpha;
  }
  CircleSides.Alpha=A->CircleRightAlpha; CircleBlend.Alpha=A->CircleAlpha;
  Locomotion.Alpha=FMath::FInterpTo(Locomotion.Alpha,FMath::Clamp(A->Speed/150.f,0.f,1.f),Delta,10);
  UAnimSequence* TransitionClip=A->TransitionSequence ? A->TransitionSequence.Get() : D->Idle.Get();
  if (TransitionClip!=PreviousTransition || A->TransitionTime<PreviousTransitionTime)
   Transition.SetAccumulatedTime(A->TransitionTime);
  PreviousTransition=TransitionClip; PreviousTransitionTime=A->TransitionTime;
  Transition.SetSequence(TransitionClip);
  Transition.SetExplicitTime(A->TransitionTime); TransitionBlend.Alpha=A->TransitionAlpha;
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
  const float SideWeight=FMath::Abs(LocalVelocity.Y)/FMath::Max(1.f,FMath::Abs(LocalVelocity.X)+FMath::Abs(LocalVelocity.Y));
  auto ReferenceFor=[&](const TArray<float>& Rates)
  { return Rates.Num()==4?FMath::Lerp(Rates[LocalVelocity.X>=0?0:1],Rates[LocalVelocity.Y<0?2:3],SideWeight):300.f; };
  const float Reference=FMath::Lerp(ReferenceFor(BossDefinition->JogReferenceSpeeds),FMath::Lerp(
      ReferenceFor(BossDefinition->CircleLeftReferenceSpeeds),ReferenceFor(BossDefinition->CircleRightReferenceSpeeds),CircleRightAlpha),CircleAlpha);
  MoveRate=FMath::Clamp(Speed/FMath::Max(1.f,Reference),.1f,3.f);
  CombatAlpha=FMath::FInterpTo(CombatAlpha,BossState==EBossState::Dormant?0.f:1.f,Delta,8);
  FallAlpha=FMath::FInterpTo(FallAlpha,bFalling?1.f:0.f,Delta,12);
  StunAlpha=FMath::FInterpTo(StunAlpha,BossState==EBossState::PoiseBroken?1.f:0.f,Delta,12);
  DeathAlpha=FMath::FInterpConstantTo(DeathAlpha,BossState==EBossState::Dead?1.f:0.f,Delta,10);
  DeathTime=BossState==EBossState::Dead?FMath::Min(Boss->BossActions->GetStateElapsed(),BossDefinition->Death->GetPlayLength()):0;
  HitTime=Boss->BossActions->GetHitReactionTime();
  if (BossDefinition->HitReactions.IsValidIndex(HitDirection)) HitSequence=BossDefinition->HitReactions[HitDirection];
  UpdateLocomotion(Boss,Delta);
 }
}
void UCountessBossAnimInstance::BeginLocomotionTransition(EBossLocomotionState State,UAnimSequence* Sequence,float Duration)
{
 if (!Sequence) return;
 LocomotionState=State; TransitionSequence=Sequence; LocomotionElapsed=0;
 TransitionDuration=FMath::Max(.15f,Duration); TransitionTime=0;
 TransitionCooldown=TransitionDuration+.12f;
}
void UCountessBossAnimInstance::UpdateLocomotion(const ACountessBossCharacter* Boss,float Delta)
{
 const auto* D=BossDefinition.Get(); if (!D || D->AnimationRevision<2 || Delta<=0) return;
 const float Yaw=Boss->GetActorRotation().Yaw;
 Acceleration=(Speed-PreviousSpeed)/Delta;
 YawRate=bPoseInitialized ? FMath::FindDeltaAngleDegrees(PreviousYaw,Yaw)/Delta : 0;
 PreviousYaw=Yaw; PreviousSpeed=Speed; bPoseInitialized=true;
 FacingDelta=Boss->BossActions->GetFacingDelta();
 LocomotionElapsed+=Delta; TransitionCooldown=FMath::Max(0.f,TransitionCooldown-Delta);
 const bool bFree=Boss->BossActions->CanMove() && !bFalling;
 const bool bWasMoving=bMoving;
 bMoving=bMoving ? Speed>D->MoveStopThreshold : Speed>D->MoveStartThreshold;
 const FVector CurrentDirection=LocalVelocity.GetSafeNormal2D();
 const FVector Requested=Boss->GetActorTransform().InverseTransformVectorNoScale(Boss->GetCharacterMovement()->GetLastUpdateRequestedVelocity()).GetSafeNormal2D();
 const FVector DesiredDirection=Requested.IsNearlyZero()?CurrentDirection:Requested;
 DirectionChange=bMoving && bWasMoving ? FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
     FVector::DotProduct(DesiredDirection,PreviousMoveDirection),-1.f,1.f))) : 0;
 auto DirectionIndex=[](const FVector& V) { return FMath::Abs(V.X)>=FMath::Abs(V.Y)?(V.X>=0?0:1):(V.Y<0?2:3); };
 auto Clip=[](const TArray<TObjectPtr<UAnimSequence>>& Clips,int32 Index)->UAnimSequence*
 { return Clips.IsValidIndex(Index)?Clips[Index].Get():nullptr; };
 if (!bFree)
 {
  // A full-body action can interrupt any transition, and no hidden start/stop clip restarts on release.
  LocomotionState=bMoving?EBossLocomotionState::Moving:EBossLocomotionState::Idle;
  TransitionDuration=0; TransitionCooldown=0;
 }
 else if (bMoving && !bWasMoving)
 {
  LatchedDirection=DirectionIndex(CurrentDirection);
  auto* S=Clip(D->MoveStarts,LatchedDirection);
  BeginLocomotionTransition(EBossLocomotionState::Start,S,S?S->GetPlayLength():.4f);
 }
 else if (!bMoving && bWasMoving)
 {
  auto* S=Clip(D->MoveStops,LatchedDirection);
  BeginLocomotionTransition(EBossLocomotionState::Stop,S,S?S->GetPlayLength():.45f);
 }
 else if (bMoving && DirectionChange>115.f && TransitionCooldown<=0)
 {
  const int32 OldDirection=LatchedDirection; LatchedDirection=DirectionIndex(DesiredDirection);
  auto* S=Clip(D->MovePivots,OldDirection);
  BeginLocomotionTransition(EBossLocomotionState::Pivot,S,S?S->GetPlayLength():.5f);
 }
 else if (!bMoving && FMath::Abs(FacingDelta)>55.f && TransitionCooldown<=0)
 {
  const int32 Index=(FMath::Abs(FacingDelta)>135.f?2:0)+(FacingDelta>0?1:0);
  auto* S=Clip(D->Turns,Index);
  BeginLocomotionTransition(EBossLocomotionState::Turn,S,FMath::Clamp(FMath::Abs(FacingDelta)/95.f,.65f,1.6f));
 }
 if (bMoving)
 {
  // Keep a stable previous heading through deceleration; only switch quadrant well away from its boundary.
  if (FMath::Abs(FMath::Abs(CurrentDirection.X)-FMath::Abs(CurrentDirection.Y))>.2f) LatchedDirection=DirectionIndex(CurrentDirection);
  PreviousMoveDirection=CurrentDirection;
 }
 const bool bTransition=bFree && TransitionSequence && LocomotionElapsed<TransitionDuration;
 const float Fade=FMath::Max(.05f,D->MovementBlendTime);
 bTransitionActive=bTransition && LocomotionElapsed<TransitionDuration-Fade;
 TransitionRate=TransitionSequence && TransitionDuration>0 ? TransitionSequence->GetPlayLength()/TransitionDuration : 1.f;
 const float Desired=bTransition?FMath::Min(1.f,(TransitionDuration-LocomotionElapsed)/Fade):0.f;
 TransitionAlpha=FMath::FInterpConstantTo(TransitionAlpha,Desired,Delta,1.f/Fade);
 if (TransitionSequence && TransitionDuration>0)
  TransitionTime=FMath::Min(TransitionSequence->GetPlayLength(),LocomotionElapsed/TransitionDuration*TransitionSequence->GetPlayLength());
 if (!bTransition && TransitionAlpha<.01f) LocomotionState=bMoving?EBossLocomotionState::Moving:EBossLocomotionState::Idle;
 CircleAlpha=FMath::FInterpTo(CircleAlpha,bFree && bMoving && Boss->BossActions->IsStrafing() ? FMath::Clamp(FMath::Abs(YawRate)/85.f,0.f,.8f):0.f,Delta,8.f);
 CircleRightAlpha=FMath::FInterpTo(CircleRightAlpha,YawRate>0?1.f:0.f,Delta,8.f);
}
