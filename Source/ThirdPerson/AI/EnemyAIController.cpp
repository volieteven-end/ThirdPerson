// Fill out your copyright notice in the Description page of Project Settings.


#include "EnemyAIController.h"
#include "Kismet/GameplayStatics.h"
#include "../Components/HealthComponent.h"
#include "EnemyCharacter.h"
#include "Navigation/PathFollowingComponent.h"
#include "../Components/CombatComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "MeleeAICombatSubsystem.h"

namespace EnemyBlackboardKeys
{
	static const FName TargetActor(TEXT("TargetActor"));
	static const FName HomeLocation(TEXT("HomeLocation"));
	static const FName LastKnownLocation(TEXT("LastKnownLocation"));
	static const FName HasLineOfSight(TEXT("bHasLineOfSight"));
	static const FName HasAttackToken(TEXT("bHasAttackToken"));
}

AEnemyAIController::AEnemyAIController()
{
	PrimaryActorTick.bCanEverTick = true;

	EnemyPerception = CreateDefaultSubobject<UAIPerceptionComponent>(
		TEXT("EnemyPerception"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = 70.f;
	SightConfig->SetMaxAge(3.f);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	EnemyPerception->ConfigureSense(*SightConfig);
	EnemyPerception->SetDominantSense(SightConfig->GetSenseImplementation());
	SetPerceptionComponent(*EnemyPerception);
	EnemyPerception->OnTargetPerceptionUpdated.AddDynamic(
		this, &ThisClass::HandleTargetPerceptionUpdated);
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!SightConfig)
	{
		SightConfig = NewObject<UAISenseConfig_Sight>(
			this,
			TEXT("RuntimeSightConfig"));

		SightConfig->PeripheralVisionAngleDegrees = 70.f;
		SightConfig->SetMaxAge(3.f);
		SightConfig->DetectionByAffiliation.bDetectEnemies = true;
		SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
		SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	}

	if (EnemyPerception && SightConfig)
	{
		SightConfig->SightRadius = SightRadius;
		SightConfig->LoseSightRadius =
			FMath::Max(SightRadius, LoseSightRadius);

		EnemyPerception->ConfigureSense(*SightConfig);
		EnemyPerception->SetDominantSense(
			SightConfig->GetSenseImplementation());

		EnemyPerception->RequestStimuliListenerUpdate();
	}

	bUsingBehaviorTree =
		BehaviorTreeAsset &&
		RunBehaviorTree(BehaviorTreeAsset);

	if (bUsingBehaviorTree && GetBlackboardComponent() && InPawn)
	{
		GetBlackboardComponent()->SetValueAsVector(
			EnemyBlackboardKeys::HomeLocation,
			InPawn->GetActorLocation());

		GetBlackboardComponent()->SetValueAsBool(
			EnemyBlackboardKeys::HasAttackToken,
			false);
	}
}

void AEnemyAIController::OnUnPossess()
{
	if (APawn* ControlledPawn = GetPawn())
	{
		if (UWorld* World = ControlledPawn->GetWorld())
		{
			if (UMeleeAICombatSubsystem* Coordinator =
				World->GetSubsystem<UMeleeAICombatSubsystem>())
			{
				Coordinator->ReleaseEnemy(ControlledPawn);
			}
		}
	}
	bUsingBehaviorTree = false;
	Super::OnUnPossess();
}

void AEnemyAIController::HandleTargetPerceptionUpdated(
	AActor* Actor,
	FAIStimulus Stimulus)
{
	APawn* SensedPawn = Cast<APawn>(Actor);
	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (!bUsingBehaviorTree || !BlackboardComp || !SensedPawn ||
		!SensedPawn->IsPlayerControlled())
	{
		return;
	}

	BlackboardComp->SetValueAsVector(
		EnemyBlackboardKeys::LastKnownLocation, Stimulus.StimulusLocation);
	BlackboardComp->SetValueAsBool(
		EnemyBlackboardKeys::HasLineOfSight, Stimulus.WasSuccessfullySensed());

	if (Stimulus.WasSuccessfullySensed())
	{
		BlackboardComp->SetValueAsObject(EnemyBlackboardKeys::TargetActor, Actor);
	}
	else
	{
		BlackboardComp->ClearValue(EnemyBlackboardKeys::TargetActor);
		if (APawn* ControlledPawn = GetPawn())
		{
			if (UMeleeAICombatSubsystem* Coordinator =
				ControlledPawn->GetWorld()->GetSubsystem<UMeleeAICombatSubsystem>())
			{
				Coordinator->ReleaseAttackToken(ControlledPawn);
			}
		}
		BlackboardComp->SetValueAsBool(EnemyBlackboardKeys::HasAttackToken, false);
	}
}

void AEnemyAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bUsingBehaviorTree)
	{
		return;
	}

	APawn* EnemyPawn = GetPawn();

	APawn* PlayerPawn =
		UGameplayStatics::GetPlayerPawn(this, 0);

	if (!EnemyPawn || !PlayerPawn)
	{
		return;
	}

	AEnemyCharacter* EnemyCharacter =
		Cast<AEnemyCharacter>(EnemyPawn);

	if (!EnemyCharacter)
	{
		return;
	}

	const float DistanceSquared = FVector::DistSquared(
		EnemyPawn->GetActorLocation(),
		PlayerPawn->GetActorLocation());

	const bool bShouldChase =
		DistanceSquared <= FMath::Square(ChaseDistance);

	const bool bTooClose =
		DistanceSquared <= FMath::Square(StopDistance);

	// 玩家进入追击范围：中断巡逻。
	if (bShouldChase)
	{
		if (bIsPatrolling)
		{
			StopMovement();
			bIsPatrolling = false;
		}

		// 到达攻击距离：停止并持续尝试攻击。
		if (bTooClose)
		{
			if (bIsChasing)
			{
				StopMovement();
				bIsChasing = false;
			}

			TryAttack();
			return;
		}

		// 在追击范围、但不够近：追玩家。
		if (!bIsChasing ||GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			MoveToActor(PlayerPawn,StopDistance,false); // 不额外叠加双方胶囊体半径
			bIsChasing = true;
		}

		return;
	}

	// 玩家离开追击范围：停止追击。
	if (bIsChasing)
	{
		StopMovement();
		bIsChasing = false;
	}

	// 没有 Patrol Point 时，敌人原地待机。
	AActor* PatrolTarget =
		EnemyCharacter->GetCurrentPatrolPoint();

	if (!PatrolTarget)
	{
		return;
	}

	// 向当前巡逻点移动。
	if (!bIsPatrolling)
	{
		MoveToActor(PatrolTarget, 50.f);
		bIsPatrolling = true;
		return;
	}

	// 到达巡逻点后，切换到下一个。
	if (GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		EnemyCharacter->AdvancePatrolPoint();
		bIsPatrolling = false;
	}
}
void AEnemyAIController::TryAttack()
{
	AEnemyCharacter* EnemyCharacter =
		Cast<AEnemyCharacter>(GetPawn());

	if (!EnemyCharacter)
	{
		return;
	}

	if (UCombatComponent* CombatComponent =
		EnemyCharacter->FindComponentByClass<UCombatComponent>())
	{
		CombatComponent->TryAttack();
	}
}
