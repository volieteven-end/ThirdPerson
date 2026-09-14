#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "MeleeAIBehaviorNodes.generated.h"

class UCombatComponent;
class ACharacter;
class AAIController;

/** 更新敌人间的分离信息，缓解多个近战敌人追击同一目标时的重叠。 */
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

/** 刷新近战 AI 的目标、距离和攻击条件，供行为树分支决策使用。 */
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
	/** Keep retreat selected until this far away; attack motion never requests retreat. */
	UPROPERTY(EditAnywhere, Category = "Combat") float TooCloseReleaseDistance = 130.f;
};

/** 在行为树分支存续期间维护攻击预约；分支退出时释放占用，避免令牌泄漏。 */
UCLASS()
class THIRDPERSON_API UBTService_MeleeAttackReservation : public UBTService
{
	GENERATED_BODY()
public:
	UBTService_MeleeAttackReservation();
protected:
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
private:
	friend struct FMeleeAITestAccess;
	TWeakObjectPtr<APawn> ReservedPawn;
};

/** 向世界级近战协调器申请接近目标的位置，避免所有敌人挤向同一点。 */
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

/** 申请同时进攻的名额；成功后才进入攻击分支，不直接触发伤害。 */
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

/** 归还攻击名额，让其他敌人可以发起进攻。 */
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

/** 发起近战动作并等待完成；中止时清理任务状态，攻击窗口仍由战斗组件管理。 */
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
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "1.0", ClampMax = "45.0"))
	float AttackFacingTolerance = 10.f;
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.1"))
	float MaximumFacingTime = 1.f;

private:
	friend struct FMeleeAITestAccess;
	EBTNodeResult::Type TryStartAttack(UBehaviorTreeComponent& OwnerComp);
	void ReleaseToken(UBehaviorTreeComponent& OwnerComp);
	void RestoreFacing();
	TWeakObjectPtr<UCombatComponent> ActiveCombatComponent;
	TWeakObjectPtr<ACharacter> AttackPawn;
	TWeakObjectPtr<AAIController> AttackController;
	TWeakObjectPtr<AActor> AttackTarget;
	TWeakObjectPtr<AActor> PreviousFocusActor;
	FVector PreviousFocusLocation = FVector::ZeroVector;
	float FacingStartedAt = 0.f;
	bool bAttackStarted = false;
	bool bFacingPolicySaved = false;
	bool bPreviousOrientToMovement = false;
	bool bPreviousControllerDesiredRotation = false;
	bool bPreviousControllerYaw = false;
};

/** 选择近战收招后的可达退让点，为下一轮交战留出空间。 */
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

/** 把当前巡逻目标写入黑板，衔接行为树的移动任务。 */
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

/** 清除指定黑板键，避免上一行为分支的数据影响下一轮决策。 */
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
