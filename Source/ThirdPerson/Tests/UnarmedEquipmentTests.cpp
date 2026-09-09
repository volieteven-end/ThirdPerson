#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "../Character/TPCCharacter.h"
#include "../Character/TPCPlayerController.h"
#include "../Components/ActionComponent.h"
#include "../Components/CombatComponent.h"
#include "../Components/EquipmentComponent.h"
#include "../Components/HealthComponent.h"
#include "../Weapons/WeaponActor.h"
#include "../Weapons/WeaponDefinition.h"
#include "../Tutorial/TutorialActors.h"
#include "../Boss/BossDefinition.h"
#include "../Animation/MeleeTraceGeometry.h"
#include "BossReadabilityCapture.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPCReachTuningTest,"ThirdPerson.Equipment.ReachTuning",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTPCReachTuningTest::RunTest(const FString&)
{
    TestEqual(TEXT("Player melee baseline is increased once"),GetDefault<ATPCCharacter>()->CombatComponent->BaseMeleeReachScale,1.15f);
    TestEqual(TEXT("Ordinary enemies retain their baseline"),GetDefault<UCombatComponent>()->BaseMeleeReachScale,1.f);
    TestEqual(TEXT("Countess melee baseline is increased once"),GetDefault<UBossDefinition>()->MeleeReachScale,1.15f);
    const FVector Origin(10,20,90),Point(110,220,135);
    const FVector Extended=TPCMeleeTrace::Extend(Point,Origin,1.15f);
    TestTrue(TEXT("Real sweep points extend horizontally by fifteen percent"),Extended.Equals(FVector(125,250,135),.001f));
    TestEqual(TEXT("Reach tuning never raises the damage plane"),Extended.Z,Point.Z);
    TestTrue(TEXT("Scale one preserves authored sweep positions"),TPCMeleeTrace::Extend(Point,Origin,1.f).Equals(Point));
    return true;
}

namespace EquipmentPIE
{
class FRun : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TWeakObjectPtr<ATPCCharacter> Player;
    TWeakObjectPtr<ATPCPlayerController> Controller;
    TWeakObjectPtr<ATutorialTrainingEnemy> Enemy;
    TWeakObjectPtr<AWeaponActor> Sword;
    TWeakObjectPtr<UWeaponDefinition> Weapon;
    TWeakObjectPtr<UClass> ArmedClass;
    TArray<FCombatHitSpec> Hits;
    TSet<uint64> Queued;
    FKey PulseKey;
    FKey JumpKey=EKeys::F;
    double ReleaseAt=-1,Started=FPlatformTime::Seconds(),StepStart=Started;
    int32 Step=0;
    bool bCapturedPunch=false,bSawJump=false;
    FVector BeforeMove;
    void Key(FKey K,bool Down)
    {
        if(Controller.IsValid())Controller->InputKey(FInputKeyEventArgs::CreateSimulated(K,Down?IE_Pressed:IE_Released,Down?1.f:0.f));
    }
    void Pulse(FKey K) { Key(K,true); PulseKey=K; ReleaseAt=FPlatformTime::Seconds()+.045; }
    void Next() { ++Step;StepStart=FPlatformTime::Seconds(); }
    void Capture(UWorld* W,const TCHAR* Name)
    {
        if(!FParse::Param(FCommandLine::Get(),TEXT("EquipmentVisualAudit")))return;
        const FString Dir=FPaths::ProjectSavedDir()/TEXT("UnarmedCombatFix/Captures");
        IFileManager::Get().MakeDirectory(*Dir,true);
        Test->TestTrue(Name,CaptureBossReadabilityFrame(W,Dir/Name));
    }
    void OnHit(const FCombatHitSpec& Spec,const FCombatHitResult& Result,AActor* Source)
    {
        if(Source==Player.Get() && Result.ActualDamage>0.f)
        {
            Hits.Add(Spec);
            UE_LOG(LogTemp,Display,TEXT("EQUIPMENT_HIT step=%d serial=%lld damage=%.1f pos=%s enemy=%s"),Step,Spec.ActionSerial,Spec.Damage,*Player->GetActorLocation().ToString(),*Enemy->GetActorLocation().ToString());
        }
    }
    void FaceTarget(ATPCCharacter* P)
    {
        if(!Enemy.IsValid())return;
        P->GetCharacterMovement()->StopMovementImmediately();
        const FVector Target=Enemy->GetActorLocation();
        P->SetActorLocation(Target-FVector(125,0,0),false,nullptr,ETeleportType::TeleportPhysics);
        P->SetActorRotation(FRotator::ZeroRotator); Controller->SetControlRotation(FRotator::ZeroRotator);
    }
public:
    explicit FRun(FAutomationTestBase* T):Test(T){}
    ~FRun()
    {
        Key(EKeys::D,false); if(PulseKey.IsValid())Key(PulseKey,false);
        if(Enemy.IsValid())Enemy->FindComponentByClass<UHealthComponent>()->OnCombatHitResolved.RemoveAll(this);
    }
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();
        if(ReleaseAt>0 && Now>=ReleaseAt){Key(PulseKey,false);ReleaseAt=-1;}
        if(Now-Started>95){Test->AddError(FString::Printf(TEXT("Unarmed PIE timeout at step %d with %d real hits"),Step,Hits.Num()));return true;}
        UWorld* W=GEditor?GEditor->PlayWorld:nullptr; if(!W||!W->HasBegunPlay())return false;
        auto* PC=Cast<ATPCPlayerController>(UGameplayStatics::GetPlayerController(W,0));
        auto* P=PC?Cast<ATPCCharacter>(PC->GetPawn()):nullptr; if(!P)return false;
        Controller=PC; Player=P;
        auto* C=P->CombatComponent.Get(); auto* A=P->ActionComponent.Get(); auto* E=P->EquipmentComponent.Get();
        const double T=Now-StepStart;
        switch(Step)
        {
        case 0:
            if(T<1.)return false;
            if(P->DefaultMappingContext) for(const auto& Mapping:P->DefaultMappingContext->GetMappings())
                if(Mapping.Action==P->JumpAction) { JumpKey=Mapping.Key; Test->AddInfo(TEXT("Testing current jump binding: ")+JumpKey.ToString()); break; }
            Sword=E->GetEquippedWeaponActor(); Weapon=E->GetEquippedWeaponDefinition(); ArmedClass=P->GetMesh()->GetAnimClass();
            if(!Test->TestTrue(TEXT("Saved map starts with drawn equipment"),Sword.IsValid()&&E->IsWeaponDrawn()))return true;
            Enemy=W->SpawnActor<ATutorialTrainingEnemy>(P->GetActorLocation()+FVector(300,0,0),FRotator(0,180,0));
            if(!Test->TestNotNull(TEXT("Passive real skeleton target"),Enemy.Get()))return true;
            Enemy->FindComponentByClass<UHealthComponent>()->OnCombatHitResolved.AddRaw(this,&FRun::OnHit);
            Pulse(EKeys::X);Next();return false;
        case 1:
            if(T<1.9)return false;
            Test->TestFalse(TEXT("Physical X input sheathes the sword"),E->IsWeaponDrawn());
            Test->TestTrue(TEXT("The same sword remains visible"),E->GetEquippedWeaponActor()==Sword.Get()&&!Sword->IsHidden());
            Test->TestEqual(TEXT("The sword is attached to its spine mount"),Sword->GetAttachParentSocketName(),Weapon->SheathSocketName);
            Test->TestEqual(TEXT("Mannequin unarmed ABP is active after the action ends"),P->GetMesh()->GetAnimClass(),P->UnarmedAnimationClass.Get());
            Test->TestNull(TEXT("A sheathed weapon supplies no sword action set"),A->GetActionSet());
            UE_LOG(LogTemp,Display,TEXT("BACK_MOUNT actor=%s spine=%s sword=%s base=%s tip=%s"),*P->GetActorTransform().ToString(),
                *P->GetMesh()->GetSocketTransform(Weapon->SheathSocketName).ToString(),*Sword->GetActorTransform().ToString(),
                *Sword->GetSkeletalWeaponMesh()->GetSocketLocation(TEXT("BladeBase")).ToString(),*Sword->GetSkeletalWeaponMesh()->GetSocketLocation(TEXT("BladeTip")).ToString());
            Capture(W,TEXT("01_Sheathed.png")); BeforeMove=P->GetActorLocation();Key(EKeys::D,true);Next();return false;
        case 2:
            if(T<.5)return false;
            Key(EKeys::D,false);Test->TestTrue(TEXT("Unarmed movement still responds to real input"),FVector::Dist2D(BeforeMove,P->GetActorLocation())>60);
            Pulse(JumpKey);Next();return false;
        case 3:
            bSawJump |= P->GetCharacterMovement()->IsFalling();
            if(T<1.7)return false;
            Test->TestTrue(TEXT("Unarmed jump leaves the ground"),bSawJump);
            Test->TestTrue(TEXT("Unarmed jump returns to grounded locomotion"),P->GetCharacterMovement()->IsMovingOnGround());
            FaceTarget(P);Hits.Reset();Pulse(EKeys::LeftMouseButton);Next();return false;
        case 4:
            if(A->GetActionState()==ETPCActionState::Attack)
            {
                const uint64 Id=A->GetActionInstanceId();
                if(A->GetMontagePosition()>.08f&&!Queued.Contains(Id))
                {
                    Queued.Add(Id);Pulse(EKeys::LeftMouseButton);
                    UE_LOG(LogTemp,Display,TEXT("EQUIPMENT_PUNCH id=%llu montage=%s pos=%.3f player=%s enemy=%s"),Id,*GetNameSafe(P->GetMesh()->GetAnimInstance()->GetCurrentActiveMontage()),A->GetMontagePosition(),*P->GetActorLocation().ToString(),*Enemy->GetActorLocation().ToString());
                }
                if(!bCapturedPunch && A->GetMontagePosition()>.20f){Capture(W,TEXT("02_Punch.png"));bCapturedPunch=true;}
            }
            if(T<3.9)return false;
            Test->TestEqual(TEXT("Three real unarmed attacks reach the target"),Hits.Num(),3);
            {
                TSet<int64> Serials;
                for(const auto& Hit:Hits){Test->TestEqual(TEXT("Fists use unarmed damage, not the stowed sword"),Hit.Damage,25.f);Serials.Add(Hit.ActionSerial);}
                Test->TestEqual(TEXT("One real result per unarmed action"),Serials.Num(),3);
            }
            Test->TestFalse(TEXT("Punching never draws the sword"),E->IsWeaponDrawn());
            Test->TestEqual(TEXT("Unarmed combo releases its action channel"),A->GetActionState(),ETPCActionState::Free);
            Pulse(EKeys::X);Next();return false;
        case 5:
            if(T<1.9)return false;
            Test->TestTrue(TEXT("X draws the same actor"),E->IsWeaponDrawn()&&E->GetEquippedWeaponActor()==Sword.Get());
            Test->TestEqual(TEXT("Sword returns to the hand socket"),Sword->GetAttachParentSocketName(),Weapon->EquipSocketName);
            Test->TestEqual(TEXT("Drawing restores the original sword ABP"),P->GetMesh()->GetAnimClass(),ArmedClass.Get());
            Capture(W,TEXT("03_Drawn.png"));FaceTarget(P);Hits.Reset();Pulse(EKeys::LeftMouseButton);Next();return false;
        case 6:
            if(T<1.8)return false;
            Test->TestEqual(TEXT("Drawing restores a real sword hit"),Hits.Num(),1);
            if(!Hits.IsEmpty())Test->TestEqual(TEXT("Drawn attack uses sword damage"),Hits[0].Damage,Weapon->Damage);
            E->UnequipWeapon();Next();return false;
        case 7:
            if(T<.3)return false;
            Test->TestNull(TEXT("Removing equipment removes the actor"),E->GetEquippedWeaponActor());
            Test->TestEqual(TEXT("No equipment also selects the unarmed ABP"),P->GetMesh()->GetAnimClass(),P->UnarmedAnimationClass.Get());
            FaceTarget(P);Hits.Reset();Pulse(EKeys::LeftMouseButton);Next();return false;
        case 8:
            if(T<1.5)return false;
            Test->TestEqual(TEXT("No equipped weapon still allows a physical punch"),Hits.Num(),1);
            P->HealthComponent->ApplyDamage(1000000.f);Next();return false;
        case 9:
            if(!PC->IsDeathScreenOpen())return false;
            PC->RestartAfterDeath();Next();return false;
        case 10:
            if(T<1.)return false;
            Test->TestTrue(TEXT("Death while unarmed restores fresh drawn equipment"),E->IsWeaponDrawn()&&E->GetEquippedWeaponActor());
            Test->TestEqual(TEXT("Respawn restores the armed animation class"),P->GetMesh()->GetAnimClass(),ArmedClass.Get());
            Test->TestFalse(TEXT("Respawn does not leave input paused"),UGameplayStatics::IsGamePaused(W));
            return true;
        default:return true;
        }
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPCUnarmedPIETest,"ThirdPerson.Equipment.PIE.UnarmedAndBackMount",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTPCUnarmedPIETest::RunTest(const FString&)
{
    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Third/Tutorial/Maps/L_ForestTutorial"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(EquipmentPIE::FRun(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
