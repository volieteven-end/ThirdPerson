#include "ComboChainPointNotify.h"
#include "CombatNotifyContext.h"
#include "../Components/CombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

void UComboChainPointNotify::Notify(USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);
    if (!MeshComp || !MeshComp->GetWorld() || !MeshComp->GetWorld()->IsGameWorld())
    {
        return;
    }
    if (AActor* Owner = MeshComp->GetOwner())
    {
        if (UCombatComponent* Combat = Owner->FindComponentByClass<UCombatComponent>())
        {
            Combat->ReachComboChainPoint(Animation, GetCombatNotifyMontageInstanceId(EventReference));
        }
    }
}
