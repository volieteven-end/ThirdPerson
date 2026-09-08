#include "BossDefinition.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Particles/ParticleSystem.h"
#include "BossBloodWave.h"
#include "BossDamageWindowNotifyState.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"

float FBossActionStage::Length() const { return Montage ? Montage->GetPlayLength() : (Sequence ? Sequence->GetPlayLength() : 0.f); }
bool FBossActionStage::ReadHitWindow(float& Start,float& End) const
{
 if (!Montage) { Start=HitStart; End=HitEnd; return End>Start; }
 int32 Count=0;
 for (const auto& Event:Montage->Notifies)
  if (Event.NotifyStateClass && Event.NotifyStateClass->IsA<UBossDamageWindowNotifyState>())
  { Start=Event.GetTriggerTime(); End=Event.GetEndTriggerTime(); ++Count; }
 return Count==1 && End>Start;
}

UBossDefinition::UBossDefinition()
{
 DisplayName = NSLOCTEXT("CountessBoss", "Name", "绯刃伯爵夫人");
 auto Anim = [](const TCHAR* Name)
 {
  const FString Path = FString(TEXT("/Game/ParagonCountess/Characters/Heroes/Countess/Animations/")) + Name + TEXT(".") + Name;
  ConstructorHelpers::FObjectFinder<UAnimSequence> Asset(*Path);
  return Asset.Object;
 };
 auto FX = [](const TCHAR* Path) { ConstructorHelpers::FObjectFinder<UParticleSystem> Asset(Path); return Asset.Object; };
 Intro=Anim(TEXT("LevelStart")); PhaseCast=Anim(TEXT("Cast")); StunStart=Anim(TEXT("Stun_Start"));
 StunLoop=Anim(TEXT("Stun_Loop")); Death=Anim(TEXT("Death")); Idle=Anim(TEXT("Idle_Pose")); Relaxed=Anim(TEXT("Idle_Relaxed"));
 Falling=Anim(TEXT("Jump_Apex")); Landing=Anim(TEXT("Jump_Land"));
 Jog={Anim(TEXT("Jog_Fwd_Combat")),Anim(TEXT("Jog_Bwd_Combat")),Anim(TEXT("Jog_Left_Combat")),Anim(TEXT("Jog_Right_Combat"))};
 HitReactions={Anim(TEXT("Hitreact_Fwd")),Anim(TEXT("Hitreact_Bwd")),Anim(TEXT("Hitreact_Left")),Anim(TEXT("Hitreact_Right"))};
 auto Stage=[&](const TCHAR* Name, float Damage, float Start, float End, EBossHitShape Shape=EBossHitShape::Blades)
 { FBossActionStage S; S.Sequence=Anim(Name); S.Damage=Damage; S.HitStart=Start; S.HitEnd=End; S.Shape=Shape; return S; };
 auto Add=[&](EBossAction Id,float Min,float Max,float Warn,float Recovery,float CD,float W1,float W2,TArray<FBossActionStage> Stages)
 { FBossActionDefinition A; A.Id=Id; A.MinRange=Min; A.MaxRange=Max; A.Telegraph=Warn; A.Recovery=Recovery;
   A.Cooldown=CD; A.PhaseOneWeight=W1; A.PhaseTwoWeight=W2; A.Stages=MoveTemp(Stages); Actions.Add(A); };
 Add(EBossAction::Combo,0,220,.4f,.55f,2.5f,45,35,{Stage(TEXT("Primary_Attack_A_Normal"),16,.15f,.37f),Stage(TEXT("Primary_Attack_B_Normal"),18,.13f,.4f),Stage(TEXT("Primary_Attack_Normal"),22,.15f,.4f)});
 auto Heavy=Stage(TEXT("Primary_Attack_A_Slow"),30,.25f,.62f); Heavy.FacingCommitTime=.18f;
 Add(EBossAction::DelayedSlash,0,240,.55f,.55f,5,20,20,{Heavy});
 auto Q=Stage(TEXT("Ability_Q"),22,.1f,.4f,EBossHitShape::Radial); Q.bCanBeParried=false;
 Add(EBossAction::Siphon,0,260,.65f,.65f,9,15,15,{Q});
 auto Rush=Stage(TEXT("Ability_RMB"),24,.43f,.75f); Rush.MoveStart=.12f; Rush.MoveEnd=.42f;
 Add(EBossAction::ShadowRush,300,650,.55f,.6f,8,10,15,{Rush,Stage(TEXT("Primary_Attack_Normal"),18,.15f,.4f)});
 auto Wave=Stage(TEXT("Ability_E"),20,.45f,.46f,EBossHitShape::Projectile); Wave.FacingCommitTime=.3f; Wave.bCanBeParried=false;
 Add(EBossAction::BloodWave,400,1200,.55f,.55f,7,10,15,{Wave});
 auto Feast=Stage(TEXT("Ability_Ultimate"),38,2.05f,2.25f,EBossHitShape::Radial); Feast.Radius=300; Feast.bCanBeBlocked=false; Feast.bCanBeParried=false;
 Add(EBossAction::BloodFeast,0,350,.9f,1,18,0,20,{Feast});
 ImpactEffect=FX(TEXT("/Game/ParagonCountess/FX/Particles/Abilities/Primary/FX/p_CountessImpact"));
 SiphonEffect=FX(TEXT("/Game/ParagonCountess/FX/Particles/Abilities/BladeSiphon/FX/P_Countess_BladeSiphon_RingFX"));
 RushEffect=FX(TEXT("/Game/ParagonCountess/FX/Particles/Abilities/BlinkStrike/FX/P_Countess_BlinkStrike_HitFX"));
 RushBeginEffect=FX(TEXT("/Game/ParagonCountess/FX/Particles/Abilities/BlinkStrike/FX/P_Countess_TeleportBegin"));
 RushArriveEffect=FX(TEXT("/Game/ParagonCountess/FX/Particles/Abilities/BlinkStrike/FX/P_Countess_TeleportArrive"));
 WaveCastEffect=FX(TEXT("/Game/ParagonCountess/FX/Particles/Abilities/RollingDark/FX/P_Countess_RD_CastBurst"));
 FeastEffect=FX(TEXT("/Game/ParagonCountess/FX/Particles/Abilities/Ultimate/FX/p_CountessUlt_GroundImpactFX"));
 PhaseEffect=FX(TEXT("/Game/ParagonCountess/FX/Particles/Abilities/Ultimate/FX/p_CountessUlt_CastFX"));
 TrailEffect=FX(TEXT("/Game/ParagonCountess/FX/Particles/p_CountessMeleeTrail"));
 WaveClass=ABossBloodWave::StaticClass();
 if (FPackageName::DoesPackageExist(TEXT("/Game/Third/Bosses/Countess/Materials/M_BossTelegraph")))
  WarningMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Third/Bosses/Countess/Materials/M_BossTelegraph.M_BossTelegraph"));
}
const FBossActionDefinition* UBossDefinition::FindAction(EBossAction Id) const
{ return Actions.FindByPredicate([Id](const FBossActionDefinition& A){return A.Id==Id;}); }
bool UBossDefinition::Validate(FString& Error) const
{
 if (MaxHealth<=0 || PoiseOne<=0 || PoiseTwo<=0 || BreakDuration<=0 || PhaseThreshold<=0 || PhaseThreshold>=1)
 { Error=TEXT("Invalid health/poise/phase tuning"); return false; }
 TSet<EBossAction> Seen;
 for (const auto& A:Actions)
 {
  if (A.Id==EBossAction::None || Seen.Contains(A.Id) || A.Stages.IsEmpty() || A.Cooldown<0 || A.Telegraph<0 || A.Recovery<0 || A.MinRange>A.MaxRange)
  { Error=TEXT("Invalid/duplicate action definition"); return false; }
  Seen.Add(A.Id);
  for (const auto& S:A.Stages)
  {
   float Start=0,End=0;
   if (!S.Sequence || S.Sequence->IsValidAdditive() || S.Length()<=0 || !S.ReadHitWindow(Start,End) || Start<0 || End>S.Length() || S.Damage<0)
   { Error=FString::Printf(TEXT("Invalid clip or hit window for action %d: %s, events %d, start %.4f, end %.4f, length %.4f"),static_cast<int32>(A.Id),*GetNameSafe(S.Montage),S.Montage?S.Montage->Notifies.Num():0,Start,End,S.Length()); return false; }
   if (AnimationRevision>=2)
   {
    const float FirstEvent=S.MoveStart>=0 ? S.MoveStart : Start;
    if (!S.Montage || S.MinReadableWindup<=0 || FirstEvent-S.EntryBlendTime+.0001f<S.MinReadableWindup ||
        S.FacingCommitTime>FirstEvent-.149f || !S.RecoveryMontage ||
        (S.MoveStart>=0 && (S.MoveEnd<=S.MoveStart || Start<S.MoveEnd)))
    { Error=FString::Printf(TEXT("Unreadable body startup / missing recovery: %s"),*GetNameSafe(S.Montage)); return false; }
   }
  }
 }
 if (Seen.Num()!=6 || !Idle || !Relaxed || !Intro || !Death || !StunStart || !StunLoop || !PhaseCast || !WaveClass)
 { Error=TEXT("Missing mandatory Countess assets/actions"); return false; }
 if (AnimationRevision>=2 && (Jog.Num()!=4 || JogReferenceSpeeds.Num()!=4 || MoveStarts.Num()!=4 ||
     MoveStops.Num()!=4 || MovePivots.Num()!=4 || CircleLeft.Num()!=4 || CircleRight.Num()!=4 || Turns.Num()!=4))
 { Error=TEXT("Incomplete readable Boss locomotion set"); return false; }
 return true;
}
