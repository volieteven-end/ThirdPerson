// Boss 动画资源回归：核对预警、命中、转向锁定及收招时间轴。
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "AnimationBlueprintLibrary.h"
#include "../Boss/BossDefinition.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossReadableAssetTest,"ThirdPerson.Boss.Readability.AssetTimelines",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBossReadableAssetTest::RunTest(const FString&)
{
 auto* D=LoadObject<UBossDefinition>(nullptr,TEXT("/Game/Third/Bosses/Countess/DA_CountessBoss.DA_CountessBoss"));
 if (!TestNotNull(TEXT("Saved encounter definition"),D)) return false;
 TestEqual(TEXT("Saved assets were migrated, not just C++ defaults"),D->AnimationRevision,2);
 FString Error; TestTrue(*FString::Printf(TEXT("Asset validation: %s"),*Error),D->Validate(Error));
 for (const auto& A:D->Actions) for (int32 I=0;I<A.Stages.Num();++I)
 {
  const auto& S=A.Stages[I]; float Start=0,End=0; S.ReadHitWindow(Start,End);
  const FString Label=FString::Printf(TEXT("Action %d stage %d"),static_cast<int32>(A.Id),I);
  float Minimum=.35f,CommitLead=.15f;
  switch (A.Id)
  {
  case EBossAction::Combo: Minimum=I==0?.55f:.35f; break;
  case EBossAction::DelayedSlash: Minimum=.85f; CommitLead=.2f; break;
  case EBossAction::Siphon: Minimum=.75f; break;
  case EBossAction::ShadowRush: Minimum=I==0?.65f:.35f; break;
  case EBossAction::BloodWave: Minimum=.75f; CommitLead=.2f; break;
  case EBossAction::BloodFeast: Minimum=2.05f; break;
  default: break;
  }
  const float Event=S.MoveStart>=0?S.MoveStart:Start;
  TestTrue(Label+TEXT(" visible startup excludes blend-in"),Event-S.EntryBlendTime>=Minimum-.0001f);
  TestTrue(Label+TEXT(" facing commits before release"),Event-S.FacingCommitTime>=CommitLead-.0001f);
  if (!TestNotNull(Label+TEXT(" authored montage"),S.Montage.Get())) continue;
  TestEqual(Label+TEXT(" runtime montage rate is normal"),S.Montage->RateScale,1.f);
  bool bFoundStrike=false;
  for (const auto& Segment:S.Montage->SlotAnimTracks[0].AnimTrack.AnimSegments)
   if (Start>=Segment.StartPos && Start<Segment.StartPos+Segment.GetLength())
   { bFoundStrike=true; TestTrue(Label+TEXT(" strike segment is not slowed"),FMath::IsNearlyEqual(Segment.AnimPlayRate,1.f,.001f)); }
  TestTrue(Label+TEXT(" damage belongs to a real animation segment"),bFoundStrike);
  if (TestNotNull(Label+TEXT(" animated recovery"),S.RecoveryMontage.Get()))
   TestTrue(Label+TEXT(" preserves recovery floor"),S.RecoveryMontage->GetPlayLength()>=A.Recovery-.0001f);
  if (S.MoveStart>=0) TestTrue(Label+TEXT(" rush finishes before blade activates"),S.MoveEnd<=Start);
  AddInfo(FString::Printf(TEXT("%s readable=%.3f hit=%.3f..%.3f commit=%.3f"),*Label,Event-S.EntryBlendTime,Start,End,S.FacingCommitTime));
 }
 for (const auto* Clips:{&D->Jog,&D->CircleLeft,&D->CircleRight}) for (const auto& Asset:*Clips)
 {
  auto* S=Asset.Get();
  if (!TestNotNull(TEXT("Owned locomotion cycle"),S)) continue;
  TestTrue(TEXT("Vendor animations remain source-only"),S->GetPathName().StartsWith(TEXT("/Game/Third/Bosses/Countess/Animations/AS_Countess_")));
  TArray<FAnimSyncMarker> Markers; UAnimationBlueprintLibrary::GetAnimationSyncMarkers(S,Markers);
  TestEqual(S->GetName()+TEXT(" has one pair of foot markers"),Markers.Num(),2);
  TestFalse(S->GetName()+TEXT(" is a full pose, not an additive used as base"),S->IsValidAdditive());
 }
 for (const auto* Clips:{&D->MoveStarts,&D->MoveStops,&D->MovePivots,&D->Turns})
  for (const auto& Asset:*Clips) if (auto* S=Asset.Get(); TestNotNull(TEXT("Transition clip"),S)) TestTrue(S->GetName()+TEXT(" has bounded positive duration"),S->GetPlayLength()>.2f && S->GetPlayLength()<1.7f);
 for (const auto* Rates:{&D->JogReferenceSpeeds,&D->CircleLeftReferenceSpeeds,&D->CircleRightReferenceSpeeds})
 {
  TestEqual(TEXT("Each gait has directional foot-speed calibration"),Rates->Num(),4);
  for (float Rate:*Rates) TestTrue(TEXT("Foot speed is finite and useful"),FMath::IsFinite(Rate) && Rate>50 && Rate<900);
 }
 return true;
}
#endif
