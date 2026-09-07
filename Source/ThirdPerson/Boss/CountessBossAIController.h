#pragma once
#include "CoreMinimal.h"
#include "../AI/EnemyAIController.h"
#include "CountessBossAIController.generated.h"
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
