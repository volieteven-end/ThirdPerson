#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BTService.h"
#include "BossDefinition.h"
#include "BossBehaviorNodes.generated.h"

UCLASS()
class THIRDPERSON_API UBTService_UpdateBossContext : public UBTService
{
 GENERATED_BODY()
public:
 UBTService_UpdateBossContext();
protected:
 virtual void TickNode(UBehaviorTreeComponent& Owner,uint8* Memory,float Delta) override;
};

/** One state branch. A changed state completes on the next decision tick, then the priority selector reevaluates. */
UCLASS()
class THIRDPERSON_API UBTTask_BossState : public UBTTaskNode
{
 GENERATED_BODY()
public:
 UBTTask_BossState();
 UPROPERTY(EditAnywhere, Category="Boss") EBossState RequiredState=EBossState::Combat;
protected:
 virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& Owner,uint8* Memory) override;
 virtual void TickTask(UBehaviorTreeComponent& Owner,uint8* Memory,float Delta) override;
};
