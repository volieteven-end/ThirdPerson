#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Slate/SceneViewport.h"
#include "Engine/Blueprint.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/ShapeComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "DynamicRHI.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "../Character/TPCCharacter.h"
#include "../Components/CombatComponent.h"
#include "../Components/ActionComponent.h"
#include "../Components/EquipmentComponent.h"
#include "../Components/HealthComponent.h"
#include "../Components/StaminaComponent.h"
#include "../Weapons/WeaponActor.h"
#include "../Weapons/WeaponDefinition.h"
#include "../Weapons/WeaponVFXComponent.h"
#include "../Weapons/WeaponVFXProfile.h"
#include "../Weapons/ProjectilePoolSubsystem.h"
#include "../Boss/CountessBossCharacter.h"
#include "../Boss/BossActionComponent.h"
#include "../Boss/BossBloodWave.h"
#include "../Animation/AttackWindowNotifyState.h"
#include "../Animation/CombatNotifyContext.h"
#include "Animation/AnimMontage.h"
#include "BossReadabilityCapture.h"

struct FCombatVFXTestAccess
{
    static void PrepareBoss(UBossActionComponent& A, AActor* Target)
    { A.CancelAction(); A.Target = Target; A.CooldownUntil.Reset(); A.State = EBossState::Combat; A.Phase = 2; A.RecoilUntil = 0; A.bPhasePending = false; }
    static bool HasEffect(const UBossActionComponent& A, UParticleSystem* Template)
    { return A.Effects.ContainsByPredicate([Template](const auto& FX) { return IsValid(FX) && FX->Template == Template; }); }
    static float BossTime(const UBossActionComponent& A)
    { return A.ActiveMontage ? A.Boss->GetMesh()->GetAnimInstance()->Montage_GetPosition(A.ActiveMontage) : 0.f; }
    static int32 BossStage(const UBossActionComponent& A) { return A.StageIndex; }
    static bool Play(UCombatComponent& C, const UActionDefinition* D)
    { return C.StartDefinedAttack(D, EActiveCombatAttackType::Normal); }
    static bool Window(const UCombatComponent& C) { return C.bAttackWindowActive; }
    static bool DebugVisible(const UCombatComponent& C) { return C.bDrawHandTraceDebug; }
    static bool Blade(const UWeaponVFXComponent& V, FVector& Base, FVector& Tip) { return V.ReadBlade(Base, Tip); }
    static int32 MontageId(const UCombatComponent& C) { return C.ActiveAttackInstanceId; }
    static void Buff(UCombatComponent& C, float Seconds)
    { C.SwordBuffMultiplier = 1.2f; C.SwordBuffExpiresAt = C.GetWorld()->GetTimeSeconds() + Seconds; }
    static UNiagaraComponent* Trail(const UWeaponVFXComponent& V) { return V.Trail; }
    static UNiagaraComponent* Sword(const UWeaponVFXComponent& V) { return V.Sword; }
    static void Impact(AWeaponProjectile& P, AActor* Wall)
    { FHitResult Hit; Hit.ImpactPoint = P.GetActorLocation(); Hit.ImpactNormal = FVector::BackwardVector; P.HandleProjectileHit(nullptr, Wall, nullptr, FVector::ZeroVector, Hit); }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatVFXAssetTest, "ThirdPerson.VFX.AssetContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatVFXAssetTest::RunTest(const FString&)
{
    const auto* BP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Third/Weapon/BP_SwordWeapon.BP_SwordWeapon"));
    if (!TestNotNull(TEXT("Sword BP reopens"), BP)) return false;
    const auto* CDO = Cast<AWeaponActor>(BP->GeneratedClass->GetDefaultObject());
    const UWeaponVFXProfile* Profile = CDO && CDO->WeaponVFX ? CDO->WeaponVFX->Profile.Get() : nullptr;
    if (!TestNotNull(TEXT("Saved inherited VFX profile survives blueprint reload"), Profile)) return false;
    TestFalse(TEXT("Player automatic VFX disabled; authored notifies own effects"), CDO->WeaponVFX->bEnableAutomaticEffects);
    for (const auto* System : {Profile->BasicTrail.Get(), Profile->IceTrail.Get(), Profile->ElectricTrail.Get(), Profile->BuffSword.Get()})
    {
        if (!TestNotNull(TEXT("Required Niagara system exists"), System)) continue;
        TestTrue(TEXT("Tuning copy belongs to this project"), System->GetPathName().StartsWith(TEXT("/Game/Third/Effects/Combat/")));
        TestTrue(TEXT("Authored sword length remains a float"), System->GetExposedParameters().IndexOf(
            FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.SwordLength"))) != INDEX_NONE);
    }
    const auto* D = LoadObject<UBossDefinition>(nullptr, TEXT("/Game/Third/Bosses/Countess/DA_CountessBoss.DA_CountessBoss"));
    if (!TestNotNull(TEXT("Countess definition reopens"), D)) return false;
    FString Error; TestTrue(TEXT("Original readable attack contracts preserved"), D->Validate(Error));
    for (const UObject* FX : {D->SiphonCastEffect.Get(), D->SiphonHitEffect.Get(), D->RushSlashEffect.Get(),
        D->FeastSlashEffect.Get(), D->WaveFlightEffect.Get(), D->WaveImpactEffect.Get()})
        TestTrue(TEXT("Countess accents use project copies"), FX && FX->GetPathName().StartsWith(TEXT("/Game/Third/Effects/Combat/")));
    TestEqual(TEXT("Six moves retained"), D->Actions.Num(), 6);
    for (const auto* System : {D->WaveFlightEffect.Get(), D->WaveImpactEffect.Get()})
        if (System) for (const UParticleEmitter* E : System->Emitters)
            if (E) for (const UParticleLODLevel* LOD : E->LODLevels)
                if (LOD && LOD->RequiredModule)
                    TestTrue(TEXT("Flight emission lasts until pool release; detached impact is finite"),
                        System == D->WaveFlightEffect ? LOD->RequiredModule->EmitterLoops == 0 : LOD->RequiredModule->EmitterLoops > 0);
    return true;
}

namespace CombatVFXPIE
{
struct FSaveScope
{
    FString Original = FCommandLine::Get();
    FString Slot = TEXT("CombatVFXAutomation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    FSaveScope() { FCommandLine::Set(*(TEXT("-TPCSaveSlot=") + Slot + TEXT(" ") + Original)); }
    ~FSaveScope() { UGameplayStatics::DeleteGameInSlot(Slot, 0); FCommandLine::Set(*Original); }
};
class FReleaseSaveScope : public IAutomationLatentCommand
{
    TSharedPtr<FSaveScope> Scope;
public:
    explicit FReleaseSaveScope(TSharedPtr<FSaveScope> InScope) : Scope(InScope) {}
    bool Update() override { return !GEditor || !GEditor->PlayWorld; }
};

class FScenario : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TSharedPtr<FSaveScope> Save;
    TWeakObjectPtr<ATPCCharacter> Player;
    TWeakObjectPtr<ACameraActor> Camera;
    const int32 Rates[3] = {30, 60, 120};
    int32 Rate = 0, Case = 0, Stage = 0, Pass = 1, CaptureCount = 0;
    bool SawTrail = false, SawBuff = false, SawAuthoredTrail = false, DidInterrupt = false;
    double Start = 0, ActiveAt = -1, WallStart = FPlatformTime::Seconds();
    bool PreviousFixed = FApp::UseFixedTimeStep();
    double PreviousDelta = FApp::GetFixedDeltaTime();
    int32 PreviousVFX = IConsoleManager::Get().FindConsoleVariable(TEXT("tpc.CombatVFX"))->GetInt();
    FString Trace = TEXT("enabled,fps,case,time,gpu_ms,trail,buff\n");
    FString Label(const TCHAR* What) const { return FString::Printf(TEXT("VFX enabled=%d fps=%d case=%d: %s"), Pass, Rates[Rate], Case, What); }
    bool Capture() const { return FParse::Param(FCommandLine::Get(), TEXT("CombatVFXCapture")); }
    void Reset(ATPCCharacter* P)
    {
        P->CombatComponent->CancelActiveAttack(); P->CombatComponent->CancelGuard(); P->CombatComponent->ClearSwordBuff();
        P->CombatComponent->SetCombatEnabled(true); P->ActionComponent->SetInputSuppressed(false);
        P->ActionComponent->ClearInputBuffers(); P->EquipmentComponent->SetWeaponDrawn(true);
        P->GetMesh()->GetAnimInstance()->StopAllMontages(0.f);
        P->GetCharacterMovement()->SetMovementMode(MOVE_Walking); P->GetCharacterMovement()->StopMovementImmediately();
        P->StaminaComponent->SetCurrentStamina(100.f);
        P->SetActorLocationAndRotation(FVector(0, -1000, 98), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
    }
    void Next(UWorld* W)
    {
        Test->AddInfo(Label(TEXT("completed")));
        Reset(Player.Get());
        ++Case; Stage = 1;
        if (Case == 15)
        {
            Case = 0;
            if (Rate == 1 && Pass == 0) Pass = 1;
            else { ++Rate; Pass = Rate == 1 && Capture() ? 0 : 1; }
        }
        Start = W->GetTimeSeconds();
    }
public:
    FScenario(FAutomationTestBase* T, TSharedPtr<FSaveScope> S) : Test(T), Save(S) {}
    ~FScenario() override
    {
        FApp::SetUseFixedTimeStep(PreviousFixed); FApp::SetFixedDeltaTime(PreviousDelta);
        IConsoleManager::Get().FindConsoleVariable(TEXT("tpc.CombatVFX"))->Set(PreviousVFX);
        const FString Dir = FPaths::ProjectSavedDir() / TEXT("CombatVFX");
        IFileManager::Get().MakeDirectory(*Dir, true);
        FFileHelper::SaveStringToFile(Trace, *(Dir / TEXT("frames.csv")));
    }
    bool Update() override
    {
        if (Rate == 3) return true;
        if (FPlatformTime::Seconds() - WallStart > 240) { Test->AddError(TEXT("VFX PIE timed out")); return true; }
        UWorld* W = GEditor ? GEditor->PlayWorld.Get() : nullptr;
        if (!W || !W->HasBegunPlay() || W->GetTimeSeconds() < 1.f) return false;
        if (Stage == 0)
        {
            Player = Cast<ATPCCharacter>(UGameplayStatics::GetPlayerCharacter(W, 0));
            if (!Player.IsValid()) return false;
            for (TActorIterator<ACountessBossCharacter> It(W); It; ++It) It->Destroy();
            auto* P = Player.Get();
            Test->TestFalse(TEXT("Existing BP debug override is hidden on equip"), FCombatVFXTestAccess::DebugVisible(*P->CombatComponent));
            P->HealthComponent->MaxHealth = 10000.f; P->HealthComponent->SetCurrentHealth(10000.f);
            P->EquipmentComponent->EquipWeapon(LoadObject<UWeaponDefinition>(nullptr, TEXT("/Game/Third/DataAsset/DA_TestSword.DA_TestSword")));
            if (!Test->TestNotNull(TEXT("Saved sword equipped"), P->EquipmentComponent->GetEquippedWeaponActor())) return true;
            if (!Test->TestNotNull(TEXT("Runtime profile retained"), P->EquipmentComponent->GetEquippedWeaponActor()->WeaponVFX->Profile.Get())) return true;
            W->GetWorldSettings()->MinUndilatedFrameTime = 0; W->GetWorldSettings()->MaxUndilatedFrameTime = 1;
            FApp::SetUseFixedTimeStep(true);
            const FVector Eye(320, -1390, 270);
            Camera = W->SpawnActor<ACameraActor>(Eye, (FVector(40, -1000, 110) - Eye).Rotation());
            Camera->GetCameraComponent()->SetFieldOfView(55.f);
            Cast<APlayerController>(P->GetController())->SetViewTarget(Camera.Get());
            IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir() / TEXT("CombatVFX")), true);
            if (Capture() && W->GetGameViewport() && W->GetGameViewport()->GetGameViewport())
                W->GetGameViewport()->GetGameViewport()->SetFixedViewportSize(1920, 1080);
            Stage = 1;
        }
        auto* P = Player.Get();
        auto* C = P->CombatComponent.Get();
        auto* Weapon = P->EquipmentComponent->GetEquippedWeaponActor();
        auto* VFX = Weapon->WeaponVFX.Get();
        if (Camera.IsValid())
        {
            const FVector Eye = P->GetActorLocation() + FVector(450, -600, 270);
            Camera->SetActorLocationAndRotation(Eye, (P->GetActorLocation() + FVector(20, 0, 45) - Eye).Rotation());
        }
        if (Stage == 1)
        {
            Reset(P); FApp::SetFixedDeltaTime(1. / Rates[Rate]);
            IConsoleManager::Get().FindConsoleVariable(TEXT("tpc.CombatVFX"))->Set(Pass);
            Start = W->GetTimeSeconds(); ActiveAt = -1; SawTrail = SawBuff = SawAuthoredTrail = DidInterrupt = false; CaptureCount = 0;
            Stage = 2; return false;
        }
        double T = W->GetTimeSeconds() - Start;
        if (Stage == 2)
        {
            if (T < .2) return false;
            const auto* Set = C->GetActionSet();
            if (!Test->TestNotNull(TEXT("Saved action set"), Set)) return true;
            if (Case < 8 || Case >= 13)
            {
                const UActionDefinition* Moves[] = {Set->GroundCombo[0], Set->GroundCombo.Last(), Set->AirCombo[0],
                    Set->AirCombo.Last(), Set->Rising, Set->Dive, Set->SprintAttack, Set->ParryCounter};
                // Real saved montages/notifies; movement regressions are covered by the separate sword/dive suites.
                Test->TestTrue(Label(TEXT("authored action starts")), FCombatVFXTestAccess::Play(*C, Case >= 13 ? Set->GroundCombo[Case - 12].Get() : Moves[Case]));
            }
            else if (Case < 11 || Case == 12)
            {
                FCombatVFXTestAccess::Buff(*C, Case == 12 ? 10.f : .55f); VFX->RefreshState();
                Weapon->SetAttackEffectActive(true);
            }
            else
            {
                auto* Pool = W->GetSubsystem<UProjectilePoolSubsystem>(); FCombatHitSpec Hit; Hit.Damage = 20.f;
                auto* Wave = Pool->AcquireWithHitSpec(ABossBloodWave::StaticClass(), FTransform(FVector(500, -1000, 90)), P, P, Hit, 900.f);
                if (!Test->TestNotNull(Label(TEXT("blood wave acquired")), Wave)) return true;
                auto* Flight = Wave->FindComponentByClass<UParticleSystemComponent>();
                Test->TestTrue(Label(TEXT("flight activates only with cosmetic switch")), Flight && Flight->IsActive() == (Pass == 1 && FApp::CanEverRender()));
                auto* Wall = W->SpawnActor<AActor>();
                FCombatVFXTestAccess::Impact(*Wave, Wall);
                Test->TestFalse(Label(TEXT("impact returns wave immediately")), Wave->IsProjectileActive());
                Test->TestFalse(Label(TEXT("pooled flight is stopped")), Flight->IsActive());
                const auto* Same = Pool->AcquireWithHitSpec(ABossBloodWave::StaticClass(), FTransform(FVector(600, -1000, 90)), P, P, Hit, 900.f);
                Test->TestTrue(Label(TEXT("reacquisition reuses the same wave")), Same == Wave);
                Test->TestTrue(Label(TEXT("reacquired flight restarts")), Flight->IsActive() == (Pass == 1 && FApp::CanEverRender()));
                Pool->Release(Wave); Wall->Destroy(); Next(W); return false;
            }
            Start = W->GetTimeSeconds(); Stage = 3; return false;
        }
        const bool Active = FCombatVFXTestAccess::Window(*C);
        SawTrail |= VFX->IsTrailActive(); SawBuff |= VFX->IsBuffActive();
        TArray<USceneComponent*> Children; P->GetMesh()->GetChildrenComponents(true, Children);
        for (auto* Child : Children)
            if (const auto* FX = Cast<UParticleSystemComponent>(Child))
                SawAuthoredTrail |= FX->IsActive() && FX->IsVisible() && FX->Template && FX->Template->GetName() == TEXT("P_Trail");
        if (Case == 12)
        {
            if (!DidInterrupt && T > .15)
            {
                DidInterrupt = true;
                Test->TestFalse(Label(TEXT("gameplay buff does not start automatic sword FX")), SawBuff || SawTrail);
                P->HealthComponent->ApplyDamage(20000.f);
                Test->TestEqual(Label(TEXT("real lethal damage enters death")), P->ActionComponent->GetActionState(), ETPCActionState::Dead);
            }
            if (DidInterrupt && T > .3)
            {
                Test->TestFalse(Label(TEXT("death clears trail without waiting for buff expiry")), VFX->IsTrailActive());
                Test->TestFalse(Label(TEXT("death clears sword enchantment")), VFX->IsBuffActive());
                P->RestartAfterDeath(); // The real presentation gate and respawn handoff remain in force.
                auto* Reborn = Cast<ATPCCharacter>(UGameplayStatics::GetPlayerCharacter(W, 0));
                if (Reborn && Reborn != P)
                {
                    Test->TestFalse(Label(TEXT("old weapon is destroyed on respawn")), IsValid(Weapon));
                    auto* NewWeapon = Reborn->EquipmentComponent->GetEquippedWeaponActor();
                    if (!Test->TestNotNull(Label(TEXT("respawn restores equipped sword")), NewWeapon)) return true;
                    Test->TestFalse(Label(TEXT("new sword has no inherited attack trail")), NewWeapon->WeaponVFX->IsTrailActive());
                    Test->TestFalse(Label(TEXT("new sword has no inherited buff")), NewWeapon->WeaponVFX->IsBuffActive());
                    Reset(Reborn);
                    NewWeapon->SetAttackEffectActive(true);
                    Test->TestFalse(Label(TEXT("respawn does not restore automatic sword FX")), NewWeapon->WeaponVFX->IsTrailActive());
                    NewWeapon->SetAttackEffectActive(false);
                    Test->TestFalse(Label(TEXT("respawn keeps attack debug hidden")), FCombatVFXTestAccess::DebugVisible(*Reborn->CombatComponent));
                    Cast<APlayerController>(Reborn->GetController())->SetViewTarget(Camera.Get());
                    Player = Reborn; Next(W); return false;
                }
            }
            if (T > 12) { Test->AddError(Label(TEXT("death presentation never allowed respawn"))); return true; }
            return false;
        }
        if (Active && ActiveAt < 0) ActiveAt = T;
        if (Capture() && Rate == 1 && Pass == 1 && Case >= 13 && CaptureCount < 4 && T > .22 + CaptureCount * .32)
            Test->TestTrue(TEXT("Handoff PIE capture"), CaptureBossReadabilityFrame(W, FPaths::ProjectSavedDir() /
                FString::Printf(TEXT("CombatVFX/handoff_%d_%d.png"), Case - 11, CaptureCount++)));
        if (Capture() && Rate == 1 && Case < 8)
        {
            const double GPU = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
            Trace += FString::Printf(TEXT("%d,%d,%d,%.4f,%.4f,%d,%d\n"), Pass, Rates[Rate], Case, T, GPU, VFX->IsTrailActive(), VFX->IsBuffActive());
            if (Active && ActiveAt >= 0 && T - ActiveAt > .07 + .07 * CaptureCount && CaptureCount < 2)
                Test->TestTrue(TEXT("PIE viewport capture"), CaptureBossReadabilityFrame(W, FPaths::ProjectSavedDir() /
                    FString::Printf(TEXT("CombatVFX/sword_%d_%d_%d.png"), Pass, Case, CaptureCount++)));
        }
        if ((Case < 8 || Case >= 13) && T > 1.8)
        {
            Test->TestFalse(Label(TEXT("C++ attack windows do not create duplicate trail FX")), SawTrail);
            Test->TestNull(Label(TEXT("No automatic Niagara component was allocated")), FCombatVFXTestAccess::Trail(*VFX));
            if (FApp::CanEverRender() && (Case == 0 || Case >= 13))
                Test->TestTrue(Label(TEXT("Animation-authored P_Trail remains visible")), SawAuthoredTrail);
            TInlineComponentArray<UShapeComponent*> Shapes(Weapon);
            for (auto* Shape : Shapes) Test->TestFalse(Label(TEXT("Weapon helper shape remains invisible")), Shape->IsVisible());
            C->CancelActiveAttack();
            Test->TestFalse(Label(TEXT("cancel stops all attack emission")), VFX->IsTrailActive());
            Next(W); return false;
        }
        if (Case >= 8 && Case < 11)
        {
            if (!DidInterrupt && T > .15)
            {
                DidInterrupt = true;
                if (Case == 8) P->SetActorLocation(P->GetActorLocation() + FVector(500, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
                if (Case == 9) P->EquipmentComponent->SetWeaponDrawn(false);
                if (Case == 10) C->SetCombatEnabled(false);
            }
            if (T > .8)
            {
                Test->TestFalse(Label(TEXT("buff does not enable automatic sword enchantment")), SawBuff);
                Test->TestFalse(Label(TEXT("buff expires / sheath / disabled combat stop sword")), VFX->IsBuffActive());
                Weapon->SetAttackEffectActive(false);
                Test->TestFalse(Label(TEXT("no surviving attack trail")), VFX->IsTrailActive());
                Next(W);
            }
        }
        return false;
    }
};

class FBossScenario : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TSharedPtr<FSaveScope> Save;
    TWeakObjectPtr<ATPCCharacter> Player;
    TWeakObjectPtr<ACountessBossCharacter> Boss;
    int32 Stage = 0, Case = 0, Pass = 0, Captured = 0;
    double Start = 0, WallStart = FPlatformTime::Seconds();
    bool SawAccent = false;
    bool PreviousFixed = FApp::UseFixedTimeStep(); double PreviousDelta = FApp::GetFixedDeltaTime();
    int32 PreviousVFX = IConsoleManager::Get().FindConsoleVariable(TEXT("tpc.CombatVFX"))->GetInt();
    FString Trace = TEXT("enabled,case,time,gpu_ms\n");
    const EBossAction Cases[6] = {EBossAction::Combo, EBossAction::DelayedSlash, EBossAction::Siphon,
        EBossAction::ShadowRush, EBossAction::BloodWave, EBossAction::BloodFeast};
    bool Capture() const { return FParse::Param(FCommandLine::Get(), TEXT("CombatVFXCapture")); }
public:
    FBossScenario(FAutomationTestBase* T, TSharedPtr<FSaveScope> S) : Test(T), Save(S) {}
    ~FBossScenario() override
    {
        FApp::SetUseFixedTimeStep(PreviousFixed); FApp::SetFixedDeltaTime(PreviousDelta);
        IConsoleManager::Get().FindConsoleVariable(TEXT("tpc.CombatVFX"))->Set(PreviousVFX);
        IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir() / TEXT("CombatVFX")), true);
        FFileHelper::SaveStringToFile(Trace, *(FPaths::ProjectSavedDir() / TEXT("CombatVFX/boss_frames.csv")));
    }
    bool Update() override
    {
        if (Pass == 2) return true;
        if (FPlatformTime::Seconds() - WallStart > 240) { Test->AddError(TEXT("Boss VFX timed out")); return true; }
        UWorld* W = GEditor ? GEditor->PlayWorld.Get() : nullptr;
        if (!W || !W->HasBegunPlay() || W->GetTimeSeconds() < 1.f) return false;
        if (Stage == 0)
        {
            Player = Cast<ATPCCharacter>(UGameplayStatics::GetPlayerCharacter(W, 0));
            for (TActorIterator<ACountessBossCharacter> It(W); It; ++It) { Boss = *It; break; }
            if (!Player.IsValid() || !Boss.IsValid()) return false;
            if (auto* AI = Cast<AAIController>(Boss->GetController())) { AI->GetBrainComponent()->StopLogic(TEXT("VFX fixture")); AI->StopMovement(); }
            Boss->BossActions->Definition = DuplicateObject<UBossDefinition>(Boss->BossActions->Definition, Boss.Get());
            for (auto& A : Boss->BossActions->Definition->Actions) A.PhaseOneWeight = A.PhaseTwoWeight = 0;
            Player->HealthComponent->MaxHealth = 10000.f; Player->HealthComponent->SetCurrentHealth(10000.f);
            Player->CombatComponent->SetCombatEnabled(false);
            W->GetWorldSettings()->MinUndilatedFrameTime = 0; W->GetWorldSettings()->MaxUndilatedFrameTime = 1;
            FApp::SetUseFixedTimeStep(true); FApp::SetFixedDeltaTime(1. / 60);
            const FVector Eye(650, -740, 360);
            auto* Camera = W->SpawnActor<ACameraActor>(Eye, (FVector(230, 0, 85) - Eye).Rotation());
            Camera->GetCameraComponent()->SetFieldOfView(55.f);
            Cast<APlayerController>(Player->GetController())->SetViewTarget(Camera);
            if (Capture() && W->GetGameViewport() && W->GetGameViewport()->GetGameViewport())
                W->GetGameViewport()->GetGameViewport()->SetFixedViewportSize(1920, 1080);
            if (!Capture()) Pass = 1;
            Stage = 1;
        }
        auto* B = Boss.Get(); auto* A = B->BossActions.Get();
        if (Stage == 1)
        {
            FCombatVFXTestAccess::PrepareBoss(*A, Player.Get());
            B->GetCharacterMovement()->StopMovementImmediately();
            B->SetActorLocationAndRotation(FVector(0, 0, 98), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
            const float Distance = Case == 3 ? 450.f : Case == 4 ? 650.f : 180.f;
            Player->SetActorLocationAndRotation(FVector(Distance, 0, 98), FRotator(0, 180, 0), false, nullptr, ETeleportType::TeleportPhysics);
            Player->GetCharacterMovement()->StopMovementImmediately();
            Player->HealthComponent->SetCurrentHealth(10000.f);
            IConsoleManager::Get().FindConsoleVariable(TEXT("tpc.CombatVFX"))->Set(Pass);
            Start = W->GetTimeSeconds(); Captured = 0; SawAccent = false; Stage = 2; return false;
        }
        if (Stage == 2)
        {
            if (W->GetTimeSeconds() - Start < .3) return false;
            for (auto& Move : A->Definition->Actions) Move.PhaseOneWeight = Move.PhaseTwoWeight = Move.Id == Cases[Case] ? 100.f : 0.f;
            Test->TestTrue(FString::Printf(TEXT("Boss VFX action %d starts"), Case), A->TryStartAction(Cases[Case]));
            Start = W->GetTimeSeconds(); Stage = 3; return false;
        }
        const float Time = FCombatVFXTestAccess::BossTime(*A);
        const double T = W->GetTimeSeconds() - Start;
        const auto* D = A->GetDefinition();
        UParticleSystem* Accent = Case == 2 ? D->SiphonCastEffect : Case == 3 ? D->RushSlashEffect : Case == 5 ? D->FeastSlashEffect : nullptr;
        if (Accent) SawAccent |= FCombatVFXTestAccess::HasEffect(*A, Accent);
        if (Capture())
        {
            Trace += FString::Printf(TEXT("%d,%d,%.4f,%.4f\n"), Pass, Case, T, FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
            float HitStart, HitEnd; D->FindAction(Cases[Case])->Stages[0].ReadHitWindow(HitStart, HitEnd);
            const float At[] = {FMath::Max(.1f, HitStart - .2f), HitStart + .08f, HitStart + (Case == 4 ? .4f : .2f)};
            if (Captured < 3 && FCombatVFXTestAccess::BossStage(*A) == 0 && Time >= At[Captured])
                Test->TestTrue(TEXT("Boss same-camera screenshot"), CaptureBossReadabilityFrame(W, FPaths::ProjectSavedDir() /
                    FString::Printf(TEXT("CombatVFX/boss_%d_%d_%d.png"), Pass, Case, Captured++)));
        }
        if (T > .2 && (!A->IsActionActive() || T > 8))
        {
            Test->TestFalse(TEXT("Boss action terminates"), A->IsActionActive());
            if (Accent) Test->TestEqual(TEXT("Only enabled accent was observed"), SawAccent, Pass == 1);
            A->CancelAction();
            for (auto* FX : {D->SiphonCastEffect.Get(), D->RushSlashEffect.Get(), D->FeastSlashEffect.Get()})
                Test->TestFalse(TEXT("No accent survives cancel/reset"), FCombatVFXTestAccess::HasEffect(*A, FX));
            if (++Case == 6) { Case = 0; ++Pass; }
            Stage = 1;
        }
        return false;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatVFXPIETest, "ThirdPerson.VFX.PIE.LifecycleAt30_60_120",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatVFXPIETest::RunTest(const FString&)
{
    auto Scope = MakeShared<CombatVFXPIE::FSaveScope>();
    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Third/Bosses/Countess/Maps/L_CountessBossTest"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(CombatVFXPIE::FScenario(this, Scope));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    ADD_LATENT_AUTOMATION_COMMAND(CombatVFXPIE::FReleaseSaveScope(Scope));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossVFXPIETest, "ThirdPerson.VFX.PIE.BossAccentWindows",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBossVFXPIETest::RunTest(const FString&)
{
    auto Scope = MakeShared<CombatVFXPIE::FSaveScope>();
    FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/Third/Bosses/Countess/Maps/L_CountessBossTest"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(CombatVFXPIE::FBossScenario(this, Scope));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    ADD_LATENT_AUTOMATION_COMMAND(CombatVFXPIE::FReleaseSaveScope(Scope));
    return true;
}
#endif
