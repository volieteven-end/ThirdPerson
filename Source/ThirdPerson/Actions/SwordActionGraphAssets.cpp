#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_Slot.h"
#include "K2Node_VariableGet.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace SwordGraphAuthoring
{
template<class T> T* Node(UEdGraph* Graph, int32 X = 0, int32 Y = 0)
{
    auto* N = NewObject<T>(Graph); Graph->AddNode(N, false, false); N->CreateNewGuid();
    N->NodePosX = X; N->NodePosY = Y; N->PostPlacedNewNode(); N->AllocateDefaultPins();
    return N;
}
bool Connect(UEdGraphPin* A, UEdGraphPin* B)
{
    return A && B && A->GetOwningNode()->GetGraph()->GetSchema()->TryCreateConnection(A, B);
}
UAnimStateTransitionNode* Transition(UAnimStateNode* From, UAnimStateNode* To)
{
    UEdGraph* Graph = From->GetGraph();
    for (UEdGraphNode* N : Graph->Nodes)
        if (auto* T = Cast<UAnimStateTransitionNode>(N))
            if (T->GetPreviousState() == From && T->GetNextState() == To) return T;
    auto* T = Node<UAnimStateTransitionNode>(Graph, (From->NodePosX + To->NodePosX) / 2, (From->NodePosY + To->NodePosY) / 2);
    if (!Connect(From->GetOutputPin(), T->GetInputPin()) || !Connect(T->GetOutputPin(), To->GetInputPin())) return nullptr;
    return T;
}
bool Rule(UAnimStateNode* From, UAnimStateNode* To, FName Property = NAME_None, bool bNegate = false, int32 Priority = 1)
{
    auto* T = Transition(From, To); if (!T || !T->BoundGraph) return false;
    T->PriorityOrder = Priority; T->CrossfadeDuration = .08f; T->bDisabled = false;
    T->bAutomaticRuleBasedOnSequencePlayerInState = Property.IsNone();
    T->AutomaticRuleTriggerTime = .08f;
    UAnimGraphNode_TransitionResult* Result = nullptr;
    const auto OldNodes = T->BoundGraph->Nodes;
    for (UEdGraphNode* N : OldNodes)
        if (auto* R = Cast<UAnimGraphNode_TransitionResult>(N)) Result = R;
        else N->DestroyNode();
    if (!Result) return false;
    Result->BreakAllNodeLinks();
    if (Property.IsNone()) return true;
    auto* Get = Node<UK2Node_VariableGet>(T->BoundGraph, -400, 0);
    Get->VariableReference.SetSelfMember(Property); Get->ReconstructNode();
    UEdGraphPin* Output = Get->FindPin(Property, EGPD_Output);
    if (bNegate)
    {
        auto* Not = Node<UK2Node_CallFunction>(T->BoundGraph, -200, 0);
        Not->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("Not_PreBool")));
        Not->ReconstructNode();
        if (!Connect(Output, Not->FindPin(TEXT("A")))) return false;
        Output = Not->FindPin(TEXT("ReturnValue"), EGPD_Output);
    }
    return Connect(Output, Result->FindPin(TEXT("bCanEnterTransition")));
}
UAnimStateNode* State(UEdGraph* Graph, TMap<FString, UAnimStateNode*>& States, const FString& Name, const FString& Sequence, int32 X, int32 Y)
{
    if (auto** Existing = States.Find(Name)) return *Existing;
    auto* S = Node<UAnimStateNode>(Graph, X, Y);
    FBlueprintEditorUtils::RenameGraph(S->BoundGraph, Name);
    auto* Player = Node<UAnimGraphNode_SequencePlayer>(S->BoundGraph, -300, 0);
    auto* Anim = LoadObject<UAnimSequence>(nullptr, *(TEXT("/Game/Third/SwordAnimation/") + Sequence + TEXT(".") + Sequence));
    if (!Anim) return nullptr;
    Player->SetAnimationAsset(Anim); Player->Node.SetLoopAnimation(false); Player->ReconstructNode();
    if (!Connect(Player->FindPin(TEXT("Pose"), EGPD_Output), S->GetPoseSinkPinInsideState())) return nullptr;
    S->bAlwaysResetOnEntry = true;
    States.Add(Name, S); return S;
}
}

bool UpdateSwordAnimationGraphs()
{
    using namespace SwordGraphAuthoring;
    auto* BP = LoadObject<UAnimBlueprint>(nullptr, TEXT("/Game/Third/Character/ABP_TPCCharacter.ABP_TPCCharacter"));
    if (!BP) return false;
    TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
    TMap<FString, UAnimStateNode*> States; UEdGraph* StateGraph = nullptr;
    for (UEdGraph* G : Graphs) for (UEdGraphNode* N : G->Nodes)
    {
        if (auto* S = Cast<UAnimStateNode>(N)) { States.Add(S->GetStateName(), S); StateGraph = G; }
        if (auto* Slot = Cast<UAnimGraphNode_Slot>(N)) Slot->Node.bAlwaysUpdateSourcePose = true;
        if (auto* Player = Cast<UAnimGraphNode_SequencePlayer>(N))
        {
            if (G->GetName().StartsWith(TEXT("Jump")))
            {
                const TCHAR* Name = G->GetName() == TEXT("JumpStart") ? TEXT("AS_Jump_In_Anim") :
                    (G->GetName() == TEXT("JumpLoop") ? TEXT("AS_Jump_Loop_Anim") : TEXT("AS_Jump_Out_Anim"));
                if (auto* Seq = LoadObject<UAnimSequence>(nullptr, *(FString(TEXT("/Game/Third/Actions/Sword/Sequences/")) + Name + TEXT(".") + Name)))
                    Player->SetAnimationAsset(Seq);
            }
            // One-shot states reset on reentry; only the airborne / guard loops repeat.
            Player->Node.SetLoopAnimation(G->GetName() == TEXT("JumpLoop") || G->GetName() == TEXT("BlockHold"));
        }
    }
    for (const TCHAR* Name : { TEXT("Ground"), TEXT("JumpStart"), TEXT("JumpLoop"), TEXT("JumpEnd"), TEXT("Crouch"),
                              TEXT("CrouchIn"), TEXT("CrouchOut"), TEXT("BlockIn"), TEXT("BlockHold") })
        if (!States.Contains(Name)) return false;
    auto* Out = State(StateGraph, States, TEXT("BlockOut"), TEXT("Block_Out_Anim"), 700, 800);
    auto* Hit = State(StateGraph, States, TEXT("BlockHit"), TEXT("Block_Hit_Anim"), 350, 1000);
    if (!Out || !Hit) return false;
    for (const auto& Pair : States) if (Pair.Key != TEXT("Ground") && Pair.Key != TEXT("Crouch")) Pair.Value->bAlwaysResetOnEntry = true;
    // Remove the old parry-driven transitions: parry timing is gameplay, not a pose exit condition.
    const auto Nodes = StateGraph->Nodes;
    for (UEdGraphNode* N : Nodes)
        if (auto* T = Cast<UAnimStateTransitionNode>(N))
            if ((T->GetPreviousState() == States[TEXT("BlockIn")] || T->GetPreviousState() == States[TEXT("BlockHold")]) &&
                T->GetNextState() == States[TEXT("Ground")]) T->DestroyNode();
    bool Ok = true;
    Ok &= Rule(States[TEXT("BlockIn")], States[TEXT("BlockHold")]);
    Ok &= Rule(States[TEXT("BlockIn")], Out, TEXT("bIsBlocking"), true, 0);
    Ok &= Rule(States[TEXT("BlockHold")], Out, TEXT("bIsBlocking"), true, 0);
    Ok &= Rule(States[TEXT("BlockIn")], Hit, TEXT("bGuardHitActive"), false, 0);
    Ok &= Rule(States[TEXT("BlockHold")], Hit, TEXT("bGuardHitActive"), false, 0);
    Ok &= Rule(Hit, States[TEXT("BlockHold")], TEXT("bGuardHoldReady"));
    Ok &= Rule(Hit, Out, TEXT("bIsBlocking"), true, 0);
    Ok &= Rule(Out, States[TEXT("Ground")]);
    Ok &= Rule(Out, States[TEXT("BlockIn")], TEXT("bIsBlocking"), false, 0);
    for (const TCHAR* Name : { TEXT("Ground"), TEXT("JumpStart"), TEXT("JumpLoop"), TEXT("JumpEnd"), TEXT("Crouch"), TEXT("CrouchIn"), TEXT("CrouchOut") })
        Ok &= Rule(States[Name], States[TEXT("BlockIn")], TEXT("bIsBlocking"), false, 0);
    for (const TCHAR* Name : { TEXT("Ground"), TEXT("JumpEnd"), TEXT("Crouch"), TEXT("CrouchIn"), TEXT("CrouchOut"), TEXT("BlockOut") })
        Ok &= Rule(States[Name], States[TEXT("JumpStart")], TEXT("bWantJumpPose"), false, 0);
    Ok &= Rule(States[TEXT("JumpStart")], States[TEXT("JumpEnd")], TEXT("bIsInAir"), true, 0);
    Ok &= Rule(States[TEXT("CrouchIn")], States[TEXT("CrouchOut")], TEXT("bIsCrouched"), true, 0);
    Ok &= Rule(States[TEXT("CrouchOut")], States[TEXT("CrouchIn")], TEXT("bIsCrouched"), false, 0);
    // Static entry clips remain available at rest; held input blends to footwork instead of skating.
    Ok &= Rule(States[TEXT("Ground")], States[TEXT("Crouch")], TEXT("bCrouchMoveRequested"), false, 1);
    Ok &= Rule(States[TEXT("Ground")], States[TEXT("CrouchIn")], TEXT("bCrouchEntryRequested"), false, 2);
    Ok &= Rule(States[TEXT("Crouch")], States[TEXT("Ground")], TEXT("bStandingMoveRequested"), false, 1);
    Ok &= Rule(States[TEXT("Crouch")], States[TEXT("CrouchOut")], TEXT("bStandingExitRequested"), false, 2);
    Ok &= Rule(States[TEXT("CrouchIn")], States[TEXT("Crouch")], TEXT("bCrouchEntryCanExit"));
    Ok &= Rule(States[TEXT("CrouchOut")], States[TEXT("Ground")], TEXT("bCrouchExitCanExit"));
    Ok &= Rule(States[TEXT("JumpEnd")], States[TEXT("Ground")], TEXT("bLandingCanExit"));
    for (const auto& Pair : { TPair<FString,FString>(TEXT("Ground"),TEXT("Crouch")),
        TPair<FString,FString>(TEXT("Crouch"),TEXT("Ground")), TPair<FString,FString>(TEXT("CrouchIn"),TEXT("Crouch")),
        TPair<FString,FString>(TEXT("CrouchOut"),TEXT("Ground")), TPair<FString,FString>(TEXT("JumpEnd"),TEXT("Ground")) })
        if (auto* T = Transition(States[Pair.Key], States[Pair.Value])) T->CrossfadeDuration = .12f;
    if (!Ok) return false;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); FKismetEditorUtilities::CompileBlueprint(BP);
    if (BP->Status == BS_Error) return false;
    BP->MarkPackageDirty(); FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
    return UPackage::SavePackage(BP->GetOutermost(), BP,
        *FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args);
}
#endif
