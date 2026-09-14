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

/** 远程战斗黑板更新：根据距离、视线和角色状态决定射击、后撤或追击，不在服务中直接结算伤害。 */
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
	UPROPERTY(EditAnywhere, Category = "Blackboard") FBlackboardKeySelector ShouldApproachTargetKey;
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0")) float MinimumAttackDistance = 180.f;
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0")) float MaximumAttackDistance = 950.f;
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0")) float RetreatTriggerDistance = 300.f;
    /** Hysteresis: remain in retreat until outside this distance; avoids stepping back and forth. */
    UPROPERTY(EditAnywhere, Category="Combat", meta=(ClampMin="0.0")) float RetreatStopDistance = 440.f;
	/** Backpedalling is slower than the player's ordinary 450 cm/s run. */
	UPROPERTY(EditAnywhere, Category = "Combat|Movement", meta = (ClampMin = "0.0", Units = "cm/s")) float RetreatSpeed = 260.f;
	UPROPERTY(EditAnywhere, Category = "Combat|Movement", meta = (ClampMin = "0.0", Units = "cm/s")) float ApproachSpeed = 360.f;
	UPROPERTY(EditAnywhere, Category = "Combat|Movement", meta = (ClampMin = "0.0", Units = "cm/s")) float PatrolSpeed = 220.f;
	UPROPERTY(EditAnywhere, Category = "Combat|Retreat", meta = (ClampMin = "0.0", Units = "s")) float RetreatReactionDelay = 0.2f;
	UPROPERTY(EditAnywhere, Category = "Combat|Retreat", meta = (ClampMin = "0.1", Units = "s")) float MaximumRetreatDuration = 1.f;
	/** A committed pause between backsteps, even when the player keeps pressing. */
	UPROPERTY(EditAnywhere, Category = "Combat|Retreat", meta = (ClampMin = "0.1", Units = "s")) float RetreatCooldown = 1.4f;

private:
	friend struct FRangedAITestAccess;
	void ApplyMovementSpeed(ACharacter* Character, float Speed);
	void RestoreMovementSpeed();
	TWeakObjectPtr<ACharacter> MovementPawn;
	TWeakObjectPtr<AActor> SpacingTarget;
	float SavedMovementSpeed = 0.f;
	float AppliedMovementSpeed = 0.f;
	double CloseSince = -1.;
	double RetreatStartedAt = 0.;
	double NextRetreatAllowedAt = 0.;
	bool bRetreating = false;
};

/** 在导航可达区域选择远离目标的后撤点，为弓手保留射击距离。 */
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
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0")) float RetreatDistance = 240.f;
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0", ClampMax = "1.0")) float SideStepAmount = 0.3f;
};

/** 驱动一次远程攻击并等待动作结束；真正放箭由动画通知交给战斗组件处理。 */
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

/** 为远程敌人选择可达巡逻点，未发现目标时使用。 */
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
