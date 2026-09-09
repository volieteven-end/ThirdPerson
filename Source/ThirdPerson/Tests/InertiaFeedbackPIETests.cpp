#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "InputKeyEventArgs.h"
#include "UnrealClient.h"
#include "../Character/TPCCharacter.h"
#include "../Character/TPCPlayerController.h"
#include "../AI/EnemyCharacter.h"
#include "../Components/ActionComponent.h"
#include "../Components/CombatComponent.h"
#include "../Components/StaminaComponent.h"
#include "../Components/HealthComponent.h"
#include "../Weapons/WeaponProjectile.h"

namespace InertiaFeedback
{
class FScenario : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TWeakObjectPtr<ATPCCharacter> Player;
    TWeakObjectPtr<ATPCPlayerController> PC;
    TWeakObjectPtr<AEnemyCharacter> Enemy;
    TWeakObjectPtr<AWeaponProjectile> Arrow;
    TWeakObjectPtr<ACameraActor> Camera;
    int32 Rate=0, Case=0, Stage=0, Step=0, Frame=0, ArrowSamples=0;
    const int32 FPS[3]={30,60,120};
    double Start=0, AttackAt=-1, JumpAt=-1, Contact=-1, WallStart=FPlatformTime::Seconds();
    bool PreviousFixed=FApp::UseFixedTimeStep(); double PreviousDelta=FApp::GetFixedDeltaTime();
    bool SawAir=false, SawAttack=false;
    float AirSpeed=0, BeforeAttackSpeed=0, BeforeAttackZ=0, MinimumAttackSpeed=MAX_flt, MinimumContactSpeed=MAX_flt;
    FVector ContactPosition;
    FString Trace=TEXT("fps,case,time,step,x,y,z,vx,vy,vz,attack,action,position,input_paused\n");
    FString Label(const FString& Text) const { return FString::Printf(TEXT("%d FPS case %d: %s"),FPS[Rate],Case,*Text); }
    void Key(FKey K,bool Down) { if (PC.IsValid()) PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,Down?IE_Pressed:IE_Released,Down?1.f:0.f)); }
    void Release()
    {
        for (FKey K:{EKeys::W,EKeys::SpaceBar,EKeys::F,EKeys::LeftMouseButton}) Key(K,false);
        if (Player.IsValid()) { Player->CancelSprintOrDodgeInput(); Player->ClearMoveInput(); Player->EndJump(); }
    }
    void Next(UWorld* W)
    {
        Release(); if (Enemy.IsValid()) Enemy->Destroy(); if (Arrow.IsValid()) Arrow->Destroy();
        if (Player.IsValid()) Player->CombatComponent->CancelActiveAttack();
        Test->AddInfo(Label(TEXT("completed")));
        if (++Case==5) { Case=0; ++Rate; }
        Stage=1; Start=W->GetTimeSeconds();
    }
    void Capture(const FString& Name, FVector Focus, bool Close=false)
    {
        if (!Camera.IsValid() || Rate!=1) return;
        const FVector Location=Focus+(Close?FVector(0,-300,200):FVector(170,-470,160));
        Camera->SetActorLocationAndRotation(Location,(Focus-Location).Rotation());
        if (PC->PlayerCameraManager) PC->PlayerCameraManager->SetGameCameraCutThisFrame();
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("InertiaFeedback")/Name,false,false);
    }
public:
    explicit FScenario(FAutomationTestBase* T):Test(T){}
    ~FScenario() override
    {
        Release(); FApp::SetUseFixedTimeStep(PreviousFixed); FApp::SetFixedDeltaTime(PreviousDelta);
        FFileHelper::SaveStringToFile(Trace,*(FPaths::ProjectSavedDir()/TEXT("InertiaFeedback/movement_trace.csv")));
    }
    bool Update() override
    {
        if (Rate==3) return true;
        if (FPlatformTime::Seconds()-WallStart>180) { Test->AddError(Label(TEXT("wall timeout"))); return true; }
        UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr; if (!W || !W->HasBegunPlay()) return false;
        if (Stage==0)
        {
            Player=Cast<ATPCCharacter>(UGameplayStatics::GetPlayerCharacter(W,0)); PC=Cast<ATPCPlayerController>(UGameplayStatics::GetPlayerController(W,0));
            if (!Player.IsValid() || !PC.IsValid()) return false;
            for (TActorIterator<AEnemyCharacter> It(W);It;++It) { if (It->GetController()) It->GetController()->Destroy(); It->Destroy(); }
            Player->bEnableRootMotionTurn=false;
            Player->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
            Player->HealthComponent->MaxHealth=10000; Player->HealthComponent->SetCurrentHealth(10000);
            if (FParse::Param(FCommandLine::Get(),TEXT("InertiaVisualAudit")))
            { Camera=W->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->FieldOfView=50.f; PC->SetViewTarget(Camera.Get()); }
            Stage=1; Start=W->GetTimeSeconds();
        }
        auto* P=Player.Get(); auto* M=P->GetCharacterMovement(); auto* A=P->ActionComponent.Get();
        if (Stage==1)
        {
            FApp::SetUseFixedTimeStep(true); FApp::SetFixedDeltaTime(1./FPS[Rate]);
            if (W->GetTimeSeconds()-Start<.6) return false;
            P->SetActorLocationAndRotation(FVector(-500,-900,98),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
            M->SetMovementMode(MOVE_Walking); M->StopMovementImmediately(); PC->SetControlRotation(FRotator::ZeroRotator);
            P->StaminaComponent->SetCurrentStamina(100); Step=Frame=ArrowSamples=0; SawAir=SawAttack=false;
            Contact=AttackAt=JumpAt=-1; MinimumAttackSpeed=MinimumContactSpeed=MAX_flt; AirSpeed=0;
            Stage=2; Start=W->GetTimeSeconds();
            if (Case<4) Key(EKeys::W,true);
            if (Case==3) Key(EKeys::SpaceBar,true);
            if (Case==4)
            {
                P->SetActorLocation(FVector(500,-900,98));
                auto* Class=LoadClass<AEnemyCharacter>(nullptr,TEXT("/Game/Third/Character/BP_EnemyRangedCharacter.BP_EnemyRangedCharacter_C"));
                Enemy=W->SpawnActor<AEnemyCharacter>(Class,FVector(-500,-900,98),FRotator::ZeroRotator);
                if (!Enemy.IsValid()) { Test->AddError(TEXT("ranged fixture spawn failed")); return true; }
                if (auto* AI=Cast<AAIController>(Enemy->GetController())) if (AI->GetBrainComponent()) AI->GetBrainComponent()->StopLogic(TEXT("Isolate authored bow release"));
                Enemy->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
                Test->TestTrue(Label(TEXT("saved ranged enemy starts real montage shot")),Enemy->FindComponentByClass<UCombatComponent>()->TryRangedAttackAt(P));
            }
        }
        const double T=W->GetTimeSeconds()-Start;
        const FVector L=P->GetActorLocation(), V=M->Velocity;
        Trace+=FString::Printf(TEXT("%d,%d,%.5f,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%d,%.4f,%d\n"),FPS[Rate],Case,T,Step,L.X,L.Y,L.Z,V.X,V.Y,V.Z,
            P->CombatComponent->IsMeleeAttackInProgress(),int32(A->GetActionState()),A->GetMontagePosition(),P->IsLocomotionInputPaused());
        SawAir|=M->IsFalling(); SawAttack|=P->CombatComponent->IsMeleeAttackInProgress();
        if (Camera.IsValid() && Case<4)
        {
            const FVector C=L+FVector(170,-470,160); Camera->SetActorLocationAndRotation(C,(L-C).Rotation());
        }
        if (Case<=2)
        {
            if (Step==0 && T>.4) { Test->TestTrue(Label(TEXT("run-up reaches speed")),V.Size2D()>400); Key(EKeys::F,true); JumpAt=T; Step=1; }
            if (Step==1 && T-JumpAt>.1)
            {
                Key(EKeys::F,false);
                if (Case==0) { BeforeAttackSpeed=V.Size2D(); BeforeAttackZ=V.Z; Key(EKeys::LeftMouseButton,true); AttackAt=T; }
                if (Case==2) Key(EKeys::W,false);
                Step=2;
            }
            if (Case==0 && AttackAt>=0)
            {
                const double Since=T-AttackAt;
                if (Since>.05) Key(EKeys::LeftMouseButton,false);
                if (M->IsFalling() && Since>.04 && Since<.3) MinimumAttackSpeed=FMath::Min(MinimumAttackSpeed,float(V.Size2D()));
                if (Step==2 && Since>.23)
                {
                    Test->TestTrue(Label(TEXT("air LMB starts attack through Enhanced Input")),SawAttack);
                    Test->TestTrue(Label(FString::Printf(TEXT("air inertia retained: before=%.2f min=%.2f"),BeforeAttackSpeed,MinimumAttackSpeed)),MinimumAttackSpeed>BeforeAttackSpeed*.75f);
                    Test->TestTrue(Label(TEXT("upward inertia follows gravity rather than being zeroed")),BeforeAttackZ>0 && FMath::Abs(V.Z-(BeforeAttackZ+M->GetGravityZ()*Since))<90.f);
                    Capture(TEXT("AirAttack.png"),L); Step=3;
                }
            }
            if (M->IsFalling()) AirSpeed=V.Size2D();
            if (SawAir && M->IsMovingOnGround() && Contact<0)
            {
                Contact=T; ContactPosition=L;
                Test->AddInfo(Label(FString::Printf(TEXT("contact speed=%.2f precontact=%.2f"),V.Size2D(),AirSpeed)));
                Test->TestTrue(Label(TEXT("landing preserves nonzero horizontal velocity on first grounded frame")),V.Size2D()>AirSpeed*.5f);
            }
            if (Contact>=0)
            {
                const double Since=T-Contact;
                if (Since<.1) MinimumContactSpeed=FMath::Min(MinimumContactSpeed,float(V.Size2D()));
                if (Case==1 && Frame<4 && Since>=Frame*.05) { Capture(FString::Printf(TEXT("Landing_%02d.png"),Frame),L); ++Frame; }
                if (Since>.2 && Step<4)
                {
                    Test->TestTrue(Label(FString::Printf(TEXT("contact decelerates without stop: min=%.2f"),MinimumContactSpeed)),MinimumContactSpeed>AirSpeed*.3f);
                    Test->TestTrue(Label(TEXT("physical carry-through during landing")),FVector::Dist2D(ContactPosition,L)>25.f);
                    if (Case==2) Test->TestTrue(Label(TEXT("released input decelerates after contact")),V.Size2D()<AirSpeed*.9f);
                    Step=4;
                }
                if (Since>.75)
                {
                    if (Case==2) Test->TestTrue(Label(TEXT("released-input landing settles without endless drift")),V.Size2D()<5.f);
                    else Test->TestTrue(Label(TEXT("held input resumes normal locomotion")),V.Size2D()>400.f);
                    Next(W); return false;
                }
            }
        }
        else if (Case==3)
        {
            if (Step==0 && T>.5) { Test->TestTrue(Label(TEXT("held Space sprint before slash")),P->bIsSprinting); Key(EKeys::LeftMouseButton,true); AttackAt=T; Step=1; }
            if (AttackAt>=0)
            {
                const double Since=T-AttackAt;
                if (Since>.05) Key(EKeys::LeftMouseButton,false);
                if (Step==1 && Since>.1) { Test->TestTrue(Label(TEXT("input selected Sprint action")),A->GetActiveDefinition() && A->GetActiveDefinition()->ActionId==TEXT("Sword.Sprint")); Step=2; }
                if (Frame<3 && Since>=.6+Frame*.15) { Capture(FString::Printf(TEXT("Sprint_%02d.png"),Frame),L); ++Frame; }
                if (Since>1.05)
                {
                    Test->TestTrue(Label(TEXT("sprint attack played")),SawAttack);
                    Test->TestFalse(Label(TEXT("sprint slash recovery finishes within one second")),P->CombatComponent->IsMeleeAttackInProgress());
                    Test->TestTrue(Label(TEXT("held W is responsive after shortened recovery")),V.Size2D()>250.f);
                    Next(W); return false;
                }
            }
        }
        else if (Case==4)
        {
            if (!Arrow.IsValid()) for (TActorIterator<AWeaponProjectile> It(W);It;++It)
                if (It->IsProjectileActive() && It->GetOwner()==Enemy.Get()) { Arrow=*It; break; }
            if (Arrow.IsValid() && !Arrow->HasImpacted())
            {
                auto* Mesh=Arrow->FindComponentByClass<UStaticMeshComponent>(); auto* PM=Arrow->FindComponentByClass<UProjectileMovementComponent>();
                if (!Mesh || !PM) { Test->AddError(TEXT("arrow components missing")); Next(W); return false; }
                FVector Min,Max; Mesh->GetLocalBounds(Min,Max);
                const FVector E=Max-Min; const FVector LocalAxis=E.Y>E.X && E.Y>E.Z?FVector::YAxisVector:FVector::XAxisVector;
                const float Alignment=FVector::DotProduct(Mesh->GetComponentTransform().TransformVectorNoScale(LocalAxis).GetSafeNormal(),PM->Velocity.GetSafeNormal());
                if (ArrowSamples==5)
                {
                    Test->AddInfo(Label(FString::Printf(TEXT("arrow shaft/velocity dot=%.4f mesh rotation=%s"),Alignment,*Mesh->GetRelativeRotation().ToString())));
                    Capture(TEXT("ArrowFlight.png"),Mesh->GetComponentTransform().TransformPosition((Min+Max)*.5f),true);
                }
                Test->TestTrue(Label(TEXT("arrow tip follows velocity, neither sideways nor tail-first")),Alignment>.98f);
                const FVector Tip=Mesh->GetComponentTransform().TransformPosition(FVector(0.f,Max.Y,0.f));
                Test->TestTrue(Label(TEXT("arrowhead coincides with swept collision, shaft trails behind")),FVector::Dist(Tip,Arrow->GetActorLocation())<1.f);
                ++ArrowSamples;
            }
            if (ArrowSamples>=8) { Next(W); return false; }
        }
        if (T>5.) { Test->AddError(Label(TEXT("scenario timeout"))); Next(W); return false; }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPCInertiaFeedbackTest,"ThirdPerson.Feedback.PIE.InertiaAndArrow",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTPCInertiaFeedbackTest::RunTest(const FString&)
{
    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Third/Bosses/Countess/Maps/L_CountessBossTest"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(InertiaFeedback::FScenario(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
