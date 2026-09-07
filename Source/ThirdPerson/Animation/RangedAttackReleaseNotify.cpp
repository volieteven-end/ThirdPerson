#include "RangedAttackReleaseNotify.h"

#include "../Components/CombatComponent.h"
#include "Components/SkeletalMeshComponent.h"

void URangedAttackReleaseNotify::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (!MeshComp || !MeshComp->GetWorld() ||
		!MeshComp->GetWorld()->IsGameWorld())
	{
		return;
	}

	if (AActor* OwnerActor = MeshComp->GetOwner())
	{
		if (UCombatComponent* Combat =
			OwnerActor->FindComponentByClass<UCombatComponent>())
		{
			Combat->ReleaseRangedProjectile();
		}
	}
}
