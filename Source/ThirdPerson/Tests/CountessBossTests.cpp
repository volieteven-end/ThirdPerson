#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "../Boss/CountessBossCharacter.h"
#include "../Boss/BossActionComponent.h"
#include "../Boss/BossBloodWave.h"
#include "../Boss/CountessBossAIController.h"
#include "../Boss/BossBehaviorNodes.h"
#include "../Boss/CountessBossAnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "../Character/TPCCharacter.h"
#include "../Components/HealthComponent.h"
#include "../Components/CombatComponent.h"
#include "../Components/StaminaComponent.h"
#include "../Weapons/ProjectilePoolSubsystem.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/BTCompositeNode.h"

struct FCountessBossTestAccess
{
 static void Initialize(UBossActionComponent& A,ACountessBossCharacter* B,AActor* Target)
 {
  A.Boss=B; A.Health=B->FindComponentByClass<UHealthComponent>(); A.bInitialized=true; A.Target=Target;
  A.HomeLocation=B->GetActorLocation(); A.HomeRotation=B->GetActorRotation(); A.State=EBossState::Combat; A.Phase=1; A.Poise=100;
  A.Random.Initialize(137); A.Health->OnCombatHitResolved.AddUObject(&A,&UBossActionComponent::OnResolvedHit);
 }
 static void Arm(UBossActionComponent& A,EBossAction Id,int32 Index=0)
 { A.CurrentAction=Id; A.StageIndex=Index; A.State=EBossState::Action; A.Step=UBossActionComponent::EActionStep::Playing; ++A.ActionSerial; A.WindowHits.Reset(); A.bOneShotFired=false; }
 static void Hit(UBossActionComponent& A,AActor* V) { A.ApplyHit(V,V->GetActorLocation()); }
 static void Sample(UBossActionComponent& A,float Old,float New) { A.SampleDamage(Old,New); }
 static void PoiseHit(UBossActionComponent& A,float Value) { A.ReducePoise(Value); }
 static void ExpireState(UBossActionComponent& A,float Seconds) { A.StateStarted=A.Now()-Seconds; }
 static void Tick(UBossActionComponent& A,float Delta=.016f) { A.TickComponent(Delta,LEVELTICK_All,nullptr); }
 static void Cooldown(UBossActionComponent& A,EBossAction Id,float Time) { A.CooldownUntil.Add(Id,A.Now()+Time); }
 static void StaleEnd(UBossActionComponent& A) { A.OnMontageEnded(A.ActiveMontage,true,A.ActionSerial-1); }
 static void Montage(UBossActionComponent& A,UAnimMontage* M) { A.ActiveMontage=M; A.MontageInstanceId=25; }
 static void OwnMotion(UBossActionComponent& A,uint16 Id) { A.MoveSourceId=Id; }
 static void Target(UBossActionComponent& A,AActor* Target) { A.Target=Target; }
 static void StartStage(UBossActionComponent& A) { A.StartStage(); }
 static FCombatHitSpec PlayerHit(UCombatComponent& C,EActiveCombatAttackType Type)
 { auto Previous=C.ActiveAttackType; C.ActiveAttackType=Type; auto Spec=C.MakeCurrentHitSpec(FVector::ZeroVector); C.ActiveAttackType=Previous; return Spec; }
};
namespace
{
struct FBossTestWorld
{
 UWorld* W; ACountessBossCharacter* B; ATPCCharacter* P; UBossActionComponent* A; UHealthComponent* H; UHealthComponent* PH;
 FBossTestWorld()
 {
  W=UWorld::CreateWorld(EWorldType::Game,false); GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
  FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  B=W->SpawnActor<ACountessBossCharacter>(ACountessBossCharacter::StaticClass(),FVector(100,0,100),FRotator(0,180,0),Spawn);
  P=W->SpawnActor<ATPCCharacter>(ATPCCharacter::StaticClass(),FVector(0,0,100),FRotator::ZeroRotator,Spawn);
  A=B->BossActions; H=B->FindComponentByClass<UHealthComponent>(); PH=P->FindComponentByClass<UHealthComponent>();
  FCountessBossTestAccess::Initialize(*A,B,P);
 }
 ~FBossTestWorld()
 {
  A->CancelAction(); H->OnCombatHitResolved.RemoveAll(A);
  W->DestroyWorld(false); GEngine->DestroyWorldContext(W); if (W->IsRooted()) W->RemoveFromRoot();
 }
};
constexpr auto BossTestFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossAssetContractTest,"ThirdPerson.Boss.AssetContract",BossTestFlags)
bool FBossAssetContractTest::RunTest(const FString&)
{
 const auto* D=GetDefault<UBossDefinition>(); FString Error;
 TestTrue(*FString::Printf(TEXT("Six-action source asset contract: %s"),*Error),D->Validate(Error));
 if (!Error.IsEmpty()) AddError(Error);
 if (auto* Saved=LoadObject<UBossDefinition>(nullptr,TEXT("/Game/Third/Bosses/Countess/DA_CountessBoss.DA_CountessBoss")))
 {
  Error.Reset(); TestTrue(TEXT("Authored definition validates after disk reload"),Saved->Validate(Error)); if (!Error.IsEmpty()) AddError(Error);
  TestTrue(TEXT("Saved behavior tree retains its context service"),Saved->BehaviorTree && Saved->BehaviorTree->RootNode && Saved->BehaviorTree->RootNode->Services.Num()==1);
  if (Saved->BehaviorTree && TestNotNull(TEXT("Saved behavior tree retains its blackboard"),Saved->BehaviorTree->BlackboardAsset.Get()))
   for (FName Key:{FName(TEXT("TargetActor")),FName(TEXT("HomeLocation")),FName(TEXT("bHasLineOfSight")),FName(TEXT("bEncounterActive")),FName(TEXT("BossPhase"))})
    TestTrue(*FString::Printf(TEXT("Saved blackboard key %s exists"),*Key.ToString()),Saved->BehaviorTree->BlackboardAsset->GetKeyID(Key)!=FBlackboard::InvalidKey);
 }
 TestEqual(TEXT("Combo has three available stages"),D->FindAction(EBossAction::Combo)->Stages.Num(),3);
 TestEqual(TEXT("Wave cannot be parried"),D->FindAction(EBossAction::BloodWave)->Stages[0].bCanBeParried,false);
 TestEqual(TEXT("Feast cannot be blocked"),D->FindAction(EBossAction::BloodFeast)->Stages[0].bCanBeBlocked,false);
 TestEqual(TEXT("Damage separate from warning radius"),D->FindAction(EBossAction::BloodFeast)->Stages[0].Radius,300.f);
 auto* Tree=ACountessBossAIController::CreateDefaultBossTree(GetTransientPackage());
 TestEqual(TEXT("Eight ordered state branches"),Tree->RootNode->Children.Num(),8);
 TestTrue(TEXT("Target key exists"),Tree->BlackboardAsset->GetKeyID(TEXT("TargetActor"))!=FBlackboard::InvalidKey);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossDefenseTest,"ThirdPerson.Boss.DefenseOutcomes",BossTestFlags)
bool FBossDefenseTest::RunTest(const FString&)
{
 FBossTestWorld T; auto* Combat=T.P->FindComponentByClass<UCombatComponent>(); auto* SP=T.P->FindComponentByClass<UStaminaComponent>();
 SP->SetCurrentStamina(SP->GetMaxStamina()); // This unit world deliberately does not dispatch actor BeginPlay.
 Combat->StartBlock(); FCountessBossTestAccess::Arm(*T.A,EBossAction::Combo);
 FCombatHitSpec Spec; Spec.Damage=20;
 auto Result=T.PH->ApplyCombatHit(Spec,T.B);
 TestTrue(TEXT("Physical strike parried"),Result.bParried); TestEqual(TEXT("Parry zero HP"),Result.ActualDamage,0.f);
 TestFalse(TEXT("Parry cancels entire boss action"),T.A->IsActionActive()); TestEqual(TEXT("Parry costs forty poise"),T.A->Poise,60.f);
 Combat->StopBlock(); Combat->StartBlock(); Spec.bCanBeParried=false;
 const float Stamina=SP->GetCurrentStamina(); Result=T.PH->ApplyCombatHit(Spec,T.B);
 TestTrue(TEXT("Blood spell during parry window uses ordinary block"),Result.bBlocked); TestFalse(TEXT("Spell does not parry"),Result.bParried);
 TestEqual(TEXT("Blocked damage is quarter"),Result.ActualDamage,5.f); TestTrue(TEXT("Block consumes stamina"),SP->GetCurrentStamina()<Stamina);
 Spec.bCanBeBlocked=false; Result=T.PH->ApplyCombatHit(Spec,T.B);
 TestEqual(TEXT("Feast ignores held block"),Result.ActualDamage,20.f); TestFalse(TEXT("Feast reaction not block reaction"),Result.bBlocked);
 T.PH->SetInvulnerableFor(1); Result=T.PH->ApplyCombatHit(Spec,T.B);
 TestEqual(TEXT("Dodge immunity still beats unblockable"),Result.Outcome,ECombatHitOutcome::Ignored);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossPoisePhaseTest,"ThirdPerson.Boss.PoisePhaseOrdering",BossTestFlags)
bool FBossPoisePhaseTest::RunTest(const FString&)
{
 FBossTestWorld T; T.H->SetCurrentHealth(751); T.A->Poise=10;
 FCombatHitSpec Spec; Spec.Damage=5; Spec.PoiseDamage=30;
 T.H->ApplyCombatHit(Spec,T.P);
 TestTrue(TEXT("Threshold marks pending phase"),T.A->bPhasePending); TestEqual(TEXT("Same hit breaks before phase cast"),T.A->State,EBossState::PoiseBroken);
 FCountessBossTestAccess::ExpireState(*T.A,1.f); T.H->ApplyCombatHit(Spec,T.P);
 TestTrue(TEXT("Additional damage does not extend break"),T.A->GetStateElapsed()>=1.f);
 FCountessBossTestAccess::ExpireState(*T.A,2.41f); FCountessBossTestAccess::Tick(*T.A);
 TestEqual(TEXT("Phase only starts after full break"),T.A->State,EBossState::PhaseTransition);
 const float Before=T.H->CurrentHealth; T.H->ApplyCombatHit(Spec,T.P); TestEqual(TEXT("Phase cast immune"),T.H->CurrentHealth,Before);
 FCountessBossTestAccess::ExpireState(*T.A,1.51f); FCountessBossTestAccess::Tick(*T.A);
 TestEqual(TEXT("Second phase"),T.A->Phase,2); TestEqual(TEXT("Phase two poise"),T.A->Poise,120.f);
 T.H->Heal(1000); T.A->ConsiderPhase(T.H->CurrentHealth,T.H->MaxHealth); TestEqual(TEXT("Healing cannot revert phase"),T.A->Phase,2);
 FCountessBossTestAccess::PoiseHit(*T.A,100); TestEqual(TEXT("Post break immunity is poise only"),T.A->Poise,120.f);
 T.H->ApplyCombatHit(Spec,T.P); TestTrue(TEXT("Post break still takes HP damage"),T.H->CurrentHealth<T.H->MaxHealth);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossDedupSiphonTest,"ThirdPerson.Boss.HitDedupAndActualLifesteal",BossTestFlags)
bool FBossDedupSiphonTest::RunTest(const FString&)
{
 FBossTestWorld T; T.H->SetCurrentHealth(500); T.PH->SetCurrentHealth(8);
 FCountessBossTestAccess::Arm(*T.A,EBossAction::Siphon); FCountessBossTestAccess::Hit(*T.A,T.P);
 TestEqual(TEXT("Overkill heals half actual HP loss, not half base damage"),T.H->CurrentHealth,504.f);
 FCountessBossTestAccess::Hit(*T.A,T.P); TestEqual(TEXT("Duplicate overlap cannot heal again"),T.H->CurrentHealth,504.f);
 T.PH->SetCurrentHealth(100); FCountessBossTestAccess::Arm(*T.A,EBossAction::Combo);
 FCountessBossTestAccess::Hit(*T.A,T.P); FCountessBossTestAccess::Hit(*T.A,T.P);
 TestEqual(TEXT("Both blades share one stage hit set"),T.PH->CurrentHealth,84.f);
 FCountessBossTestAccess::Arm(*T.A,EBossAction::Combo,1); FCountessBossTestAccess::Hit(*T.A,T.P);
 TestEqual(TEXT("Next stage can hit again"),T.PH->CurrentHealth,66.f);
 FCombatHitSpec Spec; Spec.Damage=100; TestEqual(TEXT("Enemy friendly fire ignored"),T.H->ApplyCombatHit(Spec,T.B).ActualDamage,0.f);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossCancellationTest,"ThirdPerson.Boss.CancellationAndPool",BossTestFlags)
bool FBossCancellationTest::RunTest(const FString&)
{
 FBossTestWorld T; auto* Pool=T.W->GetSubsystem<UProjectilePoolSubsystem>();
 FCombatHitSpec Spec; Spec.Damage=20;
 auto* Wave=Pool->AcquireWithHitSpec(ABossBloodWave::StaticClass(),FTransform(FVector(300,0,100)),T.B,T.B,Spec,900);
 TestNotNull(TEXT("Wave acquired from existing pool"),Wave); TestTrue(TEXT("Wave immediate return contract"),Wave->bReturnImmediatelyOnImpact);
 TestTrue(TEXT("Wave flight budget is 1200/900"),FMath::IsNearlyEqual(Wave->FlightLifetime,1200.f/900.f));
 FCountessBossTestAccess::Arm(*T.A,EBossAction::Combo); auto* M=NewObject<UAnimMontage>(); FCountessBossTestAccess::Montage(*T.A,M);
 TestTrue(TEXT("Current notify accepted"),T.A->IsCurrentNotify(M,25)); TestFalse(TEXT("Previous montage notify rejected"),T.A->IsCurrentNotify(M,24));
 FCountessBossTestAccess::StaleEnd(*T.A); TestTrue(TEXT("Old delegate cannot cancel new action"),T.A->IsActionActive());
 auto Own=MakeShared<FRootMotionSource_MoveToForce>(); Own->InstanceName=TEXT("BossOwn"); Own->Duration=1;
 auto Other=MakeShared<FRootMotionSource_MoveToForce>(); Other->InstanceName=TEXT("Unrelated"); Other->Duration=1;
 const uint16 OwnedId=T.B->GetCharacterMovement()->ApplyRootMotionSource(Own), OtherId=T.B->GetCharacterMovement()->ApplyRootMotionSource(Other);
 FCountessBossTestAccess::OwnMotion(*T.A,OwnedId); T.A->CancelAction();
 TestFalse(TEXT("Cancellation rejects old notifies"),T.A->IsCurrentNotify(M,25)); TestFalse(TEXT("Cancellation returns owned projectiles"),Wave->IsProjectileActive());
 TestTrue(TEXT("Unrelated movement source preserved"),T.B->GetCharacterMovement()->GetRootMotionSourceByID(OtherId).IsValid());
 TestEqual(TEXT("Parent arrow still five seconds"),GetDefault<AWeaponProjectile>()->ImpactLifetime,5.f);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossSelectionTest,"ThirdPerson.Boss.SelectionAndReset",BossTestFlags)
bool FBossSelectionTest::RunTest(const FString&)
{
 FBossTestWorld T;
 TestFalse(TEXT("Phase one never picks Feast"),T.A->IsActionLegal(EBossAction::BloodFeast,200,true));
 TestFalse(TEXT("No LOS no skill"),T.A->IsActionLegal(EBossAction::Combo,100,false));
 TestFalse(TEXT("Rush too near"),T.A->IsActionLegal(EBossAction::ShadowRush,200,true));
 for (const auto& A:T.A->GetDefinition()->Actions) FCountessBossTestAccess::Cooldown(*T.A,A.Id,5);
 TestEqual(TEXT("No cooldown bypass when all unavailable"),T.A->SelectAction(200,true),EBossAction::None);
 T.A->Phase=2; T.A->Poise=3; T.H->SetCurrentHealth(100); T.A->RequestReset();
 TestEqual(TEXT("Disengage resets before any new attacks"),T.A->State,EBossState::Resetting);
 T.A->CompleteReset(); TestEqual(TEXT("Reset phase"),T.A->Phase,1); TestEqual(TEXT("Reset poise"),T.A->Poise,100.f); TestEqual(TEXT("Reset health"),T.H->CurrentHealth,T.H->MaxHealth);
 TestEqual(TEXT("Reset dormant"),T.A->State,EBossState::Dormant);
 T.A->Die(); T.A->RequestReset(); TestEqual(TEXT("Dead cannot reenter reset"),T.A->State,EBossState::Dead);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossFrameRateTest,"ThirdPerson.Boss.CrossedWindowsAt30_60_120",BossTestFlags)
bool FBossFrameRateTest::RunTest(const FString&)
{
 for (int32 FPS:{30,60,120})
 {
  FBossTestWorld T; FCountessBossTestAccess::Arm(*T.A,EBossAction::Siphon);
  float Old=0; for (int32 I=1;I<=FPS;++I) { float Time=I/static_cast<float>(FPS); FCountessBossTestAccess::Sample(*T.A,Old,Time); Old=Time; }
  TestEqual(FString::Printf(TEXT("One radial hit at %d FPS"),FPS),T.PH->CurrentHealth,78.f);
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossNativeAnimationTest,"ThirdPerson.Boss.NativeAnimationAndSavedBlueprint",BossTestFlags)
bool FBossNativeAnimationTest::RunTest(const FString&)
{
 auto* Class=LoadClass<ACountessBossCharacter>(nullptr,TEXT("/Game/Third/Bosses/Countess/BP_CountessBoss.BP_CountessBoss_C"));
 if (!TestNotNull(TEXT("Saved Boss BP reopens"),Class)) return false;
 const auto* CDO=Class->GetDefaultObject<ACountessBossCharacter>();
 if (!TestNotNull(TEXT("Saved component definition retained"),CDO->BossActions->Definition.Get())) return false;
 FBossTestWorld T;
 T.B->GetMesh()->SetSkeletalMesh(CDO->GetMesh()->GetSkeletalMeshAsset()); T.A->Definition=CDO->BossActions->Definition;
 auto* Mesh=T.B->GetMesh();
 for (FName Name:{FName(TEXT("BladeBase_L")),FName(TEXT("BladeTip_L")),FName(TEXT("BladeBase_R")),FName(TEXT("BladeTip_R"))}) TestTrue(*Name.ToString(),Mesh->DoesSocketExist(Name));
 TestNotNull(TEXT("Native anim instance exists"),Cast<UCountessBossAnimInstance>(Mesh->GetAnimInstance()));
 auto Animate=[&]() { Mesh->TickAnimation(1.f/60,false); Mesh->RefreshBoneTransforms(); Mesh->ConditionallyDispatchQueuedAnimEvents(); };
 for (int32 I=0;I<10;++I) Animate();
 const FVector Initial=Mesh->GetSocketLocation(TEXT("BladeTip_R"));
 FCountessBossTestAccess::Arm(*T.A,EBossAction::DelayedSlash); FCountessBossTestAccess::StartStage(*T.A);
 TestTrue(TEXT("Authored montage playing through DefaultSlot"),Mesh->GetAnimInstance()->Montage_IsPlaying(T.A->GetDefinition()->FindAction(EBossAction::DelayedSlash)->Stages[0].Montage));
 for (int32 I=0;I<25;++I) { Animate(); FCountessBossTestAccess::Tick(*T.A,1.f/60); }
 const FVector After=Mesh->GetSocketLocation(TEXT("BladeTip_R"));
 TestFalse(TEXT("Actual blade pose animates, not reference pose"),Initial.Equals(After,1.f));
 TestFalse(TEXT("Pose finite"),After.ContainsNaN());
 T.A->Die(); for (int32 I=0;I<180;++I) Animate(); const FVector Death=Mesh->GetSocketLocation(TEXT("spine_03"));
 for (int32 I=0;I<60;++I) Animate(); TestTrue(TEXT("Death holds final pose instead of looping"),Death.Equals(Mesh->GetSocketLocation(TEXT("spine_03")),.1f));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossPlayerPoiseTest,"ThirdPerson.Boss.PlayerHitContextsAndBackBlock",BossTestFlags)
bool FBossPlayerPoiseTest::RunTest(const FString&)
{
 FBossTestWorld T; auto* C=T.P->FindComponentByClass<UCombatComponent>();
 const EActiveCombatAttackType Types[]={EActiveCombatAttackType::Normal,EActiveCombatAttackType::Air,EActiveCombatAttackType::Uppercut,EActiveCombatAttackType::AirDive};
 const float Expected[]={10,12,30,25};
 for (int32 I=0;I<4;++I)
 {
  T.A->Poise=100; FCombatHitSpec Spec=FCountessBossTestAccess::PlayerHit(*C,Types[I]);
  TestEqual(TEXT("Actual player context has the intended total poise"),Spec.PoiseDamage,Expected[I]);
  Spec.Damage=1; T.H->ApplyCombatHit(Spec,T.P);
  if (Types[I]==EActiveCombatAttackType::Uppercut)
  { const FVector Before=T.B->GetVelocity(); T.B->ApplyUppercutHit(T.P); TestTrue(TEXT("Boss uppercut never launches the capsule"),Before.Equals(T.B->GetVelocity())); }
  TestEqual(TEXT("One typed hit deducts total poise once"),T.A->Poise,100-Expected[I]);
 }
 T.P->SetActorRotation(FRotator(0,180,0)); C->StartBlock();
 T.P->FindComponentByClass<UStaminaComponent>()->SetCurrentStamina(100);
 FCombatHitSpec Spec; Spec.Damage=20;
 TestEqual(TEXT("Back-facing held block does not block or parry"),T.PH->ApplyCombatHit(Spec,T.B).ActualDamage,20.f);
 return true;
}
#endif
