#include "BossBehaviorNodes.h"
#include "CountessBossAIController.h"
#include "CountessBossCharacter.h"
#include "BossActionComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
UBTService_UpdateBossContext::UBTService_UpdateBossContext()
{ NodeName=TEXT("Update Boss Context (0.1s)"); Interval=.1f; RandomDeviation=0; INIT_SERVICE_NODE_NOTIFY_FLAGS(); bCallTickOnSearchStart=true; }
void UBTService_UpdateBossContext::TickNode(UBehaviorTreeComponent& Owner,uint8* Memory,float Delta)
{
 Super::TickNode(Owner,Memory,Delta);
 if (auto* AI=Cast<ACountessBossAIController>(Owner.GetAIOwner())) AI->UpdateBossBlackboard(Delta);
}
UBTTask_BossState::UBTTask_BossState()
{ NodeName=TEXT("Boss State / Combat Decision"); bNotifyTick=true; }
EBTNodeResult::Type UBTTask_BossState::ExecuteTask(UBehaviorTreeComponent& Owner,uint8* Memory)
{
 const auto* Boss=Owner.GetAIOwner()?Cast<ACountessBossCharacter>(Owner.GetAIOwner()->GetPawn()):nullptr;
 return Boss && Boss->BossActions->State==RequiredState?EBTNodeResult::InProgress:EBTNodeResult::Failed;
}
void UBTTask_BossState::TickTask(UBehaviorTreeComponent& Owner,uint8* Memory,float Delta)
{
 const auto* Boss=Owner.GetAIOwner()?Cast<ACountessBossCharacter>(Owner.GetAIOwner()->GetPawn()):nullptr;
 if (!Boss || Boss->BossActions->State!=RequiredState) { FinishLatentTask(Owner,EBTNodeResult::Succeeded); return; }
 if (RequiredState==EBossState::Combat) Boss->BossActions->DriveDecision();
}
