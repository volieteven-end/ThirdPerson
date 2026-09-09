#include "TPCFootstepNotify.h"
#include "TPCCharacterAudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

void UTPCFootstepNotify::Notify(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& Reference)
{
	Super::Notify(Mesh, Animation, Reference);
	if (!Mesh || !Mesh->GetWorld() || !Mesh->GetWorld()->IsGameWorld() || !Mesh->GetOwner()) return;
	if (auto* Audio = Mesh->GetOwner()->FindComponentByClass<UTPCCharacterAudioComponent>()) Audio->PlayFootstep(FootBone);
}
