// Fill out your copyright notice in the Description page of Project Settings.


#include "AttackHitNotify.h"
#include "CombatNotifyContext.h"
#include "../Components/CombatComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UAttackHitNotify::Notify(USkeletalMeshComponent* MeshComp,UAnimSequenceBase* Animation,const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !MeshComp->GetWorld() ||!MeshComp->GetWorld()->IsGameWorld())
	{
		return;
	}

	if (AActor* OwnerActor = MeshComp->GetOwner())
	{
		if (UCombatComponent* CombatComponent =
			OwnerActor->FindComponentByClass<UCombatComponent>())
		{
			if (CombatComponent->IsCurrentAttackNotify(Animation, GetCombatNotifyMontageInstanceId(EventReference)))
			{
				CombatComponent->PerformAttackHit();
			}
		}
	}
}