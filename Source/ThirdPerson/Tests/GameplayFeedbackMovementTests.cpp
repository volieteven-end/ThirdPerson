#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Misc/CommandLine.h"
#include "Kismet/GameplayStatics.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimMontage.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "InputMappingContext.h"
#include "../Character/TPCCharacter.h"
#include "../Character/TPCPlayerController.h"
#include "../Animation/TPCAnimInstance.h"
#include "../Components/ActionComponent.h"
#include "../Components/CombatComponent.h"
#include "../Components/StaminaComponent.h"
#include "../Components/HealthComponent.h"
#include "../AI/EnemyCharacter.h"
#include "../Boss/CountessBossCharacter.h"
#include "../Boss/BossActionComponent.h"

namespace FeedbackMovement
{
class FScenario : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TWeakObjectPtr<ATPCCharacter> Player;
    TWeakObjectPtr<ATPCPlayerController> PC;
    TWeakObjectPtr<AEnemyCharacter> Enemy;
    TWeakObjectPtr<ACountessBossCharacter> Boss;
    TWeakObjectPtr<ACameraActor> AuditCamera;
    int32 CaptureIndex=0;
    int32 Stage=0, Case=0, Rate=0, Step=0;
    const int32 FPS[3]={30,60,120};
    double Start=0, Contact=-1, WallStart=FPlatformTime::Seconds();
    bool PreviousFixed=FApp::UseFixedTimeStep(); double PreviousDelta=FApp::GetFixedDeltaTime();
    bool SawDodge=false, SawSprint=false, SawAir=false, SawLanding=false, SawHeldContact=false;
    FVector ContactPosition;
    FString Trace=TEXT("fps,case,time,step,state,speed,x,y,z,sprint,dodge,input_paused,left_foot_x,left_foot_z,right_foot_x,right_foot_z\n");
    FString Label(const TCHAR* Text) const { return FString::Printf(TEXT("%d FPS case %d: %s"),FPS[Rate],Case,Text); }
    void Key(FKey K,bool Down)
    {
        if (PC.IsValid()) PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,Down?IE_Pressed:IE_Released,Down?1.f:0.f));
    }
    void Release()
    {
        for (const FKey K:{EKeys::W,EKeys::LeftShift,EKeys::LeftAlt,EKeys::C,EKeys::SpaceBar}) Key(K,false);
        if (Player.IsValid()) { Player->CancelSprintOrDodgeInput(); Player->ClearMoveInput(); Player->StopCrouch(); Player->EndJump(); }
    }
    void Stop()
    {
        Release();
        FApp::SetUseFixedTimeStep(PreviousFixed); FApp::SetFixedDeltaTime(PreviousDelta);
        FFileHelper::SaveStringToFile(Trace,*(FPaths::ProjectSavedDir()/TEXT("GameplayFeedback/movement_trace.csv")));
    }
    void Next(UWorld* W)
    {
        Release(); if (Enemy.IsValid()) Enemy->Destroy(); if (Boss.IsValid()) Boss->Destroy();
        Test->AddInfo(Label(TEXT("completed")));
        if (++Case==12) { Case=0; ++Rate; }
        Stage=1; Start=W->GetTimeSeconds(); Step=0; CaptureIndex=0;
    }
public:
    explicit FScenario(FAutomationTestBase* T):Test(T){}
    ~FScenario() override { Stop(); }
    bool Update() override
    {
        if (Rate==3) return true;
        if (FPlatformTime::Seconds()-WallStart>180) { Test->AddError(Label(TEXT("wall timeout"))); return true; }
        UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr; if (!W || !W->HasBegunPlay()) return false;
        if (Stage==0)
        {
            Player=Cast<ATPCCharacter>(UGameplayStatics::GetPlayerCharacter(W,0));
            PC=Cast<ATPCPlayerController>(UGameplayStatics::GetPlayerController(W,0));
            if (!Player.IsValid() || !PC.IsValid()) return false;
            for (TActorIterator<AEnemyCharacter> It(W);It;++It) { if (It->GetController()) It->GetController()->Destroy(); It->Destroy(); }
            auto* P=Player.Get(); P->bEnableRootMotionTurn=false;
            P->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
            P->HealthComponent->MaxHealth=10000.f; P->HealthComponent->SetCurrentHealth(10000.f);
            int32 ShiftMappings=0,AltMappings=0,DashMappings=0;
            for (const auto& M:P->DefaultMappingContext->GetMappings())
            {
                if (M.Key==EKeys::LeftShift && M.Action==P->SprintAction) ++ShiftMappings;
                if (M.Key==EKeys::LeftAlt || M.Key==EKeys::RightAlt) ++AltMappings;
                if (M.Action==P->DashAction) ++DashMappings;
            }
            Test->TestEqual(TEXT("Shift has exactly one shared mapping"),ShiftMappings,1);
            Test->TestEqual(TEXT("Alt is unused"),AltMappings,0);
            Test->TestEqual(TEXT("No second independent dash key remains"),DashMappings,0);
            if (FParse::Param(FCommandLine::Get(),TEXT("FeedbackVisualAudit")))
            {
                AuditCamera=W->SpawnActor<ACameraActor>(); AuditCamera->GetCameraComponent()->FieldOfView=50.f;
                PC->SetViewTarget(AuditCamera.Get());
            }
            Start=W->GetTimeSeconds(); Stage=1;
        }
        auto* P=Player.Get(); auto* M=P->GetCharacterMovement(); auto* Anim=P->GetMesh()->GetAnimInstance();
        if (Stage==1)
        {
            FApp::SetUseFixedTimeStep(true); FApp::SetFixedDeltaTime(1./FPS[Rate]);
            if (W->GetTimeSeconds()-Start<.3) return false;
            P->StaminaComponent->SetCurrentStamina(100.f);
            P->SetActorLocationAndRotation(FVector(0,-900,98),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
            PC->SetControlRotation(FRotator::ZeroRotator); M->StopMovementImmediately();
            SawDodge=SawSprint=SawAir=SawLanding=SawHeldContact=false; Contact=-1;
            Stage=2; Start=W->GetTimeSeconds();
            if (Case==0 || Case==1) { Key(EKeys::W,true); Key(EKeys::LeftShift,true); }
            if (Case==2) Key(EKeys::LeftAlt,true);
            if (Case==3 || Case==4) Key(EKeys::LeftShift,true);
            if (Case==5 || Case==6 || Case==7) { if (Case==5) Key(EKeys::W,true); Key(EKeys::SpaceBar,true); }
            if (Case==8) Key(EKeys::W,true);
            if (Case==9) Key(EKeys::C,true);
            if (Case==10)
            {
                auto* Class=LoadClass<AEnemyCharacter>(nullptr,TEXT("/Game/Third/Character/BP_EnemyCharacter.BP_EnemyCharacter_C"));
                Enemy=W->SpawnActor<AEnemyCharacter>(Class,FVector(500,-900,98),FRotator::ZeroRotator);
                if (!Enemy.IsValid()) { Test->AddError(TEXT("NPC spawn failed")); return true; }
                if (auto* AI=Cast<AAIController>(Enemy->GetController())) if (AI->GetBrainComponent()) AI->GetBrainComponent()->StopLogic(TEXT("Isolate recoil timer"));
                Enemy->ApplyParryStagger(P);
            }
            if (Case==11)
            {
                auto* Class=LoadClass<ACountessBossCharacter>(nullptr,TEXT("/Game/Third/Bosses/Countess/BP_CountessBoss.BP_CountessBoss_C"));
                Boss=W->SpawnActor<ACountessBossCharacter>(Class,FVector(180,-900,98),FRotator::ZeroRotator);
                if (!Boss.IsValid()) { Test->AddError(TEXT("Boss parry fixture spawn failed")); return true; }
                auto* D=DuplicateObject<UBossDefinition>(Boss->BossActions->Definition,Boss.Get());
                for (auto& A:D->Actions) { A.PhaseOneWeight=A.PhaseTwoWeight=A.Id==EBossAction::Combo?1.f:0.f; A.Cooldown=.1f; }
                Boss->BossActions->Definition=D; Boss->StartEncounter(P);
            }
        }
        const double T=W->GetTimeSeconds()-Start;
        const FName State=Anim->GetCurrentStateName(Anim->GetStateMachineIndex(TEXT("Locomotion")));
        SawDodge|=P->IsDashing(); SawSprint|=P->bIsSprinting; SawAir|=M->IsFalling(); SawLanding|=State==TEXT("JumpEnd");
        const FVector L=P->GetActorLocation(), LF=P->GetMesh()->GetSocketLocation(TEXT("foot_l")), RF=P->GetMesh()->GetSocketLocation(TEXT("foot_r"));
        Trace+=FString::Printf(TEXT("%d,%d,%.4f,%d,%s,%.2f,%.2f,%.2f,%.2f,%d,%d,%d,%.2f,%.2f,%.2f,%.2f\n"),
            FPS[Rate],Case,T,Step,*State.ToString(),M->Velocity.Size2D(),L.X,L.Y,L.Z,P->bIsSprinting,P->IsDashing(),P->IsLocomotionInputPaused(),LF.X,LF.Z,RF.X,RF.Z);
        if (AuditCamera.IsValid())
        {
            const FVector CameraPosition=L+FVector(280,-500,180);
            AuditCamera->SetActorLocationAndRotation(CameraPosition,(L+FVector(0,0,-20)-CameraPosition).Rotation());
        }
        if (Rate==1 && AuditCamera.IsValid() && Case==9 && CaptureIndex<5 && T>=.17+CaptureIndex*.05)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("GameplayFeedback")/
                FString::Printf(TEXT("CrouchEntry_%02d.png"),CaptureIndex),false,false); ++CaptureIndex;
        }
        if (Case==0)
        {
            if (Step==0 && T>=.07) { Test->TestFalse(Label(TEXT("short press does not start sprint")),SawSprint); Key(EKeys::LeftShift,false); Step=1; }
            if (T>1.5) { Test->TestTrue(Label(TEXT("short Shift release starts root-motion dodge through Enhanced Input")),SawDodge); Test->TestFalse(Label(TEXT("tap never also sprinted")),SawSprint); Next(W); return false; }
        }
        else if (Case==1)
        {
            if (Step==0 && T>.55) { Test->TestTrue(Label(TEXT("held Shift starts sprint")),P->bIsSprinting); Test->TestEqual(Label(TEXT("sprint speed")),M->MaxWalkSpeed,650.f); Key(EKeys::LeftShift,false); Step=1; }
            if (T>.8) { Test->TestFalse(Label(TEXT("hold/release never dodges")),SawDodge); Test->TestFalse(Label(TEXT("release stops sprint")),P->bIsSprinting); Test->TestEqual(Label(TEXT("walk speed restored")),M->MaxWalkSpeed,450.f); Next(W); return false; }
        }
        else if (Case==2)
        {
            if (T>.2) { Test->TestFalse(Label(TEXT("Alt does not dodge")),SawDodge); Test->TestFalse(Label(TEXT("Alt does not sprint")),SawSprint); Next(W); return false; }
        }
        else if (Case==3)
        {
            if (Step==0 && T>.35) { Test->TestFalse(Label(TEXT("held stationary Shift is not sprinting")),SawSprint); Test->TestEqual(Label(TEXT("stationary Shift spends no stamina")),P->StaminaComponent->GetCurrentStamina(),100.f); Key(EKeys::W,true); Step=1; }
            if (T>.6) { Test->TestTrue(Label(TEXT("W after held Shift starts sprint without repressing Shift")),P->bIsSprinting); Test->TestFalse(Label(TEXT("stationary hold never dodges")),SawDodge); Next(W); return false; }
        }
        else if (Case==4)
        {
            if (Step==0 && T>.04) { PC->ToggleInventory(); Step=1; }
            if (Step==1 && T>.15) { Key(EKeys::LeftShift,false); Step=2; }
            if (Step==2 && T>.3) { PC->ToggleInventory(); Step=3; }
            if (T>.5) { Test->TestFalse(Label(TEXT("menu discards pending Shift tap")),SawDodge); Test->TestFalse(Label(TEXT("menu clears held sprint")),SawSprint); Next(W); return false; }
        }
        else if (Case==5 || Case==6 || Case==7)
        {
            if (Step==0 && T>.1) { Key(EKeys::SpaceBar,false); Step=1; }
            if (SawAir && M->IsMovingOnGround() && Contact<0) { Contact=T; ContactPosition=L; }
            if (Contact>=0)
            {
                const double Since=T-Contact;
                if (Rate==1 && AuditCamera.IsValid() && Case==5 && CaptureIndex<5 && Since>=CaptureIndex*.05)
                {
                    FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("GameplayFeedback")/
                        FString::Printf(TEXT("LandingMove_%02d.png"),CaptureIndex),false,false); ++CaptureIndex;
                }
                if (Since<.05 && P->IsLocomotionInputPaused()) { SawHeldContact=true; Test->TestTrue(Label(TEXT("feet remain planted during contact buffer")),FVector::Dist2D(ContactPosition,L)<1.5f); }
                if (Case==5 && Since>.28)
                {
                    Test->TestTrue(Label(TEXT("landing pose was actually evaluated")),SawLanding);
                    Test->TestTrue(Label(TEXT("landing has brief contact, not instant slide")),SawHeldContact);
                    Test->TestEqual(Label(TEXT("held W blends to Ground well before the .867s clip ends")),State,FName(TEXT("Ground")));
                    Test->TestTrue(Label(TEXT("held W physically moves after short contact")),FVector::Dist2D(ContactPosition,L)>3.f); Next(W); return false;
                }
                if (Case==6)
                {
                    if (Step==1 && Since>.28) { Test->TestEqual(Label(TEXT("idle landing retains recovery animation")),State,FName(TEXT("JumpEnd"))); Step=2; }
                    if (Since>1.) { Test->TestEqual(Label(TEXT("idle landing eventually exits")),State,FName(TEXT("Ground"))); Next(W); return false; }
                }
                if (Case==7)
                {
                    if (Step==1 && Since>.02) { Key(EKeys::SpaceBar,true); Step=2; }
                    if (Since>.15) { Test->TestTrue(Label(TEXT("re-jump interrupts short landing buffer")),M->IsFalling()); Next(W); return false; }
                }
            }
            if (T>4.) { Test->AddError(Label(TEXT("jump/landing did not complete"))); Next(W); return false; }
        }
        else if (Case==8)
        {
            if (Step==0 && T>.3) { Key(EKeys::C,true); Step=1; }
            if (Step==1 && T>.5) { Test->TestEqual(Label(TEXT("moving crouch blends straight to crouch locomotion")),State,FName(TEXT("Crouch"))); Test->TestTrue(Label(TEXT("crouch has actual walking speed")),M->Velocity.Size2D()>20.f); Key(EKeys::C,false); Step=2; }
            if (T>.75) { Test->TestEqual(Label(TEXT("moving uncrouch blends back to walking")),State,FName(TEXT("Ground"))); Next(W); return false; }
        }
        else if (Case==9)
        {
            if (Step==0 && T>.18) { Test->TestEqual(Label(TEXT("stationary crouch keeps entry animation")),State,FName(TEXT("CrouchIn"))); Key(EKeys::W,true); Step=1; }
            if (T>.4) { Test->TestEqual(Label(TEXT("W during entry blends to crouch walking before .5s")),State,FName(TEXT("Crouch"))); Test->TestTrue(Label(TEXT("crouch entry is not a full input lock")),M->Velocity.Size2D()>20.f); Next(W); return false; }
        }
        else if (Case==10)
        {
            if (Step==0 && T>1.1) { Test->TestTrue(Label(TEXT("NPC remains staggered beyond original one-second window")),Enemy->IsParryStaggered()); Test->TestTrue(Label(TEXT("NPC recoil montage still plays instead of idle during stagger")),Enemy->GetMesh()->GetAnimInstance()->Montage_IsPlaying(Enemy->ParryStaggerMontage)); Step=1; }
            if (T>1.5) { Test->TestFalse(Label(TEXT("NPC exits the new 1.4s stagger")),Enemy->IsParryStaggered()); Test->TestTrue(Label(TEXT("NPC walking is restored")),Enemy->GetCharacterMovement()->MovementMode==MOVE_Walking); Next(W); return false; }
        }
        else if (Case==11)
        {
            auto* A=Boss->BossActions.Get();
            auto EnableAttack=[A](bool Enabled)
            { for (auto& Move:A->Definition->Actions) Move.PhaseOneWeight=Move.PhaseTwoWeight=(Enabled && Move.Id==EBossAction::Combo)?1.f:0.f; };
            if (Step==0 && A->State==EBossState::Action && A->GetStateElapsed()>.08)
            {
                EnableAttack(false); Boss->ApplyParryStagger(P); Contact=T; Step=1;
                Test->TestEqual(Label(TEXT("one Boss parry removes 40 poise")),A->Poise,60.f);
                Test->TestEqual(Label(TEXT("saved Boss light recoil is .7s")),A->GetDefinition()->ParryRecoil,.7f);
                Test->TestEqual(Label(TEXT("saved full poise break remains 2.4s")),A->GetDefinition()->BreakDuration,2.4f);
            }
            if (Step==1 && T-Contact>.4) { Test->TestFalse(Label(TEXT("Boss recoil remains locked after old .3s window")),A->CanMove()); Step=2; }
            if (Step==2 && T-Contact>.8)
            {
                Test->TestTrue(Label(TEXT("Boss navigation recovers after .7s recoil")),A->CanMove());
                EnableAttack(true); Step=3;
            }
            if (Step==3 && A->State==EBossState::Action && A->GetStateElapsed()>.08)
            {
                EnableAttack(false); Boss->ApplyParryStagger(P); Contact=T; Step=4;
                Test->TestEqual(Label(TEXT("second real action parry leaves 20 poise")),A->Poise,20.f);
            }
            if (Step==4 && T-Contact>.8) { EnableAttack(true); Step=5; }
            if (Step==5 && A->State==EBossState::Action && A->GetStateElapsed()>.08)
            {
                EnableAttack(false); Boss->ApplyParryStagger(P); Contact=T; Step=6;
                Test->TestEqual(Label(TEXT("three real action parries trigger full poise break")),A->State,EBossState::PoiseBroken);
            }
            if (Step==6 && T-Contact>2.1) { Test->TestEqual(Label(TEXT("full break remains active at 2.1s")),A->State,EBossState::PoiseBroken); Step=7; }
            if (Step==7 && T-Contact>2.6) { Test->TestEqual(Label(TEXT("full break restores combat after 2.4s")),A->State,EBossState::Combat); Next(W); return false; }
            if (T>16.) { Test->AddError(Label(TEXT("Boss parry scenario timed out"))); Next(W); return false; }
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPCFeedbackMovementTest,"ThirdPerson.Feedback.PIE.InputLocomotionAndParry",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTPCFeedbackMovementTest::RunTest(const FString&)
{
    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Third/Bosses/Countess/Maps/L_CountessBossTest"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FeedbackMovement::FScenario(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
