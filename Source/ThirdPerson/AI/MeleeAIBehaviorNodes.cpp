#include "MeleeAIBehaviorNodes.h"

#include "EnemyCharacter.h"
#include "MeleeAICombatSubsystem.h"
#include "../Components/CombatComponent.h"
#include "../Components/HealthComponent.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"

namespace MeleeAIKeys
{
	static const FName TargetActor(TEXT("TargetActor"));
	static const FName DistanceToTarget(TEXT("DistanceToTarget"));
	static const FName HasLineOfSight(TEXT("bHasLineOfSight"));
	static const FName IsInAttackRange(TEXT("bIsInAttackRange"));
	static const FName IsTooClose(TEXT("bIsTooClose"));
	static const FName CombatSlotLocation(TEXT("CombatSlotLocation"));
	static const FName HasAttackToken(TEXT("bHasAttackToken"));
	static const FName RetreatLocation(TEXT("RetreatLocation"));
	static const FName PatrolTarget(TEXT("PatrolTarget"));
	static const FName LastKnownLocation(TEXT("LastKnownLocation"));
	static const FName NeedsSeparation(TEXT("bNeedsSeparation"));
	static const FName SeparationLocation(TEXT("SeparationLocation"));
}

UBTService_UpdateEnemySeparation::UBTService_UpdateEnemySeparation()
{
	NodeName = TEXT("Update Enemy Separation");
	Interval = 0.12f;
	RandomDeviation = 0.025f;
	NeedsSeparationKey.SelectedKeyName = MeleeAIKeys::NeedsSeparation;
	SeparationLocationKey.SelectedKeyName = MeleeAIKeys::SeparationLocation;
}

void UBTService_UpdateEnemySeparation::TickNode(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory,
	float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();
	AEnemyCharacter* Enemy = Controller
		? Cast<AEnemyCharacter>(Controller->GetPawn()) : nullptr;
	UWorld* World = Enemy ? Enemy->GetWorld() : nullptr;
	if (!Blackboard || !Enemy || !World)
	{
		return;
	}

	auto ClearSeparation = [Blackboard, this]()
	{
		Blackboard->SetValueAsBool(
			NeedsSeparationKey.SelectedKeyName, false);
		Blackboard->ClearValue(SeparationLocationKey.SelectedKeyName);
	};

	if (const UHealthComponent* Health =
		Enemy->FindComponentByClass<UHealthComponent>();
		!Health || Health->GetCurrentHealth() <= 0.f)
	{
		ClearSeparation();
		return;
	}

	if (bIgnoreWhileAttacking)
	{
		if (const UCombatComponent* Combat =
			Enemy->FindComponentByClass<UCombatComponent>();
			Combat && (Combat->IsMeleeAttackInProgress() ||
				Combat->IsRangedAttackInProgress()))
		{
			ClearSeparation();
			return;
		}
	}

	const float QueryRadius = FMath::Max(
		SeparationRadius, SeparationReleaseRadius);
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(EnemySeparation), false, Enemy);
	World->OverlapMultiByObjectType(
		Overlaps,
		Enemy->GetActorLocation(),
		FQuat::Identity,
		ObjectQuery,
		FCollisionShape::MakeSphere(QueryRadius),
		QueryParams);

	FVector WeightedAway = FVector::ZeroVector;
	FVector NearestAway = FVector::ZeroVector;
	float NearestDistance = BIG_NUMBER;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AEnemyCharacter* Other = Cast<AEnemyCharacter>(Overlap.GetActor());
		if (!Other || Other == Enemy)
		{
			continue;
		}

		const UHealthComponent* OtherHealth =
			Other->FindComponentByClass<UHealthComponent>();
		if (!OtherHealth || OtherHealth->GetCurrentHealth() <= 0.f)
		{
			continue;
		}

		FVector Away = Enemy->GetActorLocation() - Other->GetActorLocation();
		Away.Z = 0.f;
		float Distance = Away.Size();
		if (Distance > QueryRadius)
		{
			continue;
		}

		if (Distance <= UE_KINDA_SMALL_NUMBER)
		{
			const float Angle = FMath::DegreesToRadians(
				static_cast<float>((Enemy->GetUniqueID() * 47) % 360));
			Away = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
			Distance = 0.f;
		}
		else
		{
			Away /= Distance;
		}

		const float Weight = FMath::Square(
			1.f - FMath::Clamp(Distance / QueryRadius, 0.f, 1.f));
		WeightedAway += Away * Weight;
		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			NearestAway = Away;
		}
	}

	const bool bWasSeparating = Blackboard->GetValueAsBool(
		NeedsSeparationKey.SelectedKeyName);
	const float ActiveRadius = bWasSeparating
		? QueryRadius : SeparationRadius;
	if (NearestDistance >= ActiveRadius)
	{
		ClearSeparation();
		return;
	}

	FVector MoveDirection = WeightedAway.GetSafeNormal2D();
	if (MoveDirection.IsNearlyZero())
	{
		MoveDirection = NearestAway.GetSafeNormal2D();
	}
	if (MoveDirection.IsNearlyZero())
	{
		ClearSeparation();
		return;
	}

	const float PenetrationCorrection = FMath::Max(
		0.f, SeparationRadius - NearestDistance);
	const FVector DesiredLocation = Enemy->GetActorLocation() +
		MoveDirection * (SeparationMoveDistance + PenetrationCorrection);

	FNavLocation ProjectedLocation;
	UNavigationSystemV1* Navigation =
		FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!Navigation || !Navigation->ProjectPointToNavigation(
		DesiredLocation, ProjectedLocation, NavigationProjectionExtent))
	{
		ClearSeparation();
		return;
	}

	Blackboard->SetValueAsVector(
		SeparationLocationKey.SelectedKeyName, ProjectedLocation.Location);
	Blackboard->SetValueAsBool(
		NeedsSeparationKey.SelectedKeyName, true);
}

UBTService_UpdateMeleeCombat::UBTService_UpdateMeleeCombat()
{
	NodeName = TEXT("Update Melee Combat Context");
	Interval = 0.15f;
	RandomDeviation = 0.03f;
	TargetActorKey.SelectedKeyName = MeleeAIKeys::TargetActor;
	DistanceToTargetKey.SelectedKeyName = MeleeAIKeys::DistanceToTarget;
	HasLineOfSightKey.SelectedKeyName = MeleeAIKeys::HasLineOfSight;
	IsInAttackRangeKey.SelectedKeyName = MeleeAIKeys::IsInAttackRange;
	IsTooCloseKey.SelectedKeyName = MeleeAIKeys::IsTooClose;
}

void UBTService_UpdateMeleeCombat::TickNode(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory,
	float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* Controller = OwnerComp.GetAIOwner();
	APawn* EnemyPawn = Controller ? Controller->GetPawn() : nullptr;
	AActor* TargetActor = Blackboard
		? Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName))
		: nullptr;
	if (!Blackboard || !EnemyPawn || !TargetActor)
	{
		return;
	}

	const float Distance = FVector::Dist2D(
		EnemyPawn->GetActorLocation(), TargetActor->GetActorLocation());
	Blackboard->SetValueAsFloat(DistanceToTargetKey.SelectedKeyName, Distance);
	Blackboard->SetValueAsBool(HasLineOfSightKey.SelectedKeyName,
		Controller->LineOfSightTo(TargetActor));
	Blackboard->SetValueAsBool(IsInAttackRangeKey.SelectedKeyName,
		Distance <= AttackDistance);
	Blackboard->SetValueAsBool(IsTooCloseKey.SelectedKeyName,
		Distance <= TooCloseDistance);
}

UBTTask_RequestCombatSlot::UBTTask_RequestCombatSlot()
{
	NodeName = TEXT("Request Combat Slot");
	TargetActorKey.SelectedKeyName = MeleeAIKeys::TargetActor;
	CombatSlotLocationKey.SelectedKeyName = MeleeAIKeys::CombatSlotLocation;
}

EBTNodeResult::Type UBTTask_RequestCombatSlot::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	APawn* EnemyPawn = Controller ? Controller->GetPawn() : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AActor* TargetActor = Blackboard
		? Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName))
		: nullptr;
	UWorld* World = EnemyPawn ? EnemyPawn->GetWorld() : nullptr;
	UMeleeAICombatSubsystem* Coordinator = World
		? World->GetSubsystem<UMeleeAICombatSubsystem>() : nullptr;
	if (!Blackboard || !EnemyPawn || !TargetActor || !Coordinator)
	{
		return EBTNodeResult::Failed;
	}

	const FVector SlotLocation = Coordinator->RequestCombatSlot(
		EnemyPawn, TargetActor, MinRadius, MaxRadius);
	Blackboard->SetValueAsVector(CombatSlotLocationKey.SelectedKeyName, SlotLocation);
	return EBTNodeResult::Succeeded;
}

UBTTask_RequestAttackToken::UBTTask_RequestAttackToken()
{
	NodeName = TEXT("Request Attack Token");
	HasAttackTokenKey.SelectedKeyName = MeleeAIKeys::HasAttackToken;
}

EBTNodeResult::Type UBTTask_RequestAttackToken::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	APawn* EnemyPawn = Controller ? Controller->GetPawn() : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	UWorld* World = EnemyPawn ? EnemyPawn->GetWorld() : nullptr;
	UMeleeAICombatSubsystem* Coordinator = World
		? World->GetSubsystem<UMeleeAICombatSubsystem>() : nullptr;
	if (!Blackboard || !EnemyPawn || !Coordinator)
	{
		return EBTNodeResult::Failed;
	}

	const bool bGranted = Coordinator->RequestAttackToken(EnemyPawn);
	Blackboard->SetValueAsBool(HasAttackTokenKey.SelectedKeyName, bGranted);
	return bGranted ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}

UBTTask_ReleaseAttackToken::UBTTask_ReleaseAttackToken()
{
	NodeName = TEXT("Release Attack Token");
	HasAttackTokenKey.SelectedKeyName = MeleeAIKeys::HasAttackToken;
}

EBTNodeResult::Type UBTTask_ReleaseAttackToken::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	APawn* EnemyPawn = Controller ? Controller->GetPawn() : nullptr;
	if (EnemyPawn && EnemyPawn->GetWorld())
	{
		if (UMeleeAICombatSubsystem* Coordinator =
			EnemyPawn->GetWorld()->GetSubsystem<UMeleeAICombatSubsystem>())
		{
			Coordinator->ReleaseAttackToken(EnemyPawn);
		}
	}

	if (UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent())
	{
		Blackboard->SetValueAsBool(HasAttackTokenKey.SelectedKeyName, false);
	}
	return EBTNodeResult::Succeeded;
}

UBTTask_PerformMeleeAttack::UBTTask_PerformMeleeAttack()
{
	NodeName = TEXT("Perform Melee Attack");
	bNotifyTick = true;
	bCreateNodeInstance = true;
	TargetActorKey.SelectedKeyName = MeleeAIKeys::TargetActor;
	HasAttackTokenKey.SelectedKeyName = MeleeAIKeys::HasAttackToken;
}

EBTNodeResult::Type UBTTask_PerformMeleeAttack::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	APawn* EnemyPawn = Controller ? Controller->GetPawn() : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AActor* TargetActor = Blackboard
		? Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName))
		: nullptr;
	if (!EnemyPawn || !TargetActor || !Blackboard ||
		!Blackboard->GetValueAsBool(HasAttackTokenKey.SelectedKeyName))
	{
		return EBTNodeResult::Failed;
	}

	const float ActualDistance = FVector::Dist2D(
		EnemyPawn->GetActorLocation(), TargetActor->GetActorLocation());
	if (ActualDistance > MaximumAttackDistance)
	{
		ReleaseToken(OwnerComp);
		return EBTNodeResult::Failed;
	}

	// Character movement normally owns enemy yaw. Stop the path and face the
	// target explicitly so RotateToFace does not fight bOrientRotationToMovement.
	Controller->StopMovement();
	
	ActiveCombatComponent = EnemyPawn->FindComponentByClass<UCombatComponent>();
	if (!ActiveCombatComponent.IsValid())
	{
		ReleaseToken(OwnerComp);
		return EBTNodeResult::Failed;
	}

	ActiveCombatComponent->TryAttack();
	if (!ActiveCombatComponent->IsMeleeAttackInProgress())
	{
		ReleaseToken(OwnerComp);
		return EBTNodeResult::Succeeded;
	}
	return EBTNodeResult::InProgress;
}

void UBTTask_PerformMeleeAttack::TickTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory,
	float DeltaSeconds)
{
	if (!ActiveCombatComponent.IsValid() ||
		!ActiveCombatComponent->IsMeleeAttackInProgress())
	{
		ReleaseToken(OwnerComp);
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UBTTask_PerformMeleeAttack::AbortTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory)
{
	ReleaseToken(OwnerComp);
	return Super::AbortTask(OwnerComp, NodeMemory);
}

void UBTTask_PerformMeleeAttack::ReleaseToken(UBehaviorTreeComponent& OwnerComp) const
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	APawn* EnemyPawn = Controller ? Controller->GetPawn() : nullptr;
	if (EnemyPawn && EnemyPawn->GetWorld())
	{
		if (UMeleeAICombatSubsystem* Coordinator =
			EnemyPawn->GetWorld()->GetSubsystem<UMeleeAICombatSubsystem>())
		{
			Coordinator->ReleaseAttackToken(EnemyPawn);
		}
	}
	if (UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent())
	{
		Blackboard->SetValueAsBool(HasAttackTokenKey.SelectedKeyName, false);
	}
	if (Controller)
	{
		Controller->ClearFocus(EAIFocusPriority::Gameplay);
	}
}

UBTTask_SelectRetreatPosition::UBTTask_SelectRetreatPosition()
{
	NodeName = TEXT("Select Retreat Position");
	TargetActorKey.SelectedKeyName = MeleeAIKeys::TargetActor;
	RetreatLocationKey.SelectedKeyName = MeleeAIKeys::RetreatLocation;
}

EBTNodeResult::Type UBTTask_SelectRetreatPosition::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	APawn* EnemyPawn = Controller ? Controller->GetPawn() : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AActor* TargetActor = Blackboard
		? Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName))
		: nullptr;
	if (!EnemyPawn || !TargetActor || !Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	FVector Away = (EnemyPawn->GetActorLocation() -
		TargetActor->GetActorLocation()).GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = -TargetActor->GetActorForwardVector().GetSafeNormal2D();
	}
	const FVector DesiredLocation = EnemyPawn->GetActorLocation() + Away * RetreatDistance;
	FNavLocation ProjectedLocation;
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(
		EnemyPawn->GetWorld());
	const FVector Result = Navigation && Navigation->ProjectPointToNavigation(
		DesiredLocation, ProjectedLocation)
		? ProjectedLocation.Location : DesiredLocation;
	Blackboard->SetValueAsVector(RetreatLocationKey.SelectedKeyName, Result);
	return EBTNodeResult::Succeeded;
}

UBTTask_SetCurrentPatrolPoint::UBTTask_SetCurrentPatrolPoint()
{
	NodeName = TEXT("Set Current Patrol Point");
	PatrolTargetKey.SelectedKeyName = MeleeAIKeys::PatrolTarget;
}

EBTNodeResult::Type UBTTask_SetCurrentPatrolPoint::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	AEnemyCharacter* Enemy = Controller
		? Cast<AEnemyCharacter>(Controller->GetPawn()) : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Enemy || !Blackboard)
	{
		return EBTNodeResult::Failed;
	}

	if (bAdvanceFirst)
	{
		Enemy->AdvancePatrolPoint();
	}
	AActor* PatrolTarget = Enemy->GetCurrentPatrolPoint();
	Blackboard->SetValueAsObject(PatrolTargetKey.SelectedKeyName, PatrolTarget);
	return PatrolTarget ? EBTNodeResult::Succeeded : EBTNodeResult::Failed;
}

UBTTask_ClearBlackboardValue::UBTTask_ClearBlackboardValue()
{
	NodeName = TEXT("Clear Blackboard Value");
	KeyToClear.SelectedKeyName = MeleeAIKeys::LastKnownLocation;
}

EBTNodeResult::Type UBTTask_ClearBlackboardValue::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard || KeyToClear.SelectedKeyName.IsNone())
	{
		return EBTNodeResult::Failed;
	}

	Blackboard->ClearValue(KeyToClear.SelectedKeyName);
	return EBTNodeResult::Succeeded;
}
