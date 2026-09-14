// 命中反馈与 UI 回归：覆盖菜单存档策略、分页、属性和角色预览；视觉捕获用例与运行时用例分开。
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetSwitcher.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "WidgetBlueprint.h"
#include "Misc/CommandLine.h"
#include "HAL/PlatformProcess.h"
#include "Kismet/GameplayStatics.h"
#include "../Character/TPCCharacter.h"
#include "../Character/TPCPlayerController.h"
#include "../AI/EnemyCharacter.h"
#include "../Boss/CountessBossCharacter.h"
#include "../Boss/BossActionComponent.h"
#include "../Components/CombatComponent.h"
#include "../Components/EquipmentComponent.h"
#include "../Components/HealthComponent.h"
#include "../Components/HitFeedbackComponent.h"
#include "../Components/LevelComponent.h"
#include "../UI/InventroyWidget.h"
#include "../UI/InventoryPresentation.h"
#include "../UI/MainMenuWidget.h"
#include "../Save/TPCSaveGame.h"
#include "../Save/TPCSaveSlots.h"
#include "../World/TreasureChestActor.h"
#include "../Weapons/WeaponDefinition.h"
#include "../Items/PickupActor.h"
#include "BossReadabilityCapture.h"
#include "HAL/FileManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/SViewport.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Engine/GameViewportClient.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"

struct FCombatUIAccess
{
    static bool Pump(UHitFeedbackComponent& C) { return C.UpdateRealTime(0); }
    static double End(const UHitFeedbackComponent& C) { return C.ExpiresAt; }
    static void ReadyBoss(UBossActionComponent& A,ATPCCharacter* P) { A.Target=P; A.bHasLOS=true; A.State=EBossState::Combat; A.CurrentAction=EBossAction::None; A.bStandoff=false; A.bStandoffRolled=false; A.NextStandoffAt=0; A.NextMoveRequest=0; A.Random.Initialize(3); }
    static void EndStandoff(UBossActionComponent& A) { A.StandoffUntil=A.Now()-.1; }
    static void Chest(ATreasureChestActor& C) { C.RewardPickupClass=APickupActor::StaticClass(); C.RewardPickupCount=2; }
    static UStaticMesh* Mesh(const ATreasureChestActor& C) { return C.ChestMesh->GetStaticMesh(); }
};
namespace CombatUITest
{
constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
struct FSlotScope
{
    FString Original=FCommandLine::Get(), Slot=TEXT("CombatUI_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    FSlotScope() { FCommandLine::Set(*(TEXT("-TPCSaveSlot=")+Slot+TEXT(" ")+Original)); }
    ~FSlotScope() { UGameplayStatics::DeleteGameInSlot(Slot,0); FCommandLine::Set(*Original); }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuSavePolicyTest,"ThirdPerson.CombatUI.MenuSavePolicy",CombatUITest::Flags)
bool FMenuSavePolicyTest::RunTest(const FString&)
{
    CombatUITest::FSlotScope Scope; FString Map; FText Error;
    TestFalse(TEXT("No save disables continue"),UMainMenuWidget::ResolveContinueMap(nullptr,Map,Error));
    auto* S=NewObject<UTPCSaveGame>(); S->bHasCheckpoint=false;
    UGameplayStatics::SaveGameToSlot(S,Scope.Slot,0);
    TestTrue(TEXT("Arena-only progress can continue"),UMainMenuWidget::ResolveContinueMap(nullptr,Map,Error));
    TestEqual(TEXT("Arena-only return uses tutorial hub"),Map,FString(TEXT("/Game/Third/Tutorial/Maps/L_ForestTutorial")));
    S->bHasCheckpoint=true; S->CheckpointMap=TEXT(""); UGameplayStatics::SaveGameToSlot(S,Scope.Slot,0);
    TestTrue(TEXT("Legacy campaign checkpoint resolves"),UMainMenuWidget::ResolveContinueMap(nullptr,Map,Error));
    TestTrue(TEXT("Legacy map is campaign"),Map.EndsWith(TEXT("Lvl_ThirdPerson")));
    S->CheckpointMap=TEXT("/Game/Third/MissingCheckpoint_UnitTest"); UGameplayStatics::SaveGameToSlot(S,Scope.Slot,0);
    TestFalse(TEXT("Missing checkpoint is not silently discarded"),UMainMenuWidget::ResolveContinueMap(nullptr,Map,Error));
    TestTrue(TEXT("Original missing-map save retained"),UGameplayStatics::DoesSaveGameExist(Scope.Slot,0));
    S->CheckpointMap=TEXT("/Game/Third/DataAsset/DA_TestSword"); UGameplayStatics::SaveGameToSlot(S,Scope.Slot,0);
    TestFalse(TEXT("Existing non-map package rejected"),UMainMenuWidget::ResolveContinueMap(nullptr,Map,Error));
    const TArray<uint8> CorruptHeader={0,0,0,0};
    TestTrue(TEXT("Write invalid header to temporary slot only"),UGameplayStatics::SaveDataToSlot(CorruptHeader,Scope.Slot,0));
    TestFalse(TEXT("Corrupt formal save header rejected"),UMainMenuWidget::ResolveContinueMap(nullptr,Map,Error));
    TestTrue(TEXT("Rejected data is not silently erased"),UGameplayStatics::DoesSaveGameExist(Scope.Slot,0));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatUIAssetsTest,"ThirdPerson.CombatUI.SavedWidgets",CombatUITest::Flags)
bool FCombatUIAssetsTest::RunTest(const FString&)
{
    auto* Menu=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/Third/UI/MainMenu/WBP_MainMenu.WBP_MainMenu"));
    auto* Bag=LoadObject<UWidgetBlueprint>(nullptr,TEXT("/Game/Third/Widget/WBP_Inventory.WBP_Inventory"));
    if (!TestNotNull(TEXT("Independent UMG blueprint"),Menu) || !TestNotNull(TEXT("Original inventory"),Bag)) return false;
    TestTrue(TEXT("Menu has its own base"),Menu->ParentClass==UMainMenuWidget::StaticClass());
    for (const FName Name:{FName(TEXT("StartButton")),FName(TEXT("ContinueButton")),FName(TEXT("QuitButton")),FName(TEXT("ConfirmPanel"))}) TestNotNull(*Name.ToString(),Menu->WidgetTree->FindWidget(Name));
    TestNotNull(TEXT("Real WidgetSwitcher"),Cast<UWidgetSwitcher>(Bag->WidgetTree->FindWidget(TEXT("InventoryPages"))));
    for (const FName Name:{FName(TEXT("InventoryGrid")),FName(TEXT("AttributesText")),FName(TEXT("CharacterPortrait")),FName(TEXT("CloseButton"))}) TestNotNull(*Name.ToString(),Bag->WidgetTree->FindWidget(Name));
    return true;
}

namespace CombatUITest
{
class FScenario : public IAutomationLatentCommand
{
    FAutomationTestBase* Test; TSharedPtr<FSlotScope> Scope; int32 Stage=0;
    double Deadline=FPlatformTime::Seconds()+90,Started=0;
    TWeakObjectPtr<ATPCCharacter> Player;
    TWeakObjectPtr<UInventoryWidget> UI; bool bUI;
public:
    FScenario(FAutomationTestBase* T,TSharedPtr<FSlotScope> S,bool IncludeUI=true):Test(T),Scope(S),bUI(IncludeUI){}
    bool Update() override
    {
        if (FPlatformTime::Seconds()>Deadline) { Test->AddError(TEXT("Combat UI PIE timed out")); return true; }
        UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr; if (!W || W->GetTimeSeconds()<1) return false;
        auto* P=Cast<ATPCCharacter>(UGameplayStatics::GetPlayerPawn(W,0)); if (!P) return false;
        auto* PC=Cast<ATPCPlayerController>(P->GetController()); if (!PC) return false;
        if (Stage==0)
        {
            Player=P; P->HealthComponent->SetEncounterInvulnerable(true);
            auto* Sword=LoadObject<UWeaponDefinition>(nullptr,TEXT("/Game/Third/DataAsset/DA_TestSword.DA_TestSword"));
            P->EquipmentComponent->EquipWeapon(Sword); P->EquipmentComponent->SetWeaponDrawn(true);
            auto* E=W->SpawnActor<AEnemyCharacter>(FVector(20000,0,150),FRotator::ZeroRotator);
            auto* Feedback=P->HitFeedbackComponent.Get(); FCombatHitSpec Spec; Spec.ActionSerial=1000; Spec.WindowId=TEXT("Primary");
            FCombatHitResult Hit; Hit.ActualDamage=1; Hit.Outcome=ECombatHitOutcome::Hit;
            for (int32 FPS:{30,60,120})
            {
                ++Spec.ActionSerial; W->GetWorldSettings()->SetTimeDilation(.8f);
                const double Before=FPlatformTime::Seconds(); P->CombatComponent->OnMeleeHitResolved.Broadcast(E,Spec,Hit);
                Test->TestTrue(TEXT("Confirmed player sword hit starts feedback"),Feedback->IsFeedbackActive());
                Test->TestTrue(TEXT("Prior time scale is multiplied"),FMath::IsNearlyEqual(W->GetWorldSettings()->TimeDilation,.08f));
                const double End=FCombatUIAccess::End(*Feedback); P->CombatComponent->OnMeleeHitResolved.Broadcast(E,Spec,Hit);
                Test->TestEqual(TEXT("Repeated target does not extend"),FCombatUIAccess::End(*Feedback),End);
                while (Feedback->IsFeedbackActive() && FPlatformTime::Seconds()-Before<.2) { FPlatformProcess::Sleep(1.f/FPS); FCombatUIAccess::Pump(*Feedback); }
                const double Elapsed=FPlatformTime::Seconds()-Before;
                Test->TestTrue(*FString::Printf(TEXT("%d FPS real-time duration %.4f"),FPS,Elapsed),Elapsed>=.032 && Elapsed<.033334+1./FPS+.02);
                Test->TestTrue(TEXT("Previous nonunit dilation restored"),FMath::IsNearlyEqual(W->GetWorldSettings()->TimeDilation,.8f));
                Feedback->OnMeleeHit(E,Spec,Hit); Test->TestFalse(TEXT("Same swing cannot retrigger after expiry"),Feedback->IsFeedbackActive());
            }
            ++Spec.ActionSerial; Hit.bBlocked=true; Feedback->OnMeleeHit(E,Spec,Hit); Test->TestFalse(TEXT("Blocked hits ignored"),Feedback->IsFeedbackActive());
            Hit.bBlocked=false; Hit.bParried=true; Feedback->OnMeleeHit(E,Spec,Hit); Test->TestFalse(TEXT("Parried hits ignored"),Feedback->IsFeedbackActive());
            Hit.bParried=false; Hit.ActualDamage=0; Feedback->OnMeleeHit(E,Spec,Hit); Test->TestFalse(TEXT("Zero damage ignored"),Feedback->IsFeedbackActive());
            Hit.ActualDamage=1; Hit.bKilled=true; Feedback->OnMeleeHit(E,Spec,Hit); Test->TestTrue(TEXT("Kills receive feedback"),Feedback->IsFeedbackActive());
            W->GetWorldSettings()->SetTimeDilation(.55f); Feedback->ClearFeedback(); Test->TestTrue(TEXT("Foreign change not overwritten"),FMath::IsNearlyEqual(W->GetWorldSettings()->TimeDilation,.55f));
            W->GetWorldSettings()->SetTimeDilation(1); ++Spec.ActionSerial; Feedback->OnMeleeHit(E,Spec,Hit); PC->SetPause(true); FCombatUIAccess::Pump(*Feedback);
            Test->TestFalse(TEXT("Pause clears feedback"),Feedback->IsFeedbackActive()); PC->SetPause(false);
            auto Stats=UInventoryPresentation::ReadAttributes(P); Test->TestEqual(TEXT("Stats use actual combat damage"),Stats.EffectiveBaseDamage,P->CombatComponent->GetPresentationDamage());
            Test->TestEqual(TEXT("Stats show weapon base independently"),Stats.WeaponBaseDamage,Sword->Damage);
            P->EquipmentComponent->SetWeaponDrawn(false); Test->TestFalse(TEXT("Stats reflect sheathing"),UInventoryPresentation::ReadAttributes(P).bArmed); P->EquipmentComponent->SetWeaponDrawn(true);
            E->Destroy();
            auto* Chest=W->SpawnActor<ATreasureChestActor>(FVector(22000,0,200),FRotator::ZeroRotator); FCombatUIAccess::Chest(*Chest);
            Chest->ClosedMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")); Chest->OpenedMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
            Chest->RefreshChestAppearance(); Test->TestEqual(TEXT("Closed mesh applied"),FCombatUIAccess::Mesh(*Chest),Chest->ClosedMesh.Get());
            int32 BeforePickups=0; for (TActorIterator<APickupActor> It(W);It;++It) ++BeforePickups;
            Chest->Interact(P); Chest->Interact(P); Test->TestEqual(TEXT("Open mesh applied"),FCombatUIAccess::Mesh(*Chest),Chest->OpenedMesh.Get());
            int32 AfterPickups=0; for (TActorIterator<APickupActor> It(W);It;++It) ++AfterPickups;
            Test->TestEqual(TEXT("Repeated chest use awards only configured count"),AfterPickups-BeforePickups,2);
            Chest->OpenedMesh=nullptr; Chest->RefreshChestAppearance(); Test->TestEqual(TEXT("Missing open mesh remains visible"),FCombatUIAccess::Mesh(*Chest),Chest->ClosedMesh.Get()); Chest->Destroy();
            for (TActorIterator<ACountessBossCharacter> It(W);It;++It)
            {
                auto* A=It->BossActions.Get(); auto* Def=DuplicateObject<UBossDefinition>(const_cast<UBossDefinition*>(A->GetDefinition()),GetTransientPackage()); Def->StandoffChance=1; A->Definition=Def;
                It->SetActorLocation(FVector(0,0,100)); P->SetActorLocation(FVector(350,0,100)); A->CancelAction(); FCombatUIAccess::ReadyBoss(*A,P);
                A->DriveDecision(); Test->TestTrue(TEXT("Legal action can deliberately yield to standoff"),A->IsStandoffActive());
                FCombatUIAccess::EndStandoff(*A); A->DriveDecision(); Test->TestFalse(TEXT("Standoff expires without repeated roll"),A->IsStandoffActive()); Test->TestTrue(TEXT("Expired standoff resumes attack"),A->IsActionActive());
                A->RequestReset(); Test->TestFalse(TEXT("Reset clears tactic"),A->IsStandoffActive()); break;
            }
            P->SetActorLocation(FVector(-2925,0,112));
            if (!bUI) return true;
            PC->ToggleInventory(); TArray<UUserWidget*> Widgets; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Widgets,UInventoryWidget::StaticClass(),true);
            if (Widgets.IsEmpty()) { Test->AddError(TEXT("Missing inventory UI")); return true; } UI=Cast<UInventoryWidget>(Widgets[0]);
            Started=W->GetTimeSeconds(); Stage=1; return false;
        }
        if (Stage==1 && W->GetTimeSeconds()-Started>1)
        {
            int32 Captures=0; for (TActorIterator<AInventoryPreviewActor> It(W);It;++It) Captures+=It->GetCaptureCount();
            Test->TestTrue(TEXT("Visible portrait captures"),Captures>0);
            Cast<UButton>(UI->GetWidgetFromName(TEXT("AttributesPageButton")))->OnClicked.Broadcast(); Started=W->GetTimeSeconds(); Stage=2; return false;
        }
        if (Stage==2 && W->GetTimeSeconds()-Started>.3)
        {
            auto* Pages=Cast<UWidgetSwitcher>(UI->GetWidgetFromName(TEXT("InventoryPages"))); Test->TestTrue(TEXT("Attribute button switches actual page"),Pages && Pages->GetActiveWidgetIndex()==1);
            P->LevelComponent->AddExperience(P->LevelComponent->GetExperienceToNextLevel());
            UI->ChooseLevelUpgrade(0); Test->TestTrue(TEXT("Upgrade returns cursor to open inventory"),PC->bShowMouseCursor); Test->TestFalse(TEXT("Inventory upgrade does not leave game paused"),UGameplayStatics::IsGamePaused(W));
            FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Tab,FModifierKeysState(),0,false,0,0)); Started=W->GetTimeSeconds(); Stage=3; return false;
        }
        if (Stage==3 && W->GetTimeSeconds()-Started>.3)
        {
            int32 Count=0; for (TActorIterator<AInventoryPreviewActor> It(W);It;++It) if (IsValid(*It)) ++Count;
            Test->TestEqual(TEXT("Closing releases portrait and all captures"),Count,0);
            PC->ToggleInventory(); Started=W->GetTimeSeconds(); Stage=4; return false;
        }
        if (Stage==4 && W->GetTimeSeconds()-Started>.3)
        {
            Cast<UButton>(UI->GetWidgetFromName(TEXT("CloseButton")))->OnClicked.Broadcast();
            Test->TestFalse(TEXT("Original close button dismisses either page"),UI->GetWidgetFromName(TEXT("InventoryPanel"))->IsVisible());
            PC->ToggleInventory(); PC->ShowDeathScreen();
            int32 Count=0; for (TActorIterator<AInventoryPreviewActor> It(W);It;++It) if (IsValid(*It)) ++Count;
            Test->TestEqual(TEXT("Death screen releases portrait"),Count,0); PC->SetPause(false); return true;
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatUIPIE,"ThirdPerson.CombatUI.PIE.HitFeedbackAndInventory",CombatUITest::Flags)
bool FCombatUIPIE::RunTest(const FString&)
{
    auto Scope=MakeShared<CombatUITest::FSlotScope>(); FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Third/Bosses/Countess/Maps/L_CountessBossTest"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); ADD_LATENT_AUTOMATION_COMMAND(CombatUITest::FScenario(this,Scope)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatUIRuntime,"ThirdPerson.CombatUI.PIE.RuntimeOnly",CombatUITest::Flags)
bool FCombatUIRuntime::RunTest(const FString&)
{
    auto Scope=MakeShared<CombatUITest::FSlotScope>(); FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Third/Bosses/Countess/Maps/L_CountessBossTest"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); ADD_LATENT_AUTOMATION_COMMAND(CombatUITest::FScenario(this,Scope,false)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
namespace CombatUITest
{
class FCapture : public IAutomationLatentCommand
{
    FAutomationTestBase* Test; TSharedPtr<FSlotScope> Scope; FString Page; FIntPoint Resolution;
    double Deadline=FPlatformTime::Seconds()+120,ReadyAt=0; bool bOpened=false,bTravelSent=false;
public:
    FCapture(FAutomationTestBase* T,TSharedPtr<FSlotScope> S,FString P,FIntPoint R):Test(T),Scope(S),Page(P),Resolution(R){}
    bool Update() override
    {
        if (FPlatformTime::Seconds()>Deadline) { Test->AddError(TEXT("UI screenshot timed out")); return true; }
        auto* W=GEditor?GEditor->PlayWorld.Get():nullptr; if (!W || !W->GetGameViewport()) return false;
        if (bTravelSent)
        {
            if (!W->GetMapName().Contains(TEXT("L_ForestTutorial")) || !UGameplayStatics::GetPlayerPawn(W,0)) return false;
            Test->TestEqual(TEXT("Only confirmed new game clears temporary save"),UGameplayStatics::DoesSaveGameExist(Scope->Slot,0),Resolution.X!=1920);
            TArray<UUserWidget*> Menus; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Menus,UMainMenuWidget::StaticClass(),true);
            Test->TestTrue(TEXT("Menu released after travel"),Menus.IsEmpty()); return true;
        }
        if (!bOpened)
        {
            if (Page==TEXT("Menu"))
            {
                TArray<UUserWidget*> Menus; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Menus,UMainMenuWidget::StaticClass(),true);
                if (Menus.IsEmpty()) return false;
                Test->TestNull(TEXT("Menu spawns no combat player"),UGameplayStatics::GetPlayerPawn(W,0));
                auto* Menu=Cast<UMainMenuWidget>(Menus[0]);
                Test->TestTrue(TEXT("Valid temporary save enables continue"),Cast<UButton>(Menu->GetWidgetFromName(TEXT("ContinueButton")))->GetIsEnabled());
                Menu->StartNewGame(); Test->TestTrue(TEXT("New game requests confirmation"),Menu->GetWidgetFromName(TEXT("ConfirmPanel"))->IsVisible());
                Menu->CancelNewGame(); Test->TestTrue(TEXT("Cancel preserves temporary progress"),UGameplayStatics::DoesSaveGameExist(Scope->Slot,0));
            }
            else
            {
                auto* P=Cast<ATPCCharacter>(UGameplayStatics::GetPlayerPawn(W,0)); if (!P || W->GetTimeSeconds()<1) return false;
                P->HealthComponent->SetEncounterInvulnerable(true);
                P->SetActorLocation(FVector(-1600,-800,115)); P->GetController()->SetControlRotation(FRotator(-12,70,0));
                P->EquipmentComponent->EquipWeapon(LoadObject<UWeaponDefinition>(nullptr,TEXT("/Game/Third/DataAsset/DA_TestSword.DA_TestSword")));
                P->EquipmentComponent->SetWeaponDrawn(true);
                Cast<ATPCPlayerController>(P->GetController())->ToggleInventory();
                TArray<UUserWidget*> UIs; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,UIs,UInventoryWidget::StaticClass(),true);
                if (UIs.IsEmpty()) { Test->AddError(TEXT("Inventory unavailable")); return true; }
                Cast<UInventoryWidget>(UIs[0])->SwitchPage(Page==TEXT("Attributes")?1:0);
            }
            bOpened=true; ReadyAt=FPlatformTime::Seconds()+5; return false;
        }
        if (FPlatformTime::Seconds()<ReadyAt) return false;
        if (Page!=TEXT("Menu")) for (TActorIterator<AInventoryPreviewActor> It(W);It;++It)
        {
            TArray<FLinearColor> Colors; It->Capture->TextureTarget->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(Colors);
            int32 BodyPixels=0,EmptyPixels=0;
            for (const auto& C:Colors) { BodyPixels+=C.A<.5f; EmptyPixels+=C.A>.9f; }
            Test->TestTrue(TEXT("Portrait has rendered model pixels"),BodyPixels>100); Test->TestTrue(TEXT("Portrait preserves transparent surroundings"),EmptyPixels>Colors.Num()/2);
        }
        auto View=W->GetGameViewport()->GetGameViewportWidget(); if (!View) return false;
        TArray<FColor> Pixels; FIntVector Size;
        if (!FSlateApplication::Get().TakeScreenshot(View.ToSharedRef(),Pixels,Size)) { Test->AddError(TEXT("Slate capture failed")); return true; }
        const FString Folder=FPaths::ProjectSavedDir()/TEXT("CombatUI/Captures"); IFileManager::Get().MakeDirectory(*Folder,true);
        TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);
        const FString File=Folder/FString::Printf(TEXT("%s_%dx%d.png"),*Page,Resolution.X,Resolution.Y);
        Test->TestTrue(TEXT("Saved actual UMG screenshot"),FFileHelper::SaveArrayToFile(PNG,*File));
        Test->TestEqual(TEXT("Capture width"),Size.X,Resolution.X); Test->TestEqual(TEXT("Capture height"),Size.Y,Resolution.Y);
        if (Page==TEXT("Menu") && Resolution.X!=1280)
        {
            TArray<UUserWidget*> Menus; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Menus,UMainMenuWidget::StaticClass(),true);
            auto* Menu=Cast<UMainMenuWidget>(Menus[0]);
            if (Resolution.X==1920) { Menu->StartNewGame(); Menu->ConfirmNewGame(); } else Menu->ContinueGame();
            bTravelSent=true; return false;
        }
        return true;
    }
};
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FCombatUIVisual,"ThirdPerson.CombatUI.Visual",CombatUITest::Flags)
void FCombatUIVisual::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
    for (const TCHAR* Page:{TEXT("Menu"),TEXT("Bag"),TEXT("Attributes")}) for (const TCHAR* Size:{TEXT("1280x720"),TEXT("1920x1080"),TEXT("2560x1080")})
    { const FString Name=FString(Page)+TEXT(".")+Size; Names.Add(Name); Commands.Add(Name); }
}
bool FCombatUIVisual::RunTest(const FString& Parameters)
{
    FString Page,Size,X,Y; Parameters.Split(TEXT("."),&Page,&Size); Size.Split(TEXT("x"),&X,&Y); FIntPoint Resolution(FCString::Atoi(*X),FCString::Atoi(*Y));
    auto Scope=MakeShared<CombatUITest::FSlotScope>();
    if (Page==TEXT("Menu")) { auto* S=NewObject<UTPCSaveGame>(); S->bHasCheckpoint=false; UGameplayStatics::SaveGameToSlot(S,Scope->Slot,0); }
    FAutomationEditorCommonUtils::LoadMap(Page==TEXT("Menu")?TEXT("/Game/Third/UI/MainMenu/L_MainMenu"):TEXT("/Game/Third/Bosses/Countess/Maps/L_CountessBossTest"));
    FRequestPlaySessionParams Params; auto* Settings=DuplicateObject<ULevelEditorPlaySettings>(GetMutableDefault<ULevelEditorPlaySettings>(),GetTransientPackage());
    Settings->NewWindowWidth=Resolution.X; Settings->NewWindowHeight=Resolution.Y; Settings->CenterNewWindow=false; Settings->NewWindowPosition=FIntPoint::ZeroValue;
    Params.EditorPlaySettings=Settings; Params.bAllowOnlineSubsystem=false;
    auto Window=SNew(SWindow).Title(FText::FromString(TEXT("Combat UI validation"))).ClientSize(FVector2D(Resolution.X,Resolution.Y)).ScreenPosition(FVector2D::ZeroVector)
        .AutoCenter(EAutoCenter::None).SaneWindowPlacement(false).AdjustInitialSizeAndPositionForDPIScale(false).CreateTitleBar(false).UseOSWindowBorder(false).SizingRule(ESizingRule::FixedSize);
    FSlateApplication::Get().AddWindow(Window); Params.CustomPIEWindow=Window; GEditor->RequestPlaySession(Params);
    ADD_LATENT_AUTOMATION_COMMAND(CombatUITest::FCapture(this,Scope,Page,Resolution)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
