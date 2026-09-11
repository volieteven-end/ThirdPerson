#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "../Character/TPCCharacter.h"
#include "../Character/TPCCharacterMovementComponent.h"
#include "../AI/EnemyCharacter.h"
#include "../Components/CombatComponent.h"
#include "../Components/ActionComponent.h"
#include "../Components/EquipmentComponent.h"
#include "../Components/HealthComponent.h"
#include "../Components/StaminaComponent.h"
#include "../Weapons/WeaponDefinition.h"

namespace DiveContactPIE
{
struct FSaveScope
{
    FString Original = FCommandLine::Get();
    FString Slot = TEXT("DiveContactAutomation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    FSaveScope() { FCommandLine::Set(*(TEXT("-TPCSaveSlot=") + Slot + TEXT(" ") + Original)); }
    ~FSaveScope() { UGameplayStatics::DeleteGameInSlot(Slot, 0); FCommandLine::Set(*Original); }
};

class FScenario : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TSharedPtr<FSaveScope> SaveScope;
    TWeakObjectPtr<ATPCCharacter> Player;
    TArray<TWeakObjectPtr<AEnemyCharacter>> Enemies;
    TWeakObjectPtr<AStaticMeshActor> Wall;
    const FVector Center = FVector(20000, 0, 0);
    int32 FPS, Stage = 0, Case = 0;
    double Start = 0, WallStart = FPlatformTime::Seconds();
    bool PreviousFixed = FApp::UseFixedTimeStep();
    double PreviousDelta = FApp::GetFixedDeltaTime();
    bool SawDescent = false, SawGroundRecovery = false, SawUpwardBounce = false, SawPrematureLand = false;
    bool SawHeadContact = false;
    float EnemyHealth = 0;
    FString Trace = TEXT("fps,case,time,x,y,z,vx,vy,vz,descending,falling,section,enemy_hp\n");

    FString Label(const TCHAR* What) const { return FString::Printf(TEXT("%d FPS case %d: %s"), FPS, Case, What); }
    AStaticMeshActor* Cube(UWorld* W, const FVector& Location, const FVector& Scale)
    {
        auto* Result = W->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
        Result->SetMobility(EComponentMobility::Movable);
        Result->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
        Result->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
        Result->SetActorScale3D(Scale);
        return Result;
    }
    AEnemyCharacter* SpawnEnemy(UWorld* W, const FVector& Offset)
    {
        UClass* Class = LoadClass<AEnemyCharacter>(nullptr, TEXT("/Game/Third/Character/BP_EnemyCharacter.BP_EnemyCharacter_C"));
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* E = W->SpawnActor<AEnemyCharacter>(Class, Center + Offset + FVector(0, 0, 100), FRotator(0, 180, 0), Params);
        if (!E) return nullptr;
        if (E->GetController()) E->GetController()->Destroy();
        E->FindComponentByClass<UCombatComponent>()->SetCombatEnabled(false);
        auto* H = E->FindComponentByClass<UHealthComponent>();
        H->MaxHealth = 10000.f; H->SetCurrentHealth(10000.f);
        E->SetActorLocation(Center + Offset + FVector(0, 0, E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f));
        Enemies.Add(E);
        return E;
    }
    void Next(UWorld* W)
    {
        for (auto& E : Enemies) if (E.IsValid()) E->Destroy();
        Enemies.Empty();
        if (Wall.IsValid()) Wall->Destroy();
        ++Case; Stage = 1; Start = W->GetTimeSeconds();
    }
public:
    FScenario(FAutomationTestBase* T, int32 Rate, TSharedPtr<FSaveScope> Scope) : Test(T), SaveScope(Scope), FPS(Rate) {}
    ~FScenario() override
    {
        FApp::SetUseFixedTimeStep(PreviousFixed); FApp::SetFixedDeltaTime(PreviousDelta);
        const FString Dir = FPaths::ProjectSavedDir() / TEXT("DiveContact");
        IFileManager::Get().MakeDirectory(*Dir, true);
        FFileHelper::SaveStringToFile(Trace, *(Dir / FString::Printf(TEXT("trace_%d.csv"), FPS)));
    }
    bool Update() override
    {
        if (FPlatformTime::Seconds() - WallStart > 90) { Test->AddError(Label(TEXT("wall timeout"))); return true; }
        UWorld* W = GEditor ? GEditor->PlayWorld.Get() : nullptr;
        if (!W || !W->HasBegunPlay() || W->GetTimeSeconds() < 1.f) return false;
        if (Case == 6) return true;
        if (Stage == 0)
        {
            Player = Cast<ATPCCharacter>(UGameplayStatics::GetPlayerCharacter(W, 0));
            if (!Player.IsValid()) return false;
            auto* P = Player.Get();
            auto* Saved = LoadObject<UWeaponDefinition>(nullptr, TEXT("/Game/Third/DataAsset/DA_TestSword.DA_TestSword"));
            auto* Fixture = Saved ? DuplicateObject<UWeaponDefinition>(Saved, GetTransientPackage()) : nullptr;
            if (!Test->TestNotNull(TEXT("saved sword available"), Fixture)) return true;
            Fixture->Damage = 25.f; Fixture->DiveLandingImpactRadius = 120.f;
            if (!Test->TestTrue(TEXT("real sword/actions equipped"), P->EquipmentComponent->EquipWeapon(Fixture))) return true;
            P->EquipmentComponent->SetWeaponDrawn(true);
            P->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
            P->bEnableRootMotionTurn = false;
            Cube(W, Center + FVector(0, 0, -20), FVector(30, 30, .4f));
            W->GetWorldSettings()->MinUndilatedFrameTime = 0; W->GetWorldSettings()->MaxUndilatedFrameTime = 1;
            FApp::SetUseFixedTimeStep(true); FApp::SetFixedDeltaTime(1. / FPS);
            Stage = 1; Start = W->GetTimeSeconds();
        }
        auto* P = Player.Get();
        if (!P) { Test->AddError(TEXT("player disappeared")); return true; }
        auto* M = P->GetCharacterMovement();
        auto* C = P->CombatComponent.Get();
        if (Stage == 1)
        {
            if (W->GetTimeSeconds() - Start < .25f) return false;
            C->CancelActiveAttack(); P->ClearMoveInput(); P->EndJump();
            P->StaminaComponent->SetCurrentStamina(100.f);
            P->LockedTarget = nullptr;
            // The authored start/loop carries the player forward ~2.1 m before
            // head height. Position the target in that real trajectory, not under
            // the takeoff point (which only tests an empty-ground dive).
            const FVector TargetOffset(Case < 4 ? 210.f : 0.f, 0, 0);
            auto* E = SpawnEnemy(W, TargetOffset);
            if (!Test->TestNotNull(Label(TEXT("enemy spawned")), E)) return true;
            EnemyHealth = E->FindComponentByClass<UHealthComponent>()->GetCurrentHealth();
            Test->AddInfo(FString::Printf(TEXT("enemy capsule radius=%.1f halfHeight=%.1f stepUp=%d"),
                E->GetCapsuleComponent()->GetScaledCapsuleRadius(), E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), int32(E->GetCapsuleComponent()->CanCharacterStepUpOn)));
            // Real physical falls: center, edge, wall blocking the preferred exit, group,
            // normal fall (must retain engine behavior), and dive interrupted above the head.
            if (Case == 2) Wall = Cube(W, Center + TargetOffset + FVector(80, 0, 175), FVector(.4f, 4, 3.5f));
            if (Case == 3) { SpawnEnemy(W, TargetOffset + FVector(-80, 0, 0)); SpawnEnemy(W, TargetOffset + FVector(0, 80, 0)); }
            P->SetActorLocationAndRotation(Center + FVector(0, Case == 1 ? 40.f : 0.f, 600), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
            if (P->GetController()) P->GetController()->SetControlRotation(FRotator::ZeroRotator);
            M->SetMovementMode(MOVE_Falling); M->StopMovementImmediately();
            SawDescent = SawGroundRecovery = SawUpwardBounce = SawPrematureLand = SawHeadContact = false;
            if (Case != 4)
            {
                P->HandleAirDiveAttack();
                if (!Test->TestTrue(Label(TEXT("real dive request accepted")), C->IsAirDiveDescending())) return true;
            }
            Stage = 2; Start = W->GetTimeSeconds(); return false;
        }
        const double T = W->GetTimeSeconds() - Start;
        auto* E = Enemies[0].Get();
        if (!E) { Test->AddError(Label(TEXT("enemy disappeared"))); return true; }
        const FVector Position = P->GetActorLocation(), Velocity = M->Velocity;
        auto* Anim = P->GetMesh()->GetAnimInstance();
        const FName Section = Anim ? Anim->Montage_GetCurrentSection() : NAME_None;
        Trace += FString::Printf(TEXT("%d,%d,%.4f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%d,%s,%.2f\n"),
            FPS, Case, T, Position.X-Center.X, Position.Y-Center.Y, Position.Z, Velocity.X, Velocity.Y, Velocity.Z,
            C->IsAirDiveDescending(), M->IsFalling(), *Section.ToString(), E->FindComponentByClass<UHealthComponent>()->GetCurrentHealth());
        if (Velocity.Z < -100.f) SawDescent = true;
        if (SawDescent && Velocity.Z > 20.f) SawUpwardBounce = true;
        const float Feet = M->GetActorFeetLocation().Z;
        if (Feet > 40.f && Feet < E->GetActorLocation().Z + E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 15.f &&
            FVector::Dist2D(Position, E->GetActorLocation()) < M->GetPawnOwner()->GetSimpleCollisionRadius() + E->GetCapsuleComponent()->GetScaledCapsuleRadius()) SawHeadContact = true;
        if (Section == TEXT("Land"))
        {
            if (Feet > 10.f) SawPrematureLand = true;
            else SawGroundRecovery = true;
        }
        if (Case == 5 && T > .12f && C->IsAirDiveDescending()) C->CancelActiveAttack();
        if (Case == 4 || Case == 5)
        {
            if (T > 2.f)
            {
                Test->TestTrue(Label(TEXT("ordinary/cancelled fall retains automatic head bounce")), SawUpwardBounce);
                Next(W);
            }
            return false;
        }
        if (SawDescent && T > .5f && !C->IsMeleeAttackInProgress() && !M->IsFalling())
        {
            Test->TestFalse(Label(TEXT("no upward head bounce during dive")), SawUpwardBounce);
            Test->TestFalse(Label(TEXT("enemy is never treated as ground recovery")), SawPrematureLand);
            Test->TestTrue(Label(TEXT("real ground recovery played")), SawGroundRecovery);
            Test->TestTrue(Label(TEXT("trajectory actually intersects head region")), SawHeadContact);
            Test->TestTrue(Label(TEXT("ended on floor, not head")), Feet < 10.f);
            Test->TestTrue(Label(TEXT("landing correction stays local")), FVector::Dist2D(Position, E->GetActorLocation()) < 200.f);
            Test->TestTrue(Label(TEXT("real landing impact damages target")), E->FindComponentByClass<UHealthComponent>()->GetCurrentHealth() < EnemyHealth);
            if (Case == 2) Test->TestTrue(Label(TEXT("wall remains solid")), Position.X - Center.X < 270.f - P->GetCapsuleComponent()->GetScaledCapsuleRadius() + 2.f);
            Test->AddInfo(Label(TEXT("completed"))); Next(W); return false;
        }
        if (T > 5.f)
        {
            Test->AddError(FString::Printf(TEXT("%s position=%s velocity=%s section=%s descending=%d"), *Label(TEXT("dive failed to reach ground/recover")),
                *(Position-Center).ToString(), *Velocity.ToString(), *Section.ToString(), C->IsAirDiveDescending()));
            Next(W);
        }
        return false;
    }
};
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FTPCDiveContactTest, "ThirdPerson.Combat.PIE.DiveHeadContact",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FTPCDiveContactTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{ for (int32 FPS : {30, 60, 120}) { Names.Add(FString::Printf(TEXT("%dFPS"), FPS)); Commands.Add(FString::FromInt(FPS)); } }
bool FTPCDiveContactTest::RunTest(const FString& Parameters)
{
    auto Scope = MakeShared<DiveContactPIE::FSaveScope>();
    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Third/Bosses/Countess/Maps/L_CountessBossTest"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(DiveContactPIE::FScenario(this, FCString::Atoi(*Parameters), Scope));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
