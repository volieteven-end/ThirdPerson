// Boss 重试反馈回归：覆盖连续死亡重启与抬高出生位置后的恢复。
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "BrainComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "../Boss/CountessBossCharacter.h"
#include "../Boss/CountessBossAIController.h"
#include "../Boss/BossActionComponent.h"
#include "../Character/TPCCharacter.h"
#include "../Character/TPCPlayerController.h"
#include "../Components/HealthComponent.h"

namespace GameplayFeedbackPIE
{
class FBossRestartScenario : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TWeakObjectPtr<ACountessBossCharacter> Boss;
    TWeakObjectPtr<ACountessBossAIController> OriginalController;
    TWeakObjectPtr<ATPCCharacter> Player;
    TWeakObjectPtr<ATPCPlayerController> PC;
    TObjectPtr<UBossDefinition> OriginalDefinition;
    int32 Stage=0, Cycle=0;
    bool bRaisedHome=false;
    double Start=0, WallStart=FPlatformTime::Seconds();
    bool PreviousFixed=FApp::UseFixedTimeStep(); double PreviousDelta=FApp::GetFixedDeltaTime();
    FVector RestartBossLocation;
    FString Trace=TEXT("cycle,stage,time,state,action,controller,brain,move_status,move_mode,boss_x,boss_y,boss_z,home_x,home_y,home_z,target,player,player_hp,context_revision\n");
    void Stop()
    {
        FApp::SetUseFixedTimeStep(PreviousFixed); FApp::SetFixedDeltaTime(PreviousDelta);
        FFileHelper::SaveStringToFile(Trace,*(FPaths::ProjectSavedDir()/(bRaisedHome ? TEXT("GameplayFeedback/boss_raised_restart_trace.csv") : TEXT("GameplayFeedback/boss_restart_trace.csv"))));
        if (Boss.IsValid() && OriginalDefinition) Boss->BossActions->Definition=OriginalDefinition;
        if (PC.IsValid()) PC->SetPause(false);
    }
    void NextStage(int32 Value,UWorld* W) { Stage=Value; Start=W->GetTimeSeconds(); }
    void ConfigureAction()
    {
        auto* D=DuplicateObject<UBossDefinition>(OriginalDefinition,Boss.Get());
        const EBossAction Wanted=static_cast<EBossAction>(1+Cycle%6);
        float Distance=170.f;
        for (auto& A:D->Actions)
        {
            A.PhaseOneWeight=A.PhaseTwoWeight=A.Id==Wanted?1.f:0.f;
            if (A.Id==Wanted) Distance=FMath::Lerp(A.MinRange,A.MaxRange,.55f);
        }
        Boss->BossActions->Definition=D;
        Player->HealthComponent->MaxHealth=10000.f; Player->HealthComponent->SetCurrentHealth(10000.f);
        Player->SetActorLocation(Boss->GetActorLocation()+FVector(-FMath::Max(130.f,Distance),0,0),false,nullptr,ETeleportType::TeleportPhysics);
    }
public:
    explicit FBossRestartScenario(FAutomationTestBase* InTest, bool bRaised=false):Test(InTest),bRaisedHome(bRaised){}
    ~FBossRestartScenario() override { Stop(); }
    bool Update() override
    {
        if (FPlatformTime::Seconds()-WallStart>180)
        { Test->AddError(FString::Printf(TEXT("Boss restart wall timeout: cycle=%d stage=%d"),Cycle,Stage)); return true; }
        UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr; if (!W || !W->HasBegunPlay()) return false;
        if (Stage==0)
        {
            for (TActorIterator<ACountessBossCharacter> It(W);It;++It) { Boss=*It; break; }
            Player=Cast<ATPCCharacter>(UGameplayStatics::GetPlayerCharacter(W,0));
            PC=Cast<ATPCPlayerController>(UGameplayStatics::GetPlayerController(W,0));
            if (!Boss.IsValid() || !Player.IsValid() || !PC.IsValid()) return false;
            if (bRaisedHome)
            {
                // A normal level placement slightly above the floor; let real CMC gravity settle it.
                // Do not inject the private HomeLocation or reset/AI state under test.
                UClass* SavedClass=Boss->GetClass(); Boss->Destroy();
                const FTransform Placement(FRotator::ZeroRotator,FVector(0,0,220));
                auto* Placed=W->SpawnActorDeferred<ACountessBossCharacter>(SavedClass,Placement,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
                if (!Placed) { Test->AddError(TEXT("Raised placement failed")); return true; }
                Placed->FinishSpawning(Placement); Boss=Placed;
            }
            OriginalController=Cast<ACountessBossAIController>(Boss->GetController());
            OriginalDefinition=Boss->BossActions->Definition;
            Test->TestNotNull(TEXT("Saved boss starts with its real AI controller"),OriginalController.Get());
            FApp::SetUseFixedTimeStep(true); FApp::SetFixedDeltaTime(1./60);
            ConfigureAction(); NextStage(1,W);
        }
        auto* B=Boss.Get(); if (!B || !PC.IsValid()) { Test->AddError(TEXT("Boss or player controller disappeared")); return true; }
        auto* A=B->BossActions.Get(); auto* AI=Cast<ACountessBossAIController>(B->GetController());
        const FVector L=B->GetActorLocation(), H=A->GetHomeLocation();
        Trace+=FString::Printf(TEXT("%d,%d,%.4f,%d,%d,%s,%d,%d,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%s,%s,%.1f,%d\n"),
            Cycle,Stage,W->GetTimeSeconds(),static_cast<int32>(A->State),static_cast<int32>(A->CurrentAction),*GetNameSafe(AI),
            AI&&AI->GetBrainComponent()&&AI->GetBrainComponent()->IsRunning(),AI?static_cast<int32>(AI->GetMoveStatus()):-1,
            static_cast<int32>(B->GetCharacterMovement()->MovementMode),L.X,L.Y,L.Z,H.X,H.Y,H.Z,*GetNameSafe(A->GetTarget()),
            *GetNameSafe(PC->GetPawn()),Player.IsValid()?Player->HealthComponent->GetCurrentHealth():-1.f,A->GetContextRevision());
        const double Elapsed=W->GetTimeSeconds()-Start;
        if (Stage==1)
        {
            const float KillAt=.1f+(Cycle%3)*.55f;
            if (A->IsActionActive() && A->GetStateElapsed()>=KillAt)
            {
                Test->TestTrue(TEXT("Death interrupts a real boss action, not a synthetic state"),A->State==EBossState::Action);
                Player->HealthComponent->ApplyDamageFrom(20000.f,B);
                Test->TestTrue(TEXT("Real health hit killed the current protagonist"),Player->HealthComponent->GetCurrentHealth()<=0);
                NextStage(2,W);
            }
            else if (Elapsed>14.) { Test->AddError(FString::Printf(TEXT("Cycle %d: boss did not start the configured attack"),Cycle)); return true; }
        }
        else if (Stage==2)
        {
            if (PC->IsDeathScreenOpen())
            {
                ATPCCharacter* Old=Player.Get(); PC->RestartAfterDeath();
                Player=Cast<ATPCCharacter>(PC->GetPawn());
                Test->TestTrue(TEXT("Actual death UI restart replaces the old pawn"),Player.IsValid()&&Player.Get()!=Old);
                Test->TestFalse(TEXT("Restart leaves the world unpaused"),UGameplayStatics::IsGamePaused(W));
                auto* MovementOnly=DuplicateObject<UBossDefinition>(OriginalDefinition,B);
                for (auto& Action:MovementOnly->Actions) Action.PhaseOneWeight=Action.PhaseTwoWeight=0;
                A->Definition=MovementOnly; RestartBossLocation=B->GetActorLocation(); NextStage(3,W);
            }
            else if (Elapsed>8.) { Test->AddError(TEXT("Death presentation did not reach the restart UI")); return true; }
        }
        else if (Stage==3 && Elapsed>6.)
        {
            const FString Label=FString::Printf(TEXT("Restart %d"),Cycle+1);
            Test->TestTrue(Label+TEXT(" retains the possessed boss controller"),AI&&AI==OriginalController.Get()&&AI->GetPawn()==B);
            Test->TestTrue(Label+TEXT(" behavior tree continues ticking"),AI&&AI->GetBrainComponent()&&AI->GetBrainComponent()->IsRunning());
            Test->TestTrue(Label+TEXT(" reacquires the new player pawn"),A->GetTarget()==Player.Get()&&A->State==EBossState::Combat);
            Test->TestTrue(Label+TEXT(" physically resumes navigation"),FVector::Dist2D(RestartBossLocation,B->GetActorLocation())>60.f);
            Test->AddInfo(FString::Printf(TEXT("%s: state=%d controller=%s target=%s moved=%.2f home_distance=%.2f"),*Label,
                static_cast<int32>(A->State),*GetNameSafe(AI),*GetNameSafe(A->GetTarget()),FVector::Dist2D(RestartBossLocation,B->GetActorLocation()),FVector::Dist(H,L)));
            if (A->GetTarget()!=Player.Get() || A->State!=EBossState::Combat) return true;
            if (++Cycle==(bRaisedHome?3:12)) { Stop(); return true; }
            ConfigureAction(); NextStage(1,W);
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPCBossRestartFeedbackTest,"ThirdPerson.Feedback.PIE.BossRepeatedDeathRestart",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTPCBossRestartFeedbackTest::RunTest(const FString&)
{
    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Third/Bosses/Countess/Maps/L_CountessBossTest"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(GameplayFeedbackPIE::FBossRestartScenario(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPCBossRaisedRestartFeedbackTest,"ThirdPerson.Feedback.PIE.BossRaisedHomeRestart",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTPCBossRaisedRestartFeedbackTest::RunTest(const FString&)
{
    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Third/Bosses/Countess/Maps/L_CountessBossTest"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(GameplayFeedbackPIE::FBossRestartScenario(this,true));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
