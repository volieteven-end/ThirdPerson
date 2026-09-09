#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"
#include "Slate/SceneViewport.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "Misc/Parse.h"
#include "InputKeyEventArgs.h"
#include "HAL/FileManager.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "../Tutorial/TutorialDirector.h"
#include "../Tutorial/TutorialActors.h"
#include "../Tutorial/TutorialGameMode.h"
#include "../Character/TPCCharacter.h"
#include "../Components/ActionComponent.h"
#include "../Components/CombatComponent.h"
#include "../Components/EquipmentComponent.h"
#include "../Components/HealthComponent.h"
#include "../Components/StaminaComponent.h"
#include "../Components/InventoryComponent.h"
#include "../Components/LevelComponent.h"
#include "../Save/TPCSaveGame.h"
#include "../Save/TPCSaveSlots.h"
#include "../Save/CheckpointActor.h"
#include "../UI/HealthPotionWidget.h"
#include "../Weapons/WeaponDefinition.h"
#include "../Weapons/WeaponActor.h"
#include "../Items/PickupActor.h"
#include "BossReadabilityCapture.h"

namespace TutorialPIE
{
const TCHAR* Map=TEXT("/Game/Third/Tutorial/Maps/L_ForestTutorial");
struct FSaveScope
{
    FString Original=FCommandLine::Get();
    FString Slot=TEXT("TutorialAutomation_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    FSaveScope()
    {
        FCommandLine::Set(*(TEXT("-TPCSaveSlot=")+Slot+TEXT(" ")+Original));
        auto* Save=NewObject<UTPCSaveGame>(); Save->PlayerTransform=FTransform(FVector(34567,-45678,5000));
        Save->PlayerHealth=7.f; Save->PlayerLevel=9; Save->EnemiesDefeated=3;
        auto& Potions=Save->InventorySlots.AddDefaulted_GetRef();
        Potions.ItemDefinition=TSoftObjectPtr<UItemDefinition>(FSoftObjectPath(TEXT("/Game/Third/DataAsset/DA_Test0Rb.DA_Test0Rb")));
        Potions.Count=42;
        UGameplayStatics::SaveGameToSlot(Save,Slot,0);
    }
    ~FSaveScope() { UGameplayStatics::DeleteGameInSlot(Slot,0); FCommandLine::Set(*Original); }
};
class FStart : public IAutomationLatentCommand
{
public:
    bool Update() override
    {
        FRequestPlaySessionParams Params;
        auto* Settings=DuplicateObject<ULevelEditorPlaySettings>(GetMutableDefault<ULevelEditorPlaySettings>(),GetTransientPackage());
        Settings->NewWindowWidth=1920; Settings->NewWindowHeight=1080;
        Settings->CenterNewWindow=false; Settings->NewWindowPosition=FIntPoint(0,0);
        Params.EditorPlaySettings=Settings; Params.bAllowOnlineSubsystem=false;
        auto Window=SNew(SWindow).Title(FText::FromString(TEXT("Tutorial automated viewport")))
            .ClientSize(FVector2D(1920,1080)).ScreenPosition(FVector2D::ZeroVector)
            .AutoCenter(EAutoCenter::None).SaneWindowPlacement(false)
            .AdjustInitialSizeAndPositionForDPIScale(false).CreateTitleBar(false)
            .UseOSWindowBorder(false).SizingRule(ESizingRule::FixedSize);
        FSlateApplication::Get().AddWindow(Window);
        Params.CustomPIEWindow=Window;
        GEditor->RequestPlaySession(Params); return true;
    }
};
bool CaptureHUD(UWorld* W,const FString& Name,FAutomationTestBase* Test)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("TutorialVisualAudit")))return true;
    auto View=W->GetGameViewport()->GetGameViewportWidget(); if(!View.IsValid())return false;
    TArray<FColor> Pixels; FIntVector Size;
    if(!FSlateApplication::Get().TakeScreenshot(View.ToSharedRef(),Pixels,Size))return false;
    TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);
    const FString Dir=FPaths::ProjectSavedDir()/TEXT("ForestTutorial/Captures"); IFileManager::Get().MakeDirectory(*Dir,true);
    Test->AddInfo(FString::Printf(TEXT("HUD capture %s = %dx%d"),*Name,Size.X,Size.Y));
    if(Name.Contains(TEXT("1920"))){Test->TestEqual(TEXT("Actual full-HD width"),Size.X,1920);Test->TestEqual(TEXT("Actual full-HD height"),Size.Y,1080);}
    if(Name.Contains(TEXT("1280"))){Test->TestEqual(TEXT("Actual HD width"),Size.X,1280);Test->TestEqual(TEXT("Actual HD height"),Size.Y,720);}
    return FFileHelper::SaveArrayToFile(PNG,*(Dir/Name));
}
void Key(APlayerController* PC,FKey K,bool Down)
{ PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,Down?IE_Pressed:IE_Released,Down?1.f:0.f)); }

class FSmoke : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TSharedPtr<FSaveScope> Save;
    int32 Stage=0;
    double Started=FPlatformTime::Seconds(), StepTime=Started;
    TWeakObjectPtr<ATPCCharacter> Old;
    TWeakObjectPtr<ACameraActor> Camera;
    bool bOverviewReady = false;
    void Next(){++Stage;StepTime=FPlatformTime::Seconds();}
public:
    FSmoke(FAutomationTestBase* T,TSharedPtr<FSaveScope> S):Test(T),Save(S){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>240){Test->AddError(FString::Printf(TEXT("Tutorial smoke timeout at %d"),Stage));return true;}
        UWorld* W=GEditor?GEditor->PlayWorld:nullptr; if(!W||!W->HasBegunPlay())return false;
        auto* PC=Cast<ATPCPlayerController>(UGameplayStatics::GetPlayerController(W,0));
        auto* P=PC?Cast<ATPCCharacter>(PC->GetPawn()):nullptr; if(!P)return false;
        auto* D=ATutorialDirector::Find(W);
        const double T=FPlatformTime::Seconds()-StepTime;
        if(Stage==14)
        {
            if(D)return false;
            Test->TestTrue(TEXT("Return restores the campaign game mode"),W->GetAuthGameMode<ATPCGameMode>()->bUsePersistentPlayerSave);
            Test->TestEqual(TEXT("Campaign still reads the untouched disposable save"),P->LevelComponent->GetLevel(),9);
            Test->TestEqual(TEXT("Original saved health is still restored in campaign"),P->HealthComponent->GetCurrentHealth(),7.f);
            Test->TestEqual(TEXT("Original saved inventory is still restored in campaign"),P->InventoryComponent->GetHealthPotionCount(),42);
            UGameplayStatics::OpenLevel(W,FName(Map));Next();return false;
        }
        if(!D){Test->AddError(TEXT("Tutorial director missing in actual saved map"));return true;}
        switch(Stage)
        {
        case 0:
            if(T<3.)return false;
            Test->TestTrue(TEXT("Saved map uses tutorial game mode"),W->GetAuthGameMode()->IsA<ATutorialGameMode>());
            Test->TestTrue(TEXT("Campaign position is ignored"),FVector::Dist2D(P->GetActorLocation(),FVector(-5150,-2400,0))<100);
            Test->TestEqual(TEXT("Clean training level"),P->LevelComponent->GetLevel(),1);
            Test->TestEqual(TEXT("Clean training health"),P->HealthComponent->GetCurrentHealth(),100.f);
            Test->TestEqual(TEXT("Exactly two initial potions"),P->InventoryComponent->GetHealthPotionCount(),2);
            Test->TestEqual(TEXT("Dedicated sword avoids the user's edited damage"),P->EquipmentComponent->GetEquippedWeaponDefinition()->Damage,20.f);
            Test->TestTrue(TEXT("Instruction is actually active"),D->GetInstruction().ToString().Contains(TEXT("第一个")));
            Test->TestTrue(TEXT("Capture initial player view"),CaptureHUD(W,TEXT("01_Entry_1920.png"),Test));
            PC->TogglePauseMenu(); Next(); return false;
        case 1:
            if(T<.4)return false;
            Test->TestTrue(TEXT("Esc menu pauses"),UGameplayStatics::IsGamePaused(W));
            Test->TestTrue(TEXT("Capture tutorial menu"),CaptureHUD(W,TEXT("02_Menu_1920.png"),Test));
            D->ContinuePractice(); Next();return false;
        case 2:
            Test->TestFalse(TEXT("Continue restores game time"),UGameplayStatics::IsGamePaused(W));
            Test->TestFalse(TEXT("Continue releases cursor"),PC->bShowMouseCursor);
            PC->ToggleInventory(); Test->TestTrue(TEXT("Existing backpack still opens"),PC->bShowMouseCursor);
            Next();return false;
        case 3:
            if(T<.3)return false;
            Test->TestTrue(TEXT("Capture backpack compatibility"),CaptureHUD(W,TEXT("03_Backpack.png"),Test));
            PC->ToggleInventory();
            for(int32 I=0;I<4;++I)D->SkipLesson();
            D->RetryLesson(); Next();return false;
        case 4:
            if(T<.8)return false;
            Test->TestEqual(TEXT("Skip reaches potion lesson"),D->GetProgress().Lesson,4);
            Test->TestEqual(TEXT("Potion lesson starts half-health"),P->HealthComponent->GetCurrentHealth(),50.f);
            Test->TestTrue(TEXT("Retry spawns at the current station"),FVector::Dist2D(P->GetActorLocation(),D->GetCheckpoint().GetLocation())<80);
            Test->TestTrue(TEXT("Capture potion instruction"),CaptureHUD(W,TEXT("04_Potion_1920.png"),Test));
            Key(PC,EKeys::Q,true); Next();return false;
        case 5:
            Key(PC,EKeys::Q,false);
            if(T<.3)return false;
            Test->TestEqual(TEXT("Real Q input completes potion objective"),D->GetProgress().Lesson,5);
            Test->TestEqual(TEXT("Potion was really consumed"),P->InventoryComponent->GetHealthPotionCount(),1);
            Test->TestEqual(TEXT("Potion really healed"),P->HealthComponent->GetCurrentHealth(),75.f);
            D->RetryLesson(); Next();return false;
        case 6:
            if(T<1.)return false;
            Test->TestNotNull(TEXT("Current station creates a real training enemy"),D->GetTrainingTarget());
            Test->TestTrue(TEXT("Training enemy uses real animation instance"),D->GetTrainingTarget()&&D->GetTrainingTarget()->GetMesh()->GetAnimInstance());
            Old=P; P->HealthComponent->ApplyDamage(1000000.f); Next();return false;
        case 7:
            if(!PC->IsDeathScreenOpen())return false;
            PC->RestartAfterDeath(); Next();return false;
        case 8:
            if(T<.8)return false;
            Test->TestTrue(TEXT("Death creates a fresh pawn"),P!=Old.Get());
            Test->TestEqual(TEXT("Death retains current lesson"),D->GetProgress().Lesson,5);
            Test->TestEqual(TEXT("Completed potion lesson survives death"),D->GetProgress().Status[4],ETutorialLessonStatus::Completed);
            Test->TestTrue(TEXT("Death returns to station checkpoint"),FVector::Dist2D(P->GetActorLocation(),D->GetCheckpoint().GetLocation())<90);
            Test->TestFalse(TEXT("Death retry does not leave game paused"),UGameplayStatics::IsGamePaused(W));
            P->SetActorLocation(FVector(0,0,-600),false,nullptr,ETeleportType::TeleportPhysics);Old=P;Next();return false;
        case 9:
            if(T<.8)return false;
            Test->TestTrue(TEXT("Out-of-bounds recovery creates clean pawn"),P!=Old.Get());
            Test->TestTrue(TEXT("Out-of-bounds returns to checkpoint"),FVector::Dist2D(P->GetActorLocation(),D->GetCheckpoint().GetLocation())<90);
            // This is a disposable runtime checkpoint, not a placed campaign actor or a real save.
            W->SpawnActor<ACheckpointActor>(P->GetActorLocation(),FRotator::ZeroRotator);
            Next();return false;
        case 10:
            if(T<.4)return false;
            if(auto* Snapshot=Cast<UTPCSaveGame>(UGameplayStatics::LoadGameFromSlot(Save->Slot,0)))
            {
                Test->TestEqual(TEXT("Training checkpoint did not overwrite health"),Snapshot->PlayerHealth,7.f);
                Test->TestTrue(TEXT("Training checkpoint did not overwrite position"),Snapshot->PlayerTransform.GetLocation().Equals(FVector(34567,-45678,5000)));
            }
            else Test->AddError(TEXT("Disposable save disappeared"));
            D->SkipLesson(); Next();return false;
        case 11:
            if(T<.8)return false;
            Test->TestTrue(TEXT("Basic completion opens free practice"),D->IsComplete());
            Test->TestTrue(TEXT("Optional air station unlocked"),D->GetProgress().IsUnlocked(6));
            Test->TestEqual(TEXT("Skipped combat is recorded distinctly"),D->GetProgress().Status[5],ETutorialLessonStatus::Skipped);
            Test->TestTrue(TEXT("Capture completion summary"),CaptureHUD(W,TEXT("05_Completion.png"),Test));
            D->ContinuePractice();
            if(auto Window=W->GetGameViewport()->GetWindow())Window->Resize(FVector2D(1280,720));
            Next();return false;
        case 12:
            if(T<1.)return false;
            Test->TestTrue(TEXT("Capture 720p UI"),CaptureHUD(W,TEXT("06_FreePractice_1280.png"),Test));
            Camera=W->SpawnActor<ACameraActor>(FVector(-7700,-7600,8500),FRotator(-41,44,0));
            Camera->GetCameraComponent()->FieldOfView=66;
            PC->SetViewTarget(Camera.Get()); PC->TogglePauseMenu(); Next();return false;
        case 13:
            if (!bOverviewReady)
            {
                if(T<2.)return false;
                Test->TestTrue(TEXT("Capture 720p tutorial menu"),CaptureHUD(W,TEXT("Menu_1280.png"),Test));
                D->ContinuePractice(); PC->SetViewTarget(Camera.Get());
                bOverviewReady=true; StepTime=FPlatformTime::Seconds(); return false;
            }
            if(T<1.)return false;
            if(FParse::Param(FCommandLine::Get(),TEXT("TutorialVisualAudit")))
                Test->TestTrue(TEXT("Capture whole authored map"),CaptureBossReadabilityFrame(W,FPaths::ProjectSavedDir()/TEXT("ForestTutorial/Captures/07_Overview.png")));
            D->ReturnToCampaign();Next();return false;
        case 15:
            if(T<1.)return false;
            Test->TestEqual(TEXT("Re-entering starts a new tutorial session"),D->GetProgress().Lesson,0);
            Test->TestEqual(TEXT("Old skipped progress was not serialized"),D->GetProgress().Status[1],ETutorialLessonStatus::Locked);
            Test->TestEqual(TEXT("Re-entering has fresh training supplies"),P->InventoryComponent->GetHealthPotionCount(),2);
            return true;
        default:break;
        }
        return false;
    }
};

// Drives the saved map through actual input and collision/animation results. No objective
// injection, actor teleportation, manufactured damage or save-backed character fixture.
class FCourseRun : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TSharedPtr<FSaveScope> Save;
    TWeakObjectPtr<ATPCPlayerController> Controller;
    TSet<FKey> Held;
    TMap<FKey,double> ReleaseAt;
    TSet<uint64> Queued;
    int32 LastLesson=-2,LastObjective=-2,DodgePhase=0;
    double WallStart=FPlatformTime::Seconds(),ChangedAt=0,LastLog=0,ActionAt=-1;
    bool JumpSent=false,GuardReleased=false,SeenBasicSummary=false;
    bool AirOnly=false,AirSetup=false;
    int32 AirRate=0;
    bool PreviousFixed=FApp::UseFixedTimeStep();
    double PreviousDelta=FApp::GetFixedDeltaTime();
    void Set(FKey K,bool Down)
    {
        if(!Controller.IsValid() || Held.Contains(K)==Down)return;
        Key(Controller.Get(),K,Down); if(Down)Held.Add(K);else Held.Remove(K);
    }
    void Pulse(FKey K,double Now,float Duration=.055f)
    { if(!Held.Contains(K)){Set(K,true);ReleaseAt.Add(K,Now+Duration);} }
    void Release()
    { auto Copy=Held;for(auto K:Copy)Set(K,false);ReleaseAt.Reset(); }
    bool Walk(ATPCCharacter* P,FVector Goal,float Radius=55.f)
    {
        const FVector Delta=Goal-P->GetActorLocation();
        if(Delta.Size2D()<Radius){Set(EKeys::W,false);return true;}
        Controller->SetControlRotation(FRotator(-14,Delta.Rotation().Yaw,0));Set(EKeys::W,true);return false;
    }
    void Face(ATPCCharacter* P,ATutorialTrainingEnemy* E)
    {
        const auto R=FRotator(0,(E->GetActorLocation()-P->GetActorLocation()).Rotation().Yaw,0);
        Controller->SetControlRotation(FRotator(-14,R.Yaw,0));
        if(P->ActionComponent->GetActionState()==ETPCActionState::Free)P->SetActorRotation(R);
    }
public:
    FCourseRun(FAutomationTestBase* T,TSharedPtr<FSaveScope> S,int32 InAirRate=0):Test(T),Save(S),AirOnly(InAirRate>0),AirRate(InAirRate){}
    ~FCourseRun() override { Release();if(AirOnly){FApp::SetUseFixedTimeStep(PreviousFixed);FApp::SetFixedDeltaTime(PreviousDelta);} }
    bool Update() override
    {
        UWorld* W=GEditor?GEditor->PlayWorld:nullptr;if(!W||!W->HasBegunPlay())return false;
        auto* PC=Cast<ATPCPlayerController>(UGameplayStatics::GetPlayerController(W,0));Controller=PC;
        auto* P=PC?Cast<ATPCCharacter>(PC->GetPawn()):nullptr;auto* D=ATutorialDirector::Find(W);if(!P||!D)return false;
        if(AirOnly&&!AirSetup)
        {
            FApp::SetUseFixedTimeStep(true);FApp::SetFixedDeltaTime(1./AirRate);
            W->GetWorldSettings()->MinUndilatedFrameTime=0;W->GetWorldSettings()->MaxUndilatedFrameTime=1;
            AirSetup=true;for(int32 I=0;I<6;++I)D->SkipLesson();
            P->SetActorLocation(FVector(-3050,2300,112),false,nullptr,ETeleportType::TeleportPhysics);
            for(TActorIterator<ATutorialZone> It(W);It;++It)if(It->bEntry&&It->LessonIndex==6)D->HandleZone(*It,P);
            D->RetryLesson();return false;
        }
        const double Now=W->GetTimeSeconds();
        for(auto It=ReleaseAt.CreateIterator();It;++It)if(Now>=It.Value()){Set(It.Key(),false);It.RemoveCurrent();}
        const auto& Progress=D->GetProgress();const int32 L=Progress.Lesson,O=Progress.Objective;
        if(L!=LastLesson||O!=LastObjective)
        {
            Test->AddInfo(FString::Printf(TEXT("REAL_ROUTE lesson=%d objective=%d position=%s"),L,O,*P->GetActorLocation().ToString()));
            if(L!=LastLesson){Release();Queued.Reset();DodgePhase=0;}
            LastLesson=L;LastObjective=O;ChangedAt=Now;
        }
        if(FPlatformTime::Seconds()-WallStart>480 || Now-ChangedAt>70)
        {
            CaptureHUD(W,TEXT("Route_Failure.png"),Test);
            Test->AddError(FString::Printf(TEXT("Real course stalled lesson=%d objective=%d at %s, instruction=%s"),L,O,*P->GetActorLocation().ToString(),*D->GetInstruction().ToString()));
            return true;
        }
        auto* A=P->ActionComponent.Get();const auto* Def=A->GetActiveDefinition();auto* E=D->GetTrainingTarget();
        if(Now-LastLog>2.)
        {
            UE_LOG(LogTemp,Display,TEXT("ROUTE_TRACE l=%d o=%d c=%d pos=%s act=%s t=%.3f enemy=%s hp=%.1f"),L,O,Progress.Count,
                *P->GetActorLocation().ToString(),Def?*Def->ActionId.ToString():TEXT("none"),A->GetMontagePosition(),E?*E->GetActorLocation().ToString():TEXT("none"),P->HealthComponent->GetCurrentHealth());LastLog=Now;
        }
        if(P->HealthComponent->GetCurrentHealth()<=0){Test->AddError(TEXT("Real route player unexpectedly died"));return true;}
        if(UGameplayStatics::IsGamePaused(W))
        {
            Test->TestTrue(TEXT("Basic summary is reached without skips"),D->IsComplete());
            CaptureHUD(W,TEXT("08_BasicsCompleted.png"),Test);SeenBasicSummary=true;Release();D->ContinuePractice();return false;
        }
        if(L==INDEX_NONE)
        {
            if(Progress.Status[6]==ETutorialLessonStatus::Completed)
            {
                if(AirOnly){Release();return true;}
                Release();for(int32 I=0;I<7;++I)Test->TestEqual(FString::Printf(TEXT("Actual course %d completed, not skipped"),I),Progress.Status[I],ETutorialLessonStatus::Completed);
                Test->TestTrue(TEXT("Completion menu appeared"),SeenBasicSummary);
                Test->TestEqual(TEXT("Training kills award no experience or level"),P->LevelComponent->GetLevel(),1);
                Test->TestEqual(TEXT("Training experience remains exactly zero"),P->LevelComponent->GetCurrentExperience(),0);
                Test->TestEqual(TEXT("Training kills do not contribute to campaign victory"),W->GetAuthGameMode<ATPCGameMode>()->GetEnemiesDefeated(),0);
                int32 Drops=0;for(TActorIterator<APickupActor> It(W);It;++It)++Drops;
                Test->TestEqual(TEXT("Training enemies do not create loot"),Drops,0);
                CaptureHUD(W,TEXT("10_AllTrainingCompleted.png"),Test);return true;
            }
            Walk(P,FVector(-3050,2300,110));return false;
        }
        switch(L)
        {
        case 0:
            PC->SetControlRotation(FRotator(-14,0,0));Set(EKeys::W,true);
            Set(EKeys::SpaceBar,O==2);
            if(O==3 && !JumpSent && P->GetActorLocation().X>-2650)
            {Pulse(EKeys::F,Now);JumpSent=true;}
            break;
        case 1:
        case 5:
            if(!E){Walk(P,D->GetCheckpoint().GetLocation());break;}
            Face(P,E);
            if(A->GetActionState()==ETPCActionState::Free)
            {
                if(!Walk(P,E->GetActorLocation(),125.f))break;
                if(P->StaminaComponent->GetCurrentStamina()>40.f)Pulse(EKeys::LeftMouseButton,Now);
            }
            else if(Def && Def->ActionId.ToString().StartsWith(TEXT("Sword.Light.")) && A->GetMontagePosition()>.09f && !Queued.Contains(A->GetActionInstanceId()))
            { Queued.Add(A->GetActionInstanceId());Pulse(EKeys::LeftMouseButton,Now); }
            if(L==5 && P->HealthComponent->GetCurrentHealth()<45.f)Pulse(EKeys::Q,Now);
            break;
        case 2:
            if(DodgePhase==0)
            {if(Walk(P,FVector(2200,-2400,110),35.f)){DodgePhase=1;ActionAt=Now;}break;}
            PC->SetControlRotation(FRotator(-14,0,0));
            if(DodgePhase==1 && Now-ActionAt>.2)
            {Set(EKeys::W,true);Pulse(EKeys::SpaceBar,Now);DodgePhase=2;ActionAt=Now;}
            if(DodgePhase==2 && Now-ActionAt>.12)Set(EKeys::W,false);
            if(DodgePhase==2 && O==1 && A->GetActionState()==ETPCActionState::Free)
            {DodgePhase=3;ActionAt=Now;}
            if(DodgePhase==3)
            {
                P->SetActorRotation(FRotator::ZeroRotator);Set(EKeys::D,true);
                if(Now-ActionAt>.07){Pulse(EKeys::SpaceBar,Now);DodgePhase=4;ActionAt=Now;}
            }
            if(DodgePhase==4 && Now-ActionAt>.12)Set(EKeys::D,false);
            break;
        case 3:
            if(!E)
            {
                if(P->GetActorLocation().X<4300)Walk(P,FVector(4400,-2400,110));
                else Walk(P,D->GetCheckpoint().GetLocation());break;
            }
            Face(P,E);
            if(FVector::Dist2D(P->GetActorLocation(),E->GetActorLocation())>155.f)
            {Walk(P,E->GetActorLocation(),150);break;}
            Set(EKeys::W,false);
            if(O==0){Set(EKeys::RightMouseButton,true);break;}
            if(!GuardReleased){Set(EKeys::RightMouseButton,false);GuardReleased=true;ActionAt=Now;break;}
            if(auto* Anim=E->GetMesh()->GetAnimInstance())
            {
                auto* M=Anim->GetCurrentActiveMontage();
                if(M && Anim->Montage_GetPosition(M)>.20f && Anim->Montage_GetPosition(M)<.32f && Now-ActionAt>.7)
                {Pulse(EKeys::RightMouseButton,Now,.3f);ActionAt=Now;}
            }
            break;
        case 4:
            if(P->GetActorLocation().Y<2500)Walk(P,FVector(4400,2600,110));
            else if(P->HealthComponent->GetCurrentHealth()<100.f)
            {Set(EKeys::W,false);Pulse(EKeys::Q,Now);}
            else Walk(P,D->GetCheckpoint().GetLocation());
            break;
        case 6:
            if(!E){Walk(P,D->GetCheckpoint().GetLocation());break;}
            if(AirOnly && FParse::Param(FCommandLine::Get(),TEXT("TutorialHitAudit")) && Def && Def->ActionId==TEXT("Sword.Air.Dive"))
            {
                auto* WeaponMesh=P->EquipmentComponent->GetEquippedWeaponActor()->GetSkeletalWeaponMesh();
                const auto B=WeaponMesh->GetSocketLocation(TEXT("BladeBase")),Tip=WeaponMesh->GetSocketLocation(TEXT("BladeTip"));
                UE_LOG(LogTemp,Display,TEXT("DIVE_AUDIT action=%llu t=%.4f dt=%.4f window=%d player=%s enemy=%s blade=%s tip=%s hp=%.1f"),
                    A->GetActionInstanceId(),A->GetMontagePosition(),W->GetDeltaSeconds(),P->CombatComponent->IsComponentTickEnabled(),
                    *P->GetActorLocation().ToString(),*E->GetActorLocation().ToString(),*B.ToString(),*Tip.ToString(),E->FindComponentByClass<UHealthComponent>()->GetCurrentHealth());
            }
            Face(P,E);Set(EKeys::W,false);
            if(A->GetActionState()==ETPCActionState::Free && !P->GetCharacterMovement()->IsFalling() && E->GetLaunchPhase()==EEnemyLaunchPhase::None)
            {
                if(P->StaminaComponent->GetCurrentStamina()<95.f)break;
                if(!Walk(P,E->GetActorLocation(),120.f))break;
                if(!P->LockedTarget){Pulse(EKeys::MiddleMouseButton,Now);break;}
                Set(EKeys::C,true);Pulse(EKeys::LeftMouseButton,Now);ActionAt=Now;
            }
            if(Def)
            {
                const FName Id=Def->ActionId;
                if(Id==TEXT("Sword.Rising"))Set(EKeys::C,false);
                if((Id==TEXT("Sword.Rising")||Id==TEXT("Sword.Air.Light.1"))&& A->GetMontagePosition()>.09f && !Queued.Contains(A->GetActionInstanceId()))
                {Queued.Add(A->GetActionInstanceId());Pulse(EKeys::LeftMouseButton,Now);}
                if(Id==TEXT("Sword.Air.Light.2") && A->GetMontagePosition()>.48f && !Queued.Contains(A->GetActionInstanceId()))
                {Queued.Add(A->GetActionInstanceId());CaptureHUD(W,TEXT("09_AirCombo.png"),Test);Pulse(EKeys::R,Now);}
            }
            break;
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTutorialSmokeTest,"ThirdPerson.Tutorial.PIE.SmokeAndSaveIsolation",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTutorialSmokeTest::RunTest(const FString&)
{
    auto Scope=MakeShared<TutorialPIE::FSaveScope>();
    FAutomationEditorCommonUtils::LoadMap(TutorialPIE::Map);
    ADD_LATENT_AUTOMATION_COMMAND(TutorialPIE::FStart());
    ADD_LATENT_AUTOMATION_COMMAND(TutorialPIE::FSmoke(this,Scope));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTutorialActualCourseTest,"ThirdPerson.Tutorial.PIE.ActualCourse",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTutorialActualCourseTest::RunTest(const FString&)
{
    auto Scope=MakeShared<TutorialPIE::FSaveScope>();
    FAutomationEditorCommonUtils::LoadMap(TutorialPIE::Map);
    ADD_LATENT_AUTOMATION_COMMAND(TutorialPIE::FStart());
    ADD_LATENT_AUTOMATION_COMMAND(TutorialPIE::FCourseRun(this,Scope));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FTutorialAirAuditTest,"ThirdPerson.Tutorial.PIE.AirHitAtFixedRate",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FTutorialAirAuditTest::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{for(int32 FPS:{30,60,120}){Names.Add(FString::Printf(TEXT("%dFPS"),FPS));Commands.Add(FString::FromInt(FPS));}}
bool FTutorialAirAuditTest::RunTest(const FString& Parameters)
{
    auto Scope=MakeShared<TutorialPIE::FSaveScope>();
    FAutomationEditorCommonUtils::LoadMap(TutorialPIE::Map);
    ADD_LATENT_AUTOMATION_COMMAND(TutorialPIE::FStart());
    ADD_LATENT_AUTOMATION_COMMAND(TutorialPIE::FCourseRun(this,Scope,FCString::Atoi(*Parameters)));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
