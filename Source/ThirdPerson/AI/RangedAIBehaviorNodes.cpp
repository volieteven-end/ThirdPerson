#include "RangedAIBehaviorNodes.h"

#include "../Components/CombatComponent.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "EnemyCharacter.h"
#include "../Components/HealthComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace RangedAIKeys
{
	static const FName TargetActor(TEXT("TargetActor"));
	static const FName DistanceToTarget(TEXT("DistanceToTarget"));
	static const FName HasLineOfSight(TEXT("bHasLineOfSight"));
	static const FName IsInAttackRange(TEXT("bIsInRangedAttackRange"));
	static const FName IsTooClose(TEXT("bIsTooClose"));
	static const FName RetreatLocation(TEXT("RetreatLocation"));
}

UBTService_UpdateRangedCombat::UBTService_UpdateRangedCombat()
{
	NodeName = TEXT("Update Ranged Combat Context");
    bCreateNodeInstance = true;
    bNotifyCeaseRelevant = true;
    bCallTickOnSearchStart = true;
	Interval = 0.15f;
	RandomDeviation = 0.03f;
	TargetActorKey.SelectedKeyName = RangedAIKeys::TargetActor;
	DistanceToTargetKey.SelectedKeyName = RangedAIKeys::DistanceToTarget;
	HasLineOfSightKey.SelectedKeyName = RangedAIKeys::HasLineOfSight;
	IsInRangedAttackRangeKey.SelectedKeyName = RangedAIKeys::IsInAttackRange;
	IsTooCloseKey.SelectedKeyName = RangedAIKeys::IsTooClose;
}

void UBTService_UpdateRangedCombat::TickNode(
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
    if (IsValid(TargetActor))
    {
        const UHealthComponent* Health = TargetActor->FindComponentByClass<UHealthComponent>();
        if (Health && Health->CurrentHealth <= 0.f)
        {
            Blackboard->ClearValue(TargetActorKey.SelectedKeyName);
            TargetActor = nullptr;
        }
    }
    const bool bDisabled = Blackboard && (Blackboard->GetValueAsBool(TEXT("bIsDead")) || Blackboard->GetValueAsBool(TEXT("bIsStunned")));
    if (!Blackboard || !EnemyPawn || !IsValid(TargetActor) || bDisabled)
    {
        RestoreFacing();
        if (Blackboard)
        {
            Blackboard->SetValueAsBool(IsInRangedAttackRangeKey.SelectedKeyName, false);
            Blackboard->SetValueAsBool(IsTooCloseKey.SelectedKeyName, false);
            Blackboard->SetValueAsBool(HasLineOfSightKey.SelectedKeyName, false);
            Blackboard->SetValueAsFloat(DistanceToTargetKey.SelectedKeyName, 0.f);
        }
        return;
    }
    if (ACharacter* Character = Cast<ACharacter>(EnemyPawn))
    {
        if (FacingPawn.Get() != Character)
        {
            RestoreFacing();
            FacingPawn = Character; FacingController = Controller;
            bSavedOrient = Character->GetCharacterMovement()->bOrientRotationToMovement;
            bSavedDesired = Character->GetCharacterMovement()->bUseControllerDesiredRotation;
            bSavedControllerYaw = Character->bUseControllerRotationYaw;
        }
        Character->bUseControllerRotationYaw = false;
        Character->GetCharacterMovement()->bOrientRotationToMovement = false;
        Character->GetCharacterMovement()->bUseControllerDesiredRotation = true;
        FacingTarget = TargetActor;
        Controller->SetFocus(TargetActor, EAIFocusPriority::Gameplay);
    }
    const float Distance = FVector::Dist2D(EnemyPawn->GetActorLocation(), TargetActor->GetActorLocation());
    const bool bSight = Controller->LineOfSightTo(TargetActor);
    const float Enter = FMath::Max(MinimumAttackDistance, RetreatTriggerDistance);
    const float Exit = FMath::Max(Enter + 50.f, RetreatStopDistance);
    const bool bRetreat = Distance < (Blackboard->GetValueAsBool(IsTooCloseKey.SelectedKeyName) ? Exit : Enter);
    Blackboard->SetValueAsFloat(DistanceToTargetKey.SelectedKeyName, Distance);
    Blackboard->SetValueAsBool(HasLineOfSightKey.SelectedKeyName, bSight);
    Blackboard->SetValueAsBool(IsTooCloseKey.SelectedKeyName, bRetreat);
    Blackboard->SetValueAsBool(IsInRangedAttackRangeKey.SelectedKeyName,
        !bRetreat && bSight && Distance >= MinimumAttackDistance && Distance <= MaximumAttackDistance);
}

void UBTService_UpdateRangedCombat::RestoreFacing()
{
    if (FacingController.IsValid() && FacingController->GetFocusActor() == FacingTarget.Get())
        FacingController->ClearFocus(EAIFocusPriority::Gameplay);
    if (FacingPawn.IsValid())
    {
        FacingPawn->bUseControllerRotationYaw = bSavedControllerYaw;
        FacingPawn->GetCharacterMovement()->bOrientRotationToMovement = bSavedOrient;
        FacingPawn->GetCharacterMovement()->bUseControllerDesiredRotation = bSavedDesired;
    }
    FacingPawn.Reset(); FacingController.Reset(); FacingTarget.Reset();
}
void UBTService_UpdateRangedCombat::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    RestoreFacing();
    Super::OnCeaseRelevant(OwnerComp, NodeMemory);
}

UBTTask_SelectRangedRetreatPosition::UBTTask_SelectRangedRetreatPosition()
{
	NodeName = TEXT("Select Ranged Retreat Position");
	TargetActorKey.SelectedKeyName = RangedAIKeys::TargetActor;
	RetreatLocationKey.SelectedKeyName = RangedAIKeys::RetreatLocation;
}

EBTNodeResult::Type UBTTask_SelectRangedRetreatPosition::ExecuteTask(
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
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(EnemyPawn->GetWorld());
    const FVector Start = EnemyPawn->GetActorLocation();
    const float OriginalDistance = FVector::Dist2D(Start, TargetActor->GetActorLocation());
    const float SideAngle = FMath::RadiansToDegrees(FMath::Atan(SideStepAmount));
    const float Sign = FMath::RandBool() ? 1.f : -1.f;
    // Try shorter / side-stepped paths near walls instead of retreating through geometry.
    if (Nav) for (float Scale : {1.f, .5f, .25f}) for (float Angle : {0.f, Sign*SideAngle, -Sign*SideAngle, 55.f, -55.f, 80.f, -80.f})
    {
        FNavLocation Point;
        const FVector Goal = Start + Away.RotateAngleAxis(Angle, FVector::UpVector) * RetreatDistance * Scale;
        if (!Nav->ProjectPointToNavigation(Goal, Point, FVector(80,80,200))) continue;
        if (FVector::Dist2D(Point.Location, TargetActor->GetActorLocation()) < OriginalDistance + 50.f) continue;
        UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(EnemyPawn, Start, Point.Location, EnemyPawn);
        if (Path && Path->IsValid() && !Path->IsPartial())
        {
            Blackboard->SetValueAsVector(RetreatLocationKey.SelectedKeyName, Point.Location);
            return EBTNodeResult::Succeeded;
        }
    }
    // Cornered: stand facing the player and retry after the branch's Wait, never fall through to Chase.
    Blackboard->SetValueAsVector(RetreatLocationKey.SelectedKeyName, Start);
    return EBTNodeResult::Succeeded;
}

UBTTask_PerformRangedAttack::UBTTask_PerformRangedAttack()
{
	NodeName = TEXT("Perform Ranged Attack");
	bNotifyTick = true;
	bCreateNodeInstance = true;
	TargetActorKey.SelectedKeyName = RangedAIKeys::TargetActor;
	HasLineOfSightKey.SelectedKeyName = RangedAIKeys::HasLineOfSight;
}

EBTNodeResult::Type UBTTask_PerformRangedAttack::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory)
{
	AAIController* Controller = OwnerComp.GetAIOwner();
	APawn* EnemyPawn = Controller ? Controller->GetPawn() : nullptr;
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AActor* TargetActor = Blackboard
		? Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName))
		: nullptr;
	if (!Controller || !EnemyPawn || !Blackboard || !TargetActor ||
		!Blackboard->GetValueAsBool(HasLineOfSightKey.SelectedKeyName) ||
		FVector::Dist2D(EnemyPawn->GetActorLocation(),
			TargetActor->GetActorLocation()) > MaximumAttackDistance)
	{
		return EBTNodeResult::Failed;
	}

	Controller->StopMovement();
	// UpdateRangedCombat owns facing, including movement and cooldowns.
	ActiveCombatComponent = EnemyPawn->FindComponentByClass<UCombatComponent>();
	if (!ActiveCombatComponent.IsValid() ||
		!ActiveCombatComponent->TryRangedAttackAt(TargetActor))
	{
		return EBTNodeResult::Failed;
	}

	return ActiveCombatComponent->IsRangedAttackInProgress()
		? EBTNodeResult::InProgress
		: EBTNodeResult::Succeeded;
}

void UBTTask_PerformRangedAttack::TickTask(
	UBehaviorTreeComponent& OwnerComp,
	uint8* NodeMemory,
	float DeltaSeconds)
{
	if (!ActiveCombatComponent.IsValid() ||
		!ActiveCombatComponent->IsRangedAttackInProgress())
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UBTTask_PerformRangedAttack::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    // A retreat / stun / death must not leave a pending release notify behind.
    if (ActiveCombatComponent.IsValid()) ActiveCombatComponent->CancelActiveAttack(.1f);
    ActiveCombatComponent.Reset();
    return EBTNodeResult::Aborted;
}

UBTTask_SelectRangedPatrolLocation::UBTTask_SelectRangedPatrolLocation()
{
    NodeName = TEXT("Select Ranged Patrol Location");
    HomeLocationKey.SelectedKeyName = TEXT("HomeLocation");
    PatrolLocationKey.SelectedKeyName = TEXT("PatrolLocation");
}
EBTNodeResult::Type UBTTask_SelectRangedPatrolLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    auto* Controller = OwnerComp.GetAIOwner();
    auto* Enemy = Controller ? Cast<AEnemyCharacter>(Controller->GetPawn()) : nullptr;
    auto* BB = OwnerComp.GetBlackboardComponent();
    if (!Enemy || !BB) return EBTNodeResult::Failed;
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Enemy->GetWorld());
    if (!Nav) return EBTNodeResult::Failed;
    FNavLocation Point;
    if (AActor* Patrol = Enemy->GetCurrentPatrolPoint())
    {
        Enemy->AdvancePatrolPoint();
        if (!Nav->ProjectPointToNavigation(Patrol->GetActorLocation(), Point, FVector(100,100,300))) return EBTNodeResult::Failed;
    }
    else
    {
        const FVector Home = BB->GetValueAsVector(HomeLocationKey.SelectedKeyName);
        FNavLocation Center;
        if (!Nav->ProjectPointToNavigation(Home, Center, FVector(150,150,400))) return EBTNodeResult::Failed;
        bool Found = false;
        for (int32 Attempt=0; Attempt<12; ++Attempt)
        {
            if (!Nav->GetRandomReachablePointInRadius(Center.Location, FMath::Max(100.f,Enemy->RangedPatrolRadius), Point)) continue;
            if (FVector::Dist2D(Enemy->GetActorLocation(),Point.Location) < 100.f) continue;
            auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(Enemy, Enemy->GetActorLocation(), Point.Location, Enemy);
            if (Path && Path->IsValid() && !Path->IsPartial()) {Found=true;break;}
        }
        if (!Found) return EBTNodeResult::Failed;
    }
    BB->SetValueAsVector(PatrolLocationKey.SelectedKeyName, Point.Location);
    return EBTNodeResult::Succeeded;
}

