// 动作规则回归：覆盖缓存有效期、动作代际、闪避续接和根运动交接；测试由框架注册，不是未使用代码。
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "../Animation/CombatActionRules.h"
#include "../Animation/TPCAnimInstance.h"
#include "../Character/TPCCharacter.h"
#include "../Components/CombatComponent.h"
#include "../Components/StaminaComponent.h"
#include "../Components/HealthComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MotionWarpingComponent.h"
#include "RootMotionModifier_SkewWarp.h"

// These tests exercise C++ state transitions in a transient world, not project assets / PIE visuals.
struct FTPActionTestAccess
{
    static void ClearAttackRootMotion(ATPCCharacter& C) { C.ClearAttackRootMotionForDodge(); }
	static void Attack(UCombatComponent& C, UAnimMontage* M, int32 Id = 7)
	{
		C.ResetMeleeAction();
		C.bCombatEnabled = true;
		C.ActiveAttackMontage = M;
		C.ActiveAttackInstanceId = Id;
		C.bMeleeAttackInProgress = true;
		C.AttackMontages = { M, M };
		C.NextAttackIndex = 1;
	}
	static void StaleEnd(UCombatComponent& C, UAnimMontage* M)
	{
		C.HandleAttackMontageEnded(M, true, C.AttackGeneration - 1);
	}
	static void Interrupt(UCombatComponent& C, UAnimMontage* M)
	{
		C.HandleAttackBlendingOut(M, true, C.AttackGeneration);
	}
	static bool ChainReached(const UCombatComponent& C) { return C.ComboBuffer.bChainPointReached; }
	static void FinalStage(UCombatComponent& C) { C.NextAttackIndex = C.AttackMontages.Num(); }
	static void Dodge(ATPCCharacter& C) { C.MotionAction = ETPCMotionAction::Dodge; }
	static void CancelDodge(ATPCCharacter& C) { C.CancelMotionAction(); }
	static void RotationPolicy(ATPCCharacter& C) { C.ApplyActionRotationPolicy(); }
	static void StopGround(ATPCCharacter& C) { C.StopGroundInputMomentum(); }
	static void Hit(ATPCCharacter& C) { C.LockMovementForHit(0.2f); }
	static void EndHit(ATPCCharacter& C) { C.ClearHitMovementLock(); }
	static void Die(ATPCCharacter& C) { C.HandleDeath(); }
	static void AnimOwner(UTPCAnimInstance& A, ATPCCharacter* C) { A.Character = C; }
	static void SetAirBudget(UCombatComponent& C, int32 N) { C.NextAirAttackIndex = N; }
	static int32 AirBudget(const UCombatComponent& C) { return C.NextAirAttackIndex; }
    static int32 DashNext(const UCombatComponent& C) { return C.DashComboWindow.NextIndex; }
    static bool DashWaiting(const UCombatComponent& C) { return C.DashComboWindow.bDashing; }
    static bool DamageWindow(const UCombatComponent& C) { return C.bAttackWindowActive; }
    static void MarkDamageWindow(UCombatComponent& C) { C.bAttackWindowActive = true; }
    static void MarkDashUsed(UCombatComponent& C) { C.bDashUsedInCombo = true; }
    static void ChangeMontages(UCombatComponent& C) { C.AttackMontages.RemoveAt(1); }
    static void NaturalDashEnd(ATPCCharacter& C) { C.CancelMotionAction(0.f, false); }
};

namespace
{
	struct FActionTestWorld
	{
		UWorld* World = nullptr;
		ATPCCharacter* Character = nullptr;
		FActionTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Character = World->SpawnActor<ATPCCharacter>(ATPCCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
		}
		~FActionTestWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			if (World->IsRooted()) { World->RemoveFromRoot(); }
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPComboBufferTest, "ThirdPerson.ActionSystem.ComboBuffer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTPComboBufferTest::RunTest(const FString& Parameters)
{
	TPCActionRules::FComboBuffer Buffer;
	Buffer.Reset(10);
	Buffer.Queue();
	Buffer.Queue();
	TestFalse(TEXT("Early input waits for chain point"), Buffer.Consume(10, true));
	Buffer.bChainPointReached = true;
	TestFalse(TEXT("Previous generation cannot consume"), Buffer.Consume(9, true));
	TestFalse(TEXT("No next stage cannot consume"), Buffer.Consume(10, false));
	TestTrue(TEXT("One next stage is consumed at chain point"), Buffer.Consume(10, true));
	Buffer.Queue();
	TestFalse(TEXT("Spam cannot consume the same stage twice"), Buffer.Consume(10, true));
	Buffer.Reset(11);
	TestFalse(TEXT("New stage has no old input"), Buffer.bQueued);
	Buffer.bChainPointReached = true;
	Buffer.Queue();
	TestTrue(TEXT("Late press during recovery can chain"), Buffer.Consume(11, true));
	Buffer.Reset(12);
	TestFalse(TEXT("Interruption resets the point"), Buffer.bChainPointReached);
	TestFalse(TEXT("Interruption drops the queue"), Buffer.bQueued);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPMovementRulesTest, "ThirdPerson.ActionSystem.MovementRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTPMovementRulesTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Free locomotion unlocked"), TPCActionRules::LocksMovement(false, false, false, false, false));
	for (int32 Mask = 1; Mask < 32; ++Mask)
	{
		TestTrue(FString::Printf(TEXT("Every lock combination %d blocks input"), Mask),
			TPCActionRules::LocksMovement((Mask & 1) != 0, (Mask & 2) != 0, (Mask & 4) != 0, (Mask & 8) != 0, (Mask & 16) != 0));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPCombatLifecycleTest, "ThirdPerson.ActionSystem.CombatLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTPCombatLifecycleTest::RunTest(const FString& Parameters)
{
	FActionTestWorld Fixture;
	ATPCCharacter* P = Fixture.Character;
	if (!TestNotNull(TEXT("Transient character"), P)) { return false; }
	UCombatComponent& C = *P->CombatComponent;
	UAnimMontage* Montage = NewObject<UAnimMontage>();
	FTPActionTestAccess::Attack(C, Montage);
	TestTrue(TEXT("Attack locks movement"), P->IsMovementInputLocked());
	C.TryAttack();
	TestTrue(TEXT("Early click is buffered"), C.HasBufferedComboInput());
	C.CloseComboInputWindow();
	TestTrue(TEXT("Legacy window end preserves the buffer"), C.HasBufferedComboInput());
	TestFalse(TEXT("Legacy window end is not a chain point by default"), FTPActionTestAccess::ChainReached(C));
	TestFalse(TEXT("Stale montage instance notify rejected"), C.IsCurrentAttackNotify(Montage, 6));
	TestTrue(TEXT("Current montage instance notify accepted"), C.IsCurrentAttackNotify(Montage, 7));
	C.ReachComboChainPoint(Montage, 6);
	TestFalse(TEXT("Stale point cannot arm a new attack"), FTPActionTestAccess::ChainReached(C));
	FTPActionTestAccess::StaleEnd(C, Montage);
	TestTrue(TEXT("Stale end of same asset cannot reset new action"), C.IsMeleeAttackInProgress());
	FTPActionTestAccess::Interrupt(C, Montage);
	TestFalse(TEXT("Interrupt clears active attack"), C.IsMeleeAttackInProgress());
	TestFalse(TEXT("Interrupt clears queued input"), C.HasBufferedComboInput());
	TestFalse(TEXT("Interrupt releases movement"), P->IsMovementInputLocked());
	FTPActionTestAccess::Attack(C, Montage);
	FTPActionTestAccess::FinalStage(C);
	C.TryAttack();
	TestFalse(TEXT("Final combo stage does not wrap"), C.HasBufferedComboInput());
	C.SetCombatEnabled(false);
	C.TryAttack();
	TestFalse(TEXT("Disabled combat cannot restart"), C.IsMeleeAttackInProgress());
	TestFalse(TEXT("Disabled combat rejects old damage notify"), C.IsCurrentAttackNotify(Montage, 7));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPAirAndMovementTest, "ThirdPerson.ActionSystem.AirAndMovement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTPAirAndMovementTest::RunTest(const FString& Parameters)
{
	FActionTestWorld Fixture;
	ATPCCharacter* P = Fixture.Character;
	if (!TestNotNull(TEXT("Transient character"), P)) { return false; }
	UCharacterMovementComponent* Movement = P->GetCharacterMovement();
	Movement->SetMovementMode(MOVE_Falling);
	const FVector Momentum(120.f, 90.f, 230.f);
	Movement->Velocity = Momentum;
	P->CombatComponent->TryAirAttack();
	TestFalse(TEXT("AirLight does not queue a forced downward launch"), Movement->HandlePendingLaunch());
	TestEqual(TEXT("AirLight retains existing momentum"), Movement->Velocity, Momentum);
	P->StartBlock();
	TestTrue(TEXT("Air block locks input"), P->IsMovementInputLocked());
	TestEqual(TEXT("Air block preserves XYZ inertia"), Movement->Velocity, Momentum);
	P->CombatComponent->SetCombatEnabled(false);
	P->CombatComponent->SetCombatEnabled(true);
	FTPActionTestAccess::Hit(*P);
	TestTrue(TEXT("Hit locks input"), P->IsMovementInputLocked());
	TestEqual(TEXT("Air hit preserves existing inertia"), Movement->Velocity, Momentum);
	FTPActionTestAccess::EndHit(*P);
	TestFalse(TEXT("Hit completion releases input"), P->IsMovementInputLocked());
	FTPActionTestAccess::SetAirBudget(*P->CombatComponent, 2);
	P->CombatComponent->CancelActiveAttack();
	TestEqual(TEXT("Cancel does not reset per-flight attack budget"), FTPActionTestAccess::AirBudget(*P->CombatComponent), 2);
	P->CombatComponent->HandleOwnerLanded();
	TestEqual(TEXT("Landed restores the air-attack budget"), FTPActionTestAccess::AirBudget(*P->CombatComponent), 0);
	Movement->SetMovementMode(MOVE_Walking);
	Movement->Velocity = Momentum;
	Movement->MaxWalkSpeed = 540.f;
	FTPActionTestAccess::StopGround(*P);
	TestTrue(TEXT("Ground action removes residual walking velocity"), Movement->Velocity.IsNearlyZero());
	TestEqual(TEXT("Stopping non-sprint preserves configured BS speed"), Movement->MaxWalkSpeed, 540.f);
	P->StaminaComponent->SetCurrentStamina(100.f);
	const float OldDashTime = P->LastDashTime;
	P->Dash();
	TestEqual(TEXT("Missing dash montage does not spend stamina"), P->StaminaComponent->GetCurrentStamina(), 100.f);
	TestEqual(TEXT("Missing dash montage does not start cooldown"), P->LastDashTime, OldDashTime);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPRootTurnAndDeathTest, "ThirdPerson.ActionSystem.RootTurnAndDeath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTPRootTurnAndDeathTest::RunTest(const FString& Parameters)
{
	FActionTestWorld Fixture;
	ATPCCharacter* P = Fixture.Character;
	if (!TestNotNull(TEXT("Transient character"), P)) { return false; }
	UTPCAnimInstance* Anim = NewObject<UTPCAnimInstance>(P->GetMesh());
	FTPActionTestAccess::AnimOwner(*Anim, P);
	P->SetActorRotation(FRotator(0.f, 37.f, 0.f));
	Anim->TurnInPlaceDirection = ETPCTurnInPlaceDirection::Right90;
	Anim->CommitTurnInPlace();
	TestEqual(TEXT("Legacy CommitTurn never adds a second rotation"), P->GetActorRotation().Yaw, 37.0);
	TestTrue(TEXT("Legacy Turn state can exit"), Anim->bTurnAnimationFinished);
	FTPActionTestAccess::Dodge(*P);
	FTPActionTestAccess::RotationPolicy(*P);
	TestTrue(TEXT("Dodge locks movement"), P->IsMovementInputLocked());
	TestFalse(TEXT("Dodge disables controller yaw"), P->bUseControllerRotationYaw);
	TestFalse(TEXT("Dodge disables movement yaw"), P->GetCharacterMovement()->bOrientRotationToMovement);
	FTPActionTestAccess::CancelDodge(*P);
	TestFalse(TEXT("Interrupted dodge releases movement"), P->IsMovementInputLocked());
	TestTrue(TEXT("Free locomotion rotation restored"), P->GetCharacterMovement()->bOrientRotationToMovement);
	UAnimMontage* Montage = NewObject<UAnimMontage>();
	FTPActionTestAccess::Attack(*P->CombatComponent, Montage);
	P->CombatComponent->TryAttack();
	FTPActionTestAccess::Die(*P);
	TestTrue(TEXT("Death retains movement lock"), P->IsMovementInputLocked());
	TestFalse(TEXT("Death cancels the attack"), P->CombatComponent->IsMeleeAttackInProgress());
	TestFalse(TEXT("Death clears buffered input"), P->CombatComponent->HasBufferedComboInput());
	P->HandlePrimaryAttack();
	TestFalse(TEXT("Input after death cannot restart attack"), P->CombatComponent->IsMeleeAttackInProgress());
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPDashWindowTest, "ThirdPerson.ActionSystem.DashComboWindow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTPDashWindowTest::RunTest(const FString& Parameters)
{
    TPCActionRules::FDashComboWindow W;
    TestFalse(TEXT("Free dash does not queue an old combo"), W.Queue(0));
    W.Arm(1);
    TestTrue(TEXT("Early left click during first dash is queued"), W.Queue(10));
    TestEqual(TEXT("Dash input must wait for montage end"), W.Consume(10), -1);
    W.Finish(12, 0.65);
    TestTrue(TEXT("Queue retained through natural dash end"), W.bQueued);
    TestEqual(TEXT("Next stage resumes before deadline"), W.Consume(12.64), 1);
    TestEqual(TEXT("One click cannot resume twice"), W.Consume(12.64), -1);
    W.Arm(2); W.Finish(20, 0.65);
    TestEqual(TEXT("Expired continuation is discarded"), W.Consume(20.66), -1);
    W.Arm(1); W.Finish(30, 0);
    TestFalse(TEXT("Zero duration disables preservation"), W.IsOpen(30));
    W.Arm(1); W.Queue(0); W.Reset();
    TestFalse(TEXT("An interruption clears the pending click"), W.bQueued);
    TestFalse(TEXT("An interruption clears continuation"), W.IsOpen(0));
    TestFalse(TEXT("Attacking is not a dash prohibition"), TPCActionRules::BlocksDodge(false,false,false,false));
    for (int32 I=1; I<16; ++I)
        TestTrue(TEXT("Death/hit/guard/active dash still prohibit another dash"),
            TPCActionRules::BlocksDodge((I&1)!=0,(I&2)!=0,(I&4)!=0,(I&8)!=0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPDashCancelTest, "ThirdPerson.ActionSystem.DashCancelLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTPDashCancelTest::RunTest(const FString& Parameters)
{
    FActionTestWorld F;
    ATPCCharacter* P=F.Character;
    if(!TestNotNull(TEXT("Character"),P)) return false;
    UCombatComponent& C=*P->CombatComponent;
    P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    UAnimMontage* M=NewObject<UAnimMontage>();
    FTPActionTestAccess::Attack(C,M);
    FTPActionTestAccess::MarkDamageWindow(C);
    C.TryAttack();
    C.CancelAttackForDash(0.08f);
    TestFalse(TEXT("Dash cancel closes active attack"),C.IsMeleeAttackInProgress());
    TestFalse(TEXT("Dash cancel closes damage trace"),FTPActionTestAccess::DamageWindow(C));
    TestFalse(TEXT("Pre-dash attack input is not reused"),C.HasBufferedComboInput());
    TestFalse(TEXT("Old damage notifies are rejected"),C.IsCurrentAttackNotify(M,7));
    TestEqual(TEXT("First dash preserves next stage"),FTPActionTestAccess::DashNext(C),1);
    TestTrue(TEXT("Wait for dash end"),FTPActionTestAccess::DashWaiting(C));
    FTPActionTestAccess::StaleEnd(C,M);
    TestEqual(TEXT("Stale attack end cannot clear dash continuation"),FTPActionTestAccess::DashNext(C),1);
    TestTrue(TEXT("A new click during dash is accepted"),C.QueueAttackDuringDash());
    TestTrue(TEXT("Natural dash end requests queued continuation"),C.FinishDashForCombo(false,0.65f));
    TestFalse(TEXT("Natural end opens timed window"),FTPActionTestAccess::DashWaiting(C));
    C.CancelAttackForDash(0.08f);
    TestEqual(TEXT("Second consecutive dash resets combo"),FTPActionTestAccess::DashNext(C),INDEX_NONE);
    TestFalse(TEXT("Second dash does not queue continuation"),C.QueueAttackDuringDash());
    FTPActionTestAccess::Attack(C,M); FTPActionTestAccess::MarkDashUsed(C);
    C.CancelAttackForDash(0.08f);
    TestEqual(TEXT("Already resumed chain gets no second preservation"),FTPActionTestAccess::DashNext(C),INDEX_NONE);
    FTPActionTestAccess::Attack(C,M); C.CancelAttackForDash(0.08f);
    FTPActionTestAccess::ChangeMontages(C);
    TestFalse(TEXT("Changing weapon montage list invalidates continuation"),C.FinishDashForCombo(false,0.65f));
    TestEqual(TEXT("Changed montage list clears stored stage"),FTPActionTestAccess::DashNext(C),INDEX_NONE);
    FTPActionTestAccess::Attack(C,M); C.CancelAttackForDash(0.08f);
    C.SetCombatEnabled(false);
    TestEqual(TEXT("Death/combat disable clears stored stage"),FTPActionTestAccess::DashNext(C),INDEX_NONE);
    TestFalse(TEXT("Disabled combat rejects buffered dash input"),C.QueueAttackDuringDash());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPDashCancelGuardsTest, "ThirdPerson.ActionSystem.DashCancelGuards",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTPDashCancelGuardsTest::RunTest(const FString& Parameters)
{
    FActionTestWorld F;
    ATPCCharacter* P=F.Character;
    if(!TestNotNull(TEXT("Character"),P)) return false;
    UCombatComponent& C=*P->CombatComponent;
    UAnimMontage* M=NewObject<UAnimMontage>();
    P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    FTPActionTestAccess::Attack(C,M);
    P->StaminaComponent->SetCurrentStamina(0.f);
    P->Dash();
    TestTrue(TEXT("Insufficient stamina preserves current attack"),C.IsMeleeAttackInProgress());
    P->StaminaComponent->SetCurrentStamina(100.f);
    P->Dash();
    TestTrue(TEXT("Missing dash montage preserves current attack"),C.IsMeleeAttackInProgress());
    TestEqual(TEXT("Rejected dash spends no stamina"),P->StaminaComponent->GetCurrentStamina(),100.f);
    P->LastDashTime=P->GetWorld()->GetTimeSeconds();
    P->Dash();
    TestTrue(TEXT("Cooldown rejection preserves current attack"),C.IsMeleeAttackInProgress());
    C.CancelAttackForDash(0.08f); FTPActionTestAccess::Dodge(*P);
    P->HandlePrimaryAttack();
    FTPActionTestAccess::Hit(*P);
    TestFalse(TEXT("Hit interrupts dash"),P->IsDashing());
    TestEqual(TEXT("Hit clears dash combo and its input"),FTPActionTestAccess::DashNext(C),INDEX_NONE);
    FTPActionTestAccess::EndHit(*P);
    FTPActionTestAccess::Attack(C,M); C.CancelAttackForDash(0.08f);
    C.StartBlock();
    TestEqual(TEXT("Entering guard drops continuation"),FTPActionTestAccess::DashNext(C),INDEX_NONE);
    C.SetCombatEnabled(false); C.SetCombatEnabled(true);
    FTPActionTestAccess::Attack(C,M); FTPActionTestAccess::FinalStage(C);
    C.CancelAttackForDash(0.08f);
    TestEqual(TEXT("Final attack never wraps to first through dash"),FTPActionTestAccess::DashNext(C),INDEX_NONE);
    FTPActionTestAccess::Attack(C,M); P->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    C.CancelAttackForDash(0.08f);
    TestEqual(TEXT("Airborne attacks do not preserve a ground combo"),FTPActionTestAccess::DashNext(C),INDEX_NONE);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPSprintActionInterlockTest, "ThirdPerson.ActionSystem.SprintActionInterlock",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTPSprintActionInterlockTest::RunTest(const FString& Parameters)
{
    FActionTestWorld F;
    F.Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    F.Character->StaminaComponent->SetCurrentStamina(100.f);
    F.Character->HealthComponent->CurrentHealth = 100.f;
    F.Character->StartSprint();
    TestEqual(TEXT("Free sprint speed"), F.Character->GetCharacterMovement()->MaxWalkSpeed, F.Character->SprintMaxWalkSpeed);
    UAnimMontage* M = NewObject<UAnimMontage>();
    FTPActionTestAccess::Attack(*F.Character->CombatComponent, M);
    F.Character->Tick(.016f);
    TestEqual(TEXT("New attack state cancels stale sprint speed"), F.Character->GetCharacterMovement()->MaxWalkSpeed, 450.f);
    F.Character->StartSprint();
    TestEqual(TEXT("Shift cannot override active attack movement lock"), F.Character->GetCharacterMovement()->MaxWalkSpeed, 450.f);
    F.Character->CombatComponent->CancelActiveAttack();
    F.Character->StartSprint();F.Character->StartSprint();F.Character->StopSprint();F.Character->StopSprint();
    TestEqual(TEXT("Repeated starts and canceled/released stops restore walk speed"), F.Character->GetCharacterMovement()->MaxWalkSpeed, 450.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTPDodgeRootMotionHandoffTest, "ThirdPerson.ActionSystem.DodgeRootMotionHandoff",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTPDodgeRootMotionHandoffTest::RunTest(const FString& Parameters)
{
    FActionTestWorld W;
    ATPCCharacter* P = W.Character;
    auto* Warp = P->MotionWarpingComponent.Get();
    if (!Warp->HasBeenInitialized()) { Warp->InitializeComponent(); }
    auto* Modifier = NewObject<URootMotionModifier_SkewWarp>(Warp);
    Warp->AddModifier(Modifier);
    Modifier->SetState(ERootMotionModifierState::Active);
    Warp->AddOrUpdateWarpTargetFromLocationAndRotation(P->AttackWarpTargetName, FVector(600,0,0), FRotator::ZeroRotator);
    P->GetCharacterMovement()->RootMotionParams.Set(FTransform(FVector(120,0,0)));
    const FVector Before = P->GetActorLocation();
    FTPActionTestAccess::ClearAttackRootMotion(*P);
    TestTrue(TEXT("Old attack warp is disabled before dodge playback"), Modifier->GetState() == ERootMotionModifierState::Disabled);
    TestNull(TEXT("Attack target is removed"), Warp->FindWarpTarget(P->AttackWarpTargetName));
    TestFalse(TEXT("Previous extracted attack delta cannot move the new dodge"), P->GetCharacterMovement()->RootMotionParams.bHasRootMotion);
    TestEqual(TEXT("Handoff does not teleport the capsule"), P->GetActorLocation(), Before);
    Warp->AddOrUpdateWarpTargetFromLocationAndRotation(P->AttackWarpTargetName, FVector(200,0,0), FRotator::ZeroRotator);
    TestNotNull(TEXT("The next attack can install a new target"), Warp->FindWarpTarget(P->AttackWarpTargetName));
    return true;
}

#endif


