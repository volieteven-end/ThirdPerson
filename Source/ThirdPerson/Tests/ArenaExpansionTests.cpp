#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "../Arena/ArenaActors.h"
#include "../Arena/ArenaGameMode.h"
#include "../Arena/ArenaTravelSubsystem.h"
#include "../Arena/ArenaAssetTools.h"
#include "../Tutorial/TutorialDirector.h"
#include "../Character/TPCCharacter.h"
#include "../Character/TPCPlayerController.h"
#include "../AI/EnemyCharacter.h"
#include "../Boss/CountessBossCharacter.h"
#include "../Boss/BossActionComponent.h"
#include "../Components/HealthComponent.h"
#include "../Components/StaminaComponent.h"
#include "../Components/InventoryComponent.h"
#include "../Components/EquipmentComponent.h"
#include "../Components/LevelComponent.h"
#include "../Save/TPCSaveGame.h"
#include "../Save/TPCSaveSlots.h"
#include "../Save/TPCPlayerProgress.h"
#include "../Weapons/WeaponDefinition.h"
#include "../Items/ItemDefinition.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "HAL/FileManager.h"
#include "BossReadabilityCapture.h"

struct FArenaTestAccess
{
    static void Ready(UBossActionComponent& A,ACountessBossCharacter* B,ATPCCharacter* P)
    {
        A.Boss=B; A.Health=B->FindComponentByClass<UHealthComponent>(); A.Target=P; A.bInitialized=true;
        A.HomeLocation=B->GetActorLocation(); A.State=EBossState::Combat; A.LastSeen=A.Now()-30.; A.OutsideSince=-1.;
    }
    static void ExpireBoundaryGrace(UBossActionComponent& A) { A.OutsideSince=A.Now()-2.1; }
    static void Wave(AArenaWaveDirector& D,int32 Seed) { D.Random.Initialize(Seed); D.PendingRanged.Reset(); D.StartWave(); }
    static int32 Ranged(const AArenaWaveDirector& D) { int32 N=0; for (bool B:D.PendingRanged) N+=B?1:0; return N; }
};

namespace ArenaTests
{
constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
struct FSaveScope
{
    FString Original=FCommandLine::Get();
    FString Slot=TEXT("ArenaAutomation_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    FSaveScope()
    {
        FCommandLine::Set(*(TEXT("-TPCSaveSlot=")+Slot+TEXT(" ")+Original));
        auto* Save=NewObject<UTPCSaveGame>(); Save->PlayerHealth=73; Save->PlayerStamina=81;
        Save->PlayerLevel=3; Save->PlayerExperience=7; Save->CheckpointMap=TEXT("Lvl_ThirdPerson");
        Save->PlayerTransform=FTransform(FVector(800,800,500));
        Save->bHasEquipmentState=true; Save->bDoubleJumpUnlocked=true;
        Save->EquippedWeapon=TSoftObjectPtr<UWeaponDefinition>(FSoftObjectPath(TEXT("/Game/Third/DataAsset/DA_TestSword.DA_TestSword")));
        FSaveDoorState Door; Door.SaveId=TEXT("CampaignDoor"); Door.bIsOpen=true; Save->DoorStates.Add(Door);
        UGameplayStatics::SaveGameToSlot(Save,Slot,0);
    }
    ~FSaveScope() { UGameplayStatics::DeleteGameInSlot(Slot,0); FCommandLine::Set(*Original); }
};
template<class T> T* First(UWorld* W) { for (TActorIterator<T> It(W); It; ++It) return *It; return nullptr; }
void Position(ATPCCharacter* P,const FVector& Point)
{
    P->SetActorLocationAndRotation(Point,FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
    P->GetCharacterMovement()->StopMovementImmediately(); P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}
class FTravelScenario : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TSharedPtr<FSaveScope> SaveScope;
    int32 Step=0;
    double Started=FPlatformTime::Seconds(), StepAt=Started;
    FTutorialTravelSnapshot Tutorial;
    TWeakObjectPtr<UWorld> DeathWorld;
    int32 PreviousWave=0,Cleared=0;
    int32 EarnedLevel=0,EarnedXP=0;
    float SavedBossHealth=0;
    bool bRetried=false;
    void Next() { ++Step; StepAt=FPlatformTime::Seconds(); }
    bool Leave(UWorld* W,ATPCCharacter* P,const TCHAR* Destination)
    {
        for (TActorIterator<AArenaPortal> It(W); It; ++It)
            if (It->GetDestinationMap()==Destination)
            {
                Position(P,It->GetActorLocation()+It->GetActorForwardVector()*180-FVector(0,0,40));
                It->Interact(P); return true;
            }
        Test->AddError(FString::Printf(TEXT("Expected portal missing at step %d for %s"),Step,Destination)); return false;
    }
public:
    FTravelScenario(FAutomationTestBase* T,TSharedPtr<FSaveScope> S):Test(T),SaveScope(S){}
    bool Update() override
    {
        if (FPlatformTime::Seconds()-Started>140) { Test->AddError(FString::Printf(TEXT("Arena travel timed out at step %d"),Step)); return true; }
        UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr; if (!W || !W->HasBegunPlay() || W->GetTimeSeconds()<.4f) return false;
        auto* P=Cast<ATPCCharacter>(UGameplayStatics::GetPlayerPawn(W,0)); if (!P) return false;
        // Formal enemies can level the player up and legitimately pause for a choice.
        if (P->LevelComponent->HasPendingUpgradeChoice())
        {
            P->LevelComponent->SelectUpgrade(0);
            if (auto* PC=Cast<ATPCPlayerController>(P->GetController())) PC->RestoreGameplayInput();
            return false;
        }
        const FString Map=UGameplayStatics::GetCurrentLevelName(W,true);
        auto* Travel=UArenaTravelSubsystem::Get(W);
        if (!Travel) { Test->AddError(TEXT("No travel subsystem")); return true; }
        if (Step==0)
        {
            auto* D=ATutorialDirector::Find(W); if (!D) return false;
            D->SkipLesson(); Tutorial=D->ExportTravelSnapshot(); Tutorial.Count=1;
            Test->TestTrue(TEXT("Save partial lesson fixture"),D->ImportTravelSnapshot(Tutorial));
            Test->TestEqual(TEXT("Tutorial did not load campaign health"),P->HealthComponent->GetCurrentHealth(),100.f);
            Test->AddExpectedError(TEXT("目标地图不存在"),EAutomationExpectedErrorFlags::Contains,1);
            Test->TestFalse(TEXT("Missing destination leaves the session in place"),Travel->Travel(P,TEXT("/Game/Third/Arenas/Maps/MissingArena"),TEXT("ArenaArrival")));
            if (!Leave(W,P,UArenaTravelSubsystem::BossMap())) return true;
            Test->TestFalse(TEXT("Duplicate travel rejected"),Travel->Travel(P,UArenaTravelSubsystem::BossMap(),TEXT("ArenaArrival")));
            Next(); return false;
        }
        if (Step==1)
        {
            if (Map!=TEXT("L_CountessBossTest")) return false;
            auto* B=First<ACountessBossCharacter>(W); auto* Region=First<AArenaBounds>(W); if (!B || !Region) return false;
            Test->TestFalse(TEXT("Boss arrival is outside combat boundary"),Region->ContainsActor(P));
            Test->TestEqual(TEXT("Boss is dormant in lobby"),B->BossActions->State,EBossState::Dormant);
            Test->TestEqual(TEXT("Portable health loaded"),P->HealthComponent->GetCurrentHealth(),73.f);
            Test->TestEqual(TEXT("Formal weapon, not training sword"),P->EquipmentComponent->GetEquippedWeaponDefinition()->WeaponId,
                LoadObject<UWeaponDefinition>(nullptr,TEXT("/Game/Third/DataAsset/DA_TestSword.DA_TestSword"))->WeaponId);
            Test->TestTrue(TEXT("Formal ability restored"),P->bDoubleJumpUnlocked);
            Test->TestTrue(TEXT("Old checkpoint not applied in arena"),FVector::Dist(P->GetActorLocation(),FVector(800,800,500))>500);
            SavedBossHealth=B->FindComponentByClass<UHealthComponent>()->GetMaxHealth();
            P->HealthComponent->SetEncounterInvulnerable(true);
            Position(P,Region->GetActorLocation()+FVector(1700,1700,-350));
            B->BossActions->UpdateContext(.2f);
            Test->TestTrue(TEXT("Far corner starts encounter"),B->BossActions->IsEncounterActive());
            Next(); return false;
        }
        if (Step==2)
        {
            if (FPlatformTime::Seconds()-StepAt<6.) return false;
            auto* B=First<ACountessBossCharacter>(W);
            Test->TestTrue(TEXT("Far-corner encounter survives old leash/LOS timeout"),B && B->BossActions->IsEncounterActive());
            if (!Leave(W,P,UArenaTravelSubsystem::TutorialMap())) return true;
            Next(); return false;
        }
        if (Step==3 || Step==7)
        {
            if (Map!=TEXT("L_ForestTutorial")) return false;
            auto* D=ATutorialDirector::Find(W); if (!D) return false;
            Test->TestEqual(TEXT("Tutorial lesson retained"),D->GetProgress().Lesson,Tutorial.Lesson);
            Test->TestEqual(TEXT("Partial objective count retained"),D->GetProgress().Count,Tutorial.Count);
            Test->TestEqual(TEXT("Training weapon restored"),P->EquipmentComponent->GetEquippedWeaponDefinition(),D->PlayerWeapon.Get());
            Test->AddInfo(FString::Printf(TEXT("Tutorial kit actual=%s expected=%s"),*GetPathNameSafe(P->EquipmentComponent->GetEquippedWeaponDefinition()),*GetPathNameSafe(D->PlayerWeapon.Get())));
            auto* Disk=Cast<UTPCSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveScope->Slot,0));
            Test->TestNotNull(TEXT("Formal save exists"),Disk); if (!Disk) return true;
            Test->TestTrue(TEXT("Campaign checkpoint preserved"),Disk->PlayerTransform.GetLocation().Equals(FVector(800,800,500)));
            Test->TestEqual(TEXT("Campaign doors preserved"),Disk->DoorStates.Num(),1);
            if (Step==7)
            {
                Test->TestEqual(TEXT("Earned level survives tutorial"),Disk->PlayerLevel,EarnedLevel);
                Test->TestEqual(TEXT("Earned XP survives tutorial"),Disk->PlayerExperience,EarnedXP);
                if (!Leave(W,P,UArenaTravelSubsystem::BossMap())) return true;
            }
            else if (!Leave(W,P,UArenaTravelSubsystem::WaveMap())) return true;
            Next(); return false;
        }
        if (Step==4)
        {
            if (Map!=TEXT("L_RandomArena")) return false;
            auto* D=First<AArenaWaveDirector>(W); if (!D) return false;
            Test->TestEqual(TEXT("No enemies in arrival lobby"),D->GetWave(),0);
            P->HealthComponent->SetEncounterInvulnerable(true);
            Position(P,FVector(-1300,0,110)); Next(); return false;
        }
        if (Step==5)
        {
            auto* D=First<AArenaWaveDirector>(W); if (!D || D->GetWave()<=PreviousWave) return false;
            TArray<AEnemyCharacter*> Alive; int32 Ranged=0;
            for (TActorIterator<AEnemyCharacter> It(W); It; ++It) if (It->FindComponentByClass<UHealthComponent>()->GetCurrentHealth()>0)
            { Alive.Add(*It); if (It->IsA(D->RangedClass)) ++Ranged; }
            if (Alive.Num()!=D->GetRemaining() || Alive.Num()<3) return false;
            Test->TestTrue(TEXT("Wave has 3-4 enemies"),Alive.Num()<=4);
            Test->TestTrue(TEXT("Wave contains melee and 1-2 ranged"),Ranged>=1 && Ranged<=2 && Alive.Num()-Ranged>=1);
            for (auto* E : Alive) E->FindComponentByClass<UHealthComponent>()->ApplyDamageFrom(100000.f,P);
            PreviousWave=D->GetWave();
            if (++Cleared<3) return false;
            EarnedLevel=P->LevelComponent->GetLevel(); EarnedXP=P->LevelComponent->GetCurrentExperience();
            Test->TestTrue(TEXT("Formal combat awarded XP"),EarnedLevel>3 || EarnedXP>7);
            Test->TestFalse(TEXT("Wave kills never win the campaign"),W->GetAuthGameMode<ATPCGameMode>()->IsGameWon());
            P->InventoryComponent->AddItem(P->InventoryComponent->HealthPotionDefinition,4);
            P->HealthComponent->SetEncounterInvulnerable(false);
            P->HealthComponent->ApplyDamage(100000.f); DeathWorld=W;
            Next(); return false;
        }
        if (Step==6)
        {
            if (DeathWorld.Get()==W)
            {
                if (auto* PC=Cast<ATPCPlayerController>(P->GetController())) PC->RestartAfterDeath();
                bRetried=true; return false;
            }
            if (!bRetried || Map!=TEXT("L_RandomArena")) return false;
            auto* D=First<AArenaWaveDirector>(W); if (!D) return false;
            Test->TestEqual(TEXT("Death retry restarts waves"),D->GetWave(),0);
            Test->TestEqual(TEXT("Death retry restores full health"),P->HealthComponent->GetCurrentHealth(),P->HealthComponent->GetMaxHealth());
            Test->TestEqual(TEXT("Death retains XP"),P->LevelComponent->GetCurrentExperience(),EarnedXP);
            Test->TestTrue(TEXT("Death retains collected inventory"),P->InventoryComponent->GetHealthPotionCount()>=4);
            if (!Leave(W,P,UArenaTravelSubsystem::TutorialMap())) return true;
            Next(); return false;
        }
        if (Step==8)
        {
            if (Map!=TEXT("L_CountessBossTest")) return false;
            auto* B=First<ACountessBossCharacter>(W); if (!B) return false;
            Test->TestEqual(TEXT("Reentered Boss has full health"),B->FindComponentByClass<UHealthComponent>()->GetCurrentHealth(),SavedBossHealth);
            Test->TestEqual(TEXT("Reentered Boss waits for player"),B->BossActions->State,EBossState::Dormant);
            return true;
        }
        return false;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaAssetTest,"ThirdPerson.Arena.SavedMapsAndNavigation",ArenaTests::Flags)
bool FArenaAssetTest::RunTest(const FString&)
{ return TestTrue(TEXT("Saved portals, maps, arrival tags and nav are valid"),UArenaAssetTools::ValidateArenaExpansion()); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaRulesTest,"ThirdPerson.Arena.BoundaryAndWaveRules",ArenaTests::Flags)
bool FArenaRulesTest::RunTest(const FString&)
{
    UWorld* W=UWorld::CreateWorld(EWorldType::Game,false); GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    for (int32 I=0;I<180;++I) W->Tick(LEVELTICK_TimeOnly,1.f/60.f); // Expired times must remain >=0; -1 is the unset sentinel.
    auto* Region=W->SpawnActor<AArenaBounds>(); Region->Bounds->SetBoxExtent(FVector(2470,2470,1200));
    auto* B=W->SpawnActor<ACountessBossCharacter>(); auto* P=W->SpawnActor<ATPCCharacter>();
    auto* A=B->BossActions.Get(); A->ArenaBoundary=Region;
    for (float X:{-2300.f,2300.f}) for (float Y:{-2300.f,2300.f})
    {
        P->SetActorLocation(FVector(X,Y,600)); FArenaTestAccess::Ready(*A,B,P); A->UpdateContext(.2f);
        TestEqual(TEXT("Whole arena including jumping corners retains encounter after stale LOS"),A->State,EBossState::Combat);
    }
    P->SetActorLocation(FVector(2800,0,100)); A->UpdateContext(.2f);
    TestEqual(TEXT("Brief boundary crossing does not reset"),A->State,EBossState::Combat);
    P->SetActorLocation(FVector(0,0,100)); A->UpdateContext(.2f);
    TestEqual(TEXT("Reentering boundary clears grace"),A->State,EBossState::Combat);
    P->SetActorLocation(FVector(2800,0,100)); FArenaTestAccess::ExpireBoundaryGrace(*A); A->UpdateContext(.2f);
    TestEqual(TEXT("Leaving boundary beyond grace resets"),A->State,EBossState::Resetting);
    FArenaTestAccess::Ready(*A,B,P); A->ArenaBoundary=nullptr; FArenaTestAccess::ExpireBoundaryGrace(*A); A->UpdateContext(.2f);
    TestEqual(TEXT("Unbound boss retains legacy leash"),A->State,EBossState::Resetting);
    auto* D=W->SpawnActor<AArenaWaveDirector>();
    for (int32 Seed=1;Seed<=100;++Seed)
    {
        FArenaTestAccess::Wave(*D,Seed); const int32 N=D->GetRemaining(),R=FArenaTestAccess::Ranged(*D);
        TestTrue(TEXT("Seeded waves satisfy count/type cap"),N>=3 && N<=4 && R>=1 && R<=2 && N-R>=1);
    }
    A->CancelAction(); W->DestroyWorld(false); GEngine->DestroyWorldContext(W); if (W->IsRooted()) W->RemoveFromRoot();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaTravelTest,"ThirdPerson.Arena.PIE.RoundTripWavesAndDeath",ArenaTests::Flags)
bool FArenaTravelTest::RunTest(const FString&)
{
    auto Scope=MakeShared<ArenaTests::FSaveScope>();
    FAutomationEditorCommonUtils::LoadMap(UArenaTravelSubsystem::TutorialMap());
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(ArenaTests::FTravelScenario(this,Scope));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

class FArenaVisualScenario : public IAutomationLatentCommand
{
    FAutomationTestBase* Test; FString Scene; TSharedPtr<ArenaTests::FSaveScope> Save;
    double Started=FPlatformTime::Seconds(),CameraAt=0;
    TWeakObjectPtr<ACameraActor> Camera;
public:
    FArenaVisualScenario(FAutomationTestBase* T,FString Name,TSharedPtr<ArenaTests::FSaveScope> S):Test(T),Scene(Name),Save(S){}
    bool Update() override
    {
        if (FPlatformTime::Seconds()-Started>50) { Test->AddError(TEXT("Arena visual capture timeout")); return true; }
        UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr; if (!W || !W->HasBegunPlay() || W->GetTimeSeconds()<1.f) return false;
        auto* P=Cast<ATPCCharacter>(UGameplayStatics::GetPlayerPawn(W,0)); if (!P || !P->GetController()) return false;
        if (!Camera.IsValid())
        {
            FVector Look(0,0,0),Eye(-4000,-3800,3300);
            if (Scene==TEXT("Tutorial"))
            { Look=P->GetActorLocation()+P->GetActorForwardVector()*150; Eye=Look+P->GetActorForwardVector()*200+FVector(0,0,1850); }
            Camera=W->SpawnActor<ACameraActor>(Eye,(Look-Eye).Rotation()); Camera->GetCameraComponent()->SetFieldOfView(78);
            Cast<APlayerController>(P->GetController())->SetViewTarget(Camera.Get()); CameraAt=FPlatformTime::Seconds();
            return false;
        }
        if (FPlatformTime::Seconds()-CameraAt<4.) return false;
        const FString Dir=FPaths::ProjectSavedDir()/TEXT("ArenaExpansion/Captures"); IFileManager::Get().MakeDirectory(*Dir,true);
        Test->TestTrue(TEXT("Rendered arena screenshot"),CaptureBossReadabilityFrame(W,Dir/(Scene+TEXT(".png"))));
        return true;
    }
};
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FArenaCaptureTest,"ThirdPerson.Visuals.Arena",ArenaTests::Flags)
void FArenaCaptureTest::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{ for (const TCHAR* Name:{TEXT("Tutorial"),TEXT("Boss"),TEXT("Random")}) { Names.Add(Name); Commands.Add(Name); } }
bool FArenaCaptureTest::RunTest(const FString& Scene)
{
    auto Scope=MakeShared<ArenaTests::FSaveScope>();
    FAutomationEditorCommonUtils::LoadMap(Scene==TEXT("Tutorial")?UArenaTravelSubsystem::TutorialMap():Scene==TEXT("Boss")?UArenaTravelSubsystem::BossMap():UArenaTravelSubsystem::WaveMap());
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FArenaVisualScenario(this,Scene,Scope));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
