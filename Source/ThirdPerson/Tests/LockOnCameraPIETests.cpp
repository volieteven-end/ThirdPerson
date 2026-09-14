// 锁定相机回归：在不同帧率检查目标切换、自由观察和镜头跟随，避免锁定强夺镜头控制。
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "InputActionValue.h"
#include "InputKeyEventArgs.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "MotionWarpingComponent.h"
#include "UnrealClient.h"
#include "../Character/TPCCharacter.h"
#include "../Character/TPCPlayerController.h"
#include "../AI/EnemyCharacter.h"
#include "../Components/ActionComponent.h"
#include "../Components/CombatComponent.h"
#include "../Components/EquipmentComponent.h"
#include "../Components/HealthComponent.h"
#include "../Components/StaminaComponent.h"
#include "../Weapons/WeaponDefinition.h"

namespace LockOnCameraTests
{
class FScenario : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TWeakObjectPtr<ATPCCharacter> Player;
    TWeakObjectPtr<ATPCPlayerController> Controller;
    TWeakObjectPtr<AEnemyCharacter> Target, Neighbour;
    int32 Stage = 0, Case = 0, Rate = 0, Step = 0;
    const int32 Rates[3] = {30, 60, 120};
    double Start = 0., WallStart = FPlatformTime::Seconds();
    bool bPreviousFixed = FApp::UseFixedTimeStep();
    double PreviousDelta = FApp::GetFixedDeltaTime();
    FRotator SavedView;
    FVector StartLocation;
    float SavedYaw = 0.f;
    bool bSawWarp = false, bCaptured = false;

    FString Label(const TCHAR* Text) const
    { return FString::Printf(TEXT("%d FPS case %d: %s"), Rates[Rate], Case, Text); }

    void Key(FKey Key, bool bDown)
    {
        if (Controller.IsValid()) Controller->InputKey(FInputKeyEventArgs::CreateSimulated(
            Key, bDown ? IE_Pressed : IE_Released, bDown ? 1.f : 0.f));
    }

    void Mouse(float X, float Y)
    {
        Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseX, IE_Axis, X, 1));
        Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseY, IE_Axis, Y, 1));
    }

    void Release()
    {
        Key(EKeys::W, false); Key(EKeys::LeftAlt, false);
        if (Player.IsValid()) { Player->ClearMoveInput(); Player->FinishLookInput(); }
    }

    static void Place(AActor* Actor, const FVector& Position)
    { Actor->SetActorLocationAndRotation(Position, FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics); }

    void Next(UWorld* World)
    {
        Release();
        Test->AddInfo(Label(TEXT("completed")));
        if (++Case == 9) { Case = 0; ++Rate; }
        Stage = 1; Step = 0; Start = World->GetTimeSeconds();
    }

public:
    explicit FScenario(FAutomationTestBase* InTest) : Test(InTest) {}
    ~FScenario() override
    {
        Release();
        FApp::SetUseFixedTimeStep(bPreviousFixed);
        FApp::SetFixedDeltaTime(PreviousDelta);
    }

    bool Update() override
    {
        if (Rate == 3) return true;
        if (FPlatformTime::Seconds() - WallStart > 120.)
        { Test->AddError(Label(TEXT("scenario timed out"))); return true; }
        UWorld* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
        if (!World || !World->HasBegunPlay()) return false;
        if (Stage == 0)
        {
            Player = Cast<ATPCCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));
            Controller = Cast<ATPCPlayerController>(UGameplayStatics::GetPlayerController(World, 0));
            if (!Player.IsValid() || !Controller.IsValid()) return false;
            // Fixtures live only in PIE; never save or change the user's level/assets.
            for (TActorIterator<AEnemyCharacter> It(World); It; ++It)
            { if (It->GetController()) It->GetController()->Destroy(); It->Destroy(); }
            UClass* EnemyClass = LoadClass<AEnemyCharacter>(nullptr,
                TEXT("/Game/Third/Character/BP_EnemyCharacter.BP_EnemyCharacter_C"));
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Target = World->SpawnActor<AEnemyCharacter>(EnemyClass, FVector(300,-900,98), FRotator::ZeroRotator, Params);
            Neighbour = World->SpawnActor<AEnemyCharacter>(EnemyClass, FVector(300,-650,98), FRotator::ZeroRotator, Params);
            if (!Target.IsValid() || !Neighbour.IsValid())
            { Test->AddError(TEXT("Lock-on fixtures failed to spawn")); return true; }
            for (auto* Enemy : {Target.Get(), Neighbour.Get()})
            {
                if (Enemy->GetController()) Enemy->GetController()->Destroy();
                Enemy->GetCharacterMovement()->DisableMovement();
                auto* Health = Enemy->FindComponentByClass<UHealthComponent>();
                Health->MaxHealth = 10000.f; Health->SetCurrentHealth(10000.f);
            }
            auto* P = Player.Get();
            P->bEnableRootMotionTurn = false;
            P->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
            P->EquipmentComponent->EquipWeapon(LoadObject<UWeaponDefinition>(nullptr,
                TEXT("/Game/Third/DataAsset/DA_TestSword.DA_TestSword")));
            P->HealthComponent->MaxHealth = 10000.f; P->HealthComponent->SetCurrentHealth(10000.f);
            Test->AddInfo(FString::Printf(TEXT("Saved player settings: lock rotation %.1f, magnetism range %.1f, stop %.1f, warp limit %.1f"),
                P->LockOnRotationSpeed, P->AttackMagnetismRange, P->AttackMagnetismStopDistance, P->MaxAttackWarpTranslation));
            Stage = 1; Start = World->GetTimeSeconds();
        }

        auto* P = Player.Get(); auto* PC = Controller.Get(); auto* Movement = P->GetCharacterMovement();
        if (Stage == 1)
        {
            FApp::SetUseFixedTimeStep(true); FApp::SetFixedDeltaTime(1. / Rates[Rate]);
            if (World->GetTimeSeconds() - Start < .2) return false;
            Release(); P->CombatComponent->CancelActiveAttack(); P->StopBlock(); P->StopSprint();
            P->StaminaComponent->SetCurrentStamina(100.f); P->LastDashTime = -BIG_NUMBER;
            Place(P, FVector(0,-900,98)); Place(Target.Get(), FVector(300,-900,98));
            Place(Neighbour.Get(), FVector(300,-650,98));
            Movement->SetMovementMode(MOVE_Walking); Movement->StopMovementImmediately();
            PC->SetControlRotation(FRotator(-15,0,0)); P->LockedTarget = Target.Get();
            StartLocation = P->GetActorLocation(); bSawWarp = bCaptured = false;
            Stage = 2; Start = World->GetTimeSeconds(); return false;
        }
        const double Time = World->GetTimeSeconds() - Start;
        if (Stage == 2)
        {
            if (Time < .2) return false; // Allow the real spring arm/input/view to update.
            SavedView = PC->GetControlRotation(); SavedYaw = P->GetActorRotation().Yaw;
            if (Case == 0) Mouse(60.f, 60.f); // Exercise both axes beyond the saved 0.07 mouse sensitivity.
            if (Case == 1 || Case == 2 || Case == 3 || Case == 4)
            { PC->SetControlRotation(FRotator(-35,90,0)); SavedView = PC->GetControlRotation(); }
            if (Case == 2) Key(EKeys::W, true);
            if (Case == 3) P->HandlePrimaryAttack();
            if (Case == 4) P->Dash();
            if (Case == 5) Key(EKeys::LeftAlt, true);
            if (Case == 6)
            { Place(Target.Get(), P->GetActorLocation() + FVector(P->LockOnRadius + 500.f,0,0)); }
            if (Case == 7)
            {
                Place(Target.Get(), P->GetActorLocation() + FVector(100,0,0));
                P->HandlePrimaryAttack();
                const auto* Warp = P->MotionWarpingComponent->FindWarpTarget(P->AttackWarpTargetName);
                Test->TestNotNull(Label(TEXT("close attack creates its warp target")), Warp);
                if (Warp) Test->TestTrue(Label(TEXT("close enemy never pulls the player backward")),
                    FVector::DotProduct(Warp->GetLocation() - P->GetActorLocation(), P->GetActorForwardVector()) >= -.01f);
            }
            if (Case == 8)
            {
                const float CapsuleContact = P->GetCapsuleComponent()->GetScaledCapsuleRadius() +
                    Target->GetCapsuleComponent()->GetScaledCapsuleRadius();
                Place(Target.Get(), P->GetActorLocation() + FVector(CapsuleContact + .5f,0,0));
                P->HandlePrimaryAttack();
                const auto* Warp = P->MotionWarpingComponent->FindWarpTarget(P->AttackWarpTargetName);
                Test->TestNotNull(Label(TEXT("capsule-contact attack keeps a safe warp target")), Warp);
                if (Warp)
                {
                    Test->TestTrue(Label(TEXT("contact preserves the existing collision safety margin")),
                        FVector::Dist2D(Warp->GetLocation(), Target->GetActorLocation()) >= CapsuleContact + 7.f);
                    Test->TestTrue(Label(TEXT("contact correction is only the small safety margin, not a retreat")),
                        FVector::Dist2D(Warp->GetLocation(), P->GetActorLocation()) <= 8.1f);
                }
            }
            Stage = 3; Start = World->GetTimeSeconds(); return false;
        }

        if (Case == 0)
        {
            if (Step == 0 && Time > .15)
            {
                const FRotator View = PC->GetControlRotation();
                Test->AddInfo(FString::Printf(TEXT("%d FPS mouse camera change: yaw %.2f, pitch %.2f"), Rates[Rate],
                    FMath::FindDeltaAngleDegrees(SavedView.Yaw, View.Yaw), FMath::FindDeltaAngleDegrees(SavedView.Pitch, View.Pitch)));
                Test->TestTrue(Label(TEXT("real mouse input rotates locked camera horizontally")),
                    FMath::Abs(FMath::FindDeltaAngleDegrees(SavedView.Yaw, View.Yaw)) > 3.f);
                Test->TestTrue(Label(TEXT("real mouse input pitches locked camera vertically")),
                    FMath::Abs(FMath::FindDeltaAngleDegrees(SavedView.Pitch, View.Pitch)) > 3.f);
                Test->TestEqual(Label(TEXT("ordinary mouse movement does not switch the enemy")), P->GetLockedTarget(), Target.Get());
                SavedView = View; Step = 1;
            }
            if (Time > .7)
            {
                Test->TestTrue(Label(TEXT("camera does not recenter after releasing the mouse")), SavedView.Equals(PC->GetControlRotation(), .1f));
                Next(World);
            }
        }
        else if (Case == 1)
        {
            if (Step == 0 && Time > .4)
            {
                Test->TestTrue(Label(TEXT("camera can orbit 90 degrees without turning the player away")), FMath::Abs(P->GetActorRotation().Yaw) < 3.f);
                Test->TestTrue(Label(TEXT("camera yaw and pitch stay under player control")), SavedView.Equals(PC->GetControlRotation(), .1f));
                Place(Target.Get(), P->GetActorLocation() + FVector(300,180,100)); Step = 1;
            }
            if (Time > 1.)
            {
                const float TargetYaw = (Target->GetActorLocation() - P->GetActorLocation()).Rotation().Yaw;
                Test->TestTrue(Label(TEXT("body tracks a moving elevated enemy independently")),
                    FMath::Abs(FMath::FindDeltaAngleDegrees(TargetYaw, P->GetActorRotation().Yaw)) < 3.f);
                Test->TestTrue(Label(TEXT("body stays upright")), FMath::Abs(P->GetActorRotation().Pitch) < .1f && FMath::Abs(P->GetActorRotation().Roll) < .1f);
                Test->TestTrue(Label(TEXT("target height does not flatten or tilt the camera")), SavedView.Equals(PC->GetControlRotation(), .1f));
                Next(World);
            }
        }
        else if (Case == 2 && Time > .45)
        {
            const FVector Delta = P->GetActorLocation() - StartLocation;
            Test->TestTrue(Label(TEXT("W still follows the freely rotated camera")), Delta.Y > 25.f && FMath::Abs(Delta.X) < 10.f);
            const float TargetYaw = (Target->GetActorLocation() - P->GetActorLocation()).Rotation().Yaw;
            Test->TestTrue(Label(TEXT("movement remains target-facing strafing")),
                FMath::Abs(FMath::FindDeltaAngleDegrees(TargetYaw, P->GetActorRotation().Yaw)) < 15.f);
            Test->TestFalse(Label(TEXT("movement does not borrow camera yaw for body rotation")), Movement->bUseControllerDesiredRotation);
            Next(World);
        }
        else if (Case == 3)
        {
            if (Time < .18)
                if (const auto* Warp = P->MotionWarpingComponent->FindWarpTarget(P->AttackWarpTargetName))
                {
                    bSawWarp = true;
                    Test->TestTrue(Label(TEXT("locked attack pull is limited to a light 60 cm assist")),
                        FVector::Dist2D(Warp->GetLocation(), P->GetActorLocation()) <= 60.1f);
                    Test->TestTrue(Label(TEXT("retargeting cannot renew the pull budget every frame")),
                        FVector::Dist2D(Warp->GetLocation(), StartLocation) <= 60.1f);
                    Test->TestTrue(Label(TEXT("attack faces the enemy rather than the side-facing camera")),
                        FMath::Abs(Warp->GetRotation().Rotator().Yaw) < 3.f);
                }
            if (Step == 0 && Time > .3)
            { SavedYaw = P->GetActorRotation().Yaw; Place(Target.Get(), P->GetActorLocation() + FVector(-250,0,0)); Step = 1; }
            if (Step == 1 && Time > .5)
            {
                Test->TestTrue(Label(TEXT("lock facing does not override the active attack rotation window")),
                    FMath::Abs(FMath::FindDeltaAngleDegrees(SavedYaw, P->GetActorRotation().Yaw)) < 8.f);
                Step = 2;
            }
            if (Time > 1.5)
            {
                Test->TestTrue(Label(TEXT("actual attack published an assist target")), bSawWarp);
                Test->TestTrue(Label(TEXT("attack never steers or flattens the camera")), SavedView.Equals(PC->GetControlRotation(), .1f));
                Next(World);
            }
        }
        else if (Case == 4)
        {
            if (Step == 0 && Time > .08)
            { Test->TestTrue(Label(TEXT("actual root-motion dodge started")), P->IsDashing()); Place(Target.Get(), P->GetActorLocation() + FVector(0,400,0)); Step = 1; }
            if (Step == 1 && Time > .2)
            {
                Test->TestTrue(Label(TEXT("lock tracking cannot rotate a running dodge")),
                    FMath::Abs(FMath::FindDeltaAngleDegrees(SavedYaw, P->GetActorRotation().Yaw)) < 5.f);
                Step = 2;
            }
            if (Time > 1.5)
            {
                Test->TestFalse(Label(TEXT("dodge completed")), P->IsDashing());
                Test->TestTrue(Label(TEXT("dodge leaves manual camera untouched")), SavedView.Equals(PC->GetControlRotation(), .1f));
                Next(World);
            }
        }
        else if (Case == 5)
        {
            if (Step == 0 && Time > .08) { Mouse(80.f,0.f); Step = 1; }
            if (Time > .3)
            {
                if (P->GetLockedTarget() != Neighbour.Get())
                {
                    int32 Width = 0, Height = 0; PC->GetViewportSize(Width, Height);
                    FVector2D CurrentScreen, NextScreen;
                    const bool bCurrentVisible = PC->ProjectWorldLocationToScreen(Target->GetLockOnAimPoint(), CurrentScreen, true);
                    const bool bNextVisible = PC->ProjectWorldLocationToScreen(Neighbour->GetLockOnAimPoint(), NextScreen, true);
                    FHitResult Hit;
                    FCollisionQueryParams Query(SCENE_QUERY_STAT(LockOnFixtureSight), false, P);
                    World->LineTraceSingleByChannel(Hit, P->FollowCamera->GetComponentLocation(), Neighbour->GetLockOnAimPoint(), ECC_Visibility, Query);
                    Test->AddInfo(FString::Printf(TEXT("Switch fixture: viewport %dx%d, projected %d/%d at %s/%s, sight hit %s, Alt %d, camera %s"),
                        Width, Height, bCurrentVisible, bNextVisible, *CurrentScreen.ToString(), *NextScreen.ToString(),
                        *GetNameSafe(Hit.GetActor()), PC->IsInputKeyDown(EKeys::LeftAlt), *P->FollowCamera->GetComponentLocation().ToString()));
                }
                Test->TestEqual(Label(TEXT("Alt plus horizontal mouse deliberately switches target")), P->GetLockedTarget(), Neighbour.Get());
                Test->TestTrue(Label(TEXT("switch gesture does not swing the camera")), SavedView.Equals(PC->GetControlRotation(), .1f));
                Next(World);
            }
        }
        else if (Case == 6 && Time > .3)
        {
            Test->TestNull(Label(TEXT("out-of-range target releases lock")), P->GetLockedTarget());
            Test->TestTrue(Label(TEXT("unlock leaves camera at the last manual orientation")), SavedView.Equals(PC->GetControlRotation(), .1f));
            Test->TestTrue(Label(TEXT("unlocked locomotion restores movement-facing rotation")), Movement->bOrientRotationToMovement);
            Next(World);
        }
        else if ((Case == 7 || Case == 8) && Time > 1.5) Next(World);

        if (FParse::Param(FCommandLine::Get(), TEXT("LockOnVisualAudit")) && Rate == 1 && Case == 7)
        {
            if (Step == 0 && Time > .35) { PC->SetControlRotation(FRotator(-30,25,0)); Step = 1; }
            // Wait for camera motion blur / temporal history after the audit's deliberate orbit.
            if (Time > .65 && !bCaptured)
            {
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("LockOnCamera/HighAngleCombat.png"), false, false);
                bCaptured = true;
            }
        }
        return false;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPCLockOnFreeCameraTest, "ThirdPerson.LockOn.PIE.FreeCameraAt30_60_120",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FTPCLockOnFreeCameraTest::RunTest(const FString&)
{
    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(LockOnCameraTests::FScenario(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
