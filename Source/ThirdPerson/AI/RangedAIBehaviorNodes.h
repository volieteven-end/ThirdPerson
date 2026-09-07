#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "RangedAIBehaviorNodes.generated.h"

class UCombatComponent;
class ACharacter;
class AAIController;

/** Updates the distance bands used by a ranged-enemy Behavior Tree. */
UCLASS()
class THIRDPERSON_API UBTService_UpdateRangedCombat : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_UpdateRangedCombat();

protected:
    virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    void RestoreFacing();
    TWeakObjectPtr<ACharacter> FacingPawn;
    TWeakObjectPtr<AAIController> FacingController;
    TWeakObjectPtr<AActor> FacingTarget;
    bool bSavedOrient = false;
    bool bSavedDesired = false;
    bool bSavedControllerYaw = false;
	virtual void TickNode(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory,
		float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector TargetActorKey;
	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector DistanceToTargetKey;
	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector HasLineOfSightKey;
	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector IsInRangedAttackRangeKey;
	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector IsTooCloseKey;
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0")) float MinimumAttackDistance = 450.f;
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0")) float MaximumAttackDistance = 950.f;
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0")) float RetreatTriggerDistance = 450.f;
    /** Hysteresis: remain in retreat until outside this distance; avoids stepping back and forth. */
    UPROPERTY(EditAnywhere, Category="Combat", meta=(ClampMin="0.0")) float RetreatStopDistance = 550.f;
};

/** Selects a navigable point away from the player with a small side-step. */
UCLASS()
class THIRDPERSON_API UBTTask_SelectRangedRetreatPosition : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_SelectRangedRetreatPosition();

protected:
	virtual EBTNodeResult::Type ExecuteTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector TargetActorKey;
	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector RetreatLocationKey;
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0")) float RetreatDistance = 500.f;
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0", ClampMax = "1.0")) float SideStepAmount = 0.3f;
};

/** Starts a ranged montage and waits until its projectile-release cycle finishes. */
UCLASS()
class THIRDPERSON_API UBTTask_PerformRangedAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_PerformRangedAttack();

protected:
	virtual EBTNodeResult::Type ExecuteTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;
	virtual void TickTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory,
		float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector TargetActorKey;
	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector HasLineOfSightKey;
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0")) float MaximumAttackDistance = 1000.f;

private:
	TWeakObjectPtr<UCombatComponent> ActiveCombatComponent;
};

/** PatrolPoints takes precedence; otherwise choose a reachable point inside the home radius. */
UCLASS()
class THIRDPERSON_API UBTTask_SelectRangedPatrolLocation : public UBTTaskNode
{
 GENERATED_BODY()
public:
 UBTTask_SelectRangedPatrolLocation();
protected:
 virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
 UPROPERTY(EditAnywhere, Category="Blackboard") FBlackboardKeySelector HomeLocationKey;
 UPROPERTY(EditAnywhere, Category="Blackboard") FBlackboardKeySelector PatrolLocationKey;
};
