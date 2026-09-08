#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "BrainComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
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
#include "../AI/MeleeAIBehaviorNodes.h"
#include "../AI/MeleeAICombatSubsystem.h"
#include "../Character/TPCCharacter.h"
#include "../Components/CombatComponent.h"
#include "../Components/HealthComponent.h"

struct FMeleeAITestAccess
{
	static bool HasToken(const UMeleeAICombatSubsystem& Coordinator, const APawn* Pawn)
	{ return Coordinator.AttackTokenHolders.Contains(const_cast<APawn*>(Pawn)); }
	static bool DamageWindow(const UCombatComponent& Combat) { return Combat.bAttackWindowActive; }
	static void DisableRewards(AEnemyCharacter& Enemy)
	{ Enemy.ExperienceReward = 0; Enemy.DropChance = 0.f; }
	static void LoseSight(AEnemyAIController& Controller, AActor* Target)
	{
		FAIStimulus Stimulus;
		Stimulus.StimulusLocation = Target->GetActorLocation();
		Stimulus.MarkNoLongerSensed();
		Controller.HandleTargetPerceptionUpdated(Target, Stimulus);
	}
};

namespace MeleeAITests
{
constexpr auto Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
const TCHAR* Cases[] = {
	TEXT("NaturalAttack"), TEXT("RepeatedOrdinaryHit"), TEXT("HitInterruptsAttack"),
	TEXT("FaceBeforeAttack"), TEXT("ApproachStunReleasesReservation"), TEXT("FailedMoveReleasesReservation"),
	TEXT("AttackAbortClosesDamage"), TEXT("TargetMemoryExpires"), TEXT("ParrySupersedesOrdinaryHit"),
	TEXT("UppercutSupersedesOrdinaryHit"), TEXT("DeathDuringOrdinaryHit")
};

class FScenario : public IAutomationLatentCommand
{
	FAutomationTestBase* Test;
	TWeakObjectPtr<ATPCCharacter> Player;
	TWeakObjectPtr<AEnemyCharacter> Enemy, Ally;
	UClass* EnemyClass = nullptr;
	int32 Stage = 0, Case = 0, FPS = 0, Step = 0, AttackStarts = 0, AllyAttackStarts = 0;
	const int32 FrameRates[3] = {30, 60, 120};
	double Start = 0, EventAt = 0, SecondHitAt = 0;
	double WallStart = FPlatformTime::Seconds();
	bool bPreviousFixed = FApp::UseFixedTimeStep();
	double PreviousDelta = FApp::GetFixedDeltaTime();
	bool bSawAttack = false, bReleased = false;
	float HitDuration = 0.7f;
	TSet<FString> ReportedFailures;
	FDelegateHandle AttackHandle, AllyAttackHandle;
	FString Trace = TEXT("fps,case,time,attack,hit_reaction,stunned,token,coordinator_count,move,distance,speed,yaw,control_yaw,target,too_close,damage_window\n");

	FString Label(const TCHAR* Message) const
	{ return FString::Printf(TEXT("%d FPS %s: %s"), FrameRates[FPS], Cases[Case], Message); }
	void Check(bool bCondition, const TCHAR* Message)
	{
		if (bCondition) return;
		const FString Full = Label(Message);
		if (!ReportedFailures.Contains(Full)) { ReportedFailures.Add(Full); Test->AddError(Full); }
	}
	void RestoreClock()
	{ FApp::SetUseFixedTimeStep(bPreviousFixed); FApp::SetFixedDeltaTime(PreviousDelta); }
	void DestroyParticipant(TWeakObjectPtr<AEnemyCharacter>& Actor, FDelegateHandle& Handle)
	{
		if (Actor.IsValid())
		{
			Actor->FindComponentByClass<UCombatComponent>()->OnMeleeAttackStarted.Remove(Handle);
			if (AController* Controller = Actor->GetController()) { Controller->UnPossess(); Controller->Destroy(); }
			Actor->Destroy();
		}
		Actor.Reset(); Handle.Reset();
	}
	void Next()
	{
		Test->AddInfo(Label(TEXT("scenario completed")));
		UE_LOG(LogTemp, Display, TEXT("Melee AI: %s"), *Label(TEXT("scenario completed")));
		++Case;
		if (Case == UE_ARRAY_COUNT(Cases)) { Case = 0; ++FPS; }
		Stage = 1;
	}
	static void Place(ACharacter* Actor, FVector Location, float Yaw = 0.f)
	{
		Actor->GetCharacterMovement()->StopMovementImmediately();
		Actor->SetActorLocationAndRotation(Location, FRotator(0,Yaw,0), false, nullptr, ETeleportType::TeleportPhysics);
		if (Actor->GetController()) Actor->GetController()->SetControlRotation(FRotator(0,Yaw,0));
	}
	AEnemyCharacter* SpawnEnemy(UWorld* World, FVector Location, float Yaw)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Spawned = World->SpawnActor<AEnemyCharacter>(EnemyClass, Location, FRotator(0,Yaw,0), Params);
		// Repeated death scenarios must not open the unrelated XP upgrade UI and
		// pause PIE. Keep the real enemy death path, but remove its test rewards.
		if (Spawned) FMeleeAITestAccess::DisableRewards(*Spawned);
		return Spawned;
	}
public:
	explicit FScenario(FAutomationTestBase* InTest) : Test(InTest) {}
	~FScenario() override { RestoreClock(); }
	bool Update() override
	{
		if (FPlatformTime::Seconds() - WallStart > 180.)
		{ Test->AddError(TEXT("Melee AI PIE wall timeout")); RestoreClock(); return true; }
		UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
		if (!World || !World->HasBegunPlay()) return false;
		if (FPS == 3)
		{
			DestroyParticipant(Enemy, AttackHandle); DestroyParticipant(Ally, AllyAttackHandle);
			const FString Directory = FPaths::ProjectSavedDir() / TEXT("MeleeAIRepair");
			IFileManager::Get().MakeDirectory(*Directory, true);
			FFileHelper::SaveStringToFile(Trace, *(Directory / TEXT("combat_lifecycle.csv")));
			RestoreClock(); return true;
		}
		if (Stage == 0)
		{
			Player = Cast<ATPCCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));
			if (!Player.IsValid()) return false;
			for (TActorIterator<AEnemySpawner> It(World); It; ++It) It->Destroy();
			for (TActorIterator<AEnemyCharacter> It(World); It; ++It)
			{
				if (AController* Controller = It->GetController()) { Controller->UnPossess(); Controller->Destroy(); }
				It->Destroy();
			}
			EnemyClass = LoadClass<AEnemyCharacter>(nullptr, TEXT("/Game/Third/Character/BP_EnemyCharacter.BP_EnemyCharacter_C"));
			if (!Test->TestNotNull(TEXT("Saved melee blueprint exists"), EnemyClass)) return true;
			World->GetWorldSettings()->MinUndilatedFrameTime = 0;
			World->GetWorldSettings()->MaxUndilatedFrameTime = 1;
			auto* Health = Player->FindComponentByClass<UHealthComponent>();
			Health->MaxHealth = 10000; Health->SetCurrentHealth(10000);
			FApp::SetUseFixedTimeStep(true);
			Stage = 1;
		}
		if (Stage == 1)
		{
			DestroyParticipant(Enemy, AttackHandle); DestroyParticipant(Ally, AllyAttackHandle);
			FApp::SetFixedDeltaTime(1. / FrameRates[FPS]);
			Player->StopAnimMontage(); Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			const float Distance = Case == 0 || Case == 1 ? 600.f : (Case == 4 || Case == 5 ? 300.f : 140.f);
			Place(Player.Get(), FVector(Distance,-1200,98));
			Enemy = SpawnEnemy(World, FVector(0,-1200,98), Case == 3 ? 55.f : 0.f);
			if (!Test->TestNotNull(TEXT("Real melee actor spawned in PIE"), Enemy.Get())) return true;
			AttackStarts = AllyAttackStarts = Step = 0; bSawAttack = bReleased = false;
			Start = World->GetTimeSeconds(); EventAt = SecondHitAt = 0;
			HitDuration = Enemy->HitReactMontage ? Enemy->HitReactMontage->GetPlayLength() / FMath::Max(.01f,Enemy->HitReactMontage->RateScale) : .35f;
			AttackHandle = Enemy->FindComponentByClass<UCombatComponent>()->OnMeleeAttackStarted.AddLambda([this]()
			{
				++AttackStarts;
				const FVector ToTarget = (Player->GetActorLocation()-Enemy->GetActorLocation()).GetSafeNormal2D();
				Check(FVector::DotProduct(Enemy->GetActorForwardVector(),ToTarget) >= FMath::Cos(FMath::DegreesToRadians(10.1f)),
					TEXT("Every strike starts aligned within ten degrees"));
			});
			auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, Enemy->GetActorLocation(), Player->GetActorLocation(), Enemy.Get());
			Check(Path && Path->IsValid() && !Path->IsPartial(), TEXT("Test lane has a complete navigation path"));
			Stage = 2;
			return false;
		}
		if (!Enemy.IsValid() || !Player.IsValid())
		{ Test->AddError(TEXT("Melee PIE participant disappeared")); RestoreClock(); return true; }
		auto* AI = Cast<AEnemyAIController>(Enemy->GetController());
		auto* BB = AI ? AI->GetBlackboardComponent() : nullptr;
		auto* Combat = Enemy->FindComponentByClass<UCombatComponent>();
		auto* Health = Enemy->FindComponentByClass<UHealthComponent>();
		auto* Move = Enemy->GetCharacterMovement();
		auto* Coordinator = World->GetSubsystem<UMeleeAICombatSubsystem>();
		if (!AI || !BB || !Coordinator) { Test->AddError(TEXT("Saved melee controller/blackboard/coordinator missing")); return true; }
		const double Time = World->GetTimeSeconds()-Start;
		const bool bAttacking = Combat->IsMeleeAttackInProgress();
		const float Distance = FVector::Dist2D(Enemy->GetActorLocation(),Player->GetActorLocation());
		bSawAttack |= bAttacking;
		Player->FindComponentByClass<UHealthComponent>()->Heal(10000);
		Trace += FString::Printf(TEXT("%d,%s,%.4f,%d,%d,%d,%d,%d,%d,%.2f,%.2f,%.2f,%.2f,%d,%d,%d\n"),
			FrameRates[FPS],Cases[Case],Time,bAttacking,Enemy->IsHitReacting(),BB->GetValueAsBool(TEXT("bIsStunned")),
			FMeleeAITestAccess::HasToken(*Coordinator,Enemy.Get()),Coordinator->GetActiveAttackerCount(),static_cast<int32>(AI->GetMoveStatus()),
			Distance,Enemy->GetVelocity().Size2D(),Enemy->GetActorRotation().Yaw,AI->GetControlRotation().Yaw,
			BB->GetValueAsObject(TEXT("TargetActor"))==Player.Get(),BB->GetValueAsBool(TEXT("bIsTooClose")),FMeleeAITestAccess::DamageWindow(*Combat));

		if (Case == 0)
		{
			if (bAttacking)
			{
				Check(AI->GetMoveStatus()==EPathFollowingStatus::Idle, TEXT("Committed strike is not running a movement task"));
				Check(!BB->GetValueAsBool(TEXT("bIsTooClose")), TEXT("Attack root motion cannot trigger retreat"));
				Check(BB->GetValueAsObject(TEXT("TargetActor"))==Player.Get(), TEXT("No target churn during a frontal strike"));
				Check(FMeleeAITestAccess::HasToken(*Coordinator,Enemy.Get()), TEXT("Reservation stays held through the strike"));
			}
			if (Time>4.7) { Check(AttackStarts>=2,TEXT("Natural perception and saved tree produce repeated strikes")); Next(); }
		}
		else if (Case == 1 || Case == 2)
		{
			const bool bCanHit = Case == 1 ? AI->GetMoveStatus()==EPathFollowingStatus::Moving && Enemy->GetVelocity().Size2D()>50 && !bAttacking
				: bAttacking && Enemy->GetCurrentMontage() && Enemy->GetMesh()->GetAnimInstance()->Montage_GetPosition(Enemy->GetCurrentMontage())>.1f;
			if (Step == 0 && bCanHit)
			{
				Health->ApplyDamage(1); EventAt = Time; Step = 1;
				Check(Enemy->IsHitReacting() && !Combat->IsMeleeAttackInProgress(),TEXT("Ordinary hit immediately owns recovery and cancels attack"));
			}
			if (Step == 1 && Case == 1 && Time-EventAt>.25) { Health->ApplyDamage(1); SecondHitAt=Time; Step=2; }
			if (Step>0)
			{
				const double Since = Time-(SecondHitAt>0?SecondHitAt:EventAt);
				if (Since<HitDuration-.04)
				{
					Check(Enemy->IsHitReacting() && BB->GetValueAsBool(TEXT("bIsStunned")),TEXT("Recovery remains active until the authored hit duration"));
					Check(!Combat->IsCombatEnabled() && !Combat->IsMeleeAttackInProgress(),TEXT("Hit recovery cannot start a new attack"));
					Check(Move->MovementMode==MOVE_None && Enemy->GetVelocity().Size2D()<1,TEXT("Grounded hit recovery cannot slide"));
				}
				if (Since>HitDuration+.12)
				{
					Check(!Enemy->IsHitReacting() && !BB->GetValueAsBool(TEXT("bIsStunned")),TEXT("Ordinary hit recovers without leaving AI stuck"));
					Check(Combat->IsCombatEnabled() && Move->MovementMode==MOVE_Walking,TEXT("Recovery restores the prior combat and movement policy"));
					Next();
				}
			}
		}
		else if (Case == 3)
		{
			if (Time>2) { Check(AttackStarts>0,TEXT("Off-angle enemy turns and attacks instead of timing out")); Next(); }
		}
		else if (Case == 4 || Case == 5)
		{
			if (Step==0 && FMeleeAITestAccess::HasToken(*Coordinator,Enemy.Get()) && !bAttacking && AI->GetMoveStatus()==EPathFollowingStatus::Moving)
			{
				EventAt=Time; Step=1;
				if (Case==4)
				{
					// Direct BB interruption proves the sequence scope, independently of
					// EnemyCharacter's native stagger cleanup.
					BB->SetValueAsBool(TEXT("bIsStunned"),true);
					Ally=SpawnEnemy(World,FVector(-120,-900,98),-45);
					if (!Test->TestNotNull(TEXT("Second real melee actor spawned"), Ally.Get())) return true;
					AllyAttackHandle=Ally->FindComponentByClass<UCombatComponent>()->OnMeleeAttackStarted.AddLambda([this](){++AllyAttackStarts;});
				}
				else AI->StopMovement(); // Fail the pending MoveTo before the attack task is reached.
			}
			if (Step>0 && Time-EventAt>.08 && !bReleased)
			{
				Check(!FMeleeAITestAccess::HasToken(*Coordinator,Enemy.Get()),TEXT("Leaving an approach releases its reservation"));
				Check(!BB->GetValueAsBool(TEXT("bHasAttackToken")),TEXT("Blackboard reservation agrees with coordinator"));
				bReleased=true;
			}
			if (Step>0 && Time-EventAt>(Case==4?3.2:.2))
			{
				if (Case==4) Check(AllyAttackStarts>0,TEXT("Another melee enemy can attack while the original is stunned"));
				Next();
			}
		}
		else if (Case == 6)
		{
			if (Step==0 && bAttacking && FMeleeAITestAccess::DamageWindow(*Combat))
			{ BB->SetValueAsBool(TEXT("bIsStunned"),true); EventAt=Time; Step=1; }
			if (Step>0 && Time-EventAt>.08)
			{
				Check(!Combat->IsMeleeAttackInProgress() && !FMeleeAITestAccess::DamageWindow(*Combat),TEXT("Aborted strike leaves no damage window or pending attack"));
				Check(!FMeleeAITestAccess::HasToken(*Coordinator,Enemy.Get()),TEXT("Attack abort releases its reservation"));
				if (Time-EventAt>.25) Next();
			}
		}
		else if (Case == 7)
		{
			if (Step==0 && BB->GetValueAsObject(TEXT("TargetActor"))==Player.Get())
			{
				AI->GetBrainComponent()->StopLogic(TEXT("Isolate perception memory")); AI->StopMovement();
				Combat->CancelActiveAttack(); Place(Enemy.Get(),FVector(0,-1200,98),180);
				FMeleeAITestAccess::LoseSight(*AI,Player.Get()); EventAt=Time; Step=1;
			}
			if (Step==1 && Time-EventAt>.4)
			{
				Check(BB->GetValueAsObject(TEXT("TargetActor"))==Player.Get(),TEXT("Turning away does not immediately erase an engaged target"));
				Place(Player.Get(),FVector(2200,-1200,98)); EventAt=Time; Step=2;
			}
			else if (Step==2 && Time-EventAt>1.15)
			{ Check(BB->GetValueAsObject(TEXT("TargetActor"))==nullptr,TEXT("Genuinely distant target expires after the memory grace")); Next(); }
		}
		else if (Case >= 8)
		{
			if (Step==0) { Health->ApplyDamage(1); EventAt=Time; Step=1; }
			if (Step==1 && Time-EventAt>.15)
			{
				if (Case==8) Enemy->ApplyParryStagger(Player.Get());
				else if (Case==9) Enemy->ApplyUppercutHit(Player.Get());
				else Health->ApplyDamage(1000);
				Step=2;
			}
			if (Step==2 && Time-EventAt>HitDuration+.1)
			{
				Check(!Enemy->IsHitReacting(),TEXT("Stronger reaction cancels the old ordinary-hit timer"));
				Check(!Combat->IsCombatEnabled(),TEXT("Old hit timer cannot unlock the stronger reaction"));
				if (Case==8) Check(Enemy->IsParryStaggered() && BB->GetValueAsBool(TEXT("bIsStunned")),TEXT("Parry punish window survives the prior hit deadline"));
				else if (Case==9) Check(Enemy->GetLaunchPhase()!=EEnemyLaunchPhase::None && BB->GetValueAsBool(TEXT("bIsStunned")),TEXT("Uppercut remains physically gated after the old hit deadline"));
				else Check(Health->GetCurrentHealth()<=0 && Move->MovementMode==MOVE_None && !FMeleeAITestAccess::HasToken(*Coordinator,Enemy.Get()),TEXT("Dead enemy never resumes movement or keeps an attack slot"));
				Next();
			}
		}
		if (Time>8 && Stage==2) { Check(false,TEXT("Scenario precondition or recovery timed out")); Next(); }
		return false;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMeleeAIAssetContractTest,"ThirdPerson.MeleeAI.AssetContract",MeleeAITests::Flags)
bool FMeleeAIAssetContractTest::RunTest(const FString&)
{
	auto* Tree=LoadObject<UBehaviorTree>(nullptr,TEXT("/Game/Third/AI/BT_MeleeEnemy.BT_MeleeEnemy"));
	if (!TestNotNull(TEXT("Saved melee behavior tree"),Tree) || !TestNotNull(TEXT("Saved root"),Tree->RootNode.Get())) return false;
	TArray<UBTCompositeNode*> Pending{Tree->RootNode};
	int32 AttackScopes=0, AttackSequences=0, Retreats=0;
	while (!Pending.IsEmpty())
	{
		auto* Node=Pending.Pop(EAllowShrinking::No);
		bool bAttackSequence=false;
		for (const auto& Child:Node->Children)
		{
			if (Child.ChildComposite) Pending.Add(Child.ChildComposite);
			if (Child.ChildTask && Child.ChildTask->IsA<UBTTask_PerformMeleeAttack>()) bAttackSequence=true;
			for (UBTDecorator* Decorator:Child.Decorators)
				if (auto* BB=Cast<UBTDecorator_Blackboard>(Decorator); BB && BB->GetSelectedBlackboardKey()==TEXT("bIsTooClose"))
				{ ++Retreats; TestEqual(TEXT("Retreat observes entering and leaving its hysteresis range"),BB->GetFlowAbortMode(),EBTFlowAbortMode::Both); }
		}
		if (bAttackSequence)
		{
			++AttackSequences;
			for (UBTService* Service:Node->Services) if (Service->IsA<UBTService_MeleeAttackReservation>()) ++AttackScopes;
		}
	}
	TestEqual(TEXT("One attack sequence"),AttackSequences,1);
	TestEqual(TEXT("Reservation scope serialized on the entire attack sequence"),AttackScopes,1);
	TestEqual(TEXT("One retreat condition"),Retreats,1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMeleeAICombatLifecycleTest,"ThirdPerson.MeleeAI.PIE.CombatLifecycle",MeleeAITests::Flags)
bool FMeleeAICombatLifecycleTest::RunTest(const FString&)
{
	FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(MeleeAITests::FScenario(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
