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
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/PrimitiveComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "../UI/EnemyHealthWidget.h"

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

bool URangedAIAssetTools::ConfigureEnemyPresentation()
{
#if WITH_EDITOR
	auto* Blueprint = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Third/Character/BP_EnemyRangedCharacter.BP_EnemyRangedCharacter"));
	auto* Sequence = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/ArcherAnimsetPro/Animations/InPlace/Bow_InPlace_Death_01.Bow_InPlace_Death_01"));
	auto* WidgetClass = LoadClass<UEnemyHealthWidget>(nullptr, TEXT("/Game/Third/Widget/WBP_EnemyHealth.WBP_EnemyHealth_C"));
	auto* MeleeDeath = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Death/AM_EnemyDeath.AM_EnemyDeath"));
	auto* SwordDeath = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Third/SwordAnimation/Dead_Anim.Dead_Anim"));
	if (!Blueprint || !Sequence || !WidgetClass || !MeleeDeath || !SwordDeath ||
		MeleeDeath->GetSkeleton() != SwordDeath->GetSkeleton() || MeleeDeath->SlotAnimTracks.Num() != 1 ||
		MeleeDeath->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() != 1 || MeleeDeath->CompositeSections.Num() > 1) return false;
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	auto* Enemy = Cast<AEnemyCharacter>(Blueprint->GeneratedClass->GetDefaultObject());
	auto* BarProperty = FindFProperty<FObjectProperty>(AEnemyCharacter::StaticClass(), TEXT("HealthBarWidget"));
	auto* Bar = Enemy && BarProperty ? Cast<UWidgetComponent>(BarProperty->GetObjectPropertyValue_InContainer(Enemy)) : nullptr;
	if (!Bar || !Enemy->GetMesh()->GetSkeletalMeshAsset() ||
		Enemy->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton() != Sequence->GetSkeleton()) return false;

	const TCHAR* PackageName = TEXT("/Game/Third/ArcherAnimation/AM_RangedDeath");
	auto* Montage = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Third/ArcherAnimation/AM_RangedDeath.AM_RangedDeath"), nullptr, LOAD_NoWarn);
	if (!Montage)
	{
		auto* Template = UAnimMontage::CreateSlotAnimationAsDynamicMontage(Sequence, TEXT("DefaultSlot"), .12f, .1f, 1.f, 1);
		if (!Template) return false;
		Montage = DuplicateObject<UAnimMontage>(Template, CreatePackage(PackageName), TEXT("AM_RangedDeath"));
		Montage->ClearFlags(RF_Transient);
		Montage->SetFlags(RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(Montage);
	}
	// Existing authored changes are retained on rerun; never rebuild other montages.
	Montage->bEnableAutoBlendOut = false;
	Montage->PostEditChange();
	if (!RangedAssetMigration::Save(Montage)) return false;
	// The old MM_Death_Front_01 ends with its pelvis still at standing height.
	// Reuse the complete sword fall without overwriting the dirty melee Blueprint.
	auto& Segment = MeleeDeath->SlotAnimTracks[0].AnimTrack.AnimSegments[0];
	Segment.SetAnimReference(SwordDeath);
	Segment.AnimStartTime = 0.f; Segment.AnimEndTime = SwordDeath->GetPlayLength();
	Segment.StartPos = 0.f; Segment.AnimPlayRate = 1.f; Segment.LoopingCount = 1;
	MeleeDeath->SetCompositeLength(MeleeDeath->CalculateSequenceLength());
	MeleeDeath->bEnableAutoBlendOut = false;
	MeleeDeath->PostEditChange();
	if (!RangedAssetMigration::Save(MeleeDeath)) return false;
	Enemy->DeathMontage = Montage;
	Bar->SetWidgetClass(WidgetClass);
	Bar->SetWidgetSpace(EWidgetSpace::Screen);
	Bar->SetDrawSize(FVector2D(160,20));
	Bar->SetRelativeLocation(FVector(0,0,120));
	Bar->SetVisibility(true);
	Bar->SetHiddenInGame(false);
	Bar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bar->SetGenerateOverlapEvents(false);
	Bar->SetWindowFocusable(false);
	TInlineComponentArray<UPrimitiveComponent*> Primitives(Enemy);
	for (auto* Component : Primitives) Component->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	return RangedAssetMigration::Save(Blueprint);
#else
	return false;
#endif
}

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
