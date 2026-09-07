#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "MotionWarpingComponent.h"
#include "../Character/TPCCharacter.h"
#include "../Character/TPCPlayerController.h"
#include "../AI/EnemyCharacter.h"
#include "../Boss/CountessBossCharacter.h"
#include "../Components/ActionComponent.h"
#include "../Components/CombatComponent.h"
#include "../Components/EquipmentComponent.h"
#include "../Components/HealthComponent.h"
#include "../Components/StaminaComponent.h"
#include "../Weapons/WeaponDefinition.h"
#include "../Weapons/WeaponActor.h"
#include "../UI/PlayerDeathWidget.h"
#include "../Animation/TPCAnimInstance.h"
#include "BehaviorTree/BlackboardComponent.h"

struct FTPCSwordPIETestAccess
{
    static void Reset(ATPCCharacter& P)
    {
        P.CancelMotionAction(); P.ClearHitMovementLock(); P.StopCrouch(); P.StopSprint(); P.EndJump();
        P.GetWorldTimerManager().ClearTimer(P.HitMovementLockTimerHandle);
        P.CombatComponent->SetCombatEnabled(false); P.CombatComponent->SetCombatEnabled(true);
        P.ActionComponent->ClearInputBuffers(); P.ActionComponent->SetInputSuppressed(false);
        P.LockedTarget = nullptr; P.LastMoveInputAxis = FVector2D::ZeroVector; P.bEnableRootMotionTurn = false;
        P.GetCharacterMovement()->SetMovementMode(MOVE_Walking); P.GetCharacterMovement()->StopMovementImmediately();
        P.LastDashTime = -BIG_NUMBER;
        P.StaminaComponent->SetCurrentStamina(100.f); P.PreviousHealth = 10000.f; P.HealthComponent->MaxHealth = 10000.f; P.HealthComponent->SetCurrentHealth(10000.f);
        P.SetActorLocationAndRotation(FVector(0,-1000,98), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
        if (P.GetController()) P.GetController()->SetControlRotation(FRotator::ZeroRotator);
    }
    static bool DamageWindow(const UCombatComponent& C) { return C.bAttackWindowActive; }
    static bool CombatEnabled(const UCombatComponent& C) { return C.bCombatEnabled; }
    static int32 LaunchCount(const AEnemyCharacter& E) { return E.LaunchesThisFlight; }
    static bool DiveLanded(const UCombatComponent& C) { return C.bAirDiveLanded; }
    static void Axis(ATPCCharacter& P, FVector2D Axis) { P.LastMoveInputAxis = Axis; }
};

namespace SwordPIE
{
class FCoreScenario : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TWeakObjectPtr<ATPCCharacter> Player;
    TWeakObjectPtr<AEnemyCharacter> Enemy;
    int32 Stage = 0, CaseIndex = 0, FPSIndex = 0, Step = 0, Starts = 0;
    const int32 FrameRates[3] = {30,60,120};
    double Start = 0, WallStart = FPlatformTime::Seconds();
    bool bPreviousFixed = FApp::UseFixedTimeStep(); double PreviousDelta = FApp::GetFixedDeltaTime();
    bool bQueued = false, bImmuneSeen = false, bAfterWindowSeen = false, bScreenshot = false;
    FVector StartLocation; float StartYaw = 0.f, AccumulatedYaw = 0.f;
    FDelegateHandle StartHandle;
    float ReferenceDamage[2] = {0.f,0.f};
    FString Trace = TEXT("fps,case,time,action,position_x,position_y,position_z,hp,sp,enemy_hp,actor_yaw,enemy_x,enemy_y\n");
    FString LocomotionTrace = TEXT("fps,time,direction,actor_yaw,speed,foot_l_x,foot_l_y,foot_l_z,foot_r_x,foot_r_y,foot_r_z\n");
    void Stop()
    {
        FApp::SetUseFixedTimeStep(bPreviousFixed); FApp::SetFixedDeltaTime(PreviousDelta);
        if (Player.IsValid()) Player->CombatComponent->OnMeleeAttackStarted.Remove(StartHandle);
    }
    FString Label(const TCHAR* Text) const { return FString::Printf(TEXT("%d FPS case %d: %s"), FrameRates[FPSIndex], CaseIndex, Text); }
    void Next(UWorld* W)
    {
        Test->AddInfo(Label(TEXT("completed")));
        ++CaseIndex; if (CaseIndex == 11) { CaseIndex = 0; ++FPSIndex; }
        Stage = 1; Start = W->GetTimeSeconds();
    }
public:
    explicit FCoreScenario(FAutomationTestBase* InTest) : Test(InTest) {}
    ~FCoreScenario() override { Stop(); }
    bool Update() override
    {
        if (FPlatformTime::Seconds() - WallStart > 180)
        { Test->AddError(FString::Printf(TEXT("Sword PIE timeout at stage %d case %d step %d"), Stage, CaseIndex, Step)); return true; }
        UWorld* W = GEditor ? GEditor->PlayWorld : nullptr; if (!W || !W->HasBegunPlay()) return false;
        if (FPSIndex == 3)
        {
            FFileHelper::SaveStringToFile(Trace, *(FPaths::ProjectSavedDir() / TEXT("SwordActionImplementation/player_pie_trace.csv")));
            FFileHelper::SaveStringToFile(LocomotionTrace, *(FPaths::ProjectSavedDir() / TEXT("SwordActionImplementation/locomotion_pie_trace.csv")));
            Stop(); return true;
        }
        if (Stage == 0)
        {
            Player = Cast<ATPCCharacter>(UGameplayStatics::GetPlayerCharacter(W, 0)); if (!Player.IsValid()) return false;
            for (TActorIterator<ACountessBossCharacter> It(W); It; ++It) It->Destroy();
            auto* Weapon = LoadObject<UWeaponDefinition>(nullptr, TEXT("/Game/Third/DataAsset/DA_TestSword.DA_TestSword"));
            if (!Test->TestTrue(TEXT("PIE equips the real sword actor"), Player->EquipmentComponent->EquipWeapon(Weapon))) return true;
            Test->TestNotNull(TEXT("Saved player owns unified action component"), Player->ActionComponent.Get());
            UClass* Class = LoadClass<AEnemyCharacter>(nullptr, TEXT("/Game/Third/Character/BP_EnemyCharacter.BP_EnemyCharacter_C"));
            FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Enemy = W->SpawnActor<AEnemyCharacter>(Class, FVector(160,-1000,98), FRotator(0,180,0), Params);
            if (!Test->TestNotNull(TEXT("Saved sword opponent spawned"), Enemy.Get())) return true;
            if (auto* AI = Cast<AAIController>(Enemy->GetController())) { if (AI->GetBrainComponent()) AI->GetBrainComponent()->StopLogic(TEXT("Sword deterministic probe")); AI->StopMovement(); }
            Enemy->FindComponentByClass<UCombatComponent>()->SetCombatEnabled(false);
            Enemy->FindComponentByClass<UHealthComponent>()->MaxHealth = 10000.f;
            StartHandle = Player->CombatComponent->OnMeleeAttackStarted.AddLambda([this]() { ++Starts; });
            W->GetWorldSettings()->MinUndilatedFrameTime = 0; W->GetWorldSettings()->MaxUndilatedFrameTime = 1;
            FApp::SetUseFixedTimeStep(true); Stage = 1;
        }
        auto* P = Player.Get(); auto* A = P ? P->ActionComponent.Get() : nullptr; auto* C = P ? P->CombatComponent.Get() : nullptr;
        if (!P || !Enemy.IsValid()) { Test->AddError(TEXT("PIE participants disappeared")); return true; }
        if (Stage == 1)
        {
            FTPCSwordPIETestAccess::Reset(*P); FApp::SetFixedDeltaTime(1. / FrameRates[FPSIndex]);
            Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking); Enemy->GetCharacterMovement()->StopMovementImmediately();
            Enemy->SetActorLocationAndRotation(FVector(140,-1000,98), FRotator(0,180,0), false, nullptr, ETeleportType::TeleportPhysics);
            Enemy->FindComponentByClass<UHealthComponent>()->SetCurrentHealth(10000.f);
            Starts = 0; Step = 0; bQueued = bImmuneSeen = bAfterWindowSeen = false;
            Start = W->GetTimeSeconds(); Stage = 2; return false;
        }
        if (Stage == 2 && W->GetTimeSeconds() - Start > .2)
        {
            Test->TestTrue(Label(TEXT("actual frame delta")), FMath::IsNearlyEqual(W->GetDeltaSeconds(), 1.f / FrameRates[FPSIndex], .0001f));
            if (CaseIndex <= 1) { P->LockedTarget = Enemy.Get(); P->HandlePrimaryAttack(); }
            else if (CaseIndex <= 6)
            {
                const FVector2D Axis[] = { {1,0}, {-1,0}, {0,-1}, {0,1}, {1,1} };
                Enemy->SetActorLocation(FVector(-1400,1200,98), false, nullptr, ETeleportType::TeleportPhysics);
                FTPCSwordPIETestAccess::Axis(*P, Axis[CaseIndex-2]); StartLocation = P->GetActorLocation(); P->Dash();
                Test->TestTrue(Label(TEXT("root dodge accepted")), P->IsDashing());
                Test->TestFalse(Label(TEXT("startup is not invulnerable")), A->IsInvulnerable());
            }
            else if (CaseIndex == 7) P->StartBlock();
            else if (CaseIndex == 8)
            {
                Enemy->SetActorLocation(FVector(-1400,1200,98), false, nullptr, ETeleportType::TeleportPhysics);
                P->SetActorLocation(FVector(0,-1000,1700), false, nullptr, ETeleportType::TeleportPhysics);
                P->GetCharacterMovement()->SetMovementMode(MOVE_Falling); P->GetCharacterMovement()->Velocity = FVector(80,0,-50);
                P->HandlePrimaryAttack();
                Test->TestTrue(Label(TEXT("air light preserves vertical inertia")), FMath::IsNearlyEqual(P->GetVelocity().Z, -50.f, .1f));
            }
            else if (CaseIndex==9)
            {
                P->bEnableRootMotionTurn = true; StartYaw = P->GetActorRotation().Yaw; AccumulatedYaw = 0.f;
                P->GetController()->SetControlRotation(FRotator(0,90,0));
            }
            else
            {
                P->LockedTarget=Enemy.Get(); P->GetCharacterMovement()->MaxWalkSpeed=150.f;
                P->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
                StartYaw=P->GetActorRotation().Yaw;
            }
            Start = W->GetTimeSeconds(); Stage = 3; return false;
        }
        if (Stage != 3) return false;
        const float Elapsed = W->GetTimeSeconds() - Start;
        const UActionDefinition* D = A->GetActiveDefinition();
        const FVector L = P->GetActorLocation();
        Trace += FString::Printf(TEXT("%d,%d,%.4f,%s,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n"), FrameRates[FPSIndex], CaseIndex, Elapsed,
            D ? *D->ActionId.ToString() : TEXT("None"), L.X, L.Y, L.Z, P->HealthComponent->GetCurrentHealth(), P->StaminaComponent->GetCurrentStamina(), Enemy->FindComponentByClass<UHealthComponent>()->GetCurrentHealth(), P->GetActorRotation().Yaw, Enemy->GetActorLocation().X, Enemy->GetActorLocation().Y);
        if (FParse::Param(FCommandLine::Get(), TEXT("SwordVisualAudit")) && FPSIndex == 0 && CaseIndex == 1 && Elapsed > .35f && !bScreenshot)
        { FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("SwordActionImplementation/SwordCombo.png"), false, false); bScreenshot = true; }
        if (CaseIndex <= 1)
        {
            if (CaseIndex==1 && D)
            {
                Test->TestTrue(Label(TEXT("close-range followups retain the opponent-facing yaw")), FMath::Abs(FMath::FindDeltaAngleDegrees(0.f, P->GetActorRotation().Yaw)) < 8.f);
                const float Gap=P->GetCapsuleComponent()->GetScaledCapsuleRadius()+Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius();
                Test->TestTrue(Label(TEXT("all four stages preserve physical capsule separation")),FVector::Dist2D(P->GetActorLocation(),Enemy->GetActorLocation())>=Gap-1.f);
                if (const auto* Warp=P->MotionWarpingComponent->FindWarpTarget(P->AttackWarpTargetName); Warp && A->GetMontagePosition()<.18f)
                {
                    Test->TestTrue(Label(TEXT("each stage publishes a capsule-centre warp target")),FMath::IsNearlyEqual(Warp->GetLocation().Z,P->GetActorLocation().Z,1.f));
                    Test->TestTrue(Label(TEXT("warp target stays outside opponent capsule")),FVector::Dist2D(Warp->GetLocation(),Enemy->GetActorLocation())>=Gap+7.f);
                }
            }
            if (D && A->GetMontagePosition() >= .07f && (!bQueued || (CaseIndex == 1 && Step != Starts)))
            {
                for (int32 I = 0; I < 5; ++I) P->HandlePrimaryAttack();
                bQueued = true; Step = Starts;
            }
            if (Elapsed > 4.f && !C->IsMeleeAttackInProgress())
            {
                Test->TestEqual(Label(TEXT("exactly one buffered stage per press batch")), Starts, CaseIndex == 0 ? 2 : 4);
                Test->TestFalse(Label(TEXT("end releases movement")), P->IsMovementInputLocked());
                Test->TestFalse(Label(TEXT("end drops combo intent")), C->HasBufferedComboInput());
                Test->TestNull(Label(TEXT("end clears warp target")), P->MotionWarpingComponent->FindWarpTarget(P->AttackWarpTargetName));
                const float Damage = 10000.f - Enemy->FindComponentByClass<UHealthComponent>()->GetCurrentHealth();
                Test->TestTrue(Label(TEXT("every authored stage hits the stationary opponent in front")), FMath::IsNearlyEqual(Damage, CaseIndex == 0 ? 50.f : 108.75f, .1f));
                Test->TestTrue(Label(TEXT("one hit per stage/group, not per frame")), Damage <= (CaseIndex == 0 ? 50.1f : 108.85f));
                Test->AddInfo(FString::Printf(TEXT("%d FPS case %d: actual opponent damage %.2f"), FrameRates[FPSIndex], CaseIndex, Damage));
                if (FPSIndex == 0) ReferenceDamage[CaseIndex] = Damage;
                else Test->TestTrue(Label(TEXT("hit count and damage match 30 FPS")), FMath::IsNearlyEqual(Damage, ReferenceDamage[CaseIndex], .1f));
                Next(W);
            }
        }
        else if (CaseIndex <= 6)
        {
            bImmuneSeen |= A->IsInvulnerable(); if (Elapsed > .35f) bAfterWindowSeen |= !A->IsInvulnerable();
            if (Elapsed > 1.15f)
            {
                const FVector Delta = P->GetActorLocation() - StartLocation;
                const FVector Directions[] = { FVector::ForwardVector, -FVector::ForwardVector, -FVector::RightVector, FVector::RightVector, FVector::ForwardVector };
                Test->TestTrue(Label(TEXT("quantized direction matches root displacement")), FVector::DotProduct(Delta.GetSafeNormal2D(), Directions[CaseIndex-2]) > .85f);
                Test->TestTrue(Label(TEXT("native root motion supplies approximately five metres")), Delta.Size2D() > 400.f && Delta.Size2D() < 560.f);
                Test->TestTrue(Label(TEXT("authored immunity window observed")), bImmuneSeen && bAfterWindowSeen);
                Test->TestFalse(Label(TEXT("natural end releases dodge")), P->IsDashing()); Next(W);
            }
        }
        else if (CaseIndex == 7)
        {
            FCombatHitSpec Hit; Hit.Damage = 20.f;
            if (Step == 0 && Elapsed >= .05f) { P->StopBlock(); Test->TestTrue(Label(TEXT("tap preserves parry window")), C->IsParryWindowActive()); Step = 1; }
            if (Step == 1 && Elapsed >= .12f)
            {
                const auto Result = P->HealthComponent->ApplyCombatHit(Hit, Enemy.Get());
                Test->TestTrue(Label(TEXT("tap parries a real hit")), Result.bParried && Result.ActualDamage == 0.f);
                P->HandlePrimaryAttack(); Test->TestTrue(Label(TEXT("parry grants one counter")), A->GetActiveDefinition() && A->GetActiveDefinition()->ActionId == TEXT("Sword.ParryCounter"));
                Step = 2;
            }
            if (Step == 2 && !C->IsMeleeAttackInProgress()) { P->StartBlock(); Start = W->GetTimeSeconds(); Step = 3; return false; }
            if (Step == 3 && Elapsed >= .25f)
            {
                const auto Result = P->HealthComponent->ApplyCombatHit(Hit, Enemy.Get());
                Test->TestTrue(Label(TEXT("hold blocks after parry expires")), Result.bBlocked && FMath::IsNearlyEqual(Result.ActualDamage, 5.f));
                Test->TestTrue(Label(TEXT("first block spends 15 stamina")), FMath::IsNearlyEqual(P->StaminaComponent->GetCurrentStamina(), 85.f, .1f)); Step = 4;
            }
            if (Step == 4 && Elapsed >= .72f)
            {
                const auto Result = P->HealthComponent->ApplyCombatHit(Hit, Enemy.Get());
                Test->TestTrue(Label(TEXT("second held block works without repress")), Result.bBlocked && C->IsBlocking());
                Test->TestTrue(Label(TEXT("second block spends another 15")), FMath::IsNearlyEqual(P->StaminaComponent->GetCurrentStamina(), 70.f, .1f));
                P->StaminaComponent->SetCurrentStamina(10.f);
                const auto Broken = P->HealthComponent->ApplyCombatHit(Hit, Enemy.Get());
                Test->TestTrue(Label(TEXT("exhaustion enters breach rather than permanent guard")), Broken.bGuardBroken && !C->IsBlocking() && A->GetActionState() == ETPCActionState::HitReact); Step = 5;
            }
            if (Step == 5 && Elapsed >= 1.85f)
            { Test->TestEqual(Label(TEXT("breach ends in Free")), A->GetActionState(), ETPCActionState::Free); Next(W); }
        }
        else if (CaseIndex == 8)
        {
            if (!bQueued && Elapsed >= .07f) { P->HandlePrimaryAttack(); bQueued = true; }
            if (Step == 0 && Starts == 2 && !C->IsMeleeAttackInProgress())
            {
                Test->TestTrue(Label(TEXT("both light stages finish while falling")), P->GetCharacterMovement()->IsFalling());
                P->HandlePrimaryAttack(); Test->TestEqual(Label(TEXT("third air light is rejected")), Starts, 2); Step = 1;
            }
            if (Step == 1 && !P->GetCharacterMovement()->IsFalling())
            {
                Test->TestEqual(Label(TEXT("expired press did not become an automatic ground attack")), Starts, 2);
                P->StartJump(); Step = 2;
            }
            if (Step == 2 && P->GetCharacterMovement()->IsFalling())
            {
                P->HandlePrimaryAttack(); Test->TestTrue(Label(TEXT("landing resets the air budget")), A->GetActiveDefinition() && A->GetActiveDefinition()->ActionId == TEXT("Sword.Air.Light.1")); Next(W);
            }
        }
        else if (CaseIndex==9)
        {
            const float Yaw = P->GetActorRotation().Yaw; AccumulatedYaw += FMath::FindDeltaAngleDegrees(StartYaw, Yaw); StartYaw = Yaw;
            if (Elapsed > 1.5f)
            {
                Test->TestTrue(Label(TEXT("turn extracts one 90-degree rotation, no double commit")), FMath::IsNearlyEqual(AccumulatedYaw, 90.f, 8.f));
                Test->TestFalse(Label(TEXT("turn ends without a residual lock")), P->IsTurningInPlace()); Next(W);
            }
        }
        else
        {
            Enemy->SetActorLocation(P->GetActorLocation()+FVector(500,0,0),false,nullptr,ETeleportType::TeleportPhysics);
            const FVector Direction=Elapsed<.6f?FVector(-1,.02f,0):(Elapsed<1.2f?FVector(-1,-.02f,0):FVector(1,0,0));
            P->AddMovementInput(Direction.GetSafeNormal(),Elapsed<1.2f?1.f:.12f);
            const auto* Anim=Cast<UTPCAnimInstance>(P->GetMesh()->GetAnimInstance());
            const FVector Left=P->GetMesh()->GetSocketLocation(TEXT("ball_l")), Right=P->GetMesh()->GetSocketLocation(TEXT("ball_r"));
            if (Anim)
            {
                LocomotionTrace+=FString::Printf(TEXT("%d,%.4f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n"),FrameRates[FPSIndex],Elapsed,
                    Anim->Direction,P->GetActorRotation().Yaw,Anim->GroundSpeed,Left.X,Left.Y,Left.Z,Right.X,Right.Y,Right.Z);
                if (Elapsed>.2f && Elapsed<1.15f)
                {
                    Test->TestTrue(Label(TEXT("backwards wrap never interpolates through a forward direction")),FMath::Abs(Anim->Direction)>170.f);
                    bImmuneSeen|=Anim->Direction>170.f; bAfterWindowSeen|=Anim->Direction<-170.f;
                }
            }
            Test->TestTrue(Label(TEXT("strafe direction does not turn the whole actor")),FMath::Abs(FMath::FindDeltaAngleDegrees(StartYaw,P->GetActorRotation().Yaw))<3.f);
            Test->TestFalse(Label(TEXT("locomotion bones remain finite")),Left.ContainsNaN()||Right.ContainsNaN());
            if (FPSIndex==0 && Step<6 && Elapsed>=.53f+Step/30.f && FParse::Param(FCommandLine::Get(),TEXT("SwordVisualAudit")))
            {
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("SwordActionImplementation")/FString::Printf(TEXT("SwordBackWrap_%02d.png"),Step),false,false); ++Step;
            }
            if (Elapsed>2.2f)
            {
                Test->TestTrue(Label(TEXT("both sides of the backward seam were physically exercised")),bImmuneSeen&&bAfterWindowSeen);
                Test->TestTrue(Label(TEXT("low-speed reversal changes velocity without an actor snap")),P->GetVelocity().X>5.f && P->GetVelocity().Size2D()<40.f);
                P->ClearMoveInput(); P->GetCharacterMovement()->StopMovementImmediately(); P->GetCharacterMovement()->MaxWalkSpeed=450.f;
                Next(W);
            }
        }
        if (Elapsed > 10.f) { Test->AddError(Label(TEXT("case failed to reach its ending state"))); Next(W); }
        return false;
    }
};

/** Runs authored montages and real collision in PIE; no action flags or notify results are injected. */
class FLifecycleScenario : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TWeakObjectPtr<ATPCCharacter> Player;
    TWeakObjectPtr<AEnemyCharacter> Enemy;
    TArray<TWeakObjectPtr<AEnemyCharacter>> ExtraEnemies;
    TWeakObjectPtr<AStaticMeshActor> Platform;
    int32 Phase = 0, Case = 0, FPSIndex = 0, Step = 0;
    const int32 Rates[3] = {30,60,120};
    double Start = 0., StepStart = 0., WallStart = FPlatformTime::Seconds(), UIStart = 0.;
    bool bOldFixed = FApp::UseFixedTimeStep(); double OldDelta = FApp::GetFixedDeltaTime();
    bool bA = false, bB = false, bC = false, bD = false;
    TSet<FName> SeenActions, Queued;
    TSet<FString> HitGroups;
    FDelegateHandle HitHandle;
    TWeakObjectPtr<AWeaponActor> OriginalWeapon;
    FVector InitialVelocity, FinalDeathBone;
    float FrozenYaw = 0.f, MaxHeight = 0.f, BeforeBuffDamage = 0.f, FlightReference = -1.f;
    FString BladeTrace=TEXT("fps,time,position,window,base_x,base_y,base_z,tip_x,tip_y,tip_z,enemy_x,enemy_y,enemy_z,enemy_r,enemy_h\n");
    float MultiReference[3] = {0,0,0};
    FString Trace = TEXT("fps,case,time,action,player_z,player_vz,enemy_phase,enemy_z,enemy_hp,sp,montage_position,falling,dive_landed\n");
    FString Label(const TCHAR* Text) const { return FString::Printf(TEXT("Lifecycle %d FPS case %d: %s"), Rates[FPSIndex], Case, Text); }
    AEnemyCharacter* SpawnEnemy(UWorld* W, FVector Location)
    {
        UClass* Class = LoadClass<AEnemyCharacter>(nullptr,TEXT("/Game/Third/Character/BP_EnemyCharacter.BP_EnemyCharacter_C"));
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* E = W->SpawnActor<AEnemyCharacter>(Class,Location,FRotator(0,180,0),Params);
        if (!E) return nullptr;
        if (auto* AI = Cast<AAIController>(E->GetController()))
        { if (AI->GetBrainComponent()) AI->GetBrainComponent()->StopLogic(TEXT("Sword lifecycle deterministic probe")); AI->StopMovement(); }
        E->FindComponentByClass<UCombatComponent>()->SetCombatEnabled(false);
        E->FindComponentByClass<UHealthComponent>()->MaxHealth = 10000.f;
        E->FindComponentByClass<UHealthComponent>()->SetCurrentHealth(10000.f);
        E->GetCharacterMovement()->bUseRVOAvoidance = false;
        return E;
    }
    void Stop()
    {
        FApp::SetUseFixedTimeStep(bOldFixed); FApp::SetFixedDeltaTime(OldDelta);
        if (Enemy.IsValid()) Enemy->FindComponentByClass<UHealthComponent>()->OnCombatHitResolved.Remove(HitHandle);
        if (UWorld* W = GEditor ? GEditor->PlayWorld.Get() : nullptr)
            if (auto* PC = UGameplayStatics::GetPlayerController(W,0)) PC->SetPause(false);
    }
    void Next(UWorld* W)
    {
        Test->AddInfo(Label(TEXT("completed")));
        ++Case; if (Case == 18) { Case = 0; ++FPSIndex; }
        Phase = 1; Start = W->GetTimeSeconds();
    }
    bool Clean(ATPCCharacter* P) const
    {
        return !FTPCSwordPIETestAccess::DamageWindow(*P->CombatComponent) && !P->CombatComponent->HasBufferedComboInput() &&
            !P->MotionWarpingComponent->FindWarpTarget(P->AttackWarpTargetName);
    }
    void Screenshot(const TCHAR* Name, bool bOnce)
    {
        if (bOnce && FPSIndex == 0 && FParse::Param(FCommandLine::Get(),TEXT("SwordVisualAudit")))
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("SwordActionImplementation")/Name,
                FCString::Strcmp(Name,TEXT("SwordDeathScreen.png"))==0,false);
    }
public:
    explicit FLifecycleScenario(FAutomationTestBase* In) : Test(In) {}
    ~FLifecycleScenario() override { Stop(); }
    bool Update() override
    {
        if (FPlatformTime::Seconds()-WallStart>300.)
        { Test->AddError(Label(TEXT("wall-clock timeout"))); return true; }
        UWorld* W = GEditor ? GEditor->PlayWorld : nullptr;
        if (!W || !W->HasBegunPlay()) return false;
        if (FPSIndex == 3)
        {
            FFileHelper::SaveStringToFile(Trace,*(FPaths::ProjectSavedDir()/TEXT("SwordActionImplementation/lifecycle_pie_trace.csv")));
            FFileHelper::SaveStringToFile(BladeTrace,*(FPaths::ProjectSavedDir()/TEXT("SwordActionImplementation/air_blade_trace.csv")));
            Stop(); return true;
        }
        auto* P = Cast<ATPCCharacter>(UGameplayStatics::GetPlayerCharacter(W,0));
        if (!P) return false;
        auto* PC = Cast<ATPCPlayerController>(P->GetController());
        if (!Test->TestNotNull(TEXT("Lifecycle uses the real player controller"),PC)) return true;
        if (Phase == 0)
        {
            for (TActorIterator<ACountessBossCharacter> It(W);It;++It) It->Destroy();
            W->GetWorldSettings()->MinUndilatedFrameTime=0; W->GetWorldSettings()->MaxUndilatedFrameTime=1;
            FApp::SetUseFixedTimeStep(true); Phase=1;
        }
        if (Phase == 1)
        {
            if (Enemy.IsValid()) { Enemy->FindComponentByClass<UHealthComponent>()->OnCombatHitResolved.Remove(HitHandle); Enemy->Destroy(); }
            for (auto E : ExtraEnemies) if (E.IsValid()) E->Destroy(); ExtraEnemies.Reset();
            if (Platform.IsValid()) Platform->Destroy(); Platform.Reset();
            Player=P; FTPCSwordPIETestAccess::Reset(*P); P->SetDoubleJumpUnlocked(false);
            FApp::SetFixedDeltaTime(1./Rates[FPSIndex]);
            auto* Weapon=LoadObject<UWeaponDefinition>(nullptr,TEXT("/Game/Third/DataAsset/DA_TestSword.DA_TestSword"));
            if (FPSIndex==0 && Case==0) Test->TestTrue(Label(TEXT("sword is equipped from the saved default")),P->EquipmentComponent->GetEquippedWeaponDefinition()==Weapon);
            P->EquipmentComponent->EquipWeapon(Weapon); P->EquipmentComponent->SetWeaponDrawn(true);
            Enemy=SpawnEnemy(W,FVector(115,-1000,98));
            if (!Test->TestNotNull(Label(TEXT("real opponent")),Enemy.Get())) return true;
            HitGroups.Reset(); SeenActions.Reset(); Queued.Reset();
            HitHandle=Enemy->FindComponentByClass<UHealthComponent>()->OnCombatHitResolved.AddLambda(
                [this](const FCombatHitSpec& S,const FCombatHitResult& R,AActor* Source)
                {
                    if (Source!=Player.Get() || R.ActualDamage<=0) return;
                    const FString Key=FString::Printf(TEXT("%llu.%s"),S.ActionSerial,*S.WindowId.ToString());
                    Test->TestFalse(Label(TEXT("one damage result per action and authored hit group")),HitGroups.Contains(Key)); HitGroups.Add(Key);
                });
            Step=0; bA=bB=bC=bD=false; Start=W->GetTimeSeconds(); Phase=2; return false;
        }
        auto* A=P->ActionComponent.Get(); auto* C=P->CombatComponent.Get();
        if (Phase == 2)
        {
            if (W->GetTimeSeconds()-Start<.25) return false;
            Test->TestTrue(Label(TEXT("actual frame delta")),FMath::IsNearlyEqual(W->GetDeltaSeconds(),1.f/Rates[FPSIndex],.0001f));
            switch(Case)
            {
            case 0: P->LockedTarget=Enemy.Get(); P->StartCrouch(); P->HandlePrimaryAttack(); break;
            case 1: P->StartCrouch(); P->HandlePrimaryAttack(); break;
            case 2:
                P->SetActorLocation(FVector(0,-1000,2500),false,nullptr,ETeleportType::TeleportPhysics);
                P->GetCharacterMovement()->SetMovementMode(MOVE_Falling); P->GetCharacterMovement()->Velocity=FVector(100,0,150);
                P->HandleAirDiveAttack(); break;
            case 3:
                Enemy->SetActorLocation(FVector(115,-1000,3500),false,nullptr,ETeleportType::TeleportPhysics); Enemy->ApplyUppercutHit(P); break;
            case 4: Enemy->ApplyUppercutHit(P); break;
            case 5:
                P->SetActorLocation(FVector(0,-1000,2200),false,nullptr,ETeleportType::TeleportPhysics);
                P->GetCharacterMovement()->SetMovementMode(MOVE_Falling); InitialVelocity=FVector(120,40,-50);
                P->GetCharacterMovement()->Velocity=InitialVelocity; P->StartBlock();
                Test->TestTrue(Label(TEXT("air guard preserves all three velocity components on entry")),P->GetVelocity().Equals(InitialVelocity,.01f)); break;
            case 6: BeforeBuffDamage=C->MakeCurrentHitSpec(FVector::ZeroVector).Damage; P->HandleBuff(); Test->TestTrue(Label(TEXT("buff consumes exactly 20 SP at start")),FMath::IsNearlyEqual(P->StaminaComponent->GetCurrentStamina(),80.f,.01f)); break;
            case 7: OriginalWeapon=P->EquipmentComponent->GetEquippedWeaponActor(); P->HandleToggleWeapon(); break;
            case 8: P->StartJump(); break;
            case 9: P->StartSprint(); P->HandlePrimaryAttack(); break;
            case 10: P->LockedTarget=Enemy.Get(); P->HandlePrimaryAttack(); break;
            case 11:
            {
                auto* Cube=W->SpawnActor<AStaticMeshActor>(FVector(115,-1000,500),FRotator::ZeroRotator);
                Cube->SetMobility(EComponentMobility::Movable);
                Cube->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
                Cube->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll")); Cube->SetActorScale3D(FVector(8,8,1)); Platform=Cube;
                Enemy->SetActorLocation(FVector(115,-1000,645),false,nullptr,ETeleportType::TeleportPhysics);
                Enemy->ApplyUppercutHit(P); break;
            }
            case 12:
                P->HandlePrimaryAttack(); P->HealthComponent->ApplyDamageFrom(20000.f,Enemy.Get());
                Test->TestEqual(Label(TEXT("death takes highest priority")),A->GetActionState(),ETPCActionState::Dead);
                Test->TestTrue(Label(TEXT("death closes traces, combo and warp")),Clean(P)); break;
            case 14:
            case 15:
            {
                auto* Style=LoadObject<UWeaponDefinition>(nullptr,*FString::Printf(TEXT("/Game/Third/Actions/Sword/DA_Sword_Style%02d.DA_Sword_Style%02d"),Case-12,Case-12));
                Test->TestTrue(Label(TEXT("variant uses the same equipment entry")),P->EquipmentComponent->EquipWeapon(Style));
                P->HandlePrimaryAttack(); break;
            }
            case 13:
                ExtraEnemies.Add(SpawnEnemy(W,FVector(120,-1095,98))); ExtraEnemies.Add(SpawnEnemy(W,FVector(120,-905,98)));
                P->HandlePrimaryAttack(); break;
            case 16:
            case 17:
            {
                auto* Cube=W->SpawnActor<AStaticMeshActor>(Case==16?FVector(140,-1000,200):FVector(0,-1000,500),FRotator::ZeroRotator);
                Cube->SetMobility(EComponentMobility::Movable);
                Cube->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
                Cube->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
                Cube->SetActorScale3D(Case==16?FVector(.3f,8,4):FVector(8,8,1)); Platform=Cube;
                if (Case==16)
                {
                    Enemy->SetActorLocation(FVector(320,-1000,98),false,nullptr,ETeleportType::TeleportPhysics);
                    P->LockedTarget=Enemy.Get(); P->HandlePrimaryAttack();
                }
                else
                {
                    P->SetActorLocation(FVector(0,-1000,1200),false,nullptr,ETeleportType::TeleportPhysics);
                    P->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
                    P->HandleAirDiveAttack();
                }
                break;
            }
            }
            Start=StepStart=W->GetTimeSeconds(); Phase=3; return false;
        }
        const float T=W->GetTimeSeconds()-Start;
        const float S=W->GetTimeSeconds()-StepStart;
        const auto* D=A->GetActiveDefinition(); if (D) SeenActions.Add(D->ActionId);
        const auto EnemyPhase=Enemy.IsValid()?Enemy->GetLaunchPhase():EEnemyLaunchPhase::Dead;
        Trace+=FString::Printf(TEXT("%d,%d,%.4f,%s,%.2f,%.2f,%d,%.2f,%.2f,%.2f,%.5f,%d,%d\n"),Rates[FPSIndex],Case,T,
            D?*D->ActionId.ToString():TEXT("None"),P->GetActorLocation().Z,P->GetVelocity().Z,static_cast<int32>(EnemyPhase),
            Enemy.IsValid()?Enemy->GetActorLocation().Z:-1.f,Enemy.IsValid()?Enemy->FindComponentByClass<UHealthComponent>()->GetCurrentHealth():0.f,P->StaminaComponent->GetCurrentStamina(),
            A->GetMontagePosition(),P->GetCharacterMovement()->IsFalling(),FTPCSwordPIETestAccess::DiveLanded(*C));
        FCombatHitSpec Hit; Hit.Damage=20.f;
        switch(Case)
        {
        case 0:
            if (D && D->ActionId==TEXT("Sword.Air.Light.1"))
            {
                auto* Mesh=P->EquipmentComponent->GetEquippedWeaponActor()->GetSkeletalWeaponMesh();
                const FVector B=Mesh->GetSocketLocation(TEXT("BladeBase")), Tip=Mesh->GetSocketLocation(TEXT("BladeTip")), E=Enemy->GetActorLocation();
                BladeTrace+=FString::Printf(TEXT("%d,%.5f,%.5f,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n"),Rates[FPSIndex],T,A->GetMontagePosition(),FTPCSwordPIETestAccess::DamageWindow(*C)?1:0,B.X,B.Y,B.Z,Tip.X,Tip.Y,Tip.Z,E.X,E.Y,E.Z,Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius(),Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
            }
            if (T<.14f) Test->TestFalse(Label(TEXT("rising waits for its authored commit before launch")),P->GetCharacterMovement()->IsFalling());
            if (D && A->GetMontagePosition()>.07f && (D->ActionId==TEXT("Sword.Rising") || D->ActionId==TEXT("Sword.Air.Light.1")) && !Queued.Contains(D->ActionId))
            { Queued.Add(D->ActionId); P->HandlePrimaryAttack(); }
            if (D && D->ActionId==TEXT("Sword.Air.Light.2") && A->GetMontagePosition()>.47f && !bA)
            {
                bA=true; const uint64 Before=A->GetActionInstanceId();
                const float SP=P->StaminaComponent->GetCurrentStamina(); P->StaminaComponent->SetCurrentStamina(0.f);
                P->HandleAirDiveAttack(); Test->TestEqual(Label(TEXT("insufficient stamina does not interrupt current air attack")),A->GetActionInstanceId(),Before);
                P->StaminaComponent->SetCurrentStamina(SP); P->HandleAirDiveAttack();
            }
            if (EnemyPhase==EEnemyLaunchPhase::Airborne) bB=true;
            if (EnemyPhase==EEnemyLaunchPhase::LandImpact) bC=true;
            if (EnemyPhase==EEnemyLaunchPhase::DownIdle && !bD)
            {
                bD=true; Enemy->FindComponentByClass<UHealthComponent>()->ApplyDamageFrom(5.f,P);
                Test->TestEqual(Label(TEXT("downed hit stays in paired Down01 reaction")),Enemy->GetLaunchPhase(),EEnemyLaunchPhase::DownHit);
            }
            if (T>.7f && !bA && D && D->ActionId==TEXT("Sword.Air.Light.1")) Screenshot(TEXT("SwordAirCombo.png"),!bC);
            if (T>4.f && EnemyPhase==EEnemyLaunchPhase::None && !C->IsMeleeAttackInProgress())
            {
                for (const TCHAR* Id : {TEXT("Sword.Rising"),TEXT("Sword.Air.Light.1"),TEXT("Sword.Air.Light.2"),TEXT("Sword.Air.Dive")})
                    Test->TestTrue(Label(Id),SeenActions.Contains(FName(Id)));
                Test->TestTrue(Label(TEXT("actual blade launch, physical landing and down-hit all occurred")),bB&&bC&&bD);
                Test->TestTrue(Label(TEXT("recovery restarts enemy combat only on the ground")),!Enemy->GetCharacterMovement()->IsFalling() && FTPCSwordPIETestAccess::CombatEnabled(*Enemy->FindComponentByClass<UCombatComponent>()));
                Test->TestTrue(Label(TEXT("full aerial flow releases action transients")),Clean(P));
                const float Damage=10000.f-Enemy->FindComponentByClass<UHealthComponent>()->GetCurrentHealth();
                Test->AddInfo(FString::Printf(TEXT("%d FPS full aerial damage %.2f"),Rates[FPSIndex],Damage));
                if (FPSIndex==0) FlightReference=Damage; else Test->TestTrue(Label(TEXT("aerial hit count matches 30 FPS")),FMath::IsNearlyEqual(Damage,FlightReference,.1f));
                Next(W);
            }
            break;
        case 1:
            if (!bA && T>=.08f)
            {
                P->HandlePrimaryAttack(); P->HealthComponent->ApplyCombatHit(Hit,Enemy.Get()); bA=true;
                Test->TestEqual(Label(TEXT("precommit hit owns HitReact")),A->GetActionState(),ETPCActionState::HitReact);
            }
            if (bA && T<.7f) Test->TestFalse(Label(TEXT("interrupted rising never launches later")),P->GetCharacterMovement()->IsFalling());
            if (T>1.3f) { Test->TestTrue(Label(TEXT("rising interruption clears damage and queued followup")),Clean(P)); Test->TestEqual(Label(TEXT("hit recovery releases channel")),A->GetActionState(),ETPCActionState::Free); Next(W); }
            break;
        case 2:
            if (!bA && T>.4f)
            {
                Test->TestTrue(Label(TEXT("dive commits downward through CMC")),P->GetVelocity().Z<-600.f);
                P->HealthComponent->ApplyCombatHit(Hit,Enemy.Get()); bA=true;
                Test->TestEqual(Label(TEXT("dive hit interruption")),A->GetActionState(),ETPCActionState::HitReact);
                Test->TestTrue(Label(TEXT("dive interruption clears its trace and warp")),Clean(P));
            }
            if (T>1.f) { Test->TestTrue(Label(TEXT("interrupted dive continues physical falling")),P->GetCharacterMovement()->IsFalling() && P->GetVelocity().Z<0.f); Next(W); }
            break;
        case 3:
            if (!bA && T>.2f) { Enemy->ApplyUppercutHit(P); bA=true; }
            if (!bB && T>.3f) { Enemy->ApplyUppercutHit(P); bB=true; Test->TestEqual(Label(TEXT("third relaunch cannot renew the flight")),FTPCSwordPIETestAccess::LaunchCount(*Enemy),2); }
            if (T>1.5f && T<2.f)
            {
                Test->TestEqual(Label(TEXT("high enemy never stands at the old timeout")),EnemyPhase,EEnemyLaunchPhase::Airborne);
                Test->TestFalse(Label(TEXT("AI combat stays disabled while airborne")),FTPCSwordPIETestAccess::CombatEnabled(*Enemy->FindComponentByClass<UCombatComponent>()));
            }
            if (EnemyPhase==EEnemyLaunchPhase::LandImpact) bC=true;
            if (T>3.f && EnemyPhase==EEnemyLaunchPhase::None)
            { Test->TestTrue(Label(TEXT("high launch waits for actual Landed")),bC); Test->TestEqual(Label(TEXT("actual landing resets relaunch budget")),FTPCSwordPIETestAccess::LaunchCount(*Enemy),0); Next(W); }
            break;
        case 4:
            if (!bA && EnemyPhase==EEnemyLaunchPhase::GetUp)
            {
                bA=true; StepStart=W->GetTimeSeconds(); Enemy->FindComponentByClass<UHealthComponent>()->ApplyDamageFrom(20000.f,P);
                Test->TestEqual(Label(TEXT("killing get-up enters Dead immediately")),Enemy->GetLaunchPhase(),EEnemyLaunchPhase::Dead);
            }
            if (bA && Enemy.IsValid())
            {
                Test->TestEqual(Label(TEXT("get-up callback never exits Dead")),Enemy->GetLaunchPhase(),EEnemyLaunchPhase::Dead);
                Test->TestFalse(Label(TEXT("dead enemy never reenables attacks")),FTPCSwordPIETestAccess::CombatEnabled(*Enemy->FindComponentByClass<UCombatComponent>()));
            }
            if (bA && S>2.5f) Next(W);
            break;
        case 5:
            if (!bA && T>.3f)
            {
                Test->TestTrue(Label(TEXT("air guard retains horizontal inertia and gravity")),P->GetVelocity().Size2D()>100.f && P->GetVelocity().Z<-250.f);
                Enemy->SetActorLocation(P->GetActorLocation()+FVector(120,0,0),false,nullptr,ETeleportType::TeleportPhysics);
                const FVector Before=P->GetVelocity(); auto R=P->HealthComponent->ApplyCombatHit(Hit,Enemy.Get());
                Test->TestTrue(Label(TEXT("air block reduces damage without resetting velocity")),R.bBlocked && FMath::IsNearlyEqual(R.ActualDamage,5.f) && Before.Equals(P->GetVelocity(),.01f));
                bA=true;
            }
            if (T>.8f) { Test->TestTrue(Label(TEXT("air block hit returns to held guard")),C->IsBlocking() && !P->IsGuardHitReactionActive()); P->StopBlock(); Next(W); }
            break;
        case 6:
            if (T<.25f) Test->TestEqual(Label(TEXT("buff does not apply before commit")),C->GetSwordBuffMultiplier(),1.f);
            if (!bA && T>.4f)
            {
                Test->TestEqual(Label(TEXT("buff owns the Skill channel")),A->GetActionState(),ETPCActionState::Skill);
                Test->TestTrue(Label(TEXT("buff applies once")),FMath::IsNearlyEqual(C->GetSwordBuffMultiplier(),1.2f));
                Test->TestTrue(Label(TEXT("buff changes real damage calculation")),FMath::IsNearlyEqual(C->MakeCurrentHitSpec(FVector::ZeroVector).Damage,BeforeBuffDamage*1.2f));
                P->HealthComponent->ApplyCombatHit(Hit,Enemy.Get()); bA=true;
            }
            if (T>8.6f) { Test->TestEqual(Label(TEXT("buff expires without cumulative multipliers")),C->GetSwordBuffMultiplier(),1.f); Test->TestTrue(Label(TEXT("buff interruption releases full body")),!P->IsMovementInputLocked() && Clean(P)); Next(W); }
            break;
        case 7:
            if (Step==0)
            {
                if (T<.45f) Test->TestTrue(Label(TEXT("sheathe waits for commit notify")),P->EquipmentComponent->IsWeaponDrawn());
                if (T>.65f && !bA) { bA=true; Test->TestTrue(Label(TEXT("sheathe hides the same weapon actor")),!P->EquipmentComponent->IsWeaponDrawn() && OriginalWeapon==P->EquipmentComponent->GetEquippedWeaponActor() && OriginalWeapon->IsHidden()); }
                if (T>1.6f) { P->HandlePrimaryAttack(); Step=1; StepStart=W->GetTimeSeconds(); }
            }
            else if (S>1.7f)
            {
                Test->TestTrue(Label(TEXT("primary while sheathed draws, rather than attacking invisibly")),SeenActions.Contains(TEXT("Sword.Draw")) && !SeenActions.Contains(TEXT("Sword.Light.1")));
                Test->TestTrue(Label(TEXT("draw commits visibility without respawning equipment")),P->EquipmentComponent->IsWeaponDrawn() && OriginalWeapon==P->EquipmentComponent->GetEquippedWeaponActor() && !OriginalWeapon->IsHidden()); Next(W);
            }
            break;
        case 8:
            if (Step==0 && T>.2f)
            {
                P->EndJump(); const FVector Before=P->GetVelocity(); P->StartJump();
                Test->TestEqual(Label(TEXT("base ability has one jump")),P->JumpMaxCount,1);
                Test->TestTrue(Label(TEXT("locked second jump supplies no impulse")),Before.Equals(P->GetVelocity(),.01f)); Step=1;
            }
            else if (Step==1 && !P->GetCharacterMovement()->IsFalling())
            { P->SetDoubleJumpUnlocked(true); P->StartJump(); Step=2; StepStart=W->GetTimeSeconds(); }
            else if (Step==2 && S>.2f)
            { P->EndJump(); P->StartJump(); Step=3; StepStart=W->GetTimeSeconds(); }
            else if (Step==3 && S>.06f)
            {
                Test->TestEqual(Label(TEXT("unlocked second jump consumes exactly the second jump")),P->JumpCurrentCount,2);
                Test->TestTrue(Label(TEXT("second jump uses one real upward impulse")),P->GetVelocity().Z>500.f);
                Test->TestTrue(Label(TEXT("second jump plays in-place authored pose")),P->GetMesh()->GetAnimInstance()->Montage_IsPlaying(A->GetActionSet()->DoubleJumpMontage));
                P->EndJump(); P->StartJump(); Step=4;
            }
            else if (Step==4 && S>.2f)
            { Test->TestEqual(Label(TEXT("third jump remains blocked")),P->JumpCurrentCount,2); Next(W); }
            break;
        case 9:
            if (T<.2f) Test->TestTrue(Label(TEXT("sprint input selects its authored opener")),D && D->ActionId==TEXT("Sword.Sprint"));
            if (T>2.f) { Test->TestFalse(Label(TEXT("sprint attack naturally releases movement")),P->IsMovementInputLocked()); Test->TestTrue(Label(TEXT("sprint attack restores normal speed")),FMath::IsNearlyEqual(P->GetCharacterMovement()->MaxWalkSpeed,450.f)); Next(W); }
            break;
        case 10:
            if (!bA && T>.23f)
            { bA=true; FrozenYaw=P->GetActorRotation().Yaw; Enemy->SetActorLocation(FVector(-250,-1000,98),false,nullptr,ETeleportType::TeleportPhysics); }
            if (bA && T<1.3f) Test->TestTrue(Label(TEXT("active/recovery never spins toward a target behind")),FMath::Abs(FMath::FindDeltaAngleDegrees(FrozenYaw,P->GetActorRotation().Yaw))<8.f);
            if (T>1.8f)
            { Test->TestTrue(Label(TEXT("target-move case leaves no residual warp modifier target")),Clean(P)); Next(W); }
            break;
        case 11:
            if (!bA && EnemyPhase==EEnemyLaunchPhase::GetUp)
            { bA=true; Platform->Destroy(); StepStart=W->GetTimeSeconds(); }
            if (bA && !bB && Enemy->GetCharacterMovement()->IsFalling())
            { bB=true; Test->TestEqual(Label(TEXT("removed platform reopens Airborne physical gate")),Enemy->GetLaunchPhase(),EEnemyLaunchPhase::Airborne); }
            if (bB && EnemyPhase==EEnemyLaunchPhase::LandImpact) bC=true;
            if (bC && EnemyPhase==EEnemyLaunchPhase::None)
            { Test->TestTrue(Label(TEXT("platform fall completes a second physical recovery")),!Enemy->GetCharacterMovement()->IsFalling()); Next(W); }
            break;
        case 12:
            if (T<4.5f)
            {
                Test->TestTrue(Label(TEXT("death is not truncated at two seconds")),Player.IsValid() && PC->GetPawn()==Player.Get());
                Test->TestFalse(Label(TEXT("restart UI waits for the death performance")),PC->IsDeathScreenOpen());
            }
            if (PC->IsDeathScreenOpen() && !bA)
            {
                bA=true; UIStart=FPlatformTime::Seconds();
                Test->TestTrue(Label(TEXT("death UI uses UI-only cursor and stops gameplay")),PC->bShowMouseCursor && PC->IsMoveInputIgnored() && PC->IsPaused());
            }
            if (bA && !bD && FPlatformTime::Seconds()-UIStart>.1)
            { bD=true; Screenshot(TEXT("SwordDeathScreen.png"),true); }
            if (bA && !bB && FPlatformTime::Seconds()-UIStart>.25)
            {
                bB=true; PC->GetDeathScreen()->Restart(); // Same handler as the visible button / Enter.
                auto* New=Cast<ATPCCharacter>(PC->GetPawn());
                if (!Test->TestNotNull(Label(TEXT("restart produced a new protagonist")),New)) return true;
                Test->TestTrue(Label(TEXT("restart replaced rather than revived the dead channel")),New!=Player.Get());
                Test->TestEqual(Label(TEXT("new action state is Free")),New->ActionComponent->GetActionState(),ETPCActionState::Free);
                Test->TestTrue(Label(TEXT("HP/SP are reset")),FMath::IsNearlyEqual(New->HealthComponent->GetCurrentHealth(),New->HealthComponent->GetMaxHealth()) && FMath::IsNearlyEqual(New->StaminaComponent->GetCurrentStamina(),New->StaminaComponent->GetMaxStamina()));
                Test->TestTrue(Label(TEXT("camera, cursor, pause and control modes reset")),PC->GetViewTarget()==New && !PC->bShowMouseCursor && !PC->IsPaused() && !PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored());
                Test->TestTrue(Label(TEXT("no old input or weapon transients survive restart")),Clean(New) && New->EquipmentComponent->IsWeaponDrawn());
                Next(W);
            }
            break;
        case 14:
        case 15:
            if (D && A->GetMontagePosition()>.07f && !Queued.Contains(D->ActionId))
            { Queued.Add(D->ActionId); P->HandlePrimaryAttack(); }
            if (T>5.f && !C->IsMeleeAttackInProgress())
            {
                for (int32 Stage=1;Stage<=4;++Stage)
                    Test->TestTrue(Label(TEXT("saved alternate style runs all four distinct definitions")),SeenActions.Contains(FName(*FString::Printf(TEXT("Sword.Style%d.Light.%d"),Case-12,Stage))));
                Test->TestTrue(Label(TEXT("alternate style still uses real blade damage")),Enemy->FindComponentByClass<UHealthComponent>()->GetCurrentHealth()<10000.f);
                Test->TestTrue(Label(TEXT("alternate style ends with the same shared cleanup")),Clean(P) && FMath::IsNearlyEqual(P->GetAnimRootMotionTranslationScale(),1.f));
                Next(W);
            }
            break;
        case 13:
            if (D && A->GetMontagePosition()>.07f && !Queued.Contains(D->ActionId))
            { Queued.Add(D->ActionId); P->HandlePrimaryAttack(); }
            if (T>4.2f && !C->IsMeleeAttackInProgress())
            {
                const AEnemyCharacter* Enemies[3]={Enemy.Get(),ExtraEnemies[0].Get(),ExtraEnemies[1].Get()}; int32 Damaged=0;
                for (int32 I=0;I<3;++I)
                {
                    const float Damage=10000.f-Enemies[I]->FindComponentByClass<UHealthComponent>()->GetCurrentHealth();
                    if (Damage>0.f) ++Damaged;
                    Test->AddInfo(FString::Printf(TEXT("%d FPS multi-enemy %d damage %.2f"),Rates[FPSIndex],I,Damage));
                    Test->TestTrue(Label(TEXT("each opponent has at most one hit per stage")),Damage<=108.85f);
                    if (FPSIndex==0) MultiReference[I]=Damage;
                    else Test->TestTrue(Label(TEXT("multi-enemy hit count matches 30 FPS")),FMath::IsNearlyEqual(Damage,MultiReference[I],.1f));
                }
                Test->TestTrue(Label(TEXT("one combo hits multiple real opponents")),Damaged>=2); Next(W);
            }
            break;
        case 16:
            if (D)
                if (const auto* Warp=P->MotionWarpingComponent->FindWarpTarget(P->AttackWarpTargetName))
                {
                    bA=true;
                    Test->TestTrue(Label(TEXT("warp capsule sweep stops before the intervening wall")),Warp->GetLocation().X<=125.f-P->GetCapsuleComponent()->GetScaledCapsuleRadius()+1.f);
                    Test->TestTrue(Label(TEXT("published warp translation is bounded")),FVector::Dist(Warp->GetLocation(),P->GetActorLocation())<=P->MaxAttackWarpTranslation+1.f);
                }
            Test->TestTrue(Label(TEXT("root motion does not push the capsule through the wall")),P->GetActorLocation().X<=125.f-P->GetCapsuleComponent()->GetScaledCapsuleRadius()+1.f);
            if (T>2.f)
            { Test->TestTrue(Label(TEXT("wall case authored and cleaned a real warp target")),bA&&Clean(P)); Next(W); }
            break;
        case 17:
            if (!bA && D && P->GetMesh()->GetAnimInstance()->Montage_GetCurrentSection(D->Montage)==TEXT("Descent"))
            { bA=true; Platform->Destroy(); StepStart=W->GetTimeSeconds(); }
            if (bA && D && P->GetCharacterMovement()->IsFalling())
            {
                const float Contact=D->Montage->CompositeSections[D->Montage->GetSectionIndex(TEXT("Land"))].GetTime();
                Test->TestTrue(Label(TEXT("lost dive platform never starts grounded recovery while falling")),A->GetMontagePosition()<Contact);
                if (S>.1f && FMath::IsNearlyEqual(A->GetMontagePosition(),Contact-1.f/60.f,.002f)) bB=true;
            }
            if (bA && !P->GetCharacterMovement()->IsFalling()) bC=true;
            if (T>4.5f && !C->IsMeleeAttackInProgress())
            { Test->TestTrue(Label(TEXT("dive survives platform loss, holds contact pose, then ends after actual landing")),bA&&bB&&bC&&Clean(P)); Next(W); }
            break;
        }
        if (Phase==3 && T>14.f) { Test->AddError(Label(TEXT("case did not reach its physical ending state"))); Next(W); }
        return false;
    }
};

class FMainMapSmoke : public IAutomationLatentCommand
{
    FAutomationTestBase* Test; double Wall=FPlatformTime::Seconds(); int32 Step=0;
    TWeakObjectPtr<ATPCCharacter> Player; double Start=0;
public:
    explicit FMainMapSmoke(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        if (FPlatformTime::Seconds()-Wall>35.) { Test->AddError(TEXT("Default map did not initialize the protagonist")); return true; }
        UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr; if (!W||!W->HasBegunPlay()) return false;
        if (Step==0)
        {
            Player=Cast<ATPCCharacter>(UGameplayStatics::GetPlayerCharacter(W,0)); if (!Player.IsValid()) return false;
            auto* P=Player.Get();
            if (!P->GetCharacterMovement()->IsMovingOnGround()) return false;
            // The normal map contains live opponents. Isolate startup wiring from their
            // legitimate hit locks; combat interruption is exercised in Lifecycle.
            for (TActorIterator<AEnemyCharacter> It(W); It; ++It)
            {
                It->FindComponentByClass<UCombatComponent>()->SetCombatEnabled(false);
                if (auto* AI=Cast<AAIController>(It->GetController()))
                {
                    AI->StopMovement();
                    if (AI->BrainComponent) AI->BrainComponent->StopLogic(TEXT("Sword startup wiring test"));
                }
            }
            Test->TestNotNull(TEXT("Default map has the saved sword action set without test equipment injection"),P->ActionComponent->GetActionSet());
            Test->TestTrue(TEXT("Default map starts with an actual sword actor"),P->EquipmentComponent->GetEquippedWeaponActor()!=nullptr);
            Test->TestTrue(TEXT("Default map starts alive and with stamina"),P->HealthComponent->GetCurrentHealth()>0 && P->StaminaComponent->GetCurrentStamina()>0);
            Test->TestNotNull(TEXT("Heavy, buff and equipment inputs are saved"),P->AirDiveAction.Get());
            P->HandlePrimaryAttack();
            Test->TestTrue(TEXT("Default map primary input reaches Sword.Light.1"),P->ActionComponent->GetActiveDefinition() && P->ActionComponent->GetActiveDefinition()->ActionId==TEXT("Sword.Light.1"));
            Start=W->GetTimeSeconds(); Step=1;
        }
        if (W->GetTimeSeconds()-Start>2.2)
        {
            auto* P=Player.Get();
            Test->AddInfo(FString::Printf(TEXT("Default-map completion: state=%d montage=%s position=%.3f melee=%d block=%d guardHit=%d hp=%.2f"),
                static_cast<int32>(P->ActionComponent->GetActionState()),*GetNameSafe(P->GetMesh()->GetAnimInstance()->GetCurrentActiveMontage()),
                P->ActionComponent->GetMontagePosition(),P->CombatComponent->IsMeleeAttackInProgress(),P->CombatComponent->IsBlocking(),
                P->IsGuardHitReactionActive(),P->HealthComponent->GetCurrentHealth()));
            Test->TestFalse(TEXT("Default-map attack ends without a movement lock"),Player->IsMovementInputLocked()); return true;
        }
        return false;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPCSwordMainMapTest,"ThirdPerson.Sword.PIE.MainMapStartup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTPCSwordMainMapTest::RunTest(const FString&)
{
    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(SwordPIE::FMainMapSmoke(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPCSwordPIELifecycleTest,"ThirdPerson.Sword.PIE.LifecycleAt30_60_120",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTPCSwordPIELifecycleTest::RunTest(const FString&)
{
    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Third/Bosses/Countess/Maps/L_CountessBossTest"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(SwordPIE::FLifecycleScenario(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPCSwordPIECoreTest, "ThirdPerson.Sword.PIE.CoreAt30_60_120",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTPCSwordPIECoreTest::RunTest(const FString&)
{
    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Third/Bosses/Countess/Maps/L_CountessBossTest"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(SwordPIE::FCoreScenario(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
