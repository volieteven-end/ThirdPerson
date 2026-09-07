#include "CountessBossAIController.h"
#include "CountessBossCharacter.h"
#include "BossActionComponent.h"
#include "BossBehaviorNodes.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BrainComponent.h"
ACountessBossAIController::ACountessBossAIController()
{ bAllowStrafe=true; SightRadius=900; LoseSightRadius=1800; }
void ACountessBossAIController::Tick(float Delta)
{ AAIController::Tick(Delta); } // Deliberately skip the small-enemy manual TryAttack fallback.
UBehaviorTree* ACountessBossAIController::CreateDefaultBossTree(UObject* Outer)
{
 auto* Tree=NewObject<UBehaviorTree>(Outer,TEXT("BT_CountessBoss_Native"));
 auto* BB=NewObject<UBlackboardData>(Tree,TEXT("BB_CountessBoss_Native")); Tree->BlackboardAsset=BB;
 auto Add=[BB](FName Name,UClass* Type)
 { FBlackboardEntry E; E.EntryName=Name; E.KeyType=NewObject<UBlackboardKeyType>(BB,Type); BB->Keys.Add(E); };
 Add(TEXT("TargetActor"),UBlackboardKeyType_Object::StaticClass());
 for (FName Name:{FName(TEXT("HomeLocation")),FName(TEXT("LastKnownLocation")),FName(TEXT("MoveGoal"))}) Add(Name,UBlackboardKeyType_Vector::StaticClass());
 for (FName Name:{FName(TEXT("bHasLineOfSight")),FName(TEXT("bIsDead")),FName(TEXT("bIsStunned")),FName(TEXT("bEncounterActive")),FName(TEXT("bActionActive")),FName(TEXT("bPhasePending")),FName(TEXT("bResetRequested")),FName(TEXT("bHasAttackToken"))}) Add(Name,UBlackboardKeyType_Bool::StaticClass());
 Add(TEXT("BossPhase"),UBlackboardKeyType_Int::StaticClass()); Add(TEXT("SelectedAction"),UBlackboardKeyType_Int::StaticClass());
 auto* Root=NewObject<UBTComposite_Selector>(Tree,TEXT("BossPriority")); Tree->RootNode=Root;
 Root->Services.Add(NewObject<UBTService_UpdateBossContext>(Root));
 for (EBossState State:{EBossState::Dead,EBossState::Resetting,EBossState::Dormant,EBossState::Intro,EBossState::PoiseBroken,EBossState::PhaseTransition,EBossState::Action,EBossState::Combat})
 {
  auto* Task=NewObject<UBTTask_BossState>(Root); Task->RequiredState=State;
  FBTCompositeChild Child; Child.ChildTask=Task; Root->Children.Add(Child);
 }
 return Tree;
}
void ACountessBossAIController::OnPossess(APawn* InPawn)
{
 if (auto* Boss=Cast<ACountessBossCharacter>(InPawn)) BehaviorTreeAsset=Boss->BossActions->GetDefinition()->BehaviorTree;
 if (!BehaviorTreeAsset) BehaviorTreeAsset=CreateDefaultBossTree(this);
 Super::OnPossess(InPawn);
}
void ACountessBossAIController::OnUnPossess()
{
 if (auto* Boss=Cast<ACountessBossCharacter>(GetPawn())) Boss->BossActions->CancelAction();
 Super::OnUnPossess();
}
void ACountessBossAIController::HandleTargetPerceptionUpdated(AActor* Actor,FAIStimulus Stimulus)
{
 // Preserve target during the five-second LOS grace. Never use the base immediate-clear path.
 if (auto* Boss=Cast<ACountessBossCharacter>(GetPawn()); Boss && Stimulus.WasSuccessfullySensed() && Actor)
 {
  if (FVector::DistSquared2D(Actor->GetActorLocation(),Boss->GetActorLocation())<=FMath::Square(Boss->BossActions->GetDefinition()->EngageRadius)) Boss->BossActions->BeginEncounter(Actor);
 }
}
void ACountessBossAIController::UpdateBossBlackboard(float Delta)
{
 auto* Boss=Cast<ACountessBossCharacter>(GetPawn()); auto* BB=GetBlackboardComponent(); if (!Boss || !BB) return;
 auto* A=Boss->BossActions.Get(); A->UpdateContext(Delta);
 BB->SetValueAsObject(TEXT("TargetActor"),A->GetTarget()); BB->SetValueAsVector(TEXT("HomeLocation"),A->GetHomeLocation());
 BB->SetValueAsVector(TEXT("LastKnownLocation"),A->GetLastKnownLocation()); BB->SetValueAsInt(TEXT("BossPhase"),A->Phase);
 BB->SetValueAsInt(TEXT("SelectedAction"),static_cast<int32>(A->CurrentAction));
 BB->SetValueAsBool(TEXT("bHasLineOfSight"),A->HasTargetLOS()); BB->SetValueAsBool(TEXT("bIsDead"),A->State==EBossState::Dead);
 BB->SetValueAsBool(TEXT("bIsStunned"),A->State==EBossState::PoiseBroken); BB->SetValueAsBool(TEXT("bEncounterActive"),A->IsEncounterActive());
 BB->SetValueAsBool(TEXT("bActionActive"),A->IsActionActive()); BB->SetValueAsBool(TEXT("bPhasePending"),A->bPhasePending);
 BB->SetValueAsBool(TEXT("bResetRequested"),A->State==EBossState::Resetting);
}
