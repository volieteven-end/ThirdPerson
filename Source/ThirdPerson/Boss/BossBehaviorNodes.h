#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BTService.h"
#include "BossDefinition.h"
#include "BossBehaviorNodes.generated.h"

/** 更新 Boss 行为树上下文，将遭遇状态和目标信息交给状态任务。 */
UCLASS()
class THIRDPERSON_API UBTService_UpdateBossContext : public UBTService
{
 GENERATED_BODY()
public:
 UBTService_UpdateBossContext();
protected:
 virtual void TickNode(UBehaviorTreeComponent& Owner,uint8* Memory,float Delta) override;
};

/** 驱动 Boss 当前状态对应的行为；具体招式、对峙和重置由 Boss 动作组件统一调度。 */
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
