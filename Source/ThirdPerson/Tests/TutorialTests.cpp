// 教程纯逻辑回归：覆盖事件去重、课程解锁和落地命中安全，不靠模拟完成信号跳过玩法。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../Tutorial/TutorialCourse.h"
#include "../Tutorial/TutorialGameMode.h"
#include "../Components/ActionComponent.h"
#include "../Components/InventoryComponent.h"
#include "../Components/HealthComponent.h"
#include "../Components/CombatComponent.h"
#include "../Components/EquipmentComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "../Character/TPCCharacter.h"
#include "../Weapons/WeaponDefinition.h"
#include "Animation/AnimMontage.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTutorialProgressTest,"ThirdPerson.Tutorial.ProgressAndReplay",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTutorialProgressTest::RunTest(const FString&)
{
    auto* C = NewObject<UTutorialCourse>(); UTutorialCourse::PopulateDefaults(*C);
    FString Error; TestTrue(TEXT("Default course validates"),C->IsValidCourse(Error));
    TestEqual(TEXT("Six basics plus optional air lesson"),C->Lessons.Num(),7);
    FTutorialProgress P; P.Initialize(C);
    TestFalse(TEXT("Cannot start a locked lesson"),P.StartLesson(3));
    TestFalse(TEXT("Not completed at initialization"),P.BasicsFinished());
    auto O = *P.CurrentObjective();
    TestEqual(TEXT("Wrong action cannot progress"),P.Observe(ETutorialSignal::EnemyDefeated,1,O.TargetId),ETutorialProgressResult::Ignored);
    TestEqual(TEXT("Wrong target cannot progress"),P.Observe(O.Signal,1,TEXT("WrongMarker")),ETutorialProgressResult::Ignored);
    TestEqual(TEXT("Zero serial is invalid"),P.Observe(O.Signal,0,O.TargetId),ETutorialProgressResult::Ignored);
    P.Skip(); TestEqual(TEXT("Skip is not recorded as a completion"),P.Status[0],ETutorialLessonStatus::Skipped);
    TestEqual(TEXT("Skip unlocks the next station"),P.Lesson,1);
    uint64 Serial=10;
    while (P.Lesson != INDEX_NONE)
    {
        const int32 L=P.Lesson; const int32 Step=P.Objective; O=*P.CurrentObjective();
        for (int32 N=0;N<O.RequiredCount;++N)
        {
            ++Serial; auto Result=P.Observe(O.Signal,Serial,O.TargetId);
            TestTrue(TEXT("Valid result progresses"),Result!=ETutorialProgressResult::Ignored);
            if (P.Lesson==L && P.Objective==Step)
                TestEqual(TEXT("Repeated hit serial cannot increment a multi-hit objective"),P.Observe(O.Signal,Serial,O.TargetId),ETutorialProgressResult::Ignored);
        }
    }
    TestTrue(TEXT("All basics accounted for"),P.BasicsFinished());
    TestTrue(TEXT("Optional air arena unlocks only after basics"),P.IsUnlocked(6));
    TestEqual(TEXT("Optional arena not falsely completed"),P.Status[6],ETutorialLessonStatus::Available);
    TestTrue(TEXT("Replay skipped station"),P.StartLesson(0,true));
    O=*P.CurrentObjective(); P.Observe(O.Signal,++Serial,O.TargetId); P.Retry();
    TestEqual(TEXT("Retry resets only the active objective"),P.Objective,0);
    TestEqual(TEXT("Retry preserves completed stations"),P.Status[5],ETutorialLessonStatus::Completed);
    while(P.CurrentObjective()) { O=*P.CurrentObjective(); P.Observe(O.Signal,++Serial,O.TargetId); }
    TestEqual(TEXT("Replay replaces skipped with completed"),P.Status[0],ETutorialLessonStatus::Completed);
    TestEqual(TEXT("Replay returns to free practice, not the next mandatory station"),P.Lesson,INDEX_NONE);
    P.StartLesson(1,true); P.Skip();
    TestEqual(TEXT("Skipping a replay never erases an earned completion"),P.Status[1],ETutorialLessonStatus::Completed);
    P.Initialize(C); TestFalse(TEXT("Restart course clears completion"),P.BasicsFinished());
    TestFalse(TEXT("Restart closes optional gate"),P.IsUnlocked(6));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTutorialPolicyAndEventsTest,"ThirdPerson.Tutorial.PolicyAndObservationEvents",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTutorialPolicyAndEventsTest::RunTest(const FString&)
{
    TestTrue(TEXT("Campaign persistence default unchanged"),GetDefault<ATPCGameMode>()->bUsePersistentPlayerSave);
    TestTrue(TEXT("Campaign victory default unchanged"),GetDefault<ATPCGameMode>()->bEnableVictoryProgress);
    TestFalse(TEXT("Tutorial cannot load or save campaign progress"),GetDefault<ATutorialGameMode>()->bUsePersistentPlayerSave);
    TestFalse(TEXT("Tutorial cannot trigger campaign victory"),GetDefault<ATutorialGameMode>()->bEnableVictoryProgress);
    TestEqual(TEXT("Ordinary weapons retain their original blade-only behavior"),GetDefault<UWeaponDefinition>()->DiveLandingImpactRadius,0.f);
    UWorld* W=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* P=W->SpawnActor<ATPCCharacter>(ATPCCharacter::StaticClass(),FVector::ZeroVector,FRotator::ZeroRotator,Params);
    if (!TestNotNull(TEXT("Player fixture"),P)) return false;
    int32 Plays=0, Uses=0; float Healing=0; int32 Remaining=-1;
    P->ActionComponent->OnActionPlaybackStarted.AddLambda([&](uint64,ETPCActionState,const UActionDefinition*){++Plays;});
    const uint64 Action=P->ActionComponent->BeginAction(ETPCActionState::Attack);
    TestEqual(TEXT("Intent / BeginAction alone is not successful playback"),Plays,0);
    P->ActionComponent->BindMontage(Action,nullptr,INDEX_NONE); TestEqual(TEXT("Failed playback is silent"),Plays,0);
    auto* Montage=NewObject<UAnimMontage>();
    P->ActionComponent->BindMontage(Action,Montage,17); P->ActionComponent->BindMontage(Action,Montage,17);
    TestEqual(TEXT("Bound playback broadcasts once"),Plays,1);
    P->ActionComponent->BindMontage(Action+99,Montage,18); TestEqual(TEXT("Stale binding cannot notify"),Plays,1);
    auto* Bag=P->InventoryComponent.Get();
    Bag->AddItem(Bag->HealthPotionDefinition,2);
    Bag->OnConsumableUsed.AddLambda([&](UItemDefinition*,float H,int32 N){++Uses;Healing=H;Remaining=N;});
    P->HealthComponent->SetCurrentHealth(100.f);
    TestFalse(TEXT("Full-health Q does not succeed"),Bag->UseFirstConsumable()); TestEqual(TEXT("No fake potion signal"),Uses,0);
    P->HealthComponent->SetCurrentHealth(50.f); TestTrue(TEXT("A real potion is consumed"),Bag->UseFirstConsumable());
    TestEqual(TEXT("Exactly one successful signal"),Uses,1); TestEqual(TEXT("Actual healing is reported"),Healing,25.f);
    TestEqual(TEXT("Event sees post-consumption quantity"),Remaining,1);
    P->ActionComponent->OnActionPlaybackStarted.Clear(); Bag->OnConsumableUsed.Clear();
    GEngine->DestroyWorldContext(W); W->DestroyWorld(false); if(W->IsRooted())W->RemoveFromRoot();
    return true;
}
struct FTutorialImpactTestAccess
{
    static void Tick(UCombatComponent& C) { C.TickComponent(1.f/60,LEVELTICK_All,nullptr); }
    static void Prepare(UCombatComponent& C, bool bLanded, FName Group, bool bWindow = true)
    {
        C.bCombatEnabled = true; C.bMeleeAttackInProgress = true;
        C.bAttackWindowActive = bWindow; C.ActiveAttackType = EActiveCombatAttackType::AirDive;
        C.bAirDiveLanded = bLanded; C.ActiveHitGroup = Group;
        C.HitActors.Reset(); C.HitGroups.Reset();
        // The non-impact path has no blade in this fixture, so it must end silently.
        C.bUseWeaponBladeTrace = true; C.ActiveWeaponTraceMesh.Reset();
    }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTutorialLandingImpactTest,"ThirdPerson.Tutorial.LandingImpactSafety",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTutorialLandingImpactTest::RunTest(const FString&)
{
    struct FFixture
    {
        UWorld* World = UWorld::CreateWorld(EWorldType::Game,false);
        FFixture() { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
        ~FFixture() { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); if(World->IsRooted())World->RemoveFromRoot(); }
    } Fixture;
    UWorld* W = Fixture.World;
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Player = W->SpawnActor<ATPCCharacter>(ATPCCharacter::StaticClass(),FVector(0,0,100),FRotator::ZeroRotator,Params);
    if (!TestNotNull(TEXT("Impact player"),Player)) return false;
    auto* Weapon = NewObject<UWeaponDefinition>(Player);
    Weapon->Damage = 20.f; Weapon->DiveLandingImpactRadius = 120.f;
    Player->EquipmentComponent->EquipWeapon(Weapon);
    auto* Target = W->SpawnActor<AActor>();
    auto* Capsule = NewObject<UCapsuleComponent>(Target);
    Target->SetRootComponent(Capsule); Capsule->InitCapsuleSize(30.f,90.f);
    Capsule->SetCollisionProfileName(TEXT("Pawn")); Capsule->RegisterComponent();
    Target->SetActorLocation(FVector(100,0,100));
    auto* Health = NewObject<UHealthComponent>(Target); Health->RegisterComponent();
    auto* Wall = W->SpawnActor<AActor>();
    auto* Box = NewObject<UBoxComponent>(Wall);
    Wall->SetRootComponent(Box); Box->SetBoxExtent(FVector(5,100,120));
    Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent();
    Wall->SetActorLocation(FVector(50,0,100)); Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto* Combat = Player->CombatComponent.Get();
    const auto Tick = [&] { FTutorialImpactTestAccess::Tick(*Combat); };
    const auto NoHit = [&](const TCHAR* Label, bool Landed, FName Group, bool Window = true)
    {
        Health->SetCurrentHealth(100.f); FTutorialImpactTestAccess::Prepare(*Combat,Landed,Group,Window);
        Tick(); TestEqual(Label,Health->GetCurrentHealth(),100.f);
    };
    NoHit(TEXT("No impact before real landing"),false,TEXT("Landing"));
    NoHit(TEXT("No impact in primary airborne hit group"),true,TEXT("Primary"));
    NoHit(TEXT("No impact outside authored damage window"),true,TEXT("Landing"),false);
    Weapon->DiveLandingImpactRadius = 0;
    NoHit(TEXT("Default-radius weapons never use landing impact"),true,TEXT("Landing"));
    Weapon->DiveLandingImpactRadius = 120;
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    NoHit(TEXT("A wall occludes the impact"),true,TEXT("Landing"));
    Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Target->SetActorLocation(FVector(400,0,100));
    NoHit(TEXT("Targets outside the impact radius are not damaged"),true,TEXT("Landing"));
    Target->SetActorLocation(FVector(100,0,100));
    FTutorialImpactTestAccess::Prepare(*Combat,true,TEXT("Landing")); Tick();
    const float AfterFirstHit = Health->GetCurrentHealth();
    TestTrue(TEXT("Physical in-range landing deals real health damage"),AfterFirstHit < 100.f);
    Tick(); TestEqual(TEXT("Repeated frames cannot deal a second hit"),Health->GetCurrentHealth(),AfterFirstHit);
    return true;
}
#endif
