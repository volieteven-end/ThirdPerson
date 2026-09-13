#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "../Character/TPCCharacter.h"
#include "../Components/CombatComponent.h"
#include "../Components/ActionComponent.h"
#include "../Components/EquipmentComponent.h"
#include "../Components/StaminaComponent.h"
#include "../Components/HealthComponent.h"
#include "../Weapons/WeaponDefinition.h"

namespace DivePlayback
{
struct FScope
{
    FString Command=FCommandLine::Get(),Slot=TEXT("DivePlayback_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    bool Fixed=FApp::UseFixedTimeStep(); double Delta=FApp::GetFixedDeltaTime();
    FScope() { FCommandLine::Set(*(TEXT("-TPCSaveSlot=")+Slot+TEXT(" ")+Command)); }
    ~FScope() { FApp::SetUseFixedTimeStep(Fixed); FApp::SetFixedDeltaTime(Delta); UGameplayStatics::DeleteGameInSlot(Slot,0); FCommandLine::Set(*Command); }
};
class FScenario : public IAutomationLatentCommand
{
    FAutomationTestBase* Test; TSharedPtr<FScope> Scope; int32 FPS,Case=0,Stage=0;
    double WallStart=FPlatformTime::Seconds(),Started=0;
    TWeakObjectPtr<ATPCCharacter> Player;
    bool SawContact=false,SawDescent=false,SawLand=false,Interrupted=false,PrematureLand=false;
    float ContactPose=-1,LastStartPose=0,LastDescentPose=0;
    FString Trace=TEXT("fps,case,time,feet_z,section,pose,falling,active\n");
    FString Label(const TCHAR* Text) const { return FString::Printf(TEXT("%d FPS case %d: %s"),FPS,Case,Text); }
    void Surface(UWorld* W,FVector Center,FVector Scale,float Pitch=0)
    {
        auto* A=W->SpawnActor<AStaticMeshActor>(Center,FRotator(Pitch,0,0));
        A->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
        A->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        A->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll")); A->SetActorScale3D(Scale);
    }
public:
    FScenario(FAutomationTestBase* T,int32 Rate,TSharedPtr<FScope> S):Test(T),Scope(S),FPS(Rate){}
    ~FScenario()
    {
        const FString Dir=FPaths::ProjectSavedDir()/TEXT("ArenaExpansion"); IFileManager::Get().MakeDirectory(*Dir,true);
        FFileHelper::SaveStringToFile(Trace,*(Dir/FString::Printf(TEXT("dive_playback_%d.csv"),FPS)));
    }
    bool Update() override
    {
        if (FPlatformTime::Seconds()-WallStart>90) { Test->AddError(Label(TEXT("timeout"))); return true; }
        UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr; if (!W || !W->HasBegunPlay() || W->GetTimeSeconds()<.6f) return false;
        if (Case==7) return true;
        if (Stage==0)
        {
            auto* P=Cast<ATPCCharacter>(UGameplayStatics::GetPlayerPawn(W,0)); if (!P) return false; Player=P;
            P->EquipmentComponent->EquipWeapon(LoadObject<UWeaponDefinition>(nullptr,TEXT("/Game/Third/DataAsset/DA_TestSword.DA_TestSword")));
            P->EquipmentComponent->SetWeaponDrawn(true); P->HealthComponent->SetEncounterInvulnerable(true);
            P->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
            P->bEnableRootMotionTurn=false;
            Surface(W,FVector(20000,0,-30),FVector(80,80,.6f));
            Surface(W,FVector(20000,1500,100),FVector(12,10,.4f),12);
            Surface(W,FVector(20000,-1500,50),FVector(12,10,1));
            W->GetWorldSettings()->MinUndilatedFrameTime=0; W->GetWorldSettings()->MaxUndilatedFrameTime=1;
            FApp::SetUseFixedTimeStep(true); FApp::SetFixedDeltaTime(1./FPS); Stage=1;
        }
        auto* P=Player.Get(); if (!P) return true;
        auto* Move=P->GetCharacterMovement(); auto* Combat=P->CombatComponent.Get(); auto* Anim=P->GetMesh()->GetAnimInstance();
        UAnimMontage* Montage=P->ActionComponent->GetActionSet()->Dive->Montage;
        const float StartEnd=Montage->CompositeSections[Montage->GetSectionIndex(TEXT("Loop"))].GetTime();
        const float DescentStart=Montage->CompositeSections[Montage->GetSectionIndex(TEXT("Descent"))].GetTime();
        const float LandStart=Montage->CompositeSections[Montage->GetSectionIndex(TEXT("Land"))].GetTime();
        if (Stage==1)
        {
            Combat->CancelActiveAttack(); P->ClearMoveInput(); P->EndJump(); P->StaminaComponent->SetCurrentStamina(100.f);
            const float Heights[]={2,50,250,1100,90,2,2};
            const FVector Base(20000,Case==4?1500:Case==5?-1500:0,Case==4?125:Case==5?100:0);
            P->SetActorLocationAndRotation(Base+FVector(0,0,P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+Heights[Case]),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
            Move->SetMovementMode(MOVE_Falling); Move->StopMovementImmediately();
            if (P->GetController()) P->GetController()->SetControlRotation(FRotator::ZeroRotator);
            SawContact=SawDescent=SawLand=Interrupted=PrematureLand=false; ContactPose=-1; LastStartPose=LastDescentPose=0;
            P->HandleAirDiveAttack();
            Test->TestTrue(Label(TEXT("air R starts dive")),Combat->IsAirDiveDescending());
            Started=W->GetTimeSeconds(); Stage=2; return false;
        }
        const float T=W->GetTimeSeconds()-Started,Pose=Anim->Montage_GetPosition(Montage);
        const FName Section=Anim->Montage_GetCurrentSection(Montage);
        const bool Active=Combat->IsMeleeAttackInProgress();
        Trace+=FString::Printf(TEXT("%d,%d,%.4f,%.3f,%s,%.4f,%d,%d\n"),FPS,Case,T,Move->GetActorFeetLocation().Z,*Section.ToString(),Pose,Move->IsFalling(),Active);
        if (!Move->IsFalling() && !SawContact) { SawContact=true; ContactPose=Pose; }
        if (Section==TEXT("Start")) LastStartPose=Pose;
        if (Section==TEXT("Descent") || Section==TEXT("ContactWait")) { SawDescent=true; LastDescentPose=Pose; }
        if (Section==TEXT("Land")) { SawLand=true; PrematureLand |= Move->IsFalling(); }
        if (Case==6 && SawContact && T>.12f && !Interrupted) { Combat->CancelActiveAttack(.05f); Interrupted=true; }
        if (T>.4f && !Active)
        {
            if (Case==6) Test->TestTrue(Label(TEXT("legitimate interruption still works")),Interrupted);
            else
            {
                Test->TestTrue(Label(TEXT("physical ground reached")),SawContact);
                Test->TestTrue(Label(TEXT("Start plays to its end")),LastStartPose>=StartEnd-2.5f/FPS);
                Test->TestTrue(Label(TEXT("full Descent reaches authored contact")),SawDescent && LastDescentPose>=LandStart-2.5f/FPS);
                Test->TestTrue(Label(TEXT("Land recovery plays")),SawLand);
                Test->TestFalse(Label(TEXT("no Land section while airborne")),PrematureLand);
                if (Case==0 || Case==1 || Case==5) Test->TestTrue(Label(TEXT("early physical contact does not skip Start")),ContactPose>=0 && ContactPose<DescentStart);
            }
            ++Case; Stage=1; return false;
        }
        if (T>7.f) { Test->AddError(Label(TEXT("dive stuck"))); ++Case; Stage=1; }
        return false;
    }
};
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FDivePlaybackTest,"ThirdPerson.Combat.PIE.DiveFullPlayback",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FDivePlaybackTest::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{ for (int32 FPS:{30,60,120}) { Names.Add(FString::Printf(TEXT("%dFPS"),FPS)); Commands.Add(FString::FromInt(FPS)); } }
bool FDivePlaybackTest::RunTest(const FString& Parameters)
{
    auto Scope=MakeShared<DivePlayback::FScope>();
    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Third/Bosses/Countess/Maps/L_CountessBossTest"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(DivePlayback::FScenario(this,FCString::Atoi(*Parameters),Scope));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
