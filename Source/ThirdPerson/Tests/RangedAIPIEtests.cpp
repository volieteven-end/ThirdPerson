// 远程 AI 回归：核对行为树、射击和后撤循环，以及不同帧率下的动作结束。
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AI/Navigation/AvoidanceManager.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "BrainComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/UnrealType.h"
#include "../AI/EnemyAIController.h"
#include "../AI/EnemyCharacter.h"
#include "../AI/EnemySpawner.h"
#include "../AI/RangedAIBehaviorNodes.h"
#include "../Character/TPCCharacter.h"
#include "../Components/CombatComponent.h"
#include "../Components/HealthComponent.h"
#include "../Weapons/ProjectilePoolSubsystem.h"
#include "../Weapons/WeaponProjectile.h"

struct FRangedAITestAccess
{
	static bool HasTunedProfile(const UBTService_UpdateRangedCombat& Service)
	{
		return Service.RetreatSpeed == 260.f && Service.ApproachSpeed == 360.f && Service.PatrolSpeed == 220.f &&
			Service.RetreatTriggerDistance == 300.f && Service.RetreatStopDistance == 440.f &&
			Service.MinimumAttackDistance == 180.f && Service.MaximumRetreatDuration == 1.f && Service.RetreatCooldown == 1.4f;
	}
};

namespace RangedAITests
{
constexpr auto Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
const TCHAR* Cases[] = {
	TEXT("StationaryClose"), TEXT("OrdinaryRunCanCatch"), TEXT("CommitShotWhileApproached"),
	TEXT("HitInterruptsRetreat"), TEXT("StunCancelsPendingArrow"), TEXT("LoseTargetAndStopRestorePolicy"),
	TEXT("DistantApproach"), TEXT("RetreatHysteresisAndCooldown")
};

class FScenario : public IAutomationLatentCommand
{
	FAutomationTestBase* Test;
	TWeakObjectPtr<ATPCCharacter> Player;
	TWeakObjectPtr<AEnemyCharacter> Enemy;
	UClass* EnemyClass = nullptr;
	int32 Stage = 0, Case = 0, FPS = 0, Step = 0, RetreatStarts = 0, ShotStarts = 0;
	const int32 Rates[3] = {30,60,120};
	double Start = 0., EventAt = 0., RetreatAt = -1., RetreatEndedAt = -1., CaughtAt = -1.;
	double WallStart = FPlatformTime::Seconds();
	double ApproachInsideSince = -1.;
	bool bPreviousFixed = FApp::UseFixedTimeStep();
	double PreviousDelta = FApp::GetFixedDeltaTime();
	bool bWasRetreating = false, bWasShooting = false, bSawApproach = false;
	float MaximumDistance = 0.f, MaximumSpeed = 0.f, MinimumDistance = BIG_NUMBER;
	TSet<TWeakObjectPtr<AWeaponProjectile>> Arrows;
	TSet<FString> ReportedFailures;
	FString Trace = TEXT("fps,case,time,distance,speed,max_speed,retreat,shooting,approach,sight,move,target,stunned,arrow_count,enemy_x,player_x\n");
	FString Label(const TCHAR* Message) const
	{ return FString::Printf(TEXT("%d FPS %s: %s"),Rates[FPS],Cases[Case],Message); }
	void Check(bool Condition, const TCHAR* Message)
	{
		if (Condition) return;
		const FString Full = Label(Message);
		if (!ReportedFailures.Contains(Full)) { ReportedFailures.Add(Full); Test->AddError(Full); }
	}
	void RestoreClock()
	{ FApp::SetUseFixedTimeStep(bPreviousFixed); FApp::SetFixedDeltaTime(PreviousDelta); }
	static void ForgetAvoidance(AEnemyCharacter* Actor)
	{
		// The engine normally ages dead RVO entries out after 1.5 seconds. Our
		// rapid fixture replacement must not leave phantom neighbours in the lane.
		if (auto* Avoidance = Actor->GetWorld()->GetAvoidanceManager())
			Avoidance->RemoveAvoidanceObject(Actor->GetCharacterMovement()->GetRVOAvoidanceUID());
	}
	void DestroyEnemy()
	{
		if (Enemy.IsValid())
		{
			ForgetAvoidance(Enemy.Get());
			Enemy->GetWorld()->GetSubsystem<UProjectilePoolSubsystem>()->ReleaseForSource(Enemy.Get());
			if (AController* AI = Enemy->GetController()) { AI->UnPossess(); AI->Destroy(); }
			Enemy->Destroy();
		}
		Enemy.Reset();
	}
	void Next()
	{
		Test->AddInfo(Label(TEXT("scenario completed")));
		UE_LOG(LogTemp,Display,TEXT("Ranged AI: %s"),*Label(TEXT("scenario completed")));
		++Case;
		if (Case == UE_ARRAY_COUNT(Cases)) { Case = 0; ++FPS; }
		Stage = 1;
	}
	static void Place(ACharacter* Actor, FVector Location, float Yaw = 0.f)
	{
		Actor->GetCharacterMovement()->StopMovementImmediately();
		Actor->SetActorLocationAndRotation(Location,FRotator(0,Yaw,0),false,nullptr,ETeleportType::TeleportPhysics);
		if (Actor->GetController()) Actor->GetController()->SetControlRotation(FRotator(0,Yaw,0));
	}
public:
	explicit FScenario(FAutomationTestBase* InTest) : Test(InTest) {}
	~FScenario() override { RestoreClock(); }
	bool Update() override
	{
		if (FPlatformTime::Seconds()-WallStart > 150.)
		{ Test->AddError(TEXT("Ranged AI PIE wall timeout")); RestoreClock(); return true; }
		UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
		if (!World || !World->HasBegunPlay()) return false;
		if (FPS == 3)
		{
			DestroyEnemy();
			const FString Directory = FPaths::ProjectSavedDir()/TEXT("RangedAIRepair");
			IFileManager::Get().MakeDirectory(*Directory,true);
			FFileHelper::SaveStringToFile(Trace,*(Directory/TEXT("kiting_lifecycle.csv")));
			RestoreClock(); return true;
		}
		if (Stage == 0)
		{
			Player = Cast<ATPCCharacter>(UGameplayStatics::GetPlayerCharacter(World,0));
			if (!Player.IsValid()) return false;
			for (TActorIterator<AEnemySpawner> It(World); It; ++It) It->Destroy();
			for (TActorIterator<AEnemyCharacter> It(World); It; ++It)
			{
				ForgetAvoidance(*It);
				if (AController* AI = It->GetController()) { AI->UnPossess(); AI->Destroy(); }
				It->Destroy();
			}
			EnemyClass = LoadClass<AEnemyCharacter>(nullptr,TEXT("/Game/Third/Character/BP_EnemyRangedCharacter.BP_EnemyRangedCharacter_C"));
			if (!Test->TestNotNull(TEXT("Saved ranged blueprint exists"),EnemyClass)) return true;
			// Isolate spacing from player knockback/hit-stun; projectiles remain real.
			Player->FindComponentByClass<UHealthComponent>()->SetEncounterInvulnerable(true);
			World->GetWorldSettings()->MinUndilatedFrameTime = 0;
			World->GetWorldSettings()->MaxUndilatedFrameTime = 1;
			FApp::SetUseFixedTimeStep(true);
			Stage = 1;
		}
		if (Stage == 1)
		{
			DestroyEnemy(); Arrows.Reset();
			FApp::SetFixedDeltaTime(1./Rates[FPS]);
			Player->StopAnimMontage(); Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			const float Distance = Case == 1 ? 400.f : (Case == 3 ? 140.f : (Case == 6 ? 1100.f : (Case == 0 || Case == 7 ? 280.f : 700.f)));
			Place(Player.Get(),FVector(Distance,-1200,98),180);
			FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Enemy = World->SpawnActor<AEnemyCharacter>(EnemyClass,FVector(0,-1200,98),FRotator::ZeroRotator,Params);
			if (!Test->TestNotNull(TEXT("Real ranged enemy spawned in PIE"),Enemy.Get())) return true;
			auto* AI = Cast<AEnemyAIController>(Enemy->GetController());
			if (!Test->TestNotNull(TEXT("Saved ranged controller spawned"),AI)) return true;
			// An already-engaged target moving just outside sight radius must be approached.
			if (Case == 6) AI->GetBlackboardComponent()->SetValueAsObject(TEXT("TargetActor"),Player.Get());
			Step = RetreatStarts = ShotStarts = 0;
			Start = World->GetTimeSeconds(); EventAt = 0.; RetreatAt = RetreatEndedAt = CaughtAt = -1.;
			ApproachInsideSince = -1.;
			bWasRetreating = bWasShooting = bSawApproach = false;
			MaximumDistance = MaximumSpeed = 0.f; MinimumDistance = BIG_NUMBER;
			auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World,Enemy->GetActorLocation(),Player->GetActorLocation(),Enemy.Get());
			Check(Path && Path->IsValid() && !Path->IsPartial(),TEXT("Test lane has a complete navigation path"));
			Stage = 2;
			return false;
		}
		if (!Enemy.IsValid() || !Player.IsValid()) { Test->AddError(TEXT("Ranged PIE participant disappeared")); return true; }
		auto* AI = Cast<AEnemyAIController>(Enemy->GetController());
		auto* BB = AI ? AI->GetBlackboardComponent() : nullptr;
		auto* Combat = Enemy->FindComponentByClass<UCombatComponent>();
		auto* Move = Enemy->GetCharacterMovement();
		if (!AI || !BB || !Combat) { Test->AddError(TEXT("Ranged AI components missing")); return true; }
		const double Time = World->GetTimeSeconds()-Start;
		const float Distance = FVector::Dist2D(Enemy->GetActorLocation(),Player->GetActorLocation());
		const float Speed = Enemy->GetVelocity().Size2D();
		const bool Retreat = BB->GetValueAsBool(TEXT("bIsTooClose"));
		const bool Shooting = Combat->IsRangedAttackInProgress();
		const bool Approach = BB->GetValueAsBool(TEXT("bShouldApproachTarget"));
		const bool Sight = BB->GetValueAsBool(TEXT("bHasLineOfSight"));
		const bool Stunned = BB->GetValueAsBool(TEXT("bIsStunned"));
		const bool HasTarget = BB->GetValueAsObject(TEXT("TargetActor")) == Player.Get();
		const int32 MoveStatus = static_cast<int32>(AI->GetMoveStatus());
		MaximumDistance = FMath::Max(MaximumDistance,Distance);
		MinimumDistance = FMath::Min(MinimumDistance,Distance);
		MaximumSpeed = FMath::Max(MaximumSpeed,Speed);
		if (Distance < 130.f && CaughtAt < 0.) CaughtAt = Time;
		for (TActorIterator<AWeaponProjectile> It(World); It; ++It)
			if (It->GetOwner() == Enemy.Get() && It->IsProjectileActive()) Arrows.Add(*It);
		if (Retreat && !bWasRetreating)
		{
			++RetreatStarts;
			if (RetreatEndedAt >= 0.) Check(Time-RetreatEndedAt >= 1.35,TEXT("Retreat cooldown cannot be bypassed by repeated pressure"));
			RetreatAt = Time;
		}
		if (!Retreat && bWasRetreating) RetreatEndedAt = Time;
		if (Retreat)
		{
			Check(Time-RetreatAt < 1.25,TEXT("One continuous retreat is bounded to about one second"));
			Check(Move->MaxWalkSpeed <= 260.1f && Speed <= 261.f,TEXT("Retreat never uses the old 600 cm/s speed"));
		}
		if (Shooting && !bWasShooting) ++ShotStarts;
		if (Shooting) Check(!Retreat && AI->GetMoveStatus()==EPathFollowingStatus::Idle,TEXT("Bow wind-up owns a stationary firing opportunity"));
		if (HasTarget && Sight && Distance <= 440.f && !Stunned)
			Check(!Approach,TEXT("Close-range cooldown cannot fall through to Chase"));
		if (HasTarget && Sight && Distance <= 950.f && Approach && !Stunned)
		{
			if (ApproachInsideSince < 0.) ApproachInsideSince = Time;
			Check(Time-ApproachInsideSince <= .22,TEXT("Crossing into range ends approach within one context-service interval"));
		}
		else ApproachInsideSince = -1.;
		bWasRetreating = Retreat; bWasShooting = Shooting;
		Trace += FString::Printf(TEXT("%d,%s,%.4f,%.2f,%.2f,%.2f,%d,%d,%d,%d,%d,%d,%d,%d,%.2f,%.2f\n"),
			Rates[FPS],Cases[Case],Time,Distance,Speed,Move->MaxWalkSpeed,Retreat,Shooting,Approach,Sight,MoveStatus,
			HasTarget,Stunned,Arrows.Num(),Enemy->GetActorLocation().X,Player->GetActorLocation().X);
		if (Case == 0)
		{
			if (Time > 4.)
			{
				Check(RetreatStarts == 1,TEXT("Stationary nearby player needs one short backstep"));
				Check(MaximumDistance < 500.f,TEXT("Backstep stops in the intended band instead of fleeing to 650 cm"));
				Check(Distance >= 400.f,TEXT("An unobstructed backstep makes useful space at every frame rate"));
				Check(MaximumSpeed <= 261.f,TEXT("Close combat movement uses the retreat profile"));
				Check(ShotStarts > 0 && Arrows.Num() > 0,TEXT("Enemy actually fires after making space")); Next();
			}
		}
		else if (Case == 1)
		{
			Player->AddMovementInput((Enemy->GetActorLocation()-Player->GetActorLocation()).GetSafeNormal2D());
			if (Time > 3.2)
			{
				Check(Player->GetCharacterMovement()->MaxWalkSpeed == 450.f,TEXT("Catch test uses normal movement, not sprint"));
				Check(CaughtAt >= 0. && CaughtAt < 2.5,TEXT("Ordinary player movement can reach melee distance"));
				Check(RetreatStarts > 0,TEXT("Catch case exercised a real retreat")); Next();
			}
		}
		else if (Case == 2)
		{
			if (Step == 0 && Shooting && Enemy->GetCurrentMontage() &&
				Enemy->GetMesh()->GetAnimInstance()->Montage_GetPosition(Enemy->GetCurrentMontage()) > .3f)
			{ Place(Player.Get(),Enemy->GetActorLocation()+FVector(250,0,0),180); EventAt=Time; Step=1; }
			if (Step == 1 && Time-EventAt < .8)
				Check(Shooting && !Retreat,TEXT("Entering the 300 cm retreat band does not discard a committed shot"));
			if (Step == 1 && Time-EventAt > 1.3)
			{ Check(Arrows.Num() > 0,TEXT("Committed bow cycle releases a real arrow")); Next(); }
		}
		else if (Case == 3)
		{
			if (Step == 0 && Retreat && Speed > 30.f)
			{ Enemy->FindComponentByClass<UHealthComponent>()->ApplyDamage(1); EventAt=Time; Step=1; }
			if (Step == 1 && Time-EventAt < .5)
				Check(Enemy->IsHitReacting() && Move->MovementMode==MOVE_None && Enemy->GetVelocity().Size2D()<1.f && !Combat->IsRangedAttackInProgress(),
					TEXT("Ordinary hit stops retreat and shooting through hit recovery"));
			if (Step == 1 && Time-EventAt > .95)
			{ Check(!Enemy->IsHitReacting() && Move->MovementMode==MOVE_Walking,TEXT("Ranged enemy recovers without becoming stuck")); Next(); }
		}
		else if (Case == 4)
		{
			if (Step == 0 && Shooting && Enemy->GetCurrentMontage() &&
				Enemy->GetMesh()->GetAnimInstance()->Montage_GetPosition(Enemy->GetCurrentMontage()) > .3f)
			{ BB->SetValueAsBool(TEXT("bIsStunned"),true); EventAt=Time; Step=1; }
			if (Step == 1 && Time-EventAt > .12)
			{
				Check(!Combat->IsRangedAttackInProgress(),TEXT("Stun aborts the pending ranged attack"));
				Check(Arrows.IsEmpty(),TEXT("No late release notify fires an arrow after stun"));
				if (Time-EventAt > 1.6) Next();
			}
		}
		else if (Case == 5)
		{
			if (Step == 0 && HasTarget)
			{ Place(Player.Get(),FVector(2200,-1200,98),180); EventAt=Time; Step=1; }
			if (Step == 1 && Time-EventAt > 1.)
			{
				Check(!HasTarget && Move->MaxWalkSpeed==220.f,TEXT("Lost target restores the slower patrol/investigation profile"));
				Check(!Approach && BB->GetValueAsFloat(TEXT("DistanceToTarget"))>1.e8f,TEXT("Absent target clears stale distance and approach state"));
				AI->GetBrainComponent()->StopLogic(TEXT("Verify ranged movement ownership cleanup"));
				Check(Move->MaxWalkSpeed==360.f && Move->bOrientRotationToMovement && !Move->bUseControllerDesiredRotation && !Enemy->bUseControllerRotationYaw,
					TEXT("Stopping the tree restores the saved movement and facing policy")); Next();
			}
		}
		else if (Case == 6)
		{
			if (Approach && Speed > 20.f)
			{ bSawApproach=true; Check(Move->MaxWalkSpeed==360.f && Speed<=361.f,TEXT("Distant approach uses 360 cm/s instead of sprinting")); }
			if (Time > 3.5)
			{ Check(bSawApproach && ShotStarts>0 && Arrows.Num()>0,TEXT("Enemy approaches a distant target and resumes real shooting")); Next(); }
		}
		else if (Case == 7)
		{
			if (Step == 0 && Retreat) { EventAt=Time; Step=1; }
			if (Step == 1)
			{
				Place(Player.Get(),Enemy->GetActorLocation()+FVector(350,0,0),180);
				Check(Retreat,TEXT("Retreat persists between its enter and exit distances"));
				if (Time-EventAt > .55) { Place(Player.Get(),Enemy->GetActorLocation()+FVector(480,0,0),180); EventAt=Time; Step=2; }
			}
			else if (Step == 2 && Time-EventAt > .22)
			{
				Check(!Retreat,TEXT("Moving beyond the release band aborts the obsolete retreat path"));
				Place(Player.Get(),Enemy->GetActorLocation()+FVector(140,0,0),180); EventAt=Time; Step=3;
			}
			else if (Step == 3)
			{
				Check(!Retreat && !Approach,TEXT("Immediate re-approach cannot bypass the recovery pause"));
				if (Time-EventAt > .8) Next();
			}
		}
		if (Time>7 && Stage==2) { Check(false,TEXT("Scenario precondition or recovery timed out")); Next(); }
		return false;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRangedAIAssetContractTest,"ThirdPerson.RangedAI.AssetContract",RangedAITests::Flags)
bool FRangedAIAssetContractTest::RunTest(const FString&)
{
	auto* Tree=LoadObject<UBehaviorTree>(nullptr,TEXT("/Game/Third/AI/BT_RangeEnemy.BT_RangeEnemy"));
	UClass* Class=LoadClass<AEnemyCharacter>(nullptr,TEXT("/Game/Third/Character/BP_EnemyRangedCharacter.BP_EnemyRangedCharacter_C"));
	if (!TestNotNull(TEXT("Saved ranged tree"),Tree) || !TestNotNull(TEXT("Saved ranged enemy class"),Class)) return false;
	auto* Enemy=Class->GetDefaultObject<AEnemyCharacter>();
	TestEqual(TEXT("Saved base speed is not 600 cm/s"),Enemy->GetCharacterMovement()->MaxWalkSpeed,360.f);
	TestFalse(TEXT("Patrol has no competing instant yaw controller"),Enemy->bUseControllerRotationYaw);
	TestTrue(TEXT("Approach gate exists in saved blackboard"),Tree->BlackboardAsset->GetKeyType(Tree->BlackboardAsset->GetKeyID(TEXT("bShouldApproachTarget")))==UBlackboardKeyType_Bool::StaticClass());
	int32 Profiles=0, ApproachGuards=0, Holds=0;
	TArray<UBTCompositeNode*> Pending{Tree->RootNode};
	while (!Pending.IsEmpty())
	{
		auto* Node=Pending.Pop(EAllowShrinking::No);
		for (UBTService* Service:Node->Services)
			if (auto* Ranged=Cast<UBTService_UpdateRangedCombat>(Service))
			{ ++Profiles; TestTrue(TEXT("Actual saved service has the tuned movement and spacing profile"),FRangedAITestAccess::HasTunedProfile(*Ranged)); }
		for (const auto& Child:Node->Children)
		{
			if (Child.ChildComposite) Pending.Add(Child.ChildComposite);
			if (Child.ChildTask && Child.ChildTask->GetNodeName()==TEXT("Hold Ranged Ground"))
			{ ++Holds; TestTrue(TEXT("Hold is the last fallback, after retreat/shoot/chase"),&Child==&Node->Children.Last()); }
			for (UBTDecorator* Decorator:Child.Decorators)
				if (auto* BB=Cast<UBTDecorator_Blackboard>(Decorator); BB && BB->GetSelectedBlackboardKey()==TEXT("bShouldApproachTarget"))
				{ ++ApproachGuards; TestEqual(TEXT("Chase observes enter and exit"),BB->GetFlowAbortMode(),EBTFlowAbortMode::Both); }
		}
	}
	TestEqual(TEXT("One ranged profile service"),Profiles,1);
	TestEqual(TEXT("One guarded chase branch"),ApproachGuards,1);
	TestEqual(TEXT("One stationary fallback"),Holds,1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRangedAIKitingLifecycleTest,"ThirdPerson.RangedAI.PIE.KitingAt30_60_120",RangedAITests::Flags)
bool FRangedAIKitingLifecycleTest::RunTest(const FString&)
{
	FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(RangedAITests::FScenario(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
