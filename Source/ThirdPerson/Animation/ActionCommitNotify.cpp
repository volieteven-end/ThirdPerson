#include "ActionCommitNotify.h"
#include "CombatNotifyContext.h"
#include "../Components/CombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

void UActionCommitNotify::Notify(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& Reference)
{
    Super::Notify(Mesh, Animation, Reference);
    if (!Mesh || !Mesh->GetWorld() || !Mesh->GetWorld()->IsGameWorld() || !Mesh->GetOwner()) return;
    if (auto* Combat = Mesh->GetOwner()->FindComponentByClass<UCombatComponent>())
        Combat->NotifyActionCommit(Animation, GetCombatNotifyMontageInstanceId(Reference));
}
