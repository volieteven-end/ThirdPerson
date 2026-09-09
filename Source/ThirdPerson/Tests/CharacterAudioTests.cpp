#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "AnimPose.h"
#include "Sound/SoundWave.h"
#include "AudioMixerBlueprintLibrary.h"
#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "../Audio/TPCCharacterAudioComponent.h"
#include "../Audio/TPCFootstepNotify.h"
#include "../Character/TPCCharacter.h"
#include "../AI/EnemyCharacter.h"
#include "../Components/CombatComponent.h"
#include "../Components/HealthComponent.h"
#include "../Components/StaminaComponent.h"

struct FTPCCharacterAudioTestAccess
{
    static USoundBase* Pick(UTPCCharacterAudioComponent& C, ETPCAudioCue Cue) { return C.SelectSound(Cue); }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPCAudioAssetsTest,"ThirdPerson.CharacterAudio.AssetsAndFootContacts",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTPCAudioAssetsTest::RunTest(const FString&)
{
    auto* Audio=NewObject<UTPCCharacterAudioComponent>(); TSet<FString> Packages;
    TestEqual(TEXT("All 12 cue categories are populated"),Audio->SoundBanks.Num(),12);
    for (const auto& Pair:Audio->SoundBanks)
    {
        TestTrue(TEXT("Each cue offers at least 3 variations"),Pair.Value.Sounds.Num()>=3);
        for (USoundBase* Sound:Pair.Value.Sounds)
        {
            auto* Wave=Cast<USoundWave>(Sound);
            if (!TestNotNull(TEXT("Existing one-shot SoundWave resolves"),Wave)) continue;
            TestFalse(TEXT("Footsteps and hits must not loop"),Wave->bLooping);
            TestTrue(TEXT("One-shot duration is valid"),Wave->Duration>.02f && Wave->Duration<5.f);
            TestTrue(TEXT("Wave has imported channels"),Wave->NumChannels>0);
            Packages.Add(Wave->GetOutermost()->GetName());
        }
        USoundBase* Last=nullptr;
        for (int32 I=0;I<50;++I)
        {
            auto* Next=FTPCCharacterAudioTestAccess::Pick(*Audio,Pair.Key);
            TestTrue(TEXT("No immediate repeated sample"),Next && Next!=Last); Last=Next;
        }
    }
    TestEqual(TEXT("Crouch chooses quiet bank even during speed transition"),
        UTPCCharacterAudioComponent::ChooseFootstep(450,true),ETPCAudioCue::FootCrouch);
    TestEqual(TEXT("Walk speed"),UTPCCharacterAudioComponent::ChooseFootstep(150,false),ETPCAudioCue::FootWalk);
    TestEqual(TEXT("Run speed"),UTPCCharacterAudioComponent::ChooseFootstep(450,false),ETPCAudioCue::FootRun);
    TestEqual(TEXT("Sprint speed"),UTPCCharacterAudioComponent::ChooseFootstep(650,false),ETPCAudioCue::FootSprint);
    TSet<UAnimSequence*> Sequences;
    for (const TCHAR* Name:{TEXT("BS_SwordLocomotion"),TEXT("BS_Crouch_Locomotion")})
    {
        auto* Blend=LoadObject<UBlendSpace>(nullptr,*FString::Printf(TEXT("/Game/Third/Input/%s.%s"),Name,Name));
        if (!TestNotNull(TEXT("Live locomotion blend space"),Blend)) continue;
        TestTrue(TEXT("Only dominant blend sample emits foot notifies"),Blend->NotifyTriggerMode==ENotifyTriggerMode::HighestWeightedAnimation);
        for (const auto& Sample:Blend->GetBlendSamples())
            if (Sample.Animation && Sample.Animation->GetPathName().Contains(TEXT("/Actions/Sword/Sequences/")))
                Sequences.Add(Sample.Animation);
    }
    TestEqual(TEXT("All live directional moving clips are covered"),Sequences.Num(),25);
    for (auto* Sequence:Sequences)
    {
        TSet<FName> Feet; int32 Count=0;
        for (const auto& Event:Sequence->Notifies)
            if (const auto* Notify=Cast<UTPCFootstepNotify>(Event.Notify))
            {
                ++Count; Feet.Add(Notify->FootBone);
                FAnimPose Pose; UAnimPoseExtensions::GetAnimPoseAtTime(Sequence,Event.GetTime(),FAnimPoseEvaluationOptions(),Pose);
                const auto Transform=UAnimPoseExtensions::GetBonePose(Pose,Notify->FootBone,EAnimPoseSpaces::World);
                TestTrue(Sequence->GetName()+TEXT(" notify occurs near floor contact"),Transform.GetLocation().Z<5.f);
            }
        TestEqual(Sequence->GetName()+TEXT(" has exactly 2 contact notifies"),Count,2);
        TestTrue(TEXT("Both left and right feet are authored"),Feet.Contains(TEXT("ball_l")) && Feet.Contains(TEXT("ball_r")));
    }
    TArray<FString> Sorted=Packages.Array(); Sorted.Sort();
    IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("CharacterAudio")),true);
    FFileHelper::SaveStringArrayToFile(Sorted,*(FPaths::ProjectSavedDir()/TEXT("CharacterAudio/used_sound_packages.txt")));
    return true;
}

namespace CharacterAudioPIE
{
class FScenario : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TWeakObjectPtr<ATPCCharacter> Player;
    TWeakObjectPtr<AEnemyCharacter> Enemy;
    TArray<FTPCAudioRequest> PlayerEvents,EnemyEvents;
    FDelegateHandle PlayerHandle,EnemyHandle;
    int32 Stage=0,Case=0; double Start=0,WallStart=FPlatformTime::Seconds();
    bool PreviousFixed=FApp::UseFixedTimeStep(); double PreviousDelta=FApp::GetFixedDeltaTime();
    bool Render=FParse::Param(FCommandLine::Get(),TEXT("CharacterAudioRender"));
    FString Trace=TEXT("case,actor,time,cue,sound,volume,pitch,speed,foot,audio_component\n");
    int32 Steps[4]={}; float Volume[4]={};
    static int32 Count(const TArray<FTPCAudioRequest>& Events,ETPCAudioCue Cue)
    { return Events.FilterByPredicate([Cue](const auto& E){return E.Cue==Cue;}).Num(); }
    void Record(const FTPCAudioRequest& E,bool IsEnemy)
    {
        (IsEnemy?EnemyEvents:PlayerEvents).Add(E);
        Trace+=FString::Printf(TEXT("%d,%s,%.4f,%d,%s,%.4f,%.4f,%.1f,%s,%d\n"),Case,IsEnemy?TEXT("enemy"):TEXT("player"),
            E.Time,static_cast<int32>(E.Cue),*GetNameSafe(E.Sound),E.Volume,E.Pitch,E.Speed,*E.Foot.ToString(),E.bAudioComponentCreated);
        if (Render) Test->TestTrue(TEXT("Engine creates a playable audio component"),E.bAudioComponentCreated);
    }
    void Reset(UWorld* W)
    {
        auto* P=Player.Get(); P->CombatComponent->SetCombatEnabled(false); P->CombatComponent->SetCombatEnabled(true);
        P->StopCrouch(); P->StopSprint(); P->EndJump(); P->ClearMoveInput();
        P->GetCharacterMovement()->StopMovementImmediately(); P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        P->SetActorLocationAndRotation(FVector(-500,-900,98),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
        P->GetCharacterMovement()->MaxWalkSpeed=Case==1?150.f:Case==3?650.f:450.f;
        P->StaminaComponent->SetCurrentStamina(100.f);
        if (Enemy.IsValid()) Enemy->SetActorLocation(FVector(1500,1500,98),false,nullptr,ETeleportType::TeleportPhysics);
        if (Case==0) P->StartCrouch();
        PlayerEvents.Reset(); EnemyEvents.Reset(); Start=W->GetTimeSeconds(); Stage=1;
    }
    void Verify()
    {
        const FString Label=FString::Printf(TEXT("Audio case %d: "),Case);
        if (Case<4)
        {
            const ETPCAudioCue Cue[]={ETPCAudioCue::FootCrouch,ETPCAudioCue::FootWalk,ETPCAudioCue::FootRun,ETPCAudioCue::FootSprint};
            TSet<FName> Feet; double Last=-100.; int32 N=0;
            for (const auto& E:PlayerEvents) if (E.Cue==Cue[Case])
            { Feet.Add(E.Foot); Test->TestTrue(Label+TEXT("no blend duplicate"),E.Time-Last>=.089); Last=E.Time; Volume[Case]+=E.Volume; ++N; }
            Steps[Case]=N; if (N) Volume[Case]/=N;
            Test->TestTrue(Label+TEXT("moving animation emits multiple footsteps"),N>=3);
            Test->TestEqual(Label+TEXT("both feet actually emit in PIE"),Feet.Num(),2);
            if (Case==3)
            {
                Test->TestTrue(TEXT("Cadence increases crouch < walk < run < sprint"),Steps[0]<Steps[1]&&Steps[1]<Steps[2]&&Steps[2]<Steps[3]);
                Test->TestTrue(TEXT("Loudness increases crouch < walk < run < sprint"),Volume[0]<Volume[1]&&Volume[1]<Volume[2]&&Volume[2]<Volume[3]);
            }
            Test->AddInfo(FString::Printf(TEXT("Locomotion %d: %d steps, mean volume %.3f"),Case,N,Volume[Case]));
        }
        else if (Case==4) Test->TestEqual(TEXT("Idle produces no footsteps"),PlayerEvents.Num(),0);
        else if (Case==5) Test->TestEqual(TEXT("Real jump produces one landing thud"),Count(PlayerEvents,ETPCAudioCue::Landing),1);
        else if (Case==6)
        {
            Test->TestEqual(TEXT("Actual empty attack window emits one whoosh"),Count(PlayerEvents,ETPCAudioCue::SwordLight),1);
            Test->TestEqual(TEXT("Empty swing has no victim flesh sound"),EnemyEvents.Num(),0);
        }
        else if (Case==7)
        {
            Test->TestEqual(TEXT("Connected sword swing emits once"),Count(PlayerEvents,ETPCAudioCue::SwordLight),1);
            Test->TestEqual(TEXT("Actual sweep emits one flesh impact, not once per frame"),
                Count(EnemyEvents,ETPCAudioCue::FleshLight)+Count(EnemyEvents,ETPCAudioCue::FleshHeavy),1);
        }
        else if (Case==8) Test->TestEqual(TEXT("Interrupted windup emits no swing"),PlayerEvents.Num(),0);
        else if (Case==9)
        {
            Test->TestEqual(TEXT("Damaged player cries out once"),Count(PlayerEvents,ETPCAudioCue::PlayerHurt),1);
            Test->TestEqual(TEXT("Damaged player receives impact sound"),Count(PlayerEvents,ETPCAudioCue::FleshLight),1);
        }
        else if (Case==10 || Case==11)
        {
            Test->TestEqual(TEXT("Defense emits only its distinct metallic cue"),PlayerEvents.Num(),1);
            Test->TestEqual(TEXT("Correct block/parry outcome cue"),Count(PlayerEvents,Case==10?ETPCAudioCue::Block:ETPCAudioCue::Parry),1);
        }
        else if (Case==12) Test->TestEqual(TEXT("Invulnerable miss is silent"),PlayerEvents.Num(),0);
        else if (Case==13) Test->TestEqual(TEXT("Heavy enemy wound uses meat and bone layer"),Count(EnemyEvents,ETPCAudioCue::FleshHeavy),1);
        else if (Case==14) Test->TestEqual(TEXT("Real uppercut selects heavy whoosh"),Count(PlayerEvents,ETPCAudioCue::SwordHeavy),1);
        Test->AddInfo(Label+TEXT("completed"));
    }
public:
    explicit FScenario(FAutomationTestBase* T):Test(T){}
    ~FScenario() override
    {
        FApp::SetUseFixedTimeStep(PreviousFixed); FApp::SetFixedDeltaTime(PreviousDelta);
        if (Player.IsValid()) Player->CharacterAudioComponent->OnAudioRequested.Remove(PlayerHandle);
        if (Enemy.IsValid()) Enemy->FindComponentByClass<UTPCCharacterAudioComponent>()->OnAudioRequested.Remove(EnemyHandle);
        FFileHelper::SaveStringToFile(Trace,*(FPaths::ProjectSavedDir()/TEXT("CharacterAudio/runtime_trace.csv")));
    }
    bool Update() override
    {
        if (FPlatformTime::Seconds()-WallStart>150.) { Test->AddError(FString::Printf(TEXT("Audio PIE timeout case %d stage %d"),Case,Stage)); return true; }
        UWorld* W=GEditor?GEditor->PlayWorld.Get():nullptr; if (!W || !W->HasBegunPlay()) return false;
        if (Stage==0)
        {
            Player=Cast<ATPCCharacter>(UGameplayStatics::GetPlayerCharacter(W,0)); if (!Player.IsValid()) return false;
            for (TActorIterator<AEnemyCharacter> It(W);It;++It) { if (It->GetController()) It->GetController()->Destroy(); It->Destroy(); }
            auto* P=Player.Get(); P->bEnableRootMotionTurn=false;
            // Headless PIE has no camera update; keep this test's listener on its pawn.
            if (auto* PC=Cast<APlayerController>(P->GetController()))
                PC->SetAudioListenerOverride(P->GetMesh(),FVector::ZeroVector,FRotator::ZeroRotator);
            P->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
            P->HealthComponent->MaxHealth=10000; P->HealthComponent->SetCurrentHealth(10000);
            auto* Floor=W->SpawnActor<AStaticMeshActor>(FVector(0,0,-60),FRotator::ZeroRotator);
            Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
            Floor->SetActorScale3D(FVector(150,150,1));
            auto* Class=LoadClass<AEnemyCharacter>(nullptr,TEXT("/Game/Third/Character/BP_EnemyCharacter.BP_EnemyCharacter_C"));
            FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Enemy=W->SpawnActor<AEnemyCharacter>(Class,FVector(1500,1500,98),FRotator(0,180,0),Params);
            if (!Enemy.IsValid()) { Test->AddError(TEXT("Real melee enemy fixture failed")); return true; }
            if (Enemy->GetController()) Enemy->GetController()->Destroy();
            Enemy->GetCharacterMovement()->DisableMovement();
            auto* EnemyHealth=Enemy->FindComponentByClass<UHealthComponent>(); EnemyHealth->MaxHealth=10000; EnemyHealth->SetCurrentHealth(10000);
            PlayerHandle=P->CharacterAudioComponent->OnAudioRequested.AddLambda([this](const auto& E){Record(E,false);});
            EnemyHandle=Enemy->FindComponentByClass<UTPCCharacterAudioComponent>()->OnAudioRequested.AddLambda([this](const auto& E){Record(E,true);});
            if (Render)
                if (auto Device=W->GetAudioDevice())
                {
                    Test->AddInfo(FString::Printf(TEXT("Mixer audit device=%u primary=%.3f muted=%d app=%.3f unfocused=%.3f"),
                        Device->DeviceID,Device->GetPrimaryVolume(),Device->IsAudioDeviceMuted(),FApp::GetVolumeMultiplier(),FApp::GetUnfocusedVolumeMultiplier()));
                    // Unattended editor windows are not focused. Solo only this disposable
                    // PIE device; never change the project's or desktop's saved mixer settings.
                    GEngine->GetAudioDeviceManager()->SetSoloDevice(Device->DeviceID);
                    Device->SetTransientPrimaryVolume(1.f);
                }
            // Real mixer recording must advance in wall-clock time with the audio device.
            FApp::SetUseFixedTimeStep(!Render);
            FApp::SetFixedDeltaTime(1./60);
            Reset(W);
        }
        auto* P=Player.Get(); const double Elapsed=W->GetTimeSeconds()-Start;
        if (Stage==1 && Elapsed>.8)
        {
            PlayerEvents.Reset(); EnemyEvents.Reset(); Stage=2; Start=W->GetTimeSeconds();
            if (Render) UAudioMixerBlueprintLibrary::StartRecordingOutput(W,6.f);
            if (Case==5) P->StartJump();
            if (Case==7) Enemy->SetActorLocation(P->GetActorLocation()+FVector(140,0,0),false,nullptr,ETeleportType::TeleportPhysics);
            if (Case==6 || Case==7 || Case==8) P->CombatComponent->TryAttack();
            if (Case==8) P->CombatComponent->CancelActiveAttack(0.f);
            if (Case>=9 && Case<=13)
            {
                FCombatHitSpec Spec; Spec.Damage=5.f; Spec.ImpactPoint=P->GetActorLocation();
                Enemy->SetActorLocation(P->GetActorLocation()+FVector(160,0,0),false,nullptr,ETeleportType::TeleportPhysics);
                if (Case==10 || Case==11) P->CombatComponent->StartBlock();
                if (Case==10) Spec.bCanBeParried=false;
                if (Case==12) P->HealthComponent->SetInvulnerableFor(.5f);
                if (Case==13) { Spec.PoiseDamage=40.f; Spec.ImpactPoint=Enemy->GetActorLocation(); Enemy->FindComponentByClass<UHealthComponent>()->ApplyCombatHit(Spec,P); }
                else P->HealthComponent->ApplyCombatHit(Spec,Enemy.Get());
            }
            if (Case==14) P->CombatComponent->TryUppercutAttack();
        }
        else if (Stage==2)
        {
            if (Case<4) P->AddMovementInput(FVector::ForwardVector);
            const double Duration=Case<4?4.5:Case==5?2.5:2.;
            if (Elapsed>=Duration)
            {
                Verify();
                if (Render) UAudioMixerBlueprintLibrary::StopRecordingOutput(W,EAudioRecordingExportType::WavFile,
                    FString::Printf(TEXT("case_%02d"),Case),FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("CharacterAudio")));
                Stage=3; Start=W->GetTimeSeconds(); P->GetCharacterMovement()->StopMovementImmediately();
            }
        }
        else if (Stage==3 && Elapsed>.4)
        {
            if (++Case==15) return true;
            Reset(W);
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPCAudioPIETest,"ThirdPerson.CharacterAudio.PIE.MovementAndCombat",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTPCAudioPIETest::RunTest(const FString&)
{
    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Third/Bosses/Countess/Maps/L_CountessBossTest"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(CharacterAudioPIE::FScenario(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
