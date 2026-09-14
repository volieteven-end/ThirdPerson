// 敌人表现回归：检查近战／远程血条、死亡动画和相机碰撞响应，包括运行时生成的敌人。
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/GameViewportClient.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/ProgressBar.h"
#include "Blueprint/WidgetTree.h"
#include "BrainComponent.h"
#include "AIController.h"
#include "Camera/CameraActor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SViewport.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "ImageUtils.h"
#include "../AI/EnemyCharacter.h"
#include "../AI/EnemySpawner.h"
#include "../Arena/ArenaActors.h"
#include "../Boss/CountessBossCharacter.h"
#include "../Character/TPCCharacter.h"
#include "../Character/TPCPlayerController.h"
#include "../Components/CombatComponent.h"
#include "../Components/HealthComponent.h"
#include "../Components/EquipmentComponent.h"
#include "../Components/LevelComponent.h"
#include "../UI/EnemyHealthWidget.h"
#include "../Weapons/WeaponActor.h"
#include "../Weapons/WeaponDefinition.h"

namespace EnemyPresentationTests
{
constexpr auto Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
const TCHAR* Classes[] = {
    TEXT("/Game/Third/Character/BP_EnemyCharacter.BP_EnemyCharacter_C"),
    TEXT("/Game/Third/Character/BP_EnemyRangedCharacter.BP_EnemyRangedCharacter_C")
};
UWidgetComponent* HealthBar(AEnemyCharacter* Enemy)
{
    for (auto* Widget : TInlineComponentArray<UWidgetComponent*>(Enemy))
        if (Widget->GetFName() == TEXT("HealthBarWidget")) return Widget;
    return nullptr;
}
AEnemyCharacter* SpawnedEnemy(AEnemySpawner* Spawner)
{
    return Cast<AEnemyCharacter>(FindFProperty<FObjectProperty>(AEnemySpawner::StaticClass(), TEXT("CurrentEnemy"))->GetObjectPropertyValue_InContainer(Spawner));
}
void StopAI(AEnemyCharacter* Enemy)
{
    if (auto* AI = Cast<AAIController>(Enemy->GetController()))
    { AI->StopMovement(); if (AI->GetBrainComponent()) AI->GetBrainComponent()->StopLogic(TEXT("Presentation test fixture")); }
    Enemy->GetCharacterMovement()->StopMovementImmediately();
    Enemy->GetCharacterMovement()->DisableMovement();
    Enemy->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}
struct FScope
{
    FString Original = FCommandLine::Get(), Slot = TEXT("EnemyPresentation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    bool bFixed = FApp::UseFixedTimeStep(); double Delta = FApp::GetFixedDeltaTime();
    FScope() { FCommandLine::Set(*(TEXT("-TPCSaveSlot=") + Slot + TEXT(" ") + Original)); }
    ~FScope()
    { UGameplayStatics::DeleteGameInSlot(Slot, 0); FCommandLine::Set(*Original); FApp::SetUseFixedTimeStep(bFixed); FApp::SetFixedDeltaTime(Delta); }
};

class FScenario : public IAutomationLatentCommand
{
    FAutomationTestBase* Test; TSharedPtr<FScope> Scope; int32 FPS, Stage = 0;
    double WallStart = FPlatformTime::Seconds(), At = 0, DeathAt = 0;
    TWeakObjectPtr<AEnemySpawner> Spawners[2]; TWeakObjectPtr<AEnemyCharacter> Enemies[2];
    float Durations[2] = {}; FVector StandingHead[2], FallenHead[2]; int32 FirstKill = 0; bool bSecondKilled = false;

    void CheckCamera(AEnemyCharacter* Enemy)
    {
        TArray<AActor*> Actors; Enemy->GetAttachedActors(Actors, true, true); Actors.Add(Enemy);
        for (AActor* Actor : Actors)
        {
            TInlineComponentArray<UPrimitiveComponent*> Components; Actor->GetComponents(Components, true);
            for (auto* Component : Components)
                Test->TestEqual(*FString::Printf(TEXT("%s.%s ignores Camera"), *Actor->GetName(), *Component->GetName()),
                    Component->GetCollisionResponseToChannel(ECC_Camera), ECR_Ignore);
        }
        FHitResult Hit;
        const FVector Center = Enemy->GetActorLocation();
        Test->TestFalse(TEXT("Spring-arm-sized Camera sweep passes through enemy"), Enemy->GetWorld()->SweepSingleByChannel(
            Hit, Center-FVector(100,0,0), Center+FVector(100,0,0), FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(12)));
    }
    void CheckHealth(AEnemyCharacter* Enemy, float Expected)
    {
        auto* Bar = HealthBar(Enemy);
        if (!Test->TestNotNull(TEXT("Enemy health component widget exists"), Bar)) return;
        auto* Widget = Cast<UEnemyHealthWidget>(Bar->GetUserWidgetObject());
        if (!Test->TestNotNull(TEXT("Actual enemy UMG was constructed and bound"), Widget)) return;
        Test->TestTrue(TEXT("Living enemy health bar is visible"), Bar->IsVisible() && !Bar->bHiddenInGame);
        int32 Count = 0;
        Widget->WidgetTree->ForEachWidget([&](UWidget* Child)
        {
            if (auto* Progress = Cast<UProgressBar>(Child))
            { ++Count; Test->TestTrue(TEXT("UMG progress matches real health"), FMath::IsNearlyEqual(Progress->GetPercent(), Expected, .001f)); }
        });
        Test->TestTrue(TEXT("Health widget has a real progress bar"), Count > 0);
    }
    void Capture(UWorld* World, const TCHAR* Name)
    {
        if (FPS != 60) return;
        auto View = World->GetGameViewport()->GetGameViewportWidget();
        TArray<FColor> Pixels; FIntVector Size = FIntVector::ZeroValue;
        if (!View || !FSlateApplication::Get().TakeScreenshot(View.ToSharedRef(), Pixels, Size))
        { Test->AddError(TEXT("Enemy presentation screenshot unavailable")); return; }
        const FString Folder = FPaths::ProjectSavedDir()/TEXT("EnemyPresentation/Captures");
        IFileManager::Get().MakeDirectory(*Folder, true);
        TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, PNG);
        Test->TestTrue(TEXT("Saved runtime presentation screenshot"), FFileHelper::SaveArrayToFile(PNG, *(Folder/(FString(Name)+TEXT(".png")))));
    }
    void Kill(AEnemyCharacter* Enemy)
    {
        Enemy->FindComponentByClass<UHealthComponent>()->ApplyDamage(100000);
        Test->TestEqual(TEXT("Death state committed"), Enemy->GetLaunchPhase(), EEnemyLaunchPhase::Dead);
        auto* Instance = Enemy->GetMesh()->GetAnimInstance()->GetActiveInstanceForMontage(Enemy->DeathMontage);
        if (Test->TestNotNull(TEXT("Configured death animation starts"), Instance))
            Test->TestFalse(TEXT("Death instance cannot blend back to idle"), Instance->bEnableAutoBlendOut);
        Test->TestFalse(TEXT("Dead enemy cannot attack"), Enemy->FindComponentByClass<UCombatComponent>()->IsCombatEnabled());
        Test->TestFalse(TEXT("Death hides the overhead health bar"), HealthBar(Enemy)->IsVisible());
        const float LifeBefore = Enemy->GetLifeSpan();
        Enemy->FindComponentByClass<UHealthComponent>()->OnDeath.Broadcast();
        Test->TestTrue(TEXT("Repeated death event does not restart animation/lifetime"), FMath::IsNearlyEqual(LifeBefore, Enemy->GetLifeSpan()));
        CheckCamera(Enemy);
    }
public:
    FScenario(FAutomationTestBase* InTest, TSharedPtr<FScope> InScope, int32 InFPS) : Test(InTest), Scope(InScope), FPS(InFPS) {}
    bool Update() override
    {
        if (FPlatformTime::Seconds()-WallStart > 100)
        { Test->AddError(FString::Printf(TEXT("Enemy presentation timed out at stage %d"), Stage)); return true; }
        UWorld* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
        if (!World || !World->HasBegunPlay() || World->GetTimeSeconds() < 1) return false;
        auto* Player = Cast<ATPCCharacter>(UGameplayStatics::GetPlayerPawn(World,0)); if (!Player) return false;
        if (Player->LevelComponent->HasPendingUpgradeChoice())
        { Player->LevelComponent->SelectUpgrade(0); Cast<ATPCPlayerController>(Player->GetController())->RestoreGameplayInput(); return false; }
        const double Now = World->GetTimeSeconds();
        if (Stage == 0)
        {
            FApp::SetUseFixedTimeStep(true); FApp::SetFixedDeltaTime(1./FPS);
            Player->HealthComponent->SetEncounterInvulnerable(true);
            Player->SetActorLocation(FVector(-700,0,100));
            Stage = 1; At = Now; return false;
        }
        if (Stage == 1)
        {
            int32 WaveEnemies = 0;
            for (TActorIterator<AEnemyCharacter> It(World); It; ++It) ++WaveEnemies;
            if (WaveEnemies < 3 || Now-At < 5) return false;
            for (TActorIterator<AArenaWaveDirector> It(World); It; ++It) It->SetActorTickEnabled(false);
            for (TActorIterator<AEnemyCharacter> It(World); It; ++It)
            { CheckCamera(*It); CheckHealth(*It,1); StopAI(*It); It->SetActorLocation(FVector(1400,1200+WaveEnemies--*250,100)); }
            for (int32 Index = 0; Index < 2; ++Index)
            {
                auto* Class = LoadClass<AEnemyCharacter>(nullptr, Classes[Index]);
                const FTransform Transform(FRotator::ZeroRotator, FVector(0,Index ? 160 : -160,100));
                auto* Spawner = World->SpawnActorDeferred<AEnemySpawner>(AEnemySpawner::StaticClass(), Transform);
                FindFProperty<FClassProperty>(AEnemySpawner::StaticClass(), TEXT("EnemyClass"))->SetObjectPropertyValue_InContainer(Spawner,Class);
                FindFProperty<FFloatProperty>(AEnemySpawner::StaticClass(), TEXT("RespawnDelay"))->SetPropertyValue_InContainer(Spawner,.4f);
                Spawner->FinishSpawning(Transform); Spawners[Index] = Spawner;
                auto* Enemy = SpawnedEnemy(Spawner);
                if (!Test->TestNotNull(TEXT("Real level spawner produced enemy"), Enemy)) return true;
                Enemies[Index] = Enemy; StopAI(Enemy); CheckCamera(Enemy); CheckHealth(Enemy,1);
                Test->TestEqual(TEXT("Living capsule still blocks Pawn"), Enemy->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Block);
                auto* Equipment = Enemy->FindComponentByClass<UEquipmentComponent>();
                auto* Definition = Equipment->GetEquippedWeaponDefinition();
                if (Definition) { Equipment->UnequipWeapon(); Equipment->EquipWeapon(Definition); CheckCamera(Enemy); }
                if (!Test->TestNotNull(TEXT("Both normal enemy types have death montages"), Enemy->DeathMontage.Get()) ||
                    !Test->TestNotNull(TEXT("Mesh has an actual animation instance"), Enemy->GetMesh()->GetAnimInstance())) return true;
                Durations[Index] = Enemy->DeathMontage->GetPlayLength()/Enemy->DeathMontage->RateScale;
            }
            // Spawn with deliberately stale overrides, as old placed actors can have.
            auto* BossClass = LoadClass<ACountessBossCharacter>(nullptr,TEXT("/Game/Third/Bosses/Countess/BP_CountessBoss.BP_CountessBoss_C"));
            const FTransform BossTransform(FRotator::ZeroRotator,FVector(800,0,110));
            auto* Boss = World->SpawnActorDeferred<ACountessBossCharacter>(BossClass,BossTransform);
            Boss->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera,ECR_Block);
            Boss->GetMesh()->SetCollisionResponseToChannel(ECC_Camera,ECR_Block);
            Boss->FinishSpawning(BossTransform); CheckCamera(Boss);
            Test->TestNull(TEXT("Boss keeps its own death graph, not a small-enemy montage"),Boss->DeathMontage.Get()); Boss->Destroy();
            FHitResult Ground;
            Test->TestTrue(TEXT("Environment still blocks Camera"),World->LineTraceSingleByChannel(Ground,FVector(0,0,300),FVector(0,0,-100),ECC_Camera));
            auto* Camera = World->SpawnActor<ACameraActor>(FVector(650,-30,310),FRotator(-19,180,0));
            Player->GetController<APlayerController>()->SetViewTarget(Camera);
            for (int32 Index = 0; Index < 2; ++Index)
            {
                auto* Health = Enemies[Index]->FindComponentByClass<UHealthComponent>();
                Health->ApplyDamage(Health->GetMaxHealth()*.5f); CheckHealth(Enemies[Index].Get(),.5f);
            }
            Stage = 2; At = Now; return false;
        }
        if (Stage == 2)
        {
            if (Now-At < .8f) return false;
            Capture(World,TEXT("alive_half_health"));
            for (int32 Index = 0; Index < 2; ++Index) StandingHead[Index] = Enemies[Index]->GetMesh()->GetSocketLocation(TEXT("head"));
            FirstKill = Durations[0] >= Durations[1] ? 0 : 1;
            Kill(Enemies[FirstKill].Get()); DeathAt = Now; Stage = 3; return false;
        }
        if (Stage == 3)
        {
            if (!bSecondKilled && Now-DeathAt >= FMath::Abs(Durations[1]-Durations[0]))
            { Kill(Enemies[1-FirstKill].Get()); bSecondKilled = true; }
            if (Now-DeathAt < Durations[FirstKill]+.13f) return false;
            for (int32 Index = 0; Index < 2; ++Index)
            {
                auto* Enemy = Enemies[Index].Get();
                if (!Test->TestNotNull(TEXT("Corpse survives full death animation"),Enemy)) return true;
                FallenHead[Index] = Enemy->GetMesh()->GetSocketLocation(TEXT("head"));
                const FString PoseEvidence = FString::Printf(TEXT("%s physically falls in the rendered pose (head %.1f -> %.1f)"),*Enemy->GetName(),StandingHead[Index].Z,FallenHead[Index].Z);
                Test->AddInfo(PoseEvidence);
                Test->TestTrue(*PoseEvidence,FallenHead[Index].Z < StandingHead[Index].Z-100);
                auto* Instance = Enemy->GetMesh()->GetAnimInstance()->GetActiveInstanceForMontage(Enemy->DeathMontage);
                if (Test->TestNotNull(TEXT("Death instance still holds its pose"),Instance))
                    Test->TestTrue(TEXT("Death reached the final frame"),Instance->GetPosition() > Enemy->DeathMontage->GetPlayLength()-.05f);
            }
            Capture(World,TEXT("death_final_pose")); Stage = 4; At = Now; return false;
        }
        if (Stage == 4)
        {
            if (Now-At < .1f) return false;
            for (int32 Index = 0; Index < 2; ++Index)
                if (Test->TestNotNull(TEXT("Final corpse pose remains until cleanup"),Enemies[Index].Get()))
                    Test->TestTrue(TEXT("Corpse does not stand back up"),FVector::Dist(FallenHead[Index],Enemies[Index]->GetMesh()->GetSocketLocation(TEXT("head"))) < 1.f);
            Stage = 5; return false;
        }
        if (Stage == 5)
        {
            if (Now-DeathAt < Durations[FirstKill]+1.6f) return false;
            for (int32 Index = 0; Index < 2; ++Index)
            {
                Test->TestFalse(TEXT("Original corpse was cleaned up"),Enemies[Index].IsValid());
                auto* Respawned = SpawnedEnemy(Spawners[Index].Get());
                if (Test->TestNotNull(TEXT("Level spawner respawned a new living enemy"),Respawned))
                { StopAI(Respawned); CheckCamera(Respawned); CheckHealth(Respawned,1); }
            }
            Test->AddInfo(FString::Printf(TEXT("%d FPS: wave/spawner/Boss Camera, HP updates, death pose and respawn verified"),FPS));
            return true;
        }
        return false;
    }
};
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FEnemyPresentationPIETest,"ThirdPerson.EnemyPresentation.PIE",EnemyPresentationTests::Flags)
void FEnemyPresentationPIETest::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{ for (int32 FPS : {30,60,120}) { Names.Add(FString::Printf(TEXT("%dFPS"),FPS)); Commands.Add(FString::FromInt(FPS)); } }
bool FEnemyPresentationPIETest::RunTest(const FString& Parameters)
{
    auto Scope = MakeShared<EnemyPresentationTests::FScope>();
    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Third/Arenas/Maps/L_RandomArena"));
    FRequestPlaySessionParams Params;
    auto* Settings = DuplicateObject<ULevelEditorPlaySettings>(GetMutableDefault<ULevelEditorPlaySettings>(),GetTransientPackage());
    Settings->NewWindowWidth = 1280; Settings->NewWindowHeight = 720;
    Params.EditorPlaySettings = Settings; Params.bAllowOnlineSubsystem = false;
    auto Window = SNew(SWindow).Title(FText::FromString(TEXT("Enemy presentation validation"))).ClientSize(FVector2D(1280,720))
        .ScreenPosition(FVector2D::ZeroVector).AutoCenter(EAutoCenter::None).SaneWindowPlacement(false)
        .AdjustInitialSizeAndPositionForDPIScale(false).CreateTitleBar(false).UseOSWindowBorder(false).SizingRule(ESizingRule::FixedSize);
    FSlateApplication::Get().AddWindow(Window); Params.CustomPIEWindow = Window; GEditor->RequestPlaySession(Params);
    ADD_LATENT_AUTOMATION_COMMAND(EnemyPresentationTests::FScenario(this,Scope,FCString::Atoi(*Parameters)));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
