#include "MeleeAIAssetTools.h"

#if WITH_EDITOR
#include "MeleeAIBehaviorNodes.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_Service.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#endif

bool UMeleeAIAssetTools::ConfigureMeleeBehaviorTree()
{
#if WITH_EDITOR
	auto* Tree = LoadObject<UBehaviorTree>(nullptr, TEXT("/Game/Third/AI/BT_MeleeEnemy.BT_MeleeEnemy"));
	auto* Graph = Tree ? Cast<UBehaviorTreeGraph>(Tree->BTGraph) : nullptr;
	if (!Tree || !Graph || !Tree->RootNode) return false;
	UBehaviorTreeGraphNode* AttackNode = nullptr;
	UBTDecorator_Blackboard* RetreatCondition = nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		auto* BTNode = Cast<UBehaviorTreeGraphNode>(Node);
		if (!BTNode) continue;
		if (auto* Composite = Cast<UBTCompositeNode>(BTNode->NodeInstance))
		{
			bool bHasRequest = false, bHasAttack = false;
			for (const FBTCompositeChild& Child : Composite->Children)
			{
				bHasRequest |= Child.ChildTask && Child.ChildTask->IsA<UBTTask_RequestAttackToken>();
				bHasAttack |= Child.ChildTask && Child.ChildTask->IsA<UBTTask_PerformMeleeAttack>();
			}
			if (bHasRequest && bHasAttack) AttackNode = BTNode;
		}
		for (UBehaviorTreeGraphNode* DecoratorNode : BTNode->Decorators)
			if (auto* Decorator = Cast<UBTDecorator_Blackboard>(DecoratorNode->NodeInstance);
				Decorator && Decorator->GetSelectedBlackboardKey() == TEXT("bIsTooClose"))
				RetreatCondition = Decorator;
	}
	// Refuse to save a partially matched or unrelated tree.
	if (!AttackNode || !RetreatCondition) return false;
	auto* AbortMode = FindFProperty<FByteProperty>(UBTDecorator::StaticClass(), TEXT("FlowAbortMode"));
	if (!AbortMode) return false;
	bool bHasScope = false;
	for (UBehaviorTreeGraphNode* Service : AttackNode->Services)
		bHasScope |= Service->NodeInstance && Service->NodeInstance->IsA<UBTService_MeleeAttackReservation>();
	if (!bHasScope)
	{
		auto* Service = NewObject<UBehaviorTreeGraphNode_Service>(Graph);
		Service->CreateNewGuid();
		Service->NodeInstance = NewObject<UBTService_MeleeAttackReservation>(Tree);
		Service->UpdateNodeClassData();
		AttackNode->AddSubNode(Service, Graph);
	}
	// Exit retreat at the service's release distance instead of finishing an
	// obsolete 180 cm destination after the player has already moved away.
	AbortMode->SetPropertyValue_InContainer(RetreatCondition, EBTFlowAbortMode::Both);
	Graph->UpdateAsset();
	auto* Composite = Cast<UBTCompositeNode>(AttackNode->NodeInstance);
	if (!Composite || !Composite->Services.ContainsByPredicate([](const UBTService* Service)
		{ return Service && Service->IsA<UBTService_MeleeAttackReservation>(); })) return false;
	Tree->MarkPackageDirty();
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;
	const FString Filename = FPackageName::LongPackageNameToFilename(
		Tree->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
	return UPackage::SavePackage(Tree->GetOutermost(), Tree, *Filename, Args);
#else
	return false;
#endif
}
