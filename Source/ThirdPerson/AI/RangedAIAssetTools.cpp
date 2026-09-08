#include "RangedAIAssetTools.h"

#if WITH_EDITOR
#include "EnemyCharacter.h"
#include "RangedAIBehaviorNodes.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_Decorator.h"
#include "BehaviorTreeGraphNode_Task.h"
#include "Engine/Blueprint.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace RangedAssetMigration
{
bool Save(UObject* Asset)
{
	Asset->MarkPackageDirty();
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;
	const FString Filename = FPackageName::LongPackageNameToFilename(
		Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
	return UPackage::SavePackage(Asset->GetOutermost(), Asset, *Filename, Args);
}
}
#endif

bool URangedAIAssetTools::ConfigureRangedCombat()
{
#if WITH_EDITOR
	auto* Tree = LoadObject<UBehaviorTree>(nullptr, TEXT("/Game/Third/AI/BT_RangeEnemy.BT_RangeEnemy"));
	auto* Board = LoadObject<UBlackboardData>(nullptr, TEXT("/Game/Third/AI/BB_RangedEnemy.BB_RangedEnemy"));
	auto* Blueprint = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Third/Character/BP_EnemyRangedCharacter.BP_EnemyRangedCharacter"));
	auto* Graph = Tree ? Cast<UBehaviorTreeGraph>(Tree->BTGraph) : nullptr;
	if (!Tree || !Board || Tree->BlackboardAsset != Board || !Blueprint || !Graph) return false;

	UBehaviorTreeGraphNode* TargetNode = nullptr;
	UBehaviorTreeGraphNode* ChaseNode = nullptr;
	UBehaviorTreeGraphNode* HoldNode = nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		auto* BTNode = Cast<UBehaviorTreeGraphNode>(Node);
		if (!BTNode) continue;
		if (auto* Selector = Cast<UBTComposite_Selector>(BTNode->NodeInstance))
		{
			bool bHasRetreat = false, bHasShot = false;
			for (const auto& Child : Selector->Children)
				if (Child.ChildComposite) for (const auto& Entry : Child.ChildComposite->Children)
				{
					bHasRetreat |= Entry.ChildTask && Entry.ChildTask->IsA<UBTTask_SelectRangedRetreatPosition>();
					bHasShot |= Entry.ChildTask && Entry.ChildTask->IsA<UBTTask_PerformRangedAttack>();
				}
			if (bHasRetreat && bHasShot) TargetNode = BTNode;
		}
		if (auto* Composite = Cast<UBTCompositeNode>(BTNode->NodeInstance))
			if (Composite->Children.Num() == 1)
				if (auto* Move = Cast<UBTTask_MoveTo>(Composite->Children[0].ChildTask);
					Move && Move->GetSelectedBlackboardKey() == TEXT("TargetActor")) ChaseNode = BTNode;
		if (auto* Wait = Cast<UBTTask_Wait>(BTNode->NodeInstance);
			Wait && Wait->GetNodeName() == TEXT("Hold Ranged Ground")) HoldNode = BTNode;
	}
	auto* Selector = TargetNode ? Cast<UBTComposite_Selector>(TargetNode->NodeInstance) : nullptr;
	if (!Selector || !ChaseNode || !Selector->Children.ContainsByPredicate([ChaseNode](const FBTCompositeChild& Child)
		{ return Child.ChildComposite == ChaseNode->NodeInstance; })) return false;
	const FName ApproachKey(TEXT("bShouldApproachTarget"));
	auto* KeyProperty = FindFProperty<FStructProperty>(UBTDecorator_Blackboard::StaticClass(), TEXT("BlackboardKey"));
	auto* AbortProperty = FindFProperty<FByteProperty>(UBTDecorator::StaticClass(), TEXT("FlowAbortMode"));
	auto* OperationProperty = FindFProperty<FByteProperty>(UBTDecorator_Blackboard::StaticClass(), TEXT("OperationType"));
	if (!KeyProperty || !AbortProperty || !OperationProperty) return false;

	if (Board->GetKeyID(ApproachKey) == FBlackboard::InvalidKey)
	{
		FBlackboardEntry Entry;
		Entry.EntryName = ApproachKey;
		Entry.EntryDescription = TEXT("Approach only outside shooting range or without a clear firing line; never walk forward during retreat recovery.");
		Entry.KeyType = NewObject<UBlackboardKeyType_Bool>(Board);
		Board->Keys.Add(Entry);
	}
	if (Board->GetKeyType(Board->GetKeyID(ApproachKey)) != UBlackboardKeyType_Bool::StaticClass()) return false;
	UBTDecorator_Blackboard* ApproachCondition = nullptr;
	for (UBehaviorTreeGraphNode* Node : ChaseNode->Decorators)
		if (auto* Condition = Cast<UBTDecorator_Blackboard>(Node->NodeInstance);
			Condition && Condition->GetSelectedBlackboardKey() == ApproachKey) ApproachCondition = Condition;
	if (!ApproachCondition)
	{
		auto* DecoratorNode = NewObject<UBehaviorTreeGraphNode_Decorator>(Graph);
		DecoratorNode->CreateNewGuid();
		ApproachCondition = NewObject<UBTDecorator_Blackboard>(Tree);
		DecoratorNode->NodeInstance = ApproachCondition;
		DecoratorNode->UpdateNodeClassData();
		ChaseNode->AddSubNode(DecoratorNode, Graph);
	}
	auto* Key = KeyProperty->ContainerPtrToValuePtr<FBlackboardKeySelector>(ApproachCondition);
	Key->SelectedKeyName = ApproachKey;
	Key->ResolveSelectedKey(*Board);
	AbortProperty->SetPropertyValue_InContainer(ApproachCondition, EBTFlowAbortMode::Both);
	OperationProperty->SetPropertyValue_InContainer(ApproachCondition, EBasicKeyOperation::Set);
	ApproachCondition->BuildDescription();
	if (!HoldNode)
	{
		auto* WaitNode = NewObject<UBehaviorTreeGraphNode_Task>(Graph);
		WaitNode->CreateNewGuid();
		auto* Wait = NewObject<UBTTask_Wait>(Tree);
		Wait->NodeName = TEXT("Hold Ranged Ground");
		Wait->WaitTime = FValueOrBBKey_Float(0.2f);
		Wait->RandomDeviation = FValueOrBBKey_Float(0.f);
		WaitNode->NodeInstance = Wait;
		WaitNode->UpdateNodeClassData();
		WaitNode->NodePosX = ChaseNode->NodePosX + 400;
		WaitNode->NodePosY = ChaseNode->NodePosY;
		Graph->AddNode(WaitNode, false, false);
		WaitNode->AllocateDefaultPins();
		TargetNode->GetOutputPin()->MakeLinkTo(WaitNode->GetInputPin());
		HoldNode = WaitNode;
	}
	Graph->UpdateAsset();
	if (Selector->Children.Num() != 4 || Selector->Children.Last().ChildTask != HoldNode->NodeInstance) return false;
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	auto* Enemy = Cast<AEnemyCharacter>(Blueprint->GeneratedClass->GetDefaultObject());
	if (!Enemy) return false;
	Enemy->bUseControllerRotationYaw = false;
	Enemy->GetCharacterMovement()->MaxWalkSpeed = 360.f;
	// The context service owns the combat-facing override; ordinary patrol keeps
	// movement-owned yaw, with no competing instant controller-yaw assignment.
	Enemy->GetCharacterMovement()->bOrientRotationToMovement = true;
	Enemy->GetCharacterMovement()->bUseControllerDesiredRotation = false;
	return RangedAssetMigration::Save(Board) && RangedAssetMigration::Save(Tree) && RangedAssetMigration::Save(Blueprint);
#else
	return false;
#endif
}
