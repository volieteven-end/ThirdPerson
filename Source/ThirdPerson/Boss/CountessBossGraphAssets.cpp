// Editor-only authoring of the inspectable graphs used by the C++ Boss.
#if WITH_EDITOR
#include "BossDefinition.h"
#include "BossStatusWidget.h"
#include "CountessBossAnimInstance.h"
#include "CountessBossAIController.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/AnimBlueprintFactory.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_SequenceEvaluator.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_TwoWayBlend.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_ApplyAdditive.h"
#include "AnimGraphNode_Inertialization.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "BehaviorTreeGraphNode_Service.h"
#include "BossBehaviorNodes.h"
#include "EdGraphSchema_BehaviorTree.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace
{
const FString Root=TEXT("/Game/Third/Bosses/Countess/");
template<class T> T* Find(const FString& Name)
{ return LoadObject<T>(nullptr,*(Root+Name+TEXT(".")+FPackageName::GetLongPackageAssetName(Name)),nullptr,LOAD_NoWarn); }
bool Save(UObject* A)
{
 A->MarkPackageDirty(); FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; Args.SaveFlags=SAVE_NoError;
 return UPackage::SavePackage(A->GetOutermost(),A,*FPackageName::LongPackageNameToFilename(A->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
}
template<class T> T* AddNode(UEdGraph* Graph,int32 X,int32 Y)
{
 T* Node=NewObject<T>(Graph); Graph->AddNode(Node,false,false); Node->CreateNewGuid();
 Node->NodePosX=X; Node->NodePosY=Y; Node->PostPlacedNewNode(); Node->AllocateDefaultPins(); return Node;
}
bool Wire(UEdGraphNode* From,FName Output,UEdGraphNode* To,FName Input)
{
 UEdGraphPin* A=From->FindPin(Output,EGPD_Output); UEdGraphPin* B=To->FindPin(Input,EGPD_Input);
 if (!A || !B || !From->GetGraph()->GetSchema()->TryCreateConnection(A,B))
 { UE_LOG(LogTemp,Error,TEXT("Boss graph connection failed: %s.%s -> %s.%s"),*From->GetName(),*Output.ToString(),*To->GetName(),*Input.ToString()); return false; }
 return true;
}
bool Bind(UAnimGraphNode_Base* Node,FName Pin,FName Member)
{
 for (auto& P:Node->ShowPinForProperties) if (P.PropertyName==Pin) P.bShowPin=true;
 Node->ReconstructNode();
 auto* Get=AddNode<UK2Node_VariableGet>(Node->GetGraph(),Node->NodePosX-230,Node->NodePosY+140);
 Get->VariableReference.SetSelfMember(Member); Get->ReconstructNode();
 return Wire(Get,Member,Node,Pin);
}
UAnimGraphNode_SequencePlayer* Player(UEdGraph* Graph,UAnimSequence* Sequence,int32 X,int32 Y)
{
 auto* N=AddNode<UAnimGraphNode_SequencePlayer>(Graph,X,Y); N->SetAnimationAsset(Sequence); N->ReconstructNode(); return N;
}
UAnimGraphNode_TwoWayBlend* Blend(UEdGraph* Graph,UEdGraphNode* A,UEdGraphNode* B,FName Alpha,int32 X,int32 Y,bool& Ok)
{
 auto* N=AddNode<UAnimGraphNode_TwoWayBlend>(Graph,X,Y);
 Ok&=Wire(A,TEXT("Pose"),N,TEXT("A")); Ok&=Wire(B,TEXT("Pose"),N,TEXT("B")); Ok&=Bind(N,TEXT("Alpha"),Alpha); return N;
}
bool CalibrateFootSpeeds(UBossDefinition* D)
{
 D->JogReferenceSpeeds.SetNum(4);
 const FVector Directions[]={FVector::ForwardVector,-FVector::ForwardVector,-FVector::RightVector,FVector::RightVector};
 for (int32 I=0;I<4;++I)
 {
  auto* Sequence=D->Jog[I].Get(); const auto& Ref=Sequence->GetSkeleton()->GetReferenceSkeleton();
  const int32 Samples=FMath::Max(60,Sequence->GetNumberOfSampledKeys()); const float DT=Sequence->GetPlayLength()/Samples;
  TArray<float> Speeds;
  for (FName Foot:{FName(TEXT("foot_l")),FName(TEXT("foot_r"))})
  {
   const int32 FootIndex=Ref.FindBoneIndex(Foot); if (FootIndex==INDEX_NONE) return false;
   TArray<FVector> Points; float Lowest=MAX_flt;
   for (int32 N=0;N<=Samples;++N)
   {
    FTransform Pose=FTransform::Identity;
    for (int32 Bone=FootIndex;Bone!=INDEX_NONE;Bone=Ref.GetParentIndex(Bone))
    { FTransform Local; Sequence->GetBoneTransform(Local,FSkeletonPoseBoneIndex(Bone),FAnimExtractContext(static_cast<double>(N*DT)),true); Pose=Pose*Local; }
    const FVector P=FRotator(0,-90,0).RotateVector(Pose.GetLocation()); Points.Add(P); Lowest=FMath::Min(Lowest,static_cast<float>(P.Z));
   }
   for (int32 N=1;N<Points.Num();++N)
   {
    const float Speed=-FVector::DotProduct((Points[N]-Points[N-1])/DT,Directions[I]);
    if (Points[N].Z<Lowest+7 && Points[N-1].Z<Lowest+7 && Speed>50 && Speed<900) Speeds.Add(Speed);
   }
  }
  if (Speeds.Num()<4) { UE_LOG(LogTemp,Error,TEXT("Insufficient planted-foot samples for %s"),*Sequence->GetName()); return false; }
  Speeds.Sort(); D->JogReferenceSpeeds[I]=Speeds[Speeds.Num()/2];
  UE_LOG(LogTemp,Display,TEXT("Countess foot calibration: %s, %d stance segments, %.2f cm/s at rate 1"),*Sequence->GetName(),Speeds.Num(),D->JogReferenceSpeeds[I]);
 }
 return true;
}
}

bool BuildCountessGraphAssets(USkeletalMesh* Mesh,UBossDefinition* D,UClass*& AnimClass)
{
 if (!CalibrateFootSpeeds(D)) return false;
 auto* BS=Find<UBlendSpace>(TEXT("Animations/BS_CountessLocomotion"));
 if (!BS)
 {
  BS=NewObject<UBlendSpace>(CreatePackage(*(Root+TEXT("Animations/BS_CountessLocomotion"))),TEXT("BS_CountessLocomotion"),RF_Public|RF_Standalone);
  BS->SetSkeleton(Mesh->GetSkeleton()); BS->SetPreviewMesh(Mesh);
  // Direction in component space. Speed drives a separate calibrated playback-rate input.
  auto* Param=FindFProperty<FStructProperty>(UBlendSpace::StaticClass(),TEXT("BlendParameters"));
  if (!Param) return false;
  for (int32 I=0;I<2;++I)
  { auto* P=Param->ContainerPtrToValuePtr<FBlendParameter>(BS,I); P->DisplayName=I==0?TEXT("Forward"):TEXT("Right"); P->Min=-1; P->Max=1; P->GridNum=4; }
  BS->AddSample(D->Idle,FVector::ZeroVector);
  const FVector Axis[]={FVector(1,0,0),FVector(-1,0,0),FVector(0,-1,0),FVector(0,1,0)};
  for (int32 I=0;I<4;++I) BS->AddSample(D->Jog[I],Axis[I]);
  BS->ValidateSampleData(); BS->ResampleData(); BS->PostEditChange(); FAssetRegistryModule::AssetCreated(BS);
  if (!Save(BS)) return false;
 }
 auto* BP=Find<UAnimBlueprint>(TEXT("Animations/ABP_CountessBoss"));
 if (!BP)
 {
  auto* Factory=NewObject<UAnimBlueprintFactory>(); Factory->ParentClass=UCountessBossAnimInstance::StaticClass();
  Factory->TargetSkeleton=Mesh->GetSkeleton(); Factory->PreviewSkeletalMesh=Mesh;
  BP=Cast<UAnimBlueprint>(Factory->FactoryCreateNew(UAnimBlueprint::StaticClass(),CreatePackage(*(Root+TEXT("Animations/ABP_CountessBoss"))),TEXT("ABP_CountessBoss"),RF_Public|RF_Standalone,nullptr,GWarn));
  if (!BP) return false;
  UEdGraph* G=nullptr; for (UEdGraph* Graph:BP->FunctionGraphs) if (Graph->GetFName()==TEXT("AnimGraph")) G=Graph;
  if (!G) return false;
  UAnimGraphNode_Root* Output=nullptr; for (UEdGraphNode* N:G->Nodes) if (auto* R=Cast<UAnimGraphNode_Root>(N)) Output=R;
  if (!Output) return false;
  auto* Walk=AddNode<UAnimGraphNode_BlendSpacePlayer>(G,-1600,0); Walk->SetAnimationAsset(BS); Walk->ReconstructNode();
  bool Ok=Bind(Walk,TEXT("X"),TEXT("MoveX")) && Bind(Walk,TEXT("Y"),TEXT("MoveY")) && Bind(Walk,TEXT("PlayRate"),TEXT("MoveRate"));
  auto* Rest=Player(G,D->Relaxed,-1600,-300);
  auto* Idle=Blend(G,Rest,Walk,TEXT("CombatAlpha"),-1100,-50,Ok);
  auto* Fall=Player(G,D->Falling,-1100,400);
  auto* Falling=Blend(G,Idle,Fall,TEXT("FallAlpha"),-750,0,Ok);
  auto* Stun=Player(G,D->StunLoop,-750,650);
  auto* Stunned=Blend(G,Falling,Stun,TEXT("StunAlpha"),-400,0,Ok);
  auto* Slot=AddNode<UAnimGraphNode_Slot>(G,0,0); Slot->Node.SlotName=TEXT("DefaultSlot"); Slot->Node.bAlwaysUpdateSourcePose=true;
  Ok&=Wire(Stunned,TEXT("Pose"),Slot,TEXT("Source"));
  auto* Hit=AddNode<UAnimGraphNode_SequenceEvaluator>(G,0,420); Hit->SetAnimationAsset(D->HitReactions[0]);
  Ok&=Bind(Hit,TEXT("Sequence"),TEXT("HitSequence")); Ok&=Bind(Hit,TEXT("ExplicitTime"),TEXT("HitTime"));
  auto* Add=AddNode<UAnimGraphNode_ApplyAdditive>(G,380,0);
  Ok&=Wire(Slot,TEXT("Pose"),Add,TEXT("Base")); Ok&=Wire(Hit,TEXT("Pose"),Add,TEXT("Additive")); Ok&=Bind(Add,TEXT("Alpha"),TEXT("HitAlpha"));
  auto* Inert=AddNode<UAnimGraphNode_Inertialization>(G,750,0); Ok&=Wire(Add,TEXT("Pose"),Inert,TEXT("Source"));
  auto* Death=AddNode<UAnimGraphNode_SequenceEvaluator>(G,750,400); Death->SetAnimationAsset(D->Death); Death->Node.SetShouldLoop(false);
  Ok&=Bind(Death,TEXT("ExplicitTime"),TEXT("DeathTime"));
  auto* Dead=Blend(G,Inert,Death,TEXT("DeathAlpha"),1100,0,Ok);
  Output->NodePosX=1500; Output->NodePosY=0; Ok&=Wire(Dead,TEXT("Pose"),Output,TEXT("Result"));
  if (!Ok) return false;
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); FKismetEditorUtilities::CompileBlueprint(BP);
  if (BP->Status==BS_Error) return false;
  FAssetRegistryModule::AssetCreated(BP); if (!Save(BP)) return false;
 }
 AnimClass=BP->GeneratedClass;
 auto* Tree=Find<UBehaviorTree>(TEXT("AI/BT_CountessBoss"));
 if (!Tree)
 {
  Tree=DuplicateObject<UBehaviorTree>(ACountessBossAIController::CreateDefaultBossTree(GetTransientPackage()),CreatePackage(*(Root+TEXT("AI/BT_CountessBoss"))),TEXT("BT_CountessBoss"));
  Tree->SetFlags(RF_Public|RF_Standalone);
  auto* BB=DuplicateObject<UBlackboardData>(Tree->BlackboardAsset,CreatePackage(*(Root+TEXT("AI/BB_CountessBoss"))),TEXT("BB_CountessBoss"));
  BB->SetFlags(RF_Public|RF_Standalone); Tree->BlackboardAsset=BB;
  auto* Graph=NewObject<UBehaviorTreeGraph>(Tree,TEXT("BehaviorTreeGraph")); Tree->BTGraph=Graph;
  UBTCompositeNode* RuntimeRoot=Tree->RootNode;
  Graph->Schema=UEdGraphSchema_BehaviorTree::StaticClass(); Graph->GetSchema()->CreateDefaultNodesForGraph(*Graph);
  Graph->OnCreated(); Graph->Initialize(); Tree->RootNode=RuntimeRoot; Graph->SpawnMissingNodes();
  int32 LayoutIndex=0;
  for (UEdGraphNode* N:Graph->Nodes) { N->NodePosX=LayoutIndex++*280; N->NodePosY=N->IsA<UBehaviorTreeGraphNode_Root>()?-300:0; }
  for (UEdGraphNode* N:Graph->Nodes) if (auto* R=Cast<UBehaviorTreeGraphNode_Root>(N)) R->BlackboardAsset=BB;
  FAssetRegistryModule::AssetCreated(Tree); FAssetRegistryModule::AssetCreated(BB);
  if (!Save(BB) || !Save(Tree)) return false;
 }
 D->BehaviorTree=Tree;
 auto* Blackboard=Find<UBlackboardData>(TEXT("AI/BB_CountessBoss"));
 if (!Blackboard) return false;
 // Creating the editor Root node can replace the runtime blackboard with its default (null).
 // Keep both serialized owners in sync, including when repairing an existing generated graph.
 Tree->BlackboardAsset=Blackboard;
 if (auto* Graph=Cast<UBehaviorTreeGraph>(Tree->BTGraph))
 {
  for (UEdGraphNode* Node:Graph->Nodes)
   if (auto* RootNode=Cast<UBehaviorTreeGraphNode_Root>(Node)) RootNode->BlackboardAsset=Blackboard;
  for (UEdGraphNode* Node:Graph->Nodes)
   if (auto* N=Cast<UBehaviorTreeGraphNode>(Node); N && N->NodeInstance==Tree->RootNode && N->Services.IsEmpty())
   {
    auto* Service=NewObject<UBehaviorTreeGraphNode_Service>(Graph); Service->CreateNewGuid();
    Service->NodeInstance=NewObject<UBTService_UpdateBossContext>(Tree); Service->UpdateNodeClassData();
    N->AddSubNode(Service,Graph);
   }
  Graph->UpdateAsset();
  Tree->BlackboardAsset=Blackboard;
  if (!Tree->RootNode || Tree->RootNode->Services.Num()!=1 || !Save(Tree)) return false;
 }
 UE_LOG(LogTemp,Display,TEXT("Countess saved BT root: %s, children: %d, graph nodes: %d"),*GetNameSafe(Tree->RootNode),Tree->RootNode?Tree->RootNode->Children.Num():0,Tree->BTGraph?Tree->BTGraph->Nodes.Num():0);
 auto* Widget=Find<UWidgetBlueprint>(TEXT("UI/WBP_BossStatus"));
 if (!Widget)
 {
  auto* Factory=NewObject<UWidgetBlueprintFactory>(); Factory->ParentClass=UBossStatusWidget::StaticClass();
  Widget=Cast<UWidgetBlueprint>(Factory->FactoryCreateNew(UWidgetBlueprint::StaticClass(),CreatePackage(*(Root+TEXT("UI/WBP_BossStatus"))),TEXT("WBP_BossStatus"),RF_Public|RF_Standalone,nullptr,GWarn));
  if (!Widget) return false;
  FKismetEditorUtilities::CompileBlueprint(Widget); if (Widget->Status==BS_Error) return false;
  FAssetRegistryModule::AssetCreated(Widget); if (!Save(Widget)) return false;
 }
 D->StatusWidgetClass=Widget->GeneratedClass;
 return AnimClass && Save(D);
}
#endif
