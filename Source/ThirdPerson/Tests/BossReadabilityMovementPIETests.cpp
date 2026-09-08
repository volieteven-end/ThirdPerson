#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "../Boss/CountessBossCharacter.h"
#include "../Boss/CountessBossAnimInstance.h"
#include "../Boss/BossActionComponent.h"
#include "../Components/HealthComponent.h"
#include "BossReadabilityCapture.h"

struct FCountessReadableMovementAccess
{
 static void Prepare(UBossActionComponent& A,AActor* Target)
 { A.CancelAction(); A.Target=Target; A.State=EBossState::Combat; A.RecoilUntil=0; A.CooldownUntil.Reset(); A.bPhasePending=false; A.NextMoveRequest=0; A.bStrafing=true; }
 static void Move(UBossActionComponent& A,FVector Goal) { A.MoveToGoal(Goal,180.f,true); }
};
namespace
{
class FReadableMovementScenario : public IAutomationLatentCommand
{
 FAutomationTestBase* Test;
 TWeakObjectPtr<ACountessBossCharacter> Boss;
 TWeakObjectPtr<ACharacter> Player;
 UBossDefinition* Original=nullptr;
 int32 Phase=0,Case=0,Rate=0,VideoTick=0,VideoFrame=0;
 const int32 Rates[3]={30,60,120};
 double Started=0,WallStart=FPlatformTime::Seconds();
 bool PreviousFixed=FApp::UseFixedTimeStep(); double PreviousDelta=FApp::GetFixedDeltaTime();
 bool SeenStart=false,SeenMove=false,SeenStop=false,SeenPivot=false,SeenTurn=false,SeenCircle=false,RequestedStop=false,Reversed=false,StartedAttack=false;
 int32 StartEntries=0; EBossLocomotionState PreviousState=EBossLocomotionState::Idle;
 FVector Goal,InitialLocation,MeshOffset; FString Trace=TEXT("fps,case,time,state,speed,x,y,yaw,transition_time,transition_alpha,circle_alpha\n");
 FString Label(const TCHAR* Text) const { return FString::Printf(TEXT("%d FPS locomotion case %d: %s"),Rates[Rate],Case,Text); }
 void Restore()
 {
  FApp::SetUseFixedTimeStep(PreviousFixed); FApp::SetFixedDeltaTime(PreviousDelta);
  if (Boss.IsValid() && Original) Boss->BossActions->Definition=Original;
 }
public:
 explicit FReadableMovementScenario(FAutomationTestBase* T):Test(T){}
 ~FReadableMovementScenario() override { Restore(); }
 bool Update() override
 {
  if (Rate==3) return true;
  if (FPlatformTime::Seconds()-WallStart>240) { Test->AddError(Label(TEXT("timed out"))); return true; }
  UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr; if (!W || !W->HasBegunPlay()) return false;
  if (Phase==0)
  {
   for (TActorIterator<ACountessBossCharacter> It(W);It;++It) { Boss=*It; break; }
   Player=UGameplayStatics::GetPlayerCharacter(W,0); if (!Boss.IsValid() || !Player.IsValid()) return false;
   Original=Boss->BossActions->Definition;
   auto* D=DuplicateObject<UBossDefinition>(Original,Boss.Get()); for (auto& A:D->Actions) A.PhaseOneWeight=A.PhaseTwoWeight=0;
   Boss->BossActions->Definition=D;
   Player->FindComponentByClass<UHealthComponent>()->MaxHealth=10000; Player->FindComponentByClass<UHealthComponent>()->SetCurrentHealth(10000);
   if (auto* AI=Cast<AAIController>(Boss->GetController())) { AI->GetBrainComponent()->StopLogic(TEXT("Readable animation fixture")); AI->StopMovement(); }
   if (FParse::Param(FCommandLine::Get(),TEXT("BossReadableVideo")))
   {
    const FVector Eye(700,-650,450); auto* Camera=W->SpawnActor<ACameraActor>(Eye,(FVector(0,0,80)-Eye).Rotation());
    Camera->GetCameraComponent()->SetFieldOfView(65); Cast<APlayerController>(Player->GetController())->SetViewTarget(Camera);
   }
   W->GetWorldSettings()->MinUndilatedFrameTime=0; W->GetWorldSettings()->MaxUndilatedFrameTime=1;
   MeshOffset=Boss->GetMesh()->GetRelativeLocation(); Phase=1;
  }
  auto* B=Boss.Get(); auto* A=B->BossActions.Get(); auto* Anim=Cast<UCountessBossAnimInstance>(B->GetMesh()->GetAnimInstance());
  if (!Anim) { Test->AddError(TEXT("Saved animation blueprint is not active")); return true; }
  if (Phase==1)
  {
   FApp::SetUseFixedTimeStep(true); FApp::SetFixedDeltaTime(1./Rates[Rate]);
   FCountessReadableMovementAccess::Prepare(*A,Player.Get());
   for (auto& Action:A->Definition->Actions) Action.PhaseOneWeight=Action.PhaseTwoWeight=0;
   B->SetActorLocationAndRotation(FVector(0,0,98),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
   B->GetCharacterMovement()->SetMovementMode(MOVE_Walking); B->GetCharacterMovement()->StopMovementImmediately();
   B->GetCharacterMovement()->bOrientRotationToMovement=false;
   Player->SetActorLocation(FVector(900,0,98),false,nullptr,ETeleportType::TeleportPhysics);
   if (Case>=8)
   {
    B->SetActorLocationAndRotation(FVector(0,-350,98),FRotator(0,90,0),false,nullptr,ETeleportType::TeleportPhysics);
    Player->SetActorLocation(FVector(0,0,98),false,nullptr,ETeleportType::TeleportPhysics);
   }
   InitialLocation=B->GetActorLocation(); Started=W->GetTimeSeconds(); Phase=2;
   SeenStart=SeenMove=SeenStop=SeenPivot=SeenTurn=SeenCircle=RequestedStop=Reversed=StartedAttack=false; StartEntries=0; PreviousState=EBossLocomotionState::Idle;
   return false;
  }
  const float T=W->GetTimeSeconds()-Started;
  if (Phase==2)
  {
   if (T<.3f) return false; // settle the previous montage and floor, outside measured movement
   const FVector Directions[]={FVector(1,0,0),FVector(-1,0,0),FVector(0,-1,0),FVector(0,1,0)};
   Goal=InitialLocation+Directions[FMath::Min(Case,3)]*550.f;
   if (Case==4 || Case==7) Goal=InitialLocation+FVector(550,0,0);
   if (Case<5 || Case==7) FCountessReadableMovementAccess::Move(*A,Goal);
   else if (Case<8) Player->SetActorLocation(InitialLocation+(Case==5?FVector(0,550,0):FVector(-550,0,0)),false,nullptr,ETeleportType::TeleportPhysics);
   Started=W->GetTimeSeconds(); Phase=3; return false;
  }
  const auto S=Anim->LocomotionState;
  SeenStart|=S==EBossLocomotionState::Start; SeenMove|=S==EBossLocomotionState::Moving; SeenStop|=S==EBossLocomotionState::Stop;
  SeenPivot|=S==EBossLocomotionState::Pivot; SeenTurn|=S==EBossLocomotionState::Turn;
  SeenCircle|=Anim->CircleAlpha>.08f;
  if (Case>=8)
  {
   const FVector Radial=(B->GetActorLocation()-Player->GetActorLocation()).GetSafeNormal2D();
   Goal=Player->GetActorLocation()+Radial.RotateAngleAxis(Case==8?35.f:-35.f,FVector::UpVector)*350.f;
   FCountessReadableMovementAccess::Move(*A,Goal);
  }
  if (S==EBossLocomotionState::Start && S!=PreviousState) ++StartEntries; PreviousState=S;
  if (Case<4 && T<1.3f) FCountessReadableMovementAccess::Move(*A,Goal); // real request refreshes, not new animation starts
  if (Case<4 && T>1.4f && !RequestedStop)
  { Cast<AAIController>(B->GetController())->StopMovement(); B->GetCharacterMovement()->StopMovementImmediately(); RequestedStop=true; }
  if (Case==4 && T>.85f && !Reversed)
  { Goal=InitialLocation+FVector(-550,0,0); FCountessReadableMovementAccess::Move(*A,Goal); Reversed=true; }
  if (Case==7 && T>.85f && !StartedAttack)
  {
   for (auto& Action:A->Definition->Actions) if (Action.Id==EBossAction::Combo) Action.PhaseOneWeight=Action.PhaseTwoWeight=1;
   Player->SetActorLocation(B->GetActorLocation()+B->GetActorForwardVector()*110.f,false,nullptr,ETeleportType::TeleportPhysics);
   StartedAttack=A->TryStartAction(EBossAction::Combo); Test->TestTrue(Label(TEXT("real attack can interrupt locomotion")),StartedAttack);
  }
  if (Case==7 && T>1.1f && T<1.7f) Test->TestTrue(Label(TEXT("full-body attack clears the locomotion overlay")),Anim->TransitionAlpha<.01f);
  const FVector P=B->GetActorLocation();
  Trace+=FString::Printf(TEXT("%d,%d,%.4f,%d,%.2f,%.2f,%.2f,%.2f,%.3f,%.3f,%.3f\n"),Rates[Rate],Case,T,static_cast<int32>(S),Anim->Speed,P.X,P.Y,B->GetActorRotation().Yaw,Anim->TransitionTime,Anim->TransitionAlpha,Anim->CircleAlpha);
  if (FParse::Param(FCommandLine::Get(),TEXT("BossReadableVideo")) && Rate==0 && (++VideoTick%2)==0)
  {
   IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("BossReadability/MovementFrames")),true);
   GEngine->AddOnScreenDebugMessage(9852,1.f,FColor::White,FString::Printf(TEXT("LOCOMOTION | case %d state %d | %.2fs"),Case,static_cast<int32>(S),T));
   Test->TestTrue(TEXT("Capture targets the actual PIE viewport"),CaptureBossReadabilityFrame(W,FPaths::ProjectSavedDir()/FString::Printf(TEXT("BossReadability/MovementFrames/move_%05d.png"),VideoFrame++)));
  }
  if (T>(Case>=8?5.f:3.f))
  {
   Test->TestTrue(Label(TEXT("mesh stays attached to capsule")),MeshOffset.Equals(B->GetMesh()->GetRelativeLocation(),.1f));
   if (Case<5 || Case==7)
   {
    Test->TestTrue(Label(TEXT("observed authored start")),SeenStart);
    Test->TestTrue(Label(TEXT("observed moving loop")),SeenMove);
    Test->TestTrue(Label(TEXT("navigation actually translated the body")),FVector::Dist2D(InitialLocation,P)>30.f);
    if (Case<4) { Test->TestTrue(Label(TEXT("observed authored stop")),SeenStop); Test->TestEqual(Label(TEXT("request refreshes do not restart the start animation")),StartEntries,1); }
   }
   if (Case==4) Test->TestTrue(Label(TEXT("direction reversal plays a pivot")),SeenPivot);
   if (Case==5 || Case==6) { Test->TestTrue(Label(TEXT("observed turn-in-place")),SeenTurn); Test->TestTrue(Label(TEXT("one facing owner converges on target")),FMath::Abs(A->GetFacingDelta())<4.f); }
   if (Case>=8)
   {
    Test->TestTrue(Label(TEXT("continuous orbit uses authored circle animation")),SeenCircle && SeenMove);
    Test->TestEqual(Label(TEXT("orbit request refreshes do not restart feet")),StartEntries,1);
   }
   Test->AddInfo(Label(TEXT("completed")));
   if (++Case==10) { Case=0; ++Rate; }
   Phase=1;
   if (Rate==3)
   {
    IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("BossReadability")),true);
    FFileHelper::SaveStringToFile(Trace,*(FPaths::ProjectSavedDir()/TEXT("BossReadability/locomotion_timeline.csv"))); Restore(); return true;
   }
  }
  return false;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossReadableMovementTest,"ThirdPerson.Boss.Readability.PIE.LocomotionAt30_60_120",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBossReadableMovementTest::RunTest(const FString&)
{
 FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Third/Bosses/Countess/Maps/L_CountessBossTest"));
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(FReadableMovementScenario(this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
