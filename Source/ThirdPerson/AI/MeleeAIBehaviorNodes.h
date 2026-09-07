#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "MeleeAIBehaviorNodes.generated.h"

class UCombatComponent;

/**
 * Periodically writes a nearby-enemy avoidance destination to the Blackboard.
 * Put a high-priority separation branch above chase/slot movement so crowded
 * melee enemies spread out without moving during an attack montage.
 */
UCLASS()
class THIRDPERSON_API UBTService_UpdateEnemySeparation : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_UpdateEnemySeparation();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory,
		float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector NeedsSeparationKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector SeparationLocationKey;

	/** Start separating when another living enemy is closer than this. */
	UPROPERTY(EditAnywhere, Category = "Separation", meta = (ClampMin = "1.0"))
	float SeparationRadius = 135.f;

	/** Hysteresis radius; prevents rapid true/false changes near the boundary. */
	UPROPERTY(EditAnywhere, Category = "Separation", meta = (ClampMin = "1.0"))
	float SeparationReleaseRadius = 165.f;

	/** Distance of the temporary navigation destination away from the crowd. */
	UPROPERTY(EditAnywhere, Category = "Separation", meta = (ClampMin = "1.0"))
	float SeparationMoveDistance = 110.f;

	UPROPERTY(EditAnywhere, Category = "Separation")
	FVector NavigationProjectionExtent = FVector(120.f, 120.f, 220.f);

	/** An active attack owns the enemy's position and must not be interrupted. */
	UPROPERTY(EditAnywhere, Category = "Separation")
	bool bIgnoreWhileAttacking = true;
};

UCLASS()
class THIRDPERSON_API UBTService_UpdateMeleeCombat : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_UpdateMeleeCombat();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory,
		float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector TargetActorKey;
	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector DistanceToTargetKey;
	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector HasLineOfSightKey;
	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector IsInAttackRangeKey;
	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector IsTooCloseKey;
	UPROPERTY(EditAnywhere, Category = "Combat") float AttackDistance = 165.f;
	UPROPERTY(EditAnywhere, Category = "Combat") float TooCloseDistance = 95.f;
};

UCLASS()
class THIRDPERSON_API UBTTask_RequestCombatSlot : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_RequestCombatSlot();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector TargetActorKey;
	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector CombatSlotLocationKey;
	UPROPERTY(EditAnywhere, Category = "Combat") float MinRadius = 220.f;
	UPROPERTY(EditAnywhere, Category = "Combat") float MaxRadius = 300.f;
};

UCLASS()
class THIRDPERSON_API UBTTask_RequestAttackToken : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_RequestAttackToken();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector HasAttackTokenKey;
};

UCLASS()
class THIRDPERSON_API UBTTask_ReleaseAttackToken : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_ReleaseAttackToken();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector HasAttackTokenKey;
};

UCLASS()
class THIRDPERSON_API UBTTask_PerformMeleeAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_PerformMeleeAttack();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory,
		float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector TargetActorKey;
	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector HasAttackTokenKey;
	/** Final safety check; an attack never starts beyond this center-to-center distance. */
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0"))
	float MaximumAttackDistance = 180.f;

private:
	void ReleaseToken(UBehaviorTreeComponent& OwnerComp) const;
	TWeakObjectPtr<UCombatComponent> ActiveCombatComponent;
};

UCLASS()
class THIRDPERSON_API UBTTask_SelectRetreatPosition : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_SelectRetreatPosition();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector TargetActorKey;
	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector RetreatLocationKey;
	UPROPERTY(EditAnywhere, Category = "Combat") float RetreatDistance = 180.f;
};

UCLASS()
class THIRDPERSON_API UBTTask_SetCurrentPatrolPoint : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_SetCurrentPatrolPoint();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector PatrolTargetKey;
	UPROPERTY(EditAnywhere, Category = "Patrol") bool bAdvanceFirst = false;
};

/** Clears one selected Blackboard entry after an investigate/search branch. */
UCLASS()
class THIRDPERSON_API UBTTask_ClearBlackboardValue : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_ClearBlackboardValue();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector KeyToClear;
};
