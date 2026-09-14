
#pragma once

#include "CoreMinimal.h"
#include "Runtime/AIModule/Classes/AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "EnemyAIController.generated.h"

class UBehaviorTree;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;

/** 普通敌人的感知与行为树入口，维护目标和巡逻状态；近战与远程行为由对应节点实现。 */
UCLASS()
class THIRDPERSON_API AEnemyAIController
	: public AAIController
{
	GENERATED_BODY()

public:
	AEnemyAIController();
	bool bIsPatrolling = false;
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UFUNCTION()
	virtual void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UPROPERTY(EditDefaultsOnly, Category = "AI|Behavior Tree")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	TObjectPtr<UAIPerceptionComponent> EnemyPerception;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception")
	float SightRadius = 900.f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception")
	float LoseSightRadius = 1200.f;
	/** Engaged melee enemies tolerate a brief occlusion or a turn away from the target. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception", meta = (ClampMin = "0.1"))
	float MeleeTargetMemorySeconds = 0.75f;

	UPROPERTY(EditDefaultsOnly, Category = "AI")
	float ChaseDistance = 800.f;

	UPROPERTY(EditDefaultsOnly, Category = "AI")
	float StopDistance = 150.f;
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	float AttackDistance = 150.f;
	void TryAttack();
private:
	friend struct FMeleeAITestAccess;
	bool UsesMeleeTargetMemory() const;
	void UpdateRememberedTarget();
	void ClearCombatTarget();
	FTimerHandle TargetMemoryTimer;
	TWeakObjectPtr<AActor> RememberedTarget;
	float LastTargetContactTime = 0.f;
	bool bIsChasing = false;
	bool bUsingBehaviorTree = false;
};
