#pragma once
#include "CoreMinimal.h"
class UAnimMontage;
class USkeletalMeshComponent;

namespace TPCBladeSampling
{
/** Sample the authored equipment socket between rendered frames, with root motion removed once. */
bool SampleEquipmentPose(const UAnimMontage* Montage, float MontageTime,
    const USkeletalMeshComponent* CharacterMesh, FName EquipSocket, FTransform& OutComponentPose);
}
