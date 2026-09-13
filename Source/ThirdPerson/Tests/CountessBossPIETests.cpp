#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "../Boss/CountessBossCharacter.h"
#include "../Boss/CountessBossAIController.h"
#include "../Boss/BossActionComponent.h"
#include "../Components/HealthComponent.h"
#include "../Weapons/ProjectilePoolSubsystem.h"
#include "../Boss/CountessBossAnimInstance.h"
#include "../Boss/BossStatusWidget.h"
#include "../Boss/BossTelegraph.h"
#include "../Boss/BossBloodWave.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Misc/App.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerStart.h"
#include "../Components/LevelComponent.h"
#include "../GameMode/TPCGameMode.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "../Character/TPCCharacter.h"
#include "../Components/ActionComponent.h"
#include "../Components/CombatComponent.h"
#include "../Components/EquipmentComponent.h"
#include "../Components/StaminaComponent.h"
#include "../Weapons/WeaponDefinition.h"
#include "BossReadabilityCapture.h"

struct FCountessPIETestAccess
{
 static void Prepare(UBossActionComponent& A,AActor* Target,int32 Phase)
 { A.CancelAction(); A.CooldownUntil.Reset(); A.Target=Target; A.Phase=Phase; A.bPhasePending=false; A.RecoilUntil=0; A.Poise=A.GetMaxPoise(); A.State=EBossState::Combat; A.LastSeen=A.Now(); A.LastPoiseHit=A.Now(); A.PoiseImmuneUntil=0; }
 static bool HasMotion(const UBossActionComponent& A) { return A.MoveSourceId!=0; }
 static int32 Stage(const UBossActionComponent& A) { return A.StageIndex; }
 static float ClipTime(const UBossActionComponent& A) { return A.ActiveMontage?A.Boss->GetMesh()->GetAnimInstance()->Montage_GetPosition(A.ActiveMontage):0; }
 static bool ValidateRush(UBossActionComponent& A,FVector& Out) { A.LockedDirection=A.Boss->GetActorForwardVector(); return A.ValidateRush(Out); }
 static void TickWarning(UBossActionComponent& A) { A.TickAction(1.f/60); }
 static bool PlayingStage(const UBossActionComponent& A) { return A.Step==UBossActionComponent::EActionStep::Playing; }
 static bool Recovering(const UBossActionComponent& A) { return A.Step==UBossActionComponent::EActionStep::Recovery; }
 static bool Committed(const UBossActionComponent& A) { return A.bFacingCommitted; }
};

namespace
{
class FCountessPIEScenario : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;
 int32 Stage=0;
 double WallStart=FPlatformTime::Seconds(),StageStart=0;
 TWeakObjectPtr<ACountessBossCharacter> Boss;
 TWeakObjectPtr<ACharacter> Player;
 FVector InitialLocation,MeshRelative;
 TObjectPtr<UBossDefinition> OriginalDefinition;
 bool bBackpedalled=false;
 bool bKeptFacing=true;
 bool bDeathUIBefore=false,bDeathUIAfter=false;
 FVector FinalDeathPose=FVector::ZeroVector;
public:
 explicit FCountessPIEScenario(FAutomationTestBase* InTest):Test(InTest) {}
 bool Update() override
 {
  if (FPlatformTime::Seconds()-WallStart>50)
  { Test->AddError(FString::Printf(TEXT("Boss PIE scenario timed out at stage %d"),Stage)); return true; }
  UWorld* W=GEditor?GEditor->PlayWorld:nullptr;
  if (!W || !W->HasBegunPlay()) return false;
  if (Stage==0)
  {
   for (TActorIterator<ACountessBossCharacter> It(W);It;++It) { Boss=*It; break; }
   Player=UGameplayStatics::GetPlayerCharacter(W,0);
   if (!Boss.IsValid() || !Player.IsValid()) return false;
   Player->FindComponentByClass<UHealthComponent>()->SetEncounterInvulnerable(true);
   InitialLocation=Boss->GetActorLocation(); MeshRelative=Boss->GetMesh()->GetRelativeLocation();
   auto* AI=Cast<ACountessBossAIController>(Boss->GetController());
   if (!Test->TestNotNull(TEXT("Boss auto possessed by native AI"),AI)) return true;
   Test->TestTrue(TEXT("Runtime behavior tree running"),AI->GetBrainComponent() && AI->GetBrainComponent()->IsRunning());
   Test->TestNotNull(TEXT("Runtime behavior tree has initialized blackboard"),AI->GetBlackboardComponent());
   Test->TestNotNull(TEXT("Compiled independent AnimBP is active"),Cast<UAnimBlueprintGeneratedClass>(Boss->GetMesh()->GetAnimInstance()->GetClass()));
   auto* Nav=UNavigationSystemV1::GetCurrent(W); FNavLocation Location;
   Test->TestTrue(TEXT("Test arena navigation ready in PIE"),Nav && Nav->ProjectPointToNavigation(Boss->GetActorLocation(),Location,FVector(100,100,200)));
   OriginalDefinition=Boss->BossActions->Definition;
   auto* MovementOnly=DuplicateObject<UBossDefinition>(OriginalDefinition,Boss.Get());
   for (auto& Action:MovementOnly->Actions) Action.PhaseOneWeight=Action.PhaseTwoWeight=0;
   Boss->BossActions->Definition=MovementOnly;
   StageStart=W->GetTimeSeconds(); Stage=1;
  }
  else if (Stage==1 && W->GetTimeSeconds()-StageStart>8)
  {
   auto* A=Boss->BossActions.Get();
   Test->TestTrue(TEXT("Approach starts an encounter"),A->IsEncounterActive());
   Test->TestTrue(TEXT("Intro exits before combat"),A->State!=EBossState::Intro);
   Test->TestTrue(TEXT("AI actually moves capsule, not only an animation"),FVector::Dist2D(InitialLocation,Boss->GetActorLocation())>30);
   Test->TestTrue(TEXT("Mesh remains attached to capsule"),MeshRelative.Equals(Boss->GetMesh()->GetRelativeLocation(),.1f));
   TArray<UUserWidget*> Widgets; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Widgets,UBossStatusWidget::StaticClass(),true);
   Test->TestEqual(TEXT("Encounter displays exactly one standalone Boss HUD"),Widgets.Num(),1);
   if (Widgets.Num()==1)
   {
    Test->TestTrue(TEXT("Boss HUD keeps its top-center viewport anchor"),Widgets[0]->GetAnchorsInViewport().Minimum.Equals(FVector2D(.5f,0)));
    Test->TestTrue(TEXT("Boss HUD centers its own width"),Widgets[0]->GetAlignmentInViewport().Equals(FVector2D(.5f,0)));
   }
   A->CancelAction();
   auto* NoAttacks=DuplicateObject<UBossDefinition>(OriginalDefinition,Boss.Get());
   for (auto& Action:NoAttacks->Actions) Action.PhaseOneWeight=Action.PhaseTwoWeight=0;
   A->Definition=NoAttacks;
   Boss->SetActorLocationAndRotation(FVector(0,0,100),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
   Player->SetActorLocation(FVector(150,0,100),false,nullptr,ETeleportType::TeleportPhysics);
   StageStart=W->GetTimeSeconds(); Stage=2;
  }
  else if (Stage==2)
  {
   const FVector V=Boss->GetVelocity();
   if (V.Size2D()>25)
   {
    bBackpedalled|=FVector::DotProduct(V.GetSafeNormal2D(),Boss->GetActorForwardVector())<-.3f;
    bKeptFacing&=FVector::DotProduct((Player->GetActorLocation()-Boss->GetActorLocation()).GetSafeNormal2D(),Boss->GetActorForwardVector())>.4f;
   }
   if (W->GetTimeSeconds()-StageStart>2.5)
   {
    Test->TestTrue(TEXT("Close-range locomotion includes backward movement"),bBackpedalled);
    Test->TestTrue(TEXT("Backpedal/orbit faces player instead of turning away"),bKeptFacing);
    Boss->BossActions->Definition=OriginalDefinition;
    auto* Level=Player->FindComponentByClass<ULevelComponent>(); Level->RestoreProgress(1,0,{});
    auto* Mode=Cast<ATPCGameMode>(W->GetAuthGameMode()); const int32 Kills=Mode->GetEnemiesDefeated();
    FCombatHitSpec Lethal; Lethal.Damage=100000; Lethal.PoiseDamage=10000;
    auto* H=Boss->FindComponentByClass<UHealthComponent>(); H->SetEncounterInvulnerable(false);
    H->ApplyCombatHit(Lethal,Player.Get());
    H->ApplyCombatHit(Lethal,Player.Get());
    Test->TestEqual(TEXT("One death awards exactly 300 XP (two levels, 75 remainder)"),Level->GetCurrentExperience(),75);
    Test->TestEqual(TEXT("One death levels the fresh player to level three"),Level->GetLevel(),3);
    Test->TestEqual(TEXT("Repeated lethal hits count the kill only once"),Mode->GetEnemiesDefeated(),Kills+1);
    Test->TestFalse(TEXT("Boss does not call global WinGame"),Mode->IsGameWon());
    Test->TestEqual(TEXT("Lethal threshold plus poise hit never starts phase two"),Boss->BossActions->Phase,1);
    Test->TestEqual(TEXT("Real health death delegates reach Boss override"),Boss->BossActions->State,EBossState::Dead);
    Test->TestFalse(TEXT("Death immediately closes all action windows"),Boss->BossActions->IsActionActive());
    Test->TestEqual(TEXT("Death immediately disables capsule collision"),Boss->GetCapsuleComponent()->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
    Test->TestTrue(TEXT("Death schedules five-second lifetime"),Boss->GetLifeSpan()>=4.9f && Boss->GetLifeSpan()<=5.f);
    if (auto* PC=Cast<APlayerController>(Player->GetController())) PC->SetPause(false); // XP upgrade UI may pause the single-player world.
    StageStart=W->GetTimeSeconds(); Stage=3;
   }
  }
  else if (Stage==3)
  {
   const float Elapsed=W->GetTimeSeconds()-StageStart;
   if (Elapsed>.5f && !bDeathUIBefore)
   {
    TArray<UUserWidget*> Widgets; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Widgets,UBossStatusWidget::StaticClass(),true);
    Test->TestEqual(TEXT("Defeat HUD remains during the first two seconds"),Widgets.Num(),1); bDeathUIBefore=true;
   }
   if (Elapsed>2.6f && !bDeathUIAfter)
   {
    TArray<UUserWidget*> Widgets; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Widgets,UBossStatusWidget::StaticClass(),true);
    Test->TestEqual(TEXT("Defeat HUD fades out after two seconds"),Widgets.Num(),0);
    FinalDeathPose=Boss->GetMesh()->GetSocketLocation(TEXT("spine_03")); bDeathUIAfter=true;
   }
   if (Elapsed>4.5f && Boss.IsValid()) Test->TestTrue(TEXT("Compiled AnimBP holds Death's final frame"),FinalDeathPose.Equals(Boss->GetMesh()->GetSocketLocation(TEXT("spine_03")),.1f));
   if (Elapsed>5.3f) { Test->TestFalse(TEXT("Dead boss is removed after five game seconds"),Boss.IsValid()); return true; }
  }
  return false;
 }
};

/** End-to-end checks use the saved BP, real mesh pose, CMC, collision queries, Montages and projectile pool. */
class FCountessActionScenario : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;
 TWeakObjectPtr<ACountessBossCharacter> Boss;
 TWeakObjectPtr<ACharacter> Player;
 TObjectPtr<UBossDefinition> Definition;
 int32 State=0,CaseIndex=0,FPSIndex=0;
 double Start=0,WallStart=FPlatformTime::Seconds();
 bool PreviousFixed=FApp::UseFixedTimeStep(); double PreviousDelta=FApp::GetFixedDeltaTime();
 FVector MeshOffset,StartPosition,DeadPose;
 FRotator CameraRotation;
 bool bMotionObserved=false,bMissedTelegraph=false;
 FString BladeTrace=TEXT("action,stage,time,left_distance,right_distance\n");
 bool bScreenshotRequested=false;
 int32 ObservedStage=-1,VideoTick=0,VideoFrame=0;
 float StageHealth=0,PreparationTravel=0;
 FVector InitialPreparationBlade;
 bool bCheckedRelease=false,bSampledPreparation=false;
 double RecoveryStarted=-1;
 FString ReadabilityTrace=TEXT("fps,phase,action,stage,montage_time,first_event,hit_start,commit,preparation_travel,health,committed,recovering,world_elapsed\n");
 struct FCase { EBossAction Action; float Distance; int32 Phase; float Damage; };
 const FCase Cases[11]={{EBossAction::Combo,110,1,34},{EBossAction::DelayedSlash,150,1,30},{EBossAction::Siphon,200,1,22},
  {EBossAction::ShadowRush,500,1,24},{EBossAction::BloodWave,500,1,20},{EBossAction::BloodFeast,200,2,38},
  {EBossAction::Combo,110,2,56},{EBossAction::ShadowRush,500,2,42},
  {EBossAction::DelayedSlash,150,2,30},{EBossAction::Siphon,200,2,22},{EBossAction::BloodWave,500,2,20}};
 const int32 FrameRates[3]={30,60,120};
 void Stop() { FApp::SetUseFixedTimeStep(PreviousFixed); FApp::SetFixedDeltaTime(PreviousDelta); }
public:
 explicit FCountessActionScenario(FAutomationTestBase* InTest):Test(InTest) {}
 virtual ~FCountessActionScenario() override { Stop(); }
 bool Update() override
 {
  if (FPlatformTime::Seconds()-WallStart>300) { Test->AddError(FString::Printf(TEXT("Boss actions timed out at state %d case %d FPS %d"),State,CaseIndex,FrameRates[FPSIndex])); Stop(); return true; }
  UWorld* W=GEditor?GEditor->PlayWorld:nullptr; if (!W || !W->HasBegunPlay()) return false;
  if (State==0)
  {
   for (TActorIterator<ACountessBossCharacter> It(W);It;++It) { Boss=*It; break; }
   Player=UGameplayStatics::GetPlayerCharacter(W,0); if (!Boss.IsValid() || !Player.IsValid()) return false;
   Definition=Boss->BossActions->Definition;
   auto* D=DuplicateObject<UBossDefinition>(Definition,Boss.Get()); for (auto& A:D->Actions) A.PhaseOneWeight=A.PhaseTwoWeight=0;
   Boss->BossActions->Definition=D;
   Player->FindComponentByClass<UHealthComponent>()->SetEncounterInvulnerable(true);
   MeshOffset=Boss->GetMesh()->GetRelativeLocation();
   if (FParse::Param(FCommandLine::Get(),TEXT("BossVisualAudit")) || FParse::Param(FCommandLine::Get(),TEXT("BossReadableVideo")))
   {
    const FVector Eye(650,-650,350); auto* Camera=W->SpawnActor<ACameraActor>(Eye,(FVector(80,0,70)-Eye).Rotation());
    Camera->GetCameraComponent()->SetFieldOfView(65);
    Cast<APlayerController>(Player->GetController())->SetViewTarget(Camera);
    IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("BossReadability/Frames")),true);
   }
   FApp::SetFixedDeltaTime(1./60); FApp::SetUseFixedTimeStep(true); State=1; Start=W->GetTimeSeconds();
   W->GetWorldSettings()->MinUndilatedFrameTime=0; W->GetWorldSettings()->MaxUndilatedFrameTime=1;
  }
  else if (State==1 && Boss->BossActions->State==EBossState::Combat)
  {
   auto* AI=Cast<AAIController>(Boss->GetController()); AI->GetBrainComponent()->StopLogic(TEXT("Deterministic action verification")); AI->StopMovement();
   Boss->BossActions->Definition=Definition;
   Player->GetCharacterMovement()->StopMovementImmediately(); Player->GetCharacterMovement()->DisableMovement();
   State=2;
  }
  else if (State==2)
  {
   FApp::SetFixedDeltaTime(1./FrameRates[FPSIndex]);
   const FCase& C=Cases[CaseIndex];
   FCountessPIETestAccess::Prepare(*Boss->BossActions,Player.Get(),C.Phase);
   Boss->SetActorLocationAndRotation(FVector(0,0,98),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
   Boss->GetCharacterMovement()->SetMovementMode(MOVE_Walking); Boss->GetCharacterMovement()->StopMovementImmediately();
   Player->SetActorLocationAndRotation(FVector(C.Distance,0,98),FRotator(0,180,0),false,nullptr,ETeleportType::TeleportPhysics);
   auto* H=Player->FindComponentByClass<UHealthComponent>(); H->MaxHealth=1000; H->SetCurrentHealth(1000); H->SetEncounterInvulnerable(false);
   Boss->FindComponentByClass<UHealthComponent>()->SetCurrentHealth(1000); Boss->FindComponentByClass<UHealthComponent>()->SetEncounterInvulnerable(false);
   StartPosition=Boss->GetActorLocation(); CameraRotation=Player->GetController()->GetControlRotation();
   bMotionObserved=false; bMissedTelegraph=false; ObservedStage=-1; RecoveryStarted=-1; Start=W->GetTimeSeconds(); State=3;
  }
  else if (State==3 && W->GetTimeSeconds()-Start>.2)
  {
   Test->TestTrue(FString::Printf(TEXT("World really ticks at requested %d FPS (delta %.6f)"),FrameRates[FPSIndex],W->GetDeltaSeconds()),FMath::IsNearlyEqual(W->GetDeltaSeconds(),1.f/FrameRates[FPSIndex],.0001f));
   Test->TestTrue(FString::Printf(TEXT("%d FPS action %d accepted"),FrameRates[FPSIndex],CaseIndex),Boss->BossActions->TryStartAction(Cases[CaseIndex].Action));
   Start=W->GetTimeSeconds(); State=4;
  }
  else if (State==4)
  {
   const auto* A=Boss->BossActions.Get();
   if (Definition->AnimationRevision>=2 && FCountessPIETestAccess::PlayingStage(*A))
   {
    const int32 Stage=FCountessPIETestAccess::Stage(*A); const auto& S=Definition->FindAction(Cases[CaseIndex].Action)->Stages[Stage];
    const float Time=FCountessPIETestAccess::ClipTime(*A); float HitStart=0,HitEnd=0; S.ReadHitWindow(HitStart,HitEnd);
    const float Event=S.MoveStart>=0?S.MoveStart:HitStart;
    const float HP=Player->FindComponentByClass<UHealthComponent>()->CurrentHealth;
    const FVector Blade=Boss->GetMesh()->GetSocketLocation(TEXT("BladeTip_R"));
    if (ObservedStage!=Stage)
    {
     ObservedStage=Stage; StageHealth=HP; PreparationTravel=0; InitialPreparationBlade=Blade; bCheckedRelease=false; bSampledPreparation=false;
     Test->TestTrue(TEXT("Next stage starts directly in animation, not idle warning"),Time<.08f);
    }
    // The entry cross-fade and the final fast swing cannot masquerade as animated anticipation.
    if (Time>=S.EntryBlendTime+.02f && Time<Event-.05f)
    {
     if (!bSampledPreparation) { InitialPreparationBlade=Blade; bSampledPreparation=true; }
     PreparationTravel=FMath::Max(PreparationTravel,static_cast<float>(FVector::Dist(Blade,InitialPreparationBlade)));
    }
    if (Time<HitStart-.0001f) Test->TestEqual(TEXT("No damage during the body's preparation"),HP,StageHealth);
    if (Time<Event-.0001f && S.MoveStart>=0) Test->TestFalse(TEXT("Rush cannot move before visible preparation"),FCountessPIETestAccess::HasMotion(*A));
    if (!bCheckedRelease && Time>=Event)
    {
     bCheckedRelease=true;
     Test->TestTrue(TEXT("Preparation animates the actual weapon/body, not a frozen pose"),PreparationTravel>=8.f);
     Test->AddInfo(FString::Printf(TEXT("Readable phase=%d action=%d stage=%d fps=%d event=%.3f bodyTravel=%.1f"),Cases[CaseIndex].Phase,static_cast<int32>(Cases[CaseIndex].Action),Stage,FrameRates[FPSIndex],Event,PreparationTravel));
    }
    if (Time>=S.FacingCommitTime+2.f/FrameRates[FPSIndex]) Test->TestTrue(TEXT("Facing actually commits before release"),FCountessPIETestAccess::Committed(*A));
    if (Time<S.FacingCommitTime-2.f/FrameRates[FPSIndex]) Test->TestFalse(TEXT("Early preparation still permits facing"),FCountessPIETestAccess::Committed(*A));
    ReadabilityTrace+=FString::Printf(TEXT("%d,%d,%d,%d,%.4f,%.4f,%.4f,%.4f,%.2f,%.1f,%d,0,%.4f\n"),FrameRates[FPSIndex],Cases[CaseIndex].Phase,static_cast<int32>(Cases[CaseIndex].Action),Stage,Time,Event,HitStart,S.FacingCommitTime,PreparationTravel,HP,FCountessPIETestAccess::Committed(*A)?1:0,W->GetTimeSeconds()-Start);
   }
   if (FCountessPIETestAccess::Recovering(*A))
   {
    if (RecoveryStarted<0) RecoveryStarted=W->GetTimeSeconds();
    ReadabilityTrace+=FString::Printf(TEXT("%d,%d,%d,%d,%.4f,0,0,0,0,%.1f,1,1,%.4f\n"),FrameRates[FPSIndex],Cases[CaseIndex].Phase,static_cast<int32>(Cases[CaseIndex].Action),FCountessPIETestAccess::Stage(*A),FCountessPIETestAccess::ClipTime(*A),Player->FindComponentByClass<UHealthComponent>()->CurrentHealth,W->GetTimeSeconds()-Start);
   }
   if (FPSIndex==0 && (CaseIndex==0 || CaseIndex==3))
   {
    float Distances[2]; const auto* Capsule=Player->GetCapsuleComponent();
    const float HalfSegment=Capsule->GetScaledCapsuleHalfHeight()-Capsule->GetScaledCapsuleRadius();
    const FVector Bottom=Player->GetActorLocation()-FVector(0,0,HalfSegment),Top=Player->GetActorLocation()+FVector(0,0,HalfSegment);
    for (int32 I=0;I<2;++I)
    {
     FVector P,Q; const FString Side=I==0?TEXT("L"):TEXT("R");
     FMath::SegmentDistToSegmentSafe(Boss->GetMesh()->GetSocketLocation(FName(*(TEXT("BladeBase_")+Side))),Boss->GetMesh()->GetSocketLocation(FName(*(TEXT("BladeTip_")+Side))),Bottom,Top,P,Q);
     Distances[I]=FVector::Dist(P,Q)-Capsule->GetScaledCapsuleRadius();
    }
    BladeTrace+=FString::Printf(TEXT("%d,%d,%.4f,%.2f,%.2f\n"),CaseIndex,FCountessPIETestAccess::Stage(*A),FCountessPIETestAccess::ClipTime(*A),Distances[0],Distances[1]);
   }
   bMotionObserved|=FCountessPIETestAccess::HasMotion(*A);
   const double Elapsed=W->GetTimeSeconds()-Start;
   if (!bScreenshotRequested && FParse::Param(FCommandLine::Get(),TEXT("BossVisualAudit")) && FPSIndex==0 && CaseIndex==5 && Elapsed>.55)
   { FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/CountessBoss_PIE.png"),true,false); bScreenshotRequested=true; }
   float FeastHit=0,FeastEnd=0; Definition->FindAction(EBossAction::BloodFeast)->Stages[0].ReadHitWindow(FeastHit,FeastEnd);
   if (Cases[CaseIndex].Action==EBossAction::BloodFeast && Elapsed<FeastHit-1./FrameRates[FPSIndex])
   { bool Found=false; for (TActorIterator<ABossTelegraph> It(W);It;++It) Found=true; bMissedTelegraph|=!Found; }
   if (!A->IsActionActive() || Elapsed>10)
   {
    const float Damage=1000-Player->FindComponentByClass<UHealthComponent>()->CurrentHealth;
    Test->TestEqual(FString::Printf(TEXT("Actual damage action %d at %d FPS"),CaseIndex,FrameRates[FPSIndex]),Damage,Cases[CaseIndex].Damage);
    Test->TestTrue(TEXT("Every action includes recovery and terminates"),!A->IsActionActive() && Elapsed>=1);
    Test->TestTrue(TEXT("Observed recovery satisfies configured floor at this frame rate"),RecoveryStarted>=0 && W->GetTimeSeconds()-RecoveryStarted+2./FrameRates[FPSIndex]>=Definition->FindAction(Cases[CaseIndex].Action)->Recovery);
    Test->TestTrue(TEXT("All actions leave mesh fixed to capsule"),MeshOffset.Equals(Boss->GetMesh()->GetRelativeLocation(),.1f));
    Test->TestTrue(TEXT("Actions preserve player control rotation"),CameraRotation.Equals(Player->GetController()->GetControlRotation(),.1f));
    if (Cases[CaseIndex].Action==EBossAction::ShadowRush)
    {
     Test->TestTrue(TEXT("Rush owns a real CMC movement source"),bMotionObserved);
     const float Travel=FVector::Dist2D(StartPosition,Boss->GetActorLocation());
     Test->TestTrue(TEXT("Rush moves capsule and obeys maximum travel"),Travel>100 && Travel<=500);
    }
    if (Cases[CaseIndex].Action==EBossAction::Siphon) Test->TestEqual(TEXT("Real Siphon restores half actual damage"),Boss->FindComponentByClass<UHealthComponent>()->CurrentHealth,1011.f);
    if (Cases[CaseIndex].Action==EBossAction::BloodFeast) Test->TestFalse(TEXT("Feast warning remains until landing hit"),bMissedTelegraph);
    for (TActorIterator<ABossBloodWave> It(W);It;++It) Test->TestFalse(TEXT("Impacted wave is returned to pool"),It->IsProjectileActive());
    Test->AddInfo(FString::Printf(TEXT("Action=%d Phase=%d FPS=%d Damage=%.0f Duration=%.3f Travel=%.2f"),CaseIndex,Cases[CaseIndex].Phase,FrameRates[FPSIndex],Damage,Elapsed,FVector::Dist2D(StartPosition,Boss->GetActorLocation())));
    if (++CaseIndex<UE_ARRAY_COUNT(Cases)) State=2;
    else if (++FPSIndex<3) { CaseIndex=0; State=2; }
    else
    {
     IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("BossReadability")),true);
     FFileHelper::SaveStringToFile(BladeTrace,*(FPaths::ProjectSavedDir()/TEXT("CountessBossDesignAudit/blade_contact.csv")));
     FFileHelper::SaveStringToFile(ReadabilityTrace,*(FPaths::ProjectSavedDir()/TEXT("BossReadability/action_timeline.csv"))); Stop(); return true;
    }
   }
   if (FParse::Param(FCommandLine::Get(),TEXT("BossReadableVideo")) && FPSIndex==0 && (++VideoTick%2)==0)
   {
    // Fixed 30 Hz simulation, capture every other frame => a truthful 15 FPS, normal-speed recording.
    for (TActorIterator<ABossTelegraph> It(W);It;++It) It->SetActorHiddenInGame(true);
    GEngine->AddOnScreenDebugMessage(9851,1.f,FColor::White,FString::Printf(TEXT("BODY TELEGRAPH AUDIT | phase %d action %d | %.2fs"),Cases[CaseIndex].Phase,static_cast<int32>(Cases[CaseIndex].Action),Elapsed));
    Test->TestTrue(TEXT("Capture targets the actual PIE viewport"),CaptureBossReadabilityFrame(W,FPaths::ProjectSavedDir()/FString::Printf(TEXT("BossReadability/Frames/attack_%05d.png"),VideoFrame++)));
   }
  }
  return false;
 }
};

/** Inputs and real blade/radial collision run in PIE; no direct synthetic ApplyHit calls. */
class FCountessDefenseScenario : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;
 TWeakObjectPtr<ACountessBossCharacter> Boss; TWeakObjectPtr<ATPCCharacter> Player;
 UBossDefinition* Original=nullptr;
 int32 Step=0,Case=0,Phase=1,Rate=0,Hits=0,Parries=0,Blocks=0;
 const int32 Rates[3]={30,60,120};
 double Started=0,ReactionAt=-1,RecoveryAt=-1,WallStart=FPlatformTime::Seconds();
 bool Input=false,Counter=false,CounterHit=false,SawInvulnerability=false,SawRecoil=false;
 float FrozenYaw=0,CounterHealth=0;
 bool PreviousFixed=FApp::UseFixedTimeStep(); double PreviousDelta=FApp::GetFixedDeltaTime();
 FDelegateHandle HitHandle;
 FString Trace=TEXT("fps,phase,case,time,boss_state,action,clip_time,player_health,boss_health,recovery_elapsed\n");
 FString Label(const TCHAR* Text) const { return FString::Printf(TEXT("%d FPS phase %d defense %d: %s"),Rates[Rate],Phase,Case,Text); }
 EBossAction Action() const { return Case==2?EBossAction::Siphon:Case==4?EBossAction::DelayedSlash:EBossAction::Combo; }
 void Restore()
 {
  FApp::SetUseFixedTimeStep(PreviousFixed); FApp::SetFixedDeltaTime(PreviousDelta);
  if (Boss.IsValid() && Original) Boss->BossActions->Definition=Original;
  if (Player.IsValid()) Player->HealthComponent->OnCombatHitResolved.Remove(HitHandle);
 }
public:
 explicit FCountessDefenseScenario(FAutomationTestBase* T):Test(T){}
 ~FCountessDefenseScenario() override { Restore(); }
 bool Update() override
 {
  if (FPlatformTime::Seconds()-WallStart>240) { Test->AddError(Label(TEXT("timed out"))); return true; }
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr; if (!W || !W->HasBegunPlay()) return false;
  if (Step==0)
  {
   for (TActorIterator<ACountessBossCharacter> It(W);It;++It) { Boss=*It; break; }
   Player=Cast<ATPCCharacter>(UGameplayStatics::GetPlayerCharacter(W,0)); if (!Boss.IsValid() || !Player.IsValid()) return false;
   Original=Boss->BossActions->Definition; Boss->BossActions->Definition=DuplicateObject<UBossDefinition>(Original,Boss.Get());
   if (auto* AI=Cast<AAIController>(Boss->GetController())) { AI->GetBrainComponent()->StopLogic(TEXT("Boss player defense fixture")); AI->StopMovement(); }
   auto* Weapon=LoadObject<UWeaponDefinition>(nullptr,TEXT("/Game/Third/DataAsset/DA_TestSword.DA_TestSword"));
   auto* Fixture=DuplicateObject<UWeaponDefinition>(Weapon,Player.Get()); Fixture->Damage=25.f;
   Test->TestTrue(TEXT("Real player sword actions with transient damage fixture"),Player->EquipmentComponent->EquipWeapon(Fixture));
   Player->EquipmentComponent->SetWeaponDrawn(true);
   HitHandle=Player->HealthComponent->OnCombatHitResolved.AddLambda([this](const FCombatHitSpec&,const FCombatHitResult& R,AActor*)
   { ++Hits; Parries+=R.bParried?1:0; Blocks+=R.bBlocked?1:0; });
   W->GetWorldSettings()->MinUndilatedFrameTime=0; W->GetWorldSettings()->MaxUndilatedFrameTime=1;
   Step=1;
  }
  auto* B=Boss.Get(); auto* A=B->BossActions.Get(); auto* P=Player.Get(); auto* H=B->FindComponentByClass<UHealthComponent>();
  if (Step==1)
  {
   FApp::SetUseFixedTimeStep(true); FApp::SetFixedDeltaTime(1./Rates[Rate]);
   FCountessPIETestAccess::Prepare(*A,P,Phase);
   P->CombatComponent->SetCombatEnabled(false); P->CombatComponent->SetCombatEnabled(true);
   P->ActionComponent->ClearInputBuffers(); P->ActionComponent->SetInputSuppressed(false);
   P->GetCharacterMovement()->SetMovementMode(MOVE_Walking); P->GetCharacterMovement()->StopMovementImmediately();
   B->GetCharacterMovement()->SetMovementMode(MOVE_Walking); B->GetCharacterMovement()->StopMovementImmediately();
   B->SetActorLocationAndRotation(FVector(0,0,98),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
   P->SetActorLocationAndRotation(FVector(Case==2?190:110,0,98),FRotator(0,180,0),false,nullptr,ETeleportType::TeleportPhysics);
   P->GetController()->SetControlRotation(FRotator(0,180,0));
   P->HealthComponent->MaxHealth=1000; P->HealthComponent->SetCurrentHealth(1000); P->HealthComponent->SetEncounterInvulnerable(false);
   H->SetCurrentHealth(1400); H->SetEncounterInvulnerable(false); P->StaminaComponent->SetCurrentStamina(100);
   Hits=Parries=Blocks=0; Input=Counter=CounterHit=SawInvulnerability=SawRecoil=false; ReactionAt=RecoveryAt=-1;
   Started=W->GetTimeSeconds(); Step=2; return false;
  }
  const double T=W->GetTimeSeconds()-Started;
  if (Step==2)
  {
   if (T<.35) return false;
   if (Case==0) P->StartBlock();
   if (Case==3) A->Poise=10; // real first player hit now reaches the break threshold
   Test->TestTrue(Label(TEXT("Boss action starts")),A->TryStartAction(Action()));
   Started=W->GetTimeSeconds(); Step=3; return false;
  }
  const float Clip=FCountessPIETestAccess::ClipTime(*A);
  if (Case==0) SawRecoil|=A->GetHitReactionAlpha()>.015f;
  if (Case==1 && !Input && Clip>=.55f)
  { P->StartBlock(); Input=true; Test->TestTrue(Label(TEXT("late guard opens actual parry window")),P->CombatComponent->IsParryWindowActive()); }
  if (Case==1 && Parries>0)
  {
   if (ReactionAt<0) ReactionAt=W->GetTimeSeconds();
   const double Age=W->GetTimeSeconds()-ReactionAt;
   if (Age<.66) Test->TestFalse(Label(TEXT("parry preserves its .7 second punish window")),A->CanMove());
   if (!Counter && Age>.07)
   {
    CounterHealth=H->CurrentHealth; P->CombatComponent->CancelGuard();
    Counter=P->CombatComponent->TryParryCounter(); Test->TestTrue(Label(TEXT("successful parry enables the real counter action")),Counter);
   }
   if (Age<.7 && H->CurrentHealth<CounterHealth) CounterHit=true;
  }
  if (Case==2 && !Input && Clip>=.66f)
  {
   P->SetActorRotation(FRotator(0,90,0)); P->GetController()->SetControlRotation(FRotator(0,90,0)); P->Dash(); Input=true;
   Test->TestTrue(Label(TEXT("dodge uses real root-motion player action")),P->IsDashing());
  }
  if (Case==2 && Clip>=.80f && Clip<=.86f) SawInvulnerability|=P->ActionComponent->IsInvulnerable();
  if (Case==3 && !Input && T>=.1)
  { P->HandlePrimaryAttack(); Input=true; Test->TestNotNull(Label(TEXT("counterattack uses a saved sword definition")),P->ActionComponent->GetActiveDefinition()); }
  if (Case==3 && A->State==EBossState::PoiseBroken && ReactionAt<0) ReactionAt=W->GetTimeSeconds();
  if (Case==3 && ReactionAt>=0 && W->GetTimeSeconds()-ReactionAt<2.35)
   Test->TestEqual(Label(TEXT("poise break preserves the full 2.4 second punish window")),A->State,EBossState::PoiseBroken);
  if (Case==4 && !Input && FCountessPIETestAccess::Committed(*A))
  {
   FrozenYaw=B->GetActorRotation().Yaw; P->SetActorLocation(FVector(0,600,98),false,nullptr,ETeleportType::TeleportPhysics); Input=true;
  }
  if (Case==4 && Input && FCountessPIETestAccess::PlayingStage(*A))
   Test->TestTrue(Label(TEXT("committed heavy does not rotate after escaping to the side")),FMath::Abs(FMath::FindDeltaAngleDegrees(FrozenYaw,B->GetActorRotation().Yaw))<.2f);
  if (FCountessPIETestAccess::Recovering(*A))
  {
   if (RecoveryAt<0) RecoveryAt=W->GetTimeSeconds();
   Test->TestFalse(Label(TEXT("cannot chain a new action through recovery")),A->TryStartAction(EBossAction::DelayedSlash));
   if (Case==4 && !Counter)
   {
    P->SetActorLocationAndRotation(B->GetActorLocation()+B->GetActorForwardVector()*110.f,FRotator(0,180,0),false,nullptr,ETeleportType::TeleportPhysics);
    P->GetCharacterMovement()->StopMovementImmediately(); CounterHealth=H->CurrentHealth;
    P->HandlePrimaryAttack(); Counter=true;
   }
   if (Case==4 && H->CurrentHealth<CounterHealth) CounterHit=true;
  }
  Trace+=FString::Printf(TEXT("%d,%d,%d,%.4f,%d,%d,%.4f,%.2f,%.2f,%.4f\n"),Rates[Rate],Phase,Case,T,static_cast<int32>(A->State),static_cast<int32>(A->CurrentAction),Clip,P->HealthComponent->CurrentHealth,H->CurrentHealth,RecoveryAt<0?-1.f:static_cast<float>(W->GetTimeSeconds()-RecoveryAt));
  if (T>4.5)
  {
   Test->TestTrue(Label(TEXT("returns control after full recovery or reaction")),!A->IsActionActive() && A->CanMove());
   if (Case==0)
   {
    Test->TestEqual(Label(TEXT("every real blade contact was blocked once")),Blocks,Phase==1?2:3);
    Test->TestEqual(Label(TEXT("held guard was not a parry")),Parries,0);
    Test->TestEqual(Label(TEXT("quarter damage without canceling later combo blades")),P->HealthComponent->CurrentHealth,Phase==1?991.5f:986.f);
    Test->TestTrue(Label(TEXT("block adds a small recoil overlay")),SawRecoil);
   }
   else if (Case==1)
   {
    Test->TestEqual(Label(TEXT("one successful parry cancels remaining combo")),Parries,1);
    Test->TestEqual(Label(TEXT("no further contacts after parry")),Hits,1);
    Test->TestTrue(Label(TEXT("real parry counter lands within recoil")),CounterHit);
   }
   else if (Case==2) Test->TestTrue(Label(TEXT("dodge i-frames overlap the actual radial release")),SawInvulnerability);
   else if (Case==3) Test->TestTrue(Label(TEXT("real player blade breaks Boss poise")),ReactionAt>=0 && H->CurrentHealth<1400);
   else if (Case==4) Test->TestTrue(Label(TEXT("whiff has an animated, attackable recovery")),CounterHit && RecoveryAt>=0);
   if (Case!=0) Test->TestEqual(Label(TEXT("the successful defense takes no damage")),P->HealthComponent->CurrentHealth,1000.f);
   if (RecoveryAt>=0) Test->TestTrue(Label(TEXT("recovery never shorter than its configured floor")),W->GetTimeSeconds()-RecoveryAt>=Original->FindAction(Action())->Recovery);
   Test->AddInfo(Label(TEXT("completed")));
   if (++Case==5) { Case=0; if (++Phase==3) { Phase=1; ++Rate; } }
   Step=1;
   if (Rate==3)
   {
    FFileHelper::SaveStringToFile(Trace,*(FPaths::ProjectSavedDir()/TEXT("BossReadability/defense_timeline.csv"))); Restore(); return true;
   }
  }
  return false;
 }
};

class FCountessLifecycleScenario : public IAutomationLatentCommand
{
 FAutomationTestBase* Test; int32 State=0; double Start=0,WallStart=FPlatformTime::Seconds();
 TWeakObjectPtr<ACountessBossCharacter> B; TWeakObjectPtr<ACharacter> P; TWeakObjectPtr<AStaticMeshActor> Wall;
 bool PreviousFixed=FApp::UseFixedTimeStep(); double PreviousDelta=FApp::GetFixedDeltaTime();
 void Stop() { FApp::SetUseFixedTimeStep(PreviousFixed); FApp::SetFixedDeltaTime(PreviousDelta); }
public:
 explicit FCountessLifecycleScenario(FAutomationTestBase* InTest):Test(InTest) {}
 virtual ~FCountessLifecycleScenario() override { Stop(); }
 bool Update() override
 {
  if (FPlatformTime::Seconds()-WallStart>60) { Test->AddError(TEXT("Lifecycle scenario timed out")); Stop(); return true; }
  UWorld* W=GEditor?GEditor->PlayWorld:nullptr; if (!W || !W->HasBegunPlay()) return false;
  if (State==0)
  {
   for (TActorIterator<ACountessBossCharacter> It(W);It;++It) { B=*It; break; }
   P=UGameplayStatics::GetPlayerCharacter(W,0); if (!B.IsValid() || !P.IsValid()) return false;
   auto* D=DuplicateObject<UBossDefinition>(B->BossActions->Definition,B.Get()); for (auto& A:D->Actions) A.PhaseOneWeight=A.PhaseTwoWeight=0;
   B->BossActions->Definition=D; P->FindComponentByClass<UHealthComponent>()->SetEncounterInvulnerable(true);
   FApp::SetFixedDeltaTime(1./60); FApp::SetUseFixedTimeStep(true); State=1;
  }
  else if (State==1 && B->BossActions->State==EBossState::Combat)
  {
   auto Check=[&](FVector From,FVector Target,float Yaw,bool Expected,const TCHAR* Label)
   {
    B->SetActorLocationAndRotation(From,FRotator(0,Yaw,0),false,nullptr,ETeleportType::TeleportPhysics);
    B->GetCharacterMovement()->SetMovementMode(MOVE_Walking); P->SetActorLocation(Target,false,nullptr,ETeleportType::TeleportPhysics);
    FVector End; const bool Valid=FCountessPIETestAccess::ValidateRush(*B->BossActions,End);
    Test->TestEqual(Label,Valid,Expected); return End;
   };
   Check(FVector(-800,-400,98),FVector(-300,-400,98),0,true,TEXT("Flat-floor rush has valid navigation and ground"));
   const FVector WallEnd=Check(FVector(450,300,98),FVector(1000,300,98),0,true,TEXT("Wall trims a rush to the reachable side"));
   Test->TestTrue(TEXT("Rush destination remains in front of wall, including capsule radius"),WallEnd.X<570);
   const FVector StepEnd=Check(FVector(-450,400,98),FVector(-450,900,98),90,true,TEXT("Low step yields a collision-checked destination"));
   Test->TestTrue(TEXT("Low-step destination stays finite and grounded"),!StepEnd.ContainsNaN() && FMath::Abs(StepEnd.Z-98)<50);
   Check(FVector(2200,-600,98),FVector(2700,-600,98),0,false,TEXT("Cliff/unreachable destination is rejected"));
   auto* Ramp=W->SpawnActor<AStaticMeshActor>(FVector(-900,-900,0),FRotator(12,0,0));
   Ramp->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
   Ramp->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
   Ramp->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll")); Ramp->SetActorScale3D(FVector(6,6,.2f));
   B->SetActorLocationAndRotation(FVector(-900,-900,110),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
   P->SetActorLocation(FVector(-700,-900,110),false,nullptr,ETeleportType::TeleportPhysics);
   auto* Definition=B->BossActions->Definition.Get();
   auto* Siphon=Definition->Actions.FindByPredicate([](const FBossActionDefinition& A){return A.Id==EBossAction::Siphon;});
   Siphon->PhaseOneWeight=1;
   Test->TestTrue(TEXT("Slope warning action starts"),B->BossActions->TryStartAction(EBossAction::Siphon));
   FCountessPIETestAccess::TickWarning(*B->BossActions);
   for (TActorIterator<ABossTelegraph> It(W);It;++It)
   {
    Test->TestTrue(TEXT("Warning keeps the terrain normal after its follow update"),FVector::DotProduct(It->GetActorUpVector(),Ramp->GetActorUpVector())>.999f);
    Test->TestTrue(TEXT("Warning remains just above the sloped ground"),It->GetActorLocation().Z>10 && It->GetActorLocation().Z<17);
   }
   B->BossActions->CancelAction(); Siphon->PhaseOneWeight=0; Ramp->Destroy();
   B->SetActorLocationAndRotation(B->BossActions->GetHomeLocation(),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
   B->GetCharacterMovement()->StopMovementImmediately(); B->GetCharacterMovement()->DisableMovement();
   P->SetActorLocation(FVector(700,0,98),false,nullptr,ETeleportType::TeleportPhysics);
   Wall=W->SpawnActor<AStaticMeshActor>(FVector(350,0,200),FRotator::ZeroRotator);
   Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
   Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
   Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll")); Wall->SetActorScale3D(FVector(.5f,10,4));
   Start=W->GetTimeSeconds(); State=2;
  }
  else if (State==2 && W->GetTimeSeconds()-Start>1.5)
  {
   Test->TestTrue(TEXT("Real behavior-tree service updates encounter context"),B->BossActions->GetContextRevision()>10);
   Test->TestTrue(TEXT("Short occlusion retains encounter target"),B->BossActions->GetTarget()==P.Get() && B->BossActions->IsEncounterActive());
   Test->TestFalse(TEXT("Short occlusion is actually detected"),B->BossActions->HasTargetLOS()); State=3;
  }
  else if (State==3 && W->GetTimeSeconds()-Start>5.5)
  {
   Test->TestEqual(TEXT("Five-second occlusion resets encounter at home"),B->BossActions->State,EBossState::Dormant);
   Test->TestNull(TEXT("Reset clears the target"),B->BossActions->GetTarget());
   TArray<UUserWidget*> Widgets; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Widgets,UBossStatusWidget::StaticClass(),true);
   Test->TestEqual(TEXT("Disengage removes Boss HUD"),Widgets.Num(),0);
   Wall->Destroy(); B->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
   P->SetActorLocation(FVector(-600,0,98),false,nullptr,ETeleportType::TeleportPhysics); Start=W->GetTimeSeconds(); State=4;
  }
  else if (State==4 && W->GetTimeSeconds()-Start>1.15)
  {
   Test->TestEqual(TEXT("Second encounter uses short preparation, not long intro"),B->BossActions->State,EBossState::Combat);
   B->BossActions->Phase=2; B->BossActions->Poise=1; B->FindComponentByClass<UHealthComponent>()->SetCurrentHealth(100);
   B->SetActorLocation(FVector(400,-100,98),false,nullptr,ETeleportType::TeleportPhysics);
   P->FindComponentByClass<UHealthComponent>()->SetCurrentHealth(0); Start=W->GetTimeSeconds(); State=5;
  }
  else if (State==5 && W->GetTimeSeconds()-Start>3)
  {
   Test->TestEqual(TEXT("Player death drives actual AI return-home reset"),B->BossActions->State,EBossState::Dormant);
   Test->TestTrue(TEXT("Return-home uses real navigation to spawn point"),FVector::Dist2D(B->GetActorLocation(),B->BossActions->GetHomeLocation())<80);
   Test->TestEqual(TEXT("Rewar resets phase one"),B->BossActions->Phase,1);
   Test->TestEqual(TEXT("Rewar resets poise"),B->BossActions->Poise,100.f);
   Test->TestEqual(TEXT("Rewar resets full health"),B->FindComponentByClass<UHealthComponent>()->CurrentHealth,1500.f);
   for (TActorIterator<ABossTelegraph> It(W);It;++It) Test->AddError(TEXT("Reset leaked a telegraph"));
   for (TActorIterator<ABossBloodWave> It(W);It;++It) Test->TestFalse(TEXT("Reset leaves no live blood wave"),It->IsProjectileActive());
   Stop(); return true;
  }
  return false;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCountessBossPIETest,"ThirdPerson.Boss.PIE.EncounterMovementDeath",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCountessBossPIETest::RunTest(const FString&)
{
 FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Third/Bosses/Countess/Maps/L_CountessBossTest"));
 // Legacy lifecycle fixtures keep their original radius/LOS policy and campaign kill counter.
 auto* FixtureWorld=GEditor->GetEditorWorldContext().World();
 FixtureWorld->GetWorldSettings()->DefaultGameMode=LoadClass<AGameModeBase>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C"));
 for (TActorIterator<ACountessBossCharacter> It(FixtureWorld); It; ++It) It->BossActions->ArenaBoundary=nullptr;
 for (TActorIterator<APlayerStart> It(FixtureWorld); It; ++It) if (It->PlayerStartTag==TEXT("ArenaArrival")) It->Destroy();
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCountessPIEScenario>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCountessBossActionsPIETest,"ThirdPerson.Boss.PIE.SixActionsAt30_60_120",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCountessBossActionsPIETest::RunTest(const FString&)
{
 FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Third/Bosses/Countess/Maps/L_CountessBossTest"));
 // Legacy lifecycle fixtures keep their original radius/LOS policy and campaign kill counter.
 auto* FixtureWorld=GEditor->GetEditorWorldContext().World();
 FixtureWorld->GetWorldSettings()->DefaultGameMode=LoadClass<AGameModeBase>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C"));
 for (TActorIterator<ACountessBossCharacter> It(FixtureWorld); It; ++It) It->BossActions->ArenaBoundary=nullptr;
 for (TActorIterator<APlayerStart> It(FixtureWorld); It; ++It) if (It->PlayerStartTag==TEXT("ArenaArrival")) It->Destroy();
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCountessActionScenario>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCountessBossDefensePIETest,"ThirdPerson.Boss.Readability.PIE.PlayerDefenseAt30_60_120",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCountessBossDefensePIETest::RunTest(const FString&)
{
 FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Third/Bosses/Countess/Maps/L_CountessBossTest"));
 // Legacy lifecycle fixtures keep their original radius/LOS policy and campaign kill counter.
 auto* FixtureWorld=GEditor->GetEditorWorldContext().World();
 FixtureWorld->GetWorldSettings()->DefaultGameMode=LoadClass<AGameModeBase>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C"));
 for (TActorIterator<ACountessBossCharacter> It(FixtureWorld); It; ++It) It->BossActions->ArenaBoundary=nullptr;
 for (TActorIterator<APlayerStart> It(FixtureWorld); It; ++It) if (It->PlayerStartTag==TEXT("ArenaArrival")) It->Destroy();
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCountessDefenseScenario>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCountessBossLifecyclePIETest,"ThirdPerson.Boss.PIE.NavigationLOSResetReentry",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCountessBossLifecyclePIETest::RunTest(const FString&)
{
 FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Third/Bosses/Countess/Maps/L_CountessBossTest"));
 // Legacy lifecycle fixtures keep their original radius/LOS policy and campaign kill counter.
 auto* FixtureWorld=GEditor->GetEditorWorldContext().World();
 FixtureWorld->GetWorldSettings()->DefaultGameMode=LoadClass<AGameModeBase>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C"));
 for (TActorIterator<ACountessBossCharacter> It(FixtureWorld); It; ++It) It->BossActions->ArenaBoundary=nullptr;
 for (TActorIterator<APlayerStart> It(FixtureWorld); It; ++It) if (It->PlayerStartTag==TEXT("ArenaArrival")) It->Destroy();
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCountessLifecycleScenario>(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
