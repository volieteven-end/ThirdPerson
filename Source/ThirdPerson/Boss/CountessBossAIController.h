#pragma once
#include "CoreMinimal.h"
#include "../AI/EnemyAIController.h"
#include "CountessBossAIController.generated.h"
/** Countess 的感知与行为树控制器，复用敌人目标管理，并接入 Boss 专用状态决策。 */
UCLASS()
class THIRDPERSON_API ACountessBossAIController : public AEnemyAIController
{
 GENERATED_BODY()
public:
 ACountessBossAIController();
 virtual void Tick(float Delta) override;
 void UpdateBossBlackboard(float Delta);
 static UBehaviorTree* CreateDefaultBossTree(UObject* Outer);
protected:
 virtual void OnPossess(APawn* Pawn) override;
 virtual void OnUnPossess() override;
 virtual void HandleTargetPerceptionUpdated(AActor* Actor,FAIStimulus Stimulus) override;
};
