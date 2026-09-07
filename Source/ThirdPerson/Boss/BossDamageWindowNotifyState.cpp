#include "BossDamageWindowNotifyState.h"
#include "BossActionComponent.h"
#include "../Animation/CombatNotifyContext.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
void UBossDamageWindowNotifyState::NotifyBegin(USkeletalMeshComponent* Mesh,UAnimSequenceBase* Animation,float Duration,const FAnimNotifyEventReference& Ref)
{
 if (Mesh && Mesh->GetWorld() && Mesh->GetWorld()->IsGameWorld() && Mesh->GetOwner())
  if (auto* A=Mesh->GetOwner()->FindComponentByClass<UBossActionComponent>()) A->NotifyWindow(true,Animation,GetCombatNotifyMontageInstanceId(Ref));
}
void UBossDamageWindowNotifyState::NotifyEnd(USkeletalMeshComponent* Mesh,UAnimSequenceBase* Animation,const FAnimNotifyEventReference& Ref)
{
 if (Mesh && Mesh->GetWorld() && Mesh->GetWorld()->IsGameWorld() && Mesh->GetOwner())
  if (auto* A=Mesh->GetOwner()->FindComponentByClass<UBossActionComponent>()) A->NotifyWindow(false,Animation,GetCombatNotifyMontageInstanceId(Ref));
}
