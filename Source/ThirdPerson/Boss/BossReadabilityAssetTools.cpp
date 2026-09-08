#include "CountessBossAssetTools.h"
#if WITH_EDITOR
#include "BossDefinition.h"
#include "BossDamageWindowNotifyState.h"
#include "CountessBossCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "AnimationBlueprintLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

bool BuildCountessGraphAssets(USkeletalMesh*,UBossDefinition*,UClass*&,bool);
namespace BossReadabilityAuthoring
{
const FString Root=TEXT("/Game/Third/Bosses/Countess/Animations/");
UAnimSequence* Source(const FString& Name)
{ return LoadObject<UAnimSequence>(nullptr,*(TEXT("/Game/ParagonCountess/Characters/Heroes/Countess/Animations/")+Name+TEXT(".")+Name)); }
template<class T> T* Find(const FString& Name)
{ return LoadObject<T>(nullptr,*(Root+Name+TEXT(".")+Name),nullptr,LOAD_NoWarn); }
bool Save(UObject* Asset)
{
 if (!Asset) return false;
 Asset->MarkPackageDirty(); FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; Args.SaveFlags=SAVE_NoError;
 return UPackage::SavePackage(Asset->GetOutermost(),Asset,*FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
}
FTransform Pose(UAnimSequence* Sequence,int32 Bone,float Time)
{
 FTransform Result;
 Sequence->GetBoneTransform(Result,FSkeletonPoseBoneIndex(Bone),FAnimExtractContext(
     static_cast<double>(FMath::Clamp(Time,0.f,Sequence->GetPlayLength()))),true);
 return Result;
}
/** Re-sample only owned copies. No vendor package or shared skeleton is dirtied. */
UAnimSequence* Sample(const FString& Name,UAnimSequence* Template,float Duration,TFunctionRef<FTransform(int32,float)> Evaluate)
{
 if (!Template) return nullptr;
 UAnimSequence* Out=Find<UAnimSequence>(Name);
 if (!Out)
 {
  Out=DuplicateObject<UAnimSequence>(Template,CreatePackage(*(Root+Name)),*Name);
  Out->ClearFlags(RF_Transient); Out->SetFlags(RF_Public|RF_Standalone); FAssetRegistryModule::AssetCreated(Out);
 }
 // Vendor clips include fractional rates (e.g. 29.772). Keep the target's rate;
 // forcing 60 onto a populated fractional-rate model is rejected by UE.
 const int32 Frames=FMath::Max(2,FMath::RoundToInt(Duration*Out->GetDataModel()->GetFrameRate().AsDecimal()));
 const auto& Ref=Template->GetSkeleton()->GetReferenceSkeleton();
 // Collect before opening the target controller (Template and target must never alias).
 TArray<FName> Tracks; Template->GetDataModel()->GetBoneTrackNames(Tracks);
 auto& C=Out->GetController(); C.OpenBracket(NSLOCTEXT("Countess","ReadableClips","Author Boss animation copy"),false);
 C.RemoveAllBoneTracks(false); C.RemoveAllCurvesOfType(ERawCurveTrackTypes::RCT_Float,false); C.RemoveAllAttributes(false);
 C.SetNumberOfFrames(FFrameNumber(Frames),false);
 bool Ok=true;
 for (FName Track:Tracks)
 {
  const int32 Bone=Ref.FindBoneIndex(Track); if (Bone==INDEX_NONE) continue;
  TArray<FVector3f> Positions,Scales; TArray<FQuat4f> Rotations;
  for (int32 Frame=0;Frame<=Frames;++Frame)
  {
   const FTransform T=Evaluate(Bone,Frame/static_cast<float>(Frames));
   Positions.Add(FVector3f(T.GetLocation())); Scales.Add(FVector3f(T.GetScale3D())); Rotations.Add(FQuat4f(T.GetRotation().GetNormalized()));
  }
  Ok&=C.AddBoneCurve(Track,false); Ok&=C.SetBoneTrackKeys(Track,Positions,Rotations,Scales,false);
 }
 C.NotifyPopulated(); C.CloseBracket(false);
 Out->RateScale=1.f; Out->bEnableRootMotion=false; Out->bForceRootLock=true;
 Out->Notifies.Reset(); UAnimationBlueprintLibrary::RemoveAllAnimationSyncMarkers(Out);
 Out->PostEditChange();
 return Ok?Out:nullptr;
}
UAnimSequence* Bridge(const FString& Name,UAnimSequence* From,float FromTime,UAnimSequence* To,float ToTime,float Duration=.1f)
{
 auto* Out=Sample(Name,To,Duration,[&](int32 Bone,float Alpha)
 {
  FTransform Result; Result.Blend(Pose(From,Bone,FromTime),Pose(To,Bone,ToTime),Alpha*Alpha*(3.f-2.f*Alpha)); return Result;
 });
 return Save(Out)?Out:nullptr;
}
TArray<float> Contacts(UAnimSequence* Sequence,FName FootName)
{
 const auto& Ref=Sequence->GetSkeleton()->GetReferenceSkeleton(); const int32 Foot=Ref.FindBoneIndex(FootName);
 TArray<float> Heights,Times; if (Foot==INDEX_NONE) return Times;
 float Lowest=MAX_flt,Highest=-MAX_flt; constexpr int32 N=240;
 for (int32 I=0;I<N;++I)
 {
  FTransform Global=FTransform::Identity; const float T=I/static_cast<float>(N)*Sequence->GetPlayLength();
  for (int32 Bone=Foot;Bone!=INDEX_NONE;Bone=Ref.GetParentIndex(Bone)) Global=Global*Pose(Sequence,Bone,T);
  const float Z=Global.GetLocation().Z; Heights.Add(Z); Lowest=FMath::Min(Lowest,Z); Highest=FMath::Max(Highest,Z);
 }
 const float Threshold=Lowest+FMath::Max(2.f,(Highest-Lowest)*.12f);
 for (int32 I=0;I<N;++I)
  if (Heights[I]<=Threshold && Heights[(I+N-1)%N]>Threshold)
  {
   const float T=I/static_cast<float>(N)*Sequence->GetPlayLength();
   if (Times.IsEmpty() || T-Times.Last()>.2f) Times.Add(T);
  }
 return Times;
}
UAnimSequence* Cycle(const FString& Name,UAnimSequence* Sequence)
{
 if (!Sequence) return nullptr;
 TArray<FAnimSyncMarker> Markers; UAnimationBlueprintLibrary::GetAnimationSyncMarkers(Sequence,Markers);
 Markers.Sort([](const auto& A,const auto& B){return A.Time<B.Time;});
 float Begin=-1,End=-1,Right=-1;
 for (const auto& Marker:Markers)
  if (Marker.MarkerName==TEXT("LeftPlant")) { if (Begin<0) Begin=Marker.Time; else { End=Marker.Time; break; } }
 // Circle clips may contain several strides without markers. Use successive foot plants,
 // not the entire three-stride clip mislabeled as one cycle.
 if (Begin<0)
 {
  const auto Plants=Contacts(Sequence,TEXT("foot_l"));
  if (!Plants.IsEmpty()) Begin=Plants[0];
  if (Plants.Num()>1) End=Plants[1];
 }
 if (Begin<0) return nullptr;
 if (End<0) End=Begin+Sequence->GetPlayLength();
 const float Length=End-Begin;
 for (const auto& M:Markers) if (M.MarkerName==TEXT("RightPlant"))
 { const float T=M.Time<Begin?M.Time+Sequence->GetPlayLength():M.Time; if (T>Begin && T<End) { Right=(T-Begin)/Length; break; } }
 if (Right<0) for (float T:Contacts(Sequence,TEXT("foot_r")))
 { if (T<Begin) T+=Sequence->GetPlayLength(); if (T>Begin && T<End) { Right=(T-Begin)/Length; break; } }
 if (Right<0) Right=.5f;
 auto* Out=Sample(Name,Sequence,Length,[&](int32 Bone,float Alpha)
 { return Pose(Sequence,Bone,FMath::Fmod(Begin+Alpha*Length,Sequence->GetPlayLength())); });
 if (!Out) return nullptr;
 if (!UAnimationBlueprintLibrary::IsValidAnimNotifyTrackName(Out,TEXT("Feet"))) UAnimationBlueprintLibrary::AddAnimationNotifyTrack(Out,TEXT("Feet"));
 UAnimationBlueprintLibrary::AddAnimationSyncMarker(Out,TEXT("LeftPlant"),0.f,TEXT("Feet"));
 UAnimationBlueprintLibrary::AddAnimationSyncMarker(Out,TEXT("RightPlant"),Right*Out->GetPlayLength(),TEXT("Feet"));
 return Save(Out)?Out:nullptr;
}
UAnimSequence* Transition(const FString& Name,UAnimSequence* Sequence,float SourceEnd,float Duration)
{
 if (!Sequence) return nullptr;
 const float End=FMath::Min(SourceEnd,Sequence->GetPlayLength());
 auto* Out=Sample(Name,Sequence,Duration,[&](int32 Bone,float Alpha){ return Pose(Sequence,Bone,Alpha*End); });
 if (Out)
 {
  if (!UAnimationBlueprintLibrary::IsValidAnimNotifyTrackName(Out,TEXT("Feet"))) UAnimationBlueprintLibrary::AddAnimationNotifyTrack(Out,TEXT("Feet"));
  for (const TCHAR* Side:{TEXT("l"),TEXT("r")}) for (float T:Contacts(Sequence,FName(*(FString(TEXT("foot_"))+Side))))
   if (T<End) UAnimationBlueprintLibrary::AddAnimationSyncMarker(Out,FString(Side)==TEXT("l")?TEXT("LeftPlant"):TEXT("RightPlant"),T/End*Out->GetPlayLength(),TEXT("Feet"));
 }
 return Save(Out)?Out:nullptr;
}
void Segment(UAnimMontage* M,UAnimSequence* Sequence,float Begin,float End,float Duration)
{
 FAnimSegment S; S.SetAnimReference(Sequence); S.AnimStartTime=Begin; S.AnimEndTime=End;
 S.AnimPlayRate=(End-Begin)/FMath::Max(.001f,Duration); S.LoopingCount=1;
 auto& Track=M->SlotAnimTracks[0].AnimTrack; S.StartPos=Track.GetLength(); Track.AnimSegments.Add(S);
}
UAnimMontage* Montage(const FString& Name,UAnimSequence* Sequence)
{
 auto* M=Find<UAnimMontage>(Name);
 if (!M)
 {
  auto* Temp=UAnimMontage::CreateSlotAnimationAsDynamicMontage(Sequence,TEXT("DefaultSlot"),.08f,.08f,1.f,1);
  M=DuplicateObject<UAnimMontage>(Temp,CreatePackage(*(Root+Name)),*Name);
  M->ClearFlags(RF_Transient); M->SetFlags(RF_Public|RF_Standalone); FAssetRegistryModule::AssetCreated(M);
 }
 M->Notifies.Reset(); M->SlotAnimTracks.SetNum(1); M->SlotAnimTracks[0].SlotName=TEXT("DefaultSlot"); M->SlotAnimTracks[0].AnimTrack.AnimSegments.Reset();
 M->BlendIn.SetBlendTime(.08f); M->BlendOut.SetBlendTime(.08f); M->BlendOutTriggerTime=0.f; M->RateScale=1.f;
 return M;
}
bool Finalize(UAnimMontage* M)
{
 M->SetCompositeLength(M->CalculateSequenceLength()); M->UpdateLinkableElements(); M->PostEditChange(); return Save(M);
}
bool Attack(FBossActionDefinition& A,int32 Index)
{
 auto& S=A.Stages[Index];
 const FString Id=StaticEnum<EBossAction>()->GetNameStringByValue(static_cast<int64>(A.Id));
 const FString Name=FString::Printf(TEXT("Countess_%s_%d"),*Id,Index+1);
 auto* M=Montage(TEXT("AM_")+Name,S.Sequence); if (!M) return false;
 // Read ORIGINAL source timing from native bootstrap data, not the already-migrated montage.
 const auto* Original=GetDefault<UBossDefinition>()->FindAction(A.Id); const auto& O=Original->Stages[Index];
 const float OriginalHit=O.HitStart,OriginalEnd=O.HitEnd;
 float Desired=.35f,Lead=.15f;
 switch (A.Id)
 {
 case EBossAction::Combo: Desired=Index==0?.55f:.35f; break;
 case EBossAction::DelayedSlash: Desired=.85f; Lead=.20f; break;
 case EBossAction::Siphon: Desired=.75f; break;
 case EBossAction::ShadowRush: Desired=Index==0?.65f:.35f; break;
 case EBossAction::BloodWave: Desired=.75f; Lead=.20f; break;
 case EBossAction::BloodFeast: Desired=2.05f; Lead=.20f; break;
 default: return false;
 }
 S.MinReadableWindup=Desired; S.EntryBlendTime=.08f;
 const bool bRush=O.MoveStart>=0;
 float Offset=0;
 if (A.Id==EBossAction::Siphon || A.Id==EBossAction::BloodWave)
 {
  auto* Prep=Source(A.Id==EBossAction::Siphon?TEXT("Ability_Q_target_transition"):TEXT("Ability_E_target_transition"));
  if (!Prep) return false;
  const float PrepDuration=A.Id==EBossAction::Siphon?.63f:.45f;
  auto* Join=Bridge(TEXT("AS_")+Name+TEXT("_PreparationJoin"),Prep,Prep->GetPlayLength(),S.Sequence,0.f);
  if (!Prep || !Join) return false;
  Segment(M,Prep,0,Prep->GetPlayLength(),PrepDuration); Segment(M,Join,0,Join->GetPlayLength(),Join->GetPlayLength());
  Offset=M->SlotAnimTracks[0].AnimTrack.GetLength();
  Segment(M,S.Sequence,0,S.Sequence->GetPlayLength(),S.Sequence->GetPlayLength());
 }
 else
 {
  const float Event=bRush?O.MoveStart:OriginalHit;
  const float Split=A.Id==EBossAction::BloodFeast?.3f:FMath::Max(.04f,Event-.05f);
  const float Extra=FMath::Max(0.f,S.EntryBlendTime+Desired-Event);
  Segment(M,S.Sequence,0,Split,Split+Extra);
  Segment(M,S.Sequence,Split,S.Sequence->GetPlayLength(),S.Sequence->GetPlayLength()-Split);
  Offset=Extra;
 }
 S.Montage=M; S.HitStart=OriginalHit+Offset; S.HitEnd=OriginalEnd+Offset;
 S.MoveStart=bRush?O.MoveStart+Offset:-1.f; S.MoveEnd=bRush?O.MoveEnd+Offset:-1.f;
 S.FacingCommitTime=(bRush?S.MoveStart:S.HitStart)-Lead;
 if (A.Id==EBossAction::BloodFeast) S.FacingCommitTime=FMath::Min(.5f,S.FacingCommitTime);
 M->SetCompositeLength(M->CalculateSequenceLength());
 if (!UAnimationBlueprintLibrary::IsValidAnimNotifyTrackName(M,TEXT("Boss"))) UAnimationBlueprintLibrary::AddAnimationNotifyTrack(M,TEXT("Boss"));
 UAnimationBlueprintLibrary::AddAnimationNotifyStateEvent(M,TEXT("Boss"),S.HitStart,S.HitEnd-S.HitStart,UBossDamageWindowNotifyState::StaticClass());
 if (!Finalize(M)) return false;
 // The attack clip ends in the next combo stance. Only finishers play the exit, never between two blades.
 const TCHAR* Recovery=A.Id==EBossAction::Combo ? (Index==0?TEXT("Primary_Attack_A_Slow_Recovery"):Index==1?TEXT("Primary_Attack_B_Slow_Recovery"):TEXT("Primary_Attack_Slow_Recovery")) :
     A.Id==EBossAction::DelayedSlash?TEXT("Primary_Attack_A_Slow_Recovery"):TEXT("Primary_Attack_Slow_Recovery");
 S.RecoverySequence=Source(Recovery);
 auto* Exit=Montage(TEXT("AM_")+Name+TEXT("_Recovery"),S.RecoverySequence);
 auto* Join=Bridge(TEXT("AS_")+Name+TEXT("_RecoveryJoin"),S.Sequence,S.Sequence->GetPlayLength(),S.RecoverySequence,0.f);
 if (!Exit || !Join || !S.RecoverySequence) return false;
 Segment(Exit,Join,0,Join->GetPlayLength(),.10f);
 Segment(Exit,S.RecoverySequence,0,S.RecoverySequence->GetPlayLength(),FMath::Max(.55f,A.Recovery-.1f));
 S.RecoveryMontage=Exit;
 UE_LOG(LogTemp,Display,TEXT("Readable Boss: %s start %.3f end %.3f commit %.3f move %.3f..%.3f length %.3f"),*Name,S.HitStart,S.HitEnd,S.FacingCommitTime,S.MoveStart,S.MoveEnd,M->GetPlayLength());
 return Finalize(Exit);
}
}
#endif

bool UCountessBossAssetTools::UpgradeReadableAnimations()
{
#if WITH_EDITOR
 using namespace BossReadabilityAuthoring;
 auto* D=LoadObject<UBossDefinition>(nullptr,TEXT("/Game/Third/Bosses/Countess/DA_CountessBoss.DA_CountessBoss"));
 auto* Class=LoadClass<ACountessBossCharacter>(nullptr,TEXT("/Game/Third/Bosses/Countess/BP_CountessBoss.BP_CountessBoss_C"));
 if (!D || !Class) return false;
 for (auto& A:D->Actions) for (int32 I=0;I<A.Stages.Num();++I) if (!Attack(A,I)) return false;
 D->MoveStarts.Reset(); D->MoveStops.Reset(); D->MovePivots.Reset(); D->CircleLeft.Reset(); D->CircleRight.Reset(); D->Turns.Reset(); D->Jog.Reset();
 for (const TCHAR* Direction:{TEXT("Fwd"),TEXT("Bwd"),TEXT("Left"),TEXT("Right")})
 {
  const FString Prefix=FString(TEXT("Jog_"))+Direction;
  auto* Jog=Cycle(TEXT("AS_Countess_")+Prefix,Source(Prefix+TEXT("_Combat")));
  auto* Left=Cycle(TEXT("AS_Countess_")+Prefix+TEXT("_CircleLeft"),Source(Prefix+TEXT("_CircleLeft")));
  auto* Right=Cycle(TEXT("AS_Countess_")+Prefix+TEXT("_CircleRight"),Source(Prefix+TEXT("_CircleRight")));
  auto* Start=Transition(TEXT("AS_Countess_")+Prefix+TEXT("_Start"),Source(Prefix+TEXT("_Start")),.65f,.45f);
  auto* Stop=Transition(TEXT("AS_Countess_")+Prefix+TEXT("_Stop"),Source(Prefix+TEXT("_Stop")),.7f,.45f);
  auto* Pivot=Transition(TEXT("AS_Countess_")+Prefix+TEXT("_Pivot"),Source(Prefix+TEXT("_Pivot")),.9f,.6f);
  if (!Jog || !Left || !Right || !Start || !Stop || !Pivot) return false;
  D->Jog.Add(Jog); D->CircleLeft.Add(Left); D->CircleRight.Add(Right); D->MoveStarts.Add(Start); D->MoveStops.Add(Stop); D->MovePivots.Add(Pivot);
 }
 for (const TCHAR* Name:{TEXT("Turn_Left_90"),TEXT("Turn_Right_90"),TEXT("Turn_Left_180"),TEXT("Turn_Right_180")})
 {
  auto* Original=Source(Name); auto* Turn=Transition(FString(TEXT("AS_Countess_"))+Name,Original,FMath::Min(Original->GetPlayLength(),1.7f),1.f);
  if (!Turn) return false; D->Turns.Add(Turn);
 }
 D->AnimationRevision=2;
 FString Error; if (!D->Validate(Error)) { UE_LOG(LogTemp,Error,TEXT("Boss readable upgrade: %s"),*Error); return false; }
 UClass* AnimClass=nullptr;
 return BuildCountessGraphAssets(Class->GetDefaultObject<ACountessBossCharacter>()->GetMesh()->GetSkeletalMeshAsset(),D,AnimClass,true) && Save(D);
#else
 return false;
#endif
}
