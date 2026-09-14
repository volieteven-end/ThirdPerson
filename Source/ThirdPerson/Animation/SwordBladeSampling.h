// 从蒙太奇实际骨骼轨迹采样装备挂点，为高速挥剑提供帧间弧线补采样；失败时由调用者回退。
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
