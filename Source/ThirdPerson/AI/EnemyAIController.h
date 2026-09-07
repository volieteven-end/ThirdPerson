// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/AIModule/Classes/AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "EnemyAIController.generated.h"

class UBehaviorTree;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;

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
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

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

	UPROPERTY(EditDefaultsOnly, Category = "AI")
	float ChaseDistance = 800.f;

	UPROPERTY(EditDefaultsOnly, Category = "AI")
	float StopDistance = 150.f;
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	float AttackDistance = 150.f;
	void TryAttack();
private:
	bool bIsChasing = false;
	bool bUsingBehaviorTree = false;
};
